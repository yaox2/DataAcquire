/***
 * This code is a data acquisition program for the research project in PHYS398, UIUC FA2018.
 * This code is largely based on multifle test codes written by Professor George Gollion.
 * 
 * Simon Hu, Qier An, Charlie Xiao
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <SPI.h>
#include "SdFat.h"
#include <Keypad.h>
#include <LiquidCrystal.h>

//BME variables
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME680 bme; // I2C

/////////////////////////// LCD parameters ///////////////////////////
// The LCD is a GlobalFontz 16 x 2 device.
// initialize the LCD library by associating LCD interface pins
// with the arduino pins to which they are connected. first define
// the pin numbers: reset (rs), enable (en), etc.
const int rs = 12, en = 11, d4 = 36, d5 = 34, d6 = 32, d7 = 30;

// instantiate an LCD object named "lcd" now. We can use its class
// functions to do good stuff, e.g. lcd.print("the text").
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// total number of characters in one line of the LCD display
#define LCDWIDTH 16

/////////////////////////// SD card parameters ///////////////////////////
// "#define" is an instruction to the compiler. In this case, it will
// replace all occurrences of "SD_CS_PIN" with "SS", whose value is
// already known to the compiler.
#define SD_CS_PIN SS
// create a file object to which I'll write data
File myFile;
// file name
char filename[ ] = "bme_and_time_data.txt";
// instantiate a file system object
SdFat SD;

/////////////////////////// Keypad parameters ///////////////////////////
// our keypad has four rows and three columns. since this will never change
// while the program is runniong, declare these as constants so that they 
// will live in flash (program code) memory instead of the much smaller
// SRAM. 
const byte ROWS = 4; 
const byte COLS = 3;
// keypad layout
char keys[ROWS][COLS] = {
{'1','2','3'},
{'4','5','6'},
{'7','8','9'},
{'*','0','#'}
};
// looking down on the keyboard from above (the side with the keys), the pins are
// numbered 1 - 8, going from left to right, though 8 is not used. 
// Since I am using an Arduino Mega 2560 with a number of breakout boards, 
// I have the following Arduino pin assignments in order to allow the column pins to 
// generate interrupts in some future version of this program.
byte Arduino_colPins[COLS] = {2, 3, 18}; 
byte Arduino_rowPins[ROWS] = {31, 33, 35, 37};

// now instantiate a Keypad object, call it kpd. Also map its pins.
Keypad kpd = Keypad( makeKeymap(keys), Arduino_rowPins, Arduino_colPins, ROWS, COLS );

// character returned when I query the keypad (might be empty)
char query_keypad_char = NO_KEY;

// last non-null character read from keypad
char last_key_char = NO_KEY;

////////////////////////// DS3231 real time clock parameters ////////////////////////
// this is an I2C device on an Adafruit breakout board. It is a cute little thing 
// with a backup battery.

#include "RTClib.h"

// instantiate a real time clock object named "rtc":
RTC_DS3231 rtc;

// names of the days of the week:
char daysOfTheWeek[7][4] = 
  {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// declare type for a few RTC-related global variables
int RTC_hour, RTC_minute, RTC_second;
int RTC_year, RTC_month, RTC_day_of_month;


// have we already set the RTC clock from the GPS?
bool already_set_RTC_from_GPS;
   
/////////////////////////////// GPS parameters /////////////////////////////
// Set GPSECHO1 and GPSECHO2 to 'false' to turn off echoing the GPS data to the Serial console
// that are useful for debugging. Set GPSECHO3 to 'false' to prevent final navigation result 
// from being printed to monitor. Set one or more to 'true' if you want to debug and listen 
//to the raw GPS sentences. GPSECHO1 yields more verbose echoing than GPSECHO2.
#define GPSECHO1 false
#define GPSECHO2 false
#define GPSECHO3 false

// get the header file in which lots of things are defined:
#include <Adafruit_GPS.h>

// also define some more stuff relating to update rates. See 
// https://blogs.fsfe.org/t.kandler/2013/11/17/set-gps-update-
// rate-on-arduino-uno-adafruit-ultimate-gps-logger-shield/
#define PMTK_SET_NMEA_UPDATE_10SEC "$PMTK220,10000*2F"
#define PMTK_SET_NMEA_UPDATE_5SEC "$PMTK220,5000*2F"
#define PMTK_SET_NMEA_UPDATE_1HZ  "$PMTK220,1000*1F"
#define PMTK_SET_NMEA_UPDATE_2HZ  "$PMTK220,500*2B"
#define PMTK_SET_NMEA_UPDATE_5HZ  "$PMTK220,200*2C"

// let's use the Arduino's second serial port to communicate with the GPS device.
#define GPSSerial Serial2

// Connect to the GPS via the Arduino's hardware port
Adafruit_GPS GPS(&GPSSerial);
     
// we don't expect a valid GPS "sentence" to be longer than this...
#define GPSMAXLENGTH 120
// or shorter than this:
#define GPSMINLENGTH 55

// last sentence read from the GPS:
char* GPS_sentence;

// we'll also want to convert the character array to a string for convenience
String GPS_sentence_string;

String GPS_command;

// pointers into parts of a GPRMC GPS data sentence:

const int GPRMC_hour_index1 = 8;
const int GPRMC_hour_index2 = GPRMC_hour_index1 + 2;

const int GPRMC_minutes_index1 = GPRMC_hour_index2;
const int GPRMC_minutes_index2 = GPRMC_minutes_index1 + 2;
      
const int GPRMC_seconds_index1 = GPRMC_minutes_index2;
const int GPRMC_seconds_index2 = GPRMC_seconds_index1 + 2;
      
const int GPRMC_milliseconds_index1 = GPRMC_seconds_index2 + 1;   // skip the decimal point
const int GPRMC_milliseconds_index2 = GPRMC_milliseconds_index1 + 3;
      
const int GPRMC_AV_code_index1 = 19;
const int GPRMC_AV_code_index2 = GPRMC_AV_code_index1 + 1;
      
const int GPRMC_latitude_1_index1 = 21;
const int GPRMC_latitude_1_index2 = GPRMC_latitude_1_index1 + 4;
      
const int GPRMC_latitude_2_index1 = GPRMC_latitude_1_index2 + 1;   // skip the decimal point
const int GPRMC_latitude_2_index2 = GPRMC_latitude_2_index1 + 4;

const int GPRMC_latitude_NS_index1 = 31;
const int GPRMC_latitude_NS_index2 = GPRMC_latitude_NS_index1 + 1;

const int GPRMC_longitude_1_index1 = 33;
const int GPRMC_longitude_1_index2 = GPRMC_longitude_1_index1 + 5;    // 0 - 180 so we need an extra digit
      
const int GPRMC_longitude_2_index1 = GPRMC_longitude_1_index2 + 1;   // skip the decimal point
const int GPRMC_longitude_2_index2 = GPRMC_longitude_2_index1 + 4;
      
const int GPRMC_longitude_EW_index1 = 44;
const int GPRMC_longitude_EW_index2 = GPRMC_longitude_EW_index1 + 1;

// pointers into a GPGGA GPS data sentence:

const int GPGGA_hour_index1 = 8;
const int GPGGA_hour_index2 = GPGGA_hour_index1 + 2;

const int GPGGA_minutes_index1 = GPGGA_hour_index2;
const int GPGGA_minutes_index2 = GPGGA_minutes_index1 + 2;
      
const int GPGGA_seconds_index1 = GPGGA_minutes_index2;
const int GPGGA_seconds_index2 = GPGGA_seconds_index1 + 2;
      
const int GPGGA_milliseconds_index1 = GPGGA_seconds_index2 + 1;   // skip the decimal point
const int GPGGA_milliseconds_index2 = GPGGA_milliseconds_index1 + 3;
      
const int GPGGA_latitude_1_index1 = 19;
const int GPGGA_latitude_1_index2 = GPGGA_latitude_1_index1 + 4;
      
const int GPGGA_latitude_2_index1 = GPGGA_latitude_1_index2 + 1;   // skip the decimal point
const int GPGGA_latitude_2_index2 = GPGGA_latitude_2_index1 + 4;

const int GPGGA_latitude_NS_index1 = 29;
const int GPGGA_latitude_NS_index2 = GPGGA_latitude_NS_index1 + 1;

const int GPGGA_longitude_1_index1 = 31;
const int GPGGA_longitude_1_index2 = GPGGA_longitude_1_index1 + 5;    // 0 - 180 so we need an extra digit
      
const int GPGGA_longitude_2_index1 = GPGGA_longitude_1_index2 + 1;   // skip the decimal point
const int GPGGA_longitude_2_index2 = GPGGA_longitude_2_index1 + 4;
      
const int GPGGA_longitude_EW_index1 = 42;
const int GPGGA_longitude_EW_index2 = GPGGA_longitude_EW_index1 + 1;

const int GPGGA_fix_quality_index1 = 44;
const int GPGGA_fix_quality_index2 = GPGGA_fix_quality_index1 + 1;

const int GPGGA_satellites_index1 = 46;
const int GPGGA_satellites_index2 = GPGGA_satellites_index1 + 2;

// keep track of how many times we've read a character from the GPS device. 
long GPS_char_reads = 0;

// bail out if we exceed the following number of attempts. when set to 1,000,000 this corresponds
// to about 6 seconds. we need to do this to keep an unresponsive GPS device from hanging the program.
const long GPS_char_reads_maximum = 1000000;

// define some of the (self-explanatory) GPS data variables. Times/dates are UTC.
String GPS_hour_string;
String GPS_minutes_string;
String GPS_seconds_string;
String GPS_milliseconds_string;
int GPS_hour;
int GPS_minutes;
int GPS_seconds;
int GPS_milliseconds;

// this one tells us about data validity: A is good, V is invalid.
String GPS_AV_code_string;

// latitude data
String GPS_latitude_1_string;
String GPS_latitude_2_string;
String GPS_latitude_NS_string;
int GPS_latitude_1;
int GPS_latitude_2;

// longitude data
String GPS_longitude_1_string;
String GPS_longitude_2_string;
String GPS_longitude_EW_string;
int GPS_longitude_1;
int GPS_longitude_2;

// velocity information; speed is in knots! 
String GPS_speed_knots_string;
String GPS_direction_string;
float GPS_speed_knots;
float GPS_direction;

String GPS_date_string;

String GPS_fix_quality_string;
String GPS_satellites_string;
int GPS_fix_quality;
int GPS_satellites;

String GPS_altitude_string;
float GPS_altitude;

void setup() {
  Serial.begin(115200);
  while (!Serial);

 /////////////////////////// LCD setup ////////////////////////////
  // set up the LCD's number of columns and rows, then print a message:
  lcd.begin(16, 2);
  LCD_message("P398DLP GPS data", "reader/parser   ");

  // delay a bit so I have time to see the display.
  delay(1000);
  
/////////////////////////// DS3231 real time clock  setup /////////////////////////
  // turn on the RTC and check that it is talking to us.
  /*if (! rtc.begin()) {
    // uh oh, it's not talking to us.
    LCD_message("DS3231 RTC", "unresponsive");
    // delay 5 seconds so that user can read the display
    delay(5000);  

  } else {
    if (rtc.lostPower()) {
      LCD_message("DS3231 RTC lost", "power. Set to...");
      // Set the RTC with an explicit date & time: September 1, 1988, 7:37:00 am
      rtc.adjust(DateTime(2018, 10, 2, 7, 37, 0));
    }
  }  
  // set flag that we haven't already set the RTC clock from the GPS
  already_set_RTC_from_GPS = false;*/
  //end of clock set up
  
