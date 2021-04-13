/*********************

March 31,2021
This code combines libraries for
LCD control with MCP23017 port expander - Adafruit

Robust Rotary encoder reading
Copyright John Main - best-microcontroller-projects.com
https://www.best-microcontroller-projects.com/rotary-encoder.html

Objeect Oriented button routine adapted from 
An object oriented approach to a latched button
Jan 2015, for the Arduino forum, by MattS-UK
**********************/

// include the library code:
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
//PWM enabled pins are 3,5,6,9,10,11
//currently (naively) using pins  2 and 3 for CNTRL1 and CNTRL2 - need to disassemble and rewire
//pin connector PWM and button front to back 9, 10, 11, 3, 4
#define CNTRL1 11 // PWM out for controller 1
#define CNTRL2 3 // PWM out for controller 2
#define PANELBUTTON 4 //front push switch
#define CLK 5 //terminal A on rotary encoder
#define DATA 6 //terminal B on rotary encoder
#define GRNLED 7 //green led on rotary encoder
#define REDLED 8 //red led on rotary encoder
#define ESWITCH 12 //push switch on rotary encoder

//Button classs, encapsulate the functionality of a latched button
class Button {
    protected:
        uint8_t _pin;                  //hardware pin number
        boolean _state;                //current pin state
        boolean _wasPressed;           //button latch
        uint16_t _startDebounce;       //start of debounce time
        
    public:
        uint16_t debounceMs;            //period before button latches
        
        Button(uint8_t pin);
        void poll(uint16_t now);                            //call periodically to refresh the button state
        boolean wasPressed(void) { return _wasPressed; }    //return the latch state
        void reset(void);                                   //reset the latch
};

//constructor
Button::Button(uint8_t pin) {
  _pin = pin;
  _state = false;
  _startDebounce = false;
  _wasPressed = false;
  
  debounceMs = 250;
  pinMode(_pin, INPUT_PULLUP);
}

//refresh the button state
void Button::poll(uint16_t now){
  
  //pullup resistors cause the 
  // button to be HIGH when open 
  // so we invert the hardware state
  _state = !digitalRead(_pin);
  
  //when the button state is false, reset the debounce time
  if(!_state){
    _startDebounce = 0;
  }
  else {
    //start debounce time
    if(!_startDebounce) {
      _startDebounce = now;
    }
    else {
      //latch the button if it is still pressed when the debounce time expires
      if(now - _startDebounce > debounceMs) {
        _wasPressed = true;
      }    
    }
  } 
}

//reset the button
void Button::reset(void) {
  _wasPressed = false;
  _startDebounce = 0;
}


// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

Button Pbutton(PANELBUTTON);
Button Ebutton(ESWITCH);

static uint8_t prevNextCode = 0;
static uint16_t store=0;
bool PWRSTATUS=0; //status of the LED output power could make this an array and have the status different for each controller now all on or all off
byte CNTRLSELECT=0; //which controller can be edited for power
byte CNTRLR[] = {CNTRL1,CNTRL2};
int PWR[2] = {100,0}; //outpur power for LED reactors 

void setup() {
  // Debugging output
  Serial.begin(9600);
  // set up the LCD's number of columns and rows: 
  lcd.begin(20, 4);
  
  delay(10);//need a short delay to allow the screen to initialize

  // Print a message to the LCD. We track how long it takes since
  // this library has been optimized a bit and we're proud of it :)
  int time = millis();
  lcd.setCursor(0, 0);
  lcd.print("*Unit 1 UV Pwr: ");
  lcd.print(PWR[0]);
  lcd.print("%");
  lcd.setCursor(0, 1);  
  lcd.print(" Unit 2 UV Pwr:   ");
  lcd.print(PWR[1]);
  lcd.print("%");
  Serial.print("Took "); Serial.print(time); Serial.println(" ms");
  pinMode(GRNLED, OUTPUT);
  pinMode(REDLED, OUTPUT);
  digitalWrite(GRNLED,LOW);
  digitalWrite(REDLED,HIGH);
  Pbutton.debounceMs = 200;  //can't press too quickly - eliminates extra butten presses
  Ebutton.debounceMs = 200;  //can't press too quickly - eliminates extra butten presses
  //setup rotary encoder pins
  pinMode(CLK, INPUT);
  pinMode(CLK, INPUT_PULLUP);
  pinMode(DATA, INPUT);
  pinMode(DATA, INPUT_PULLUP);
  pinMode(CNTRL1, OUTPUT);  // sets the pin as output
  pinMode(CNTRL2, OUTPUT);  // sets the pin as output
  analogWrite(CNTRL1, 0);  //controller is off
  analogWrite(CNTRL2, 0);  //controller is off
}

