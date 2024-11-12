/*
=========
bitfields
=========

bitfield 0
zones: number of zones (1-4)
uint8_t 

bitfield 1
duration: duration in minutes (1-120)
uint8_t 

bitfield 2
number of schedules (1-7)

bitfields 3-9
sprinkler time schedule bitfield
uint16_t sprinklerTimeScheduleBitfield
max value 13 bits = 8191D = 0x1fff
max application value: 10110100000 (1440) = 11:59 PM +  111 (6) = Saturday => 10110100000111 (11527D = 0x2d07)
So, examples:
  Wednesdays at 6:00 AM =  360 + 3 => 101101000 + 011 =>  101101000011 (2883 = 0x0b43)
  Wednesdays at 6:30 AM =  390 + 3 => 110000110 + 011 =>  110000110011 (3123 = 0x0c33)

bits    description  
---     -----------
0-2     day of the week (0=Sunday, 6=Saturday)
3-13    time of day in minutes (1=12:00AM; 1440=11:59PM; 121=2:01AM)


==========
EEPROM map
==========

address   type        description of value
-------   ----        --------------------
0         uint8_t     zones: number of zones (1-4)
1         uint8_t     duration: number of minutes per zone (1-120)
2         uint8_t     number of sprinkler schedules (1-7)
3         uint16_t    most significant byte (MSB) of first schedule entry
4         uint16_t    least significant byte (LSB) of first schedule entry
5         uint16_t    most significant byte (MSB) of second schedule entry
6         uint16_t    least significant byte (LSB) of second schedule entry
*/


// Include necessary libraries
#include <WiFi.h>
#include <EEPROM.h>
#include <Wire.h>
#define dtNBR_ALARMS 16
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <ArduinoHttpClient.h>
#include <Arduino_JSON.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

// #define DEBUG // *********** TURN ON/OFF SERIAL LOGGING

// EEPROM settings
#define EEPROM_SIZE 128

// Define EEPROM addresses
#define EEPROM_ADDR_NUM_ZONES 0
#define EEPROM_ADDR_NUM_MINUTES 1
#define EEPROM_ADDR_NUM_SCHEDS 2
#define EEPROM_ADDR_FIRST_SCHED 3

// Define constants
#define MAX_NUM_SCHEDS 2
#define MAX_NUM_ZONES 4
#define MAX_DURATION_PER_ZONE 120
#define INTER_ZONE_DELAY_SECONDS 30           // 30-second pause to let the manifold reset to the next zone

#define EEPROM_MAX_ADDRESS (EEPROM_ADDR_FIRST_SCHED + (2 * (MAX_NUM_SCHEDS - 1)) + 1)

#define LED_SHORT_BURST_MILLISECONDS  256
#define APP_RED_DELAY 256
#define APP_ORN_DELAY 512
#define APP_YEL_DELAY 1024
#define APP_GRN_DELAY 2048
#define APP_BLU_DELAY 3072
#define APP_IND_DELAY 4096


// Global variables
AlarmID_t schedAlarmID;                         // Variable to store the alarm ID
AlarmID_t schedAlarmIDArray[MAX_NUM_SCHEDS];
AlarmID_t getSetCurrentTimeAlarmID; 
AlarmID_t onAlarmID;
AlarmID_t offAlarmID;
AlarmID_t retryGetTimeAlarmID;

int eepromAddrNumSchedules = 0; // stored at first address
uint16_t sprinklerTimeScheduleBitfield = 0x00;

// Step 1: Define the structure
struct TimeSchedule {
    uint8_t dayOfTheWeek;  // 0-6 for Sunday-Saturday
    uint8_t hour;          // 0-23 for hours of the day
    uint8_t minute;        // 0-59 for minutes of the hour
};

struct SprinklerSchedule {
  uint8_t zones;          // 1-EEPROM_MAX_NUM_ZONES
  uint8_t durationMinutes;
  uint8_t numberOfTimeSchedules;
  TimeSchedule myTimeSchedule[MAX_NUM_SCHEDS];
};

// create a default schedule
SprinklerSchedule mySprinklerSchedule = {
  3,
  30,
  2,
  {
    { 3, 6, 0 },  // Wednesday at 6:00 AM
    { 6, 6, 0 }   // Saturday at 6:00 AM
  }
};

int zones = mySprinklerSchedule.zones; // initialize to default number of zones

// Function prototypes
void setup();
void loop();
void setupSerial();
void setupRelay();
void connectToWiFi();
void getScheduleFromEEPROM();
void writeScheduleToEEPROM();
void setupAlarms();
void handleClientRequests();
void handleGetRequest(String command, JSONVar& responseObj);
void handlePostRequest(WiFiClient& client, JSONVar& responseObj);
String readHeaders(WiFiClient& client);
String readJsonBody(WiFiClient& client);
bool processScheduleCommand(JSONVar parsedData, JSONVar& responseObj);
void ScheduledSprinklerOn();
void ScheduledSprinklerOff();
void reportRelayState();
bool PrintCurrentTime();
void PrintSprinklerSchedule(String scheduleName, SprinklerSchedule theSchedule);
void PrintSprinklerTimeSchedule(SprinklerSchedule aSchedule, int numSchedules);
void GetSetCurrentTime();
uint16_t readUint16FromEEPROM(int address);
void eepromDump(int address);
bool validateSchedule(SprinklerSchedule aSprinklerSchedule);
uint16_t createSprinklerTimeScheduleBitfield(TimeSchedule myTimeSchedule);
void writeUint16ToEEPROM(int address, uint16_t value);
bool timeScheduleValidated(uint8_t numScheds, TimeSchedule theTimeSchedule[]);
void clearAlarms();
void deepCopySprinklerSchedule(SprinklerSchedule &source, SprinklerSchedule &destination);
void clearEEPROM();
void setDefaultSchedule();
void parse_worldtimeapi(JSONVar myObject);
void parse_worldclockapi(JSONVar myObject);
void parse_timeapi(JSONVar myObject);
timeDayOfWeek_t convertInt2DOW(int value);
void checkCLI();
void memCheck();
bool validateTime();
void getCurrentTimestamp();
void statusCheck();
uint8_t generateStatusWord();

