/*********************

Example code for the Adafruit RGB Character LCD Shield and Library

This code displays text on the shield, and also reads the buttons on the keypad.
When a button is pressed, the backlight changes color.

**********************/

// include the library code:
#include <LiquidCrystal.h>
#include <LCDKeypad.h>
const int RemoteTiggerPin = 11;     // the pin assignment for remote trigger 
const int MillRelayPin = 13;  // the pin assignment for controlling the mill relay
const int ProxSense=3;  //pin to sense if vial sensor is active - senses if vial slipped out of clamp
const int RightClamp=A0; //A0 is for the right prox sensor
const int LeftClamp=A1; //A1 is for the left prox sensor
const float triggervalue=1.1;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7); //originally set to lcd(8, 13, 9, 4, 5, 6, 7) in the LiquidCrystal(rs, rw, enable, d4, d5, d6, d7)  syntax 
//pin 13 is the RW pin.  Read or Write from the display
//only reading so not needed using LiquidCrystal(rs, enable, d4, d5, d6, d7)  syntax instead
int adc_key_val[5] ={50, 200, 400, 600, 800 };
int NUM_KEYS = 5;
int adc_key_in;
int key=-1;
int oldkey=-1;

//variables
int keydelay=250;
int textposition=0;
int bpressed=0;
byte l1=0;
byte l2=0;
byte lmr1=0;
byte lmr2=0;
byte lsr1=0;
byte lsr2=0;
byte timing=0;
byte counting=0;

int mill_time_min_set=30;
int mill_time_sec_set=0;
int mill_time_min_read=30;
int mill_time_sec_read=0;
long starttime=millis();
long buttonstart=millis();
long buttontime;
long timeinterval=0;
long startsec=0;
long startmin=0;
String timingtext[]={"O","T"};


void setup() {
  // Debugging output
  Serial.begin(9600);
  pinMode(RemoteTiggerPin, INPUT); //pin 11 is wired to 24vac relay that closes to grough when triggered
  digitalWrite(RemoteTiggerPin, HIGH); //pin will go from high to low
  //this will be used for remote start timing
  pinMode(ProxSense, INPUT);//pin 3 is wired to a SPST switch when low the proximity sensor values will be read. When high they will be ignored
  digitalWrite(ProxSense, HIGH); //pin will go from high to low
  //This will be used to sense vial slipping from clamp
  pinMode(MillRelayPin, OUTPUT); //pin 13 is wired to big relay for controlling power to mill motor
  digitalWrite(MillRelayPin, HIGH); //pin will drive a relay, the relay board is optically isolated with reverse logic LOW is on and HIGH is OFF
  
  
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);

  // Print a message to the LCD. We track how long it takes since
  // this library has been optimized a bit and we're proud of it :)
  int time = millis();
  time = millis() - time;
  Serial.print("Took "); Serial.print(time); Serial.println(" ms");
  lcd.setCursor(0, 0);
  lcd.print("Set      :");
  lcd.setCursor(0, 1);
  lcd.print("Time     :");
  writeData(mill_time_min_set,mill_time_sec_set,mill_time_min_read,mill_time_sec_read);
  lcd.setCursor(15, 1);
  lcd.print(timingtext[timing]);
}

