/*

xyz controller for positioner plate sample/reader from Anet Prusa 3d printer with 4x20 display
uses serial event
Richard Blair September 2018

stepper controller:
 * Copyright (C)2015-2017 Laurentiu Badea
 * This file may be redistributed under the terms of the MIT license.
 * A copy of this license has been included with this distribution in the file LICENSE.

LiquidCrystal Library 

Library originally added 18 Apr 2008
by David A. Mellis
library modified 5 Jul 2009
by Limor Fried (http://www.ladyada.net)
example added 9 Jul 2009
by Tom Igoe
modified 22 Nov 2010
by Tom Igoe


An object oriented approach to a latched button
Jan 2015, for the Arduino forum, by MattS-UK
Compiled and tested on a Mega 2560 R3
Changed to two-state button by Richard Blair September 2018

An object oriented multibutton analog channel
Richard Blair September 2018


 */

#include <Arduino.h>
#include "A4988.h"
#include <LiquidCrystal.h>

//Button class, encapsulate the functionality of a latched two state button
class Button {
    protected:
        uint8_t _pin;                  //hardware pin number
        boolean _state;                //current pin state
        boolean _wasPressed;           //button latch
        boolean _prevState;             //state when previously polled
        uint16_t _startDebounce;       //start of debounce time
        
    public:
        uint16_t debounceMS;           //period before button latches
        
        Button(uint8_t pin);           //construct the button
        void poll(uint16_t mnow);     //call periodically to refresh the button state
        boolean prevState(void) { return _prevState; }    //return the last state - use to capture state change
        boolean wasPressed(void) { return _wasPressed; }    //return the latch state
        void storeState(boolean thestate);
        void reset(void);              //reset the latch
};

//constructor
Button::Button(uint8_t pin) {
  _pin = pin;
  _state = false;
  _startDebounce = 0;
  _wasPressed = false; 
  _prevState=false; //start with botton off
  debounceMS = 100;
}

//refresh the button state
void Button::poll(uint16_t mnow){
  
  //pull down resistors added cause the button to be LOW when open 
  _state = digitalRead(_pin);
  
  if ((_prevState)!=(_state)) { //only change change state once
    
    if((_startDebounce==0)) {
      _startDebounce = mnow;
    }
    else {
      //latch the button if it is still pressed when the debounce time expires
      if(mnow - _startDebounce > debounceMS) {
        _wasPressed = _state;
        _startDebounce=0;
      }
    }
  }
}

//keep track of the previous state
void Button::storeState(boolean thestate){
  _prevState=thestate;
}

//reset the button
void Button::reset(void) {
  _wasPressed = false;
  _prevState=false;
  _startDebounce = 0;
}


//Analog Button class, encapsulate the functionality of a latched two state button
class AnalogButton {
    protected:
        uint8_t _pin;                  //hardware pin number
        int _value;                //current pin state
        int _PressedValue;            //translate voltage to integer from 1-n where n is the number of buttons
        int _prevValue;               //state when previously polled
        uint16_t _startDebounce;       //start of debounce time
        
    public:
        uint16_t debounceMS;           //period before button latches
        
        AnalogButton(uint8_t pin);           //construct the button
        void poll(uint16_t mnow);     //call periodically to refresh the button state
        int prevValue(void) { return _prevValue; }    //return the last state - use to capture state change
        int PressedValue(void) { return _PressedValue; }    //return the button pressed
        void storeValue(int theValue);
        void reset(void);              //reset the latch
};

//constructor
AnalogButton::AnalogButton(uint8_t pin) {
  _pin = pin;
  _value = 1023;
  _startDebounce = 0;
  _PressedValue = 5; 
  _prevValue=5; //start with no Button Pressed
  debounceMS = 100;
}

