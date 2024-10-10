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

// Global variables

AlarmID_t schedAlarmID;                         // Variable to store the alarm ID
AlarmID_t schedAlarmIDArray[MAX_NUM_SCHEDS];
AlarmID_t getSetCurrentTimeAlarmID; 
AlarmID_t onAlarmID;
AlarmID_t offAlarmID;
AlarmID_t retryGetTimeAlarmID;

int eepromAddrNumSchedules = 0; // stored at first address
uint16_t sprinklerTimeScheduleBitfield = 0x00;

int zones = 1;

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
void PrintCurrentTime();
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
timeDayOfWeek_t convertInt2DOW(int value);

// WiFi settings
const char ssid[] = "ARRIS-439E";
const char password[] = "287860664144";

WiFiServer server(80);

const char timeServerAddress[] = "worldtimeapi.org";
const char timeServerAPI[] = "/api/timezone/America/New_York";
int timePort = 80;
WiFiClient wifiTimeClient;
HttpClient httpTimeClient = HttpClient(wifiTimeClient, timeServerAddress, timePort);

// Define the pin for the relay (adjust the GPIO pin as needed for your hardware)
const int relayPin = 7; // Ensure this pin is appropriate for the ESP32
int relayState = 0;

char hiTimeStamp[25];

void setup() {
  setupSerial();
  delay(3000);
  setupRelay();
  delay(3000);
  connectToWiFi();
  delay(3000);
  // Initialize EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialise EEPROM");
    return;
  }
  delay(3000);
  server.begin();
  delay(3000);
  GetSetCurrentTime();
  delay(3000);
  PrintCurrentTime();
  delay(3000);
  eepromDump(EEPROM_MAX_ADDRESS);
  delay(3000);


  getScheduleFromEEPROM();
  if (validateSchedule(mySprinklerSchedule)) {
    Serial.println("setup->schedule is valid ...");
  } else {
      Serial.println("setup->schedule is not valid ...");
  };


  PrintSprinklerSchedule("mySprinklerSchedule", mySprinklerSchedule);
  delay(3000);
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
  };
  PrintSprinklerSchedule("mySprinklerSchedule", mySprinklerSchedule);
  delay(3000);
}

