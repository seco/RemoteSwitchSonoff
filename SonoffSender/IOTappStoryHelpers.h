void initialize() {  // this function is called by IOTappstory() before return. Here, you put a safe startup configuration

  loopStatus = POWER_OFF;  // command off for startup of device
}

void configESP() {
  Serial.begin(115200);
  for (int i = 0; i < 5; i++) DEBUG_PRINTLN("");
  DEBUG_PRINTLN("Start "FIRMWARE);

  // ----------- PINS ----------------
#ifdef LEDgreen
  pinMode(LEDgreen, OUTPUT);
  digitalWrite(LEDgreen, LEDOFF);
#endif
#ifdef LEDred
  pinMode(LEDred, OUTPUT);
  digitalWrite(LEDred, LEDOFF);
#endif


  // ------------- INTERRUPTS ----------------------------
  blink.detach();


  //------------- LED and DISPLAYS ------------------------
  LEDswitch(GreenFastBlink);

  readFullConfiguration();
  connectNetwork();

  DEBUG_PRINTLN("------------- Configuration Mode -------------------");
  sendSysLogMessage(6, 1, config.boardName, FIRMWARE, 10, counter++, "------------- Configuration Mode -------------------");

  initWiFiManager();


  //--------------- LOOP ----------------------------------

  while (1) {
    yield();
    loopWiFiManager();
  }
}



void loopWiFiManager() {

  // Standard
  WiFiManagerParameter p_boardName("boardName", "boardName", config.boardName, STRUCT_CHAR_ARRAY_SIZE);
  WiFiManagerParameter p_IOTappStory1("IOTappStory1", "IOTappStory1", config.IOTappStory1, STRUCT_CHAR_ARRAY_SIZE);
  WiFiManagerParameter p_IOTappStoryPHP1("IOTappStoryPHP1", "IOTappStoryPHP1", config.IOTappStoryPHP1, STRUCT_CHAR_ARRAY_SIZE);
  WiFiManagerParameter p_IOTappStory2("IOTappStory2", "IOTappStory2", config.IOTappStory2, STRUCT_CHAR_ARRAY_SIZE);
  WiFiManagerParameter p_IOTappStoryPHP2("IOTappStoryPHP2", "IOTappStoryPHP2", config.IOTappStoryPHP2, STRUCT_CHAR_ARRAY_SIZE);
  WiFiManagerParameter p_automaticUpdate("automaticUpdate", "automaticUpdate", config.automaticUpdate, 2);

  //add all parameters here
  WiFiManagerParameter p_switchName1("switchName1", "switchName1", config.switchName1, STRUCT_CHAR_ARRAY_SIZE);
  WiFiManagerParameter p_switchName2("switchName2", "switchName2", config.switchName2, STRUCT_CHAR_ARRAY_SIZE);


  // Just a quick hint
  WiFiManagerParameter p_hint("<small>*Hint: if you want to reuse the currently active WiFi credentials, leave SSID and Password fields empty</small>");

  // Initialize WiFIManager
  WiFiManager wifiManager;
  wifiManager.addParameter(&p_hint);
  wifiManager.addParameter(&p_boardName);

  //add all parameters here
  wifiManager.addParameter(&p_switchName1);
  wifiManager.addParameter(&p_switchName2);

  // Standard
  wifiManager.addParameter(&p_IOTappStory1);
  wifiManager.addParameter(&p_IOTappStoryPHP1);
  wifiManager.addParameter(&p_IOTappStory2);
  wifiManager.addParameter(&p_IOTappStoryPHP2);
  wifiManager.addParameter(&p_automaticUpdate);


  // Sets timeout in seconds until configuration portal gets turned off.
  // If not specified device will remain in configuration mode until
  // switched off via webserver or device is restarted.
  wifiManager.setConfigPortalTimeout(600);

  // It starts an access point
  // and goes into a blocking loop awaiting configuration.
  // Once the user leaves the portal with the exit button
  // processing will continue
  if (!wifiManager.startConfigPortal(config.boardName)) {
    DEBUG_PRINTLN("Not connected to WiFi but continuing anyway.");
  } else {
    // If you get here you have connected to the WiFi
    DEBUG_PRINTLN("Connected... :-)");
  }
  // Getting posted form values and overriding local variables parameters
  // Config file is written


  // Standard
  strcpy(config.boardName, p_boardName.getValue());
  strcpy(config.IOTappStory1, p_IOTappStory1.getValue());
  strcpy(config.IOTappStoryPHP1, p_IOTappStoryPHP1.getValue());
  strcpy(config.IOTappStory2, p_IOTappStory2.getValue());
  strcpy(config.IOTappStoryPHP2, p_IOTappStoryPHP2.getValue());
  strcpy(config.automaticUpdate, p_automaticUpdate.getValue());

  //add all parameters here
  strcpy(config.switchName2, p_switchName2.getValue());
  strcpy(config.switchName1, p_switchName1.getValue());

  writeConfig();
  readFullConfiguration();  // read back to fill all variables

  LEDswitch(None); // Turn LED off as we are not in configuration mode.

  espRestart('N', "Configuration finished"); //Normal Operation
}