////////////////////////////////// GPS setup //////////////////////////////

  // If you want to see a detailed report of the GPS information, open a serial 
  // monitor window by going to the Tools -> Serial Monitor menu, then checking
  // the Autoscroll box at the bottom left of the window, and setting the baud rate
  // to 115200 baud.

  // once we've set up the GPS it will free-run: it has its own built-in microprocessor.
     
  // 9600 NMEA is the default communication and baud rate for Adafruit MTK GPS units. 
  // NMEA is "National Marine Electronics Association." 
  // Note that this serial communication path is different from the one driving the serial 
  // monitor window on your laptop, which should be running at 115,200 baud.
  GPS.begin(9600);
  
  // turn on RMC (recommended minimum) and GGA (fix data, including altitude)
  // GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  
  // uncomment this line to turn on only the "minimum recommended" data:
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  
  // Set the update rate to once per second. Faster than this might make it hard for
  // the serial communication line to keep up. You'll want to check that the 
  // faster read rates work reliably for you before using them. 

  // what the heck... let's try 5 Hz.
  // GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); 
  // GPS.sendCommand(PMTK_SET_NMEA_UPDATE_2HZ); 
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ); 
  // GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ); 
  
  // Request updates on antenna status, comment out to keep quiet.
  // GPS.sendCommand(PGCMD_ANTENNA);

  // Ask for firmware version, write this to the serial monitor. Comment out to keep quiet.
  // GPSSerial.println(PMTK_Q_RELEASE);
  Serial.println(GPS_date_string);
  delay(100);
  // end of GPS setup

