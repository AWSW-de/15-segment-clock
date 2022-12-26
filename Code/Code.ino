// ###########################################################################################################################################
// #
// # 15-Segment clock using a 16x16 LED matrix project: https://www.printables.com/de/model/345319-15-segment-clock
// #
// # Code by https://github.com/AWSW-de
// #
// # Released under license: GNU General Public License v3.0: https://github.com/AWSW-de/15-segment-clock/blob/main/LICENSE
// #
// ###########################################################################################################################################
/*


   _  ______            ____                                              __                ___                   __               
 /' \/\  ___\          /\  _`\                                           /\ \__            /\_ \                 /\ \              
/\_, \ \ \__/          \ \,\L\_\     __     __     ___ ___      __    ___\ \ ,_\        ___\//\ \     ___     ___\ \ \/'\          
\/_/\ \ \___``\  _______\/_\__ \   /'__`\ /'_ `\ /' __` __`\  /'__`\/' _ `\ \ \/       /'___\\ \ \   / __`\  /'___\ \ , <          
   \ \ \/\ \L\ \/\______\ /\ \L\ \/\  __//\ \L\ \/\ \/\ \/\ \/\  __//\ \/\ \ \ \_     /\ \__/ \_\ \_/\ \L\ \/\ \__/\ \ \\`\        
    \ \_\ \____/\/______/ \ `\____\ \____\ \____ \ \_\ \_\ \_\ \____\ \_\ \_\ \__\    \ \____\/\____\ \____/\ \____\\ \_\ \_\      
     \/_/\/___/            \/_____/\/____/\/___L\ \/_/\/_/\/_/\/____/\/_/\/_/\/__/     \/____/\/____/\/___/  \/____/ \/_/\/_/      
                                            /\____/                                                                                
                                            \_/__/                                                                                 


*/
// ###########################################################################################################################################
// # Includes:
// #
// # You will need to add the following libraries to your Arduino IDE to use the project:
// # - Adafruit NeoPixel      // by Adafruit:                     https://github.com/adafruit/Adafruit_NeoPixel
// # - WiFiManager            // by tablatronix / tzapu:          https://github.com/tzapu/WiFiManager
// # - AsyncTCP               // by me-no-dev:                    https://github.com/me-no-dev/AsyncTCP
// # - ESPAsyncWebServer      // by me-no-dev:                    https://github.com/me-no-dev/ESPAsyncWebServer
// # - ESPUI                  // by s00500:                       https://github.com/s00500/ESPUI
// # - ArduinoJson            // by bblanchon:                    https://github.com/bblanchon/ArduinoJson
// # - LITTLEFS               // by lorol:                        https://github.com/lorol/LITTLEFS
// #
// ###########################################################################################################################################
#include <WiFi.h>               // Used to connect the ESP32 to your WiFi
#include <WiFiManager.h>        // Used for the WiFi Manager option to be able to connect the clock to your WiFi without code changes
#include <Adafruit_NeoPixel.h>  // Used to drive the NeoPixel LEDs
#include "time.h"               // Used for NTP time requests
#include <AsyncTCP.h>           // Used for the internal web server
#include <ESPAsyncWebServer.h>  // Used for the internal web server
#include <DNSServer.h>          // Used for the internal web server
#include <ESPUI.h>              // Used for the internal web server
#include <Preferences.h>        // Used to save the configuration to the ESP32 flash
#include <WiFiClient.h>         // Used for update function
#include <WebServer.h>          // Used for update function
#include <Update.h>             // Used for update function
#include "settings.h"           // Settings are stored in a seperate file to make to code better readable and to be able to switch to other settings faster


// ###########################################################################################################################################
// # Version number of the code:
// ###########################################################################################################################################
const char* CLOCK_VERSION = "V1.0.1";


// ###########################################################################################################################################
// # Internal web server settings:
// ###########################################################################################################################################
AsyncWebServer server(80);  // Web server for config
WebServer ewserver(8080);   // Web server for extra words
WebServer updserver(2022);  // Web server for OTA updates
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 44, 1);
DNSServer dnsServer;


// ###########################################################################################################################################
// # Declartions and variables used in the functions:
// ###########################################################################################################################################
Preferences preferences;
int langLEDlayout;
int iHour = 0;
int iMinute = 0;
int iDay = 0;
int iMonth = 0;
bool updatedevice = true;
bool updatemode = false;
int WiFiManFix = 0;
String iStartTime = "Failed to obtain time on startup... Please restart...";
int currentTimeID;
int statusLabelID, statusNightModeID;
int redVal_back, greenVal_back, blueVal_back;
int redVal_time, greenVal_time, blueVal_time;
int intensity, intensity_day, intensity_night;
int usenightmode, day_time_start, day_time_stop, statusNightMode;
int useledtest, useshowip;
char* selectLang;
int ipdelay = 1000;


// ###########################################################################################################################################
// # Setup function that runs once at startup of the ESP:
// ###########################################################################################################################################
void setup() {
  Serial.begin(115200);
  delay(500);
  preferences.begin("clock", false);  // Init ESP32 flash
  Serial.println("######################################################################");
  Serial.print("# 15-Segment Clock startup of version: ");
  Serial.println(CLOCK_VERSION);
  Serial.println("######################################################################");
  getFlashValues();                 // Read settings from flash
  strip.begin();                    // Init the LEDs
  intensity = intensity_day;        // Set the intenity to day mode for startup
  strip.setBrightness(intensity);   // Set LED brightness
  DisplayTest();                    // Perform the LED test
  startup();                        // Run startup actions
  SetWLAN(strip.Color(0, 0, 255));  // Show WLAN text
  WIFI_login();                     // WiFiManager
  WiFiManager1stBootFix();          // WiFi Manager 1st connect fix
  ShowIPaddress();                  // Display the current IP-address
  configNTPTime();                  // NTP time setup
  setupWebInterface();              // Generate the configuration page
  update_display();                 // Update LED display
  //handleOTAupdate();                // Start the ESP32 OTA update server
  Serial.println("######################################################################");
  Serial.println("# Clock startup finished...");
  Serial.println("######################################################################");
}


// ###########################################################################################################################################
// # Loop function which runs all the time after the startup was done:
// ###########################################################################################################################################
void loop() {
  printLocalTime();
  if (updatedevice == true) {                // Allow display updates (normal usage)
    ESPUI.print(currentTimeID, iStartTime);  // Update web gui "Current Time" label
    update_display();                        // Update display (1x per minute regulary)
  }
  dnsServer.processNextRequest();  // Update web server
  //if (updatemode == true) updserver.handleClient();  // ESP32 OTA updates
}