void setupSerial() {
  Serial.begin(115200);
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
  Serial.print("Connecting to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    Serial.print(".");
    delay(1000);
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected to WiFi with address ");
    Serial.println(WiFi.localIP());
    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
  } else {
    Serial.println();
    Serial.println("Failed to connect to WiFi");
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
  Serial.println("getScheduleFromEEPROM->numZones in EEPROM: " + String(numZones));
  Serial.println("getScheduleFromEEPROM->numMinutes in EEPROM: " + String(numMinutes));
  Serial.println("getScheduleFromEEPROM->numScheds in EEPROM: " + String(numScheds));
  if (numScheds >= 1) {
    for (int i = 0; i <= ((int)numScheds - 1); i++) { 
        value = readUint16FromEEPROM(EEPROM_ADDR_FIRST_SCHED + (2 * i));
        eepromSchedule.myTimeSchedule[i].dayOfTheWeek = (uint8_t)(value & 0x0007);
        uint16_t hourMinBits = (uint16_t)((value >> 3) & 0x07FF);
        eepromSchedule.myTimeSchedule[i].hour = (uint8_t)(hourMinBits / 60); // divide by 60 minutes;
        eepromSchedule.myTimeSchedule[i].minute = (uint8_t)(hourMinBits % 60); // modulus after divide by 60 minutes
        Serial.println("getScheduleFromEEPROM->eepromSchedule.myTimeSchedule[i=" + String(i) + "].dayOfTheWeek: " + String(eepromSchedule.myTimeSchedule[i].dayOfTheWeek));
        Serial.println("getScheduleFromEEPROM->eepromSchedule.myTimeSchedule[i=" + String(i) + "].hour: " + String(eepromSchedule.myTimeSchedule[i].hour));
        Serial.println("getScheduleFromEEPROM->eepromSchedule.myTimeSchedule[i=" + String(i) + "].minute: " + String(eepromSchedule.myTimeSchedule[i].minute));
    }
  }
  PrintSprinklerSchedule("eepromSchedule", eepromSchedule);
  deepCopySprinklerSchedule(eepromSchedule, mySprinklerSchedule);
}

void writeScheduleToEEPROM() {
  Serial.println("writeScheduleToEEPROM");
  PrintSprinklerSchedule("mySprinklerSchedule", mySprinklerSchedule);  
  EEPROM.write(EEPROM_ADDR_NUM_ZONES, mySprinklerSchedule.zones & 0xFF);
  EEPROM.write(EEPROM_ADDR_NUM_MINUTES, mySprinklerSchedule.durationMinutes & 0xFF);
  EEPROM.write(EEPROM_ADDR_NUM_SCHEDS, mySprinklerSchedule.numberOfTimeSchedules & 0xFF);
  for (int i = 0; i < mySprinklerSchedule.numberOfTimeSchedules; i++) {
    TimeSchedule myTimeSchedule = mySprinklerSchedule.myTimeSchedule[i];
    uint16_t myBitField = createSprinklerTimeScheduleBitfield(myTimeSchedule);
    Serial.println("writeScheduleToEEPROM->mySprinklerSchedule.myTimeSchedule[" + String(i) + "]->myBitField: " + String(myBitField));
    writeUint16ToEEPROM(EEPROM_ADDR_FIRST_SCHED + (i * 2), myBitField);
  }
  EEPROM.commit(); // Commit changes to EEPROM
}

void setupAlarms() {
  // set alarms based on mySprinklerSchedule.myTimeSchedule
  // timeDayOfWeek_t is an enum in TimeLib.h {undefined=0, 1=Sunday, 2=Monday, etc.}
  int numScheds = mySprinklerSchedule.numberOfTimeSchedules;
  int myDayOfTheWeek = -1;
  zones = (int)(mySprinklerSchedule.zones);
  Serial.println("setupAlarms->numScheds: " + String(numScheds));
  Serial.println("setupAlarms->clearing alarms ..." );
  clearAlarms();
  for (int i = 0; i < numScheds; i++) {
    int dayOfTheWeek = (int)mySprinklerSchedule.myTimeSchedule[i].dayOfTheWeek;
    int hour = (int)mySprinklerSchedule.myTimeSchedule[i].hour;
    int minute = (int)mySprinklerSchedule.myTimeSchedule[i].minute;
    Serial.println("setupAlarms->dayOfTheWeek: " + String(dayOfTheWeek));
    Serial.println("setupAlarms->hour: " + String(hour));
    Serial.println("setupAlarms->minute: " + String(minute));
    timeDayOfWeek_t dowEnum = convertInt2DOW(mySprinklerSchedule.myTimeSchedule[i].dayOfTheWeek);
    schedAlarmID = Alarm.alarmRepeat(dowEnum, mySprinklerSchedule.myTimeSchedule[i].hour, mySprinklerSchedule.myTimeSchedule[i].minute, 0, ScheduledSprinklerOn);
    Serial.println("setupAlarms->adding schedule alarm ID: " + String(schedAlarmID));
    schedAlarmIDArray[i] = schedAlarmID;
    }
    getSetCurrentTimeAlarmID = Alarm.alarmRepeat(5, 0, 0, GetSetCurrentTime); // 5:00 AM every day
    Serial.println("setupAlarms->get-set-time alarm created with ID: " + String(getSetCurrentTimeAlarmID));
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
      int sprinklerStateInt = digitalRead(relayPin);
      int daysoftheweek = 0x00;
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
      Serial.println("handleGetRequest->sprinklerStateInt: " + String(sprinklerStateInt));
      sprinklerStateInt = (sprinklerStateInt<<7) | daysoftheweek;
      Serial.println("handleGetRequest->sprinklerStateInt: " + String(sprinklerStateInt));
      sprintf(hiTimeStamp, "%04d-%02d-%02dT%02d:%02d:%02d::%03d", year(), month(), day(), hour(), minute(), second(), sprinklerStateInt);
      responseObj["status"] = hiTimeStamp;
  } else {
      responseObj["error"] = "Invalid command";
  }
}

