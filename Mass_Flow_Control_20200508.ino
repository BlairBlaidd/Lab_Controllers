// 5/8/20 validated serial communication and added checksum for read and transmit
// 5/4/20
//added RS485 communication
//latest version RGB 5/3/20
//added menu0 macros for rapid control
//displays menus and navigates
//allows editing and storing of parameters in FRAM
//reads analog in and converts to mass flow to range of controller (0-n sccm) value is scaled by process gas versus calibrated gas
//Not implemented:
//RS485 I/O
//Analog (control) out
//valve (control)
//Multi-segment programs - may be a long time coming
//
/*
 Based on the LiquidCrystal Library - Hello World
 
 Uses a Newhaven 20x4 OLED display.  These are similar, but
 not quite 100% compatible with the Hitachi HD44780 based LCD displays. 
 

  The circuit:
 * LCD RS pin to digital pin 43
 * LCD R/W pin to digital pin 41
 * LCD Enable pin to digital pin 39
 * LCD D4 pin to digital pin 37
 * LCD D5 pin to digital pin 35
 * LCD D6 pin to digital pin 33
 * LCD D7 pin to digital pin 31

 There is no need for the contrast pot as used in the LCD tutorial
 
 Library originally added 18 Apr 2008
 by David A. Mellis
 library modified 5 Jul 2009
 by Limor Fried (http://www.ladyada.net)l
 example added 9 Jul 2009
 by Tom Igoe
 modified 22 Nov 2010
 by Tom Igoe
 Library & Example Converted for OLED
 by Bill Earl 30 Jun 2012
 Library Modified for Newhaven display
 by Richard Blair 14 Jul 2019
 
 converted to object oriented version 1/16/20
 
 */

// include the library code:
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include "Adafruit_FRAM_SPI.h" //FRAM driver library
#include <Newhaven_CharacterOLED.h>
#include <Keypad.h>

//Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(FRAM_CS);  // use hardware SPI
//save memory using define
#define FRAM_CS 53
#define FRAM_SCK 52
#define FRAM_MISO 50
#define FRAM_MOSI 51
#define addr 0

//save a little memory by using define instead of const
#define ROWS 4 //four rows on keypad
#define COLS 4 //four columns on keypad
#define Controllers 4 //number of controller

class MassFlowController
{
         //class member variables
      public:
         float SetValue; //flow the controller is set to in sccm
         float StoredSetValue; //set flow memory for macro cycling between 100% and 0%
         float ReadValue; //value in sccm read from the controller
         float FlowMax; //max flow fo the controller as readvalue and set value are set by 0-5v signals
         float ProcValue; //relation to N2 of process gas being used - allows scaling and use of controller for other gases
         float CalValue; //calibration gas related to N2 - used to scale output when using gas its not calibrated for
         byte  PGas; //location in the process gas array that is the label
         byte CGas; //location in calibration gas array that is the label
         bool ONOFF; //status of controller - active or disabled
         bool Mode; //manual or program mode - program is a future feature
         byte Cnum; //controller number
         byte cycle; //location in macro cycle will range from 0 to 3 corresponding to setflows of setpoint,0,max,0
         uint32_t StartMem; //start memory location of each controller
         Adafruit_FRAM_SPI fram; //FRAM memory object
      private:
         byte Parameter[8]={0,4,5,9,13,17,18,19};
         //memory utilization
         // allow separation of float into bytes to read and write from FRAM
         union u_tag {
            uint8_t b[4]; 
            float fval;
         } u;

         
  //constructor
  public:
  MassFlowController(uint32_t MemoryStart)
  {
        StartMem=MemoryStart;
  }
  void Initialize(Adafruit_FRAM_SPI fram, byte ControllerNum)
  {
        uint32_t MemLoc;
        
        MemLoc=StartMem;
        Cnum=ControllerNum;
        cycle=0;
        //read values from FRAM
        this->fram=fram;
        SetValue=FloatRead(MemLoc);
        StoredSetValue=SetValue;
        MemLoc+=4;
        ONOFF=fram.read8(MemLoc);
        MemLoc+=1;
        FlowMax=FloatRead(MemLoc);
        MemLoc+=4;
        ProcValue=FloatRead(MemLoc);
        MemLoc+=4;
        CalValue=FloatRead(MemLoc);
        MemLoc+=4;
        PGas=fram.read8(MemLoc);
        MemLoc+=1;
        CGas=fram.read8(MemLoc);
        MemLoc+=1;
        Mode=fram.read8(MemLoc);
  }
  void SetFloatVal(float theValue, byte ParamID)
  {
       FloatWrite(StartMem+Parameter[ParamID],theValue);
       
  }
  void SetByteVal(uint8_t theValue, byte ParamID)
  {
       fram.writeEnable(true);
         fram.write8(StartMem+Parameter[ParamID], theValue);
       fram.writeEnable(false);
  }
  float GetFlow(float Channel_Voltage)
  {
    float result;
    result=floor(100*(Channel_Voltage/5.0)*FlowMax*(ProcValue/CalValue))/100; //mass flow controller signal is 0-5V, 5 is maximum flow
    //flow is scaled by process gas to N2 divided by calibration gas to N2
    //truncate to 2 decimal places without adding the trun() in math.h
    return result;
  }
  private:
  float FloatRead(uint32_t Address)
  {
    u.fval=0;
    //read floating point numer
    float result;
    fram.read(Address,u.b,4);
    result=u.fval;
    return result;
  }
  void FloatWrite(uint32_t Address, float theValue)
  {
    u.fval=theValue;
    fram.writeEnable(true);
      fram.write(Address,u.b,4);  // take array of 4 bytes that is float and write to 4 memory locations
    fram.writeEnable(false); 

  }
 };

