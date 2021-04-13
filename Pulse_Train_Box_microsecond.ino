/*********************

Example code for the Adafruit RGB Character LCD Shield and Library

This code displays text on the shield, and also reads the buttons on the keypad.
When a button is pressed, the backlight changes color.

**********************/

// include the library code:
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>



// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

int buttonPin[] = {5,6,7};    // the number of the pushbutton pin
int buttonState[]={0,0,0};    // the current reading from the input pin
int lastButtonState[] ={0,0,0};   // the previous reading from the input pin
int controlPin[]={12,13}; //pin to trigger MOSFET and mirrow with LED

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTime[] = {0,0,0};  // the last time the output pin was toggled
long debounceDelay[] = {20,20,20};    // the debounce time; increase if the output flickers


long PulseOffOn[] = {2000,300}; //time off in ms, time on in us
long MinOffOn[] = {100,20}; //minimum time off in ms, minimum time on in us
long MaxOffOn[] = {5000,10000}; //minimum time off in ms, minimum time on in us
long minPulse=10; //minimumnumber of pulses
long maxPulse=1000; //maximum number of pulses before continuous
long pulseCount = maxPulse+1;
long pulsecounter=0;
long thetimer;  //ms timer for off pulse
long thetimerus; //us timer for on pulse
long TimeTest; //variable for ms or us timing
bool PulseState;
bool pulsing=0;
String PulseString;
int cursorpos=0;
int infopos[] ={10,10,14};



void setup() {
  //Serial.begin(9600);
  pinMode(buttonPin[0], INPUT);
  pinMode(buttonPin[1], INPUT);
  pinMode(buttonPin[2], INPUT);
  pinMode(controlPin[0], OUTPUT);
  pinMode(controlPin[1], OUTPUT);
  
  // set up the LCD's number of columns and rows: 
  lcd.begin(20, 4);
  delay(10);//need a short delay to allow the screen to initialize
  lcd.setCursor(0, 0);
  lcd.print(">Time ON: "+Pad(String(PulseOffOn[1]),5)+" us");
  lcd.setCursor(16, 0); //set position to u
  lcd.print((char)228); //write greek mu for microsecond
  lcd.setCursor(0, 1);
  lcd.print(" Time OFF:"+Pad(String(PulseOffOn[0]),5)+" ms");
  lcd.setCursor(0, 2);
  PulseString=Checkcont(pulseCount);
  lcd.print(" Pulse Count:"+Pad(PulseString,5));
  lcd.setCursor(0, 3);
  lcd.print("Pulses: "+Pad(String(pulsecounter),10));

  
}

void loop() {
  int i;
  
  //Serial.begin(9600);
  i=0;
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  if (!pulsing) {
    while (i<3) {
      debounce(i);
       i+=1;
    }
    
     //Serial.println(lastButtonState[0]);
       if ((buttonState[0])&&(!lastButtonState[0])) {
         //move selection indicator
         lcd.setCursor(0, cursorpos);
         lcd.print(" ");
         cursorpos+=1;
         if (cursorpos>2){
           cursorpos=0;
         }
         lcd.setCursor(0, cursorpos);
         lcd.print(">");
       }
       
       //Serial.print(buttonState[i]);
       //Serial.print(" ");
       //Serial.print(lastButtonState[i]);
       //Serial.print(" ");
       //Serial.println(cursorpos);
       
      if ((buttonState[1])&&(!lastButtonState[1])&&(cursorpos==0)) {
         lcd.setCursor(infopos[0], cursorpos);
         PulseOffOn[1]=PulseOffOn[1]+10;
         if (PulseOffOn[1]>500){
           PulseOffOn[1]=10;
         }
         lcd.print(Pad(String(PulseOffOn[1]),4));
      }
      
      if ((buttonState[1])&&(!lastButtonState[1])&&(cursorpos==1)) {
         lcd.setCursor(infopos[1], cursorpos);
         PulseOffOn[0]=PulseOffOn[0]+10;
         if (PulseOffOn[0]>500){
           PulseOffOn[0]=10;
         }
         lcd.print(Pad(String(PulseOffOn[0]),4));
      }
      
      if ((buttonState[1])&&(!lastButtonState[1])&&(cursorpos==2)) {
         lcd.setCursor(infopos[2], cursorpos);
         pulseCount=pulseCount+50;
         if (pulseCount>1050){
           pulseCount=50;
         }
         PulseString=Checkcont(pulseCount);
         lcd.print(Pad(String(PulseString),5));
      }

      if ((buttonState[2])&&(!lastButtonState[2])) {
        pulsing=!pulsing;
        //start here
        lcd.setCursor(0, cursorpos);
        lcd.print(" ");
        //initialize timer and start pulsing
        thetimer=millis();
        thetimerus=micros();
        PulseState=1;
        digitalWrite(controlPin[0],PulseState);
        digitalWrite(controlPin[1],PulseState);
        pulsecounter=0; //reset pulse counter
        lcd.setCursor(8, 3);
        lcd.print(Pad(String(pulsecounter),10));
     }
  }
  
  if (pulsing) {
    if (PulseState) {
      TimeTest=(micros()-thetimerus);
    }
    else {
      TimeTest=(millis()-thetimer);
    }
    
    if (TimeTest>PulseOffOn[PulseState]) {
      
      //increment pulse counter when pulse completed
      if (PulseState) {
        pulsecounter+=1;
        lcd.setCursor(8, 3);
        lcd.print(Pad(String(pulsecounter),10));
      }
      
      PulseState=!PulseState;
      if (pulsecounter<=maxPulse) {
      //only check if not continuously pulsing
        if ((pulsecounter>pulseCount)&&(!PulseState)) {
          //stop pulsing if number of pulses reached and output is off
          pulsing=0;
          //replace selector to indicate control enabled
          lcd.setCursor(0, cursorpos);
          lcd.print(">");
        }
      }
      digitalWrite(controlPin[0],PulseState);
      digitalWrite(controlPin[1],PulseState);
      thetimer=millis();
      thetimerus=micros();
    }
    
    debounce(2);
    if ((buttonState[2])&&(!lastButtonState[2])) {
       //stop pulsing and replace  selector to indicate control lockout
       pulsing=!pulsing;
       lcd.setCursor(0, cursorpos);
       lcd.print(">");
       PulseState=0;
       digitalWrite(controlPin[0],PulseState);
       digitalWrite(controlPin[1],PulseState);
        //don't reset pulse counter to allow readability
       delay(50);
       //debounce it not catching double press add delay to stop restart - pulsing has been stopped
     }
  }     
}

String Checkcont(long pcount){
  String ptext;
  
  ptext=String(pcount);
  if (pcount>maxPulse) {
    ptext="Cont";
  }
  return ptext;
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

void debounce(int arraypos) {
  // read the state of the switch into a local variable:
    int reading;
    
    reading=digitalRead(buttonPin[arraypos]);
    // check to see if you just pressed the button
    // (i.e. the input went from LOW to HIGH),  and you've waited
    // long enough since the last press to ignore any noise:

    // If the switch changed, due to noise or pressing:
    if (reading != lastButtonState[arraypos]) {
    // reset the debouncing timer
      lastDebounceTime[arraypos] = millis();
      lastButtonState[arraypos]=!lastButtonState[arraypos];
    }
    
    if ((millis() - lastDebounceTime[arraypos]) > debounceDelay[arraypos]) {
      // whatever the reading is at, it's been there for longer
      // than the debounce delay, so take it as the actual current state:

      // if the button state has changed:
      if (reading != buttonState[arraypos]) {
        buttonState[arraypos] = reading;
      }
    }
}
    

  