// ###########################################################################################################################################
// # Setup the internal web server configuration page:
// ###########################################################################################################################################
void setupWebInterface() {
  dnsServer.start(DNS_PORT, "*", apIP);

  // Section General:
  // ################
  ESPUI.separator("General:");

  // Status label:
  statusLabelID = ESPUI.label("Status:", ControlColor::Dark, "Operational");

  // Clock version:
  ESPUI.label("Version", ControlColor::None, CLOCK_VERSION);

  // Time:
  currentTimeID = ESPUI.label("Time", ControlColor::Dark, "Will be updated every few seconds...");



  // Section LED settings:
  // #####################
  ESPUI.separator("LED settings:");

  // Time color selector:
  char hex_time[7] = { 0 };
  sprintf(hex_time, "#%02X%02X%02X", redVal_time, greenVal_time, blueVal_time);
  uint16_t text_colour_time;
  text_colour_time = ESPUI.text("Time", colCallTIME, ControlColor::Dark, hex_time);
  ESPUI.setInputType(text_colour_time, "color");

  // Background color selector:
  char hex_back[7] = { 0 };
  sprintf(hex_back, "#%02X%02X%02X", redVal_back, greenVal_back, blueVal_back);
  uint16_t text_colour_background;
  text_colour_background = ESPUI.text("Background", colCallBACK, ControlColor::Dark, hex_back);
  ESPUI.setInputType(text_colour_background, "color");

  // Intensity DAY slider selector: !!! DEFAULT LIMITED TO 128 of 255 !!!
  ESPUI.slider("Brightness during the day", &sliderBrightnessDay, ControlColor::Dark, intensity_day, 0, LEDintensityLIMIT);

  // Use night mode function:
  ESPUI.switcher("Use night mode to reduce brightness", &switchNightMode, ControlColor::Dark, usenightmode);

  // Night mode status:
  statusNightModeID = ESPUI.label("Night mode status", ControlColor::Dark, "Night mode not used");

  // Intensity NIGHT slider selector: !!! DEFAULT LIMITED TO 128 of 255 !!!
  ESPUI.slider("Brightness at night", &sliderBrightnessNight, ControlColor::Dark, intensity_night, 0, LEDintensityLIMIT);

  // Day mode start time:
  ESPUI.number("Day time starts at", call_day_time_start, ControlColor::Dark, day_time_start, 0, 11);

  // Day mode stop time:
  ESPUI.number("Day time ends after", call_day_time_stop, ControlColor::Dark, day_time_stop, 12, 23);

  // Set layout language:
  if (langLEDlayout == 0) selectLang = "Current layout language: German";
  if (langLEDlayout == 1) selectLang = "Current layout language: English";
  ESPUI.button(selectLang, &buttonlangChange, ControlColor::Dark, "Change layout language", (void*)4);



  // Section Startup:
  // ################
  ESPUI.separator("Startup:");

  // Startup LED test function:
  ESPUI.switcher("Show LED test on startup", &switchLEDTest, ControlColor::Dark, useledtest);

  // Show IP-address on startup:
  ESPUI.switcher("Show IP-address on startup", &switchShowIP, ControlColor::Dark, useshowip);



  // Section WiFi:
  // #############
  ESPUI.separator("WiFi:");

  // WiFi SSID:
  ESPUI.label("SSID", ControlColor::Dark, WiFi.SSID());

  // WiFi signal strength:
  ESPUI.label("Signal", ControlColor::Dark, String(WiFi.RSSI()) + "dBm");

  // Hostname:
  ESPUI.label("Hostname", ControlColor::Dark, hostname);

  // WiFi ip-address:
  ESPUI.label("IP-address", ControlColor::Dark, IpAddress2String(WiFi.localIP()));

  // WiFi MAC-address:
  ESPUI.label("MAC address", ControlColor::Dark, WiFi.macAddress());



  // Section Time settings:
  // ######################
  ESPUI.separator("Time settings:");

  // NTP server:
  ESPUI.label("NTP server", ControlColor::Dark, NTPserver);

  // Time zone:
  ESPUI.label("Time zone", ControlColor::Dark, Timezone);

  // Time:
  ESPUI.label("Startup time", ControlColor::Dark, iStartTime);


  /*
  // Section Update:
  // ###############
  ESPUI.separator("Update:");

  // Update clock:
  ESPUI.button("Activate update mode", &buttonUpdate, ControlColor::Dark, "Activate update mode", (void*)1);

  // Update URL
  ESPUI.label("Update URL", ControlColor::Dark, "http://" + IpAddress2String(WiFi.localIP()) + ":2022/ota");

  // Update User account
  ESPUI.label("Update account", ControlColor::Dark, "Username: Clock   /   Password: 16x16");
  */


  // Section Maintenance:
  // ####################
  ESPUI.separator("Maintenance:");

  // Restart clock:
  ESPUI.button("Restart", &buttonRestart, ControlColor::Dark, "Restart", (void*)1);

  // Reset WiFi settings:
  ESPUI.button("Reset WiFi settings", &buttonWiFiReset, ControlColor::Dark, "Reset WiFi settings", (void*)2);

  // Reset clock settings:
  ESPUI.button("Reset clock settings", &buttonclockReset, ControlColor::Dark, "Reset clock settings", (void*)3);



  // Update night mode status text on startup:
  if (usenightmode == 1) {
    if ((iHour <= day_time_stop) && (iHour >= day_time_start)) {
      ESPUI.print(statusNightModeID, "Day time");
    } else {
      ESPUI.print(statusNightModeID, "Night time");
    }
  }


  // Deploy the page:
  ESPUI.begin("15-Segment Clock");
}


// ###########################################################################################################################################
// # Read settings from flash:
// ###########################################################################################################################################
void getFlashValues() {
  langLEDlayout = preferences.getUInt("langLEDlayout", langLEDlayout_default);
  redVal_time = preferences.getUInt("redVal_time", redVal_time_default);
  greenVal_time = preferences.getUInt("greenVal_time", greenVal_time_default);
  blueVal_time = preferences.getUInt("blueVal_time", blueVal_time_default);
  redVal_back = preferences.getUInt("redVal_back", redVal_back_default);
  greenVal_back = preferences.getUInt("greenVal_back", greenVal_back_default);
  blueVal_back = preferences.getUInt("blueVal_back", blueVal_back_default);
  intensity_day = preferences.getUInt("intensity_day", intensity_day_default);
  intensity_night = preferences.getUInt("intensity_night", intensity_night_default);
  usenightmode = preferences.getUInt("usenightmode", usenightmode_default);
  day_time_start = preferences.getUInt("day_time_start", day_time_start_default);
  day_time_stop = preferences.getUInt("day_time_stop", day_time_stop_default);
  useledtest = preferences.getUInt("useledtest", useledtest_default);
  useshowip = preferences.getUInt("useshowip", useshowip_default);
}


// ###########################################################################################################################################
// # GUI: Reset the clock settings:
// ###########################################################################################################################################
int clockResetCounter = 0;
void buttonclockReset(Control* sender, int type, void* param) {
  updatedevice = false;
  Serial.println(String("param: ") + String(int(param)));
  switch (type) {
    case B_DOWN:
      ESPUI.print(statusLabelID, "CLOCK SETTINGS RESET REQUESTED");
      restartLED(strip.Color(255, 0, 0));
      delay(1000);
      break;
    case B_UP:
      if (clockResetCounter == 1) {
        Serial.println("Status: CLOCK SETTINGS RESET REQUEST EXECUTED");
        preferences.clear();
        delay(1000);
        preferences.putUInt("langLEDlayout", langLEDlayout_default);
        preferences.putUInt("redVal_time", redVal_time_default);
        preferences.putUInt("greenVal_time", greenVal_time_default);
        preferences.putUInt("blueVal_time", blueVal_time_default);
        preferences.putUInt("redVal_back", redVal_back_default);
        preferences.putUInt("greenVal_back", greenVal_back_default);
        preferences.putUInt("blueVal_back", blueVal_back_default);
        preferences.putUInt("intensity_day", intensity_day_default);
        preferences.putUInt("intensity_night", intensity_night_default);
        preferences.putUInt("useledtest", useledtest_default);
        preferences.putUInt("useshowip", useshowip_default);
        preferences.putUInt("usenightmode", usenightmode_default);
        preferences.putUInt("day_time_stop", day_time_stop_default);
        preferences.putUInt("day_time_stop", day_time_stop_default);
        delay(1000);
        preferences.end();
        restartLED(strip.Color(0, 255, 0));
        delay(1000);
        Serial.println("############################################################################################");
        Serial.println("# CLOCK SETTING WERE SET TO DEFAULT... CLOCK WILL NOW RESTART... PLEASE CONFIGURE AGAIN... #");
        Serial.println("############################################################################################");
        delay(1000);
        ESP.restart();
      } else {
        Serial.println("Status: CLOCK SETTINGS RESET REQUEST");
        ESPUI.updateButton(sender->id, "! Press button once more to apply settings reset !");
        clockResetCounter = clockResetCounter + 1;
      }
      break;
  }
}


// ###########################################################################################################################################
// # GUI: Change LED layout language:
// ###########################################################################################################################################
int langChangeCounter = 0;
void buttonlangChange(Control* sender, int type, void* param) {
  updatedevice = false;
  Serial.println(String("param: ") + String(int(param)));
  switch (type) {
    case B_DOWN:
      ESPUI.print(statusLabelID, "CLOCK LAYOUT LANGUAGE CHANGE REQUESTED");
      back_color();
      restartLED(strip.Color(255, 0, 0));
      delay(1000);
      break;
    case B_UP:
      if (langChangeCounter == 1) {
        Serial.println("CLOCK LAYOUT LANGUAGE CHANGE EXECUTED");
        if (langLEDlayout == 0) {  // Flip langLEDlayout setting DE to EN / EN to DE
          preferences.putUInt("langLEDlayout", 1);
        } else {
          preferences.putUInt("langLEDlayout", 0);
        }
        delay(1000);
        preferences.end();
        back_color();
        restartLED(strip.Color(0, 255, 0));
        delay(1000);
        Serial.println("##################################################################");
        Serial.println("# CLOCK LAYOUT LANGUAGE WAS CHANGED... CLOCK WILL NOW RESTART... #");
        Serial.println("##################################################################");
        delay(1000);
        ESP.restart();
      } else {
        Serial.println("CLOCK LAYOUT LANGUAGE CHANGE REQUESTED");
        ESPUI.updateButton(sender->id, "! Press button once more to apply the language change and restart !");
        langChangeCounter = langChangeCounter + 1;
      }
      break;
  }
}


