/*
Implementation by Richard Blair, May 31, 2020
UNO implementation at memory limit - must use mega

MLC90614 temperature routines
Written by Limor Fried/Ladyada for Adafruit Industries.  
BSD license, all text above must be included in any redistribution

 */
 
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <RTClib.h>
#include <SdFat.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
#include <NewPing.h> //this library has been modified to allow millimeter measurement.

const char getFmt[] PROGMEM = "%X%X%X%X,%s,%02d/%02d/%04d,%02d:%02d:%02d,%s,%s,%c"; //format string

#define SD_CS_PIN 10 // SD chip select pin.
#define RFID_SS_PIN 8 //RFID reader chip select pin.  
#define RST_PIN 9
#define numnames 7
#define FILE_NAME "P_Log.txt" // Log file name.  Must be six characters or less + extension.
// These #defines make it easy to set the backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7
//tempeature range values
#define highvalue 100.00 //minimum for fever flag
#define lowvalue 82.00 // minimum to indicate improper readings
#define stdvmin 1 //minimum allowable standard deviation for the measurements
#define numsamples 10 //number of temperature samples to take
//
//sonar parameters
#define TRIGGER_PIN  7  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     6  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 200 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.
#define SENSE_DISTANCE 100 //Minimum distance for accurate temperature recording
//interface paramters
#define timeout_time 10000 //time in milliseconds to wait for use input otherwise the system is reset.


//initialize Adafruit RTC
RTC_DS1307 rtc;

// File system object.
SdFat sd;

//initialize temeprature probe
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

//initialize the distance sensor
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); 

// Initialize LCD Display
// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// Log file.
File LogFile;

MFRC522 mfrc522(RFID_SS_PIN, RST_PIN);   // Create MFRC522 instance.
byte IDArray[7][4]={{0x00,0x00,0x00,0x00},
                    {0xCA,0x1B,0xA9,0x13},
                    {0x56,0x68,0xD4,0x25},
                    {0xCA,0x3F,0x60,0x14},
                    {0x7A,0x75,0xA1,0x13},
                    {0xBA,0xCF,0x5F,0x14},
                    {0xBA,0x87,0x70,0x13}};
                     
char NameArray[7][17]={"Unknown User",
                       "Richard Blair",   
                       "Katerina Chagoya",
                       "Alan Felix",
                       "Luciano Violante",
                       "User 5",
                       "User 6"};  
byte ID[4]={' '};  //ugh globals but its the simplest implementation
char TEMPERATURE[7]={' '};
char TEMPSTDV[5]={' '};
byte LED_Pins[]={22,24,26,28}; // right to left on box -> green _low, yellow, red, green
byte LED_ON;
int time_out=0; //whether a timout has occured
float Offset=6.265; //compensation for wrist temperature versus oral thermometer, empirically derived.
 
void setup() 
{
  byte i=0;
  //Serial.begin(9600);   // Initiate a serial communication
  lcd.begin(16, 2);
  SPI.begin();      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  
  pinMode(10, OUTPUT); //sd card specific
  if (!sd.begin(SD_CS_PIN)) {
    lcd.print("SD Init fail!");
    return;
  }
  while (i<4) {
    pinMode(LED_Pins[i], OUTPUT); //set LED inficator pins to output
    digitalWrite(LED_Pins[i], LOW); //and off
    i+=1;
  }
  digitalWrite(LED_Pins[3], HIGH); //status led far left on is OK
  LED_ON=3; //keep track of which one is one for smart switching
  
  mlx.begin();  //start temperature monitoring on I2C device
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  i=0;
  while(i<10) { //flash green LED to indicate that the system initiated properly.
    digitalWrite(LED_Pins[3], LOW);
    delay(100);
    digitalWrite(LED_Pins[3], HIGH);
    delay(100);
    i+=1;
  }
  lcd.setCursor(0, 0);
  lcd.print("Init Done!     ");
  delay(2000);

  

  //RFID prompt
  lcd.setBacklight(WHITE);
  lcd.setCursor(0, 0);
  lcd.print("Place ID near");
  lcd.setCursor(0, 1);
  lcd.print("the reader...");

}
void loop() {

  byte i=0;
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) 
  {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) 
  {
    return;
  }

  while(i< mfrc522.uid.size) {
    ID[i]=mfrc522.uid.uidByte[i];
    i+=1;
  }

  i=0;
  while(i< 4) {
    i+=1;
  }
  
  FormatStampData(ID);
  delay(1000);
} 