//refresh the button state
void AnalogButton::poll(uint16_t mnow){
  
  int analog_value;
  //pull down resistors added cause the button to be LOW when open 
  _value = analogRead(_pin);
  //convert Analog signal 0-1023 to button number, a value of 5 is no button pressed
  analog_value=5;
  
  if (_value <800) {
    analog_value=4;
  }
  
  if (_value < 600) {
    analog_value=3;
  }
  
  if (_value < 400) {
    analog_value=2;
  }
  
  if (_value < 300) {
    analog_value=1;
  }
  
  if (_value < 150) {
    analog_value=0;
  }
  
  
  if ((_prevValue)!=(analog_value)) { //only change change state once
    
    if((_startDebounce==0)) {
      _startDebounce = mnow;
    }
    else {
      //latch the button if it is still pressed when the debounce time expires
      if(mnow - _startDebounce > debounceMS) {
        _PressedValue = analog_value;
        _startDebounce=0;
      }
    }
  }
}

//keep track of the previous state
void AnalogButton::storeValue(int theValue){
  _prevValue=theValue;
}

//reset the button
void AnalogButton::reset(void) {
  _PressedValue = 5; 
  _prevValue=5; //start with no Button Pressed
  _startDebounce = 0;
}

//thermistor class, read thermistor in usable units
class Thermistor {
    protected:
        uint8_t _pin;              //hardware pin number
        int _Value;                //current Value
        int _SampleNum;            //number of samples to average
        int _CurrSample;           //Current sample being measured
        int _ValueTotal;           //Sum of integer analog reads
        float _AveValue;            //average value of analog read
        float _Temperature;         //temperature read
        float _sensorVolt;          //analog pin voltage
        float _sensorResistance;    //resistance at analog pin
        uint16_t _startPolling;    //start of delay time
        
    public:
        uint16_t WaitMS;           //period before button latches
        float Vcc;                 //voltage on thermistor
        float A;                   //A coefficient for Steinhart-Hart Equation    
        float B;                   //B coefficient for Steinhart-Hart Equation
        float C;                   //C coefficient for Steinhart-Hart Equation
        float R1;                 //current sense resistor on RAMPS board
        float Temp(void) { return _Temperature; }  //return the average temperature
        Thermistor(uint8_t pin);           //construct the thermistor
        void poll(uint16_t mnow);          //call periodically to refresh the thermistor counter and value
        void reset(uint16_t mnow);              //reset the wait
};

//constructor
Thermistor::Thermistor(uint8_t pin) {
  _pin = pin;
  _Value=0;
  _SampleNum=10;
  _CurrSample=0;
  _AveValue = 0;
  _ValueTotal = 0;
  _startPolling=0;
  WaitMS = 100; //read temperature every 100 ms
  Vcc=4.743; //ramps Vcc
  A=0.0023018; //A coefficient specific to current thermistor
  B=0.00021143; //B coefficient specific to current thermistor
  C=8.024300E-07; //C coefficient specific to current thermistor
  R1=4700;  //current sense resistor on RAMPS board
}

//refresh the button state
void Thermistor::poll(uint16_t mnow){

  
       _Value = analogRead(_pin);

      //only read value if fixed time between readings has passed
      if(mnow - _startPolling > WaitMS) {
        _CurrSample+=1; //taking a new sample
        _startPolling=mnow;
        if (_CurrSample<_SampleNum) {
          _ValueTotal+=_Value;
        }
        else {
          _ValueTotal+=_Value;
          _AveValue=float(_ValueTotal)/_SampleNum;
          _CurrSample=0;
          _sensorVolt=Vcc*(_AveValue/1023.0);
          _sensorResistance=((R1*_sensorVolt)/(Vcc-_sensorVolt))/1000.0; //resistance in kOhm
          _Temperature=(1/(A+(B*log(_sensorResistance))+(C*pow(log(_sensorResistance),3))))-273.15; //use Steinhart-Hart Equation for Anet Thermistor
          _Temperature=_Temperature*10.0;
          _Temperature=round(_Temperature);
          _Temperature=_Temperature/10.0;
          //limit slow log function by only implementing on average value
          //fit value
          _ValueTotal=0;
          
      }
    }
  }

//reset the wait
void Thermistor::reset(uint16_t mnow) {
  _startPolling = mnow;
}



// Motor steps per revolution. Most steppers are 200 steps or 1.8 degrees/step
#define X_MOTOR_STEPS 200
#define X_RPM 120
#define Y_MOTOR_STEPS 200
#define Y_RPM 120
#define Z_MOTOR_STEPS 200
#define Z_RPM 120
#define MS1 10
#define MS2 11
#define MS3 12

