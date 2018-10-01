#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <SPI.h>
#include "SdFat.h"
#include <Keypad.h>

//BME variables
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME680 bme; // I2C
//Adafruit_BME680 bme(BME_CS); // hardware SPI
//Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO,  BME_SCK);

// SD card variable
// "#define" is an instruction to the compiler. In this case, it will
// replace all occurrences of "SD_CS_PIN" with "SS", whose value is
// already known to the compiler.
#define SD_CS_PIN SS
// create a file object to which I'll write data
File myFile;
// file name
char filename[ ] = "testSD.txt";
// instantiate a file system object
SdFat SD;

//keypad variable;
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


void setup() {
  //BME set up
  Serial.begin(9600);
  while (!Serial);
  Serial.println(F("BME680 test"));

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

  //SD card set up
  // Open serial communications and wait for port to open:
  while (!Serial) { }
  // push a message to the serial monitor window
  Serial.print("Initializing SD card reading software... ");
  // fire up the SD file system object and check that all is fine so far.
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD initialization failed!");
    // delay a bit to give the serial port time, then bail out...
    delay(100);
    exit(0);
  }
  // so far so good.
  Serial.println("initialization done.");

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
    myFile.println("Temperature(°C), pressure(hPa), humidity(%), altitude(m)");    

    } else {
       
    // if the file didn't open, print an error:
    Serial.print("error opening "); Serial.println(filename); 

    // delay a bit to give the serial port time to display the message...
    delay(100);
    }

}

void loop() {
  char the_key;
  if (!bme.performReading()) {
    myFile.println("Failed to perform reading :(");
    Serial.println("Failed to perform reading :(");
    return;
  }
  //Write BME data. temperature in °C, pressure in hPa, humidity in %, altitude in m
  myFile.print(bme.temperature);
  myFile.print(",");
  myFile.print(bme.pressure / 100.0);
  myFile.print(",");
  myFile.print(bme.humidity);
  myFile.print(",");
  myFile.println(bme.readAltitude(SEALEVELPRESSURE_HPA));
  myFile.println();

  //To stop recording data if * was pressed
  if (kpd.getKeys() && kpd.key[0].kstate == PRESSED) {
    the_key =  kpd.key[0].kchar;
    Serial.print("Just detected the "); Serial.print(the_key); Serial.println(" key.");
    if (the_key == '*') {
      Serial.println("Data acquisition stopped from keyboard");
      myFile.println("Data acquisition stopped from keyboard");
      myFile.close();
      delay(100);
      exit(0);
    }
  }

  //Record data every 0.1s.
  delay(100);
}