// ###########################################################################################################################################
// # Show the IP-address on the display:
// ###########################################################################################################################################
void ShowIPaddress() {
  if (useshowip == 1) {
    Serial.println("Show current IP-address on the display: " + IpAddress2String(WiFi.localIP()));

    // Octet 1:
    back_color();
    numbers(getDigit(int(WiFi.localIP()[0]), 2), 1);
    numbers(getDigit(int(WiFi.localIP()[0]), 1), 2);
    numbers(getDigit(int(WiFi.localIP()[0]), 0), 3);
    setLED(97, 97, 1);
    setLED(0, 3, 1);
    setLED(240, 243, 1);

    // Octet 2:
    numbers(getDigit(int(WiFi.localIP()[1]), 2), 5);
    numbers(getDigit(int(WiFi.localIP()[1]), 1), 6);
    numbers(getDigit(int(WiFi.localIP()[1]), 0), 7);
    setLED(222, 222, 1);
    setLED(0, 7, 1);
    setLED(240, 247, 1);
    strip.show();
    delay(ipdelay);

    // Octet 3:
    back_color();
    numbers(getDigit(int(WiFi.localIP()[2]), 2), 1);
    numbers(getDigit(int(WiFi.localIP()[2]), 1), 2);
    numbers(getDigit(int(WiFi.localIP()[2]), 0), 3);
    setLED(97, 97, 1);
    setLED(0, 11, 1);
    setLED(240, 251, 1);

    // Octet 4:
    numbers(getDigit(int(WiFi.localIP()[3]), 2), 5);
    numbers(getDigit(int(WiFi.localIP()[3]), 1), 6);
    numbers(getDigit(int(WiFi.localIP()[3]), 0), 7);
    setLED(0, 15, 1);
    setLED(240, 255, 1);
    strip.show();
    delay(ipdelay);
  }
}


