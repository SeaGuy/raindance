
<center><h1> Sprinkler Controller 1.0.1</h1></center>

[![Arduino Library specification badge](https://img.shields.io/badge/Arduino%20Library%20Specification%20-rev%202.1-00878F.svg?style=for-the-badge)](https://github.com/arduino/Arduino/wiki/Arduino-IDE-1.5:-Library-specification)
[![Arduino IDE compatibility badge](https://img.shields.io/badge/Arduino%20IDE%20compatibility-2.3.3+-00878F.svg?style=for-the-badge)](https://www.arduino.cc/en/Main/Software)

[![Keep a Changelog badge](https://img.shields.io/badge/Keep%20a%20Changelog-1.0.1-orange.svg?style=for-the-badge)](http://keepachangelog.com)
[![Semantic Versioning badge](https://img.shields.io/badge/Semantic%20Versioning-2.0.0-orange.svg?style=for-the-badge)](http://semver.org)

## github branch: nano-9::::github SHA: 3396391da6e872cba897f038a9f4fbbc556f7751


This project consists of two main components: an **iOS application** developed in SwiftUI for late model iPhones and an **Arduino Nano ESP32-based sprinkler controller assembly**. The sprinkler controller assembly is comprised of mechanical and electrical components, a microcontroller and firmware that runs the microcontroller.

![This is a alt text.](/images/A41BB433-12EE-4F97-A9FA-9D41BFD19718.png)


The iOS app monitors and controls a sprinkler system by sending HTTP commands to an Arduino Nano ESP32 in the sprinkler controller which controls a relay that can power a water pump for sprinkling.

## Table of Contents
- [github branch: nano-9::::github SHA: 3396391da6e872cba897f038a9f4fbbc556f7751](#github-branch-nano-9github-sha-3396391da6e872cba897f038a9f4fbbc556f7751)
- [Table of Contents](#table-of-contents)
- [Overview](#overview)
- [System Components](#system-components)
  - [iOS Application](#ios-application)
    - [Features](#features)
    - [Requirements](#requirements)
    - [Installation](#installation)
  - [Sprinkler Controller Firmware](#sprinkler-controller)
    - [Features](#features-1)
    - [Hardware Setup](#hardware-setup)
    - [Firmware Installation](#firmware-installation)
- [HTTP Commands](#http-commands)
  - [Command List](#command-list)
  - [Example Requests](#example-requests)
    - [Turn Sprinkler ON](#turn-sprinkler-on)
    - [Turn Sprinkler OFF](#turn-sprinkler-off)
    - [Set Schedule](#set-schedule)
- [Usage](#usage)
- [License](#license)

## Overview

This sprinkler control system manages and automates lawn watering schedules. The system consists of an iOS app for the iPhone that acts as the user interface by monitoring sprinkler system status and controlling the system by sending commands to the Arduino Nano ESP32 micorcontroller.

![This is a alt text.](/images/iPhoneAppScreen)


The Arduino manages hardware by controlling a relay that either opens or closes a circuit between the 220VAC house electrical service and the water pump.

**NOTE:** The sprinkler system can also be turned on and off manually using the toggle switch mounted via a raised plexiglass platform on the controller assembly (see image below).  The toggle switch positions:

- *up* position:  AUTO closes a circuit between house mains and the relay, so that:
    - The pump is powered ON when the relay is activated by the microcontroller's scheduler  
    - The pump is powered OFF when the relay is deactivated by the microcontroller's scheduler
- *middle* position:   OFF (the pump is powered OFF)
- *bottom* position:  ON (the pump is powered ON)

<br>


![This is a alt text.](/images/IMG_0637.png)

<br>

### System Wiring Diagram

The wiring diagram below shows relationships among electrical components in the sprinkler controller assembly.  

![This is a alt text.](/images/design-drawings-electrical.drawio.png)
<br>

The components include:
- Arduino Nano ESP32 microcontroller
- YYG-2 one way relay
- 30W switching power supply
- Single-pole dual-throw (SPDT) toggle switch
- 5VDC terminal blocks (2)
- 600VAC/115A power distribution blocks (3)
- 220VAC/10A circuit breaker


## System Components

### iOS Application

#### Features
- Monitor the status of the sprinkler system.
- Manually control the sprinklers (turn on/off).
- Set and modify watering schedules.
- Adjust the system IP address dynamically for communication.

#### Requirements
- iOS 15.0 or later.
- Developed in SwiftUI.
- Network access to communicate with the Arduino device.

#### Installation
1. Go to your git directory
    ```zsh
    cd ~/git
    ```
2. Clone the repository:
   ```zsh
   git clone git@github.com:SeaGuy/raindance.git
   ```
2. Open the project (~/Project/git/raindance/src/raindance) in Xcode.
3. Build and run the application on your iOS device.

### Sprinkler Controller Firmware

#### Features
- Receives and processes HTTP commands from the iOS app.
- Controls a relay to activate or deactivate the water pump.
- Manages schedules and timing of watering.
- Monitors and sends back system status updates to the app.

#### Requirements
- Arduino Nano ESP32 microprocessor
- Arduino IDE 

#### Firmware Installation
1. Clone the entire repository from github if needed.

   ```zsh
   git clone git@github.com:SeaGuy/raindance.git
   ```
2. Use the Arduino IDE *Library Manager* to install the necessary libraries 
- <TimeLib.h>, Time by Michale Margolis
- <TimeAlarms.h>, TimeAlarms by Michale Margolis
- `WiFiNINA`
- <ArduinoHttpClient.h>, ArduinoHttpClient by Arduino
- <Arduino_JSON.h>, Arduino_JSON by Arduino
- <NTPClient.h>, NTPClient by Fabrice Weinberg
- <ArduinoOTA.h>, required for OTA updates
- <ESPmDNS.h>, required for OTA updates (actually part if the ESP32 Arduino core in the Arduino ESP32 Board Manager)
3. Upload the sketch to the Arduino Nano ESP32 via the Arduino IDE.
- troubleshooting:  https://support.arduino.cc/hc/en-us/articles/9810414060188-Reset-the-Arduino-bootloader-on-the-Nano-ESP32 



#### Hardware Setup
- **Arduino Nano ESP32**: The core microcontroller for the project.
- **Relay Module**: When activated by the micorcontroller, it closes the normally open (NO) circuit between house mains and the water pump.
     - YYG-2  one-way relay module
     - Switched Input/Output: 250VAC/30A
     - Power supply: 5VDC

<br>

![This is a alt text.](/images/relay-1.jpg)

- **Water Pump**: Performs the actual sprinkling.
- **Power Supply**: Provides power to the relay and pump.
- **Wiring**: Connect the relay to the appropriate GPIO pins on the Arduino Nano ESP32. The relay should control the power to the water pump.

The pins of the Arduino are inserted into headers which are soldered to a quarter-size Perma-Proto breadboard PCB, so that the Arduino can be removed and reinserted without soldering.

The active Arduino pins are:
- Vin (voltage in):  uses an internal voltage regulator which converts any voltage between 5-12vdc into the system voltage of 3.3vdc. The 5vdc power  supplied by the switching power supply will be wired here.
- GND (ground):  will be wired to the GND bus bar
- D7 (digital output 7):  controlled by firmware to enable or disable the relay
    
  

![This is a alt text.](/images/proto-board-wiring-3.jpeg)


## HTTP Commands

### Command List
The iOS app sends HTTP POST commands to control the Arduino Nano ESP32. The following commands are supported:

- **Turn Sprinkler ON:**
  - `POST /sprinkler/on`
  - Activates the relay to start the water pump.
  
- **Turn Sprinkler OFF:**
  - `POST /sprinkler/off`
  - Deactivates the relay to stop the water pump.
  
- **Set Schedule:**
  - `POST /schedule`
  - Sets or updates a watering schedule.

- **Get Status:**
  - `GET /status`
  - Returns the current system status, including whether the sprinkler is on or off.

### Example Requests

#### Turn Sprinkler ON
\``bash
POST /sprinkler/on
Host: 192.168.1.100
Content-Type: application/json

{
  "command": "ON"
}
\``

#### Turn Sprinkler OFF
\``bash
POST /sprinkler/off
Host: 192.168.1.100
Content-Type: application/json

{
  "command": "OFF"
}
\``

#### Set Schedule
\``bash
POST /schedule
Host: 192.168.1.100
Content-Type: application/json

{
  "startTime": "06:00",
  "duration": 15
}
\``

## Usage
1. Ensure the Arduino Nano ESP32 is connected to the same network as the iOS app.
2. Use the iOS app to control the sprinkler system:
   - Tap the **Sprinkler ON** button to activate the water pump.
   - Tap the **Sprinkler OFF** button to deactivate the water pump.
   - Set a schedule by specifying the start time and duration.
3. The app will communicate with the Arduino to send commands and display the status of the sprinkler system.

## Issues
1. [1/17/2025] Seeing this during board initialization:  "PrintSprinklerSchedule()->scheduleName: <k�?>::zones: <255>::durationMinutes: <255>::numberOfTimeSchedules: <255>
PrintSprinkSerial port connected"
2. [1/18/2025] send current schedule to iPhone.  Is there room for two clocks or times?
3. [1/19/2025] if schedule is not valid at powerup, a default shedule is used.  But then a schedule cannot be added from the app:  "Limit Reached" popup.

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.
