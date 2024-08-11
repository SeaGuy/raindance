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

  int size = 0;
  char buf[512];
  char command[4]; // length of command ("ON", "OFF") + 1 for null terminator
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
      buf[0] = '\0';
      command[0] = '\0';
      //Serial.println("New client still connected");
      if (client.available()) {
        Serial.println("New client connected");
        // char c = client.read();
        size = client.read(buf, 256);
        if (size > 0) {
          Serial.print("client.read() size: ");
          Serial.println(size);
          Serial.print("client.read() buf: ");
          Serial.println(buf);
          Serial.println('\n\r');

          size_t length = strlen(buf);
          Serial.print("strlen(buf): ");
          Serial.println(length);

          strncpy(command, buf + 5, 3);
          command[3] = '\0';

          length = strlen(command);
          Serial.print("strlen(command): ");
          Serial.println(length);

        
          Serial.print("command: ");
          Serial.println(command);
          Serial.println('\n\r');

          if (strcmp(command, "ONN") == 0) {
                digitalWrite(relayPin, HIGH);
                // client.print("Sprinkler is ON");
              } else if (strcmp(command, "OFF") == 0) {
                digitalWrite(relayPin, LOW);
                // client.print("Sprinkler is OFF");
              }


        }

        char c; /////////////////////////////
        Serial.write(c);
        if (c == '\n') {
          Serial.println(">>>>>>>>>> \n detected! <<<<<<<<<<<<");
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
