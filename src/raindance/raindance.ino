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
    delay(3000);
    }


}