// ###########################################################################################################################################
// # Set the numbers on the display in each single row:
// ###########################################################################################################################################
void numbers(int wert, int segment) {

  //Serial.println(wert);

  switch (segment) {
    case 1:
      {
        switch (wert) {
          case 0:
            {
              setLED(45, 48, 1);
              setLED(50, 50, 1);
              setLED(77, 77, 1);
              setLED(79, 80, 1);
              setLED(82, 82, 1);
              setLED(109, 111, 1);
              break;
            }
          case 1:
            {
              setLED(45, 45, 1);
              setLED(50, 50, 1);
              setLED(77, 77, 1);
              setLED(82, 82, 1);
              setLED(109, 109, 1);
              break;
            }
          case 2:
            {
              setLED(45, 47, 1);
              setLED(50, 50, 1);
              setLED(77, 79, 1);
              setLED(80, 80, 1);
              setLED(109, 111, 1);
              break;
            }
          case 3:
            {
              setLED(45, 47, 1);
              setLED(50, 50, 1);
              setLED(77, 78, 1);
              setLED(82, 82, 1);
              setLED(109, 111, 1);
              break;
            }
          case 4:
            {
              setLED(47, 48, 1);
              setLED(77, 79, 1);
              setLED(45, 45, 1);
              setLED(50, 50, 1);
              setLED(82, 82, 1);
              setLED(109, 109, 1);
              break;
            }
          case 5:
            {
              setLED(45, 48, 1);
              setLED(77, 79, 1);
              setLED(82, 82, 1);
              setLED(109, 111, 1);
              break;
            }
          case 6:
            {
              setLED(45, 48, 1);
              setLED(77, 80, 1);
              setLED(82, 82, 1);
              setLED(109, 111, 1);
              break;
            }
          case 7:
            {
              setLED(45, 47, 1);
              setLED(50, 50, 1);
              setLED(77, 77, 1);
              setLED(82, 82, 1);
              setLED(109, 109, 1);
              break;
            }
          case 8:
            {
              setLED(45, 48, 1);
              setLED(50, 50, 1);
              setLED(77, 80, 1);
              setLED(82, 82, 1);
              setLED(109, 111, 1);
              break;
              break;
            }
          case 9:
            {
              setLED(45, 48, 1);
              setLED(50, 50, 1);
              setLED(77, 79, 1);
              setLED(82, 82, 1);
              setLED(109, 111, 1);
              break;
            }
        }
        break;
      }

    case 2:
      {
        switch (wert) {
          case 0:
            {
              setLED(41, 43, 1);
              setLED(52, 52, 1);
              setLED(54, 54, 1);
              setLED(73, 73, 1);
              setLED(75, 75, 1);
              setLED(84, 84, 1);
              setLED(86, 86, 1);
              setLED(105, 107, 1);
              break;
            }
          case 1:
            {
              setLED(41, 41, 1);
              setLED(54, 54, 1);
              setLED(73, 73, 1);
              setLED(86, 86, 1);
              setLED(105, 105, 1);
              break;
            }
          case 2:
            {
              setLED(41, 43, 1);
              setLED(54, 54, 1);
              setLED(73, 75, 1);
              setLED(84, 84, 1);
              setLED(105, 107, 1);
              break;
            }
          case 3:
            {
              setLED(41, 43, 1);
              setLED(54, 54, 1);
              setLED(73, 74, 1);
              setLED(86, 86, 1);
              setLED(105, 107, 1);
              break;
            }
          case 4:
            {
              setLED(41, 41, 1);
              setLED(43, 43, 1);
              setLED(52, 52, 1);
              setLED(54, 54, 1);
              setLED(73, 75, 1);
              setLED(86, 86, 1);
              setLED(105, 105, 1);
              break;
            }
          case 5:
            {
              setLED(41, 43, 1);
              setLED(52, 52, 1);
              setLED(73, 75, 1);
              setLED(86, 86, 1);
              setLED(105, 107, 1);
              break;
            }
          case 6:
            {
              setLED(41, 43, 1);
              setLED(52, 52, 1);
              setLED(73, 75, 1);
              setLED(84, 84, 1);
              setLED(86, 86, 1);
              setLED(105, 107, 1);
              break;
            }
          case 7:
            {
              setLED(41, 43, 1);
              setLED(54, 54, 1);
              setLED(73, 73, 1);
              setLED(86, 86, 1);
              setLED(105, 105, 1);
              break;
            }
          case 8:
            {
              setLED(41, 43, 1);
              setLED(52, 52, 1);
              setLED(54, 54, 1);
              setLED(73, 75, 1);
              setLED(84, 84, 1);
              setLED(86, 86, 1);
              setLED(105, 107, 1);
              break;
            }
          case 9:
            {
              setLED(41, 43, 1);
              setLED(52, 52, 1);
              setLED(54, 54, 1);
              setLED(73, 75, 1);
              setLED(86, 86, 1);
              setLED(105, 107, 1);
              break;
            }
        }
        break;
      }



    case 3:
      {
        switch (wert) {
          case 0:
            {
              setLED(36, 38, 1);
              setLED(57, 57, 1);
              setLED(59, 59, 1);
              setLED(68, 68, 1);
              setLED(70, 70, 1);
              setLED(89, 89, 1);
              setLED(91, 91, 1);
              setLED(100, 102, 1);
              break;
            }
          case 1:
            {
              setLED(36, 36, 1);
              setLED(59, 59, 1);
              setLED(68, 68, 1);
              setLED(91, 91, 1);
              setLED(100, 100, 1);
              break;
            }
          case 2:
            {
              setLED(36, 38, 1);
              setLED(59, 59, 1);
              setLED(68, 70, 1);
              setLED(89, 89, 1);
              setLED(100, 102, 1);
              break;
            }
          case 3:
            {
              setLED(36, 38, 1);
              setLED(59, 59, 1);
              setLED(68, 69, 1);
              setLED(91, 91, 1);
              setLED(100, 102, 1);
              break;
            }
          case 4:
            {
              setLED(36, 36, 1);
              setLED(38, 38, 1);
              setLED(57, 57, 1);
              setLED(59, 59, 1);
              setLED(68, 70, 1);
              setLED(91, 91, 1);
              setLED(100, 100, 1);
              break;
            }
          case 5:
            {
              setLED(36, 38, 1);
              setLED(57, 57, 1);
              setLED(68, 70, 1);
              setLED(91, 91, 1);
              setLED(100, 102, 1);
              break;
            }
          case 6:
            {
              setLED(36, 38, 1);
              setLED(57, 57, 1);
              setLED(68, 70, 1);
              setLED(89, 89, 1);
              setLED(91, 91, 1);
              setLED(100, 102, 1);
              break;
            }
          case 7:
            {
              setLED(36, 38, 1);
              setLED(59, 59, 1);
              setLED(68, 68, 1);
              setLED(91, 91, 1);
              setLED(100, 100, 1);
              break;
            }
          case 8:
            {
              setLED(36, 38, 1);
              setLED(57, 57, 1);
              setLED(59, 59, 1);
              setLED(68, 70, 1);
              setLED(89, 89, 1);
              setLED(91, 91, 1);
              setLED(100, 102, 1);
              break;
            }
          case 9:
            {
              setLED(36, 38, 1);
              setLED(57, 57, 1);
              setLED(59, 59, 1);
              setLED(68, 70, 1);
              setLED(91, 91, 1);
              setLED(100, 102, 1);
              break;
            }
        }
        break;
      }

    case 4:
      {
        switch (wert) {
          case 0:
            {
              setLED(32, 34, 1);
              setLED(61, 61, 1);
              setLED(63, 63, 1);
              setLED(64, 64, 1);
              setLED(66, 66, 1);
              setLED(93, 93, 1);
              setLED(95, 95, 1);
              setLED(96, 98, 1);
              break;
            }
          case 1:
            {
              setLED(32, 32, 1);
              setLED(63, 63, 1);
              setLED(64, 64, 1);
              setLED(95, 95, 1);
              setLED(96, 96, 1);
              break;
            }
          case 2:
            {
              setLED(32, 34, 1);
              setLED(63, 63, 1);
              setLED(64, 66, 1);
              setLED(93, 93, 1);
              setLED(96, 98, 1);
              break;
            }
          case 3:
            {
              setLED(32, 34, 1);
              setLED(63, 63, 1);
              setLED(64, 65, 1);
              setLED(95, 95, 1);
              setLED(96, 98, 1);
              break;
            }
          case 4:
            {
              setLED(32, 32, 1);
              setLED(34, 34, 1);
              setLED(61, 61, 1);
              setLED(63, 63, 1);
              setLED(64, 66, 1);
              setLED(95, 95, 1);
              setLED(96, 96, 1);
              break;
            }
          case 5:
            {
              setLED(32, 34, 1);
              setLED(61, 61, 1);
              setLED(64, 66, 1);
              setLED(95, 95, 1);
              setLED(96, 98, 1);
              break;
            }
          case 6:
            {
              setLED(32, 34, 1);
              setLED(61, 61, 1);
              setLED(64, 66, 1);
              setLED(93, 93, 1);
              setLED(95, 95, 1);
              setLED(96, 98, 1);
              break;
            }
          case 7:
            {
              setLED(32, 34, 1);
              setLED(63, 63, 1);
              setLED(64, 64, 1);
              setLED(95, 95, 1);
              setLED(96, 96, 1);
              break;
            }
          case 8:
            {
              setLED(32, 34, 1);
              setLED(61, 61, 1);
              setLED(63, 63, 1);
              setLED(64, 66, 1);
              setLED(93, 93, 1);
              setLED(95, 95, 1);
              setLED(96, 98, 1);
              break;
            }
          case 9:
            {
              setLED(32, 34, 1);
              setLED(61, 61, 1);
              setLED(63, 63, 1);
              setLED(64, 66, 1);
              setLED(95, 95, 1);
              setLED(96, 98, 1);
              break;
            }
        }
        break;
      }


    case 5:
      {
        switch (wert) {
          case 0:
            {
              setLED(144, 146, 1);
              setLED(173, 173, 1);
              setLED(175, 175, 1);
              setLED(176, 176, 1);
              setLED(178, 178, 1);
              setLED(205, 205, 1);
              setLED(207, 207, 1);
              setLED(208, 210, 1);
              break;
            }
          case 1:
            {
              setLED(146, 146, 1);
              setLED(173, 173, 1);
              setLED(178, 178, 1);
              setLED(205, 205, 1);
              setLED(210, 210, 1);
              break;
            }
          case 2:
            {
              setLED(144, 146, 1);
              setLED(173, 173, 1);
              setLED(176, 178, 1);
              setLED(207, 207, 1);
              setLED(208, 210, 1);
              break;
            }
          case 3:
            {
              setLED(144, 146, 1);
              setLED(173, 173, 1);
              setLED(177, 178, 1);
              setLED(205, 205, 1);
              setLED(208, 210, 1);
              break;
            }
          case 4:
            {
              setLED(144, 144, 1);
              setLED(146, 146, 1);
              setLED(173, 173, 1);
              setLED(175, 175, 1);
              setLED(176, 178, 1);
              setLED(205, 205, 1);
              setLED(210, 210, 1);
              break;
            }
          case 5:
            {
              setLED(144, 146, 1);
              setLED(175, 175, 1);
              setLED(176, 178, 1);
              setLED(205, 205, 1);
              setLED(208, 210, 1);
              break;
            }
          case 6:
            {
              setLED(144, 146, 1);
              setLED(175, 175, 1);
              setLED(176, 178, 1);
              setLED(205, 205, 1);
              setLED(207, 207, 1);
              setLED(208, 210, 1);
              break;
            }
          case 7:
            {
              setLED(144, 146, 1);
              setLED(173, 173, 1);
              setLED(178, 178, 1);
              setLED(205, 205, 1);
              setLED(210, 210, 1);
              break;
            }
          case 8:
            {
              setLED(144, 146, 1);
              setLED(173, 173, 1);
              setLED(175, 175, 1);
              setLED(176, 178, 1);
              setLED(205, 205, 1);
              setLED(207, 207, 1);
              setLED(208, 210, 1);
              break;
            }
          case 9:
            {
              setLED(144, 146, 1);
              setLED(173, 173, 1);
              setLED(175, 175, 1);
              setLED(176, 178, 1);
              setLED(205, 205, 1);
              setLED(208, 210, 1);
              break;
            }
        }
        break;
      }


    case 6:
      {
        switch (wert) {
          case 0:
            {
              setLED(148, 150, 1);
              setLED(169, 169, 1);
              setLED(171, 171, 1);
              setLED(180, 180, 1);
              setLED(182, 182, 1);
              setLED(201, 201, 1);
              setLED(203, 203, 1);
              setLED(212, 214, 1);
              break;
            }
          case 1:
            {
              setLED(150, 150, 1);
              setLED(169, 169, 1);
              setLED(182, 182, 1);
              setLED(201, 201, 1);
              setLED(214, 214, 1);
              break;
            }
          case 2:
            {
              setLED(148, 150, 1);
              setLED(169, 169, 1);
              setLED(180, 182, 1);
              setLED(203, 203, 1);
              setLED(212, 214, 1);
              break;
            }
          case 3:
            {
              setLED(148, 150, 1);
              setLED(169, 169, 1);
              setLED(181, 182, 1);
              setLED(201, 201, 1);
              setLED(212, 214, 1);
              break;
            }
          case 4:
            {
              setLED(148, 148, 1);
              setLED(150, 150, 1);
              setLED(169, 169, 1);
              setLED(171, 171, 1);
              setLED(180, 182, 1);
              setLED(201, 201, 1);
              setLED(214, 214, 1);
              break;
            }
          case 5:
            {
              setLED(148, 150, 1);
              setLED(171, 171, 1);
              setLED(180, 182, 1);
              setLED(201, 201, 1);
              setLED(212, 214, 1);
              break;
            }
          case 6:
            {
              setLED(148, 150, 1);
              setLED(171, 171, 1);
              setLED(180, 182, 1);
              setLED(201, 201, 1);
              setLED(203, 203, 1);
              setLED(212, 214, 1);
              break;
            }
          case 7:
            {
              setLED(148, 150, 1);
              setLED(169, 169, 1);
              setLED(182, 182, 1);
              setLED(201, 201, 1);
              setLED(214, 214, 1);
              break;
            }
          case 8:
            {
              setLED(148, 150, 1);
              setLED(169, 169, 1);
              setLED(171, 171, 1);
              setLED(180, 182, 1);
              setLED(201, 201, 1);
              setLED(203, 203, 1);
              setLED(212, 214, 1);
              break;
            }
          case 9:
            {
              setLED(148, 150, 1);
              setLED(169, 169, 1);
              setLED(171, 171, 1);
              setLED(180, 182, 1);
              setLED(201, 201, 1);
              setLED(212, 214, 1);
              break;
            }
        }
        break;
      }


    case 7:
      {
        switch (wert) {
          case 0:
            {
              setLED(153, 155, 1);
              setLED(164, 164, 1);
              setLED(166, 166, 1);
              setLED(185, 185, 1);
              setLED(187, 187, 1);
              setLED(196, 196, 1);
              setLED(198, 198, 1);
              setLED(217, 219, 1);
              break;
            }
          case 1:
            {
              setLED(155, 155, 1);
              setLED(164, 164, 1);
              setLED(187, 187, 1);
              setLED(196, 196, 1);
              setLED(219, 219, 1);
              break;
            }
          case 2:
            {
              setLED(153, 155, 1);
              setLED(164, 164, 1);
              setLED(185, 187, 1);
              setLED(198, 198, 1);
              setLED(217, 219, 1);
              break;
            }
          case 3:
            {
              setLED(153, 155, 1);
              setLED(164, 164, 1);
              setLED(186, 187, 1);
              setLED(196, 196, 1);
              setLED(217, 219, 1);
              break;
            }
          case 4:
            {
              setLED(153, 153, 1);
              setLED(155, 155, 1);
              setLED(164, 164, 1);
              setLED(166, 166, 1);
              setLED(185, 187, 1);
              setLED(196, 196, 1);
              setLED(219, 219, 1);
              break;
            }
          case 5:
            {
              setLED(153, 155, 1);
              setLED(166, 166, 1);
              setLED(185, 187, 1);
              setLED(196, 196, 1);
              setLED(217, 219, 1);
              break;
            }
          case 6:
            {
              setLED(153, 155, 1);
              setLED(166, 166, 1);
              setLED(185, 187, 1);
              setLED(196, 196, 1);
              setLED(198, 198, 1);
              setLED(217, 219, 1);
              break;
            }
          case 7:
            {
              setLED(153, 155, 1);
              setLED(164, 164, 1);
              setLED(187, 187, 1);
              setLED(196, 196, 1);
              setLED(219, 219, 1);
              break;
            }
          case 8:
            {
              setLED(153, 155, 1);
              setLED(164, 164, 1);
              setLED(166, 166, 1);
              setLED(185, 187, 1);
              setLED(196, 196, 1);
              setLED(198, 198, 1);
              setLED(217, 219, 1);
              break;
            }
          case 9:
            {
              setLED(153, 155, 1);
              setLED(164, 164, 1);
              setLED(166, 166, 1);
              setLED(185, 187, 1);
              setLED(196, 196, 1);
              setLED(217, 219, 1);
              break;
            }
        }
        break;
      }

    case 8:
      {
        switch (wert) {
          case 0:
            {
              setLED(157, 159, 1);
              setLED(160, 160, 1);
              setLED(162, 162, 1);
              setLED(189, 189, 1);
              setLED(191, 191, 1);
              setLED(192, 192, 1);
              setLED(194, 194, 1);
              setLED(221, 223, 1);
              break;
            }
          case 1:
            {
              setLED(159, 159, 1);
              setLED(160, 160, 1);
              setLED(191, 191, 1);
              setLED(192, 192, 1);
              setLED(223, 223, 1);
              break;
            }
          case 2:
            {
              setLED(157, 159, 1);
              setLED(160, 160, 1);
              setLED(189, 191, 1);
              setLED(194, 194, 1);
              setLED(221, 223, 1);
              break;
            }
          case 3:
            {
              setLED(157, 159, 1);
              setLED(160, 160, 1);
              setLED(190, 191, 1);
              setLED(192, 192, 1);
              setLED(221, 223, 1);
              break;
            }
          case 4:
            {
              setLED(157, 157, 1);
              setLED(159, 159, 1);
              setLED(160, 160, 1);
              setLED(162, 162, 1);
              setLED(189, 191, 1);
              setLED(192, 192, 1);
              setLED(223, 223, 1);
              break;
            }
          case 5:
            {
              setLED(157, 159, 1);
              setLED(162, 162, 1);
              setLED(189, 191, 1);
              setLED(192, 192, 1);
              setLED(221, 223, 1);
              break;
            }
          case 6:
            {
              setLED(157, 159, 1);
              setLED(162, 162, 1);
              setLED(189, 191, 1);
              setLED(192, 192, 1);
              setLED(194, 194, 1);
              setLED(221, 223, 1);
              break;
            }
          case 7:
            {
              setLED(157, 159, 1);
              setLED(160, 160, 1);
              setLED(191, 191, 1);
              setLED(192, 192, 1);
              setLED(223, 223, 1);
              break;
            }
          case 8:
            {
              setLED(157, 159, 1);
              setLED(160, 160, 1);
              setLED(162, 162, 1);
              setLED(189, 191, 1);
              setLED(192, 192, 1);
              setLED(194, 194, 1);
              setLED(221, 223, 1);
              break;
            }
          case 9:
            {
              setLED(157, 159, 1);
              setLED(160, 160, 1);
              setLED(162, 162, 1);
              setLED(189, 191, 1);
              setLED(192, 192, 1);
              setLED(221, 223, 1);
              break;
            }
        }
        break;
      }
  }
}


