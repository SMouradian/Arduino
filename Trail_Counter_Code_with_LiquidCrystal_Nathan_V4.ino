//Arduino Program - Created 04.08.2023
//Authors - Nathan Gomez, Mary Cottier, Nick Grainger, Samuel Mouradian
//Version - 4.0
//Last Revision - 04.18.2023 by Samuel Mouradian: "Cleaned up code; removed all unnecessary LCD implementation. SD card is found, logfile is initialized, and user count is working and printing to Serial Monitor."

//Still To-Do:
  //1. Continue cleaning up code

//NOTES:
  //NONE



#include <LiquidCrystal.h>
#include <RTClib.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include <SPI.h>
#include <SD.h>
#include <avr/sleep.h>
#include <Wire.h>



// Backlighting the LCD takes power and bettery power is a premium out on
// the trail, so we'll only light it for a few seconds after a button press
#define BACKLIGHT_OFF 0x0
#define BACKLIGHT_ON 0x7
#define BACKLIGHT_PIN 6
#define BUTTONS_PIN 0



//Adafruit Data Logger (clock) Implementation
//A simple dat alogger for the Arduino analog pins
#define LOG_INTERVAL 1000 //milliseconds between entries
#define ECHO_TO_SERIAL 1 //echo dat ato serial port
#define WAIT_TO_START 0 //wait for serial input in setup()
#define redLEDpin 3 //digital pin connecting to the RED LED
#define greenLEDpin 4 //digital pin connecting to the GREEN LED
#define photocellPin 0 //analog pin connecting to the analog 0 sensor
#define tempPin 1 //analog pin connecting to the analog 1 sensor
#define BANDGAPREF 14 //special indicator that we want to measure the bandgap
#define aref_voltage 3.3 //we tie 3.3V to ARef and measure it with a multimeter
#define bandgap_voltage 1.1 //this is not super guaranteed, but it's not too off
#define LOG_INTERVAL 1000 //milliseconds between entries
#define SYNC_INTERVAL 1000 //milliseconds between calls to flush() - to write data to the card

uint32_t syncTime = 0; //time of last sync



//define the Real Time Clock variables
RTC_PCF8523 rtc;



//We use this digital in for the microwave sensor active detection. This can
//change, but we'll sleep the processor to save power between count events so whatever 
//you choose needs to be able to wake the processor up
const int sensorIn = 3;

int totalSinceStart = 0;
int totalThisDay = 0;
int totalThisHour = 0;
int hourTrip = -1;
int dayTrip = -1;
int displayPane = 0;

volatile int lastTimeDisplayed = -1;
volatile int lastCountDisplayed = -1;
volatile unsigned long eventStartTime = 0;
volatile unsigned long lastEventDuration = 0;
volatile int idleCounter = 1000;

DateTime lastEventTime;
File logfile;

const int chipSelect = 10;



//Define the LCD pins
const int rs = 8, en = 9, d4 = 4, d5 = 5, d6 = 6, d7 = 7; 
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
volatile int backlightCounter = 300;



// Interupt routine called when a proximity sensor event occurs. At the
// start of the event we'll remember the start time and at the end we'll
// compute the length of the duration, which will be a trigger to record
// the event to the SD card in the main loop. 
void onSensorChanged() {
  if (digitalRead(sensorIn) == HIGH) {
    eventStartTime = millis();
    lastEventDuration = 0;
  } else {
    lastEventDuration = millis() - eventStartTime;
  }
  sleep_disable();
  idleCounter = 1000;
}



