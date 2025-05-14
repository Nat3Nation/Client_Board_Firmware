#include <Arduino.h>
#include "Utils.h"

bool isNumeric(String str) {
  for (int i = 0; i < str.length(); i++) {
    if (!isDigit(str.charAt(i))) {
      return false;
    }
  }
  return true;
}

void flushInputBuffer() {
  Serial.flush();
  while(Serial.read() != -1);
}

String getUserInput(String prompt) {
  Serial.print(prompt);
  delay(10);
  String out = "";
  flushInputBuffer();
  while(1) {
    if (Serial.available()) {
      char input = Serial.read();
      if (input == '\r') {
        Serial.println();
        break;
      } else if (input == DEL && out.length() > 0) {
        out.remove(out.length() - 1);
        Serial.write("\b \b");
      } else {
        out += input;
        Serial.write(input);
      }
    }
  }
  return out;
}