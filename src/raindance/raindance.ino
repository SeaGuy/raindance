#include <WiFiNINA.h>
#include <WiFiServer.h>
#include <EEPROM.h>

// WiFi settings
const char* ssid = "ARRIS-439E";
const char* password = "287860664144";

int status = WL_IDLE_STATUS;
WiFiServer server(80);

// Define the pin for the relay
const int relayPin = 7;

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(9600);
  delay(500);
  while (!Serial) {
    ; // wait for SERIAL PORT TO CONNECT
  }
  Serial.println("setting up serial port");

  // Initialize the relay pin as an output
  pinMode(relayPin, OUTPUT);
  Serial.println("configuring digital output pin for relay");

  // Ensure the relay is off initially
  digitalWrite(relayPin, LOW);

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, password);
    delay(10000);
  }
  server.begin();  
  IPAddress myIPAddress = WiFi.localIP();
  Serial.print("Connected to WiFi with address ");
  Serial.println(myIPAddress);
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

  WiFiClient client = server.available();
  while (!client) {
    Serial.println("http server not available");
    delay(5000);
    client = server.available();
  }
  if (client) {
    Serial.println("New client connected");
    String currentLine = "";
    while (client.connected()) {
      //Serial.println("New client still connected");
      if (client.available()) {
        Serial.println("New client connected");
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {
          Serial.println(">>>>>>>>>> \n detetced! <<<<<<<<<<<<");
          Serial.print(">>>>>>>>>> currentLine.length() <<<<<<<<<<<<");
          Serial.println(currentLine.length());
          Serial.println(currentLine);

          if (currentLine.length() != 0) {
            // Send HTTP response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            // Handle request
            if (client.available()) {
              Serial.println(" ********* client ready to write to relay *********");
              String request = client.readStringUntil('\r');
              Serial.println(request);
              if (request.indexOf("/on") != -1) {
                digitalWrite(relayPin, HIGH);
                client.print("Sprinkler is ON");
              } else if (request.indexOf("/off") != -1) {
                digitalWrite(relayPin, LOW);
                client.print("Sprinkler is OFF");
              }
            }
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println("Client disconnected");
  } else {
    Serial.println("New client NOT connected");
    delay(3000);
  }
}
