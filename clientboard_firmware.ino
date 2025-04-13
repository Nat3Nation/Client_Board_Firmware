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

#define I2C_SDA 36
#define I2C_SCL 33

#define RELAY_PIN 27

// 0x68 is the default address for all MCP342x devices
uint8_t address = 0x6E;
MCP342x adc = MCP342x(address);

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define COMMAND_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DATA_UUID           "fb9ed969-b64c-4ea8-9111-325e3687b3fb"

BLECharacteristic *cCharacteristic;
BLECharacteristic *dCharacteristic;
String last_cmd, current_cmd;

//int json_state = 0;

ESP32Timer ITimer0(0); // Timer 0
ESP32Timer ITimer1(1); // Timer 1

// Timer 1 ISR
bool IRAM_ATTR adc_read(void* timer_arg){
  long energy_values[2];
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
    return false;
  }
  else {
    uint8_t *data = (uint8_t *)energy_values;
    //check the length of the data pointer
    dCharacteristic->setValue(data, 8);
    return true;
  }
}

// Timer 2 ISR
bool IRAM_ATTR BT_command_rc(void* timer_arg) {
  //Actuate Relay when Command recieved - Expand to include sending data back
  current_cmd = cCharacteristic->getValue().c_str();
  if(current_cmd != last_cmd){
    char command = current_cmd[0];
    Serial.print("Actuate Command Received: ");
    Serial.print(command);
    Serial.print("\n");
    Actuate(command);
    return true;
  }
  last_cmd = cCharacteristic->getValue().c_str();
  return false;
}

void Actuate(char command){ //https://techtutorialsx.com/2018/04/27/esp32-arduino-bluetooth-classic-controlling-a-relay-remotely/
  //Switch relay based on command recieved from user
  if (command == '1'){
    digitalWrite(RELAY_PIN, HIGH);
  }else{
    digitalWrite(RELAY_PIN, LOW);
  }
}

void setup()
{
  // Initialize Serial Communication
  Serial.begin(115200);
  delay(100);

  Wire.begin(I2C_SDA, I2C_SCL, 100000);

  // Wait for the user to press start
  Serial.println(" --- Energy Prediction and Monitoring System --- \n");
  Serial.println("Please press enter to continue...");
  while(Serial.available() == 0);
  flushInputBuffer();

  // Connect to the server
  esp_log_level_set("*", ESP_LOG_NONE);
  delay(100);

  // Initiate the expander
  //expander.begin();
  //delay(100);

    // Reset devices
  MCP342x::generalCallReset();
  delay(1); // MC342x needs 300us to settle, wait 1ms
  
  // Check device present
  Wire.requestFrom(address, (uint8_t)1);
  if (!Wire.available()) {
    Serial.print("No device found at address ");
    Serial.println(address, HEX);
    while (1)
      ;
  }

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
                                BLECharacteristic::PROPERTY_WRITE
                              );
  dCharacteristic = pService->createCharacteristic(
                                DATA_UUID,
                                BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_WRITE
                              );
  cCharacteristic->setValue("0");
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

  // Start timer 0
  ITimer0.setFrequency(3.0, adc_read);
  // Start timer 1
  ITimer1.setFrequency(0.5, BT_command_rc);

}

void loop()
{
  while(1){};
}