// Since microstepping is set externally, make sure this matches the selected mode
// If it doesn't, the motor will move at a different RPM than chosen
// 1=full step, 2=half step etc.
#define X_MICROSTEPS 8 //1/8 step jummer set on RAMPS board
#define Y_MICROSTEPS 8 //1/8 step jummer set on RAMPS board
#define Z_MICROSTEPS 8 //1/8 step jummer set on RAMPS board

// All the wires needed for full functionality
//test y stepper on RAMPS 1.4 board
#define Y_DIR 61
#define Y_STEP 60
#define Y_ENABLE 56

//test x stepper on RAMPS 1.4 board
#define X_DIR 55
#define X_STEP 54
#define X_ENABLE 38

//test x stepper on RAMPS 1.4 board
#define Z_DIR 48
#define Z_STEP 46
#define Z_ENABLE 62

//stops
#define X_Min 3
#define Y_Min 14
#define Z_Min 18

//plate specifics


// initialize the library with the numbers of the interface pins
//lcd(RS,EN,D4,D5,D6,D7)
int ButtonPin = A12;    // select the input pin for the Buttons on the LCD board
float ButtonValue;
LiquidCrystal lcd(64, 44, 63, 40, 42, 65); //USE 5x5 connector supplied with LCD board and AUX2 connection on RAMPS 1.4 board
//NOTE GND and Vcc must be swapped on lcd board or on RAMPS board.  Damage will result if they are not.
//LCD Board Pinout is 
//1:GND 3:NC  5:RS 7:E   9:ButtonVoltage
//2:Vcc 4:D4  6:D5 8:D6 10:D7
//LCD buttons are attached to A12
AnalogButton LCDButtons(A12);

// 2-wire basic config, microstepping is hardwired on the driver
A4988 Ystepper(Y_MOTOR_STEPS, Y_DIR, Y_STEP, Y_ENABLE, MS1, MS2, MS3);
A4988 Xstepper(X_MOTOR_STEPS, X_DIR, X_STEP, X_ENABLE, MS1, MS2, MS3);
A4988 Zstepper(Z_MOTOR_STEPS, Z_DIR, Z_STEP, Z_ENABLE, MS1, MS2, MS3);
Button StopArray[3] = {
  Button(X_Min),
  Button(Y_Min),
  Button(Z_Min)
};

//plate height = 14 mm
//well distance = 9 mm for 96 plate Nunc PLates
//Printer specific
//x,y steps/mm 6.25
//z step/mm 25

//set up thermistor
Thermistor Thermistor1(A13);
float PrevTemp1=0;

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
float XStepsmm=6.25; //2 mm belt, 16 tooth cog
float YStepsmm=6.25; //2 mm belt, 16 tooth cog 1.25 mm/revolution
float ZStepsmm=25; //m8 lead screw 
float plateheight=14; //height of Nunc ploypropylene plates
float Xwell=9;//distance in x between wells
float Ywell=9;//distance in y between wells
int XHome=0;
int YHome=0;
int ZDown=630; 
int ZHome=0;
float XAbsHome=625; //distance in steps from X stop and well A1 on plate
float YAbsHome=-903; //distance in steps from Y stop and well A1 on plate
float ZAbsHome=-1000; //distance in steps from Z stop and up position for plate reading
int XLoc,XPos; //XLoc absoute location in stepper units, XPos location in well plate
int YLoc,YPos; //YLoc absoute location in stepper units, YPos location in well plate
int ZLoc;
int PlateCol=12;  //96 well plate
int PlateRow=8;   //96 well plate rows labels A-H
#define NumCommands 6 //the number of elements in the command array
char Commands[NumCommands] = {'G','H','L','W','I','T'}; //(G)oto; (H)ome (X),(Y),(Z),(A)ll; (L)ower;(W)here am I; (I)nitialize; (T)emperature
char HomeValues[4] ={'X','Y','Z','A'};
char theCommand;
String OutputString="";
int XStop,YStop,ZStop;
int Menu=0; //sub menu location in display
void setup() {
  // initialize serial:
  Serial.begin(9600);
  //initialize head location
  //add move commands here when implementing
  XLoc=YHome;
  YLoc=XHome;
  ZLoc=ZHome;  
  XPos=0;
  YPos=0;
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  //set stepper parameters
  Ystepper.begin(Y_RPM, Y_MICROSTEPS);
  Xstepper.begin(X_RPM, X_MICROSTEPS);
  Zstepper.begin(Z_RPM, Z_MICROSTEPS);
  // Print a message to the LCD.
  lcd.print("Welcome");
  delay(500);
  WriteScreen0();
  Xstepper.disable(); //comment out for use keeps steppers cool during debugging
  Ystepper.disable(); //comment out for use keeps steppers cool during debugging
  Zstepper.disable(); //comment out for use keeps steppers cool during debugging
  //initalize stops
  Xstepper.disable();
  LCDButtons.debounceMS = 50;
  StopArray[0].debounceMS = 50;
  StopArray[1].debounceMS = 50;
  StopArray[2].debounceMS = 50;
  //ButtonXstop.debounceMS = 100;
  Serial.println("Ready");  //7 bytes sent after intialization
}

