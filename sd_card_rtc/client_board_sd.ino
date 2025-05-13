//#include <esp32-hal-timer.h>
//#include <esp_task_wdt.h>
//#include <soc/soc.h>
#include <Wire.h>
#include <MCP342x.h>
#define TIMER_BASE_CLK    (APB_CLK_FREQ)  // Add this before include
#include <ESP32TimerInterrupt.h>
#include "Utils.h"
#include "esp_log.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

//libraries for sd card and rtc
#include <SPI.h>
#include <SdFat.h>
#include <sdios.h>
#include "RTClib.h"

#define LED1 32

#define I2C_SDA 36
#define I2C_SCL 33

#define I2C_SDA_RTC 19
#define I2C_SCL_RTC 18

#define SCK   14
#define MISO  12
#define MOSI  13
#define CS    15
#define SPI_CLOCK SD_SCK_MHZ(25)
#define SD_CONFIG SdSpiConfig(CS, SHARED_SPI, SPI_CLOCK)

#define RELAY_PIN 27
#define ADC_PIN 6

String adc;

// 0x68 is the default address for all MCP342x devices
//uint8_t address = 0x6E;
//MCP342x adc = MCP342x(address);

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define COMMAND_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DATA_UUID           "fb9ed969-b64c-4ea8-9111-325e3687b3fb"

BLECharacteristic *cCharacteristic;
BLECharacteristic *dCharacteristic;
std::string last_cmd, current_cmd;

TwoWire I2CRTC = TwoWire(0);
RTC_DS3231 rtc;
SdFat sd;
SdFile file;

uint32_t cardSectorCount = 0;
uint8_t sectorBuffer[512];
SdCardFactory cardFactory;
SdCard* m_card = nullptr;

//int json_state = 0;

ESP32Timer ITimer0(0); // Timer 0
ESP32Timer ITimer1(1); // Timer 1

bool data_flag = false;

uint8_t dummy_value = 0;

//timer delay will likely need to change
unsigned long timerDelay = 30000;
int lastDay = 0;
unsigned long lastTime = 0;


void Actuate(char command){ //https://techtutorialsx.com/2018/04/27/esp32-arduino-bluetooth-classic-controlling-a-relay-remotely/
  //Switch relay based on command recieved from user
  if (command == '1'){
    digitalWrite(RELAY_PIN, HIGH);
    led_Flash(3, 100);
  }else{
    digitalWrite(RELAY_PIN, LOW);
    led_Flash(2, 500);
  }
}

// Callback when client writes
class WriteCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    std::string value = pChar->setValue(); //Changed to std::string Value
    Serial.printf("[Server] Received write: %s\n", value.c_str());
    cCharacteristic->indicate();  // Send indication to client

    //Actuate Relay when Command recieved - Expand to include sending data back
    char command = value[0];
    Serial.print("Actuate Command Received: ");
    Serial.print(command);
    Serial.print("\n");
    Actuate(command);
  }
};

// Timer 2 ISR
bool IRAM_ATTR read_ADC(void* timer_arg) {
  data_flag = true;
  return data_flag;
}

void led_Flash(uint16_t flashes, uint16_t delaymS)
{
  uint16_t index;

  for (index = 1; index <= flashes; index++)
  {
    digitalWrite(LED1, HIGH);
    delay(delaymS);
    digitalWrite(LED1, LOW);
    delay(delaymS);
  }
}

uint32_t const ERASE_SIZE = 262144L;

void eraseCard() {
  Serial.println("Erasing\n");
  uint32_t firstBlock = 0;
  uint32_t lastBlock;
  uint16_t n = 0;

  do {
    lastBlock = firstBlock + ERASE_SIZE - 1;
    if (lastBlock >= cardSectorCount) {
      lastBlock = cardSectorCount - 1;
    }
    if (!m_card->erase(firstBlock, lastBlock)) {
      Serial.print("erase failed");
    }
    firstBlock += ERASE_SIZE;
  } while (firstBlock < cardSectorCount);

  if (!m_card->readSector(0, sectorBuffer)) {
    Serial.print("erase error");
  }
}

void formatCard() {
  ExFatFormatter exFatFormatter;
  FatFormatter fatFormatter;

  // Format exFAT if larger than 32GB
  bool rtn = cardSectorCount > 67108864
               ? exFatFormatter.format(m_card, sectorBuffer, &Serial)
               : fatFormatter.format(m_card, sectorBuffer, &Serial);

  if (!rtn) {
    Serial.print("error");
  }
}

float getUsageRatio(SdFat& sd) {
  uint64_t total = sd.vol()->clusterCount();
  total *= sd.vol()->bytesPerCluster();
  uint64_t free = sd.vol()->freeClusterCount();
  free *= sd.vol()->bytesPerCluster();
  return (float)(total - free) / total;
}

String filePath = "";

// Generate file name based on date
String fileName(DateTime now) {
  String yearStr = String(now.year(), DEC);
  String monthStr = (now.month() < 10 ? "0" : "") + String(now.month(), DEC);
  String dayStr = (now.day() < 10 ? "0" : "") + String(now.day(), DEC);
  return "/SMESdata_" + dayStr + "_" + monthStr + "_" + yearStr + ".txt";
}

