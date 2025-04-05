//#include <esp32-hal-timer.h>
//#include <esp_task_wdt.h>
//#include <soc/soc.h>
#include <Wire.h>
#include <MCP342x.h>

#include "BluetoothSerial.h" //https://techtutorialsx.com/2018/04/27/esp32-arduino-bluetooth-classic-controlling-a-relay-remotely/
#include "Utils.h"
#include "esp_log.h"

#define I2C_SDA 36
#define I2C_SCL 33

#define RELAY_PIN 27

// 0x68 is the default address for all MCP342x devices
uint8_t address = 0x6E;
MCP342x adc = MCP342x(address);

BluetoothSerial SerialBT; //https://techtutorialsx.com/2018/04/27/esp32-arduino-bluetooth-classic-controlling-a-relay-remotely/

//float V_fsp = 0;
//unsigned long lastTime = 0;

Connection conn("http://192.168.1.34:8000/boards/authenticate"); 
//int json_state = 0;

//VoltageRMSRegs RMS_voltage; 
//CurrentRMSRegs RMS_current; 


void IRAM_ATTR timerISR();

/**
void ADE9000_setup(uint32_t SPI_speed) {
  SPI.begin(26,25,33);    //Initiate SPI port 26,25,33
  SPI.beginTransaction(SPISettings(SPI_speed,MSBFIRST,SPI_MODE0));    //Setup SPI parameters
  ade_0.SPI_Init(SPI_speed, PCA_PIN_P10, LOW); // for ADE9000 id 0, the enable pin should be LOW
  ade_1.SPI_Init(SPI_speed, PCA_PIN_P10, HIGH); // for ADE9000 id 0, the enable pin should be HIGH
  ade_0.begin(); //set up the chip and get it running
  ade_1.begin();
  delay(200); //give some time for everything to come up
}
**/

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
  Serial.println();
  Serial.println("----------- Step 1: WiFi Connection -----------\n");
  conn.initWiFi();
  conn.initBackend();
  delay(100);

  // Initiate the expander
  expander.begin();
  delay(100);

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
  if(!SerialBT.begin("ESP32")){ //https://techtutorialsx.com/2018/04/27/esp32-arduino-bluetooth-classic-controlling-a-relay-remotely/
    Serial.println("An error occurred initializing Bluetooth");
  }else{
    Serial.println("Bluetooth initialized");
  }

}

void loop()
{
  conn.loop();
  delay(100);
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= 3000) {

    long voltage = 0;
    long current = 0;
    MCP342x::Config status;
    // Initiate a conversion; convertAndRead() will wait until it can be read
    uint8_t err1 = adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot,
				   MCP342x::resolution16, MCP342x::gain1,
				   1000000, voltage, status);

    uint8_t err2 = adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot,
				   MCP342x::resolution16, MCP342x::gain1,
				   1000000, current, status);

    if (err) {
      Serial.print("Convert error: ");
      Serial.println(err);
    }
    else {
      Serial.print("Value: ");
      Serial.println(value);
    }
    lastTime = currentTime;
  }
  delay(100);

  //Actuate Relay when Command recieved - Expand to include sending data back
  while(SerialBT.available()){
    char command = SerialBT.read();
    Serial.println(command);
    Actuate(command);
  }
  delay(50);

}