//Static text and pins used for keypad
char keys[ROWS][COLS] = {
  {'1','2','3','A'},  
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {10, 11, 12, 13}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {14, 15, 16, 17}; //connect to the column pinouts of the keypad

//Static text used in menus
String StaticText[8][4]={  //X is menue number, Y is display row
  {"A F       S","A F       S","A F       S","A F       S"},
  {" Controller I "," Set point      "," Gas    "," Settings"},
  {"Controller I Params"," SCCM Max :        "," Procs Gas:    "," Program"},
  {"Process Gas"," Ar   Air  CH4   Ctm"," H2   N2   C3H8  Cal"," CO   CO2  C3H6 "},
  {"Calibration Gas"," Ar   Air  CH4   Ctm"," H2   N2   C3H8"," CO   CO2  C3H6 "},
  {"New Process Gas","Enter Factor Rel N2","     "," "},
  {"New Cal Gas","Enter Factor Rel N2","     "," "},
  {" Step :  Cntrl  "," Set  :       sccm"," State:   "," Time :       min"}
};
String ProcGas[10] = {"Ar","H2","CO","Air","N2","CO2","CH4","C3H8","C3H6","Custom"};
float PgasValues[9]={1.415,1.010,1.000,1.006,1.000,0.737,0.719,0.348,0.398}; //conversion factor relative to nitrogen
String CalGas[10] = {"Ar","H2","CO","Air","N2","CO2","CH4","C3H8","C3H6","Custom"};
float CgasValues[9]={1.415,1.010,1.000,1.006,1.000,0.737,0.719,0.348,0.398}; //conversion factor relative to nitrogen
char Controller[4]= {'A','B','C','D'};
char ONOFFLabel[2]={'N','Y'};
char ModeLabel[2]={'M','P'};  //manual or program - not yet enabled

//FRAM memory start locations


//menu behaviors
//rows which allow selector '*' to be displayed in each menu
boolean ActiveRows[3][4]= { //row=submenu column =active or inactive row in submenu
  {0,0,0,0},
  {1,1,0,1},
  {0,1,1,0}
};
byte CurrMen=0; //the current menu
byte ControlSelect=0; //controller selected
byte SelectPos=3; //position of selection indicator in menu1
unsigned long TimeStart;

//RS485 communication parameters
String inputString = "";         // a string to hold incoming data
String OutputString = "";         // a string to hold incoming data
byte IOPin=22; //digital pin 22 is the listen/talk select for LMC485 chip (pins 2 and 3 see circuit schematic)
bool IOMode=false; //0 is listen, 1 is talk
bool stringComplete = false; //a complete command was sent when true
byte ControllerNumber=0; //address of the controller here set to 0, allow changing in future versions for multiple boxes daisy chained.
#define NumCommands 4 //the number of elements in the command array
//add Ln as listen command to select controller on chain n can be from 0-9
char Commands[NumCommands] = {'S','R','T','V'}; //(S)et flow, (R)ead flow, (T)oggle MFC ON/OFF, (V)alve ON/OFF
char theCommand; //hold the command to be executed from serial communication


Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(FRAM_SCK, FRAM_MISO, FRAM_MOSI, FRAM_CS); //initiallize FRAM object

//four controllers with current circuit design can be expanded but human interface becomes more complicated
MassFlowController MFC[4] = {
  MassFlowController(128),
  MassFlowController(165),
  MassFlowController(202),
  MassFlowController(239)
};


//OLED_V2 for newer displays
Newhaven_CharacterOLED lcd(OLED_V2, 43, 41, 39, 37, 35, 33, 31); 
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */

void setup() 
{
  // Print a message to the LCD.
  int i;
  
  lcd.begin(20, 4);
  lcd.print("Initializing");
  
  Serial.begin(9600);
  Serial1.begin(9600); //RS485 is on serial port 1
  pinMode(IOPin, OUTPUT); //enable talk/listen selector for LMC485
  digitalWrite(IOPin,IOMode); //Start in listen mode
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
  
  //initialize non-volatile memory
  if (fram.begin()) {
    Serial.println("Found SPI FRAM");
   } else {
    Serial.println("No SPI FRAM found ... check your connections\r\n");
    while (1);
   }
  // Read the first byte
  uint8_t test = fram.read8(0x0);
  Serial.print("Restarted "); Serial.print(test); Serial.println(" times");
  
  // Test write ++ show counter on times opened
  fram.writeEnable(true);
  fram.write8(0x0, test+1);
  fram.writeEnable(false);

  fram.writeEnable(true);
  fram.write(0x1, (uint8_t *)"FTW!", 5);
  fram.writeEnable(false);
  
  //initialize 16-bit ADC
  ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  ads.begin();
  
  i=0;
  while (i<4) {
    MFC[i].Initialize(fram,i);
    i+=1;
  }
  TimeStart=millis();
  
  Menu0();
  delay(2000);
}

void loop() 
{
  byte i,j,theCommand;
  int16_t ReadRaw;
  float ReadFloat;
  float NewFlow;
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  
  //check flow and update display every 100 ms
  if (CurrMen==0) { //only update on main menu
     i=0;
     if (TimeStart>millis()) {
       TimeStart=millis(); //catch rollover of millis90 function appx every 50 days.
     }
     if ((millis()-TimeStart)>1000) { //may need to tune is readout too slow or fast
       while (i<4) {
         if (MFC[i].ONOFF) {
           ReadRaw=ads.readADC_SingleEnded(i);
           ReadFloat=6.144*(float(ReadRaw)/65535); //analog in set to max of 6.144 by 2/3 scaling see ADC chip ADS1115 documentation
           NewFlow=MFC[i].GetFlow(ReadFloat);
           if (NewFlow!=MFC[i].ReadValue) { //may need to add tolerance to prevent over updating
             MFC[i].ReadValue=NewFlow;
             lcd.setCursor(4, i);
             lcd.print(Padfloat(MFC[i].ReadValue,3,1));//display number as xxx.x
           }
         }  
       i+=1;
       TimeStart=millis();
       }
    }
  }
  
  char key = keypad.getKey();
  
  //Serial.print(keypad.bitMap[0,0]);
  //Serial.print(" ");
  //Serial.print(keypad.bitMap[0,1]);
  //Serial.print(" ");
  //Serial.print(keypad.bitMap[0,2]);
  //Serial.print(" ");
  //Serial.println(keypad.bitMap[0,3]);
  //Serial.println();
  
  if (key){
    //menu 0 behavior
    if ((key>64)&&(CurrMen==0)) {  //navigate to individual controller menu
      CurrMen=1;
      ControlSelect=(key-65);
      key=' '; //set key to unused value to debounce before navigating
      Menu1();
    }

    if ((key<64)&&(CurrMen==0)) {
    //keypad.bitmap[0,n] if non-zero its the row being pressed, value is 8 bit 2^n where n is the column, bitmap is array of (4) 4 bit numbers
      j=0;
      while (j<4) {
        if (keypad.bitMap[0,j]>0) {
          ControlSelect=j; //find row
          theCommand=theColumn(keypad.bitMap[0,j]);
          MenuMacro0(theCommand);
        }
        j+=1;
      }
    }
    //end menu 0 behavior
    
    // menu 1 behavior
    if (CurrMen==1) { //define menu behavior
      switch (key) {
        case 'D':
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=0;
          Menu0();
        break;
        case 'A':
          MoveSelector();
        break;
        case 'B':
          key=' '; //set key to unused value to debounce before navigating
          Menu1Select();
        break;
        case 'C': //in submenus C takes user back to main menu
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=0;
          Menu0();
        break;
        default:
        break;
      } 
    }
    // menu 2 behavior
    if (CurrMen==2) { //define menu behavior
      switch (key) {
        case 'D':
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=1;
          Menu1();
        break;
        case 'A':
          MoveSelector();
        break;
        case 'B':
          key=' '; //set key to unused value to debounce before navigating
          Menu2Select();
        break;
        case 'C': //in submenus C takes user back to main menu
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=0;
          Menu0();
        break;
        default:
        break;
      } 
    }
    
    // begin menu 3 behavior
    if (CurrMen==3) { //define menu behavior
      switch (key) {
        case 'D':
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=2;
          Menu2();
        break;
        case 'A':
          MoveSelectorCol(11);
        break;
        case 'B':
          key=' '; //set key to unused value to debounce before navigating
          if (SelectPos<9) { //last two items 10 and 11 are set a new gas and set calibration gas
            GasSelect(0);
            CurrMen=2;
            Menu2();
          }
          else if (SelectPos==9) { //behavior for entering new process gas
          key=' ';
          CurrMen=5;
          Menu5();
          }
          else { //navigate to calibration gas menu
            key=' '; //set key to unused value to debounce before navigating
            CurrMen=4;
            Menu4();
          }
        break;
        case 'C': //in submenus C takes user back to main menu
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=0;
          Menu0();
        break;
        default:
        break;
      } 
    }
  //end menu 3 behavior
  // begin menu 4 behavior
  if (CurrMen==4) { //define menu behavior
    switch (key) {
        case 'D':
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=3;
          Menu3();
        break;
        case 'A':
          MoveSelectorCol(10);
        break;
        case 'B':
          key=' '; //set key to unused value to debounce before navigating
          if (SelectPos<9) { //last  item 9 is to set a new calibration gas
            GasSelect(1);
            CurrMen=3;
            Menu3();
          }
          else  { //behavior for entering new calilbration gas
          key=' ';
          CurrMen=6;
          Menu6();
          }
        break;
        case 'C': //in submenus C takes user back to main menu
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=0;
          Menu0();
        break;
        default:
        break;
      } 
    }
  //end menu 4 behavior
  
  // begin menu 5 behavior
  if (CurrMen==5) { //define menu behavior
    switch (key) {
        case 'D':
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=3; //go back to process gas menu
          Menu3();
          
        break;
        case 'B':
          key=' '; //set key to unused value to debounce before navigating
          Menu5Select();
        break;
        case 'C': //in submenus C takes user back to main menu
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=0;
          Menu0();
        break;
        default:
        break;
      } 
    }
  //end menu 5 behavior
  
  // begin menu 6 behavior
  if (CurrMen==6) { //define menu behavior
    switch (key) {
        case 'D':
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=4; //go back to calibration gas menu
          Menu4();
          
        break;
        case 'B':
          key=' '; //set key to unused value to debounce before navigating
          Menu6Select();
        break;
        case 'C': //in submenus C takes user back to main menu
          key=' '; //set key to unused value to debounce before navigating
          CurrMen=0;
          Menu0();
        break;
        default:
        break;
      } 
    }
  //end menu 6 behavior

      
  }
}

//void InitializeValues()
   //add memory read to read in all values
   //just a placeholder now to set constants
//{ 
//  int i;
//  for (i=0;i<Controllers;i++) {
//     SetValues[i]=50;
//     ReadValues[i]=50;
//     FlowMax[i]=50;
     //ONOFF[i]=0;
//     ProcValue[i]=1.0;
//     CalValue[i]=1.0;
//     PGas[i]={i};
//     CGas[i]={i};
     //give feedback on loading status mostly artificial delay
//     delay(50);
//     lcd.setCursor(12+i, 0);
//     lcd.print(".");
//  }
//}

void Menu0() {
  
  ControlSelect=0; //clear controlselect
  //display main menu
  lcd.command(0x0C);//cursor off blink off
  int i;
  LCDQuickClear();
    
  for (i=0;i<Controllers;i++) {
    lcd.setCursor(0, i);
    lcd.print(StaticText[0][i]);
    lcd.setCursor(0, i);
    lcd.print(Controller[i]);
    if (MFC[i].ONOFF) {
       lcd.setCursor(4, i);
       lcd.print(Padfloat(MFC[i].ReadValue,3,1));
       lcd.setCursor(12, i);
       lcd.print(Padfloat(MFC[i].SetValue,3,1));
       lcd.setCursor(18, i);
       lcd.print(ModeLabel[MFC[i].Mode]);
    }
    else {
       lcd.setCursor(4, i);
       lcd.print(" OFF "); //turn flow read off to indicate controller is off
       lcd.setCursor(12, i);
       lcd.print(Padfloat(MFC[i].SetValue,3,1)); //still diplay the set point
       lcd.setCursor(18, i);
       lcd.print(ModeLabel[MFC[i].Mode]);
    }   
  }
}

void MenuMacro0(byte theCommand) {

  byte a;
  
  switch (theCommand) {
    case 0:
      if (MFC[ControlSelect].ONOFF) { //only perform if controller is ON;
        MFC[ControlSelect].cycle+=1; //increment cycle
        if (MFC[ControlSelect].cycle>3) {
          MFC[ControlSelect].cycle=0; //restart cycle
        }
        switch (MFC[ControlSelect].cycle) {
          case 0:
            MFC[ControlSelect].SetValue=MFC[ControlSelect].StoredSetValue;
          break;
          case 1:
            MFC[ControlSelect].SetValue=0;
          break;
          case 2:
            MFC[ControlSelect].SetValue=MFC[ControlSelect].FlowMax;
          break;
          case 3:
            MFC[ControlSelect].SetValue=0;
          break;
        }
      }
    break;
    case 1:
       MFC[ControlSelect].ONOFF=!MFC[ControlSelect].ONOFF; //turn on/off
    break;
    case 2:
       switch (ControlSelect) {
        case 0:
          a=0;
          while (a<4) {
             if (MFC[a].ONOFF) { //only if on
                MFC[a].SetValue=MFC[a].FlowMax; //set all to max flow
                MFC[a].cycle=2;
             }
           a+=1;
          }
        break;
        case 1:
          a=0;
          while (a<4) {
             if (MFC[a].ONOFF) { //only if on
                MFC[a].SetValue=0; //set all to 0 flow
                MFC[a].cycle=3;
             }
           a+=1;
          }
        break;
        case 2:
          a=0;
          while (a<4) {
             if (MFC[a].ONOFF) { //only if on
                MFC[a].SetValue=MFC[a].StoredSetValue; //set all to the setflow flow
                MFC[a].cycle=0;
             }
           a+=1;
          }
        break;
       }
    break;
    default:
    break;
  }
  Menu0();
}

void Menu1() {
  LCDQuickClear();
  lcd.command(0x0C);//cursor off blink off
  //delay(100);
  lcd.setCursor(0, 0);
  lcd.print(StaticText[1][0]);
  lcd.setCursor(12, 0);
  lcd.print(Controller[ControlSelect]); //row 1
  lcd.setCursor(14, 0);
  if (MFC[ControlSelect].ONOFF) {
    lcd.print(" ON");
  }
  else {
    lcd.print(" OFF");
  }
  lcd.setCursor(0, 1);
  lcd.print(StaticText[1][1]);
  lcd.setCursor(11, 1);
  lcd.print(Padfloat(MFC[ControlSelect].SetValue,2,2)); //row 2
  lcd.setCursor(0, 2);
  lcd.print(StaticText[1][2]);
  lcd.setCursor(5, 2);
  lcd.print(ProcGas[MFC[ControlSelect].PGas]); //row 3
  lcd.setCursor(0, 3);
  lcd.print(StaticText[1][3]); //row 4
  lcd.setCursor(0, 0);
  lcd.print("*"); //add selector
  SelectPos=0;
}

void Menu2() {
  LCDQuickClear();
  lcd.command(0x0C);//cursor off blink off
  //delay(100);
  lcd.setCursor(0, 0);
  lcd.print(StaticText[2][0]);
  lcd.setCursor(11, 0);
  lcd.print(Controller[ControlSelect]); //row 1
  lcd.setCursor(0, 1);
  lcd.print(StaticText[2][1]); //row 2
  lcd.setCursor(13,1);
  lcd.print(MFC[ControlSelect].FlowMax);
  lcd.setCursor(0, 2);
  lcd.print(StaticText[2][2]); 
  lcd.setCursor(12, 2);
  lcd.print(ProcGas[MFC[ControlSelect].PGas]); //row 3
  lcd.setCursor(0, 3);
  lcd.print(StaticText[2][3]); //row 4
  lcd.setCursor(0, 1);
  lcd.print("*"); //add selector
  SelectPos=1;
}

void Menu3() {
  byte column;
  byte row;
  byte columnarray[]={0,5,10,16}; //location of columns on screen
  
  SelectPos=MFC[ControlSelect].PGas;
  column=columnarray[SelectPos/3];
  row=1+(SelectPos % 3); //mod value is row position start row is always 1
  
  LCDQuickClear();
  lcd.command(0x0C);//cursor off blink off
  //delay(100);
  lcd.setCursor(0, 0);
  lcd.print(StaticText[3][0]);
  lcd.setCursor(12, 0);
  lcd.print(Controller[ControlSelect]); //row 1
  lcd.setCursor(0, 1);
  lcd.print(StaticText[3][1]); //row 2
  lcd.setCursor(0, 2);
  lcd.print(StaticText[3][2]); //row 3
  lcd.setCursor(0, 3);
  lcd.print(StaticText[3][3]); //row 4
  lcd.setCursor(column, row);
  lcd.print("*"); //add selector
  
}

void Menu4() {
  byte column;
  byte row;
  byte columnarray[]={0,5,10,16}; //location of columns on screen
  
  SelectPos=MFC[ControlSelect].CGas;
  column=columnarray[SelectPos/3];
  row=1+(SelectPos % 3); //mod value is row position start row is always 1
  
  LCDQuickClear();
  lcd.command(0x0C);//cursor off blink off
  //delay(100);
  lcd.setCursor(0, 0);
  lcd.print(StaticText[4][0]);
  lcd.setCursor(16, 0);
  lcd.print(Controller[ControlSelect]); //row 1
  lcd.setCursor(0, 1);
  lcd.print(StaticText[4][1]); //row 2
  lcd.setCursor(0, 2);
  lcd.print(StaticText[4][2]); //row 3
  lcd.setCursor(0, 3);
  lcd.print(StaticText[4][3]); //row 4
  lcd.setCursor(column, row);
  lcd.print("*"); //add selector
  
}

void Menu5() {

  LCDQuickClear();
  lcd.command(0x0C);//cursor off blink off
  //delay(100);
  lcd.setCursor(0, 0);
  lcd.print(StaticText[5][0]);
  lcd.setCursor(16, 0);
  lcd.print(Controller[ControlSelect]); //row 1
  lcd.setCursor(0, 1);
  lcd.print(StaticText[5][1]); //row 2
  lcd.setCursor(0, 2);
  lcd.print(Padfloat(MFC[ControlSelect].ProcValue,1,3)); //row 3
  lcd.setCursor(0, 3);
  lcd.print(StaticText[5][3]); //row 4
  
}

void Menu6() {

  LCDQuickClear();
  lcd.command(0x0C);//cursor off blink off
  //delay(100);
  lcd.setCursor(0, 0);
  lcd.print(StaticText[6][0]);
  lcd.setCursor(12, 0);
  lcd.print(Controller[ControlSelect]); //row 1
  lcd.setCursor(0, 1);
  lcd.print(StaticText[6][1]); //row 2
  lcd.setCursor(0, 2);
  lcd.print(Padfloat(MFC[ControlSelect].CalValue,1,3)); //row 3
  lcd.setCursor(0, 3);
  lcd.print(StaticText[6][3]); //row 4
  
}
   

String Padfloat(float PadNum,int before,int after)
  {
    String PaddedNum;
    String Beforepad="";
    int Places=1;
    int i=0;
    
    PaddedNum=String(PadNum,after);
    Places=int(log(PadNum)/log(10.0));
    for (i=0;i<(before-Places-1);i++) {
      Beforepad=Beforepad+" ";
    }
    PaddedNum=Beforepad+PaddedNum;
    return PaddedNum;
  }
 
void LCDQuickClear() {
  int i;
  
  for (i=0;i<5;i++) {
    lcd.setCursor(0, i);
    lcd.print("                    ");
  }
}

void  MoveSelector() {
  lcd.setCursor(0, SelectPos);
  lcd.print(" ");
  SelectPos+=1;
  if (SelectPos>3) {
    SelectPos=0;
  }
  if(!ActiveRows[CurrMen][SelectPos]) {
    while (!ActiveRows[CurrMen][SelectPos]) {
       SelectPos+=1;
       if (SelectPos>3) {
          SelectPos=0;
        }
    }
  }
  lcd.setCursor(0, SelectPos);
  lcd.print("*");
}

void  MoveSelectorCol(byte elements) {
  
  byte column;
  byte row;
  byte columnarray[]={0,5,10,16}; //location of columns on screen
  
  
  column=columnarray[SelectPos/3];
  row=1+(SelectPos % 3); //mod value is row position start row is always 1
    
  lcd.setCursor(column, row);
  lcd.print(" ");
  SelectPos+=1;
  if (SelectPos>=elements) {
    SelectPos=0;
  }
  column=columnarray[SelectPos/3];
  row=1 + (SelectPos % 3); //mod value is row position start row is always 1
  lcd.setCursor(column, row);
  lcd.print("*");
}

void GasSelect(byte gtype) { //gtype=0 process gas, gytpe=1 calibration gas
  
  switch (gtype) {
    case 0:
      MFC[ControlSelect].PGas=SelectPos;
      MFC[ControlSelect].SetByteVal(MFC[ControlSelect].PGas,5);  //write value to FRAM
      //read memory and set pgas value in float array
    break;
    case 1:
      MFC[ControlSelect].CGas=SelectPos;
      MFC[ControlSelect].SetByteVal(MFC[ControlSelect].CGas,6);  //write value to FRAM
      //read memory and set cgas value in float array
    break;
  }
}
  


void Menu1Select() { //control behavior in Controller submenu 1 - individual controller settings
  
  byte CursPos; //the cursor position
  byte valuearray[4]={0,0,0,0}; //value being entered
  byte i=0;
  byte j=0;
  char key;
  byte left=2;
  byte right=2;
  
  switch (SelectPos) {
    case 0:  //turn controller on and off
      MFC[ControlSelect].ONOFF=!MFC[ControlSelect].ONOFF;
      MFC[ControlSelect].SetByteVal(MFC[ControlSelect].ONOFF,1); //write value to FRAM
      lcd.setCursor(14, 0);
      if (MFC[ControlSelect].ONOFF) {
        lcd.print(" ON ");
       }
      else {
        lcd.print(" OFF");
       }
     break;
     case 1: //enter flow value
       MakeArray(valuearray,MFC[ControlSelect].SetValue,2,2); //store value being edited in number array
       CursPos=11;
       lcd.setCursor(CursPos, 1);
       lcd.command(0x0F); //cursor on and blink
       key = keypad.getKey();
       while ((key!='*')||(key!='#')) {  //repeat until exit (*) or enter (#)
          key = keypad.getKey();
          if ((key>47)&&(key<58)) {  //only enter numbers
            lcd.print(key);
              valuearray[j]=float((key-48));
              i+=1; //cursor position
              j+=1; //different indices for array and display because of decimal skipping
              //advance to next position jump over decimal point - left digits before point and right after
              if (i==left) { //jump decimal point
                i+=1;
                }
              if (i>(left+right)) {
                i=0;
              }
              if (i==0) {
                j=0;
              }
              lcd.setCursor(CursPos+i, 1); //move entry point
          }
          if (key=='*') {
            lcd.command(0x0C); //turn off blink
            lcd.setCursor(11, 1); //set position to location of number start
            lcd.print(Padfloat(MFC[ControlSelect].SetValue,2,2)); //put original value back
            break;
          }
          
          if (key=='#') {
            lcd.command(0x0C); //turn off blink add code to encode data
            MFC[ControlSelect].SetValue=MakeFloat(valuearray,2,2); //parse nn.nn value
            if (MFC[ControlSelect].SetValue>MFC[ControlSelect].FlowMax) { //check is value is larger than flow capacity of controller
              MFC[ControlSelect].SetValue=MFC[ControlSelect].FlowMax; //set flow value to maximum flow
              lcd.setCursor(11, 1); //set position to location of number start
              lcd.print(Padfloat(MFC[ControlSelect].SetValue,2,2)); //write new value to screen
            }
            MFC[ControlSelect].SetFloatVal(MFC[ControlSelect].SetValue,0);  //write value to FRAM
            MFC[ControlSelect].StoredSetValue=MFC[ControlSelect].SetValue; //write set value in untouched space for macro cycling
            //SetFlow(SetValues[ControlSelect],ControlSelect); //Set the output of the Analog out routine to be added
            break;
          }
          }  
     break;
     case 3:
       key=' '; //set key to unused value to debounce before navigating
       CurrMen=2;
       Menu2();
     break;
    default:
    break;
  }
}

void Menu2Select() { //control behavior in Controller submenu 2 - individual controller parameters
  
  byte CursPos; //the cursor position
  byte valuearray[4]={0,0,0,0}; //value being entered
  byte i=0;
  int j=0;
  char key;
  byte left=2;
  byte right=2;
  
  switch (SelectPos) {
    
    case 0: 
    break; 
    
    case 1://enter max flow of controller
       MakeArray(valuearray,MFC[ControlSelect].FlowMax,2,2); //store value being edited in number array
       CursPos=13;
       lcd.setCursor(CursPos, 1);
       lcd.command(0x0F); //cursor on and blink
       key = keypad.getKey();
       while ((key!='*')||(key!='#')) {  //repeat until exit (*) or enter (#)
          key = keypad.getKey();
          if ((key>47)&&(key<58)) {  //only enter numbers
            lcd.print(key);
              valuearray[j]=float((key-48));
              i+=1; //cursor position
              j+=1; //different indices for array and display because of decimal skipping
              //advance to next position jump over decimal point - left digits before point and right2 after
              if (i==left) { //jump decimal point
                i+=1;
                }
              if (i>(left+right)) {
                i=0;
              }
              if (i==0) {
                j=0;
              }
              lcd.setCursor(CursPos+i, 1); //move entry point
          }
          if (key=='*') {
            lcd.command(0x0C); //turn off blink
            lcd.setCursor(13, 1); //set position to location of number start
            lcd.print(Padfloat(MFC[ControlSelect].FlowMax,2,2)); //put original value back
            break;
          }
          
          if (key=='#') {
            lcd.command(0x0C); //turn off blink add code to encode data
            MFC[ControlSelect].FlowMax=MakeFloat(valuearray,2,2); //parse nn.nn value
            MFC[ControlSelect].SetFloatVal(MFC[ControlSelect].FlowMax,2); //write value to FRAM
            break;
          }
          }  
    break;
    
    case 2:
      key=' '; //set key to unused value to debounce before navigating
      CurrMen=3;
      Menu3();
    break;
    
    default:
    break;
  }
}

void Menu5Select() { //control behavior in Controller submenu 5 - Custom process gas
  
  byte CursPos; //the cursor position
  byte valuearray[4]={0,0,0,0}; //value being entered
  byte i=0;
  int j=0;
  char key;
  byte left=1;
  byte right=3;
  
  //Enter new Process gas parameter
  MakeArray(valuearray,MFC[ControlSelect].ProcValue,1,3); //store value being edited in number array
  CursPos=0;
  lcd.setCursor(CursPos, 2);
  lcd.command(0x0F); //cursor on and blink
  key = keypad.getKey();
  while ((key!='*')||(key!='#')) {  //repeat until exit (*) or enter (#)
     key = keypad.getKey();
     if ((key>47)&&(key<58)) {  //only enter numbers
       lcd.print(key);
       valuearray[j]=float((key-48));
       i+=1; //cursor position
       j+=1; //different indices for array and display because of decimal skipping
              //advance to next position jump over decimal point - left digits before point and right after
       if (i==left) { //jump decimal point
         i+=1;
       }
       if (i>(left+right)) {
         i=0;
              }
       if (i==0) {
         j=0;
       }
       lcd.setCursor(CursPos+i, 2); //move entry point
     }
     if (key=='*') {
       lcd.command(0x0C); //turn off blink
       lcd.setCursor(0, 2); //set position to location of number start
       lcd.print(Padfloat(MFC[ControlSelect].ProcValue,1,3)); //put original value back
       break;
      }
          
     if (key=='#') {
       lcd.command(0x0C); //turn off blink add code to encode data
       MFC[ControlSelect].ProcValue=MakeFloat(valuearray,1,3); //parse n.nnn value
       MFC[ControlSelect].SetFloatVal(MFC[ControlSelect].ProcValue,3);  //write value to FRAM
       MFC[ControlSelect].PGas=9;//set process gas label to Ctm (custom)
       MFC[ControlSelect].SetByteVal(MFC[ControlSelect].PGas,5);  //write value to FRAM
       break;
     }
   }  
}

void Menu6Select() { //control behavior in Controller submenu 6 - Custom calibration gas
  
  byte CursPos; //the cursor position
  byte valuearray[4]={0,0,0,0}; //value being entered
  byte i=0;
  int j=0;
  char key;
  byte left=1;
  byte right=3;
  
  //Enter new Calibration gas parameter
  MakeArray(valuearray,MFC[ControlSelect].CalValue,1,3); //store value being edited in number array
  CursPos=0;
  lcd.setCursor(CursPos, 2);
  lcd.command(0x0F); //cursor on and blink
  key = keypad.getKey();
  while ((key!='*')||(key!='#')) {  //repeat until exit (*) or enter (#)
     key = keypad.getKey();
     if ((key>47)&&(key<58)) {  //only enter numbers
       lcd.print(key);
       valuearray[j]=float((key-48));
       i+=1; //cursor position
       j+=1; //different indices for array and display because of decimal skipping
              //advance to next position jump over decimal point - left digits before point and right after
       if (i==left) { //jump decimal point
         i+=1;
       }
       if (i>(left+right)) {
         i=0;
              }
       if (i==0) {
         j=0;
       }
       lcd.setCursor(CursPos+i, 2); //move entry point
     }
     if (key=='*') {
       lcd.command(0x0C); //turn off blink
       lcd.setCursor(0, 2); //set position to location of number start
       lcd.print(Padfloat(MFC[ControlSelect].CalValue,1,3)); //put original value back
       break;
      }
          
     if (key=='#') {
       lcd.command(0x0C); //turn off blink add code to encode data
       MFC[ControlSelect].CalValue=MakeFloat(valuearray,1,3); //parse n.nnn value
       MFC[ControlSelect].SetFloatVal(MFC[ControlSelect].CalValue,4);  //write value to FRAM
       MFC[ControlSelect].CGas=9;//set calibration gas label to Ctm (custom)
       MFC[ControlSelect].SetByteVal(MFC[ControlSelect].CGas,6);  //write value to FRAM
       break;
     }
   }  
}

float MakeFloat(byte numarray[], byte whole, byte fraction ) {
  float theValue;
  
  byte i,j;
  
  i=1;
  j=1;
  theValue=0;
  
  while (i<=(whole+fraction)) {
    if (i<=whole) {
       theValue+=numarray[i-1]*pow(10.0,(whole-i));
    }
    else {
      theValue+=numarray[i-1]*pow(10.0,(-j));
      j+=1;
    }
  i+=1;
  }
  return theValue;
}

void MakeArray(byte numarray[], float VarValue, byte whole, byte fraction ) {
  //arrays are passed as pointers modifying array in this procedure modifies the "passed" array
  //find the coefficient of each numbers place and store in array.  Allow correct data
  //to be displayed when less than all of the digits have been edited.
  byte i,j;
  float remValue;
  float passValue;
  
  i=0;
  j=0;
  
  passValue=VarValue;
  while (i<(whole+fraction)) {
    if (i<whole) {
      remValue=floor(passValue/pow(10.0,(whole-i-1)));
      passValue=passValue-remValue*(pow(10.0,(whole-i-1)));
      numarray[i]=remValue;
    }
    else {
      if (i==whole) {
        passValue=passValue*pow(10.0,fraction);
      }
      remValue=floor(passValue/pow(10.0,(fraction-j-1)));
      passValue=passValue-remValue*(pow(10.0,(fraction-j-1)));
      numarray[i]=remValue;
      j+=1;
    }
  i+=1;
  }
}

byte theColumn(byte value) {
   byte index,returnvalue;
   bool test;

   index=0;
   returnvalue=0;
   while (index<3) { //number of columns in keypad
    test=bitRead(value,index);
       if (test) {
         returnvalue=index;
       }
    index+=1;
   }
   return returnvalue;
}

int CommandLoc(char input)  //find array index of the command
{
  int i,CLoc;
  int commandArraySize=NumCommands;
  
  i=0;
  CLoc=-1;
  
  while (i<commandArraySize) {
    if (input==Commands[i]){
      CLoc=i;
    }
    i+=1;
  }
  
  return CLoc;
}

byte GenerateChecksum(String SendString) { //2's complement of 8 bit addition modulo 256 of arbitrary length string
  byte Checksum8bit;
  int CheckSum;
  byte i;
  byte thebytes[12]={};
  
  i=0;
  CheckSum=0;
  SendString.getBytes(thebytes,SendString.length()+1); //need length +1 to get all of the string
  i=0;
  while (i<SendString.length()) { //add all the ASCII values in the string ignore last value which is zero
    CheckSum=CheckSum+thebytes[i];
    i+=1;
  }
  CheckSum=CheckSum % 256; //convert to single 8 bit number by mod 256
  CheckSum=-CheckSum; //generate 2's complement
  Checksum8bit=lowByte(CheckSum); //grab last 8 bits
  return Checksum8bit;
}

int StrToHex(char str[]) //convert 2 character string of ASCII to its hex equivalent
{
  return (int) strtol(str, 0, 16);
}


bool CheckChecksum(String ReceiveString) {  //verify checksum of arbitrary length string
  bool isGood;
  byte CheckSum1,CheckSum2,CheckSum3;
  int CheckSum;
  byte i;
  char chksum[2]={};
  byte checkbytes[12]={}; //maximum command length with 10 characters + 1 check byte (2 characters HEX)

  isGood=false;
  CheckSum=0;
  ReceiveString.getBytes(checkbytes,ReceiveString.length()); //omit carriage return
  i=0;
  while (i<(ReceiveString.length()-3)) { //first bytes are the message, last three are checksum and <CR>
    CheckSum=CheckSum+checkbytes[i];
    i+=1;
  }
  CheckSum1=CheckSum % 256; //added values from original message mod 256
  chksum[0]=checkbytes[(ReceiveString.length()-3)];//get first digit of checksum
  chksum[1]=checkbytes[(ReceiveString.length()-2)]; //get second digit of checksum
  CheckSum2=StrToHex(chksum); //convert hex values in ASCII format to integer
  CheckSum3=CheckSum1+CheckSum2; //correct check sum will make this value 0
  
  if (CheckSum3==0) {
    isGood=true;
  }
  return isGood;
}

String PadMessage(String theText, int thelength) { //add zeros at end to ensure constant length message going out
  String paddedString;
  int Strlength,i;

  paddedString=theText;
  
  Strlength=theText.length();
  i=0;
  while (i<(thelength-Strlength)) {
    paddedString=paddedString+"0";
    i+=1;
  }
  
  return paddedString;
}

boolean CommandOK(char input) //check that command character is in list of defined commands
{
  int i;
  int commandArraySize=NumCommands;
  boolean GoodCommand;
  
  GoodCommand=false;
  i=0;
  while (i<commandArraySize) {
    if (input==Commands[i]) {
      i=commandArraySize;
      GoodCommand=true;
    }
    i+=1;
  }
  
  return GoodCommand;
}


void DoSerialCommand() {
  int value;
  int valueTest;
  int MFCnum;
  int Tval;
  byte m,n;
  byte readArray[5]={}; //array to hold digits read
  int16_t ReadRawFlow;
  float ReadFloatFlow;
  String FlowSend;
  String StatusSend;
  bool badfloat=false;
  
    if (stringComplete) {
      if (CheckChecksum(inputString)) { //maske sure command sent is error free
        theCommand=inputString.charAt(0);
        if (theCommand='L') {  
          theCommand=inputString.charAt(1);
          value=theCommand-48; //convert ascii character to number 
          if (value==ControllerNumber) { //ignore if not addressing the controller
             //the commands are :(S)et flow, (R)ead flow, (T)oggle MFC ON/OFF, (V)alve ON/OFF
             //the command structure is
             //set flow: LnSmXXX.XX<CR>, n is controller number here its set to 0 in the future this can be set in the menus, m is the mass flow controller 0-3, XXX.XX is the value to set if less than 3 
             //digits must add leading zeros , ex 10.10 sccm is sent as 010.10
             //read flow: LnRm<CR>,  n is controller number here its set to 0 in the future this can be set in the menus, m is the mass flow controller 0-3, writes XXX.XX to serial port
             //toggle MFC: LnTm0<CR>, n is controller number here its set to 0 in the future this can be set in the menus, m is the mass flow controller 0-3, controller OFF, 1 is ON
             //toggle Valve:LnVm0<CR> n is controller number here its set to 0 in the future this can be set in the menus, m is the mass flow controller 0-3, 0 is valve OFF, 1 is ON
             theCommand=inputString.charAt(23);
             MFCnum=inputString.charAt(3);
             value=MFCnum-48; //convert ascii character to number 
             if ((value<4)&&(value>=0)) { //error checking for incorrect command do nothing if command syntax is wrong
               switch (CommandLoc(theCommand)) {
                 case 0: //(S)et flow
                   readArray[5]={};
                   m=0;
                   n=4;
                   while (m<5) {
                     if (m==3) { //change here if flexible decimal place entry is desired may make control by external control easier to implement
                      n+=1; //skip over decimal point in the command
                     }
                     valueTest=inputString.charAt(n)-48; //test that only numbers are being sent
                     readArray[m]=valueTest;
                     if ((valueTest<0)&&(valueTest>9)) {
                      badfloat=true;
                     }
                     m+=1;
                     n+=1;
                   }

                   StatusSend=PadMessage("0",6); //8 byte response first byte acknowledges state change (6 character + 2 character checksum)
                   //error in setting the flow - bad value
                   
                   if (!badfloat) { //only set the value if a good floating point number was read
                    StatusSend=PadMessage("1",6);//8 byte response first byte acknowledges state change (6 character + 2 character checksum)
                    //flow was set
                     MFC[MFCnum].SetValue=MakeFloat(readArray,3,2); //parse nnn.nn value
                     if (MFC[MFCnum].SetValue>MFC[MFCnum].FlowMax) { //check is value is larger than flow capacity of controller
                       MFC[MFCnum].SetValue=MFC[MFCnum].FlowMax; //set flow value to maximum flow
                     }
                     MFC[MFCnum].SetFloatVal(MFC[MFCnum].SetValue,0);  //write value to FRAM
                     MFC[MFCnum].StoredSetValue=MFC[MFCnum].SetValue; //write set value in untouched space for macro cycling
                     //SetFlow(SetValues[ControlSelect],ControlSelect); //Set the output of the Analog out routine to be added
                     CurrMen=0;
                     Menu0(); //navigate back to main menu and refresh easier than staying on menu and at the moment I can't see a reason why the keypad and computer control would be used simultaneously
                   }
                   StatusSend=StatusSend+(String(GenerateChecksum(StatusSend),HEX)); //add checksum
                   //data sent will always be 8 bytes long this simplifies code on the receiving end shorter numbers are padded with zeros
                   IOMode=1; //1 is talk
                   digitalWrite(IOPin,IOMode); //switch to talk
                   Serial1.println(StatusSend); //write to RS485 port with <CR> as terminator
                   delay(10);
                   IOMode=0; //0 is listen
                   digitalWrite(IOPin,IOMode); //switch to listen
                 break;
                 case 1: //(R)ead flow;
                   ReadRawFlow=ads.readADC_SingleEnded(MFCnum);
                   ReadFloatFlow=6.144*(float(ReadRawFlow)/65535); //analog in set to max of 6.144 by 2/3 scaling see ADC chip ADS1115 documentation
                   FlowSend=String(MFC[MFCnum].GetFlow(ReadFloatFlow),2); //convert float value to flow using calibration data stored in controller and format as 2 digit precision string
                   if (FlowSend.length()<6) {
                    FlowSend=PadMessage(FlowSend,6);//pad message to 6 bytes for standard length makes data read easier
                   }
                    FlowSend=FlowSend+(String(GenerateChecksum(FlowSend),HEX)); //append checksum to sent value
                   //data sent will always be 8 bytes long this simplifies code on the receiving end shorter numbers are padded with zeros
                   IOMode=1; //1 is talk
                   digitalWrite(IOPin,IOMode); //switch to talk
                   Serial1.println(FlowSend); //write to RS485 port with <CR> as terminator
                   delay(10);
                   IOMode=0; //0 is listen
                   digitalWrite(IOPin,IOMode); //switch to listen
                 break;
                 case 2: //(T)oggle MFC
                   Tval=inputString.charAt(4)-48;
                   StatusSend="0";
                   if ((Tval<2)&&(Tval>=0)) {//error checking for incorrect command do nothing if command syntax is wrong
                     StatusSend="1";
                     if (Tval==0) {
                      MFC[MFCnum].ONOFF=false;
                     }
                     else {
                      MFC[MFCnum].ONOFF=true;
                     }
                   }
                   StatusSend=StatusSend+inputString.charAt(4);
                   StatusSend=PadMessage(StatusSend,6); //8 byte response first byte acknowledges state change (6 character + 2 character checksum)
                   StatusSend=StatusSend+(String(GenerateChecksum(StatusSend),HEX));
                   //data sent will always be 8 bytes long this simplifies code on the receiving end shorter numbers are padded with zeros
                   IOMode=1; //1 is talk
                   digitalWrite(IOPin,IOMode); //switch to talk
                   Serial1.println(StatusSend); //write to RS485 port with <CR> as terminator
                   delay(10);
                   IOMode=0; //0 is listen
                   digitalWrite(IOPin,IOMode); //switch to listen
                   CurrMen=0;
                   Menu0(); //navigate back to main menu and refresh easier than staying on menu and at the moment I can't see a reason why the keypad and computer control would be used simultaneously

                 break;
                 case 3:
                   //future implementaion may be rolled up in the toggle function as MFC off would imply the valve is closed as well.
                 break;
                 default:
                 break;
               }
           }
        }
      }

  }
 }
 // clear the string:
 inputString = "";
 stringComplete = false;
 delay(10); //need a delay to allow processing
 }



/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX1 managed to RS485 protocol by a LTC485 on the control board .  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
 
void serialEvent1() {
  while (Serial1.available()) {
    // get the new byte:
    char inChar = (char)Serial1.read();
    // add it to the inputString:
    inputString += inChar;
    if (inChar == '\r') {   //carriage return signal end of command
      stringComplete = true;
    }
  }
}




    
   
