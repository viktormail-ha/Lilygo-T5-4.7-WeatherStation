// Change to your WiFi credentials
const char* ssid        = "SSID";                          // Specify your Wi-Fi SSID here
const char* password    = "Password";                      // Specify your Wi-Fi PASSWORD here

// Use your own API key by signing up for a free developer account at https://openweathermap.org/
String apikey           = "APKIKEY";                       // Specify your API KEY here
const char server[]     = "api.openweathermap.org";
//http://api.openweathermap.org/data/2.5/forecast?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=40
//http://api.openweathermap.org/data/2.5/weather?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=1

//Set your location according to OWM locations
String City             = "Moscow,RU";                     // Your home city See: http://bulk.openweathermap.org/sample/. Specify it in your language
String Latitude         = "54.834217";                     // Latitude of your location in decimal degrees. As an option, you can use Google Maps to find the coordinates 
String Longitude        = "38.642207";                     // Longitude of your location in decimal degrees. As an option, you can use Google Maps to find the coordinates
String Language         = "en";                            // NOTE: Only the weather description is translated by OWM. For the rest of the translations, use the lang file and specify the selected file below
                                                           // Examples: German (DE) Arabic (AR) Czech (CZ) English (EN) Greek (EL) Persian(Farsi) (FA) Galician (GL) Hungarian (HU) Japanese (JA)
                                                           // Korean (KR) Latvian (LA) Lithuanian (LT) Macedonian (MK) Slovak (SK) Slovenian (SL) Vietnamese (VI) Russian (RU)
String Hemisphere       = "north";                         // or "south"  
String Units            = "R";                             // Use "R" for Metric and pressure in "mmHg", use "M" for Metric and pressure in hPa, use "I" for Imperial 
const char* Timezone    = "MSK-3";                         // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
                                                           // See below for examples
const char* ntpServer   = "pool.ntp.org";                  // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
                                                           // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
                                                           // EU "0.europe.pool.ntp.org"
                                                           // US "0.north-america.pool.ntp.org"
                                                           // See: https://www.ntppool.org/en/                                                           
int  gmtOffset_sec      = 3 * 3600;                        // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int  daylightOffset_sec = 0;                               // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset

//Set your OWM Forecast Period
const int max_readings  = 40;                              // 5-days (40 hours) here is OWM Forecast Period, but could be changed to 3- or 4-days (24 or 32 hours)


// ====================== MOON SECTION ======================
// This option has not been thoroughly tested. If it behaves unexpectedly or is not required, it is recommended to disable it (set ShowMoonPosition, ShowMoonEventSection, ShowMoonLatVisible to zero).

// Show the Moon's position relative to the horizon
const int ShowMoonPosition     = 1;                        // 1 = show the triangle indicating the Moon's position relative to the horizon in the Moon phase section

// Moon event line under Visibility/Clouds
const int ShowMoonEventSection = 1;                        // 1 = show the next Moon event (rise/set and time)
const int ShowMoonLatVisible   = 1;                        // 1 = show the Moon's altitude when it is above the horizon
const int ShowMoonLatInvisible = 1;                        // 1 = also show the Moon's altitude when it is below the horizon


// ====================== HOME ASSISTANT SECTION ======================
// This option has not been thoroughly tested. If it behaves unexpectedly or is not required, it is recommended to disable it (set ha_data to zero).
// By default it is disabled
// If only line 1 is active (sensor 1 and/or 2), while all other sensors are disabled, line 1 is centered between City and Data.
// Otherwise (if line 1 and line 2 and/or line 3 are active), lines 1 and 2 are right-aligned at a fixed distance from Data, while line 3 is left-aligned with the beginning of Data.

const int  ha_data   = 0;                                  // 1 = use HA, otherwise disabled
const char* ha_host  = "192.168.1.150";                    // Change to your Home Assistant IP address
const int   ha_port  = 8123;                               // Specify the Home Assistant port (default: 8123)
// Create a Long-Lived Access Token in HA (Profile → Long-Lived Access Tokens → Create Token), do not use my token (it is just for example):
const char* ha_token = "eyJhGhkfdsudU71Nfdshkjfhd6IkpXcxCJ9.eyJpc3MiOiI1ZJF8NhbvlIbzMDI0YjM1YmIyNzY1MD3ZjBynrLvo7...oyMTAxNzkyNDkwfQ.AZlKs-ca6dz_Xxe6o7Celxl1Pf_cCnRBQ576ro5fUCM";