void loop() {
    int i;
    int bval;
    
    uint16_t mnow = millis();
    
    //code to test stops
    //i=0;
    //while (i<3) {  //poll all the stops
      //StopArray[i].poll(mnow);
      //if ((StopArray[i].prevState())!=(StopArray[i].wasPressed())) {
         //StopArray[i].storeState(StopArray[i].wasPressed());
         //if (StopArray[i].wasPressed()) {
           //Serial.print(char(88+i));
           //Serial.println("Stop");
         //}
         //if (!(StopArray[i].wasPressed())) {
           //Serial.print(char(88+i));
           //Serial.println("Start");
         //}
      //}
     // i+=1;
   // }
  
  Thermistor1.poll(mnow);
  if (Thermistor1.Temp()!=PrevTemp1) {
      if (Menu==0) { //only update lcd on menu 0
        lcd.setCursor(12, 3); //set position to insert temperature
        lcd.print(Pad(String(Thermistor1.Temp(),1),5));//write new temperature to display
      }
    PrevTemp1=Thermistor1.Temp();
  }
  
    
    LCDButtons.poll(mnow);
   //Serial.print(LCDButtons.prevValue());
    //Serial.print(",");
    //Serial.println(LCDButtons.PressedValue());
    //0=left button, 1=bottom button , 2= middle button, 3=right button,4=top button
    if ((LCDButtons.prevValue())!=(LCDButtons.PressedValue())) { 
      LCDButtons.storeValue(LCDButtons.PressedValue());
      if (LCDButtons.PressedValue()<5) { //only do something if a button is pressed, 5 is no button pressed or button released
        //Serial.print("Button ");
        bval=LCDButtons.PressedValue();
        //Serial.println(bval); 
        switch (bval) {
          case 0:
            if (Menu==0) {
              PositionProbe();
            }
            break;
          case 1:
            if (ZLoc==ZHome) {
              lcd.setCursor(0, 3);
              lcd.print("                    ");
              lcd.setCursor(0, 3);
              lcd.print("Lowering...");
              Lower("L");
              WriteScreen0();
            }
            else {
              Home("HZ");
            }
            break;
          case 2:
            Home("HA");
            break;
          case 3:
            Abort();
            break;
          case 4:
            Home("HZ");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Initializing...");
            Initialize("XYZ");  //return location
            WriteScreen0();
            break;
          default:
            break;
        }
      }
    }
          
  // print the string when a newline arrives:
  if (stringComplete) {
    //Serial.println(inputString); //Print the Command comment out for labview use
    theCommand=inputString.charAt(0);
    if (CommandOK(theCommand)) {
      switch (CommandLoc(theCommand)) {
        case 0:
           GotoXY(inputString); //move head to well position
           break;
        case 1:
           Home(inputString);  //send axis to home
           break;
        case 2:
           Lower(inputString);  //lower read head
           break;
        case 3:
           Where(inputString);  //return location
           break;
        case 4:
           Home("HZ");
           lcd.clear();
           lcd.setCursor(0, 0);
           lcd.print("Initializing...");
           Initialize("XYZ");  //return location
           WriteScreen0();
           break;
        case 5:
           Temperature(); //return the temperature read by the thermistor in C
           break;
        break;
        default:
           break;
      }
    }
    // clear the string:
    inputString = "";
    stringComplete = false;
  }
}

