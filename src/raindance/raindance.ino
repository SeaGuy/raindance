#include <SPI.h>
#include <WiFiNINA.h>
//#include <WiFiServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <TimeLib.h>
#include <ArduinoHttpClient.h>
#include <Arduino_JSON.h>

int zones = 3;
int durationSeconds = 10;
int interZoneDelay = 10000;

// WiFi settings
const char ssid[] = "ARRIS-439E";
const char password[] = "287860664144";

int status = WL_IDLE_STATUS;
WiFiServer server(80);

const char timeServerAddress[] = "worldtimeapi.org";  // time server address
const char timeServerAPI[] = "/api/timezone/America/New_York"; // API at time server
int timePort = 80;
WiFiClient wifiTimeClient;
HttpClient httpTimeClient = HttpClient(wifiTimeClient, timeServerAddress, timePort);

// Define the pin for the relay
const int relayPin = 7;
int relayState = 0;

void setup() {
  setupSerial();
  setupRelay();
  connectToWiFi();
  server.begin();
  GetSetCurrentTime();
  PrintCurrentTime();
  setupAlarms();
}

void loop() {
  Alarm.delay(1000); // needed to activate alarms
  handleClientRequests();
  PrintCurrentTime();

  // Read the state of the relay pin
  relayState = digitalRead(relayPin);
  Serial.print("relayState: ");
  Serial.println(relayState);


  if (relayState == HIGH) {
    Serial.println("relay is ON");
  } else {
    Serial.println("relay is OFF");
  }
}

void setupSerial() {
  Serial.begin(9600);
  delay(500);
  while (!Serial) {
    Serial.println("Setting up serial port");
    delay(500);
  }
  Serial.println("Serial port connected");
}

void setupRelay() {
  pinMode(relayPin, OUTPUT);
  Serial.println("Configuring digital output pin for relay");
  digitalWrite(relayPin, LOW);
}

void connectToWiFi() {
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, password);
    delay(1000);
  }
  Serial.print("Connected to WiFi with address ");
  Serial.println(WiFi.localIP());
  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void setupAlarms() {
  Alarm.alarmRepeat(dowWednesday, 6, 0, 1, ScheduledSprinklerOn);
  Alarm.alarmRepeat(dowSaturday, 6, 0, 1, ScheduledSprinklerOn);
  Alarm.alarmRepeat(5, 0, 0, GetSetCurrentTime);
}

void handleClientRequests() {
  Serial.println("start handleClientRequests()");

  WiFiClient client = server.available();

  JSONVar responseObj;

  if (client) {
    Serial.println("New client connected");
    String requestData = "";

    while (client.connected()) {
      if (client.available()) {
        requestData = client.readStringUntil('\n');  // Read the first line of the request
        requestData.trim();  // Remove any extra whitespace

        Serial.println("Received: " + requestData);

        // Determine if this is a GET or POST request
        if (requestData.startsWith("GET")) {
          Serial.println("Processing GET request");

          // Extract the command from the GET request
          String command = requestData.substring(5, 8);
          Serial.println("Command: " + command);

          if (command == "ONN") {
            digitalWrite(relayPin, HIGH);
            responseObj["status"] = "Sprinkler is ON";
          } else if (command == "OFF") {
            digitalWrite(relayPin, LOW);
            responseObj["status"] = "Sprinkler is OFF";
          } else if (command == "HI!") {
            String timeStamp = String(year()) + "-" + month() + "-" + day() + "T" + hour() + ":" + minute() + ":" + second();
            String sprinklerStateStr = String(digitalRead(relayPin));
            String responseStr = timeStamp + "::" + sprinklerStateStr;
            responseObj["status"] = responseStr;
          } else {
            responseObj["error"] = "Invalid command";
          }

          break;  // We have processed the GET request, so break out of the loop

        } else if (requestData.startsWith("POST")) {
          Serial.println("Processing POST request");

          // Continue reading the remaining headers
          while (client.available()) {
            String headerLine = client.readStringUntil('\n');
            headerLine.trim();
            if (headerLine.length() == 0) {
              break;  // Headers are done, now we expect the body
            }
          }

          // Read the entire JSON body
          String jsonString = "";
          while (client.available()) {
            jsonString += client.readString();
          }

          Serial.println("Extracted JSON: " + jsonString);

          JSONVar parsedData = JSON.parse(jsonString);

          if (JSON.typeof(parsedData) == "undefined") {
            Serial.println("Parsing input failed!");
            responseObj["error"] = "Failed to parse JSON";
          } else {
            String command = "SCH";  // We expect the command in the body for POST

            if (command == "SCH") {
              // Extract the parameters
              int numberOfZones = (int)parsedData["numberOfZones"];
              int duration = (int)parsedData["duration"];
              JSONVar scheduleArray = parsedData["schedule"];
              
              // Log the received parameters
              Serial.println("Received Schedule Parameters:");
              Serial.println("Number of Zones: " + String(numberOfZones));
              Serial.println("Duration per Zone: " + String(duration) + " minutes");

              for (int i = 0; i < scheduleArray.length(); i++) {
                int dayOfWeek = (int)scheduleArray[i]["dayOfWeek"];
                double time = (double)scheduleArray[i]["time"];  // time since 1970

                Serial.println("Schedule Entry " + String(i + 1) + ": Day " + String(dayOfWeek) + ", Time: " + String(time));
              }

              responseObj["status"] = "Schedule updated";
            } else {
              responseObj["error"] = "Invalid POST command";
            }
          }

          break;  // We have processed the POST request, so break out of the loop
        }
      }
    }

    // Convert JSON object to string
    String jsonResponse = JSON.stringify(responseObj);
    
    // Send HTTP response headers
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonResponse.length());
    client.println("Connection: close");
    client.println();

    // Send the response body
    client.print(jsonResponse);
    delay(1024);
    client.stop();
  } else {
    Serial.println("No client connected");
    delay(5000);
  }
}

void ScheduledSprinklerOn() {
  Serial.println("ScheduledSprinklerOn");
  digitalWrite(relayPin, HIGH);
  Alarm.timerOnce(durationSeconds, ScheduledSprinklerOff);
}

void ScheduledSprinklerOff() {
  Serial.println("ScheduledSprinklerOff");
  digitalWrite(relayPin, LOW);
  zones = zones - 1;
  if (zones > 0) {
    delay(interZoneDelay);
    ScheduledSprinklerOn();
  }
}

void PrintCurrentTime() {
  if (year() != 1970) {
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
  } else {
    Serial.println("Time is NOT Set!");
  }
}

void GetSetCurrentTime() {
  Serial.println("Getting and setting current time");
  httpTimeClient.get(timeServerAPI);
  int statusCode = httpTimeClient.responseStatusCode();
  String response = httpTimeClient.responseBody();

  if (statusCode == 200) {
    JSONVar myObject = JSON.parse(response);
    if (JSON.typeof(myObject) == "undefined") {
      Serial.println("Parsing input failed!");
      return;
    }
    String datetime = (const char*) myObject["datetime"];
    int hr = (datetime.substring(11,13)).toInt();
    int min = (datetime.substring(14,16)).toInt();
    int sec = (datetime.substring(17,19)).toInt();
    int day = (datetime.substring(8,10)).toInt();
    int month = (datetime.substring(5,7)).toInt();
    int year = (datetime.substring(0,4)).toInt();
    setTime(hr, min, sec, day, month, year);
  } else {
    Serial.println("Failed to get time; trying again in 60 seconds");
    Alarm.timerOnce(60, GetSetCurrentTime);
  }
}
