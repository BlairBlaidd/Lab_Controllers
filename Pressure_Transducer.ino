//Read Pressure transducer and update display to minimize flickering.
//Richard G. Blair 1/20/2017

// include the library code:
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>

// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// These #defines make it easy to set the backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

float TransducerValue=0;
float Pressure=0;
float Prevpressure=0;
byte TransducerPin=0;
String PrintValue;
long starttime;
int timedelay=250;

void setup() {
  // Debugging output
  Serial.begin(9600);
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);

  // Print a message to the LCD. We track how long it takes since
  // this library has been optimized a bit and we're proud of it :)
  int time = millis();
  lcd.setCursor(0, 0);//set the cursor to column 0, line 0
  lcd.print("Pressure:");
  lcd.setCursor(0, 1);//set the cursor to column 0, line 1
  lcd.print("  0.00 psig");//Initialize display only numbers will be changed.
  lcd.setBacklight(WHITE);
  starttime=millis();
}

void loop() {
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  
  if ((millis()-starttime)>timedelay){ //use instead of delay allow Arduino to perform other tasks while waiting to read a value
    starttime=millis();
    lcd.setCursor(0, 1);
    TransducerValue=float(analogRead(TransducerPin));
    //TransducerValue=TransducerValue-102  for real use transducer signal range is from 0.5 to 4.5 now 0 to 4
    Pressure=(TransducerValue*(250.0/1023.0))+14.696;
    //Pressure=(map(TransducerValue,0.0,1023.0,0.0,250.0))+14.696; 
    //convert integer read to pressure from a 
    //0-5v 200psi max psia transducer and add 14.696 for psig measurement
    //set range to 250 because TransducerValue will be between 0 and 4 volts with a max of 5v or 250 psi (50 psi per volt)
    if (Pressure!=Prevpressure) { //only update LCD when a value changes
      PrintValue=PadString(Pressure,6); //convert to 6 character string
      PrintPressure(PrintValue); //print only numbers to prevent flickering of decimal point
        //Serial.print("[");
        //Serial.print(PrintValue);
        //Serial.println("]");
    }
  }
}

String PadString (float Value, int strlen) {
  String theString;
  
  theString=String(Value,2);
  while (theString.length()<strlen) {
    theString=" "+theString; //pad string with spaces so it is strlen characters long
  }
  return theString;
}

void PrintPressure (String Value) {
  String number;
  String decimal;
  
  number=Value.substring(0,3); //get digits before decimal place
  decimal=Value.substring(4); //get digits after decimal place
  lcd.setCursor(0, 1);//set the cursor to column 0, line 1
  lcd.print(number); //print digits before decimal place
  lcd.setCursor(4, 1);//set the cursor to space after decimal place
  lcd.print(decimal); //print digits after decimal place
  
}