void WriteScreen0() {  //set up each screen as a procedure to allow multiple menus - perhaps write as object or use arrays if it gets more involved

  int mNow;
  Menu=0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Plate Pos ");
  lcd.print((char)(YPos+65));
  lcd.print(String(XPos+1)); //Display position using labels on plate store numerically A1=0,0
  lcd.setCursor(0, 1);
  lcd.print("Abs Pos "+String(XLoc)+","+String(YLoc)+","+String(ZLoc));//display stepper units for location - useful for debugging
  lcd.setCursor(0, 2);
  lcd.print("Moved 0,0,UP");//display the last move
  lcd.setCursor(0, 3);
  
  //mNow = millis();
  //Thermistor1.poll(mNow);
  //PrevTemp1=Thermistor1.Temp();
  
  //get averaged value from thermistor and insitialize variables
  //while (Thermistor1.Temp()==PrevTemp1) {
    //mNow = millis();
    //Thermistor1.poll(mNow);
  //}
  //PrevTemp1=Thermistor1.Temp();
  //lcd.print("Temperature:"+Pad(String(Thermistor1.Temp(),1),5)+"  C");//display the thermistor temperature, maximum of a xxx.x digit temperature value
  lcd.print("Temperature:"+Pad(" ",5)+"  C");//display the thermistor temperature, maximum of a xxx.x digit temperature value
  lcd.setCursor(18, 3); //set position to insert degree symbol
  lcd.print((char)223); //write degree symbol
}

void WriteScreen1() {  //set up each screen as a procedure to allow multiple menus - perhaps write as object or use arrays if it gets more involved
  Menu=1;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press left button");
  lcd.setCursor(0,1);
  lcd.print("when");
  lcd.setCursor(0,2);
  lcd.print("probe positioned...");
}