void handlePostRequest(WiFiClient& client, JSONVar& responseObj) {
  Serial.println("handlePostRequest()");
  String jsonString = readJsonBody(client);
  Serial.println("handlePostRequest->Extracted JSON: " + jsonString);
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
    // delay(INTER_ZONE_DELAY);
    offAlarmID = Alarm.timerOnce(INTER_ZONE_DELAY_SECONDS, ScheduledSprinklerOn);
  }
}

void PrintCurrentTime() {
  if (year() != 1970) {
    char timestamp[20]; // Array to hold the formatted timestamp
    int y = year();
    int m = month();
    int d = day();
    int h = hour();
    int min = minute();
    int s = second();
    // Create a timestamp in the format "YYYY-MM-DD HH:MM:SS"
    sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02d", y, m, d, h, min, s);
    Serial.print("Current time: ");
    Serial.println(timestamp);
  } else {
    Serial.println("Time is NOT Set!");
  }
}

void PrintSprinklerSchedule(String scheduleName, SprinklerSchedule theSchedule) {
  Serial.println("PrintSprinklerSchedule()->scheduleName: " + scheduleName);
  Serial.print("\tzones: ");
  Serial.println(theSchedule.zones);
  Serial.print("\tdurationMinutes: ");
  Serial.println(theSchedule.durationMinutes);
  Serial.print("\tnumberOfTimeSchedules: ");
  Serial.println(theSchedule.numberOfTimeSchedules);
  PrintSprinklerTimeSchedule(theSchedule, theSchedule.numberOfTimeSchedules);
}

void PrintSprinklerTimeSchedule(SprinklerSchedule aSchedule, int numSchedules) {
  Serial.println("\tPrintSprinklerTimeSchedule()->numSchedules: " + String(numSchedules));
  char theTime[6];
  for (int i = 0; i < numSchedules; i++) {
      Serial.print("\t\tdayOfTheWeek: ");
      Serial.print(aSchedule.myTimeSchedule[i].dayOfTheWeek);
      Serial.print(", Time: ");
      sprintf(theTime, "%02d:%02d", aSchedule.myTimeSchedule[i].hour, aSchedule.myTimeSchedule[i].minute);
      Serial.print(theTime);
      Serial.println();
  }
}

void GetSetCurrentTime() {
  Serial.println("Getting and setting current time");
  httpTimeClient.get(timeServerAPI);
  int statusCode = httpTimeClient.responseStatusCode();
  String response = httpTimeClient.responseBody();
  Alarm.free(retryGetTimeAlarmID);
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
    retryGetTimeAlarmID = Alarm.timerOnce(60, GetSetCurrentTime);   // call once after 60 mseconds
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
    Serial.println("readUint16FromEEPROM<" + String(address) + ">: " + String(value));
    return value;
}

/*
bitfields 3-9
sprinkler time schedule bitfield
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
    Serial.println("eepromDump->address<" + String(i) + ">: " +  String(value));
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
  Serial.println("timeScheduleValidated->numScheds: " + String(numScheds));
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
  // first clear scheduling alarms
  for (int i =0; i < MAX_NUM_SCHEDS; i++) {
    Serial.println("clearAlarms->clearing schedule alarm ID: " + String(schedAlarmIDArray[i]));
    Alarm.free(schedAlarmIDArray[i]);
  }
  // next clear the get-set-time alarm
  Serial.println("clearAlarms->clearing getSetCurrentTimeAlarmID: " + String(getSetCurrentTimeAlarmID));
  Alarm.free(getSetCurrentTimeAlarmID);
  Serial.println("clearAlarms->clearing onAlarmID: " + String(onAlarmID));
  Alarm.free(onAlarmID);
  Serial.println("clearAlarms->clearing offAlarmID: " + String(offAlarmID));
  Alarm.free(offAlarmID);
  Serial.println("clearAlarms->clearing retryGetTimeAlarmID: " + String(retryGetTimeAlarmID));
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