void initSDCard() {
  SPI.begin(SCK, MISO, MOSI, CS);
  if (!sd.begin(SD_CONFIG)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint32_t cardSizeMB = sd.card()->sectorCount() / 2048;
  Serial.printf("SD Card Size: %luMB\n", cardSizeMB);

  m_card = cardFactory.newCard(SD_CONFIG);
  if (!m_card ) {
    Serial.print("card init failed.");
    return;
  }
  cardSectorCount = m_card->sectorCount();
}

void writeFile(const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);
  if (!file.open(path, O_WRITE | O_CREAT | O_TRUNC)) {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.print(message);
  file.close();
  Serial.println("File written");
}

void appendFile(const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);
  if (!file.open(path, O_APPEND | O_WRITE)) {
    Serial.println("Failed to open file for appending");
    return;
  }
  file.print(message);
  file.close();
  Serial.println("Message appended");
}

void setup()
{
  // Initialize Serial Communication
  Serial.begin(115200);
  delay(100);

  I2CRTC.begin(I2C_SDA_RTC, I2C_SCL_RTC, 100000);

  //only used to set the time once
  /*if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }*/

  initSDCard();

  DateTime now = rtc.now();
  filePath = fileName(now);

  if (!file.open(filePath.c_str(), O_READ)) {
    Serial.println("File doesn't exist. Creating file...");
    writeFile(filePath.c_str(), "Energy Data, Time\r\n");
  } else {
    Serial.println("File already exists");
    file.close();
  }

  //Wire.begin(I2C_SDA, I2C_SCL, 100000);

  // Wait for the user to press start
  /*
  Serial.println(" --- Energy Prediction and Monitoring System --- \n");
  Serial.println("Please press enter to continue...");
  while(Serial.available() == 0);
  flushInputBuffer();
  */

  // Connect to the server
  esp_log_level_set("*", ESP_LOG_NONE);
  delay(100);

  // Initiate the expander
  //expander.begin();
  //delay(100);
  /*
    // Reset devices
  MCP342x::generalCallReset();
  delay(1); // MC342x needs 300us to settle, wait 1ms
  
  // Check device present
  Wire.requestFrom(address, (uint8_t)1);
  if (!Wire.available()) {
    Serial.print("No device found at address ");
    Serial.println(address, HEX);
    //while (1)
    //  ;
  }
  */
  // Initialize Relay Switching Pin
  pinMode(RELAY_PIN, OUTPUT); //https://techtutorialsx.com/2018/04/27/esp32-arduino-bluetooth-classic-controlling-a-relay-remotely/
  digitalWrite(RELAY_PIN, LOW);

  // Initialize Bluetooth for Commands/Data Transfer
  BLEDevice::init("Client_Board");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  cCharacteristic = pService->createCharacteristic(
                                COMMAND_UUID,
                                BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_WRITE |
                                BLECharacteristic::PROPERTY_NOTIFY
                              );
  dCharacteristic = pService->createCharacteristic(
                                DATA_UUID,
                                BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_WRITE |
                                BLECharacteristic::PROPERTY_NOTIFY
                              );
  cCharacteristic->setValue("0");
  cCharacteristic->setCallbacks(new WriteCallback());
  last_cmd = "0";
  dCharacteristic->setValue("None");
  
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  // Start timer 1
  ITimer1.setFrequency(0.2, read_ADC);
}


std::string energy_values = "Board-CB1,120.0,10.0";
void loop()
{
  if(data_flag) {
    Serial.println("Data Flag Set...");
    /*
    MCP342x::Config status;
    // Initiate a conversion; convertAndRead() will wait until it can be read
    uint8_t err1 = adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot,
        MCP342x::resolution16, MCP342x::gain1,
        1000000, energy_values[0], status);
    uint8_t err2 = adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot,
        MCP342x::resolution16, MCP342x::gain1,
        1000000, energy_values[1], status);

    if (err1|err2) {
      Serial.println("Convert error");
    }
    */
    //uint8_t *data = (uint8_t *)energy_values.c_str();
    //check the length of the data pointer
    //int ADC_val = analogRead(ADC_PIN);
	  //sprintf(adc, "Board-CB1,%s,1.0", std::to_string(ADC_val));
    //Serial.println("Sending Energy Data to Mainboard...");
    dCharacteristic->setValue(energy_values.c_str());
    //print out data to Serial monitor
    //Serial.println(values[0]);
    //Serial.println(values[1]);

    DateTime now = rtc.now();
    int dayOfMonth = now.day();
    int minuteOfHour = now.minute();
    String hourStr   = (now.hour() < 10 ? "0" : "") + String(now.hour(), DEC);
    String minuteStr = (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC);
    String secondStr = (now.second() < 10 ? "0" : "") + String(now.second(), DEC);
    
    if (dayOfMonth != lastDay) {
      if (getUsageRatio(sd) > 0.9) {
        Serial.print("erasing");
        eraseCard();
        formatCard();
        initSDCard();
      }
      filePath = fileName(now);
      writeFile(filePath.c_str(), "Energy Data, Time\r\n");
      lastDay = dayOfMonth;
    }

    String dataSD = energy_values + ", " + hourStr + ":" minuteStr + ":" + secondStr + "\r\n";
    appendFile(filePath.c_str(), data_SD.c_str());
    //For testing
    //dummy_value++;
    //dCharacteristic->setValue(&dummy_value, 1);
    dCharacteristic->notify();
    Serial.println(adc);

    data_flag = false;
  }
}