// WiFi settings
const char ssid[] = "ARRIS-439E";
const char password[] = "287860664144";

WiFiServer server(80);

struct TimeServerData {
  const char* url;        // time server URL
  const int port;         // port number
  const char* api;        // time server API
  void (*function)(JSONVar);     // function pointer to handler for that specific time server
};

TimeServerData myTimeServerArray[] = {
  { "worldtimeapi.org",   80,   "/api/timezone/America/New_York",                     parse_worldtimeapi },           // 213.188.196.246
  { "worldclockapi.com",  80,   "/api/json/est/now",                                  parse_worldclockapi },
  { "timeapi.io",         80,   "/api/time/current/zone?timeZone=America/New_York",   parse_timeapi }                 // 86.105.246.247
};

const int NUMBER_TIME_SERVERS = sizeof(myTimeServerArray) / sizeof(myTimeServerArray[0]);

int timePort = 80;
WiFiClient wifiTimeClient;

// pin assignments
const int relayPin = 7;   // pin for the relay (D7)

const int ___red_led_pin = 14;  // pin for red LED problem patterns
const int _green_led_pin = 15;  // pin for green LED indicates heartbeat sent
const int __blue_led_pin = 16;  // pin for blue LED indicates sprinkler on water flowing

char hiTimeStamp[25];

// status variables for iPhone reporting
uint8_t isScheduleInvalid = (uint8_t)1;
uint8_t isHeapMemLow = (uint8_t)1;
uint8_t isTimeStampNotSet = (uint8_t)1;
uint8_t isRSSIWeak = (uint8_t)1;
uint8_t arduinoCommandError = (uint8_t)1;

void setup() {
  #ifdef DEBUG
    setupSerial();
  #endif
  delay(APP_GRN_DELAY);
  setupRelay();
  delay(APP_GRN_DELAY);
  connectToWiFi();
  delay(APP_GRN_DELAY);
  // Set timeout for HTTP requests
  wifiTimeClient.setTimeout(5000);  // Set timeout to 5 seconds (5000 ms)
  // Initialize EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) {
    #ifdef DEBUG
      Serial.println("Failed to initialise EEPROM");
    #endif
    return;
  }
  delay(APP_GRN_DELAY);
  server.begin();
  delay(APP_GRN_DELAY);
  getCurrentTimestamp();
  delay(APP_GRN_DELAY);
  eepromDump(EEPROM_MAX_ADDRESS);
  delay(APP_GRN_DELAY);
  getScheduleFromEEPROM();
  if (validateSchedule(mySprinklerSchedule)) {
    #ifdef DEBUG
      Serial.println("setup->validateSchedule->schedule is valid ...");
    #endif
    isScheduleInvalid = 0;
  } else {
      #ifdef DEBUG
        Serial.println("setup->validateSchedule->schedule is not valid ...");
      #endif
      isScheduleInvalid = 1;
  };
  PrintSprinklerSchedule("mySprinklerSchedule", mySprinklerSchedule);
  delay(APP_GRN_DELAY);
  setupAlarms();
  // set the LED pins as  outputs
  pinMode(__blue_led_pin, OUTPUT);
  pinMode(___red_led_pin, OUTPUT);
  pinMode(_green_led_pin, OUTPUT);
  // pulse each LED 3 times to verify working on startup
  pulseLED(__blue_led_pin, 3, LED_SHORT_BURST_MILLISECONDS);
  pulseLED(___red_led_pin, 3, LED_SHORT_BURST_MILLISECONDS);
  pulseLED(_green_led_pin, 3, LED_SHORT_BURST_MILLISECONDS);
  delay(APP_GRN_DELAY);
}

void loop() {
  Alarm.delay(1000); // needed to activate alarms
  handleClientRequests();
  if (validateTime()) { PrintCurrentTime(); } else { getCurrentTimestamp(); }
  reportRelayState();
  PrintSprinklerSchedule("mySprinklerSchedule", mySprinklerSchedule);
  checkCLI();
  memCheck();
  statusCheck();
  delay(APP_YEL_DELAY);
}

void setupSerial() {
  Serial.begin(115200);
  delay(1024);
  while (!Serial) {
    Serial.println("Setting up serial port");
    delay(1024);
  }
  Serial.println("Serial port connected");
}

void setupRelay() {
  pinMode(relayPin, OUTPUT);
  #ifdef DEBUG
    Serial.println("Configuring digital output pin for relay");
  #endif
  digitalWrite(relayPin, LOW);
}

void connectToWiFi() {
  #ifdef DEBUG
    Serial.print("Connecting to SSID: ");
    Serial.println(ssid);
  #endif
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    #ifdef DEBUG
      Serial.print(".");
    #endif
    delay(1000);
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    #ifdef DEBUG
      Serial.println();
      Serial.print("Connected to WiFi with address ");
      Serial.println(WiFi.localIP());
    #endif
    // print the received signal strength:
    long rssi = WiFi.RSSI();
    isRSSIWeak = (rssi <= (long)-80) ? (uint8_t)1 : (uint8_t)0;
    #ifdef DEBUG
      Serial.print("signal strength (RSSI):");
      Serial.print(rssi);
      Serial.println(" dBm");
    #endif
  } else {
    #ifdef DEBUG
      Serial.println();
      Serial.println("Failed to connect to WiFi");
    #endif
  }
}

