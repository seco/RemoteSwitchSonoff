/* This sketch connects to the iopappstore and loads the assigned firmware down. The assignment is done on the server based on the MAC address of the board

    On the server, you need PHP script "IOTappStory.php" and the bin files are in the .\bin folder

    This work is based on the ESPhttpUpdate examples

    To add new constants in WiFiManager search for "NEW CONSTANTS" and insert them according the "boardName" example

  Copyright (c) [2016] [Andreas Spiess]

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/

/*
   LED signals of green LED:
   ---------------------------
   Setup till done: Blink
   ON: green LED completely on
   OFF: Green LED blinking with very short on-time
   Setup: very fast blinking green LED

*/

#define SKETCH "SonoffReceiver "
#define VERSION "V1.3"
#define FIRMWARE SKETCH VERSION

#define SERIALDEBUG         // Serial is used to present debugging messages 
#define REMOTEDEBUGGING     // telnet is used to present

#define LEDS_INVERSE   // LEDS on = GND

#include <credentials.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>        //https://github.com/kentaylor/WiFiManager
#include <Ticker.h>
#include <EEPROM.h>

#ifdef REMOTEDEBUGGING
#include <WiFiUDP.h>
#endif

extern "C" {
#include "user_interface.h" // this is for the RTC memory read/write functions
}

//--------  Sketch Specific -------

#include <ESP8266WebServer.h>
//#include <ESP8266HTTPClient.h>



// -------- PIN DEFINITIONS ------------------
#ifdef ARDUINO_ESP8266_ESP01           // Generic ESP's 
#define MODEBUTTON 0
#define LEDgreen 13
//#define LEDred 12
#else
#define MODEBUTTON D3
#define LEDgreen D7
//#define LEDred D6
#endif

// --- Sketch Specific -----
#ifdef ARDUINO_ESP8266_ESP01           // Generic ESP's 
#define RELAYPIN 12
#else
#define RELAYPIN D6
#endif


//---------- DEFINES for SKETCH ----------
#define STRUCT_CHAR_ARRAY_SIZE 50  // length of config variables
#define MAX_WIFI_RETRIES 50
#define RTCMEMBEGIN 68
#define MAGICBYTE 85

// --- Sketch Specific -----
#define SERVICENAME "SONOFF"  // name of the MDNS service used in this group of ESPs


//-------- SERVICES --------------
Ticker blink;

#ifdef REMOTEDEBUGGING
WiFiUDP UDP;
#endif

// --- Sketch Specific -----
WiFiServer server(80);

//--------- ENUMS AND STRUCTURES  -------------------

