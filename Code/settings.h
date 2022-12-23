// ###########################################################################################################################################
// #
// # 15-Segment clock using a 16x16 LED matrix project: https://www.printables.com/de/model/345319-15-segment-clock
// #
// # Code by https://github.com/AWSW-de
// #
// # Released under license: GNU General Public License v3.0: https://github.com/AWSW-de/15-segment-clock/blob/main/LICENSE
// #
// # Compatible with clock version: V1.0.0
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
// # Hardware settings:
// ###########################################################################################################################################
#define LEDPIN 32              // Arduino-Pin connected to the NeoPixels
#define NUMPIXELS 256          // How many NeoPixels are attached to the Arduino
#define LEDintensityLIMIT 128  // Limit the intensity level to be able to select in the configuration to avoid to much power drain. !!! Make sure to use a propper power supply !!!
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, LEDPIN, NEO_GRB + NEO_KHZ800);


// ###########################################################################################################################################
// # LED language layout default: !!! SET YOUR DEFAULT LANGUAGE HERE !!!
// ###########################################################################################################################################
int langLEDlayout_default = 0;  // LED language layout default (0 = DE; 1 = EN)
// NOTE: You may need to use the "Reset clock settings"-button to update the value on the device


// ###########################################################################################################################################
// # LED defaults:
// ###########################################################################################################################################
int redVal_back_default = 0;      // Default background color RED
int greenVal_back_default = 255;  // Default background color GREEN
int blueVal_back_default = 255;   // Default background color BLUE
int redVal_time_default = 255;    // Default time color RED
int greenVal_time_default = 128;  // Default time color GREEN
int blueVal_time_default = 0;     // Default time color BLUE
int intensity_day_default = 20;   // LED intensity (0..255) in day mode   - Important note: Check power consumption and used power supply capabilities!
int intensity_night_default = 5;  // LED intensity (0..255) in day mode   - Important note: Check power consumption and used power supply capabilities!
int usenightmode_default = 1;     // Use the night mode to reduce LED intensity during set times
int day_time_start_default = 7;   // Define day mode start --> time before is then night mode if used
int day_time_stop_default = 22;   // Define day mode end --> time after is then night mode if used


// ###########################################################################################################################################
// # Various default settings:
// ###########################################################################################################################################
#define AP_TIMEOUT 240          // Timeout in seconds for AP / WLAN config
int useledtest_default = 1;     // Show start animation and display test at boot
int useshowip_default = 1;      // Show the current ip at boot


// ###########################################################################################################################################
// # Variables declaration:
// ###########################################################################################################################################
#define DEFAULT_AP_NAME "15-SegClock"  // WiFi access point name of the ESP32
const char* hostname = "15-SegClock";  // Hostname to be set in your router


// ###########################################################################################################################################
// # NTP time server settings:
// ###########################################################################################################################################
const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";  // You can check a list of timezone string variables here:  https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char* NTPserver = "pool.ntp.org";               // Time server address. Choose the closest one to you here: https://gist.github.com/mutin-sa/eea1c396b1e610a2da1e5550d94b0453


// ###########################################################################################################################################
// # Test functions:
// ###########################################################################################################################################
int testTime = 0;  // LED text output test from 00:00 to 23:29 o'clock. Each minute is 1 second in the test


// ###########################################################################################################################################
// # EOF - You have successfully reached the end of the code - well done ;-)
// ###########################################################################################################################################