uint8_t i=0;
void loop() {
  static int8_t val;
  uint16_t mnow = millis();
  Pbutton.poll(mnow);        //refresh panel button status
  Ebutton.poll(mnow);        //refresh rotary encoder button statue

  if(Pbutton.wasPressed()) {
    if (PWRSTATUS) {
      digitalWrite(GRNLED,LOW);
      digitalWrite(REDLED,HIGH);
      PWRSTATUS=0;
      analogWrite(CNTRL1, 0);//turn both controllers off
      analogWrite(CNTRL2, 0);
    }
    else {
      digitalWrite(GRNLED,HIGH);
      digitalWrite(REDLED,LOW);
      PWRSTATUS=1;
      analogWrite(CNTRL1, PWR[0]*255/100);
      analogWrite(CNTRL2, PWR[1]*255/100);
    }
    Pbutton.reset();
  }

  if(Ebutton.wasPressed()) {
    lcd.setCursor(0, CNTRLSELECT); //move to position of unit currently selected
    lcd.print(" "); //delete select character
    CNTRLSELECT+=1; //increment to next controller - change to 2 or more if code modified for more units
    if (CNTRLSELECT>1) {
      CNTRLSELECT=0;
    }
    lcd.setCursor(0, CNTRLSELECT);
    lcd.print("*");
    Ebutton.reset();
  }

  //read the rotary encoder
  if( val=read_rotary() ) {
      PWR[CNTRLSELECT] += val; //increment or decrement according to rotary encoder value
      if (PWR[CNTRLSELECT]>100) { //can't have more than 100% power
        PWR[CNTRLSELECT]=100;
      }
      if (PWR[CNTRLSELECT]<0) { //can't have less than 0% power
        PWR[CNTRLSELECT]=0;
      }
      PWRLCDwrite(CNTRLSELECT, PWR[CNTRLSELECT]); //write new power to LCD screen
      if (PWRSTATUS) {
        analogWrite(CNTRLR[CNTRLSELECT], PWR[CNTRLSELECT]*255/100);
      }
   }
}

// A vald CW returns 1 or  CCW move returns -1, invalid returns 0.
int8_t read_rotary() {
  static int8_t rot_enc_table[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};
  prevNextCode <<= 2;
  if (digitalRead(DATA)) prevNextCode |= 0x02;
  if (digitalRead(CLK)) prevNextCode |= 0x01;
  prevNextCode &= 0x0f;

   // If valid then store as 16 bit data.
   if  (rot_enc_table[prevNextCode] ) {
      store <<= 4;
      store |= prevNextCode;
      //if (store==0xd42b) return 1;
      //if (store==0xe817) return -1;
      if ((store&0xff)==0x2b) return -1;
      if ((store&0xff)==0x17) return 1;
   }
   return 0;
}

void PWRLCDwrite(byte row, int value) {
 
  lcd.setCursor(16, row);
  if (value<10) { 
    lcd.print("  "); //add leading spaces for single character number
  }
  else if (value<100) {
    lcd.print(" "); //add leading spaces for double character number
  }
  lcd.print(value);
}