// ###########################################################################################################################################
// # Get a digit from a number at position pos: (Split IP-address octets in single digits)
// ###########################################################################################################################################
int getDigit(int number, int pos) {
  return (pos == 0) ? number % 10 : getDigit(number / 10, --pos);
}


// ###########################################################################################################################################
// # GUI: Restart the clock:
// ###########################################################################################################################################
int ResetCounter = 0;
void buttonRestart(Control* sender, int type, void* param) {
  updatedevice = false;
  preferences.end();
  delay(1000);
  Serial.println(String("param: ") + String(int(param)));
  switch (type) {
    case B_DOWN:
      ESPUI.print(statusLabelID, "Restart requested");
      back_color();
      restartLED(strip.Color(255, 0, 0));
      strip.show();
      break;
    case B_UP:
      if (ResetCounter == 1) {
        back_color();
        restartLED(strip.Color(0, 255, 0));
        strip.show();
        Serial.println("Status: Restart executed");
        delay(1000);
        ESP.restart();
      } else {
        Serial.println("Status: Restart request");
        ESPUI.updateButton(sender->id, "! Press button once more to apply restart !");
        ResetCounter = ResetCounter + 1;
      }
      break;
  }
}


// ###########################################################################################################################################
// # GUI: Reset the WiFi settings of the clock:
// ###########################################################################################################################################
int WIFIResetCounter = 0;
void buttonWiFiReset(Control* sender, int type, void* param) {
  updatedevice = false;
  delay(1000);
  switch (type) {
    case B_DOWN:
      if (WIFIResetCounter == 0) {
        back_color();
        SetWLAN(strip.Color(255, 0, 0));
        ESPUI.print(statusLabelID, "WIFI SETTINGS RESET REQUESTED");
        delay(1000);
        preferences.putUInt("WiFiManFix", 0);  // WiFi Manager Fix Reset
        delay(1000);
        preferences.end();
        delay(1000);
      }
      break;
    case B_UP:
      if (WIFIResetCounter == 1) {
        Serial.println("Status: WIFI SETTINGS RESET REQUEST EXECUTED");
        delay(1000);
        WiFi.disconnect();
        delay(1000);
        WiFiManager manager;
        delay(1000);
        manager.resetSettings();
        delay(1000);
        Serial.println("################################################################################################");
        Serial.println("# WIFI SETTING WERE SET TO DEFAULT... CLOCK WILL NOW RESTART... PLEASE CONFIGURE WIFI AGAIN... #");
        Serial.println("################################################################################################");
        delay(1000);
        ESP.restart();
      } else {
        Serial.println("Status: WIFI SETTINGS RESET REQUEST");
        ESPUI.updateButton(sender->id, "! Press button once more to apply WiFi reset !");
        WIFIResetCounter = WIFIResetCounter + 1;
      }
      break;
  }
}


// ###########################################################################################################################################
// # GUI: Update the clock:
// ###########################################################################################################################################
/*void buttonUpdate(Control* sender, int type, void* param) {
  preferences.end();
  updatedevice = false;
  updatemode = true;
  delay(1000);
  Serial.println(String("param: ") + String(int(param)));
  switch (type) {
    case B_DOWN:
      Serial.println("Status: Update request");
      ESPUI.print(statusLabelID, "Update requested");
      back_color();
      redVal_time = 0;
      greenVal_time = 0;
      blueVal_time = 255;
      if (langLEDlayout == 0) {  // DE:
        setLED(52, 57, 1);
      }
      if (langLEDlayout == 1) {  // EN:
        setLED(48, 53, 1);
      }
      strip.show();
      break;
    case B_UP:
      Serial.println("Status: Update executed");
      ESPUI.updateButton(sender->id, "Update mode active now - Use the update url: >>>");
      break;
  }
}*/


// ###########################################################################################################################################
// # GUI: LED test switch:
// ###########################################################################################################################################
void switchLEDTest(Control* sender, int value) {
  updatedevice = false;
  switch (value) {
    case S_ACTIVE:
      // Serial.print("Active:");
      preferences.putUInt("useledtest", 1);
      break;
    case S_INACTIVE:
      // Serial.print("Inactive");
      preferences.putUInt("useledtest", 0);
      break;
  }
  // Serial.print(" ");
  // Serial.println(sender->id);
  updatedevice = true;
}


// ###########################################################################################################################################
// # GUI: Night mode switch:
// ###########################################################################################################################################
void switchNightMode(Control* sender, int value) {
  updatedevice = false;
  switch (value) {
    case S_ACTIVE:
      // Serial.print("Active:");
      preferences.putUInt("usenightmode", 1);
      usenightmode = 1;
      if ((iHour <= day_time_stop) && (iHour >= day_time_start)) {
        intensity = intensity_day;
        ESPUI.print(statusNightModeID, "Day time");
      } else {
        intensity = intensity_night;
        ESPUI.print(statusNightModeID, "Night time");
      }
      break;
    case S_INACTIVE:
      // Serial.print("Inactive");
      preferences.putUInt("usenightmode", 0);
      ESPUI.print(statusNightModeID, "Night mode not used");
      intensity = intensity_day;
      usenightmode = 0;
      break;
  }
  // Serial.print(" ");
  // Serial.println(sender->id);
  update_display();
  updatedevice = true;
}


// ###########################################################################################################################################
// # GUI: Show IP-ADdress switch:
// ###########################################################################################################################################
void switchShowIP(Control* sender, int value) {
  updatedevice = false;
  switch (value) {
    case S_ACTIVE:
      // Serial.print("Active:");
      preferences.putUInt("useshowip", 1);
      break;
    case S_INACTIVE:
      // Serial.print("Inactive");
      preferences.putUInt("useshowip", 0);
      break;
  }
  // Serial.print(" ");
  // Serial.println(sender->id);
  updatedevice = true;
}


