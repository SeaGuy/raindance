#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <EEPROM.h>

// WiFi settings
const char* ssid = "ARRIS-439E";
const char* password = "287860664144";

// MQTT settings
const char* mqtt_server = "47c8a5f0e323493b8ad39b814771ba74.s1.eu.hivemq.cloud";
const char* mqtt_topic = "irrigation/control";

WiFiClient espClient;
PubSubClient client(espClient);

// Define the pin for the relay
const int relayPin = 7;

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(9600);
  delay(1000);
  while (!Serial) {
    ; // wait for SERIAL PORT TO CONNECT
  }
  Serial.println("setting up serial port");

  // Initialize the relay pin as an output
  pinMode(relayPin, OUTPUT);
  Serial.println("initializing digital output pin for relay");

  // Ensure the relay is off initially
  digitalWrite(relayPin, LOW);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    
    int wifiStatus = WiFi.status();
    Serial.println(wifiStatus);
    if (wifiStatus == WL_NO_SHIELD) { Serial.println("wifiStatus = WL_NO_SHIELD");}
    if (wifiStatus == WL_CONNECTED) { Serial.println("wifiStatus = WL_CONNECTED");}
    if (wifiStatus == WL_IDLE_STATUS) { Serial.println("wifiStatus = WL_IDLE_STATUS");}
    if (wifiStatus == WL_AP_CONNECTED) { Serial.println("wifiStatus = WL_AP_CONNECTED");}
    if (wifiStatus == WL_AP_LISTENING) { Serial.println("wifiStatus = WL_AP_LISTENING");}
    if (wifiStatus == WL_CONNECT_FAILED) { Serial.println("wifiStatus = WL_CONNECT_FAILED");}
    if (wifiStatus == WL_CONNECTION_LOST) { Serial.println("wifiStatus = WL_CONNECTION_LOST");}

    Serial.println(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ArduinoClient", "williamlaing", "Laingwe7&04")) {
      Serial.println("MQTT client connected");
      // Once connected, publish an announcement...
      Serial.println("MQTT client preparing to publish ...");
      if (!client.publish("outTopic", "hello world")) { 
        Serial.println("MQTT client could NOT publish");
      }
      // ... and resubscribe
      Serial.println("MQTT client preparing to subscribe ...");
      if (!client.subscribe("inTopic")) { 
        Serial.println("MQTT client could NOT subscribe");
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
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

  if (!client.connected()) {
    Serial.println("client not connected");
    delay(5000);
    reconnect();
  }
  client.loop();

}
