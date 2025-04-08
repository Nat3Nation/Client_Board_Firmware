//#include <esp32-hal-timer.h>
//#include <esp_task_wdt.h>
//#include <soc/soc.h>
#include <Wire.h>
#include <MCP342x.h>
#define TIMER_BASE_CLK    (APB_CLK_FREQ)  // Add this before include
#include <ESP32TimerInterrupt.h>

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

//int json_state = 0;

//VoltageRMSRegs RMS_voltage; 
//CurrentRMSRegs RMS_current;

ESP32Timer ITimer0(0); // Timer 0
ESP32Timer ITimer1(1); // Timer 1

// Timer 1 ISR
bool IRAM_ATTR adc_read(void* timer_arg){
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

  if (err1|err2) {
    Serial.println("Convert error");
  }
  else {
    SerialBT.print("{");
    SerialBT.print(voltage);
    SerialBT.print(",");
    SerialBT.print(current);
    SerialBT.println("}");
  }
}

// Timer 2 ISR
bool IRAM_ATTR BT_command_rc(void* timer_arg) {
  //Actuate Relay when Command recieved - Expand to include sending data back
  if(SerialBT.available()){
    char command = SerialBT.read();
    Serial.print("Actuate Command Received: ");
    Serial.print(command);
    Serial.print("\n");
    Actuate(command);
  }
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
  if(!SerialBT.begin("ESP32")){ //https://techtutorialsx.com/2018/04/27/esp32-arduino-bluetooth-classic-controlling-a-relay-remotely/
    Serial.println("An error occurred initializing Bluetooth");
  }else{
    Serial.println("Bluetooth initialized");
  }

  // Start timer 0
  ITimer0.setFrequency(3.0, adc_read);
  // Start timer 1
  ITimer1.setFrequency(0.5, BT_command_rc);

}

void loop()
{
  while(1){};
}