void getScheduleFromEEPROM() {
  bool success = false;
  uint16_t value = 0x0000;
  uint8_t numZones =    (uint8_t)EEPROM.read(EEPROM_ADDR_NUM_ZONES);
  uint8_t numMinutes =  (uint8_t)EEPROM.read(EEPROM_ADDR_NUM_MINUTES);
  uint8_t numScheds =   (uint8_t)EEPROM.read(EEPROM_ADDR_NUM_SCHEDS);
  SprinklerSchedule eepromSchedule;
  eepromSchedule.zones = numZones;
  eepromSchedule.durationMinutes = numMinutes;
  eepromSchedule.numberOfTimeSchedules = numScheds;
  if (numScheds >= 1) {
    for (int i = 0; i <= ((int)numScheds - 1); i++) { 
        value = readUint16FromEEPROM(EEPROM_ADDR_FIRST_SCHED + (2 * i));
        eepromSchedule.myTimeSchedule[i].dayOfTheWeek = (uint8_t)(value & 0x0007);
        uint16_t hourMinBits = (uint16_t)((value >> 3) & 0x07FF);
        eepromSchedule.myTimeSchedule[i].hour = (uint8_t)(hourMinBits / 60); // divide by 60 minutes;
        eepromSchedule.myTimeSchedule[i].minute = (uint8_t)(hourMinBits % 60); // modulus after divide by 60 minutes
    }
  }
  PrintSprinklerSchedule("eepromSchedule", eepromSchedule);
  deepCopySprinklerSchedule(eepromSchedule, mySprinklerSchedule);
}

void writeScheduleToEEPROM() {
  #ifdef DEBUG
    Serial.println("writeScheduleToEEPROM");
  #endif
  PrintSprinklerSchedule("mySprinklerSchedule", mySprinklerSchedule);  
  EEPROM.write(EEPROM_ADDR_NUM_ZONES, mySprinklerSchedule.zones & 0xFF);
  EEPROM.write(EEPROM_ADDR_NUM_MINUTES, mySprinklerSchedule.durationMinutes & 0xFF);
  EEPROM.write(EEPROM_ADDR_NUM_SCHEDS, mySprinklerSchedule.numberOfTimeSchedules & 0xFF);
  for (int i = 0; i < mySprinklerSchedule.numberOfTimeSchedules; i++) {
    TimeSchedule myTimeSchedule = mySprinklerSchedule.myTimeSchedule[i];
    uint16_t myBitField = createSprinklerTimeScheduleBitfield(myTimeSchedule);
    #ifdef DEBUG
      Serial.println("writeScheduleToEEPROM->mySprinklerSchedule.myTimeSchedule[" + String(i) + "]->myBitField: " + String(myBitField));
    #endif
    writeUint16ToEEPROM(EEPROM_ADDR_FIRST_SCHED + (i * 2), myBitField);
  }
  EEPROM.commit(); // Commit changes to EEPROM
}

void setupAlarms() {
  #ifdef DEBUG
    Serial.println("setupAlarms()");
  #endif
  // set alarms based on mySprinklerSchedule.myTimeSchedule
  // timeDayOfWeek_t is an enum in TimeLib.h {undefined=0, 1=Sunday, 2=Monday, etc.}
  int numScheds = mySprinklerSchedule.numberOfTimeSchedules;
  int myDayOfTheWeek = -1;
  zones = (int)(mySprinklerSchedule.zones);
  clearAlarms();
  for (int i = 0; i < numScheds; i++) {
    int dayOfTheWeek = (int)mySprinklerSchedule.myTimeSchedule[i].dayOfTheWeek;
    int hour = (int)mySprinklerSchedule.myTimeSchedule[i].hour;
    int minute = (int)mySprinklerSchedule.myTimeSchedule[i].minute;
    timeDayOfWeek_t dowEnum = convertInt2DOW(mySprinklerSchedule.myTimeSchedule[i].dayOfTheWeek);
    schedAlarmID = Alarm.alarmRepeat(dowEnum, mySprinklerSchedule.myTimeSchedule[i].hour, mySprinklerSchedule.myTimeSchedule[i].minute, 0, ScheduledSprinklerOn);
    schedAlarmIDArray[i] = schedAlarmID;
    }
    getSetCurrentTimeAlarmID = Alarm.alarmRepeat(5, 0, 0, GetSetCurrentTime); // 5:00 AM every day
  }

void handleClientRequests() {
  #ifdef DEBUG
    Serial.println("handleClientRequests");
  #endif
  WiFiClient client = server.available();
  if (client) {
    #ifdef DEBUG
      Serial.println("New client connected");
    #endif
    String requestData = client.readStringUntil('\n');
    requestData.trim();  // Remove any extra whitespace
    #ifdef DEBUG
      Serial.println("Received: " + requestData);
    #endif
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
    #ifdef DEBUG
      Serial.println("No WiFi client connected");
    #endif
    delay(1024);
  }
}