// One time initialization
void setup() {
  // Serial monitor for debugging
  Serial.begin(9600);

  // Real time clock
  if(!rtc.begin()){
    Serial.println("No rtc");
    Serial.flush();
  }
  if(!rtc.initialized() || rtc.lostPower()){
    Serial.println("Updating rtc clock");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  rtc.start();

  // Initialize the SD card
  pinMode(chipSelect, OUTPUT);
  if (!SD.begin(chipSelect)) {
    Serial.println("No SD card");
  }else{
    Serial.println("SD card found");
  }

  //Create log file
  Serial.println("Creating log file ");
  boolean newfile = false;
  if(SD.exists("data.csv")){
    newfile = true;
  }
  logfile = SD.open("data.csv", FILE_WRITE); 
  if(!logfile){
    Serial.println("couldnt create event log file");
  }
  if(newfile = true){
    logfile.println("Event Time, Duration, Event Count, Hour Count, Day Count, Cummulative Count");
  }

  // Proximity detector digital in
  pinMode(sensorIn, INPUT);
  attachInterrupt(digitalPinToInterrupt(sensorIn), onSensorChanged, CHANGE);
  Serial.println("Ready");
  delay(1000);

  //Data Logger setup implementation
  Serial.println("Initializing SD card..."); //initialize the sd card
  pinMode(10, OUTPUT); //make sure that the default chip select pin is set to output, even if you don't use it
  Serial.println("SD card initialized");
}

void loop(){
  // Manage backlighting. Only on for 30ish seconds after a button pressed
  // to save a bit of battery
  digitalWrite(BACKLIGHT_PIN, LOW);

  uint8_t buttons = analogRead(BUTTONS_PIN);
  if(buttons == 255){
    idleCounter = 1000;
    //lcd.setBacklight(BACKLIGHT_ON);
    digitalWrite(BACKLIGHT_PIN, HIGH);
    backlightCounter = 300;
  }
  if(buttons == 143){
    displayPane--;
    if(displayPane < 0){
      displayPane = 3;
    }
    lastCountDisplayed = -1;
  }
  if(buttons == 88){
    displayPane++;
    if (displayPane > 3){
      displayPane = 0;
    }
    lastCountDisplayed = -1;
  }
  if(backlightCounter > 0){
    backlightCounter--;
    if(backlightCounter == 0){
      digitalWrite(BACKLIGHT_PIN, LOW);
    }

    //Data Logger Implementation
    DateTime now;
    delay((LOG_INTERVAL -1) - (millis() % LOG_INTERVAL)); //delay for the amount of time we want between readings
    digitalWrite(greenLEDpin, HIGH);
  
    //log milliseconds since starting
    uint32_t m = millis();
    logfile.print(m); // milliseconds since start
    logfile.print(", ");

    //fetch the time
    now = rtc.now();

    //log time
    logfile.print(now.unixtime()); //seconds since 1/1/1970
    logfile.print(", ");
    logfile.print('"');
    logfile.print(now.year(), DEC);
    logfile.print("/");
    logfile.print(now.month(), DEC);
    logfile.print("/");
    logfile.print(now.day(), DEC);
    logfile.print(" ");
    logfile.print(now.hour(), DEC);
    logfile.print(":");
    logfile.print(now.minute(), DEC);
    logfile.print(":");
    logfile.print(now.second(), DEC);
    logfile.print('"');

    digitalWrite(greenLEDpin, LOW);

    // Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
    // which uses a bunch of power and takes time
    if ((millis() - syncTime) < SYNC_INTERVAL) return;
    syncTime = millis();
  
    //blink LED to show we are syncing data to the card & updating FAT!
    digitalWrite(redLEDpin, HIGH);
    logfile.flush();
    digitalWrite(redLEDpin, LOW);
}



  //Manage low power mode. If no events have occured in the last minute, then enter a sleep
  //state to save power
  if(idleCounter>0){
    idleCounter--;
    if(idleCounter == 0){
      sleep_enable();
      set_sleep_mode(SLEEP_MODE_PWR_DOWN);
      sleep_cpu();
    }
  }

  //Reset counters if needed
  DateTime now = rtc.now();
  if(now.day() != dayTrip){
    totalThisDay = 0;
    dayTrip = now.day();
  }
  if(now.hour() != hourTrip){
    totalThisHour = 0;
    hourTrip = now.hour();
  }


  //Update the display, based on the pane selected. This only updates once each
  //minute or whenever a count is detected
  int thisMinute = now.minute();
  if(lastTimeDisplayed != thisMinute || lastCountDisplayed != totalSinceStart){
    if(displayPane == 0){
      //Display date and time on first line, overall count on second
      String line0 = getShortTimeString(now);
      line0 += "     ";
      Serial.print(line0);
      lastTimeDisplayed = thisMinute;
      String line1 = String("Users: ");
      line1 += totalSinceStart;
      line1 += "     ";
      Serial.println(line1);
      lastCountDisplayed = totalSinceStart;
    }
    if(displayPane == 1){
      //Display last event time
      Serial.print("Last event time");
      String line1 = getTimeOfDayString(lastEventTime);
      Serial.println(line1);
      lastCountDisplayed = totalSinceStart;
    }
    if(displayPane == 2){
      //Display count of passes this hour
      Serial.print("Users this hour ");
      String line1 = String(totalThisHour);
      line1 += "            ";
      Serial.println(line1);
      lastCountDisplayed = totalSinceStart;
    }
    if(displayPane == 3){
      //Display count of passes this day
      Serial.print("Users this day  ");
      String line1 = String(totalThisDay);
      line1 += "            ";
      Serial.println(line1);
      lastCountDisplayed = totalSinceStart;
    }
  }


  //If an event occured, record it. The duration of the last event is a flag to
  //denote one occured.
  if(lastEventDuration != 0){
    totalSinceStart++;
    totalThisDay++;
    totalThisHour++;
    lastEventTime = now;
    String eventLogRecord = getTimeString(lastEventTime);
    eventLogRecord += ",";
    eventLogRecord += lastEventDuration;
    eventLogRecord += ",1,";
    eventLogRecord += totalThisHour;
    eventLogRecord += ",";
    eventLogRecord += totalThisDay;
    eventLogRecord += ",";
    eventLogRecord += totalSinceStart;
    Serial.println(eventLogRecord);
    logfile.println(eventLogRecord);
    logfile.flush();
    lastEventDuration = 0;
  }
    delay(1000);
}


// Get a time string in format mm-dd-yy hh:mm:ss, which seems to make 
// most spreadsheets happy. RTCs vary and may or may not support 4 digit dates,
// so we always just use last 2 for consistency.
String getTimeString(DateTime aTime){
  int thisYear = aTime.year();
  if(thisYear > 2000){
    thisYear = thisYear - 2000;
  }
  char buffer[18];
  sprintf(buffer,"%02d-%02d-%02d %02d:%02d:%02d",aTime.month(),aTime.day(),thisYear,aTime.hour(),aTime.minute(),aTime.second());
  return String(buffer);
}

// Get a time string in format mm-dd-yy hh:mm, which fits on the display
String getShortTimeString(DateTime aTime){
  int thisYear = aTime.year();
  if(thisYear > 2000){
    thisYear = thisYear - 2000;
  }
  char buffer[16];
  sprintf(buffer,"%02d-%02d-%02d %02d:%02d",aTime.month(),aTime.day(),thisYear,aTime.hour(),aTime.minute());
  return String(buffer);
}


// Get a time string in format mm-dd-yy hh:mm, which fits on the display
String getTimeOfDayString(DateTime aTime){
  char buffer[10];
  sprintf(buffer,"%02d:%02d:%02d",aTime.hour(),aTime.minute(),aTime.second());
  return String(buffer);
}