void FormatStampData(byte IDread[4]) {
  byte i;
  char theMessage[64] ={' '};
  byte UIDindex=0; //first element is unknown user default value
  byte MONTH=1,DAY=1,HOUR=12,MINUTE=0,SECOND=0;
  int YEAR=1972;
  char HSTAT='z';

  //const char getFmt[] PROGMEM = "%X%X%X%X,%s,%02d/%02d/%04d,%02d:%02d:%02d,%s,%s,%c"; //format string
  //Data fields to log ID,USER,MONTH/DAY/YEAR,HOUR:MINUTE:SECOND,TEMPERATURE,TEMPSTDV,HSTAT
  
  i=0;
  //Serial.println();
  //Serial.println("Line to Write : ");
  while (i<numnames){
    if(CompareID(IDread,IDArray[i])){
      UIDindex=i;
      //break;
    }
    i+=1;
  }
  
  DateTime now = rtc.now();
  
  if (rtc.begin()) {
    MONTH=now.month();
    DAY=now.day();
    YEAR=now.year();
    HOUR=now.hour();
    MINUTE=now.minute();
    SECOND=now.second();
  }

  //RFID Welcome
  lcd.setBacklight(TEAL);
  lcd.setCursor(0, 0);
  lcd.print("Welcome         ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(NameArray[UIDindex]);
  delay(2000);
  
  
  
  FindFormatTemperature();
  if (!time_out) {
    sprintf_P(theMessage,getFmt,ID[0],ID[1],ID[2],ID[3],NameArray[UIDindex],MONTH,DAY,YEAR,HOUR,MINUTE,SECOND,TEMPERATURE,TEMPSTDV,HSTAT);  
    //Serial.println(theMessage);
  
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    LogFile = sd.open(FILE_NAME, FILE_WRITE);
  
    if (LogFile) {
      LogFile.println(theMessage);
    } else {
      digitalWrite(LED_Pins[LED_ON],LOW);
      i=0;
      while (i<10) { //flash red led to indicate file write error
        digitalWrite(LED_Pins[2],HIGH);
        delay(50);
        digitalWrite(LED_Pins[2],LOW);
        delay(50);
        i+=1;
      }
      digitalWrite(LED_Pins[LED_ON],HIGH);
     //Serial.print("error opening ");
      //Serial.println(FILE_NAME);
    }
    LogFile.close();
  }
  else {
      lcd.setBacklight(WHITE);
      lcd.setCursor(0, 0);
      lcd.print("Place ID near   ");
      lcd.setCursor(0, 1);
      lcd.print("the reader...   ");
      time_out=0;
    }
}

bool CompareID(byte Array1[4], byte Array2[4]) { //for 4 byte ID
  bool compared=1;
  byte i;
  
  
  i=0;

  while (i<4) {
    if (Array1[i]==Array2[i]) {
      compared=compared&&1;
    }
    else {
      compared=compared&&0;
    }
    i+=1;
  }
  return compared;
}

void FindFormatTemperature() {
  float tempvalue=0;
  float tempstd=2;
  String Val1,Val2;
  float Readvals[5]={0.0};
  float ReadTotal;
  float TotalStDev;
  byte i;
  long theTime; //keep track of times to allow timeout if id scanned and no temperature taken
  

  theTime=millis();
  while(((tempvalue<lowvalue)||(tempstd>stdvmin))&&(!time_out)) {
    
    ReadTotal=0;
    TotalStDev=0;
    TestInRange();
    
    //Serial.print("FindFormatTemperature Timeout ");
    //Serial.println(time_out);
    
    //repeat measurements until StDev and temperature in range may need to add escape sequence to prevent infinite loop
    if (time_out) {
      break;
    }
    //Serial.print(tempvalue);
    //Serial.print(" ");
    //Serial.println(tempstd);
    delay(100);
    
    lcd.setBacklight(TEAL);
    lcd.setCursor(0, 0);
    digitalWrite(LED_Pins[LED_ON], LOW);
    digitalWrite(LED_Pins[1], HIGH); //turn yellow LED ON
    lcd.print("Hold hand       ");
    lcd.setCursor(0, 1);
    lcd.print("in position     ");
    delay(1000);
    digitalWrite(LED_Pins[LED_ON], HIGH);
    digitalWrite(LED_Pins[1], LOW); //turn yellow LED OFF and Normal State LED on

    i=0;
    while (i<numsamples) { //read the values
      Readvals[i]=mlx.readObjectTempF()+Offset; //offset for non-oral measurement.  
      ReadTotal+=Readvals[i];
      delay(200); //delay between samples for stastics
      i+=1;
    }
    tempvalue=ReadTotal/numsamples; //compute the average
    i=0;

  
    while (i<numsamples) {
      TotalStDev+=pow((Readvals[i]-tempvalue),2);
      i+=1;
    }
    tempstd=sqrt(TotalStDev/numsamples); //compute the standard deviation

    //Serial.print(tempvalue);
    //Serial.print(" ");
    //Serial.println(tempstd);
    delay(100);

    if (tempvalue<lowvalue) { //if below 
      lcd.setCursor(0, 0);
      lcd.print("Temperature too ");
      lcd.setCursor(0, 1);
      lcd.print("low try again   ");
      delay(500);
    }

    if (tempstd>stdvmin) {
      lcd.setCursor(0, 0);
      lcd.print("STDev too ");
      lcd.setCursor(0, 1);
      lcd.print("high try again   ");
      delay(500);
    }
    if ((millis()-theTime)>timeout_time) { //timeout is sample not taken
      //Serial.println("Timeout1");
      time_out=1;
    }
  }

  if (!time_out) {
    Val1=String(tempvalue,2); //the maligned String
    Val2=String(tempstd,2);
    Val1.toCharArray(TEMPERATURE, 7);
    Val2.toCharArray(TEMPSTDV, 5);

    if (tempvalue>highvalue) {
      digitalWrite(LED_Pins[LED_ON], LOW);
      digitalWrite(LED_Pins[2], HIGH); //turn red LED ON
      lcd.setBacklight(RED);
      lcd.setCursor(0, 0);
      lcd.print("Temp:           ");
      lcd.setCursor(6, 0);
      lcd.print(TEMPERATURE);
      lcd.print(" F");
      lcd.setCursor(0, 1);
      lcd.print("HIGH! GO HOME   ");
      delay(10000);
      digitalWrite(LED_Pins[LED_ON], HIGH);
      digitalWrite(LED_Pins[2], LOW); //turn red LED OFF and Normal status LED ON
      lcd.setBacklight(WHITE);
      lcd.setCursor(0, 0);
      lcd.print("Place ID near   ");
      lcd.setCursor(0, 1);
      lcd.print("the reader...   ");
      time_out=0;
    } 
    else { 
      lcd.setBacklight(GREEN);
      lcd.setCursor(0, 0);
      lcd.print("Temp:           ");
      lcd.setCursor(6, 0);
      lcd.print(TEMPERATURE);
      lcd.print(" F");
      lcd.setCursor(0, 1);
      lcd.print("in nominal range");
      delay(10000);
      lcd.setBacklight(WHITE);
      lcd.setCursor(0, 0);
      lcd.print("Place ID near   ");
      lcd.setCursor(0, 1);
      lcd.print("the reader...   ");
      time_out=0;
    }
  }
  else {
    lcd.setBacklight(WHITE);
    lcd.setCursor(0, 0);
    lcd.print("Place ID near   ");
    lcd.setCursor(0, 1);
    lcd.print("the reader...   ");
    time_out=0;
  }
}

void TestInRange() {
   bool outofrange=1;
   bool displayed=0;
   long timetest;
   
   //will need to add timout to stop ininite loop
   timetest=millis();
   //Serial.print("Time_test ");
   //Serial.print(millis());
   //Serial.print(":");
   //Serial.println(timetest);
   while ((outofrange)&&(!time_out)) { //timeout if nothing for more than timetest seconds
     delay(50);
     if (sonar.ping_mm()<SENSE_DISTANCE) { //ping sonar sensor to get distance
      outofrange=0;
     }

     if (!displayed) {
       lcd.setBacklight(YELLOW);
       lcd.setCursor(0, 0);
       lcd.print("Move Closer      ");
       lcd.setCursor(0, 1);
       lcd.print("to sensor        ");
       displayed=1;
     }

     if ((millis()-timetest)>timeout_time) {
        //Serial.println("Timeout2");
        time_out=1;
     }
     }
   }
