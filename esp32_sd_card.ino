// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>

// Libraries to get time from NTP Server
#include <WiFi.h>
#include "time.h"

#define SCK  14
#define MISO  12
#define MOSI  13
#define CS  15

// Replace with your network credentials
const char* ssid     = "";
const char* password = "";

// Timer variables
int lastDay = 0;
int lastMinute = 0;
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;

String filePath = "";

// NTP server to request epoch time
const char* ntpServer = "pool.ntp.org";

// Variable to save current epoch time
unsigned long epochTime; 

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

String fileName(struct tm timeinfo){

  char timeMonth[10];
  char timeYear[5];
  char timeDay[3];
  char timeMinute[3];

  strftime(timeMonth,10, "%B", &timeinfo);
  strftime(timeYear,5, "%Y", &timeinfo);
  strftime(timeDay,3, "%d", &timeinfo);
  strftime(timeMinute,3, "%M", &timeinfo);

  return "/SMESdata_minute" + String(timeMinute)+ "_" + String(timeDay) + "" + String(timeMonth) + "" + String(timeYear) + ".txt";
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

// Initialize SD card
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

// Append data to the SD card
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
  Serial.begin(115200);
  initWiFi();
  
  initSDCard();

  configTzTime("EST5EDT,M3.2.0,M11.1.0", ntpServer);
  
  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  epochTime = getTime();
  time_t rawtime = epochTime;
  struct tm *timeinfo = localtime(&rawtime);
  filePath = fileName(*timeinfo);
  
  File file = SD.open(filePath);
  if(!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, filePath.c_str(), "Epoch Time, Temperature, Humidity, Pressure \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    //Get epoch time
    epochTime = getTime();

    struct tm *timeinfo;
    time_t rawtime = epochTime;
    timeinfo = localtime(&rawtime);  // convert to calendar time
    //int dayOfMonth = timeinfo->tm_mday;
    int hourMinute = timeinfo->tm_min;
    Serial.println(hourMinute);

  if (hourMinute!=lastMinute){
    filePath = fileName(*timeinfo);
    lastDay = hourMinute;
  }
  

   /* real code
    if (dayOfMonth!=lastDay){
      filePath = fileName(*timeinfo);
      lastDay = dayOfMonth;
    }
    */
  
    //Concatenate all info separated by commas
    String dataMessage = String(epochTime) + "\r\n";
    Serial.print("Saving data: ");
    Serial.println(dataMessage);

    //Append the data to file
    appendFile(SD, filePath.c_str(), dataMessage.c_str());

    lastTime = millis();
  }
}