uint8_t i=0;
void loop() {
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  //lcd.setCursor(0, 1);
  // print the number of seconds since reset:
  //lcd.print(millis()/1000);
  
  adc_key_in = analogRead(0); //Analog 0 reads buttons attached to shield
  key = get_key(adc_key_in);  // convert into key press
  delay(50);  // wait for debounce time
  //Serial.println(key);
  
  //monitor pin 11 for remote trigger (wired to a NO relay)  only start timer if not timing
  if ((!digitalRead(RemoteTiggerPin))&(!timing)) {
    
    if (!timing) {
      //this resets display if timer ran down to zero and mill started again
      ResetTimerDisplay();
    } 
    timing=1;
    lcd.setCursor(15, 1);
    lcd.print(timingtext[timing]);
    bpressed=9;
  }
 
    //set minutes up max is 99
      if (key==3) {//left keypad is 3
      
        //reduce delay the longer the button is held
        buttontime=millis()-buttonstart;
        if (buttontime>2000) {
          buttonstart=millis();
          if (keydelay>20) {
            keydelay=keydelay/2;
          }
        }
        
        delay(keydelay);
        if (bpressed == 3) {
          l1=NumLength(mill_time_min_set);
          mill_time_min_set+=1;
          if (mill_time_min_set>99) {
            mill_time_min_set=0;
          }
          mill_time_min_read=mill_time_min_set; //set upper and lower values when changing time
          l2=NumLength(mill_time_min_set);
          if (l1!=l2){
            lcd.setCursor(6, 0);//clear tens place when rollover occurs
            lcd.print("  ");//make this longer is >99 is needed for this position
            lcd.setCursor(6, 1);//same for bottom number
            lcd.print("  ");//make this longer is >99 is needed for this position
          }
        writeData(mill_time_min_set,mill_time_sec_set,mill_time_min_read,mill_time_sec_read); //update upper display
        }
        else {
          
          bpressed=3;
          buttonstart=millis();
        }
      }
 
  
 //increment seconds maximum is 59
      if (key==0) {//0 is right keypad button
        //reduce delay the longer the button is held
        buttontime=millis()-buttonstart;
        if (buttontime>2000) {
          buttonstart=millis();
          if (keydelay>20) {
            keydelay=keydelay/2;
          }
        }
        
        delay(keydelay);
        if (bpressed == 1) {
          l1=NumLength(mill_time_sec_set);
          mill_time_sec_set+=1;
          if (mill_time_sec_set>59) {
            mill_time_sec_set=0;
          }
          mill_time_sec_read=mill_time_sec_set; //set upper and lower values when changing time
          l2=NumLength(mill_time_sec_set);
          if (l1!=l2){
            lcd.setCursor(10, 0);
            lcd.print("  ");
            lcd.setCursor(10, 1);
            lcd.print("  ");
          }
        writeData(mill_time_min_set,mill_time_sec_set,mill_time_min_read,mill_time_sec_read);
        }
        else {
          bpressed=1;
          buttonstart=millis();
        }
      }

    
//Start and pause timing
    if ((key==2) & (bpressed==0)) {//up on keypad is 2
    
      if (!timing) {
        //this resets display if timer ran down to zero and mill started again
        //only if time is 0:00
        if ((mill_time_min_read==0) & (mill_time_sec_read==0)) {
          ResetTimerDisplay();
        }
      }
      
      delay(100);
      timing=!timing;
      lcd.setCursor(15, 1);
      lcd.print(timingtext[timing]);
      if (timing){
        starttime=millis();
        Countdown();
      }
      bpressed=9;
    }
  else {
    if ((key==2) & (bpressed != 0)) {
      delay(100);
      bpressed=0;
    }
    //update countdown timer with set time may need to add conditional if actually timing
      if (!timing) {
        digitalWrite(MillRelayPin, HIGH);
        counting=0;
      }
      else {
        Countdown();
      }
   }

//Reset timer
      if ((key==1) & (!timing))  {//1 is down keypad button
        delay(100);
        if (bpressed == 10) {
          ResetTimerDisplay();
        }
        else {
          bpressed=10;
        }
      }
      
      if (key==-1)  {//reset delay when button pressed
        if (keydelay < 250) {
          keydelay=250;
        }
      }
      
//test is vial is still in clamp
   if ((timing)&&(!digitalRead(ProxSense))) { //only test when running and sense switch is on
     if ((analogRead(LeftClamp)<((1024/5)*triggervalue))||(analogRead(RightClamp)<((1024/5)*triggervalue))) {
       //one of the sensors is reading low indicating that a vial has slipped out of the clamp
       //stop the mill
       timing=!timing;
       digitalWrite(MillRelayPin, HIGH);
       counting=0;
     }
   }
       
}

