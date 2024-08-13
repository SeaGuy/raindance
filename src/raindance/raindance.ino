#include <WiFiNINA.h>
#include <WiFiServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <RTClib.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <TimeLib.h>


// Custom RTC_DS3231 class with modified I2C address

#define DS3231_STATUSREG 0x0F
#define I2C_ADDRESS 0x60
int zones = 3;
int durationSeconds = 10;

class CustomRTC_DS3231 : public RTC_DS3231 {
public:
  boolean begin(TwoWire *wireInstance) {
    _wire = wireInstance;
    _wire->begin();
    return checkIsRunning();
  }

  boolean lostPower(void) {
    return (read_i2c_register(0x60, DS3231_STATUSREG) >> 7) & 0x1;
  }

protected:
  TwoWire *_wire; // Add this line to define the _wire member variable

  uint8_t read_i2c_register(uint8_t addr, uint8_t reg) {
    _wire->beginTransmission(I2C_ADDRESS); // Custom I2C address
    _wire->write((byte)reg);
    _wire->endTransmission();
    _wire->requestFrom(I2C_ADDRESS, (byte)1);
    return _wire->read();
  }

  void write_i2c_register(uint8_t addr, uint8_t reg, uint8_t val) {
    _wire->beginTransmission(I2C_ADDRESS); // Custom I2C address
    _wire->write((byte)reg);
    _wire->write((byte)val);
    _wire->endTransmission();
  }

  boolean checkIsRunning() {
    uint8_t status = read_i2c_register(I2C_ADDRESS, 0x0F); // Status register address
    return !(status & 0x80); // Check the oscillator stop flag (OSF)
  }
};

//CustomRTC_DS3231 rtc;

// WiFi settings
const char* ssid = "ARRIS-439E";
const char* password = "287860664144";

int status = WL_IDLE_STATUS;
WiFiServer server(80);

// Define the pin for the relay
const int relayPin = 7;

// RTC_DS3231 rtc;

void setup() {
  // put your setup code here, to run once:

  setTime(13,37,0,13,8,24);

  Serial.begin(9600);
  delay(500);
  while (!Serial) {
    Serial.println("setting up serial port");
    delay(500);
    ; // wait for SERIAL PORT TO CONNECT
    // flash LED
  }
  Serial.println("serial port connected");


  //if (!rtc.begin(&Wire)) {
    //Serial.println("Couldn't find RTC");
    //while (1);
  //}
  //Serial.println("RTC found");


  //if (rtc.lostPower()) {
    //Serial.println("RTC lost power, let us set the time!");
    // This line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // Or set a specific date & time
    // rtc.adjust(DateTime(2024, 7, 29, 12, 0, 0));
  //}
  //currentDateTime = rtc.now();
  //Serial.print("rtc.now(): ");
  //Serial.println(toString(currentDateTime));

  // create the alarms
  //Alarm.alarmRepeat(dowSaturday, 6, 0, 0, ScheduledSprinklerOn);  
  //Alarm.alarmRepeat(dowWednesday, 6, 0, 0, ScheduledSprinklerOn);  
  Alarm.alarmRepeat(dowTuesday, 13, 38, 0, ScheduledSprinklerOn);  

  // Initialize the relay pin as an output
  pinMode(relayPin, OUTPUT);
  Serial.println("configuring digital output pin for relay");

  // Ensure the relay is off initially
  digitalWrite(relayPin, LOW);

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, password);
    delay(3000);
  }
  server.begin();  
  IPAddress myIPAddress = WiFi.localIP();
  Serial.print("Connected to WiFi with address ");
  Serial.println(myIPAddress);

  Serial.print("Current time: ");
  Serial.print(hour());
  Serial.print(":");
  Serial.print(minute());
  Serial.print(":");
  Serial.print(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print("/");
  Serial.print(month());
  Serial.print("/");
  Serial.print(year());
  Serial.println();
}