void Initialize(String Axes) {
 
 bool stopmotor=false;
 int StrIndex;
 char AxesArray[3]={'A','A','A'}; //x,y,and z the characrer case designates whether to move back to zero position (upper) or stop for setting up probe (lower) 
 bool AxesInit[3]={0,0,0};
 bool HoldPos[3]={0,0,0};  //toggle if position is to be held
 //Axes.toCharArray(AxesArray, 3); this should work but its not reading the string properly implement another way...
 
 StrIndex=0;
 while (StrIndex<Axes.length()) {
   AxesArray[StrIndex]=Axes.charAt(StrIndex);
   StrIndex+=1;
 }
 
 StrIndex=0;
 while (StrIndex<3) {
   
   if (AxesArray[StrIndex]=='X') {
     HoldPos[0]=0;
     AxesInit[0]=1;
   }
   if (AxesArray[StrIndex]=='Y') {
     HoldPos[1]=0;
     AxesInit[1]=1;
   }
   if (AxesArray[StrIndex]=='Z') {
     HoldPos[2]=0;
     AxesInit[2]=1;
   }
   
   if (AxesArray[StrIndex]=='x') {
     HoldPos[0]=1;
     AxesInit[0]=1;
   }
   if (AxesArray[StrIndex]=='y') {
     HoldPos[1]=1;
     AxesInit[1]=1;
   }
   if (AxesArray[StrIndex]=='z') {
     HoldPos[2]=1;
     AxesInit[2]=1;
   }
  
   StrIndex+=1;
 }
  
 
   
 if (AxesInit[0]) {
   delay(500);
   //zero x 
   Xstepper.enable();
   // set the motor to move continuously for a reasonable time to hit the stopper
   // let's say 100 complete revolutions (arbitrary number)
   Xstepper.startMove(100 * X_MOTOR_STEPS * X_MICROSTEPS*-1);     // in microsteps
   while (!stopmotor) {
     if (digitalRead(X_Min) == HIGH){
       Xstepper.stop();
       stopmotor=true;
       Xstepper.disable();
     }
     unsigned wait_time_micros = Xstepper.nextAction();
     // 0 wait time indicates the motor has stopped
     if (wait_time_micros <= 0) {
       Xstepper.disable();       // comment out to keep motor powered
     }
   }
 }
 
 
 if (AxesInit[1]) {
   delay(500);
   //zero y  
   stopmotor=false;
   Ystepper.enable();
   // set the motor to move continuously for a reasonable time to hit the stopper
   // let's say 100 complete revolutions (arbitrary number)
   Ystepper.startMove(100 * Y_MOTOR_STEPS * Y_MICROSTEPS);     // in microsteps
   while (!stopmotor) {
     if (digitalRead(Y_Min) == HIGH){
       Ystepper.stop();
       stopmotor=true;
       Ystepper.disable();
     }
     unsigned wait_time_micros = Ystepper.nextAction();
     // 0 wait time indicates the motor has stopped
     if (wait_time_micros <= 0) {
       Ystepper.disable();       // comment out to keep motor powered
     }
   }
 }
 
 if (AxesInit[2]) {
   delay(500);
   //zero z 
   stopmotor=false;
   Zstepper.enable();
   // set the motor to move continuously for a reasonable time to hit the stopper
   // let's say 100 complete revolutions (arbitrary number)
   Zstepper.startMove(100 * Z_MOTOR_STEPS * Z_MICROSTEPS);     // in microsteps
   while (!stopmotor) {
     if (digitalRead(Z_Min) == HIGH){
       Zstepper.stop();
       stopmotor=true;
       Zstepper.disable();
     }
     unsigned wait_time_micros = Zstepper.nextAction();
     // 0 wait time indicates the motor has stopped
     if (wait_time_micros <= 0) {
       Zstepper.disable();       // comment out to keep motor powered
     }
   }
 }
 
 
 if (AxesInit[2]&(!HoldPos[2])) {
   //position z
   delay(500);
   Zstepper.enable();
   Zstepper.move(ZAbsHome*Z_MICROSTEPS);
   Zstepper.disable();
 }
 
 if (AxesInit[0]&(!HoldPos[0])) {
   //position x
   delay(500);
   Xstepper.enable();
   Xstepper.move(XAbsHome*X_MICROSTEPS);
   Xstepper.disable();
 }
 
 if (AxesInit[1]&(!HoldPos[1])) {
 //position y
   delay(500);
   Ystepper.enable();
   Ystepper.move(YAbsHome*Y_MICROSTEPS);
   Ystepper.disable();
 }
 

 
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

int HomeCommandLoc(char input)
{
  int i,CLoc;
  int HCArraySize=4;
  
  i=0;
  CLoc=-1;
  
  while (i<HCArraySize) {
    if (input==HomeValues[i]){
      CLoc=i;
    }
    i+=1;
  }
  
  return CLoc;
}

void GotoXY(String CString) 
{
  String XString="";
  String YString="";
  String SendText="";
  int XValue=0;
  int YValue=0;
  int XMove=0;
  int YMove=0;
  
  if (ZLoc>0) {
    Zstepper.enable();
    Zstepper.move(-ZLoc*Z_MICROSTEPS);
    Zstepper.disable();
    ZLoc=ZHome; //move read head up for move
  }

  //Get X location
  XString=CString.substring(1,3);
  //Get Y location
  YString=CString.substring(4,6);
  
  XValue=XString.toInt();
  YValue=YString.toInt();
  if (((XValue<PlateCol)&&(XValue>=0))&&((YValue<PlateRow)&&(YValue>=0))) {
    //ignore positions out of range
    XMove=(XValue*XStepsmm*Xwell)-XLoc; //amount and direction to move in X
    YMove=(YValue*YStepsmm*Ywell)-YLoc; //amount and direction to move in X
    XPos=XValue;
    YPos=YValue;
    XLoc=(XValue*XStepsmm*Xwell); //absolute x location of head
    YLoc=(YValue*YStepsmm*Ywell); //absolute y location of head
    Xstepper.enable();
    Xstepper.move(round(XMove*X_MICROSTEPS));
    Xstepper.disable();
    delay(250);
    Ystepper.enable();
    Ystepper.move(round(YMove*Y_MICROSTEPS));
    Ystepper.disable();
    delay(250);
    if (Menu==0) {
      SendText=String(XPos)+","+String(YPos)+","+String(XLoc)+","+String(YLoc)+","+String(ZLoc);
      SendText=PadMessage(SendText,20); //ensure all sent messages are 20 bytes for Labview ease of programming
      Serial.println(SendText); //return plate position, absolute location and head position
      DisplayMove(XMove,YMove,ZLoc);
    }
  }
}
  
void Home(String CString)
{
  int XValue,YValue;
  int XMove,YMove;
  String MoveTo;
  String SendText;
  String AxesText;
  
  if (Menu==0) { //only display text if on top menu
    AxesText=CString.substring(1);  // get command without H
    AxesText.trim(); //remove any junk characters
    if (AxesText=="A") {
      AxesText="All";
    }
    lcd.setCursor(0, 3);
    lcd.print("                    "); //clear bottom line
    lcd.setCursor(0, 3);
    lcd.print("Homing "+AxesText);
  }
  
  XValue=XPos;
  YValue=YPos;
              
  if (ZLoc>ZHome) {
    Zstepper.enable();
    Zstepper.move(-ZLoc*Z_MICROSTEPS);
    Zstepper.disable();
    ZLoc=ZHome; //move read head up for move
  }
  
  switch (HomeCommandLoc(CString.charAt(1))) {
    case 0:
       MoveTo="G00,"+String(YPos);
       if (YPos<=10) {
         MoveTo="G00,0"+String(YPos);
       }
       GotoXY(MoveTo);
       break;
    case 1:
       MoveTo="G"+String(XPos)+",00";
       if (XPos<=10) {
         MoveTo="G0"+String(XPos)+",00";
       }
       GotoXY(MoveTo);
       break;
    case 2:
       ZLoc=ZHome;
       break;
    case 3:
       MoveTo="G00,00";
       GotoXY(MoveTo);
       break;
    default:
       break;
  }
  
  if (Menu==0) { //only change screen if in top menu
    WriteScreen0();
    DisplayMove(XMove,YMove,ZLoc);
    SendText=String(XPos)+","+String(YPos)+","+String(XLoc)+","+String(YLoc)+","+String(ZLoc);
    SendText=PadMessage(SendText,20); //ensure all sent messages are 20 bytes for Labview ease of programming
    Serial.println(SendText); //return plate position, absolute location and head position
  }
  
}

void Lower(String CString)
{
  String SendText;
  
  if (ZLoc==ZHome) {
    ZLoc=ZDown; //only lower the head if its up
    Zstepper.enable();
    Zstepper.move(ZLoc*Z_MICROSTEPS);
    Zstepper.disable();
  }
  
  DisplayMove(0,0,ZLoc);
  SendText=String(XPos)+","+String(YPos)+","+String(XLoc)+","+String(YLoc)+","+String(ZLoc);
  SendText=PadMessage(SendText,20); //ensure all sent messages are 20 bytes for Labview ease of programming
  Serial.println(SendText); //return plate position, absolute location and head position
  
}

void Where(String CString)
{
  String SendText;
  //LabView friendly message - need to pad for contant byte count
  SendText=String(XPos)+","+String(YPos)+","+String(XLoc)+","+String(YLoc)+","+String(ZLoc);
  SendText=PadMessage(SendText,20); //ensure all sent messages are 20 bytes for Labview ease of programming
  Serial.println(SendText); //return plate position, absolute location and head position
  
}
  

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    if (inChar == '\r') {   //carriage return signal end of command
      stringComplete = true;
    }
  }
}