/////////////////////////// BME setup /////////////////////////
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
  //end of BME setup

/////////////////////////// SD Card setup /////////////////////////
  // Open serial communications and wait for port to open:
  while (!Serial) { }
  // push a message to the serial monitor window
  // fire up the SD file system object and check that all is fine so far.
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD initialization failed!");
    // delay a bit to give the serial port time, then bail out...
    delay(100);
    exit(0);
  }
  Serial.println("SD initialization done.");

  // if the SD file exists already, delete it.
  if(SD.exists(filename)) { 
    SD.remove(filename); 
        
    // wait a bit just to make sure we're finished with the file remove
    delay(100);   
  }

  // open the new file then check that it opened properly.
  myFile = SD.open(filename, FILE_WRITE);
 
  if (myFile) {
    Serial.print("Writing to "); Serial.print(filename); Serial.println("...");
    myFile.println("Data written below has such format:");
    myFile.println("hour(UTC), minute, second, millisecond");
    myFile.println("Temperature(°C), pressure(hPa), humidity(%), altitude(m)");   
    myFile.println() 

  } else {
       
    // if the file didn't open, print an error:
    Serial.print("error opening "); Serial.println(filename); 

    // delay a bit to give the serial port time to display the message...
    delay(100);
  }
  //end of SD setup

}

