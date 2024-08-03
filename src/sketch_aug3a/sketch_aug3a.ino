
#include <EEPROM.h>

void setup() {
  // put your setup code here, to run once:
  // Start serial communication for debugging
  Serial.begin(9600);

  // Write a value to EEPROM at address 0
  int address = 0;
  byte value = 42;
  EEPROM.write(address, value);

  Serial.println("Value written to EEPROM");

}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Hello, world!");  // Print "Hello, world!" to the Serial Monitor
  delay(30000);  // Wait for 1 second
}