void DisplayMove(int XM, int YM, int ZM) {
  
  if (Menu==0) {// only change display if on top menu
    //could be smarter test if flickering
    lcd.setCursor(10, 0); //Do not overwrite static text - eliminates flickering
    lcd.print("    ");  //Clear old text
    lcd.setCursor(10, 0);
    lcd.print((char)(YPos+65));//display ASCII character
    lcd.print(String(XPos+1)); //Display position using labels on plate store numerically A1=0,0
    lcd.setCursor(8, 1);
    lcd.print("            ");
    lcd.setCursor(8, 1);
    lcd.print(String(XLoc)+","+String(YLoc)+","+String(ZLoc));//display stepper units for location - useful for debugging
    lcd.setCursor(6,2);
    lcd.print("              ");
    lcd.setCursor(6,2);
    lcd.print(String(XM)+","+String(YM)+","+UpDown(ZM));//display the last move
  }
}

void Temperature() {
  String SendText;
  //LabView friendly message - need to pad for contant byte count
  SendText=String(Thermistor1.Temp(),1);
  SendText=PadMessage(SendText,20); //ensure all sent messages are 20 bytes for Labview ease of programming
  Serial.println(SendText); //return plate position, absolute location and head position
}

String UpDown(int ZM) {
  String ZString;
  
  ZString="Down";
  if (ZM<=0) {
    ZString="Up";
  }
  
  return ZString;
}