void handleGetRequest(String command, JSONVar& responseObj) {
  #ifdef DEBUG
    Serial.println("handleGetRequest->Processing GET request with command: " + command);
  #endif
  char outputBuffer[128];
  pulseLED(_green_led_pin, 1, LED_SHORT_BURST_MILLISECONDS);
  arduinoCommandError = 0;
  if (command == "ONN") {
    digitalWrite(relayPin, HIGH);
  } else if (command == "OFF") {
      digitalWrite(relayPin, LOW);
  } else if (command == "HI!") {
      // do nothing but prepare a heartbeat response
  } else {
      // responseObj["error"] = "Invalid command";
      arduinoCommandError = 1;
  }
    uint8_t sprinklerStateInt = digitalRead(relayPin);
    uint8_t daysoftheweek = 0x00;
    for (int i = 0; i < mySprinklerSchedule.numberOfTimeSchedules; i++) {
      // replace with 2 to the power of n
      switch (mySprinklerSchedule.myTimeSchedule[i].dayOfTheWeek) {
        case 0:
          daysoftheweek |= 0x01;
          break;
        case 1:
          daysoftheweek |= 0x02;
          break;
        case 2:
          daysoftheweek |= 0x04;
          break;
        case 3:
          daysoftheweek |= 0x08;
          break;
        case 4:
          daysoftheweek |= 0x10;
          break;
        case 5:
          daysoftheweek |= 0x20;
          break;
        case 6:
          daysoftheweek |= 0x40;
          break;
        default:
          break;
      }
    }
      sprinklerStateInt = (sprinklerStateInt<<7) | daysoftheweek;
      uint8_t myStatus = generateStatusWord();
      #ifdef DEBUG
        Serial.println("myStatus: " + String(myStatus));
      #endif
      sprintf(hiTimeStamp, "%04d-%02d-%02dT%02d:%02d:%02d::%03d::%03d", year(), month(), day(), hour(), minute(), second(), sprinklerStateInt, myStatus);
      sprintf(outputBuffer, "hiTimeStamp: %s", hiTimeStamp);
      #ifdef DEBUG
        Serial.println(outputBuffer);
      #endif
      responseObj["status"] = hiTimeStamp;
}