// Font size for Home Assistant sensor lines
const int ha_font = 10;                                    // Use 8, 10, or 12 when using 2 or 3 lines (3–6 sensors); 10 usually looks best
                                                           // If you are using only one line (1–2 sensors with very short names, for example like this: "M: 47%; C: 13%" or "M: 47%"), you can try to set this to 18 or even to 24 (but I guess 12 or 18 might be better in that case)
                                                           // The 18 and 24 fonts do not include Cyrillic characters

// Line 1: sensors 1 and 2
// Sensor 1
const int   ha_sensor1_on    = 1;                          // 1 = use the sensor for display, otherwise disabled
const char* ha_sensor1       = "sensor.xerox_workcentre_6025_magenta_toner_cartridge"; // Specify the sensor ID from Home Assistant whose value will be shown on the display (in this example, the printer's magenta toner cartridge)
const char* ha_sensor1_name  = "Magenta: ";                // Specify the sensor name to be shown on the display; it is recomended to keep ": " at the end (in this example, "Magenta: "). Use short names
const int   ha_sensor1_round = 0;                          // Round the sensor value to this many decimal places (0 = integer)

// Sensor 2
const int   ha_sensor2_on    = 1;
const char* ha_sensor2       = "sensor.xerox_workcentre_6025_cyan_toner_cartridge";
const char* ha_sensor2_name  = "Cyan: ";
const int   ha_sensor2_round = 0;

// Line 2: sensors 3 and 4
// Sensor 3
const int   ha_sensor3_on    = 1;
const char* ha_sensor3       = "sensor.xerox_workcentre_6025_yellow_toner_cartridge";
const char* ha_sensor3_name  = "Yellow: ";
const int   ha_sensor3_round = 0;

// Sensor 4
const int   ha_sensor4_on    = 1;
const char* ha_sensor4       = "sensor.xerox_workcentre_6025_black_toner_cartridge";
const char* ha_sensor4_name  = "Black: ";
const int   ha_sensor4_round = 0;

// Line 3: sensors 5 and 6
// Sensor 5
const int   ha_sensor5_on    = 1;
const char* ha_sensor5       = "sensor.thb2_b76f_temperature";
const char* ha_sensor5_name  = "Street Temp: ";
const int   ha_sensor5_round = 1;

// Sensor 6
const int   ha_sensor6_on    = 1;
const char* ha_sensor6       = "sensor.thb2_b76f_humidity";
const char* ha_sensor6_name  = "Hum: ";
const int   ha_sensor6_round = 1;

// ====================== HOME ASSISTANT SENSOR 7 SECTION ======================
// Line 4: This is the same line where the weather description is displayed,
// so it is recommended to use only one sensor here, preferably without a name,
// because the weather description can be quite long.

// Sensor 7
const int   ha_sensor7_font  = 24;                            // Set font size for Sensor 7 here. 24 usually looks best for a single sensor without a name on this line (Cyrillic characters are not supported)
                                                              // You can also try 8, 10, 12, or 18, but in that case you will need to adjust
                                                              // the position of sensor 7 in the .ino file.

const int   ha_sensor7_on    = 1;                             // 1 = use the sensor for display, otherwise disabled
const char* ha_sensor7       = "sensor.thb2_b76f_temperature";// Specify the sensor ID from Home Assistant whose value will be shown on the display
const char* ha_sensor7_name  = "";                            // Specify the sensor name to be shown on the display; it is recomended to stay it empty (for Sensor 7)
const int   ha_sensor7_round = 1;                             // Round the sensor value to this many decimal places (0 = integer)


// Example time zones
// const char* Timezone = "MET-1METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
// const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";       // Central Europe
// const char* Timezone = "EST-2METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
// const char* Timezone = "EST5EDT,M3.2.0,M11.1.0";           // EST USA  
// const char* Timezone = "CST6CDT,M3.2.0,M11.1.0";           // CST USA
// const char* Timezone = "MST7MDT,M4.1.0,M10.5.0";           // MST USA
// const char* Timezone = "NZST-12NZDT,M9.5.0,M4.1.0/3";      // Auckland
// const char* Timezone = "EET-2EEST,M3.5.5/0,M10.5.5/0";     // Asia
// const char* Timezone = "ACST-9:30ACDT,M10.1.0,M4.1.0/3";   // Australia
// const char* Timezone = "MSK-3";                            // Europe - Moscow
// const char* Timezone = "<+05>-5";                          // Asia/Almaty

// Select language to use or add your own translation
#include "lang.h"
// #include "lang_ru.h"
// #include "lang_fr.h"
// #include "lang_de.h"