String Pad(String theText, int thelength) {
  String paddedString;
  int Strlength,i;

  paddedString=theText;
  
  Strlength=theText.length();
  i=0;
  while (i<(thelength-Strlength)) {
    paddedString=" "+paddedString;
    i+=1;
  }
  
  return paddedString;
}

String PadMessage(String theText, int thelength) {
  String paddedString;
  int Strlength,i;

  paddedString=theText;
  
  Strlength=theText.length();
  i=0;
  while (i<(thelength-Strlength)) {
    paddedString=paddedString+",";
    i+=1;
  }
  
  return paddedString;
}

void PositionProbe() {
  
   int LCDBPrVal;  //previous value
   int LCDBPVal;   //pressed value  these limit calls to object - each call resets button states before functions can catch state change
   int mnow;
   
   
   Home("HA");
   lcd.setCursor(0, 3);
   lcd.print("                    ");
   lcd.setCursor(0, 3);
   lcd.print("Zeroing X and Z axes");
   Initialize("xz"); //initialize x,y,z position but hold x and z at their respectiver stops for positioning probe
   WriteScreen1();
   while (Menu==1) { //isolate button polliong to limit behavior when in Menu 1, this also disables USB communication
     mnow=millis();
     LCDButtons.poll(mnow);
     LCDBPrVal=LCDButtons.prevValue();
     LCDBPVal=LCDButtons.PressedValue();
     if ((LCDBPrVal)!=(LCDBPVal)) { 
        if (LCDBPVal==0) { //only do something if return button is pressed
          Menu=0;
        }
        LCDButtons.storeValue(LCDBPVal);
      }
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Moving to");
    lcd.setCursor(0, 1);
    lcd.print("position A1...");
    delay(500);
    //move probe to home position
    //position z
    Zstepper.enable();
    Zstepper.move(ZAbsHome*Z_MICROSTEPS);
    Zstepper.disable();
    delay(500);
    //position x
    Xstepper.enable();
    Xstepper.move(XAbsHome*X_MICROSTEPS);
    Xstepper.disable();
    delay(500);  //allow voltage levels to stabilize
    //display top menu
    WriteScreen0();

}
          
void Abort() {
  
   int LCDBPrVal;  //previous value
   int LCDBPVal;   //pressed value  these limit calls to object - each call resets button states before functions can catch state change
   int mnow;
   String aborttext;
   
   
   Menu=2;
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Aborting...");
   aborttext="abort";
   aborttext=PadMessage(aborttext,20); //ensure all sent messages are 20 bytes for Labview ease of programming
   Serial.println(aborttext); //return plate position, absolute location and head position
   Serial.end();//close serial port to incoming commands
   Home("HA");
   delay(500);  //allow voltage levels to stabilize
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Aborted");
   lcd.setCursor(0, 1);
   lcd.print("Press right button");
   lcd.setCursor(0,2);
   lcd.print("to continue");
   while (Menu==2) { //isolate button polliong to limit behavior when in Menu 1, this also disables USB communication
     mnow=millis();
     LCDButtons.poll(mnow);
     LCDBPrVal=LCDButtons.prevValue();
     LCDBPVal=LCDButtons.PressedValue();
     if ((LCDBPrVal)!=(LCDBPVal)) { 
        if (LCDBPVal==3) { //only do something if return button is pressed
          Menu=0;
        }
        LCDButtons.storeValue(LCDBPVal);
      }
    }
    inputString="";
    
    WriteScreen0();
    Serial.begin(9600); //clear serial buffer to eliminate response to any commands sent while aborting

}
  
