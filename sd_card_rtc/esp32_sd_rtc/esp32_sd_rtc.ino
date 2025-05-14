// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>

#include "RTClib.h"
#include <Wire.h>

#define SCK  14
#define MISO  12
#define MOSI  13
#define CS  15

#define I2C_SDA 19
#define I2C_SCL 18

TwoWire I2CRTC = TwoWire(0);

RTC_DS3231 rtc;

int lastDay = 0;
int lastMinute = 0;
unsigned long lastTime = 0;

//change based on frequency of measurement
unsigned long timerDelay = 30000;

String filePath = "";

String fileName(DateTime now){

  String yearStr = String(now.year(), DEC);
  String monthStr = (now.month() < 10 ? "0" : "") + String(now.month(), DEC);
  String dayStr = (now.day() < 10 ? "0" : "") + String(now.day(), DEC);

  return "/SMESdata_" + dayStr +  "_" + monthStr + "_" + yearStr + ".txt";
}

void initSDCard(){
  SPI.begin(SCK, MISO, MOSI, CS);
  if (!SD.begin(CS)){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

// Write to the SD card
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  I2CRTC.begin(I2C_SDA, I2C_SCL, 100000);
  
  if (! rtc.begin(&I2CRTC)) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  
//after the time is set, this needs to be removed and then the code recompiled?
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  initSDCard();

  DateTime now = rtc.now();

  filePath = fileName(now);
  
  File file = SD.open(filePath);
  if(!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, filePath.c_str(), "Energy Data, Time \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();


}

void loop() {
  if ((millis() - lastTime) > timerDelay){
    DateTime now = rtc.now();
    int dayOfMonth = now.day();
    String hourStr = (now.hour() < 10 ? "0" : "") + String(now.hour(), DEC); 
    String minuteStr = (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC);
    String secondStr = (now.second() < 10 ? "0" : "") + String(now.second(), DEC);

    if (dayOfMonth!=lastDay){
      filePath = fileName(now);
      lastDay = dayOfMonth;
    }

    String time = hourStr + "" + minuteStr + "" + secondStr + "\r\n";
    Serial.print("Saving data: ");
    Serial.println(time);

    //Append the data to file
    appendFile(SD, filePath.c_str(), time.c_str());

    lastTime = millis();

  }

}
