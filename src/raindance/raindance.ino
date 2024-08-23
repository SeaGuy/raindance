/*
uint32_t schedule entry bits

bits    description  
======
0-2     day of the week (0=Sunday, 6=Saturday)
3-13    time of day in minutes (1=12:00AM; 1440=11:59PM; 121=2:01AM)
14-15   zones (1-3)
16-22   duration in minutes (1-120)


EEPROM map

address   type        description of value
=======   ====        ====================
0         uint32_t    number of sprinkler schedule entries (0-7)
1         uint32_t    first schedule entry
*/





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
int eepromAddrNumSchedules = 0; // stored at first address
uint32_t aSprinklerScheduleBits = 0x00;
uint32_t sprinklerScheduleBitsArray[7] = {0};

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

// Function prototypes
void handleGetRequest(String command, JSONVar& responseObj);
void handlePostRequest(WiFiClient& client, JSONVar& responseObj);
String readHeaders(WiFiClient& client);
String readJsonBody(WiFiClient& client);
void processScheduleCommand(JSONVar parsedData, JSONVar& responseObj);

void setup() {
  setupSerial();
  setupRelay();
  connectToWiFi();
  server.begin();
  GetSetCurrentTime();
  PrintCurrentTime();
  getScheduleFromEEPROM();
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

void getScheduleFromEEPROM() {
  uint32_t scheduleBits = 0;
  uint32_t numberOfSchedules = readUint32FromEEPROM(eepromAddrNumSchedules);
  Serial.println("numberOfSchedules in EEPROM: " + String(numberOfSchedules));
  if (numberOfSchedules > 7) { numberOfSchedules = 0; }
  Serial.println("numberOfSchedules: " + String(numberOfSchedules));
}

void setupAlarms() {
  Alarm.alarmRepeat(dowWednesday, 6, 0, 1, ScheduledSprinklerOn);
  Alarm.alarmRepeat(dowSaturday, 6, 0, 1, ScheduledSprinklerOn);
  Alarm.alarmRepeat(5, 0, 0, GetSetCurrentTime);
}

void handleClientRequests() {
  Serial.println("start handleClientRequests()");

  WiFiClient client = server.available();

  if (client) {
    Serial.println("New client connected");
    String requestData = client.readStringUntil('\n');
    requestData.trim();  // Remove any extra whitespace

    Serial.println("Received: " + requestData);

    JSONVar responseObj;

    if (requestData.startsWith("GET")) {
      String command = requestData.substring(5, 8);
      handleGetRequest(command, responseObj);
    } else if (requestData.startsWith("POST")) {
      String headers = readHeaders(client);
      handlePostRequest(client, responseObj);
    }

    // Send the response
    String jsonResponse = JSON.stringify(responseObj);
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonResponse.length());
    client.println("Connection: close");
    client.println();
    client.print(jsonResponse);

    delay(1024);
    client.stop();
  } else {
    Serial.println("No client connected");
    delay(5000);
  }
}

void handleGetRequest(String command, JSONVar& responseObj) {
  Serial.println("Processing GET request with command: " + command);
  
  if (command == "ONN") {
    digitalWrite(relayPin, HIGH);
    responseObj["status"] = "Sprinkler is ON";
  } else if (command == "OFF") {
    digitalWrite(relayPin, LOW);
    responseObj["status"] = "Sprinkler is OFF";
  } else if (command == "HI!") {
    String timeStamp = String(year()) + "-" + month() + "-" + day() + "T" + hour() + ":" + minute() + ":" + second();
    String sprinklerStateStr = String(digitalRead(relayPin));
    responseObj["status"] = timeStamp + "::" + sprinklerStateStr;
  } else {
    responseObj["error"] = "Invalid command";
  }
}

void handlePostRequest(WiFiClient& client, JSONVar& responseObj) {
  Serial.println("Processing POST request");

  String jsonString = readJsonBody(client);
  Serial.println("Extracted JSON: " + jsonString);

  JSONVar parsedData = JSON.parse(jsonString);

  if (JSON.typeof(parsedData) == "undefined") {
    Serial.println("Parsing input failed!");
    responseObj["error"] = "Failed to parse JSON";
  } else {
    processScheduleCommand(parsedData, responseObj);
  }
}

String readHeaders(WiFiClient& client) {
  String headers = "";
  while (client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      break;  // Headers are done
    }
    headers += line + "\n";
  }
  Serial.println("Headers: " + headers);
  return headers;
}

String readJsonBody(WiFiClient& client) {
  String jsonBody = "";
  while (client.available()) {
    jsonBody += client.readString();
  }
  return jsonBody;
}

void processScheduleCommand(JSONVar parsedData, JSONVar& responseObj) {
  Serial.println("Processing schedule command");
  
  int numberOfZones = (int)parsedData["numberOfZones"];
  int duration = (int)parsedData["duration"];
  uint32_t aSprinklerScheduleBits = 0x00;

  JSONVar scheduleArray = parsedData["schedule"];

  Serial.println("Received Schedule Parameters:");
  Serial.println("Number of Zones: " + String(numberOfZones));
  Serial.println("Duration per Zone: " + String(duration) + " minutes");

  for (int i = 0; i < scheduleArray.length(); i++) {
    int dayOfWeek = (uint32_t)scheduleArray[i]["dayOfWeek"];
    double time = (double)scheduleArray[i]["time"];  // time since 1970

    Serial.println("Schedule Entry " + String(i + 1) + ": Day " + String(dayOfWeek) + ", Time: " + String(time));
    aSprinklerScheduleBits = aSprinklerScheduleBits | dayOfWeek;
    Serial.println("aSprinklerScheduleBits: " + String(aSprinklerScheduleBits));

  }

  responseObj["status"] = "Schedule updated";
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

void storeUint32ToEEPROM(int address, uint32_t value) {
    // Store each byte of the uint32_t value into EEPROM
    EEPROM.write(address, (value >> 24) & 0xFF);        // Most significant byte
    EEPROM.write(address + 1, (value >> 16) & 0xFF);
    EEPROM.write(address + 2, (value >> 8) & 0xFF);
    EEPROM.write(address + 3, value & 0xFF);            // Least significant byte
}

uint32_t readUint32FromEEPROM(int address) {
    // Read each byte from the EEPROM and reconstruct the uint32_t value
    uint32_t value = 0;
    value |= ((uint32_t)EEPROM.read(address)) << 24;
    value |= ((uint32_t)EEPROM.read(address + 1)) << 16;
    value |= ((uint32_t)EEPROM.read(address + 2)) << 8;
    value |= (uint32_t)EEPROM.read(address + 3);
    
    return value;
}
