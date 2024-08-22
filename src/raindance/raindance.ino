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
  Alarm.alarmRepeat(dowThursday, 16, 19, 1, ScheduledSprinklerOn);
  Alarm.alarmRepeat(5, 0, 0, GetSetCurrentTime);
}

void handleClientRequests() {
  Serial.println("start handleClientRequests()");

  // get a client that is connected to the server and that has data available for reading
  WiFiClient client = server.available();

  // Prepare JSON response
  JSONVar responseObj;

  if (client) {
    Serial.println("New client connected");
    char buf[1024];
    char command[4]; // length of command ("ON", "OFF") + 1 for null terminator
    char httpMethod[4]; //the first four characters containing eother "GET " or "POST"
    enum  : int { ERROR, GET, POST} httpMethodEnum = ERROR;
    int size = 0;
    String currentLine = "";

    while (client.connected()) {
      buf[0] = '\0';
      command[0] = '\0';

      if (client.available()) {
        size = client.read(buf, 512);
        Serial.println("client.read->size of the buffer returned: " + String(size));
        Serial.println("client.read->buf: ");
        Serial.println(buf);
        strncpy(httpMethod, buf + 0, 4);
        httpMethod[4] = '\0';
        Serial.println("httpMethod: " + String(httpMethod));
        if (strcmp(httpMethod, "GET ") == 0) { 
          httpMethodEnum = GET;
          } else if (strcmp(httpMethod, "POST") == 0) {
          httpMethodEnum = POST;
          } else { 
            httpMethodEnum = ERROR;
          }
        switch (httpMethodEnum) {
          case GET:
            Serial.println("switch->GET ");
            strncpy(command, buf + 5, 3);
            command[3] = '\0';
            Serial.println("command: " + String(command));
            buf[15] = '\0';

            if (size > 0 && ((strcmp(command, "ONN") == 0) || (strcmp(command, "OFF") == 0)  || (strcmp(command, "HI!") == 0))) {
              if (strcmp(command, "ONN") == 0) {
                digitalWrite(relayPin, HIGH);
                responseObj["status"] = "Sprinkler is ON";
              } else if (strcmp(command, "OFF") == 0) {
                digitalWrite(relayPin, LOW);
                responseObj["status"] = "Sprinkler is OFF";
              } else if (strcmp(command, "DIS") == 0) {
                responseObj["status"] = "Sprinkler is DISCONNECTED";
              } else if (strcmp(command, "HI!") == 0) {
                String timeStamp = String(year()) + "-" + month() + "-" + day() + "T" + hour() + ":" + minute() + ":" + second();
                Serial.println("timeStamp: " + timeStamp);
                Serial.print("relayState: ");
                Serial.println(relayState);
                String sprinklerStateStr = String(relayState);
                Serial.println("sprinklerStateStr: " + sprinklerStateStr);
                String responseStr = timeStamp + "::" + sprinklerStateStr;
                responseObj["status"] = responseStr;
              }
            } else {
              responseObj["error"] = "Invalid command";
            }
            break;
          case POST:
            Serial.println("switch->POST");
            strncpy(command, buf + 6, 3);
            command[3] = '\0';
            Serial.println("command: " + String(command));
            buf[15] = '\0';
            break;
          default:
            Serial.println("switch->bad httpMethod!");
            break;
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