// ###########################################################################################################################################
// # Update the display / time on it:
// ###########################################################################################################################################
void update_display() {
  Serial.println("Update LED display... " + iStartTime);
  if (usenightmode == 1) {
    if ((iHour <= day_time_stop) && (iHour >= day_time_start)) {
      intensity = intensity_day;
      ESPUI.print(statusNightModeID, "Day time");
    } else {
      intensity = intensity_night;
      ESPUI.print(statusNightModeID, "Night time");
    }
  } else {
    intensity = intensity_day;
  }
  strip.setBrightness(intensity);
  show_time(iHour, iMinute);
  delay(1000);
}


// ###########################################################################################################################################
// # Display hours and minutes text function:
// ###########################################################################################################################################
void show_time(int hours, int minutes) {
  // Set background color:
  back_color();

  // Test a special time:
  // iHour = 23;
  // iMinute = 45;

  // Time:
  numbers(getDigit(iHour, 1), 1);    // Hour left digit
  numbers(getDigit(iHour, 0), 2);    // Hour right digit
  numbers(getDigit(iMinute, 1), 3);  // Minute left digit
  numbers(getDigit(iMinute, 0), 4);  // Minute right digit

  // Date:
  numbers(getDigit(iDay, 1), 5);    // Day left digit
  numbers(getDigit(iDay, 0), 6);    // Day right digit
  numbers(getDigit(iMonth, 1), 7);  // Month left digit
  numbers(getDigit(iMonth, 0), 8);  // Month right digit

  strip.show();
}


// ###########################################################################################################################################
// # Startup function:
// ###########################################################################################################################################
void startup() {
  back_color();  // Set background color
  // uint32_t c1 = strip.Color(redVal_time, greenVal_time, blueVal_time);
  // for (uint16_t i = 0; i <= 15; i++) {
  //   strip.setPixelColor(i, c1);
  //   strip.show();
  // }
  // for (uint16_t i = 240; i <= 255; i++) {
  //   strip.setPixelColor(i, c1);
  //   strip.show();
  // }
}


// ###########################################################################################################################################
// # Background color function:
// ###########################################################################################################################################
void back_color() {
  uint32_t c0 = strip.Color(redVal_back, greenVal_back, blueVal_back);
  for (int i = 0; i < NUMPIXELS; i++) {
    strip.setPixelColor(i, c0);
  }
}


// ###########################################################################################################################################
// # Startup LED test function:
// ###########################################################################################################################################
// LED test --> no blank display if WiFi was not set yet:
void DisplayTest() {
  if (useledtest) {
    Serial.println("Display test...");
    // Testing the digits:
    for (int i = 0; i < 10; i++) {
      back_color();
      numbers(i, 1);
      numbers(i, 2);
      numbers(i, 3);
      numbers(i, 4);
      numbers(i, 5);
      numbers(i, 6);
      numbers(i, 7);
      numbers(i, 8);
      strip.show();
      delay(ipdelay);
    }
  }
}


// ###########################################################################################################################################
// # Show NEUSTART or RESTART text:
// ###########################################################################################################################################
void restartLED(uint32_t color) {
  Serial.println("Show text NEUSTART / RESTART...");
  back_color();
  if (langLEDlayout == 0) {         // DE: NEUSTART
    setLEDcolor(45, 48, 1, color);  // N
    setLEDcolor(50, 50, 1, color);
    setLEDcolor(77, 77, 1, color);
    setLEDcolor(79, 79, 1, color);
    setLEDcolor(80, 80, 1, color);
    setLEDcolor(82, 82, 1, color);
    setLEDcolor(109, 109, 1, color);
    setLEDcolor(111, 111, 1, color);

    setLEDcolor(41, 43, 2, color);  // E
    setLEDcolor(52, 52, 2, color);
    setLEDcolor(73, 75, 2, color);
    setLEDcolor(84, 84, 2, color);
    setLEDcolor(105, 107, 2, color);

    setLEDcolor(36, 36, 3, color);  // U
    setLEDcolor(38, 38, 3, color);
    setLEDcolor(57, 57, 3, color);
    setLEDcolor(59, 59, 3, color);
    setLEDcolor(68, 68, 3, color);
    setLEDcolor(70, 70, 3, color);
    setLEDcolor(89, 89, 3, color);
    setLEDcolor(91, 91, 3, color);
    setLEDcolor(100, 102, 3, color);

    setLEDcolor(32, 34, 4, color);  // S
    setLEDcolor(61, 61, 4, color);
    setLEDcolor(64, 66, 4, color);
    setLEDcolor(95, 95, 4, color);
    setLEDcolor(96, 98, 4, color);

    setLEDcolor(144, 146, 5, color);  // T
    setLEDcolor(174, 174, 5, color);
    setLEDcolor(177, 177, 5, color);
    setLEDcolor(206, 206, 5, color);
    setLEDcolor(209, 209, 5, color);

    setLEDcolor(148, 150, 6, color);  // A
    setLEDcolor(169, 169, 6, color);
    setLEDcolor(171, 171, 6, color);
    setLEDcolor(180, 182, 6, color);
    setLEDcolor(201, 201, 6, color);
    setLEDcolor(203, 203, 6, color);
    setLEDcolor(212, 212, 6, color);
    setLEDcolor(214, 214, 6, color);

    setLEDcolor(153, 155, 7, color);  // R
    setLEDcolor(164, 164, 7, color);
    setLEDcolor(166, 166, 7, color);
    setLEDcolor(185, 187, 7, color);
    setLEDcolor(197, 198, 7, color);
    setLEDcolor(217, 217, 8, color);
    setLEDcolor(219, 219, 7, color);

    setLEDcolor(157, 159, 8, color);  // T
    setLEDcolor(161, 161, 8, color);
    setLEDcolor(190, 190, 8, color);
    setLEDcolor(193, 193, 8, color);
    setLEDcolor(222, 222, 8, color);
  }

  if (langLEDlayout == 1) {         // EN: RESTART
    setLEDcolor(45, 48, 1, color);  // R
    setLEDcolor(50, 50, 1, color);
    setLEDcolor(77, 80, 1, color);
    setLEDcolor(81, 81, 1, color);
    setLEDcolor(109, 109, 1, color);
    setLEDcolor(111, 111, 1, color);

    setLEDcolor(41, 43, 2, color);  // E
    setLEDcolor(52, 52, 2, color);
    setLEDcolor(74, 75, 2, color);
    setLEDcolor(84, 84, 2, color);
    setLEDcolor(105, 107, 2, color);

    setLEDcolor(36, 38, 3, color);  // S
    setLEDcolor(57, 57, 3, color);
    setLEDcolor(68, 70, 3, color);
    setLEDcolor(91, 91, 3, color);
    setLEDcolor(100, 102, 3, color);

    setLEDcolor(32, 34, 4, color);  // T
    setLEDcolor(62, 62, 4, color);
    setLEDcolor(65, 65, 4, color);
    setLEDcolor(94, 94, 4, color);
    setLEDcolor(97, 97, 4, color);

    setLEDcolor(144, 146, 5, color);  // A
    setLEDcolor(175, 175, 5, color);
    setLEDcolor(173, 173, 5, color);
    setLEDcolor(176, 178, 5, color);
    setLEDcolor(205, 205, 5, color);
    setLEDcolor(207, 207, 5, color);
    setLEDcolor(208, 208, 5, color);
    setLEDcolor(210, 210, 5, color);

    setLEDcolor(148, 150, 6, color);  // R
    setLEDcolor(169, 169, 6, color);
    setLEDcolor(171, 171, 6, color);
    setLEDcolor(180, 182, 6, color);
    setLEDcolor(202, 203, 6, color);
    setLEDcolor(212, 212, 6, color);
    setLEDcolor(214, 214, 6, color);

    setLEDcolor(153, 155, 7, color);  // T
    setLEDcolor(165, 165, 7, color);
    setLEDcolor(186, 186, 7, color);
    setLEDcolor(197, 197, 7, color);
    setLEDcolor(218, 218, 7, color);
  }
  strip.show();
}