void handlePostRequest(WiFiClient& client, JSONVar& responseObj) {
  #ifdef DEBUG
    Serial.println("handlePostRequest()");
  #endif
  String jsonString = readJsonBody(client);
  #ifdef DEBUG
    Serial.println("handlePostRequest->Extracted JSON: " + jsonString);
  #endif
  JSONVar parsedData = JSON.parse(jsonString);
  if (JSON.typeof(parsedData) == "undefined") {
    Serial.println("handlePostRequest->Parsing input failed!");
    responseObj["error"] = "handlePostRequest->Failed to parse JSON";
  } else if (processScheduleCommand(parsedData, responseObj)) {
      Serial.println("handlePostRequest->JSON was parsed!");
      responseObj["error"] = "handlePostRequest->JSON was parsed";
      getScheduleFromEEPROM();
      PrintSprinklerSchedule("mySprinklerSchedule", mySprinklerSchedule);
  } else {
      Serial.println("handlePostRequest->processScheduleCommand() failed!");
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

bool processScheduleCommand(JSONVar parsedData, JSONVar& responseObj) {
  bool success = false;
  SprinklerSchedule aSprinklerSchedule;
  Serial.println("processScheduleCommand()");
  JSONVar scheduleArray = parsedData["schedule"];
  uint8_t numberOfZones = (uint8_t)parsedData["numberOfZones"];
  uint8_t duration = (uint8_t)parsedData["duration"];
  uint8_t numberOfTimeSchedules = (uint8_t)scheduleArray.length();
  Serial.println("processScheduleCommand->Received Schedule Parameters:");
  Serial.println("\tprocessScheduleCommand->Number of zones: " + String(numberOfZones) + " zones");
  Serial.println("\tprocessScheduleCommand->Duration per zone: " + String(duration) + " minutes");
  Serial.println("\tprocessScheduleCommand->Number of time schedules: " + String(numberOfTimeSchedules) + " time schedules");
  for (int i = 0; i < scheduleArray.length(); i++) {
    int dayOfWeek = (uint32_t)scheduleArray[i]["dayOfWeek"];
    String time = scheduleArray[i]["time"];
    Serial.println("processScheduleCommand->Schedule Entry " + String(i + 1) + ": Day " + String(dayOfWeek) + ", Time: " + time);
  }
  char timeString[6] = "";
  aSprinklerSchedule.zones = numberOfZones;
  aSprinklerSchedule.durationMinutes = duration;
  aSprinklerSchedule.numberOfTimeSchedules = numberOfTimeSchedules;
  for (int i = 0; i < scheduleArray.length(); i++) {
    int dayOfWeek = (int)scheduleArray[i]["dayOfWeek"];
    String timeValue = (const char*) scheduleArray[i]["time"];
    Serial.println("processScheduleCommand->timeValue: " + timeValue);
    strcpy(timeString, timeValue.c_str());  // time "HH:mm"
    Serial.print("processScheduleCommand->timeString: ");
    Serial.println(timeString);
    String hourString = timeValue.substring(0, 2);
    String minuteString = timeValue.substring(3, 5);
    aSprinklerSchedule.myTimeSchedule[i].dayOfTheWeek = (uint8_t)dayOfWeek;
    aSprinklerSchedule.myTimeSchedule[i].hour = (uint8_t)hourString.toInt();
    aSprinklerSchedule.myTimeSchedule[i].minute =  (uint8_t)minuteString.toInt();
    Serial.println("processScheduleCommand->Schedule Entry " + String(i + 1) + ": Day " + String(dayOfWeek) + ", timeString: " + timeString);
  }
  success = validateSchedule(aSprinklerSchedule);
  responseObj["status"] = success ? "Schedule updated" : "Schedule NOT updated";
  if (success) {
    Serial.println("processScheduleCommand->schedule validated");
    deepCopySprinklerSchedule(aSprinklerSchedule, mySprinklerSchedule);
    Serial.println("processScheduleCommand->schedule deep copied");
    writeScheduleToEEPROM();
    Serial.println("processScheduleCommand->schedule written to EEPROM");
    PrintSprinklerSchedule("mySprinklerSchedule", mySprinklerSchedule);
    Serial.println("processScheduleCommand->ensure sprinkler off");
    ScheduledSprinklerOff();
    Serial.println("processScheduleCommand->setting new alarms");
    setupAlarms();
  } else {
      Serial.println("processScheduleCommand->schedule NOT updated");
      PrintSprinklerSchedule("mySprinklerSchedule", mySprinklerSchedule);
  }
  return success;
}

void ScheduledSprinklerOn() {
  int duration = 0;
  digitalWrite(relayPin, HIGH);
  duration = (int)(mySprinklerSchedule.durationMinutes * 60);
  Serial.println("ScheduledSprinklerOn->duration: " + String(duration) + " seconds;  relay pin: " + String(relayPin));
  onAlarmID = Alarm.timerOnce(duration, ScheduledSprinklerOff);
}

void ScheduledSprinklerOff() {
  Serial.println("ScheduledSprinklerOff->zones: " + String(zones));
  digitalWrite(relayPin, LOW);
  zones = zones - 1;
  if (zones > 0) {
    offAlarmID = Alarm.timerOnce(INTER_ZONE_DELAY_SECONDS, ScheduledSprinklerOn);
  } else {
    zones = (int)(mySprinklerSchedule.zones);
  }
}

bool PrintCurrentTime() {
  bool isTimeSet = false;
  if (validateTime()) {
    char timestamp[20]; // Array to hold the formatted timestamp
    int y = year();
    int m = month();
    int d = day();
    int h = hour();
    int min = minute();
    int s = second();
    // Create a timestamp in the format "YYYY-MM-DD HH:MM:SS"
    #ifdef DEBUG
      sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02d", y, m, d, h, min, s);
      Serial.print("Current time: ");
      Serial.println(timestamp);
    #endif
    isTimeSet = true;
  } else {
      #ifdef DEBUG
        Serial.println("Current time: TIME IS NOT SET!");
      #endif
  }
  return isTimeSet;
}

void PrintSprinklerSchedule(String scheduleName, SprinklerSchedule theSchedule) {
  char params[256];
  sprintf(params, "PrintSprinklerSchedule()->scheduleName: <%s>::zones: <%d>::durationMinutes: <%d>::numberOfTimeSchedules: <%d>", scheduleName, theSchedule.zones, theSchedule.durationMinutes, theSchedule.numberOfTimeSchedules);
  #ifdef DEBUG
    Serial.println(params);
  #endif
      
  PrintSprinklerTimeSchedule(theSchedule, theSchedule.numberOfTimeSchedules);
}

void PrintSprinklerTimeSchedule(SprinklerSchedule aSchedule, int numSchedules) {
  #ifdef DEBUG
    Serial.println("PrintSprinklerTimeSchedule()->numSchedules: " + String(numSchedules));
    char theTime[6];
    for (int i = 0; i < numSchedules; i++) {
        Serial.print("\tdayOfTheWeek: ");
        Serial.print(aSchedule.myTimeSchedule[i].dayOfTheWeek);
        Serial.print(", Time: ");
        sprintf(theTime, "%02d:%02d", aSchedule.myTimeSchedule[i].hour, aSchedule.myTimeSchedule[i].minute);
        Serial.print(theTime);
        Serial.println();
    }
  #endif
}

void GetSetCurrentTime() {
  Serial.println("GetSetCurrentTime()");
  /*
  httpTimeClient.get(timeServerAPI);
  int statusCode = httpTimeClient.responseStatusCode();
  String response = httpTimeClient.responseBody();
  */
  int statusCode = 0;
  int retries = NUMBER_TIME_SERVERS;
  String response, datetime;

  while (retries > 0 && statusCode != 200) {
    Serial.println("Trying to get date and time ...");
    HttpClient httpTimeClient = HttpClient(wifiTimeClient, myTimeServerArray[retries-1].url, myTimeServerArray[retries-1].port);
    httpTimeClient.setHttpResponseTimeout((uint32_t) 4096);
    Serial.print("url: ");
    Serial.println(myTimeServerArray[retries-1].url);
    Serial.print("port: ");
    Serial.println(String(myTimeServerArray[retries-1].port));
    Serial.print("api: ");
    Serial.println(myTimeServerArray[retries-1].api);

    httpTimeClient.get(myTimeServerArray[retries-1].api);

    statusCode = httpTimeClient.responseStatusCode();
    response = httpTimeClient.responseBody();
    Serial.println("GetSetCurrentTime->statusCode: " + String(statusCode));
    Serial.println("GetSetCurrentTime->response: " + response);
    retries--;
    httpTimeClient.stop();
    delay(1024);  // Delay 1 second between retries
  }
  if (statusCode == 200) {
    JSONVar myObject = JSON.parse(response);
    if (JSON.typeof(myObject) == "undefined") {
      Serial.println("Parsing input failed!");
      return;
    }
    myTimeServerArray[retries].function(myObject);
  } else {
    Serial.println("Failed to get time; trying again in 3 minutes");
    Alarm.free(retryGetTimeAlarmID);
    retryGetTimeAlarmID = Alarm.timerOnce(180, GetSetCurrentTime);   // call once after 180 mseconds
  }
}

void writeUint16ToEEPROM(int address, uint16_t value) {
    // Write each byte of the uint16_t value into EEPROM
    EEPROM.write(address, (value >> 8) & 0xFF);     // Most significant byte
    Serial.println("writeUint16ToEEPROM[eepromAddress: " + String(address) + "]->value: " + String((value >> 8) & 0xFF));
    EEPROM.write(address + 1, value & 0xFF);            // Least significant byte
    Serial.println("writeUint16ToEEPROM[eepromAddress: " + String(address + 1) + "]->value: " + String(value & 0xFF));
    EEPROM.commit(); // Commit changes to EEPROM
}

uint16_t readUint16FromEEPROM(int address) {
    // Read each byte from the EEPROM and reconstruct the uint16_t value
    uint16_t value = 0;
    value |= ((uint16_t)EEPROM.read(address) << 8);
    value |= (uint16_t)EEPROM.read(address + 1);
    //Serial.println("readUint16FromEEPROM<" + String(address) + ">: " + String(value));
    return value;
}

/*
bitfields 3-9
sprinkler time schedule bitfield
uint16_t sprinklerTimeScheduleBitfield

bits    description  
---     -----------
0-2     day of the week (0=Sunday, 6=Saturday)
3-13    time of day in minutes (1=12:00AM; 1440=11:59PM; 121=2:01AM)
struct TimeSchedule {
    uint8_t dayOfTheWeek;  // 0-6 for Sunday-Saturday
    uint8_t hour;          // 0-23 for hours of the day
    uint8_t minute;        // 0-59 for minutes of the hour
};
*/

uint16_t createSprinklerTimeScheduleBitfield(TimeSchedule myTimeSchedule) {
  uint16_t myBitField = 0x0000;
  uint16_t minutes = 0x0000;
  Serial.println("createSprinklerTimeScheduleBitfield->dayOfTheWeek: " + String(myTimeSchedule.dayOfTheWeek));
  Serial.println("createSprinklerTimeScheduleBitfield->hour: " + String(myTimeSchedule.hour));
  Serial.println("createSprinklerTimeScheduleBitfield->minute: " + String(myTimeSchedule.minute));

  myBitField = myTimeSchedule.dayOfTheWeek & 0x0007;
  Serial.println("createSprinklerTimeScheduleBitfield->myBitField->dayOfTheWeek: " + String(myBitField));

  minutes = (myTimeSchedule.hour * 60) + myTimeSchedule.minute;
  myBitField = myBitField | (minutes<<3);
  Serial.println("createSprinklerTimeScheduleBitfield->myBitField->minutes: " + String(myBitField >> 3));
  Serial.println("createSprinklerTimeScheduleBitfield->myBitField: " + String(myBitField));
  return myBitField;
}

void eepromDump(int maxAddress) {
  uint8_t value = 0;
  for (int i = 0; i <= maxAddress; i++) {
    value = EEPROM.read(i);
    // Serial.println("eepromDump->address<" + String(i) + ">: " +  String(value));
  }
}

void clearEEPROM() {
  int maxEEPROMAddress = EEPROM_ADDR_FIRST_SCHED + (MAX_NUM_SCHEDS * 2);
  Serial.println("clearEEPROM->maxEEPROMAddress: " + String(maxEEPROMAddress));
  for (int addr = 0; addr <= maxEEPROMAddress; addr++) {
    EEPROM.write(addr, (uint8_t)0x00);
  }
  EEPROM.commit();
}

void setDefaultSchedule() {
  Serial.println("setDefaultSchedule->setting default schedule");
  mySprinklerSchedule.zones = 3;
  mySprinklerSchedule.durationMinutes = 30;
  mySprinklerSchedule.numberOfTimeSchedules = 2;
  mySprinklerSchedule.myTimeSchedule[0].dayOfTheWeek = 3;
  mySprinklerSchedule.myTimeSchedule[0].hour = 6;
  mySprinklerSchedule.myTimeSchedule[0].minute = 30;
  mySprinklerSchedule.myTimeSchedule[1].dayOfTheWeek = 6;
  mySprinklerSchedule.myTimeSchedule[1].hour = 6;
  mySprinklerSchedule.myTimeSchedule[1].minute = 30;
}

bool validateSchedule(SprinklerSchedule aSprinklerSchedule) {
  bool isValid = true;
  if (!((aSprinklerSchedule.zones >= 0) && (aSprinklerSchedule.zones <= MAX_NUM_ZONES))) {
    Serial.println("validateSchedule->problem with number of zones: " + String(aSprinklerSchedule.zones));
    isValid = false; 
  } else if (!((aSprinklerSchedule.durationMinutes >= 1) && (aSprinklerSchedule.durationMinutes <= MAX_DURATION_PER_ZONE))) {
      Serial.println("validateSchedule->problem with durationMinutes: " + String(aSprinklerSchedule.durationMinutes));
      isValid = false; 
    } else if (!((aSprinklerSchedule.numberOfTimeSchedules >= 1) && (aSprinklerSchedule.numberOfTimeSchedules <= MAX_NUM_SCHEDS))) { 
        Serial.println("validateSchedule->problem with numberOfTimeSchedules: " + String(aSprinklerSchedule.numberOfTimeSchedules));
        isValid = false; 
      } else if (!timeScheduleValidated(aSprinklerSchedule.numberOfTimeSchedules, aSprinklerSchedule.myTimeSchedule)) {
          Serial.println("validateSchedule->problem with timeScheduleValidated()");
          isValid = false; 
        }
  return isValid;
}

bool timeScheduleValidated(uint8_t numScheds, TimeSchedule theTimeSchedule[]) {
  bool valid = false;
  #ifdef DEBUG
    Serial.println("timeScheduleValidated->numScheds: " + String(numScheds));
  #endif
  if ((numScheds >= 1) && (numScheds <= MAX_NUM_SCHEDS)) {
    for (int i = 0; i < numScheds; i++) {
      if (  (theTimeSchedule[i].dayOfTheWeek >= 0) && (theTimeSchedule[i].dayOfTheWeek <= 6) &&
            (theTimeSchedule[i].hour >= 0) && (theTimeSchedule[i].hour <= 23) &&
            (theTimeSchedule[i].minute >= 0) && (theTimeSchedule[i].minute <= 59) ) {
        valid = true;
      } else {
          Serial.println("timeScheduleValidated->problem with number of time schedule values out of range; breaking out of loop");
          valid = false;
          break;
      }
    }
  } else {
      Serial.println("timeScheduleValidated->problem with number of schedules passed in: " + String(numScheds));
  }
  return valid;
}

void clearAlarms() {
  #ifdef DEBUG
    Serial.println("clearAlarms()");
  #endif
  // first clear scheduling alarms
  for (int i =0; i < MAX_NUM_SCHEDS; i++) {
    //Serial.println("clearAlarms->clearing schedule alarm ID: " + String(schedAlarmIDArray[i]));
    Alarm.free(schedAlarmIDArray[i]);
  }
  // next clear the get-set-time alarm
  //Serial.println("clearAlarms->clearing getSetCurrentTimeAlarmID: " + String(getSetCurrentTimeAlarmID));
  Alarm.free(getSetCurrentTimeAlarmID);
  //Serial.println("clearAlarms->clearing onAlarmID: " + String(onAlarmID));
  Alarm.free(onAlarmID);
  //Serial.println("clearAlarms->clearing offAlarmID: " + String(offAlarmID));
  Alarm.free(offAlarmID);
  //Serial.println("clearAlarms->clearing retryGetTimeAlarmID: " + String(retryGetTimeAlarmID));
  Alarm.free(retryGetTimeAlarmID);
}

void deepCopySprinklerSchedule(SprinklerSchedule &source, SprinklerSchedule &destination) {
    // Copy basic fields
    destination.zones = source.zones;
    destination.durationMinutes = source.durationMinutes;
    destination.numberOfTimeSchedules = source.numberOfTimeSchedules;

    // Copy each TimeSchedule in the array
    for (int i = 0; i < MAX_NUM_SCHEDS; i++) {
        destination.myTimeSchedule[i].dayOfTheWeek = source.myTimeSchedule[i].dayOfTheWeek;
        destination.myTimeSchedule[i].hour = source.myTimeSchedule[i].hour;
        destination.myTimeSchedule[i].minute = source.myTimeSchedule[i].minute;
    }
}

timeDayOfWeek_t convertInt2DOW(int value) {
  timeDayOfWeek_t result = dowSunday;
  switch(value) {
    case 0:
      result = dowSunday;
      break;
    case 1:
      result = dowMonday;
      break;
    case 2:
      result = dowTuesday;
      break;
    case 3:
      result = dowWednesday;
      break;
    case 4:
      result = dowThursday;
      break;
    case 5:
      result = dowFriday;
      break;
    case 6:
      result = dowSaturday;
      break;
    case 7:
    // error case
      result = dowSunday;
      break;
  }
  return result;
}

void pulseLED(int theLED, int numberPulses, int duration) {
  char theArgs[64];
  digitalWrite(theLED, HIGH);
  #ifdef DEBUG
    sprintf(theArgs, "theLED: %02d;  numberPulses: %02d;  duration(ms): %04d", theLED, numberPulses, duration);
    Serial.print("pulseLED->theArgs: ");
    Serial.println(theArgs);
  #endif
  for (int i = 0; i < numberPulses; i++) {
    digitalWrite(theLED, LOW);
    delay(duration);
    digitalWrite(theLED, HIGH);
    delay(duration);
  }
}

void onboardLED_ON(int theLED, bool pulsed, int duration) {
  char theArgs[64];
  digitalWrite(theLED, HIGH);
  sprintf(theArgs, "theLED: %02d;  pulsed: %01d", theLED, pulsed);
  Serial.print("onboardLED_ON->theArgs: ");
  Serial.println(theArgs);
//    
    digitalWrite(theLED, LOW);
    delay(duration);
    digitalWrite(theLED, HIGH);
    delay(duration);
}

void onboardLED_OFF(int theLED) {
  char theArgs[64];
  digitalWrite(theLED, HIGH);
  sprintf(theArgs, "theLED: %02d", theLED);
  Serial.print("onboardLED_OFF->theArgs: ");
  Serial.println(theArgs);
}


void checkCLI() {
  #ifdef DEBUG
    Serial.println("checkCLI");
  #endif
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');

    // Ensure the input has at least one character for the command number
    if (input.length() > 0) {
      // Extract the command number (first character) and convert to integer
      int commandNumber = input.substring(0, 1).toInt();
      // Extract the command body (second and subsequent characters)
      String commandBody = input.substring(1);

      switch (commandNumber) {
        case 0:
          // Expect the command body to be in the format YYYY-MM-DDTHH:MM:SS
          if (commandBody.length() == 19 && commandBody.charAt(10) == 'T') {
            int year = commandBody.substring(0, 4).toInt();
            int month = commandBody.substring(5, 7).toInt();
            int day = commandBody.substring(8, 10).toInt();
            int hour = commandBody.substring(11, 13).toInt();
            int minute = commandBody.substring(14, 16).toInt();
            int second = commandBody.substring(17, 19).toInt();
            
            // Set the time using the parsed values
            setTime(hour, minute, second, day, month, year);
            delay(1024);
            if (validateTime()) {
              Serial.println("checkCLI->Time set successfully.");
            } else {
                Serial.println("checkCLI->Time NOT set successfully.");
            }
          } else {
            Serial.println("checkCLI->Invalid format. Use YYYY-MM-DDTHH:MM:SS.");
          }
          break;
        
        // Add more cases for other command numbers if needed

        default:
          Serial.println("checkCLI->Unknown command number.");
          break;
      }
    }
  }
}

void parse_worldtimeapi(JSONVar myObject) {
    Serial.println("parse_worldtimeapi");
    String datetime = (const char*) myObject["datetime"];
    Serial.println("parse_worldtimeapi->datetime: " + datetime);

    int hr = (datetime.substring(11,13)).toInt();
    int min = (datetime.substring(14,16)).toInt();
    int sec = (datetime.substring(17,19)).toInt();
    int day = (datetime.substring(8,10)).toInt();
    int month = (datetime.substring(5,7)).toInt();
    int year = (datetime.substring(0,4)).toInt();
    setTime(hr, min, sec, day, month, year);
}

void parse_worldclockapi(JSONVar myObject) {
    Serial.println("parse_worldclockapi");
    String datetime = (const char*) myObject["currentDateTime"];
    Serial.println("parse_worldclockapi->datetime: " + datetime);
    int hr = (datetime.substring(11,13)).toInt();
    int min = (datetime.substring(14,16)).toInt();
    int sec = 0;
    int day = (datetime.substring(8,10)).toInt();
    int month = (datetime.substring(5,7)).toInt();
    int year = (datetime.substring(0,4)).toInt();
    setTime(hr, min, sec, day, month, year);
}

void parse_timeapi(JSONVar myObject) {
    Serial.println("parse_timeapi");
    String datetime = (const char*) myObject["datetime"];
    Serial.println("parse_timeapi->datetime: " + datetime);
    int hr = (datetime.substring(11,13)).toInt();
    int min = (datetime.substring(14,16)).toInt();
    int sec = (datetime.substring(17,19)).toInt();
    int day = (datetime.substring(8,10)).toInt();
    int month = (datetime.substring(5,7)).toInt();
    int year = (datetime.substring(0,4)).toInt();
    setTime(hr, min, sec, day, month, year);
}

void getSetNTPTime() {
  #ifdef DEBUG
    Serial.println("getSetNTPTime()");
  #endif
  int offsetUTC = 0;
  WiFiUDP ntpUDP;
  NTPClient ntpClient(ntpUDP, "time.google.com", 3600 * -4);  // UTC offset in seconds
  delay(2048);
  ntpClient.begin();
  delay(1024);
  ntpClient.update();
  delay(1024);
  unsigned long t = (unsigned long)0;
  int theMonth = (int)0;
  int theYear = (int)0;
  while ((t <= 0) || (theYear < 2024)) {
    t = ntpClient.getEpochTime();
    delay(2048);
    // correct for daylight savings time
    theMonth = month(t);
    theYear = year(t);
  }
  offsetUTC = (theMonth >= 3 && theMonth <= 11) ? (3600 * -4) : (3600 * -5);
  ntpClient.setTimeOffset(offsetUTC);
  ntpClient.update();
  delay(1024);
  t = ntpClient.getEpochTime();
  delay(1024);
  setTime(t);
  ntpClient.end();
}

void reportRelayState() {
  // Read the state of the relay pin
  int relayState = digitalRead(relayPin);
  if (relayState == HIGH) {
    #ifdef DEBUG
      Serial.println("relay is ON");
    #endif
    pulseLED(__blue_led_pin, 3, LED_SHORT_BURST_MILLISECONDS);
  } else {
      #ifdef DEBUG
        Serial.println("relay is OFF");
      #endif
  };
}

void memCheck() {
  uint32_t myHeapVal = ESP.getFreeHeap();
  #ifdef DEBUG
    Serial.print("memCheck->Free Heap: ");
    Serial.println(ESP.getFreeHeap());
  #endif
  if (myHeapVal < 0xffff) {  // 65535
    isHeapMemLow = (uint8_t)1;
  } else {
    isHeapMemLow = (uint8_t)0;
  }
}

void getCurrentTimestamp() {
  #ifdef DEBUG
    Serial.println("getCurrentTimestamp()");
  #endif
  getSetNTPTime();
  delay(APP_GRN_DELAY);
  if (!validateTime()) {
    GetSetCurrentTime();
  }
}

bool validateTime() {
  bool isValid = false;
  if (year() != 1970) {
    isValid = true;
    isTimeStampNotSet = (uint8_t)0;
  } else {
    isValid = false;
    isTimeStampNotSet = (uint8_t)1;
    pulseLED(___red_led_pin, 3, LED_SHORT_BURST_MILLISECONDS);
  }
  return isValid;
}

void statusCheck() {
  #ifdef DEBUG
    Serial.printf("isScheduleInvalid: [%d]\t", isScheduleInvalid);
    Serial.printf("isHeapMemLow: [%d]\t", isHeapMemLow);
    Serial.printf("isTimeStampNotSet: [%d]\t", isTimeStampNotSet);
    Serial.printf("arduinoCommandError: [%d]\t", arduinoCommandError);
    long rssi = WiFi.RSSI();
    isRSSIWeak = (rssi <= (long)-80) ? (uint8_t)1 : (uint8_t)0;
    Serial.printf("isRSSI(%d)Weak: [%d]\n\r", rssi, isRSSIWeak);
  #endif
}

uint8_t generateStatusWord() {
  uint8_t arduinoStatusWord = (uint8_t)0;
  arduinoStatusWord = arduinoStatusWord | isScheduleInvalid;                       // bit 0
  arduinoStatusWord = arduinoStatusWord | isHeapMemLow<<1;            // bit 1
  arduinoStatusWord = arduinoStatusWord | isTimeStampNotSet<<2;       // bit 2
  arduinoStatusWord = arduinoStatusWord | isRSSIWeak<<3;              // bit 3
  arduinoStatusWord = arduinoStatusWord | arduinoCommandError<<4;     // bit 4
  return arduinoStatusWord;
}
 