typedef struct {
  char ssid[STRUCT_CHAR_ARRAY_SIZE];
  char password[STRUCT_CHAR_ARRAY_SIZE];
  char boardName[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStory1[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStoryPHP1[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStory2[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStoryPHP2[STRUCT_CHAR_ARRAY_SIZE];
  char udpPort[10];
  // insert NEW CONSTANTS according boardname example HERE!
  char delayTime[10];
  char magicBytes[4];
} strConfig;

strConfig config = {
  "",
  "",
  "SONOFF_RECEIVER_INIT",
  "192.168.0.200",
  "/IOTappStory/IOTappStoryv20.php",
  "iotappstory.org",
  "/ota/esp8266-v1.php",
  "8004",
  "420",
  "CFG"  // Magic Bytes
};

typedef struct {
  byte markerFlag;
  int bootTimes;
} rtcMemDef __attribute__((aligned(4)));
rtcMemDef rtcMem;

// --- Sketch Specific -----
enum relayStatusDef {
  RELAY_OFF,
  RELAY_ON
} relayStatus;

enum wifiCommandDef {
  COMMAND_OFF,
  COMMAND_ON,
  COMMAND_STATUS
} wifiCommand = COMMAND_OFF;

//---------- VARIABLES ----------

String switchName1, switchName2, boardName, IOTappStory1, IOTappStoryPHP1, IOTappStory2, IOTappStoryPHP2;
unsigned long debugEntry;
volatile unsigned long buttonEntry;
unsigned long buttonTime;
volatile bool buttonChanged = false;
volatile int greenTimesOff = 0;
volatile int redTimesOff = 0;
volatile int greenTimes = 0;
volatile int redTimes = 0;
char boardMode = 'N';  // Normal operation or Configuration mode?

#ifdef REMOTEDEBUGGING
// UDP variables
char sendBuffer[255];
IPAddress broadcastIp(255, 255, 255, 255);
#endif

// --- Sketch Specific -----
// String xx; // add NEW CONSTANTS for WiFiManager according the variable "boardname"

int timeToOff = -1;   // time for Relay to switch off without ON command
unsigned long timeToOffEntry;




//---------- FUNCTIONS ----------
// to help the compiler, sometimes, functions have  to be declared here
void loopWiFiManager(void);
void readFullConfiguration(void);
bool readRTCmem(void);
void printRTCmem(void);
void initialize(void);
void switchRelay(bool);
bool handleWiFi(void);


//---------- OTHER .H FILES ----------
#include <ESP_Helpers.h>           // General helpers for all IOTappStory sketches
#include "IOTappStoryHelpers.h"    // Sketch specific helpers for all IOTappStory sketches




//-------------------------- SETUP -----------------------------------------

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 5; i++) DEBUG_PRINTLN("");
  DEBUG_PRINTLN("Start "FIRMWARE);
  UDPDEBUG_START();
  UDPDEBUG_PRINTTXT("Start ");
  UDPDEBUG_PRINTTXT(FIRMWARE);
  UDPDEBUG_SEND();


  // ----------- PINS ----------------
  pinMode(MODEBUTTON, INPUT_PULLUP);  // MODEBUTTON as input for Config mode selection

#ifdef LEDgreen
  pinMode(LEDgreen, OUTPUT);
  digitalWrite(LEDgreen, LEDOFF);
#endif
#ifdef LEDred
  pinMode(LEDred, OUTPUT);
  digitalWrite(LEDred, LEDOFF);
#endif

  // --- Sketch Specific -----
  pinMode(RELAYPIN, OUTPUT);


  // ------------- INTERRUPTS ----------------------------
  attachInterrupt(MODEBUTTON, ISRbuttonStateChanged, CHANGE);
  blink.detach();


  //------------- LED and DISPLAYS ------------------------
  LEDswitch(GreenBlink);


  // --------- BOOT STATISTICS ------------------------
  // read and increase boot statistics (optional)
  readRTCmem();
  rtcMem.bootTimes++;
  writeRTCmem();
  printRTCmem();


  //---------- SELECT BOARD MODE -----------------------------

  system_rtc_mem_read(RTCMEMBEGIN + 100, &boardMode, 1);   // Read the "boardMode" flag RTC memory to decide, if to go to config
  if (boardMode == 'C') configESP();

  readFullConfiguration();

  // --------- START WIFI --------------------------

  connectNetwork('N');

  DEBUG_PRINTLN("------------- Normal Mode -------------------");
  UDPDEBUG_START();
  UDPDEBUG_PRINTTXT("------------- Normal Mode -------------------");
  UDPDEBUG_SEND();

  IOTappStory();



  // ----------- SPECIFIC SETUP CODE ----------------------------

  // add a DNS service
  MDNS.addService(SERVICENAME, "tcp", 8080);

  // ----------- END SPECIFIC SETUP CODE ----------------------------

  LEDswitch(None);
  pinMode(MODEBUTTON, INPUT_PULLUP);  // MODEBUTTON as input for Config mode selection

  DEBUG_PRINTLN("Setup done");
  UDPDEBUG_START();
  UDPDEBUG_PRINTTXT("Setup done");
  UDPDEBUG_SEND();
}





//--------------- LOOP ----------------------------------
void loop() {
  //-------- IOTappStory Block ---------------
  yield();
  handleModeButton();   // this routine handles the reaction of the Flash button. If short press: update of skethc, long press: Configuration

  // Normal blind (1 sec): Connecting to network
  // fast blink: Configuration mode. Please connect to ESP network
  // Slow Blink: IOTappStore Update in progress

  if (millis() - debugEntry > 2000) { // Non-Blocking second counter
    debugEntry = millis();
    sendDebugMessage();
  }


  //-------- Your Sketch ---------------

  if (millis() - timeToOffEntry > 1000) { // Non-Blocking second counter
    timeToOffEntry = millis();
    if (timeToOff-- <= 0 ) timeToOff = 0;
  }
  handleWiFi();     // main function. Handles all WiFi traffic



}
//------------------------- END LOOP --------------------------------------------

void sendDebugMessage() {
  // ------- Debug Message --------
  DEBUG_PRINT("Board: ");
  DEBUG_PRINT(config.boardName);
  DEBUG_PRINT(" Firmware: ");
  DEBUG_PRINT(FIRMWARE);
  DEBUG_PRINT(" Heap ");
  DEBUG_PRINT(ESP.getFreeHeap());

  DEBUG_PRINT(" relayStatus: ");
  DEBUG_PRINT(relayStatus);
  DEBUG_PRINT(" WifiCommand: ");
  DEBUG_PRINTLN(wifiCommand);


  UDPDEBUG_START();
  UDPDEBUG_PRINTTXT("Board: ");
  UDPDEBUG_PRINTTXT(config.boardName);
  UDPDEBUG_PRINTTXT(" Firmware: ");
  UDPDEBUG_PRINTTXT(FIRMWARE);
  long h1 = ESP.getFreeHeap();
  UDPDEBUG_PRINT(" Heap ", h1);

  UDPDEBUG_PRINT(" relayStatus: ", relayStatus);
  UDPDEBUG_PRINT(" WifiCommand: ", wifiCommand);
  UDPDEBUG_PRINT(" relayStatus: ", relayStatus);
  UDPDEBUG_PRINT(" Delay: ", timeToOff);
  UDPDEBUG_SEND();
}

void handleStatus() {
  switch (relayStatus) {
    case COMMAND_ON:

      // ----- Exit
      if (wifiCommand == COMMAND_OFF || (timeToOff <= 0 && atoi(config.delayTime) != 0)) {
        switchRelay(RELAY_OFF);
      }
      break;

    case COMMAND_OFF:

      // ----- Exit
      if (wifiCommand == COMMAND_ON) {
        switchRelay(RELAY_ON);
        timeToOff = atoi(config.delayTime);
      }
      break;

    case COMMAND_STATUS:

      // ----- Exit
      break;
  }
}

bool handleWiFi() {
  server.begin();
  WiFiClient client = server.available();
  if (!client) return false;
  else {
    DEBUG_PRINTLN("new client");
    int timeOut = 10000;
    while (!client.available() && timeOut-- > 0) delay(1);
    if (timeOut <= 0) return false;
    else {
      // Read the first line of the request
      String request = client.readStringUntil('\r');
      DEBUG_PRINT("Request "); DEBUG_PRINTLN(request);
      client.flush();

      // Match the request
      if (request.indexOf("/SWITCH=ON") != -1) {
        timeToOff = atoi(config.delayTime);
        wifiCommand = COMMAND_ON;
        UDPDEBUG_START();
        DEBUG_PRINTLN("ON request received");
        UDPDEBUG_PRINTTXT("ON request received ");
        UDPDEBUG_SEND();
      }
      if (request.indexOf("/SWITCH=OFF") != -1) {
        wifiCommand = COMMAND_OFF;
        UDPDEBUG_START();
        DEBUG_PRINTLN("OFF request received");
        UDPDEBUG_PRINTTXT("OFF request received ");
        UDPDEBUG_SEND();
      }
      if (request.indexOf("/STATUS") != -1) {
        wifiCommand = COMMAND_STATUS;
        UDPDEBUG_START();
        DEBUG_PRINTLN("Status request received");
        UDPDEBUG_PRINTTXT("STATUS request received ");
        UDPDEBUG_SEND();
      }
      handleStatus();  // define next status
      // Return the response
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println(""); //  do not forget this one
      client.println("<!DOCTYPE HTML>");
      client.println("<html><h2>");
      client.print("BOARD: ");
      client.print(config.boardName);
      client.print(" STATUS: ");
      if (relayStatus == RELAY_ON) {
        client.print("On ");
        client.print(timeToOff);
        client.print(" sec");
      }
      else client.println("Off ");

      client.println("<br><br>");
      client.println("Click <a href=\"/SWITCH=ON\">here</a> turn the SWITCH on pin 12 ON<br>");
      client.println("Click <a href=\"/SWITCH=OFF\">here</a> turn the SWITCH on pin 12 OFF<br>");
      client.println("Click <a href=\"/STATUS\">here</a> get status<br>");
      client.println("</h2></html>");
      delay(1);
      DEBUG_PRINTLN("Client disconnected");
      DEBUG_PRINTLN("");
      return true;
    }
  }
}

void switchRelay(bool state) {
  if (state) {
    DEBUG_PRINTLN("Switch On ");
    UDPDEBUG_START();
    UDPDEBUG_PRINTTXT("Switch On ");
    UDPDEBUG_SEND();
    LEDswitch(Green);
    digitalWrite(RELAYPIN, HIGH);
    relayStatus = RELAY_ON;
  } else {
    DEBUG_PRINT("Switch Off ");
    UDPDEBUG_START();
    UDPDEBUG_PRINTTXT("Switch Off ");
    UDPDEBUG_SEND();
    LEDswitch(None);
    digitalWrite(RELAYPIN, LOW);
    relayStatus = RELAY_OFF;
  }
}



void readFullConfiguration() {
  readConfig();  // configuration in EEPROM
  // insert NEW CONSTANTS according switchName1 example
}


bool readRTCmem() {
  bool ret = true;
  system_rtc_mem_read(RTCMEMBEGIN, &rtcMem, sizeof(rtcMem));
  if (rtcMem.markerFlag != MAGICBYTE) {
    rtcMem.markerFlag = MAGICBYTE;
    rtcMem.bootTimes = 0;
    system_rtc_mem_write(RTCMEMBEGIN, &rtcMem, sizeof(rtcMem));
    ret = false;
  }
  return ret;
}

void printRTCmem() {
  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("rtcMem ");
  DEBUG_PRINT("markerFlag ");
  DEBUG_PRINTLN(rtcMem.markerFlag);
  DEBUG_PRINT("bootTimes ");
  DEBUG_PRINTLN(rtcMem.bootTimes);
}


