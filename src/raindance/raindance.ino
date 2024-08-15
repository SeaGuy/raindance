#include <WiFiNINA.h>
#include <WiFiServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <TimeLib.h>
#include <ArduinoHttpClient.h>
#include <Arduino_JSON.h>

int zones = 3;
int durationSeconds = 10;

// WiFi settings
const char* ssid = "ARRIS-439E";
const char* password = "287860664144";

int status = WL_IDLE_STATUS;
WiFiServer server(80);

const char timeServerAddress[] = "worldtimeapi.org";  // server address
int timePort = 80;
WiFiClient wifiTimeClient;
HttpClient httpTimeClient = HttpClient(wifiTimeClient, timeServerAddress, timePort);

// Define the pin for the relay
const int relayPin = 7;

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
    delay(3000);
  }
  Serial.print("Connected to WiFi with address ");
  Serial.println(WiFi.localIP());
}

void setupAlarms() {
  Alarm.alarmRepeat(dowThursday, 16, 19, 1, ScheduledSprinklerOn);
  Alarm.alarmRepeat(5, 0, 0, GetSetCurrentTime);
}

void handleClientRequests() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected");
    char buf[512];
    char command[4]; // length of command ("ON", "OFF") + 1 for null terminator
    int size = 0;
    String currentLine = "";

    while (client.connected()) {
      buf[0] = '\0';
      command[0] = '\0';

      if (client.available()) {
        size = client.read(buf, 256);
        Serial.println(buf);
        strncpy(command, buf + 5, 3);
        command[3] = '\0';
        buf[15] = '\0';
        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();
        client.println("<!DOCTYPE HTML>");
        client.println("<html>");
        client.println("<head><title>Arduino Sprinkler Control</title></head>");
        client.println("<body>");

        if (size > 0 && ( (strcmp(command, "ONN") == 0) || (strcmp(command, "OFF") == 0) || (strcmp(command, "DIS") == 0) )) {
          if (strcmp(command, "ONN") == 0) {
            digitalWrite(relayPin, HIGH);
            client.println("<h1>Sprinkler is ON</h1>");
          } else if (strcmp(command, "OFF") == 0) {
            digitalWrite(relayPin, LOW);
            client.println("<h1>Sprinkler is OFF</h1>");
          } else if (strcmp(command, "DIS") == 0) {
            client.println("<h1>Sprinkler is DISCONNECTED</h1>");
          }
          client.println("</body>");
          client.println("</html>");
          client.println();
          delay(500);
          client.stop();
        }
      }
    }
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
    delay(5000);
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
  httpTimeClient.get("/api/timezone/America/New_York");
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
    Serial.println("Failed to get time");
  }
}