void loop() {

  GPS_query();
  char the_key;
  // self-explanatory
  if (!bme.performReading()) {
    myFile.println("Failed to perform reading :(");
    Serial.println("Failed to perform reading :(");
    return;
  }

  // Test GPS
  // Serial.print("\nGPS_hour (UTC) = "); Serial.print(GPS_hour); 
  // Serial.print("   GPS_minutes = "); Serial.print(GPS_minutes); 
  // Serial.print("   GPS_seconds = "); Serial.print(GPS_seconds); 
  // Serial.print("   GPS_milliseconds = "); Serial.println(GPS_milliseconds);
  // delay(500);

  // if we have GPS data and haven't yet set the RTC, set the clock now.
  /*if( (GPS_hour != 0 or GPS_minutes != 0 or GPS_seconds != 0 or GPS_milliseconds != 0)
      and !already_set_RTC_from_GPS) {

    // we have something coming in from the GPS, and haven't yet set the real time 
    // clock, so set the RTC now using the last data sentence from the GPS. This isn't
    // the most accurate way to do it, but it ought to get us within a second of the
    // correct time.

    // To set the RTC with an explicit date & time: September 1, 1988, 7:37:00 am do this:
    // rtc.adjust(DateTime(1988, 9, 1, 7, 37, 0));
    rtc.adjust(DateTime(RTC_year, RTC_month, RTC_day_of_month,
                        GPS_hour, GPS_minutes, GPS_seconds));
    already_set_RTC_from_GPS = true;
  }*/

  // Below to test RTC
  // talk to the realtime clock and print its information. note that unless you've 
  // synchronized the RTC and GPS, they won't necessarily agree.
  // DS3231_query();
  // Serial.print("RTC_hour = "); Serial.print(RTC_hour);
  // Serial.print("   RTC_minute = "); Serial.print(RTC_minute);
  // Serial.print("   RTC_second = "); Serial.println(RTC_second);
  // delay(500);
  
  // Write time data
  myFile.print(GPS_hour); myFile.print(',');
  myFile.print(GPS_minutes); myFile.print(',');
  myFile.print(GPS_seconds); myFile.print(',');
  myFile.println(GPS_milliseconds);

  // Write BME data. temperature in °C, pressure in hPa, humidity in %, altitude in m
  myFile.print(bme.temperature);
  myFile.print(",");
  myFile.print(bme.pressure / 100.0);
  myFile.print(",");
  myFile.print(bme.humidity);
  myFile.print(",");
  myFile.println(bme.readAltitude(SEALEVELPRESSURE_HPA));
  myFile.println();

  // To stop recording data if * was pressed
  if (kpd.getKeys() && kpd.key[0].kstate == PRESSED) {
    the_key =  kpd.key[0].kchar;
    if (the_key == '*') {
      Serial.println("Data acquisition stopped from keyboard");
      myFile.println("Data acquisition stopped from keyboard");
      myFile.close();
      LCD_message("Stopped", "from keyboard");
      delay(100);
      exit(0);
    }
  }

  // Record data every 0.1s.
  delay(100);
}

//////////////////////////////////////////////////////////////////////
////////////////////////// LCD_message function //////////////////////
//////////////////////////////////////////////////////////////////////

void LCD_message(String line1, String line2)
{
  // write two lines (of 16 characters each, maximum) to the LCD display.
  // I assume an object named "lcd" has been created already, has been 
  // initialized in setup, and is global.

  // set the cursor to the beginning of the first line, clear the line, then write.
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print(line1);

  // now do the next line.
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(line2);

  return;
}


//////////////////////////////////////////////////////////////////////
////////////////////// DS3231_query function /////////////////////////
//////////////////////////////////////////////////////////////////////

/*void DS3231_query()
{
  // read from the DS3231 real time clock.

  // these were already declared as global variables.
  // int RTC_minute, RTC_second;
  
  // we have already instantiated the device as an object named "now" so we can
  // call its class functions.
  DateTime now = rtc.now();

  lcd.setCursor(0, 0);
  lcd.print("Realtime clock  ");
  
  // write time information to LCD. Year, etc. first, after setting LCD to second line.
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(now.year(), DEC);
  lcd.print('/');
  lcd.print(now.month(), DEC);
  lcd.print('/');
  lcd.print(now.day(), DEC);

  // delay a second
  delay(1000);

  // now display the day of the week and time.
  RTC_hour = now.hour();
  RTC_minute = now.minute();
  RTC_second = now.second();  
  
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(daysOfTheWeek[now.dayOfTheWeek()]);
  lcd.print(". ");
  
  lcd.print(RTC_hour, DEC);
  lcd.print(':');
  if (RTC_minute < 10) {lcd.print('0');}
  lcd.print(RTC_minute, DEC);
  lcd.print(':');
  if (RTC_second < 10) {lcd.print('0');}
  lcd.print(RTC_second, DEC);

  // there are other functions available, such as 
  //    now.unixtime();
  //    DateTime future (now + TimeSpan(7,12,30,6));
  //    future.year(); etc. etc.

}*/



//////////////////////////////////////////////////////////////////////
////////////////////// GPS_query function /////////////////////////
//////////////////////////////////////////////////////////////////////