// ###########################################################################################################################################
// # Startup WiFi text function:
// ###########################################################################################################################################
void SetWLAN(uint32_t color) {
  Serial.println("Show text WLAN OK / WIFI OK...");
  back_color();
  if (langLEDlayout == 0) {         // DE: WLAN OK
    setLEDcolor(45, 45, 1, color);  // W
    setLEDcolor(47, 48, 1, color);
    setLEDcolor(50, 50, 1, color);
    setLEDcolor(77, 77, 1, color);
    setLEDcolor(79, 82, 1, color);
    setLEDcolor(109, 111, 1, color);

    setLEDcolor(43, 43, 2, color);  // L
    setLEDcolor(52, 52, 2, color);
    setLEDcolor(75, 75, 2, color);
    setLEDcolor(84, 84, 2, color);
    setLEDcolor(105, 107, 2, color);

    setLEDcolor(36, 38, 3, color);  // A
    setLEDcolor(57, 57, 3, color);
    setLEDcolor(59, 59, 3, color);
    setLEDcolor(68, 70, 3, color);
    setLEDcolor(89, 89, 3, color);
    setLEDcolor(91, 91, 3, color);
    setLEDcolor(100, 100, 3, color);
    setLEDcolor(102, 102, 3, color);

    setLEDcolor(32, 34, 4, color);  // N
    setLEDcolor(61, 61, 4, color);
    setLEDcolor(63, 64, 4, color);
    setLEDcolor(66, 66, 4, color);
    setLEDcolor(93, 93, 4, color);
    setLEDcolor(95, 95, 4, color);
    setLEDcolor(96, 96, 4, color);
    setLEDcolor(98, 98, 4, color);

    setLEDcolor(148, 150, 6, color);  // O
    setLEDcolor(169, 169, 6, color);
    setLEDcolor(171, 171, 6, color);
    setLEDcolor(180, 180, 6, color);
    setLEDcolor(182, 182, 6, color);
    setLEDcolor(201, 201, 6, color);
    setLEDcolor(203, 203, 6, color);
    setLEDcolor(212, 214, 6, color);

    setLEDcolor(153, 153, 7, color);  // K
    setLEDcolor(166, 166, 7, color);
    setLEDcolor(185, 185, 7, color);
    setLEDcolor(198, 198, 7, color);
    setLEDcolor(217, 217, 7, color);
    setLEDcolor(155, 155, 7, color);
    setLEDcolor(219, 219, 7, color);
    setLEDcolor(165, 165, 7, color);
    setLEDcolor(197, 197, 7, color);
  }

  if (langLEDlayout == 1) {         // EN: WIFI OK
    setLEDcolor(45, 45, 1, color);  // W
    setLEDcolor(47, 48, 1, color);
    setLEDcolor(50, 50, 1, color);
    setLEDcolor(77, 77, 1, color);
    setLEDcolor(79, 82, 1, color);
    setLEDcolor(109, 111, 1, color);

    setLEDcolor(42, 42, 2, color);  // I
    setLEDcolor(53, 53, 2, color);
    setLEDcolor(74, 74, 2, color);
    setLEDcolor(85, 85, 2, color);
    setLEDcolor(106, 106, 2, color);

    setLEDcolor(36, 38, 3, color);  // F
    setLEDcolor(57, 57, 3, color);
    setLEDcolor(69, 70, 3, color);
    setLEDcolor(89, 89, 3, color);
    setLEDcolor(102, 102, 3, color);

    setLEDcolor(33, 33, 4, color);  // I
    setLEDcolor(62, 62, 4, color);
    setLEDcolor(65, 65, 4, color);
    setLEDcolor(94, 94, 4, color);
    setLEDcolor(97, 97, 4, color);

    setLEDcolor(148, 150, 6, color);  // O
    setLEDcolor(169, 169, 6, color);
    setLEDcolor(171, 171, 6, color);
    setLEDcolor(180, 180, 6, color);
    setLEDcolor(182, 182, 6, color);
    setLEDcolor(201, 201, 6, color);
    setLEDcolor(203, 203, 6, color);
    setLEDcolor(212, 214, 6, color);

    setLEDcolor(153, 153, 7, color);  // K
    setLEDcolor(166, 166, 7, color);
    setLEDcolor(185, 185, 7, color);
    setLEDcolor(198, 198, 7, color);
    setLEDcolor(217, 217, 7, color);
    setLEDcolor(155, 155, 7, color);
    setLEDcolor(219, 219, 7, color);
    setLEDcolor(165, 165, 7, color);
    setLEDcolor(197, 197, 7, color);
  }
  strip.show();
}


// ###########################################################################################################################################
// # Wifi Manager setup and reconnect function that runs once at startup and during the loop function of the ESP:
// ###########################################################################################################################################
void WIFI_login() {
  Serial.print("Try to connect to WiFi: ");
  Serial.println(WiFi.SSID());
  WiFi.setHostname(hostname);
  bool WiFires;
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(AP_TIMEOUT);
  WiFires = wifiManager.autoConnect(DEFAULT_AP_NAME);
  if (!WiFires) {
    Serial.print("Failed to connect to WiFi: ");
    Serial.println(WiFi.SSID());
    SetWLAN(strip.Color(255, 0, 0));
    strip.show();
    delay(500);
  } else {
    Serial.print("Connected to WiFi: ");
    Serial.println(WiFi.SSID());
    SetWLAN(strip.Color(0, 255, 0));
    strip.show();
    delay(1000);
  }
}


// ###########################################################################################################################################
// # WiFi Manager 1st connect fix: (Needed after the 1st login to your router - Restart the device once to be able to reach the web page...)
// ###########################################################################################################################################
void WiFiManager1stBootFix() {
  WiFiManFix = preferences.getUInt("WiFiManFix", 0);
  if (WiFiManFix == 0) {
    Serial.println("######################################################################");
    Serial.println("# ESP restart needed becaouse of WiFi Manager Fix");
    Serial.println("######################################################################");
    back_color();
    SetWLAN(strip.Color(0, 255, 0));
    strip.show();
    delay(1000);
    restartLED(strip.Color(0, 255, 0));
    delay(1000);
    preferences.putUInt("WiFiManFix", 1);
    delay(1000);
    preferences.end();
    delay(1000);
    ESP.restart();
  }
}


// ###########################################################################################################################################
// # Actual function, which controls 1/0 of the LED:
// ###########################################################################################################################################
void setLED(int ledNrFrom, int ledNrTo, int switchOn) {
  if (ledNrFrom > ledNrTo) {
    setLED(ledNrTo, ledNrFrom, switchOn);  //sets LED numbers in correct order (because of the date programming below)
  } else {
    for (int i = ledNrFrom; i <= ledNrTo; i++) {
      if ((i >= 0) && (i < NUMPIXELS))
        strip.setPixelColor(i, strip.Color(redVal_time, greenVal_time, blueVal_time));
    }
  }
  if (switchOn == 0) {
    for (int i = ledNrFrom; i <= ledNrTo; i++) {
      if ((i >= 0) && (i < NUMPIXELS))
        strip.setPixelColor(i, strip.Color(redVal_back, greenVal_back, blueVal_back));
    }
  }
}


// ###########################################################################################################################################
// # Actual function, which controls 1/0 of the LED with special color
// ###########################################################################################################################################
void setLEDcolor(int ledNrFrom, int ledNrTo, int switchOn, uint32_t color) {
  if (ledNrFrom > ledNrTo) {
    setLED(ledNrTo, ledNrFrom, switchOn);  //sets LED numbers in correct order (because of the date programming below)
  } else {
    for (int i = ledNrFrom; i <= ledNrTo; i++) {
      if ((i >= 0) && (i < NUMPIXELS))
        strip.setPixelColor(i, color);
    }
  }
}


// ###########################################################################################################################################
// # NTP time functions:
// ###########################################################################################################################################
void configNTPTime() {
  initTime(Timezone);
  printLocalTime();
}
// ###########################################################################################################################################
void setTimezone(String timezone) {
  Serial.printf("Setting timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}
// ###########################################################################################################################################
void initTime(String timezone) {
  struct tm timeinfo;
  Serial.println("Setting up time");
  configTime(0, 0, NTPserver);
  if (!getLocalTime(&timeinfo)) {
    back_color();
    Serial.println("Failed to obtain time");
    ESPUI.print(currentTimeID, "Failed to obtain time");
    ESPUI.print(statusLabelID, "Failed to obtain time");

    uint32_t color = strip.Color(255, 0, 0);
    setLEDcolor(45, 47, 1, color);  // T
    setLEDcolor(49, 49, 1, color);
    setLEDcolor(78, 78, 1, color);
    setLEDcolor(81, 81, 1, color);
    setLEDcolor(110, 110, 1, color);

    setLEDcolor(42, 42, 2, color);  // I
    setLEDcolor(53, 53, 2, color);
    setLEDcolor(74, 74, 2, color);
    setLEDcolor(85, 85, 2, color);
    setLEDcolor(106, 106, 2, color);

    setLEDcolor(36, 38, 3, color);  // M
    setLEDcolor(57, 59, 3, color);
    setLEDcolor(68, 68, 3, color);
    setLEDcolor(70, 70, 3, color);
    setLEDcolor(89, 89, 3, color);
    setLEDcolor(91, 91, 3, color);
    setLEDcolor(100, 100, 3, color);
    setLEDcolor(102, 102, 3, color);

    setLEDcolor(32, 34, 4, color);  // E
    setLEDcolor(61, 61, 4, color);
    setLEDcolor(65, 66, 4, color);
    setLEDcolor(93, 93, 4, color);
    setLEDcolor(96, 98, 4, color);

    setLEDcolor(148, 150, 6, color);  // O
    setLEDcolor(169, 169, 6, color);
    setLEDcolor(171, 171, 6, color);
    setLEDcolor(180, 180, 6, color);
    setLEDcolor(182, 182, 6, color);
    setLEDcolor(201, 201, 6, color);
    setLEDcolor(203, 203, 6, color);
    setLEDcolor(212, 214, 6, color);

    setLEDcolor(153, 153, 7, color);  // K
    setLEDcolor(166, 166, 7, color);
    setLEDcolor(185, 185, 7, color);
    setLEDcolor(198, 198, 7, color);
    setLEDcolor(217, 217, 7, color);
    setLEDcolor(155, 155, 7, color);
    setLEDcolor(219, 219, 7, color);
    setLEDcolor(165, 165, 7, color);
    setLEDcolor(197, 197, 7, color);
    strip.show();
    delay(5000);
    ESP.restart();
    return;
  }
  Serial.println("Got the time from NTP");
  setTimezone(timezone);
}
// ###########################################################################################################################################
void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time 1");
    return;
  }
  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  iStartTime = String(timeStringBuff);
  Serial.println(iStartTime);
  iHour = timeinfo.tm_hour;
  iMinute = timeinfo.tm_min;
  iDay = timeinfo.tm_mday;
  iMonth = timeinfo.tm_mon + 1;

  // Serial.print("Time / Date: ");
  // Serial.print(iHour);
  // Serial.print(" - ");
  // Serial.print(iMinute);
  // Serial.print(" / ");
  // Serial.print(iDay);
  // Serial.print(" - ");
  // Serial.println(iMonth);

  delay(1000);
}
// ###########################################################################################################################################
void setTime(int yr, int month, int mday, int hr, int minute, int sec, int isDst) {
  struct tm tm;
  tm.tm_year = yr - 1900;  // Set date
  tm.tm_mon = month - 1;
  tm.tm_mday = mday;
  tm.tm_hour = hr;  // Set time
  tm.tm_min = minute;
  tm.tm_sec = sec;
  tm.tm_isdst = isDst;  // 1 or 0
  time_t t = mktime(&tm);
  Serial.printf("Setting time: %s", asctime(&tm));
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
}
// ###########################################################################################################################################