void ResetTimerDisplay() {
  //reset minutes
  l1=NumLength(mill_time_min_set);
  mill_time_min_read=mill_time_min_set; 
  l2=NumLength(mill_time_min_set);
    if (l1!=l2){
      lcd.setCursor(6, 0);//clear tens place when rollover occurs
      lcd.print("  ");//make this longer is >99 is needed for this position
      lcd.setCursor(6, 1);//same for bottom number
      lcd.print("  ");//make this longer is >99 is needed for this position
    }
          
  //reset seconds
  l1=NumLength(mill_time_sec_set);
  mill_time_sec_read=mill_time_sec_set;
  l2=NumLength(mill_time_sec_set);
  if (l1!=l2){
    lcd.setCursor(10, 0);
    lcd.print("  ");
    lcd.setCursor(10, 1);
    lcd.print("  ");
  }
          
  //write reset numbers
  writeData(mill_time_min_set,mill_time_sec_set,mill_time_min_read,mill_time_sec_read);
}

void Countdown() {
  
  if (!counting) {
    digitalWrite(MillRelayPin, LOW);
    counting=1;
  }
  startsec=mill_time_sec_read;
  startmin=mill_time_min_read;
  timeinterval=millis()-starttime;
  if (timeinterval>1000) {
    lmr1=NumLength(mill_time_min_read);
    lsr1=NumLength(mill_time_sec_read);
    mill_time_sec_read-=1;
    starttime=millis();
    if (mill_time_sec_read<0) {
      mill_time_min_read-=1;
      mill_time_sec_read=59;
      if (mill_time_min_read<0) {
        //the timer is done
        if (counting){
          digitalWrite(MillRelayPin, HIGH);
          counting=0;
        }
        timing=0;
    
        mill_time_min_read=0;
        mill_time_sec_read=0;
        
        lcd.setCursor(15, 1);
        lcd.print(timingtext[0]);
  
      }
    }
    lmr2=NumLength(mill_time_min_read);
    lsr2=NumLength(mill_time_sec_read);
    if (lmr1!=lmr2) {
      lcd.setCursor(6, 1);
      lcd.print("  ");
    }
    if (lsr1!=lsr2) {
      lcd.setCursor(10, 1);
      lcd.print("  ");
    }
  }
  if (startmin!=mill_time_min_read) {
    writeMin(mill_time_min_read);
  }
  if (startsec!=mill_time_sec_read) {
    writeSec(mill_time_sec_read);
  }
}
  
  
  
  
  
int NumLength (long thenum){
  int result=1;
  
  if (thenum==0){
    result=1;
    return result;
  }
  else {
    result=floor(log(thenum)/log(10))+1;
    return result;
  }
  
}



void writeData(long minset, long secset, long minrem, long secrem) {
      textposition=9-NumLength(minset);
      lcd.setCursor(textposition, 0);
      lcd.print(minset);
      textposition=12-NumLength(secset);
      if (textposition == 11){
        lcd.setCursor(10, 0);
        lcd.print("0");
      }
      lcd.setCursor(textposition, 0);
      lcd.print(secset);
      textposition=9-NumLength(minrem);
      lcd.setCursor(textposition, 1);
      lcd.print(minrem);
      textposition=12-NumLength(secrem);
      if (textposition == 11){
        lcd.setCursor(10, 1);
        lcd.print("0");
      }
      lcd.setCursor(textposition, 1);
      lcd.print(secrem);
}

void writeMin(long minrem) {
      textposition=9-NumLength(minrem);
      lcd.setCursor(textposition, 1);
      lcd.print(minrem);
}

void writeSec(long secrem) {
      textposition=12-NumLength(secrem);
      if (textposition == 11){
        lcd.setCursor(10, 1);
        lcd.print("0");
      }
      lcd.setCursor(textposition, 1);
      lcd.print(secrem);
}

int get_key(unsigned int input)
{
    int k;
    for (k = 0; k < NUM_KEYS; k++)
    {
      if (input < adc_key_val[k])
      {
        return k;
      }
    }   
    if (k >= NUM_KEYS)k = -1;  // No valid key pressed
    return k;
}