int GPS_query()
{

  // return 0 if we found good GPS navigational data and -1 if not.
  
  // The GPS device has its own microprocessor and, once we have loaded its parameters,
  // free-runs at a 1 Hz sampling rate. We do not trigger its registration of
  // latitude and longitude, rather we just read from it the last data record
  // it has stored. And we do it one character at a time!

  // I will keep reading from the GPS until I have a complete sentence carrying valid
  // navigational data, with a maximum number of reads to prevent the program from hanging. Once
  // the GPS reports its updates latitude/longitude information, we'll push this to the 
  // LCD display, then return to the main loop. 
   
  // zero out (or set to defaults) values returned by the GPS device in case we can't 
  // get it to respond.
  GPS_hour = GPS_minutes = GPS_seconds = GPS_milliseconds = 0;

  GPS_AV_code_string = "V";

  GPS_latitude_1 = GPS_latitude_2 = 0;
  GPS_latitude_NS_string = "x";
  
  GPS_longitude_1 = GPS_longitude_2 = 0;
  GPS_longitude_EW_string = "y";
    
  GPS_speed_knots = 0.;
  GPS_direction = 0.;
    
  GPS_date_string = "000000";
  
  GPS_fix_quality = GPS_satellites = 0;
  
  GPS_altitude = 0.;

  // initialize the number-of-reads counter since we might find ourselves
  // with an unparseable data record, or an unresponsive GPS, and want to keep trying.
  // This will let me protect against the data logger's program hanging. 
  GPS_char_reads = 0;

  // set a flag saying we want to keep trying; we'll use this to keep looking for
  // useful navigation when the GPS is working fine, but still looking for satellites.
  bool keep_trying = true;
  
  // Stay inside the following loop until we've read a complete GPS sentence with
  // good navigational data, or else the loop times out. With GPS_char_reads_maximum 
  // set to a million this'll take about 6 seconds to time out. 
  
  while (true) {
  
    // if we get back to this point but the keep_trying flag is false, we'll want
    // to declare failure and quit.

    if(!keep_trying) return -1;
    
    // this gets the last sentence read from GPS and clears a newline flag in the Adafruit 
    // library code.
    GPS_sentence = GPS.lastNMEA();
  
    while(GPS_char_reads <= GPS_char_reads_maximum) 
      {
  
      // try to read a single character from the GPS device.
      char single_GPS_char = GPS.read();
  
      // bump the number of times we've tried to read from the GPS.
      GPS_char_reads++;
  
      // now ask if we've received a complete data sentence. If yes, break
      // out of this loop.
  
      if(GPS.newNMEAreceived()) break;
  
      }
  
      
    // if we hit the limit on the number of character reads we'e tried, print a message and bail out.
    if (GPS_char_reads >= GPS_char_reads_maximum) 
      {

      keep_trying = false;
      
      Serial.println("GPS navigation data not yet available. Try again later.");
      //LCD_message("GPS navigation  ", "data unavailable");
        
      return -1;
        
      }

    // get the last complete sentence read from GP; this automatically clears a newline flag inside 
    // the Adafruit library code.
    GPS_sentence = GPS.lastNMEA();
    
    // convert GPS data sentence from a character array to a string.
    GPS_sentence_string = String(GPS_sentence);
  
    // now do a cursory check that the sentence we've just read is OK. Check that there is only
    // one $, as the first character in the sentence, and that there's an asterisk (which commes 
    // immediately before the checksum).
     
    // sentence starts with a $? 
    bool data_OK = GPS_sentence_string.charAt(1) == '$';    
  
    // sentence contains no other $? The indexOf call will return -1 if $ is not found.
    data_OK = data_OK && (GPS_sentence_string.indexOf('$', 2) <  0);
    
    // now find that asterisk...
    data_OK = data_OK && (GPS_sentence_string.indexOf('*', 0) >  0);
  
    if (GPSECHO1) 
      {
      Serial.println("\n******************\njust received a complete sentence, so parse stuff. Sentence is");
      Serial.println(GPS_sentence_string);
      }
  
    // now parse the GPS sentence. I am only interested in sentences that begin with
    // $GPGGA ("GPS fix data") or $GPRMC ("recommended minimum specific GPS/Transit data").
  
    if(GPSECHO1)
      {
      Serial.print("length of GPS_sentence_string just received...");
      Serial.println(GPS_sentence_string.length());
      }
  
    // now get substring holding the GPS command. Only proceed if it is $GPRMC or $GPGGA.
    GPS_command = GPS_sentence_string.substring(0, 7);
  
    // also trim it to make sure we don't have hidden stuff or white space sneaking in.
    GPS_command.trim();
  
    if(GPSECHO1) 
      {
      Serial.print("GPS command is "); Serial.println(GPS_command);
      }   
  
    // if data_OK is true then we have a good sentence. but we also need the sentence
    // to hold navigational data we can use, otherwise we'll want to keep listening.
    // we can only work with GPRMC and GPGGA sentences. 
  
    bool command_OK = GPS_command.equals("$GPRMC") || GPS_command.equals("$GPGGA"); 
    
    // if we have a sentence that, upon cursory inspection, is well formatted AND might
    // hold navigational data, continue to parse the sentence. If the GPS device
    // hasn't found any satellites yet, we'll want to go back to the top of the loop
    // to keep trying, rather than declaring defeat and returning.
  
    //////////////////////////////////////////////////////////////////////
    /////////////////////////// GPRMC sentence ///////////////////////////
    //////////////////////////////////////////////////////////////////////
    
     if (data_OK && GPS_command.equals("$GPRMC"))
        {
            
        if(GPSECHO2) 
          {
          Serial.print("\nnew GPS sentence: "); Serial.println(GPS_sentence_string);
          }
    
        // parse the time. these are global variables, already declared.

        GPS_hour_string = GPS_sentence_string.substring(GPRMC_hour_index1, GPRMC_hour_index2);
        GPS_minutes_string = GPS_sentence_string.substring(GPRMC_minutes_index1, GPRMC_minutes_index2);
        GPS_seconds_string = GPS_sentence_string.substring(GPRMC_seconds_index1, GPRMC_seconds_index2);
        GPS_milliseconds_string = GPS_sentence_string.substring(GPRMC_milliseconds_index1, 
          GPRMC_milliseconds_index2);
        GPS_AV_code_string = GPS_sentence_string.substring(GPRMC_AV_code_index1, GPRMC_AV_code_index2);
    
        GPS_hour = GPS_hour_string.toInt();
        GPS_minutes = GPS_minutes_string.toInt();
        GPS_seconds = GPS_seconds_string.toInt();
        GPS_milliseconds = GPS_milliseconds_string.toInt();
    
        if(GPSECHO2)
          {
          Serial.print("Time (UTC) = "); Serial.print(GPS_hour); Serial.print(":");
          Serial.print(GPS_minutes); Serial.print(":");
          Serial.print(GPS_seconds); Serial.print(".");
          Serial.println(GPS_milliseconds);
          Serial.print("A/V code is "); Serial.println(GPS_AV_code_string);
          }
    
        // now see if the data are valid: we'll expect an "A" as the AV code string.
        // We also expect an asterisk two characters from the end. Also check that the sentence 
        // is at least as long as the minimum length expected.
    
        data_OK = GPS_AV_code_string == "A";
    
        // now look for the asterisk after trimming any trailing whitespace in the GPS sentence.
        // the asterisk preceeds the sentence's checksum information, which I won't bother to check.
        int asterisk_should_be_here = GPS_sentence_string.length() - 4; 
    
        data_OK = data_OK && (GPS_sentence_string.charAt(asterisk_should_be_here) == '*');

        if(GPSECHO2)
          {
          Serial.print("expected asterisk position "); Serial.print(asterisk_should_be_here); 
          Serial.print(" at that position: "); Serial.println(GPS_sentence_string.charAt(asterisk_should_be_here));
          }
    
        // now check that the sentence is not too short.      
        data_OK = data_OK && (GPS_sentence_string.length() >= GPSMINLENGTH);
    
        if (!data_OK) 
          {

          keep_trying = true;
           
          if (GPSECHO1)
            {
            Serial.print("GPS sentence not good for navigation: "); Serial.println(GPS_sentence_string);
            Serial.println("I will keep trying...");
            }
            
          lcd.setCursor(0, 0);
          lcd.print("GPS navigation  ");
          lcd.setCursor(0, 1);
          lcd.print("data not present");
          
          }
    
        // if data are not good, go back to the top of the loop by breaking out of this if block.
        // we've already set keep_trying to be true.
        
        if (!data_OK) break;
            
        // so far so good, so keep going...
        
        // now parse latitude 
        
        GPS_latitude_1_string = GPS_sentence_string.substring(GPRMC_latitude_1_index1, 
          GPRMC_latitude_1_index2);
        GPS_latitude_2_string = GPS_sentence_string.substring(GPRMC_latitude_2_index1, 
          GPRMC_latitude_2_index2);
        GPS_latitude_NS_string = GPS_sentence_string.substring(GPRMC_latitude_NS_index1, 
          GPRMC_latitude_NS_index2);
    
        GPS_latitude_1 = GPS_latitude_1_string.toInt();      
        GPS_latitude_2 = GPS_latitude_2_string.toInt();      
    
        if(GPSECHO2)
          {
          Serial.print("Latitude x 100 = "); Serial.print(GPS_latitude_1); Serial.print(".");
          Serial.print(GPS_latitude_2); Serial.println(GPS_latitude_NS_string);
          }
          
        // now parse longitude 
        
        GPS_longitude_1_string = GPS_sentence_string.substring(GPRMC_longitude_1_index1, 
          GPRMC_longitude_1_index2);
        GPS_longitude_2_string = GPS_sentence_string.substring(GPRMC_longitude_2_index1, 
          GPRMC_longitude_2_index2);
        GPS_longitude_EW_string = GPS_sentence_string.substring(GPRMC_longitude_EW_index1, 
          GPRMC_longitude_EW_index2);
    
        GPS_longitude_1 = GPS_longitude_1_string.toInt();      
        GPS_longitude_2 = GPS_longitude_2_string.toInt();      
          
        if(GPSECHO2)
          {
          Serial.print("Longitude x 100 = "); Serial.print(GPS_longitude_1); Serial.print(".");
          Serial.print(GPS_longitude_2); Serial.println(GPS_longitude_EW_string); 
          }
    
        // now parse speed and direction. we'll need to locate the 7th and 8th commas in the
        // data sentence to do this. so use the indexOf function to find them.
        // it returns -1 if string wasn't found. the number of digits is not uniquely defined 
        // so we need to find the fields based on the commas separating them from others.
        
        int comma_A_index = GPRMC_longitude_EW_index2;
        int comma_B_index = GPS_sentence_string.indexOf(",", comma_A_index + 1);
        int comma_C_index = GPS_sentence_string.indexOf(",", comma_B_index + 1);
    
        GPS_speed_knots_string = GPS_sentence_string.substring(comma_A_index + 1, comma_B_index); 
        GPS_direction_string = GPS_sentence_string.substring(comma_B_index + 1, comma_C_index); 
        
        GPS_speed_knots = GPS_speed_knots_string.toFloat();
        GPS_direction = GPS_direction_string.toFloat();
    
        if(GPSECHO2)
          {
          Serial.print("Speed (knots) = "); Serial.println(GPS_speed_knots);
          Serial.print("Direction (degrees) = "); Serial.println(GPS_direction);
          }
          
        // now get the (UTC) date, in format DDMMYY, e.g. 080618 for 8 June 2018.
        GPS_date_string = GPS_sentence_string.substring(comma_C_index+ + 1, comma_C_index + 7);
        
        if(GPSECHO2)
          {
          Serial.print("date, in format ddmmyy = "); Serial.println(GPS_date_string);    
          }
    
        // Write message to LCD now. It will look like this (no satellite data in this record):
        //     Sats: 4006.9539N
        //     N/A  08815.4431W
        
        lcd.setCursor(0, 0);
        lcd.print("Sats: ");
        lcd.setCursor(6, 0);
        lcd.print(GPS_latitude_1_string); lcd.print("."); lcd.print(GPS_latitude_2_string); 
        lcd.print(GPS_latitude_NS_string); 
    
        lcd.setCursor(0, 1);
        lcd.print("N/A ");
        lcd.setCursor(5, 1);
        lcd.print(GPS_longitude_1_string); lcd.print("."); lcd.print(GPS_longitude_2_string); 
        lcd.print(GPS_longitude_EW_string);

        // print a summary of the data and parsed results:
        if(GPSECHO3)
          {
          Serial.print("GPS sentence: "); Serial.println(GPS_sentence_string);

          Serial.print("Time (UTC) = "); Serial.print(GPS_hour); Serial.print(":");
          Serial.print(GPS_minutes); Serial.print(":");
          Serial.print(GPS_seconds); Serial.print(".");
          Serial.println(GPS_milliseconds);
        
          Serial.print("Latitude x 100 = "); Serial.print(GPS_latitude_1); Serial.print(".");
          Serial.print(GPS_latitude_2); Serial.print(" "); Serial.print(GPS_latitude_NS_string);

          Serial.print("    Longitude x 100 = "); Serial.print(GPS_longitude_1); Serial.print(".");
          Serial.print(GPS_longitude_2); Serial.print(" "); Serial.println(GPS_longitude_EW_string); 

          Serial.print("Speed (knots) = "); Serial.print(GPS_speed_knots);
          Serial.print("     Direction (degrees) = "); Serial.println(GPS_direction);

          Serial.println("There is no satellite or altitude information in a GPRMC data sentence.");
              
          }
      
        // all done with this sentence, so return.
        return 0;
          
        }  // end of "if (data_OK && GPS_command.equals("$GPRMC"))" block

    //////////////////////////////////////////////////////////////////////
    /////////////////////////// GPGGA sentence ///////////////////////////
    //////////////////////////////////////////////////////////////////////
    
      if (data_OK && GPS_command.equals("$GPGGA"))
        {

        if(GPSECHO2) 
          {
          Serial.print("\nnew GPS sentence: "); Serial.println(GPS_sentence_string);
          }
    
        // parse the time
    
        GPS_hour_string = GPS_sentence_string.substring(GPGGA_hour_index1, GPGGA_hour_index2);
        GPS_minutes_string = GPS_sentence_string.substring(GPGGA_minutes_index1, GPGGA_minutes_index2);
        GPS_seconds_string = GPS_sentence_string.substring(GPGGA_seconds_index1, GPGGA_seconds_index2);
        GPS_milliseconds_string = GPS_sentence_string.substring(GPGGA_milliseconds_index1, 
          GPGGA_milliseconds_index2);
    
        GPS_hour = GPS_hour_string.toInt();
        GPS_minutes = GPS_minutes_string.toInt();
        GPS_seconds = GPS_seconds_string.toInt();
        GPS_milliseconds = GPS_milliseconds_string.toInt();
    
        if(GPSECHO2)
          {
          Serial.print("Time (UTC) = "); Serial.print(GPS_hour); Serial.print(":");
          Serial.print(GPS_minutes); Serial.print(":");
          Serial.print(GPS_seconds); Serial.print(".");
          Serial.println(GPS_milliseconds);
          }
    
        // now get the fix quality and number of satellites.
    
        GPS_fix_quality_string = GPS_sentence_string.substring(GPGGA_fix_quality_index1, 
          GPGGA_fix_quality_index2);
        GPS_satellites_string = GPS_sentence_string.substring(GPGGA_satellites_index1, 
          GPGGA_satellites_index2);
    
        int GPS_fix_quality = GPS_fix_quality_string.toInt();      
        int GPS_satellites = GPS_satellites_string.toInt();      
    
        if(GPSECHO2)
          {
          Serial.print("fix quality (1 for GPS, 2 for DGPS) = "); Serial.println(GPS_fix_quality);
          Serial.print("number of satellites = "); Serial.println(GPS_satellites);
          }
    
        // now see if the data are valid: we'll expect a fix, and at least three satellites.
    
        bool data_OK = (GPS_fix_quality > 0) && (GPS_satellites >= 3); 
    
        // now look for the asterisk.
        int asterisk_should_be_here = GPS_sentence_string.length() - 4; 
    
        data_OK = data_OK && (GPS_sentence_string.charAt(asterisk_should_be_here) == '*');
    
        // now check that the sentence is not too short.      
        data_OK = data_OK && (GPS_sentence_string.length() >= GPSMINLENGTH);

        if (!data_OK) 
          {

          keep_trying = true;
           
          if (GPSECHO1)
            {
            Serial.print("GPS sentence not good for navigation: "); Serial.println(GPS_sentence_string);
            Serial.println("I will keep trying...");
            }
            
          lcd.setCursor(0, 0);
          lcd.print("GPS navigation  ");
          lcd.setCursor(0, 1);
          lcd.print("data not present");
          
          }
    
        // if data are not good, go back to the top of the loop by breaking out of this if block.
        
        if (!data_OK) break;
            
        // so far so good, so keep going...
        
        // now parse latitude 
        
        String GPS_latitude_1_string = GPS_sentence_string.substring(GPGGA_latitude_1_index1, 
        GPGGA_latitude_1_index2);
        String GPS_latitude_2_string = GPS_sentence_string.substring(GPGGA_latitude_2_index1, 
        GPGGA_latitude_2_index2);
        String GPS_latitude_NS_string = GPS_sentence_string.substring(GPGGA_latitude_NS_index1, 
        GPGGA_latitude_NS_index2);
    
        int GPS_latitude_1 = GPS_latitude_1_string.toInt();      
        int GPS_latitude_2 = GPS_latitude_2_string.toInt();      
    
        if(GPSECHO2)
          {
          Serial.print("Latitude x 100 = "); Serial.print(GPS_latitude_1); Serial.print(".");
          Serial.print(GPS_latitude_2); Serial.println(GPS_latitude_NS_string);
          }
          
        // now parse longitude 
        
        String GPS_longitude_1_string = GPS_sentence_string.substring(GPGGA_longitude_1_index1, 
        GPGGA_longitude_1_index2);
        String GPS_longitude_2_string = GPS_sentence_string.substring(GPGGA_longitude_2_index1, 
        GPGGA_longitude_2_index2);
        String GPS_longitude_EW_string = GPS_sentence_string.substring(GPGGA_longitude_EW_index1, 
        GPGGA_longitude_EW_index2);
    
        int GPS_longitude_1 = GPS_longitude_1_string.toInt();      
        int GPS_longitude_2 = GPS_longitude_2_string.toInt();      
    
        if(GPSECHO2)
          {         
          Serial.print("Longitude x 100 = "); Serial.print(GPS_longitude_1); Serial.print(".");
          Serial.print(GPS_longitude_2); Serial.println(GPS_longitude_EW_string); 
          }
          
        // let's skip the "horizontal dilution" figure and go straight for the altitude now.
        // this begins two fields to the right of the num,ber of satellites so find this
        // by counting commas. use the indexOf function to find them.
        int comma_A_index = GPS_sentence_string.indexOf(",", GPGGA_satellites_index2 + 1);
        int comma_B_index = GPS_sentence_string.indexOf(",", comma_A_index + 1);
    
        String GPS_altitude_string = GPS_sentence_string.substring(comma_A_index + 1, comma_B_index); 
        
        float GPS_altitude = GPS_altitude_string.toFloat();
    
        if(GPSECHO2)
          {
          Serial.print("Altitude (meters) = "); Serial.println(GPS_altitude);
          }
    
        // Write message to LCD now. It will look like this:
        //     Sats: 4006.9539N
        //       10 08815.4431W
             
        lcd.setCursor(0, 0);
        lcd.print("Sats: ");
        lcd.setCursor(6, 0);
        lcd.print(GPS_latitude_1_string); lcd.print("."); lcd.print(GPS_latitude_2_string); 
        lcd.print(GPS_latitude_NS_string); 
    
        lcd.setCursor(0, 1);
        lcd.print("      ");
        lcd.setCursor(2, 1);
        lcd.print(GPS_satellites);
        lcd.setCursor(5, 1);
        lcd.print(GPS_longitude_1_string); lcd.print("."); lcd.print(GPS_longitude_2_string); 
        lcd.print(GPS_longitude_EW_string);

        // print a summary of the data and parsed results:
        if(GPSECHO3) {
          Serial.print("GPS sentence: "); Serial.println(GPS_sentence_string);

          Serial.print("Time (UTC) = "); Serial.print(GPS_hour); Serial.print(":");
          Serial.print(GPS_minutes); Serial.print(":");
          Serial.print(GPS_seconds); Serial.print(".");
          Serial.println(GPS_milliseconds);
        
          Serial.print("Latitude x 100 = "); Serial.print(GPS_latitude_1); Serial.print(".");
          Serial.print(GPS_latitude_2); Serial.print(" "); Serial.print(GPS_latitude_NS_string);

          Serial.print("    Longitude x 100 = "); Serial.print(GPS_longitude_1); Serial.print(".");
          Serial.print(GPS_longitude_2); Serial.print(" "); Serial.println(GPS_longitude_EW_string); 

          Serial.print("Speed (knots) = "); Serial.print(GPS_speed_knots);
          Serial.print("     Direction (degrees) = "); Serial.println(GPS_direction);

          Serial.print("Number of satellites: "); Serial.print(GPS_satellites);
          Serial.print("       Altitude (meters): "); Serial.println(GPS_altitude);
              
        }
       
        // all done with this sentence, so return.
        return 0;
       
    }   // end of "if (data_OK && GPS_command.equals("$GPGGA"))" block
  
    // we'll fall through to here (instead of returning) when we've read a complete 
    // sentence, but it doesn't have navigational information (for example, an antenna 
    // status record).
    
  }
}