// ###########################################################################################################################################
// # GUI: Convert hex color value to RGB int values - TIME:
// ###########################################################################################################################################
void getRGBTIME(String hexvalue) {
  hexvalue.toUpperCase();
  char c[7];
  hexvalue.toCharArray(c, 8);
  int red = hexcolorToInt(c[1], c[2]);
  int green = hexcolorToInt(c[3], c[4]);
  int blue = hexcolorToInt(c[5], c[6]);
  // Serial.print("red: ");
  // Serial.println(red);
  // Serial.print("green: ");
  // Serial.println(green);
  // Serial.print("blue: ");
  // Serial.println(blue);
  preferences.putUInt("redVal_time", red);
  preferences.putUInt("greenVal_time", green);
  preferences.putUInt("blueVal_time", blue);
  redVal_time = red;
  greenVal_time = green;
  blueVal_time = blue;
  update_display();
}


// ###########################################################################################################################################
// # GUI: Convert hex color value to RGB int values - BACKGROUND:
// ###########################################################################################################################################
void getRGBBACK(String hexvalue) {
  hexvalue.toUpperCase();
  char c[7];
  hexvalue.toCharArray(c, 8);
  int red = hexcolorToInt(c[1], c[2]);
  int green = hexcolorToInt(c[3], c[4]);
  int blue = hexcolorToInt(c[5], c[6]);
  // Serial.print("red: ");
  // Serial.println(red);
  // Serial.print("green: ");
  // Serial.println(green);
  // Serial.print("blue: ");
  // Serial.println(blue);
  preferences.putUInt("redVal_back", red);
  preferences.putUInt("greenVal_back", green);
  preferences.putUInt("blueVal_back", blue);
  redVal_back = red;
  greenVal_back = green;
  blueVal_back = blue;
  update_display();
}


// ###########################################################################################################################################
// # GUI: Convert hex color value to RGB int values - helper function:
// ###########################################################################################################################################
int hexcolorToInt(char upper, char lower) {
  int uVal = (int)upper;
  int lVal = (int)lower;
  uVal = uVal > 64 ? uVal - 55 : uVal - 48;
  uVal = uVal << 4;
  lVal = lVal > 64 ? lVal - 55 : lVal - 48;
  // Serial.println(uVal+lVal);
  return uVal + lVal;
}


// ###########################################################################################################################################
// # GUI: Color change for time color:
// ###########################################################################################################################################
void colCallTIME(Control* sender, int type) {
  // Serial.print("TIME Col: ID: ");
  // Serial.print(sender->id);
  // Serial.print(", Value: ");
  // Serial.println(sender->value);
  getRGBTIME(sender->value);
}


// ###########################################################################################################################################
// # GUI: Color change for background color:
// ###########################################################################################################################################
void colCallBACK(Control* sender, int type) {
  // Serial.print("BACK Col: ID: ");
  // Serial.print(sender->id);
  // Serial.print(", Value: ");
  // Serial.println(sender->value);
  getRGBBACK(sender->value);
}


// ###########################################################################################################################################
// # GUI: Slider change for LED intensity: DAY
// ###########################################################################################################################################
void sliderBrightnessDay(Control* sender, int type) {
  // Serial.print("Slider: ID: ");
  // Serial.print(sender->id);
  // Serial.print(", Value: ");
  // Serial.println(sender->value);
  preferences.putUInt("intensity_day", (sender->value).toInt());
  intensity_day = sender->value.toInt();
  update_display();
}


// ###########################################################################################################################################
// # GUI: Slider change for LED intensity: NIGHT
// ###########################################################################################################################################
void sliderBrightnessNight(Control* sender, int type) {
  // Serial.print("Slider: ID: ");
  // Serial.print(sender->id);
  // Serial.print(", Value: ");
  // Serial.println(sender->value);
  preferences.putUInt("intensity_night", (sender->value).toInt());
  intensity_night = sender->value.toInt();
  update_display();
}


// ###########################################################################################################################################
// # GUI: Time Day Mode Start
// ###########################################################################################################################################
void call_day_time_start(Control* sender, int type) {
  // Serial.print("Text: ID: ");
  // Serial.print(sender->id);
  // Serial.print(", Value: ");
  // Serial.println(sender->value);
  preferences.putUInt("day_time_start", (sender->value).toInt());
  day_time_start = sender->value.toInt();
  update_display();
}


// ###########################################################################################################################################
// # GUI: Time Day Mode Stop
// ###########################################################################################################################################
void call_day_time_stop(Control* sender, int type) {
  // Serial.print("Text: ID: ");
  // Serial.print(sender->id);
  // Serial.print(", Value: ");
  // Serial.println(sender->value);
  preferences.putUInt("day_time_stop", (sender->value).toInt());
  day_time_stop = sender->value.toInt();
  update_display();
}


// ###########################################################################################################################################
// # GUI: Convert IP-address value to string:
// ###########################################################################################################################################
String IpAddress2String(const IPAddress& ipAddress) {
  return String(ipAddress[0]) + String(".") + String(ipAddress[1]) + String(".") + String(ipAddress[2]) + String(".") + String(ipAddress[3]);
}

/*
// ###########################################################################################################################################
// # ESP32 OTA update:
// ###########################################################################################################################################
const char* loginIndex =
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=4><b>Clock Update Login Page</b></font></center>"
  "<br>"
  "</td>"
  "<br>"
  "<br>"
  "</tr>"
  "<tr>"
  "<td>Username:</td>"
  "<td><input type='text' size=25 name='userid'><br></td>"
  "</tr>"
  "<br>"
  "<br>"
  "<tr>"
  "<td>Password:</td>"
  "<td><input type='Password' size=25 name='pwd'><br></td>"
  "<br>"
  "<br>"
  "</tr>"
  "<tr>"
  "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
  "</tr>"
  "</table>"
  "</form>"
  "<script>"
  "function check(form)"
  "{"
  "if(form.userid.value=='Clock' && form.pwd.value=='16x16')"
  "{"
  "window.open('/serverIndex')"
  "}"
  "else"
  "{"
  " alert('Error Password or Username') "
  "}"
  "}"
  "</script>";

const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";

void handleOTAupdate() {
  // OTA update server pages urls:
  updserver.on("/", HTTP_GET, []() {
    updserver.sendHeader("Connection", "close");
    updserver.send(200, "text/html", "clock web server on port 2022 is up. Please use the shown url and account credentials to update...");
  });

  updserver.on("/ota", HTTP_GET, []() {
    updserver.sendHeader("Connection", "close");
    updserver.send(200, "text/html", loginIndex);
  });

  updserver.on("/serverIndex", HTTP_GET, []() {
    updserver.sendHeader("Connection", "close");
    updserver.send(200, "text/html", serverIndex);
  });

  // handling uploading firmware file:
  updserver.on(
    "/update", HTTP_POST, []() {
      updserver.sendHeader("Connection", "close");
      updserver.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = updserver.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {  //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        // flashing firmware to ESP
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {  //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          back_color();
          redVal_time = 0;
          greenVal_time = 255;
          blueVal_time = 0;
          setLED(165, 172, 1);
          setLED(52, 57, 1);
          strip.show();
          delay(3000);
        } else {
          Update.printError(Serial);
        }
      }
    });
  updserver.begin();
}
*/

// ###########################################################################################################################################
// # EOF - You have successfully reached the end of the code - well done ;-)
// ###########################################################################################################################################