void loop() {
  // put your main code here, to run repeatedly:

  // Turn the relay on (assuming HIGH level triggers the relay)
  // Serial.println("Turning ON the relay!");
  // digitalWrite(relayPin, HIGH);
  // delay(5000); // Wait for 30 seconds (30,000 milliseconds)

  // Turn the relay off
  // Serial.println("Turning OFF the relay!");
  // digitalWrite(relayPin, LOW);
  // delay(60000); // Wait for 30 seconds (30,000 milliseconds)

  Alarm.delay(1000);

  int size = 0;
  char buf[512];
  char command[4]; // length of command ("ON", "OFF") + 1 for null terminator
  WiFiClient client = server.available();
  //while (!client) {
    //Serial.println("http server not available");
    //delay(5000);
    //client = server.available();
  //}
  if (client) {
    Serial.println("New client connected");
    String currentLine = "";
    while (client.connected()) {
      buf[0] = '\0';
      command[0] = '\0';
      //Serial.println("New client still connected");
      if (client.available()) {
        Serial.println("New client connected");
        

        // char c = client.read();
        size = client.read(buf, 256);
        Serial.println(buf);

        strncpy(command, buf + 5, 3);
        command[3] = '\0';
        buf[15] = '\0';
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Content-Type: text/html");
        client.println();
        client.println("<!DOCTYPE HTML>");
        client.println("<html>");
        client.println("<head><title>Arduino Sprinkler Control</title></head>");
        client.println("<body>");

        if (size > 0 && ( (strcmp(command, "ONN") == 0) || (strcmp(command, "OFF") == 0) || (strcmp(command, "DIS") == 0) )) {
          Serial.print("client.read() size: ");
          Serial.println(size);
          
          Serial.print("client.read() buf: ");
          Serial.println(buf);

          size_t length = strlen(buf);
          Serial.print("strlen(buf): ");
          Serial.println(length);

          length = strlen(command);
          Serial.print("strlen(command): ");
          Serial.println(length);

          Serial.print("command: ");
          Serial.println(command);
          Serial.println();

          if (strcmp(command, "ONN") == 0) {
                digitalWrite(relayPin, HIGH);
                client.println("<h1>Sprinkler is ON</h1>");
              } else if (strcmp(command, "OFF") == 0) {
                digitalWrite(relayPin, LOW);
                client.println("<h1>Sprinkler is OFF</h1>");
              }
              else if (strcmp(command, "DIS") == 0) {
                client.println("<h1>Sprinkler is DISCONNECTED</h1>");
                //client.stop();
              }

          client.println("</body>");
          client.println("</html>");
            
          // Add a blank line to properly terminate the response
          client.println();

          // Stop the client
          client.stop();
        
        }
        client.println(); // add a blank line to properly terminate the response
      }
    }
    // Serial.println("Client disconnected");
  } else {
    Serial.println("New client NOT connected");
    Serial.println("http client not available");
    delay(5000);
    client = server.available();
    }

     // Print the current time every second for debugging
  Serial.print("Current time: ");
  Serial.print(hour());
  Serial.print(":");
  Serial.print(minute());
  Serial.print(":");
  Serial.print(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print("/");
  Serial.print(month());
  Serial.print("/");
  Serial.print(year());
  Serial.println();
}

// functions to be called when an alarm triggers:
void ScheduledSprinklerOn() {
  Serial.println("ScheduledSprinklerOn");
  digitalWrite(relayPin, HIGH);
  Alarm.timerOnce(durationSeconds, ScheduledSprinklerOff);
}

void ScheduledSprinklerOff() {
  Serial.println("ScheduledSprinklerOff");
  digitalWrite(relayPin, LOW);
  zones = zones - 1;
  Serial.print("zones: ");
  Serial.println(zones);

  if (zones > 0) {
    delay(5000);
    ScheduledSprinklerOn();
  }
}

