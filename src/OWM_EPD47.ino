#include <Arduino.h>            // In-built
#include <esp_task_wdt.h>       // In-built
#include "freertos/FreeRTOS.h"  // In-built
#include "freertos/task.h"      // In-built
#include "epd_driver.h"         // https://github.com/Xinyuan-LilyGO/LilyGo-EPD47
#include "esp_adc_cal.h"        // In-built
#include <ArduinoJson.h>        // https://github.com/bblanchon/ArduinoJson
#include <HTTPClient.h>         // In-built
#include <WiFi.h>               // In-built
#include <SPI.h>                // In-built
#include <time.h>               // In-built
#include "user_settings.h"
#include "forecast_record.h"

#define SCREEN_WIDTH   EPD_WIDTH
#define SCREEN_HEIGHT  EPD_HEIGHT

//String version = "2.7.1 / 4.7in"; 

enum alignment {LEFT, RIGHT, CENTER};
#define White         0xFF
#define LightGrey     0xBB
#define Grey          0x88
#define DarkGrey      0x44
#define Black         0x00

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

boolean LargeIcon   = true;
boolean SmallIcon   = false;
#define Large  20           // For icon drawing
#define Small  10           // For icon drawing
String  Time_str = "--:--:--";
String  Date_str = "-- --- ----";

// Home Assistant section:
String haValue1 = "—";
String haValue2 = "—";
String haValue3 = "—";
String haValue4 = "—";
String haValue5 = "—";
String haValue6 = "—";
String haValue7 = "—";
bool haAnySensorEnabled() {
  return (ha_sensor1_on == 1) || (ha_sensor2_on == 1) ||
         (ha_sensor3_on == 1) || (ha_sensor4_on == 1) ||
         (ha_sensor5_on == 1) || (ha_sensor6_on == 1) ||
         (ha_sensor7_on == 1);
}
bool haOnlyFirstLineEnabled() {
  return (ha_sensor3_on != 1) && (ha_sensor4_on != 1) &&
         (ha_sensor5_on != 1) && (ha_sensor6_on != 1);
}
bool showEvents = (ShowMoonEventSection == 1) || (ShowMoonLatVisible == 1);

int     wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0, EventCnt = 0, vref = 1100;
//################ PROGRAM VARIABLES and OBJECTS ##########################################

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];

float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings]    = {0};
float rain_readings[max_readings]        = {0};
float snow_readings[max_readings]        = {0};

long SleepDuration   = 30; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int  WakeupHour      = 6;  // Wakeup after 06:00 to save battery power
int  SleepHour       = 3;  // Sleep  after 03:00 to save battery power
long StartTime       = 0;
long SleepTimer      = 0;
long Delta           = 30; // ESP32 rtc speed compensation, prevents display at xx:59:yy and then xx:00:yy (one minute later) to save power

//fonts
#include "OpenSans8B.h"
#include "OpenSans10B.h"
#include "OpenSans12B.h"
#include "OpenSans18B.h"
#include "OpenSans24B.h"
#include "moon.h"
#include "sunrise.h"
#include "sunset.h"

GFXfont  currentFont;
uint8_t *framebuffer;

void BeginSleep() {
  epd_poweroff_all();
  UpdateLocalTime();
  SleepTimer = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec)) + Delta; //Some ESP32 have a RTC that is too fast to maintain accurate time, so add an offset
  esp_sleep_enable_timer_wakeup(SleepTimer * 1000000LL); // in Secs, 1000000LL converts to Secs as unit = 1uSec
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Entering " + String(SleepTimer) + " (secs) of sleep time");
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();  // Sleep for e.g. 30 minutes
}

boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset(); // Set the TZ environment variable
  delay(100);
  return UpdateLocalTime();
}

uint8_t StartWiFi() 
{
  Serial.println("\r\nWiFi Connecting to: " + String(ssid));
  IPAddress dns(8, 8, 8, 8); // Use Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
    Serial.printf("WiFi connection failed, retrying...!\n");
    WiFi.disconnect(true); // delete SID/PWD
    delay(500);
    WiFi.begin(ssid, password);
  }
  if (WiFi.waitForConnectResult() == WL_CONNECTED) 
  {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else 
  {
    wifi_signal = 0;
    Serial.println("WiFi connection *** FAILED ***");
  }
  return WiFi.status();
}

void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi switched Off");
}

void InitialiseSystem() {
  StartTime = millis();
  Serial.begin(115200);
  while (!Serial);
  Serial.println(String(__FILE__) + "\nStarting...");
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) Serial.println("Memory alloc failed!");
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
}

void loop() {
  // Nothing to do here
}

void setup() {
  InitialiseSystem();
  if (StartWiFi() == WL_CONNECTED && SetupTime() == true) {
    bool WakeUp = false;
    if (WakeupHour > SleepHour)
      WakeUp = (CurrentHour >= WakeupHour || CurrentHour <= SleepHour);
    else
      WakeUp = (CurrentHour >= WakeupHour && CurrentHour <= SleepHour);
    if (WakeUp) {
      byte Attempts = 1;
      bool RxWeather  = false;
      bool RxForecast = false;
      WiFiClient client;   // wifi client object
      while ((RxWeather == false || RxForecast == false) && Attempts <= 2) { // Try up-to 2 time for Weather and Forecast data
        if (RxWeather  == false) RxWeather  = obtainWeatherData(client, "weather");
        if (RxForecast == false) RxForecast = obtainWeatherData(client, "forecast");
        Attempts++;
      }
      Serial.println("Received all weather data...");

      // Home Assistant section
      if (ha_data == 1 && haAnySensorEnabled()) {
        obtainHomeAssistantData(client);
      }

      if (RxWeather && RxForecast) { // Only if received both Weather or Forecast proceed
        StopWiFi();         // Reduces power consumption
        epd_poweron();      // Switch on EPD display
        epd_clear();        // Clear the screen
        DisplayWeather();   // Display the weather data
        epd_update();       // Update the display to show the information
        epd_poweroff_all(); // Switch off all power to EPD
      }
    }
  }
  else {
    epd_clear();        // Clear the screen
    DisplayStatusSection(600, 20, wifi_signal);    // Wi-Fi signal strength and Battery voltage
    epd_update();       // Update the display to show the information
    epd_poweroff_all(); // Switch off all power to EPD
  }
  BeginSleep();
}

void Convert_Readings_to_Imperial() { // Only the first 3-hours are used
  WxConditions[0].Pressure = hPa_to_inHg(WxConditions[0].Pressure);
  WxForecast[0].Rainfall   = mm_to_inches(WxForecast[0].Rainfall);
  WxForecast[0].Snowfall   = mm_to_inches(WxForecast[0].Snowfall);
}

void Convert_Pressure_to_mmHg() { // Only the first 3-hours are used
  WxConditions[0].Pressure = hPa_to_mmHg(WxConditions[0].Pressure);
}

bool DecodeWeather(WiFiClient& json, String Type) {
  Serial.print(F("\nDeserializing json... "));
  auto doc = DynamicJsonDocument(64 * 1024);                     // allocate the JsonDocument
  DeserializationError error = deserializeJson(doc, json); // Deserialize the JSON document
  if (error) {                                             // Test if parsing succeeds.
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  Serial.println(" Decoding " + Type + " data");
  if (Type == "weather") {
    WxConditions[0].High        = -50; // Minimum forecast low
    WxConditions[0].Low         = 50;  // Maximum Forecast High
    WxConditions[0].FTimezone   = doc["timezone_offset"]; // "0"
    WxConditions[0].Sunrise     = root["sys"]["sunrise"].as<int>();                Serial.println("SRis: " + String(WxConditions[0].Sunrise));
    WxConditions[0].Sunset      = root["sys"]["sunset"].as<int>();                 Serial.println("SSet: " + String(WxConditions[0].Sunset));
    WxConditions[0].Temperature = root["main"]["temp"].as<float>();                Serial.println("Temp: " + String(WxConditions[0].Temperature));
    WxConditions[0].FeelsLike   = root["main"]["feels_like"].as<float>();          Serial.println("FLik: " + String(WxConditions[0].FeelsLike));
    WxConditions[0].Pressure    = root["main"]["grnd_level"].as<int>();            Serial.println("Pres: " + String(WxConditions[0].Pressure));
    WxConditions[0].Humidity    = root["main"]["humidity"].as<int>();              Serial.println("Humi: " + String(WxConditions[0].Humidity));
    WxConditions[0].Cloudcover  = root["clouds"]["all"].as<int>();                 Serial.println("CCov: " + String(WxConditions[0].Cloudcover));
    WxConditions[0].Visibility  = root["visibility"].as<int>();                    Serial.println("Visi: " + String(WxConditions[0].Visibility));
    WxConditions[0].Windspeed   = root["wind"]["speed"].as<float>();               Serial.println("WSpd: " + String(WxConditions[0].Windspeed));
    WxConditions[0].Winddir     = root["wind"]["deg"].as<float>();                 Serial.println("WDir: " + String(WxConditions[0].Winddir));
    WxConditions[0].Windgust    = root["wind"]["gust"].as<float>();                Serial.println("WGust: " + String(WxConditions[0].Windgust));
    WxConditions[0].Forecast0   = root["weather"][0]["description"].as<const char*>();      Serial.println("Fore: " + String(WxConditions[0].Forecast0));
    WxConditions[0].Icon        = root["weather"][0]["icon"].as<const char*>();             Serial.println("Icon: " + String(WxConditions[0].Icon));
    WxConditions[0].Id          = root["weather"][0]["id"].as<int>();              Serial.println("Id: " + String(WxConditions[0].Id));
  }
  if (Type == "forecast") {
    //Serial.println(json);
    Serial.print(F("\nReceiving Forecast period - ")); //------------------------------------------------
    JsonArray list                    = root["list"];
    for (byte r = 0; r < max_readings; r++) {
      Serial.println("\nPeriod-" + String(r) + "--------------");
      WxForecast[r].Dt                = list[r]["dt"].as<int>();
      WxForecast[r].Temperature       = list[r]["main"]["temp"].as<float>();       Serial.println("Temp: " + String(WxForecast[r].Temperature));
      WxForecast[r].Low               = list[r]["main"]["temp_min"].as<float>();   Serial.println("TLow: " + String(WxForecast[r].Low));
      WxForecast[r].High              = list[r]["main"]["temp_max"].as<float>();   Serial.println("THig: " + String(WxForecast[r].High));
      WxForecast[r].Pressure          = list[r]["main"]["grnd_level"].as<float>();   Serial.println("Pres: " + String(WxForecast[r].Pressure));
      WxForecast[r].Humidity          = list[r]["main"]["humidity"].as<float>();   Serial.println("Humi: " + String(WxForecast[r].Humidity));
      WxForecast[r].Icon              = list[r]["weather"][0]["icon"].as<const char*>(); Serial.println("Icon: " + String(WxForecast[r].Icon));
      WxForecast[r].Id                = list[r]["weather"][0]["id"].as<int>();     Serial.println("Id: " + String(WxForecast[r].Id));
      WxForecast[r].Rainfall          = list[r]["rain"]["3h"].as<float>();         Serial.println("Rain: " + String(WxForecast[r].Rainfall));
      WxForecast[r].Snowfall          = list[r]["snow"]["3h"].as<float>();         Serial.println("Snow: " + String(WxForecast[r].Snowfall));
      if (r < 8) { // Check next 3 x 8 Hours = 1 day
        if (WxForecast[r].High > WxConditions[0].High) WxConditions[0].High = WxForecast[r].High; // Get Highest temperature for next 24Hrs
        if (WxForecast[r].Low  < WxConditions[0].Low)  WxConditions[0].Low  = WxForecast[r].Low;  // Get Lowest  temperature for next 24Hrs
      }
    }
    //------------------------------------------
    float pressure_trend = WxForecast[2].Pressure - WxForecast[0].Pressure; // Measure pressure slope between ~now and later
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0; // Remove any small variations less than 0.1
    WxConditions[0].Trend = "=";
    if (pressure_trend > 0)  WxConditions[0].Trend = "+";
    if (pressure_trend < 0)  WxConditions[0].Trend = "-";
    if (pressure_trend == 0) WxConditions[0].Trend = "0";

    if (Units == "I") Convert_Readings_to_Imperial();

    if (Units == "R") Convert_Pressure_to_mmHg();
  }
  return true;
}

String ConvertUnixTime(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40];
  if (Units == "M" || Units == "R") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
  }
  else {
    strftime(output, sizeof(output), "%I:%M%P %m/%d/%y", now_tm);
  }
  return output;
}

bool obtainWeatherData(WiFiClient & client, const String & RequestType) {
  const String units = (Units == "R" || Units == "M" ? "metric" : "imperial");
  const String Version = "2.5";
  client.stop(); // close connection before sending a new request
  HTTPClient http;
  // Since June 2024, OWM API need v3.0 for the current, and still provide forecast as awaited on version v2.5
  // api.openweathermap.org/data/2.5/RequestType?lat={lat}&lon={lon}&appid={API key}
  String uri = "/data/"+Version+"/"+RequestType+"?lat=" + Latitude + "&lon=" + Longitude + "&appid=" + apikey + "&mode=json&units=" + units + "&lang=" + Language;
  Serial.print("Connecting: ");
  Serial.print(server + uri);
  Serial.println();
  if (RequestType == "onecall") uri += "&exclude=minutely,hourly,alerts,daily";
  http.begin(client, server, 80, uri); 
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    if (!DecodeWeather(http.getStream(), RequestType)) return false;
    client.stop();
  }
  else
  {
    Serial.printf("connection failed, http error code %i %s\n", httpCode, http.errorToString(httpCode));
    client.stop();
    http.end();
    return false;
  }
  http.end();
  return true;
}

// Home Assistant section
String fetchHAState(WiFiClient & client, const char* entity_id, int roundDigits) {
  HTTPClient http;
  String url = "http://" + String(ha_host) + ":" + String(ha_port) +
               "/api/states/" + String(entity_id);

  http.begin(client, url);
  http.addHeader("Authorization", "Bearer " + String(ha_token));
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return "—";
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, payload)) return "—";

  String state;
  if (doc["state"].is<const char*>()) {
    float f = atof(doc["state"].as<const char*>());
    if (f != 0.0f || doc["state"].as<String>() == "0") {
      state = String(f, roundDigits);
    } else {
      state = doc["state"].as<const char*>();  // нечисловой state (unavailable и т.п.)
    }
  } else if (doc["state"].is<float>() || doc["state"].is<int>()) {
    state = String(doc["state"].as<float>(), roundDigits);
  } else {
    return "—";
  }

  const char* unit = doc["attributes"]["unit_of_measurement"];
  if (unit && strlen(unit) > 0) return state + unit;  // "42%" без пробела, или state + " " + unit
  return state;
}

// Home Assistant section
bool obtainHomeAssistantData(WiFiClient & client) {
  if (ha_data != 1 || !haAnySensorEnabled()) {
    return false;   // HA полностью выключен
  }

  if (ha_sensor1_on == 1) haValue1 = fetchHAState(client, ha_sensor1, ha_sensor1_round);
  if (ha_sensor2_on == 1) haValue2 = fetchHAState(client, ha_sensor2, ha_sensor2_round);
  if (ha_sensor3_on == 1) haValue3 = fetchHAState(client, ha_sensor3, ha_sensor3_round);
  if (ha_sensor4_on == 1) haValue4 = fetchHAState(client, ha_sensor4, ha_sensor4_round);
  if (ha_sensor5_on == 1) haValue5 = fetchHAState(client, ha_sensor5, ha_sensor5_round);
  if (ha_sensor6_on == 1) haValue6 = fetchHAState(client, ha_sensor6, ha_sensor6_round);
  if (ha_sensor7_on == 1) haValue7 = fetchHAState(client, ha_sensor7, ha_sensor7_round);

  Serial.printf("HA: %s%s | %s%s | %s%s | %s%s\n",
                ha_sensor1_name, haValue1.c_str(),
                ha_sensor2_name, haValue2.c_str(),
                ha_sensor3_name, haValue3.c_str(),
                ha_sensor4_name, haValue4.c_str(),
                ha_sensor5_name, haValue5.c_str(),
                ha_sensor6_name, haValue6.c_str(),
                ha_sensor7_name, haValue7.c_str());
  return true;
}

float mm_to_inches(float value_mm) {
  return 0.0393701 * value_mm;
}

float hPa_to_inHg(float value_hPa) {
  return 0.02953 * value_hPa;
}

float hPa_to_mmHg(float value_hPa) {
  return value_hPa * 0.75006;
}

int JulianDate(int d, int m, int y) {
  int mm, yy, k1, k2, k3, j;
  yy = y - (int)((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12) mm = mm - 12;
  k1 = (int)(365.25 * (yy + 4712));
  k2 = (int)(30.6001 * mm + 0.5);
  k3 = (int)((int)((yy / 100) + 49) * 0.75) - 38;
  // 'j' for dates in Julian calendar:
  j = k1 + k2 + d + 59 + 1;
  if (j > 2299160) j = j - k3; // 'j' is the Julian date at 12h UT (Universal Time) For Gregorian calendar:
  return j;
}

int textWidth(const String& s) {
  char* data = const_cast<char*>(s.c_str());
  int32_t x1, y1, w, h;
  int32_t xx = 0, yy = 0;
  get_text_bounds(&currentFont, data, &xx, &yy, &x1, &y1, &w, &h, NULL);
  return w;
}

float SumOfPrecip(float DataArray[], int readings) {
  float sum = 0;
  for (int i = 0; i < readings; i++) sum += DataArray[i];
  return sum;
}

int utf8CharLen(uint8_t c) {
  if ((c & 0x80) == 0x00) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

void utf8UpperFirst(const String& text, String& firstChar, String& rest) {
  firstChar = "";
  rest = "";
  if (text.length() == 0) return;

  int len = utf8CharLen((uint8_t)text[0]);
  if (len > (int)text.length()) len = text.length();

  firstChar = text.substring(0, len);
  rest = text.substring(len);

  // Латиница a-z
  if (len == 1) {
    char c = firstChar[0];
    if (c >= 'a' && c <= 'z') firstChar.setCharAt(0, c - 32);
    return;
  }

  // Кириллица UTF-8: 2 байта
  if (len == 2) {
    uint8_t b0 = (uint8_t)firstChar[0];
    uint8_t b1 = (uint8_t)firstChar[1];
    uint16_t cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);

    // а-я (0x430-0x44F) → А-Я (0x410-0x42F)
    if (cp >= 0x430 && cp <= 0x44F) {
      cp -= 0x20;
    }
    // ё (0x451) → Ё (0x401)
    else if (cp == 0x451) {
      cp = 0x401;
    }

    // обратно в UTF-8
    char out[3];
    out[0] = (char)(0xC0 | (cp >> 6));
    out[1] = (char)(0x80 | (cp & 0x3F));
    out[2] = 0;
    firstChar = String(out);
  }
}

String TitleCase(String text) {
  if (text.length() == 0) return text;

  String first, rest;
  utf8UpperFirst(text, first, rest);
  return first + rest;
}

void DisplayWeather() {                          // 4.7" e-paper display is 960x540 resolution
  DisplayStatusSection(600, 20, wifi_signal);    // Wi-Fi signal strength and Battery voltage
  DisplayGeneralInfoSection();                   // Top line of the display
  DisplayDisplayWindSection(137, 150, WxConditions[0].Winddir, WxConditions[0].Windspeed, 100);
  DisplayAstronomySection(5, 252);               // Astronomy section Sun rise/set, Moon phase and Moon icon
  DisplayMainWeatherSection(320, 110);           // Centre section of display for Location, temperature, Weather report, current Wx Symbol
  DisplayWeatherIcon(855, 140);                  // Display weather icon scale = Large;
  DisplayForecastSection(285, 220);              // 3hr forecast boxes
  DisplayGraphSection(320, 220);                 // Graphs of pressure, temperature, humidity and rain or snowfall
}


void setHAFont() {
  if (ha_font == 10)      setFont(OpenSans10B);
  else if (ha_font == 12) setFont(OpenSans12B);
  else if (ha_font == 18) setFont(OpenSans18B);
  else if (ha_font == 24) setFont(OpenSans24B);
  else                    setFont(OpenSans8B);  // по умолчанию
}

void setHASensor7Font() {
  if (ha_sensor7_font == 10)      setFont(OpenSans10B);
  else if (ha_sensor7_font == 12) setFont(OpenSans12B);
  else if (ha_sensor7_font == 18) setFont(OpenSans18B);
  else if (ha_sensor7_font == 24) setFont(OpenSans24B);
  else                            setFont(OpenSans8B);  // по умолчанию
}


void DisplayGeneralInfoSection() {
  setFont(OpenSans10B);

  int dateStart = 500;
  int line1X    = 480;

  // City
  drawString(5, 0, City, LEFT);

  // Home Assistant section
  if (ha_data == 1 && haAnySensorEnabled()) {
    
    // Строка 1: сенсоры 1 и 2
    String line1 = "";
    if (ha_sensor1_on == 1) line1 += String(ha_sensor1_name) + haValue1;
    if (ha_sensor2_on == 1) {
      if (line1.length() > 0) line1 += "; ";
      line1 += String(ha_sensor2_name) + haValue2;
    }
    if (line1.length() > 0) {
      if (haOnlyFirstLineEnabled()) {
        int cityEnd   = 5 + textWidth(City);
        int line1X    = (cityEnd + dateStart) / 2;
        setHAFont();
        drawString(line1X, 0, line1, CENTER);
      } else{
        setHAFont();
        drawString(line1X, 0, line1, RIGHT);
      }
    }

    // Строка 2: сенсоры 3 и 4
    String line2 = "";
    if (ha_sensor3_on == 1) line2 += String(ha_sensor3_name) + haValue3;
    if (ha_sensor4_on == 1) {
      if (line2.length() > 0) line2 += "; ";
      line2 += String(ha_sensor4_name) + haValue4;
    }

    if (line2.length() > 0) {
      drawString(line1X, 32, line2, RIGHT);   // чуть ниже
    }

    // Строка 3: сенсоры 5 и 6
    String line3 = "";
    if (ha_sensor5_on == 1) line3 += String(ha_sensor5_name) + haValue5;
    if (ha_sensor6_on == 1) {
      if (line3.length() > 0) line3 += "; ";
      line3 += String(ha_sensor6_name) + haValue6;
    }

    if (line3.length() > 0) {
      // drawString((line2EndX + 905) / 2, 32, line3, CENTER);   // чуть ниже
      drawString(dateStart, 32, line3, LEFT);   // чуть ниже
    }
  }

  // Дата
  setFont(OpenSans10B);
  drawString(dateStart, 0, Date_str + ", " + Time_str, LEFT);
}

void DisplayWeatherIcon(int x, int y) {
  int y_offset = 0;
  if (showEvents) {
    y_offset = 15;
  }
  DisplayConditionsSection(x, y - y_offset, WxConditions[0].Icon, WxConditions[0].Id, LargeIcon);
}

void DisplayMainWeatherSection(int x, int y) {
  setFont(OpenSans8B);
  DisplayTempHumiPressSection(x, y - 60);
//  DisplayForecastTextSection(x - 55, y + 95);
  DisplayVisiCCoverSection(x, y + 55);
  if (showEvents) {
    DisplayHASensor7Section(x, y + 95);
    DisplayForecastTextSection(SCREEN_WIDTH - 20, y + 90);
    DisplayMoonEventSection(SCREEN_WIDTH - 20, y + 120);
  } else {
    DisplayHASensor7Section(x, y + 90);
    DisplayForecastTextSection(SCREEN_WIDTH - 20, y + 100);
  }
}

void DisplayDisplayWindSection(int x, int y, float angle, float windspeed, int Cradius) {
  arrow(x, y, Cradius - 22, angle, 18, 33); // Show wind direction on outer circle of width and length
  setFont(OpenSans8B);
  int dxo, dyo, dxi, dyi;
  drawCircle(x, y, Cradius, Black);       // Draw compass circle
  drawCircle(x, y, Cradius + 1, Black);   // Draw compass circle
  drawCircle(x, y, Cradius * 0.7, Black); // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45)  drawString(dxo + x + 15, dyo + y - 18, TXT_NE, CENTER);
    if (a == 135) drawString(dxo + x + 20, dyo + y - 2,  TXT_SE, CENTER);
    if (a == 225) drawString(dxo + x - 20, dyo + y - 2,  TXT_SW, CENTER);
    if (a == 315) drawString(dxo + x - 15, dyo + y - 18, TXT_NW, CENTER);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    drawLine(dxo + x, dyo + y, dxi + x, dyi + y, Black);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    drawLine(dxo + x, dyo + y, dxi + x, dyi + y, Black);
  }
  drawString(x, y - Cradius - 20,     TXT_N, CENTER);
  drawString(x, y + Cradius + 10,     TXT_S, CENTER);
  drawString(x - Cradius - 15, y - 5, TXT_W, CENTER);
  drawString(x + Cradius + 10, y - 5, TXT_E, CENTER);
  drawString(x + 3, y + 50, String(angle, 0) + "°", CENTER);
  setFont(OpenSans12B);
  drawString(x, y - 50, WindDegToOrdinalDirection(angle), CENTER);
  setFont(OpenSans24B);
  drawString(x + 3, y - 18, String(windspeed, 1), CENTER);
  setFont(OpenSans10B);
  // drawString(x, y + 25, (Units == "R" ? "м/с" : (Units == "M" ? "m/s" : "mph")), CENTER);
  drawString(x, y + 25, (Language.equalsIgnoreCase("ru") ? ((Units == "R" || Units == "M") ? "м/с" : "миль/ч") : ((Units == "R" || Units == "M") ? "m/s" : "mph")), CENTER);
}

String WindDegToOrdinalDirection(float winddirection) {
  if (winddirection >= 348.75 || winddirection < 11.25)  return TXT_N;
  if (winddirection >=  11.25 && winddirection < 33.75)  return TXT_NNE;
  if (winddirection >=  33.75 && winddirection < 56.25)  return TXT_NE;
  if (winddirection >=  56.25 && winddirection < 78.75)  return TXT_ENE;
  if (winddirection >=  78.75 && winddirection < 101.25) return TXT_E;
  if (winddirection >= 101.25 && winddirection < 123.75) return TXT_ESE;
  if (winddirection >= 123.75 && winddirection < 146.25) return TXT_SE;
  if (winddirection >= 146.25 && winddirection < 168.75) return TXT_SSE;
  if (winddirection >= 168.75 && winddirection < 191.25) return TXT_S;
  if (winddirection >= 191.25 && winddirection < 213.75) return TXT_SSW;
  if (winddirection >= 213.75 && winddirection < 236.25) return TXT_SW;
  if (winddirection >= 236.25 && winddirection < 258.75) return TXT_WSW;
  if (winddirection >= 258.75 && winddirection < 281.25) return TXT_W;
  if (winddirection >= 281.25 && winddirection < 303.75) return TXT_WNW;
  if (winddirection >= 303.75 && winddirection < 326.25) return TXT_NW;
  if (winddirection >= 326.25 && winddirection < 348.75) return TXT_NNW;
  return "?";
}

void DisplayTempHumiPressSection(int x, int y) {
  setFont(OpenSans24B);
  
  String tempHumi = String(WxConditions[0].Temperature, 1) + "° " + String(WxConditions[0].Humidity, 0) + "%";
  if (ha_data == 1 && haAnySensorEnabled()) {
    drawString(x - 30, y + 20, tempHumi, LEFT);
  } else {
    drawString(x - 30, y, tempHumi, LEFT);
  }

  if (ha_data == 1 && haAnySensorEnabled()) {
    DrawPressureAndTrend(x + textWidth(tempHumi) - 70, y + 37, WxConditions[0].Pressure, WxConditions[0].Trend);
  } else {
    DrawPressureAndTrend(x + textWidth(tempHumi) - 70, y + 15, WxConditions[0].Pressure, WxConditions[0].Trend);
  }

  int Yoffset = 65;
  setFont(OpenSans12B);
  drawString(x - 30, y + Yoffset, TXT_FEELSLIKE + String(WxConditions[0].FeelsLike, 1) + "°   " + TXT_HILO + String(WxConditions[0].High, 0) + "° | " + String(WxConditions[0].Low, 0) + "°", LEFT);
}

void DisplayVisiCCoverSection(int x, int y) {
  setFont(OpenSans12B);
  int space_width = 60;
  int cursor = x - 15;

  // Visibility
  String visi;
  if (Units == "I") {
    if (WxConditions[0].Visibility == 10000) {
        visi = "10+ mi";
    } else {
        float vis_mi = WxConditions[0].Visibility / 1609.34f;
        visi = String(vis_mi, 1) + " mi";
    }
  } else if (Units == "R") {
    if (WxConditions[0].Visibility == 10000) {
        visi = Language.equalsIgnoreCase("ru") ? "10+ км" : "10+ km";
    } else if (WxConditions[0].Visibility < 1000) {
        visi = String(WxConditions[0].Visibility) +
               (Language.equalsIgnoreCase("ru") ? " м" : " m");
    } else {
        float vis_km = WxConditions[0].Visibility / 1000.0f;
        visi = String(vis_km, 1) +
               (Language.equalsIgnoreCase("ru") ? " км" : " km");
    }
  } else {
      if (WxConditions[0].Visibility == 10000) {
          visi = "10+ km";
      } else if (WxConditions[0].Visibility < 1000) {
          visi = String(WxConditions[0].Visibility) + " m";
      } else {
          float vis_km = WxConditions[0].Visibility / 1000.0f;
          visi = String(vis_km, 1) + " km";
      }
  }
  Visibility(cursor, y, visi);
  cursor += textWidth(visi) + space_width;

  // CloudCover
  int clouds = WxConditions[0].Cloudcover;
  String cloud = String(clouds) + "%";
  if (clouds <= 10) ClearSkyCover(cursor, y, clouds);
  else if (clouds <= 30) FewCloudsCover(cursor, y, clouds);
  else if (clouds <= 60) PartlyCloudyCover(cursor, y, clouds);
  else if (clouds <= 85) MostlyCloudyCover(cursor, y, clouds);
  else CloudCover(cursor, y, clouds);
  cursor += textWidth(cloud) + space_width;;
  WindGust(cursor, y, WxConditions[0].Windgust);
}

void DisplayHASensor7Section(int x, int y) {
  // Home Assistant section, Строка 4: сенсор 7
  String line4 = "";
  if (ha_sensor7_on == 1) line4 += String(ha_sensor7_name) + haValue7;
  if (line4.length() > 0) {
    setHASensor7Font();
    drawString(x - 30, y, line4, LEFT);
  }
}

// void DisplayForecastTextSection(int x, int y) {
// #define lineWidth 34
//   setFont(OpenSans12B);
//   String Wx_Description = WxConditions[0].Forecast0;
//   Wx_Description.replace(".", ""); // remove any '.'
//   int spaceRemaining = 0, p = 0, charCount = 0, Width = lineWidth;
//   while (p < Wx_Description.length()) {
//     if (Wx_Description.substring(p, p + 1) == " ") spaceRemaining = p;
//     if (charCount > Width - 1) { // '~' is the end of line marker
//       Wx_Description = Wx_Description.substring(0, spaceRemaining) + "~" + Wx_Description.substring(spaceRemaining + 1);
//       charCount = 0;
//     }
//     p++;
//     charCount++;
//   }
//   if (WxForecast[0].Rainfall > 0) Wx_Description += " (" + String(WxForecast[0].Rainfall, 1) + String(Units == "R" ? "мм" : (Units == "M" ? "mm" : "in")) + ")";
//   String Line1 = Wx_Description.substring(0, Wx_Description.indexOf("~"));
//   String Line2 = Wx_Description.substring(Wx_Description.indexOf("~") + 1);
  
//   if (Line2.length() > 0 && Line1 != Line2) {
//     drawString(x, y, TitleCase(Line1 + " " + Line2), RIGHT);
//   } else {
//     drawString(x, y, TitleCase(Line1), RIGHT);
//   }
// }


void DisplayForecastTextSection(int x, int y) {
  setFont(OpenSans12B);

  String Wx_Description = WxConditions[0].Forecast0;
  Wx_Description.replace(".", "");  // убрать точки

  if (WxForecast[0].Rainfall > 0) {
    Wx_Description += " (" + String(WxForecast[0].Rainfall, 1) +
                      String(Language.equalsIgnoreCase("ru") ? ((Units == "R" || Units == "M") ? "мм" : "дюйм") : ((Units == "R" || Units == "M") ? "mm" : "in")) + ")";
  }

  drawString(x, y, TitleCase(Wx_Description), RIGHT);
}

void DisplayMoonEventSection(int x, int y) {
  if (!ShowMoonEventSection && !ShowMoonLatVisible) return;

  setFont(OpenSans10B);
  String result = "";

  if (ShowMoonEventSection) {
    result += getNextMoonEventStr();
  }

  if (ShowMoonLatVisible) {
    String altStr = getMoonAltitudeStr(ShowMoonLatInvisible == 1);
    if (altStr.length() > 0) {
      if (result.length() > 0 && result != "—") {
        result += ", " + String(TXT_MOON_ALT) + " " + altStr;
      } else {
        result = String(TXT_MOON_ALT) + " " + altStr;
      }
    }
  }

  if (result.length() == 0 || result == "—") return;

  addMoonIcon(x - 10, y + 12);
  drawString(x - 30, y, result, RIGHT);
}

void DisplayForecastWeather(int x, int y, int index, int fwidth) {
  x = x + fwidth * index + 15;
  int y_offset = 0;
  if (showEvents) {
    y_offset = 12;
  }
  DisplayConditionsSection(x + fwidth / 2 - 5, y + 85 + y_offset, WxForecast[index].Icon, WxForecast[index].Id, SmallIcon);
  setFont(OpenSans10B);
  drawString(x + fwidth / 2, y + 30 + y_offset, String(ConvertUnixTime(WxForecast[index].Dt + WxConditions[0].FTimezone).substring(0, 5)), CENTER);
  drawString(x + fwidth / 2, y + 130 + y_offset, String((WxForecast[index].High + WxForecast[index].Low) / 2, 0) + "°", CENTER);
}

double NormalizedMoonPhase(int d, int m, int y) {
  int j = JulianDate(d, m, y);
  //Calculate approximate moon phase
  double Phase = (j + 4.867) / 29.53059;
  return (Phase - (int) Phase);
}

void DisplayAstronomySection(int x, int y) {
  setFont(OpenSans10B);

  time_t now = time(NULL);
  struct tm *now_utc = gmtime(&now);
  int day   = now_utc->tm_mday;
  int month = now_utc->tm_mon + 1;
  int year  = now_utc->tm_year + 1900;

  // bool showEvents = (ShowMoonEventSection == 1) || (ShowMoonLatVisible == 1);
  // bool expanded   = (ShowMoonPosition == 1) || showEvents;
  if (ShowMoonPosition == 1) {

  // if (expanded) {
    DrawMoonImage(x + 20, y + 10);
    DrawMoon(x - 18, y - 28, 75, day, month, year, Hemisphere);
    drawString(x + 150, y + 25, ConvertUnixTime(WxConditions[0].Sunrise).substring(0, 5), LEFT);
    drawString(x + 150, y + 65, ConvertUnixTime(WxConditions[0].Sunset).substring(0, 5), LEFT);
    DrawSunriseImage(x + 215, y + 10);
    DrawSunsetImage(x + 215, y + 50);

    // if (showEvents) {
    //   DisplayMoonEventSection(x + 25, y + 87);
    //   drawString(x + 35, y + 107, MoonPhase(day, month, year, Hemisphere), LEFT);
    // } else {
    // drawString(x + 5, y + 102, MoonPhase(day, month, year, Hemisphere), LEFT);
    // }
  } else {
    DrawMoonImage(x + 10, y + 23);
    DrawMoon(x - 28, y - 15, 75, day, month, year, Hemisphere);
    drawString(x + 130, y + 35, ConvertUnixTime(WxConditions[0].Sunrise).substring(0, 5), LEFT);
    drawString(x + 130, y + 75, ConvertUnixTime(WxConditions[0].Sunset).substring(0, 5), LEFT);
    DrawSunriseImage(x + 195, y + 15);
    DrawSunsetImage(x + 195, y + 55);
    // drawString(x + 5, y + 102, MoonPhase(day, month, year, Hemisphere), LEFT);
  }
  drawString(x + 5, y + 102, MoonPhase(day, month, year, Hemisphere), LEFT);
}


// ====================== Moon position ======================

const float DEG2RAD = PI / 180.0f;
const float RAD2DEG = 180.0f / PI;

float toDays(time_t utc) {
  return (utc / 86400.0f) - 10957.5f;   // дни от J2000.0
}

void moonCoords(float d, float &ra, float &dec) {
  float L = 218.316f + 13.176396f * d;
  float M = 134.963f + 13.064993f * d;
  float F = 93.272f  + 13.229350f * d;

  float lon = L + 6.289f * sinf(M * DEG2RAD);
  float lat = 5.128f * sinf(F * DEG2RAD);

  float cosEps = cosf(23.439f * DEG2RAD);
  float sinEps = sinf(23.439f * DEG2RAD);
  float lonR = lon * DEG2RAD;
  float latR = lat * DEG2RAD;

  ra  = atan2f(sinf(lonR) * cosEps - tanf(latR) * sinEps, cosf(lonR));
  dec = asinf(sinf(latR) * cosEps + cosf(latR) * sinEps * sinf(lonR));
}

float moonAltitude(time_t utc, float lat, float lon) {
  float d = toDays(utc);
  float ra, dec;
  moonCoords(d, ra, dec);

  // Local sidereal time (приближённо)
  float lst = (280.160f + 360.9856235f * d) * DEG2RAD + lon * DEG2RAD;
  float ha  = lst - ra;

  float sinAlt = sinf(lat * DEG2RAD) * sinf(dec) +
                 cosf(lat * DEG2RAD) * cosf(dec) * cosf(ha);

  return asinf(constrain(sinAlt, -1.0f, 1.0f)) * RAD2DEG;
}


// ====================== Прогресс ======================
void getMoonProgress(time_t now, float lat, float lon, float &progress, float &angle) {

  time_t utc = now;

  const float HORIZON = -0.3f;
  float currentAlt = moonAltitude(utc, lat, lon);
  bool visible = (currentAlt >= HORIZON);

  // ----- Поиск предыдущего и следующего пересечения горизонта -----
  const int step   = 300;          // 5 минут
  const int window = 36 * 3600;    // ±36 часов

  time_t prevCross = 0;
  time_t nextCross = 0;
  float  prevAlt   = moonAltitude(utc - window, lat, lon);

  for (int i = 1; i <= (2 * window / step); i++) {
    time_t t = (utc - window) + (time_t)i * step;
    float alt = moonAltitude(t, lat, lon);

    if ((prevAlt < HORIZON && alt >= HORIZON) ||
        (prevAlt >= HORIZON && alt <  HORIZON)) {

      float frac = (HORIZON - prevAlt) / (alt - prevAlt + 1e-6f);
      time_t cross = t - step + (time_t)(frac * step);

      if (cross <= utc) {
        if (cross > prevCross) prevCross = cross;
      } else {
        if (nextCross == 0 || cross < nextCross) nextCross = cross;
      }
    }
    prevAlt = alt;
  }

  if (prevCross == 0 || nextCross == 0) {
    progress = 0.5f;
    angle = visible ? 270.0f : 90.0f;
    return;
  }

  float dur = (float)(nextCross - prevCross);
  if (dur < 1800.0f) dur = 12.0f * 3600.0f;

  progress = constrain((float)(utc - prevCross) / dur, 0.0f, 1.0f);

  // ----- Углы -----
  // Система координат дисплея: x вправо, y вниз
  // 0° = право, 90° = низ, 180° = лево, 270° = верх
  if (visible) {
    // 0% = лево (180°), 50% = верх (270°), 100% = право (360°/0°)
    angle = 180.0f + progress * 180.0f;
  } else {
    // 0% = право (0°), 50% = низ (90°), 100% = лево (180°)
    angle = 0.0f + progress * 180.0f;
  }

  if (angle >= 360.0f) angle -= 360.0f;
  if (angle <    0.0f) angle += 360.0f;

  // Serial.printf("UTC=%ld  alt=%.1f  visible=%d  progress=%.3f  angle=%.1f\n",
  //             utc, currentAlt, visible, progress, angle);
  // Serial.printf("prev=%ld  next=%ld  dur=%.0f\n", prevCross, nextCross, dur);
}

// Находит ближайшее событие и возвращает строку
String getNextMoonEventStr() {
  time_t utc = time(NULL);
  float lat  = Latitude.toFloat();
  float lon  = Longitude.toFloat();

  const float HORIZON = -0.3f;
  const int step   = 300;          // 5 мин
  const int window = 48 * 3600;    // ищем на 2 суток вперёд

  float prevAlt = moonAltitude(utc, lat, lon);
  time_t nextEvent = 0;
  bool   isRise    = false;

  for (int i = 1; i <= (window / step); i++) {
    time_t t = utc + (time_t)i * step;
    float alt = moonAltitude(t, lat, lon);

    // Пересечение горизонта
    if ((prevAlt < HORIZON && alt >= HORIZON) ||
        (prevAlt >= HORIZON && alt <  HORIZON)) {

      float frac = (HORIZON - prevAlt) / (alt - prevAlt + 1e-6f);
      nextEvent = t - step + (time_t)(frac * step);
      isRise = (prevAlt < HORIZON);   // снизу вверх = восход
      break;
    }
    prevAlt = alt;
  }

  if (nextEvent == 0) return "—";          // полярный случай

  String timeStr = ConvertUnixTime(nextEvent).substring(0, 5);

  if (isRise) return TXT_MOON_RISE + " " + timeStr;
  else        return TXT_MOON_SET  + " " + timeStr;
}

// Находит высоту Луны над/под горизонтом и возвращает строку
String getMoonAltitudeStr(bool allowInvisible) {
  time_t utc = time(NULL);
  float lat  = Latitude.toFloat();
  float lon  = Longitude.toFloat();
  float alt  = moonAltitude(utc, lat, lon);

  if (alt < -0.3f && !allowInvisible) return "";

  int altInt = (int)roundf(alt);
  if (altInt == 0) altInt = 0;

  char buf[12];
  snprintf(buf, sizeof(buf), "%d°", altInt);
  return String(buf);
}


void DrawMoon(int x, int y, int diameter, int dd, int mm, int yy, String hemisphere) {
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "south") Phase = 1 - Phase;
  // Draw dark part of moon
  fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, DarkGrey);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++) {
    double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);
    // Determine the edges of the lighted part of the moon
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5) {
      Xpos1 = -Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    drawLine(pW1x, pW1y, pW2x, pW2y, White);
    drawLine(pW3x, pW3y, pW4x, pW4y, White);
  }
  drawCircle(x + diameter - 1, y + diameter, diameter / 2, Black);
  
  // Добавляем полоски горизонта и рисуем индикатор положения луны над/под горизонтом
  if (ShowMoonPosition == 1) {

    // Горизонтальные полоски горизонта (слева и справа от круга)
    int horizonY = y + diameter;                    // уровень центра круга (горизонт)
    int gap      = 4;                               // небольшой зазор от края круга
    int len      = 15;                              // длина полоски (чуть больше высоты треугольника)

    drawFastHLine(x + diameter - 1 - diameter/2 - gap - len, horizonY, len, Black); // Левая полоска
    drawFastHLine(x + diameter - 1 - diameter/2 - gap - len, horizonY + 1, len, Black); // Потолще
    
    drawFastHLine(x + diameter - 1 + diameter/2 + gap,       horizonY, len, Black); // Правая полоска
    drawFastHLine(x + diameter - 1 + diameter/2 + gap,       horizonY + 1, len, Black); // Потолще

    // Координаты наблюдателя
    float lat = Latitude.toFloat();
    float lon = Longitude.toFloat();
    time_t now = time(NULL);
    float progress, angle;
    getMoonProgress(now, lat, lon, progress, angle);

    int r = diameter / 2 + 10;
    int cx = x + diameter - 1;
    int cy = y + diameter;

    float rad = angle * DEG2RAD;
    int tx = cx + (int)(r * cosf(rad));
    int ty = cy + (int)(r * sinf(rad));

    // Треугольник, направленный наружу
    int size = 11;
    float a1 = rad + 2.3f;
    float a2 = rad - 2.3f;

    int x1 = tx + (int)(size * cosf(rad));
    int y1 = ty + (int)(size * sinf(rad));
    int x2 = tx + (int)(size * 0.65f * cosf(a1));
    int y2 = ty + (int)(size * 0.65f * sinf(a1));
    int x3 = tx + (int)(size * 0.65f * cosf(a2));
    int y3 = ty + (int)(size * 0.65f * sinf(a2));

    fillTriangle(x1, y1, x2, y2, x3, y3, Black);
  }
}

String MoonPhase(int d, int m, int y, String hemisphere) {
  int c, e;
  double jd;
  int b;
  if (m < 3) {
    y--;
    m += 12;
  }
  ++m;
  c   = 365.25 * y;
  e   = 30.6  * m;
  jd  = c + e + d - 694039.09;     /* jd is total days elapsed */
  jd /= 29.53059;                        /* divide by the moon cycle (29.53 days) */
  b   = jd;                              /* int(jd) -> b, take integer part of jd */
  jd -= b;                               /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;                /* scale fraction from 0-8 and round by adding 0.5 */
  b   = b & 7;                           /* 0 and 8 are the same phase so modulo 8 for 0 */
  if (hemisphere == "south") b = 7 - b;
  if (b == 0) return TXT_MOON_NEW;              // New;              0%  illuminated
  if (b == 1) return TXT_MOON_WAXING_CRESCENT;  // Waxing crescent; 25%  illuminated
  if (b == 2) return TXT_MOON_FIRST_QUARTER;    // First quarter;   50%  illuminated
  if (b == 3) return TXT_MOON_WAXING_GIBBOUS;   // Waxing gibbous;  75%  illuminated
  if (b == 4) return TXT_MOON_FULL;             // Full;            100% illuminated
  if (b == 5) return TXT_MOON_WANING_GIBBOUS;   // Waning gibbous;  75%  illuminated
  if (b == 6) return TXT_MOON_THIRD_QUARTER;    // Third quarter;   50%  illuminated
  if (b == 7) return TXT_MOON_WANING_CRESCENT;  // Waning crescent; 25%  illuminated
  return "";
}

void DisplayForecastSection(int x, int y) {
  int f = 0;
  do {
    DisplayForecastWeather(x, y, f, 82); // x,y cordinates, forecatsr number, spacing width
    f++;
  } while (f < 8);
}


void DisplayGraphSection(int x, int y) {
  int r = 0;
  do { // Pre-load temporary arrays with with data - because C parses by reference and remember that[1] has already been converted to I units
    if (Units == "I") {
        pressure_readings[r] = WxForecast[r].Pressure * 0.02953;      // inHg
    } else if (Units == "R") {
        pressure_readings[r] = WxForecast[r].Pressure * 0.75006;      // mmHg
    } else {
        pressure_readings[r] = WxForecast[r].Pressure;                // hPa
    }
    if (Units == "I") rain_readings[r]     = WxForecast[r].Rainfall * 0.0393701; else rain_readings[r]     = WxForecast[r].Rainfall;
    if (Units == "I") snow_readings[r]     = WxForecast[r].Snowfall * 0.0393701; else snow_readings[r]     = WxForecast[r].Snowfall;
    temperature_readings[r]                = WxForecast[r].Temperature;
    humidity_readings[r]                   = WxForecast[r].Humidity;
    r++;
  } while (r < max_readings);
  int gwidth = 175, gheight = 100;
  int gx = (SCREEN_WIDTH - gwidth * 4) / 5 + 8;
  int gy = (SCREEN_HEIGHT - gheight - 30);
  int gap = gwidth + gx;
  
  bool percip_type_array[max_readings];
  bool non_negative;

  // 1. Температура
  DrawGraph(gx + 0 * gap, gy, gwidth, gheight, 10, 30,    Units == "R" || Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature_readings, max_readings, autoscale_on, barchart_off, NULL, false);
  
  // 2. Влажность
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 0, 100,   TXT_HUMIDITY_PERCENT, humidity_readings, max_readings, autoscale_off, barchart_off, NULL, true);

  // 3. Осадки
  float precip_readings[max_readings];
  float sumRain = SumOfPrecip(rain_readings, max_readings);
  float sumSnow = SumOfPrecip(snow_readings, max_readings);
  float yMaxSnow = (Units == "R" || Units == "M") ? 30.0f : 2.0f;
  // 3.1. Осадков нет
  if (sumRain < 0.001f && sumSnow < 0.001f) {
      float empty_readings[max_readings];
      for (int i = 0; i < max_readings; i++) {
          empty_readings[i] = 0.0f;
      }
      DrawGraph(gx + 2 * gap + 5, gy, gwidth, gheight, 0, yMaxSnow,
                TXT_NO_PRECIP,
                empty_readings, max_readings, autoscale_on, barchart_on, NULL, true);
  }
  // 3.2. Только дождь
  else if (sumRain >= 0.001f && sumSnow < 0.001f) {
      DrawGraph(gx + 2 * gap + 5, gy, gwidth, gheight, 0, yMaxSnow,
                Units == "R" || Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN,
                rain_readings, max_readings, autoscale_on, barchart_on, NULL, true);
  }
  // 3.3. Только снег
  else if (sumSnow >= 0.001f && sumRain < 0.001f) {
      DrawGraph(gx + 2 * gap + 5, gy, gwidth, gheight, 0, yMaxSnow,
                Units == "R" || Units == "M" ? TXT_SNOWFALL_MM : TXT_SNOWFALL_IN,
                snow_readings, max_readings, autoscale_on, barchart_on, NULL, true);
  }
  // 3.4. Есть и дождь и снег
  else {
      for (int i = 0; i < max_readings; i++) {
          if (snow_readings[i] > 0.0f) {
              precip_readings[i] = snow_readings[i];
              percip_type_array[i] = false;  // снег - серый бар
          } else {
              precip_readings[i] = rain_readings[i];
              percip_type_array[i] = true;   // дождь - черный бар
          }
      }
      DrawGraph(gx + 2 * gap + 5, gy, gwidth, gheight, 0, yMaxSnow,
                Units == "R" || Units == "M" ? TXT_PRECIP_MM : TXT_PRECIP_IN,
                precip_readings, max_readings, autoscale_on, barchart_on, percip_type_array, true);
  }

  // 4. Давление
  DrawGraph(gx + 3 * gap, gy, gwidth, gheight, 
          Units == "R" ? 650 : (Units == "M" ? 900 : 950), 
          Units == "R" ? 850 : (Units == "M" ? 1050 : 1050), 
          Units == "R" ? TXT_PRESSURE_MM : (Units == "M" ? TXT_PRESSURE_HPA : TXT_PRESSURE_IN), 
          pressure_readings, max_readings, autoscale_on, barchart_off, NULL, false);
}


void DisplayConditionsSection(int x, int y, String IconName, int Id, bool IconSize) {
  Serial.println("Icon name: " + IconName);
  // y -= 5;
  if      (IconName == "01d" || IconName == "01n") ClearSky(x, y, IconSize, IconName);              // ясное небо
  else if (IconName == "02d" || IconName == "02n") FewClouds(x, y, IconSize, IconName);             // небольшая облачность
  else if (IconName == "03d" || IconName == "03n") ScatteredClouds(x, y, IconSize, IconName);       // облачно с прояснениями (большое облако с маленьким солнцем)
  else if (IconName == "04d" || IconName == "04n") {
      if (Id == 804)
          OvercastClouds(x, y, IconSize, IconName);                                                 // сплошная облачность (большое облако и два маленьких облака)
      else
          BrokenClouds(x, y, IconSize, IconName);                                                   // облачно (большое облако с маленьким облаком)
  }
  else if (IconName == "09d" || IconName == "09n" || IconName == "10d" || IconName == "10n") {
      if (Id == 300 || Id == 310)
          LightDrizzle(x, y, IconSize, IconName);                                                   // light drizzle (лёгкая морось)
      else if (Id == 301 || Id == 311 || Id == 313 || Id == 321)
          Drizzle(x, y, IconSize, IconName);                                                        // drizzle (морось)
      else if (Id == 302 || Id == 314)
          HeavyDrizzle(x, y, IconSize, IconName);                                                   // heavy drizzle (сильная морось)
      else if (Id == 500 || Id == 520)
          LightRain(x, y, IconSize, IconName);                                                      // лёгкий дождь
      else if (Id == 502 || Id == 503 || Id == 504 || Id == 522 || Id == 531)
          HeavyRain(x, y, IconSize, IconName);                                                      // сильный дождь
      else
          Rain(x, y, IconSize, IconName);                                                           // прочий дождь (Moderate (умеренный) + freezing (ледяной) + 521)
  }
  else if (IconName == "11d" || IconName == "11n") {
      if (Id == 200 || Id == 210)
          LightThunderstorms(x, y, IconSize, IconName);                                             // thunderstorm with light rain, light thunderstorm
      else if (Id == 202 || Id == 212)
          HeavyThunderstorms(x, y, IconSize, IconName);                                             // thunderstorm with heavy rain, heavy thunderstorm
      else
          Thunderstorms(x, y, IconSize, IconName);                                                  // прочая гроза
  }
  else if (IconName == "13d" || IconName == "13n") {
      if (Id == 600 || Id == 620)
          LightSnow(x, y, IconSize, IconName);                                                      // лёгкий снег
      else if (Id == 602 || Id == 622)
          HeavySnow(x, y, IconSize, IconName);                                                      // сильный снег
      else if (Id == 615 || Id == 616)
          RainSnow(x, y, IconSize, IconName);                                                       // снег с дождем
      else
          Snow(x, y, IconSize, IconName);                                                           // прочий снег
  }
  else if (IconName == "50d" || IconName == "50n") {
      if (Id == 771 || Id == 781)
          Tornado(x, y, IconSize, IconName);                                                        // шквалы, торнадо
      else
          Mist(x, y, IconSize, IconName);                                                           // дымка, мгла, туман, пыль, песок
  }
  else    Nodata(x, y, IconSize, IconName);
}

void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  // Позиция стрелки на окружности
  float dx = (asize + plength - 5) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize + plength - 5) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;         float y1 = plength;
  float x2 = pwidth / 2;  float y2 = pwidth / 2;
  float x3 = -pwidth / 2; float y3 = pwidth / 2;
  // Направление треугольника
  float angle = (aangle + 180 - 4) * PI / 180 - 135;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, Black);
}

void DrawSegment(int x, int y, int o1, int o2, int o3, int o4, int o11, int o12, int o13, int o14) {
  drawLine(x + o1,  y + o2,  x + o3,  y + o4,  Black);
  drawLine(x + o11, y + o12, x + o13, y + o14, Black);
}

void DrawPressureAndTrend(int x, int y, float pressure, String slope) {
  // --- Давление ---
  setFont(OpenSans24B);
  String pressureStr = String(pressure, (Units == "R" || Units == "M" ? 0 : 1));
  drawString(x + 80, y - 15, pressureStr, LEFT);
  // Примерный расчёт ширины
  int char_width = 22;
  int text_width = pressureStr.length() * char_width;
  // --- Единицы ---
  setFont(OpenSans8B);
  String unitStr =
    Units == "R" ? (Language.equalsIgnoreCase("ru") ? "мм рт.ст" : "mmHg") :
    Units == "M" ? "hPa" :
                   "inHg";
  int unitYOffset = 0;
  if (Units == "M") unitYOffset = 5; 
  int unitXOffset = 30;
  if (Units == "I") unitXOffset = 20; 
  drawString(x + 80 + text_width + unitXOffset, y + 5 + unitYOffset, unitStr, LEFT);
  // --- Тренд (утолщённый) ---
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++) {
      if (slope == "+")
        DrawSegment(x + 55 - i, y + j, 0, 0, 8, -8, 8, -8, 16, 0);
      else if (slope == "0")
        DrawSegment(x + 55 - i, y + j, 8, -8, 16, 0, 8, 8, 16, 0);
      else if (slope == "-")
        DrawSegment(x + 55 - i, y + j, 0, 0, 8, 8, 8, 8, 16, 0);
    }
}

void DrawPressureMeasure(int x, int y, float pressure, String slope) {
  // drawString(x + 60, y + 20, (Units == "R" ? " mmHg" : (Units == "M" ? " hPa" : " inHg")), LEFT);
  drawString(x + 60, y + 20, (Units == "R" ? (Language.equalsIgnoreCase("ru") ? " мм" : " mmHg") : Units == "M" ? " hPa" : " inHg"), LEFT);
}

void DisplayStatusSection(int x, int y, int rssi) {
  setFont(OpenSans8B);
  DrawRSSI(x + 305, y + 15, rssi);
  DrawBattery(x + 150, y);
}

void DrawRSSI(int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20)  WIFIsignal = 30; //            <-20dbm displays 5-bars
    if (_rssi <= -40)  WIFIsignal = 24; //  -40dbm to  -21dbm displays 4-bars
    if (_rssi <= -60)  WIFIsignal = 18; //  -60dbm to  -41dbm displays 3-bars
    if (_rssi <= -80)  WIFIsignal = 12; //  -80dbm to  -61dbm displays 2-bars
    if (_rssi <= -100) WIFIsignal = 6;  // -100dbm to  -81dbm displays 1-bar
    
    if (rssi != 0) 
      fillRect(x + xpos * 8, y - WIFIsignal, 6, WIFIsignal, Black);
    else // draw empty bars
      drawRect(x + xpos * 8, y - WIFIsignal, 6, WIFIsignal, Black);
    xpos++;
  }
  if (rssi == 0) 
    drawString(x + 28, y - 18, "x", LEFT);
}

boolean UpdateLocalTime() {
  struct tm timeinfo;
  char   time_output[30], day_output[30], update_time[30];
  while (!getLocalTime(&timeinfo, 5000)) { // Wait for 5-sec for time to synchronise
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  Serial.println(&timeinfo, "%a %b %d %Y   %H:%M");      // Displays: Saturday, June 24 2017 14:05
  if (Units == "M" || Units == "R") {
    sprintf(day_output, "%s, %02u %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    strftime(update_time, sizeof(update_time), "%H:%M", &timeinfo);  // Creates: '@ 14:05'   and change from 30 to 8 <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    sprintf(time_output, "%s", update_time);
  }
  else
  {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo); // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%I:%M %p", &timeinfo);        // Creates: '@ 02:05pm'
    sprintf(time_output, "%s", update_time);
  }
  Date_str = day_output;
  Time_str = time_output;
  return true;
}

void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("eFuse Vref:%u mV\n", adc_chars.vref);
    vref = adc_chars.vref;
  }
  float voltage = analogRead(36) / 4096.0 * 6.566 * (vref / 1000.0);
  if (voltage > 1 ) { // Only display if there is a valid reading
    Serial.println("\nVoltage = " + String(voltage));
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.20) percentage = 0;  // orig 3.5
    drawRect(x + 25, y - 14, 40, 15, Black);
    fillRect(x + 65, y - 10, 4, 7, Black);
    fillRect(x + 27, y - 12, 36 * percentage / 100.0, 11, Black);
    drawString(x + 85, y - 14, String(percentage) + "%  " + String(voltage, 1) + "v", LEFT);
  }
}

// ######################################## Weather Symbol ########################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  fillCircle(x - scale * 3, y, scale, Black);                                                              // Left most circle
  fillCircle(x + scale * 3, y, scale, Black);                                                              // Right most circle
  fillCircle(x - scale, y - scale, scale * 1.4, Black);                                                    // left middle upper circle
  fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, Black);                                       // Right middle upper circle
  fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, Black);                                 // Upper and lower lines
  fillCircle(x - scale * 3, y, scale - linesize, White);                                                   // Clear left most circle
  fillCircle(x + scale * 3, y, scale - linesize, White);                                                   // Clear right most circle
  fillCircle(x - scale, y - scale, scale * 1.4 - linesize, White);                                         // left middle upper circle
  fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, White);                            // Right middle upper circle
  fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, White); // Upper and lower lines
}

void addlightdrizzle(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 15, y + 12, ",'  ,'", LEFT);
  }
  else
  {
    setFont(OpenSans18B);
    drawString(x - 30, y + 25, ".'  .'", LEFT);
  }
}

void adddrizzle(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 20, y + 12, ".' .' .'", LEFT);
  }
  else
  {
    setFont(OpenSans18B);
    drawString(x - 38, y + 25, ",' ,' ,'", LEFT);
  }
}

void addheavydrizzle(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 20, y + 12, ".'.'.'.'", LEFT);
  }
  else
  {
    setFont(OpenSans18B);
    drawString(x - 40, y + 25, ".'.'.'.'", LEFT);
  }
}

void addlightrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 15, y + 12, "/   /", LEFT);
  }
  else
  {
    setFont(OpenSans18B);
    drawString(x - 30, y + 25, "/   /", LEFT);
  }
}

void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 15, y + 12, "/ / /", LEFT);
  }
  else
  {
    setFont(OpenSans18B);
    drawString(x - 30, y + 25, "/ / /", LEFT);
  }
}

void addheavyrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 20, y + 12, "/ / / /", LEFT);
  }
  else
  {
    setFont(OpenSans18B);
    drawString(x - 40, y + 25, "/ / / /", LEFT);
  }
}

void addrainsnow(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 30, y + 15, "* / * / *", LEFT);
  }
  else
  {
    setFont(OpenSans18B);
    drawString(x - 60, y + 30, "* / * / *", LEFT);
  }
}

void addlightsnow(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 15, y + 15, "*   *", LEFT);
  }
  else
  {
    setFont(OpenSans18B);
    drawString(x - 35, y + 30, "*   *", LEFT);
  }
}

void addsnow(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 20, y + 15, "*  *  *", LEFT);
  }
  else
  {
    setFont(OpenSans18B);
    drawString(x - 45, y + 30, "*  *  *", LEFT);
  }
}

void addheavysnow(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 25, y + 15, "* * * *", LEFT);
  }
  else
  {
    setFont(OpenSans18B);
    drawString(x - 60, y + 30, "* * * *", LEFT);
  }
}

void addlighttstorm(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    x += 10;
  } else {
    x += 30;
  }
  y = y + scale / 2;
  for (int i = 1; i < 3; i++) {
    drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, Black);
  }
}

void addtstorm(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) {
    x += 5;
  } else {
    x += 15;
  }
  y = y + scale / 2;
  for (int i = 1; i < 4; i++) {
    drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, Black);
  }
}

void addheavytstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 1; i < 5; i++) {
    drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, Black);
  }
}

void addsun(int x, int y, int scale, bool IconSize) {
  int linesize = 5;
  fillRect(x - scale * 2, y, scale * 4, linesize, Black);
  fillRect(x, y - scale * 2, linesize, scale * 4, Black);
  DrawAngledLine(x + scale * 1.4, y + scale * 1.4, (x - scale * 1.4), (y - scale * 1.4), linesize * 1.5, Black); // Actually sqrt(2) but 1.4 is good enough
  DrawAngledLine(x - scale * 1.4, y + scale * 1.4, (x + scale * 1.4), (y - scale * 1.4), linesize * 1.5, Black);
  fillCircle(x, y, scale * 1.3, White);
  fillCircle(x, y, scale, Black);
  fillCircle(x, y, scale - linesize, White);
}

void addfog(int x, int y, int scale, int linesize, bool IconSize) {
  if (IconSize == SmallIcon) linesize = 3;
  for (int i = 0; i < 6; i++) {
    fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, Black);
    fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, Black);
    fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize, Black);
  }
}

void addtornado(int x, int y, int scale, bool IconSize) {
    x -= 20;
    int dy = 15;
    if (IconSize == LargeIcon) dy = 30;
    if (IconSize == LargeIcon) x -= 10;
    auto fatLine = [&](int x0, int y0, int x1, int y1) {
        drawLine(x0, y0, x1, y1, Black);
        drawLine(x0, y0 + 1, x1, y1 + 1, Black);
        drawLine(x0, y0 - 1, x1, y1 - 1, Black);
    };
    int levels = 4;                                                         // количество горизонтальных волн конуса
    for (int i = 0; i < levels; i++) {
        float len = scale * (4.0 - i);
        float offset_y = i * scale * 0.6 + dy;
        float mid_x = x + len / 2.0;
        fatLine(x, y + offset_y, mid_x, y + offset_y - scale * 0.3);        // левая часть волны
        fatLine(mid_x, y + offset_y - scale * 0.3, x + len, y + offset_y);  // правая часть волны
    }
}

void DrawAngledLine(int x, int y, int x1, int y1, int size, int color) {
  int dx = (size / 2.0) * (x - x1) / sqrt(sq(x - x1) + sq(y - y1));
  int dy = (size / 2.0) * (y - y1) / sqrt(sq(x - x1) + sq(y - y1));
  fillTriangle(x + dx, y - dy, x - dx,  y + dy,  x1 + dx, y1 - dy, color);
  fillTriangle(x - dx, y + dy, x1 - dx, y1 + dy, x1 + dx, y1 - dy, color);
}

// ################# WEATHER ICONS ###########################################

void ClearSky(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += (IconSize ? 0 : 10);
  addsun(x, y - (IconSize ? 0 : 10), scale * (IconSize ? 1.7 : 1.2), IconSize);
}

void FewClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  y += 10;
  if (IconSize == LargeIcon) scale = Large;
  addcloud(x - (IconSize ? 35 : 15), y * (IconSize ? 0.75 : 0.93), scale * (IconSize ? 0.7 : 0.6), linesize);
  addsun((x + (IconSize ? 20 : 0)), y - (IconSize ? 0 : 10), scale * (IconSize ? 1.3 : 1), IconSize);
}

void ScatteredClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  y += 10;
  if (IconSize == LargeIcon) scale = Large;
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
}

void BrokenClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  y += 10;
  if (IconSize == LargeIcon) scale = Large;
  addcloud(x - (IconSize ? 35 : 15), y * (IconSize ? 0.75 : 0.93), scale * (IconSize ? 0.7 : 0.6), linesize); // Cloud top left
  addcloud(x, y, scale * 0.75, linesize);                                                                     // Main cloud
}

void OvercastClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  y += 10;
  if (IconSize == LargeIcon) scale = Large;
  addcloud(x - (IconSize ? 35 : 15), y * (IconSize ? 0.75 : 0.93), scale * (IconSize ? 0.7 : 0.6), linesize); // Cloud top left
  addcloud(x + (IconSize ? 35 : 5), y * (IconSize ? 0.75 : 0.95), scale * (IconSize ? 0.7 : 0.6), linesize);  // Cloud top right
  addcloud(x, y, scale * 0.75, linesize);                                                                     // Main cloud
}

void LightDrizzle(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 10;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addlightdrizzle(x, y, scale, IconSize);
}

void Drizzle(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 10;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  adddrizzle(x, y, scale, IconSize);
}

void HeavyDrizzle(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 10;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addheavydrizzle(x, y, scale, IconSize);
}

void LightRain(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 10;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addlightrain(x, y, scale, IconSize);
}

void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  y += 10;
  if (IconSize == LargeIcon) scale = Large;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addrain(x, y, scale, IconSize);
}

void HeavyRain(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 10;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addheavyrain(x, y, scale, IconSize);
}

void LightThunderstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 5;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addlighttstorm(x, y, scale, IconSize);
}

void Thunderstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 5;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addtstorm(x, y, scale, IconSize);
}

void Tornado(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 5;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addtornado(x, y, scale, IconSize);
}

void HeavyThunderstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 5;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addheavytstorm(x, y, scale);
}

void RainSnow(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 10;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addrainsnow(x, y, scale, IconSize);
}

void LightSnow(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 10;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addlightsnow(x, y, scale, IconSize);
}

void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 10;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addsnow(x, y, scale, IconSize);
}

void HeavySnow(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  y += 10;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addheavysnow(x, y, scale, IconSize);
}

void Mist(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 5;
  if (IconName.endsWith("n")) addmoon(x, y, IconSize);
  if (IconSize == LargeIcon) scale = Large;
  addsun(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addfog(x, y, scale, linesize, IconSize);
}

void addmoon(int x, int y, bool IconSize) {
  int xOffset = 65;
  int yOffset = 12;
  if (IconSize == LargeIcon) {
    xOffset = 130;
    yOffset = -40;
  }
  fillCircle(x - 28 + xOffset, y - 37 + yOffset, uint16_t(Small * 1.0), Black);
  fillCircle(x - 16 + xOffset, y - 37 + yOffset, uint16_t(Small * 1.6), White);
}

void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) setFont(OpenSans24B); else setFont(OpenSans12B);
  drawString(x - 3, y - 10, "?", CENTER);
}

// ################# VISIBILITY SECTION ###########################################

void addcoversun(int x, int y, int scale) {
  int circleThickness = 2;        // толщина круга
  int rayThickness    = 3;        // толщина лучей
  float rayGap        = scale * 0.25;
  float rayLength     = scale * 0.55;
  int inner = scale + rayGap;
  int outer = inner + rayLength;
  // ----- КРУГ (контурный) -----
  for (int i = 0; i < circleThickness; i++)
      drawCircle(x, y, scale - i, Black);
  // ----- ВЕРТИКАЛЬНЫЕ ЛУЧИ -----
  fillRect(x - rayThickness/2, y - outer, rayThickness, outer - inner, Black);
  fillRect(x - rayThickness/2, y + inner, rayThickness, outer - inner, Black);
  // ----- ГОРИЗОНТАЛЬНЫЕ ЛУЧИ -----
  fillRect(x - outer, y - rayThickness/2, outer - inner, rayThickness, Black);
  fillRect(x + inner, y - rayThickness/2, outer - inner, rayThickness, Black);
  // ----- ДИАГОНАЛИ (через маленькие квадраты) -----
  int steps = rayLength;   // сколько "точек" рисовать
  for (int i = 0; i < steps; i++) {
    int dx = inner + i;
    int dy = inner + i;
    // ↘
    fillRect(x + dx - rayThickness/2, y + dy - rayThickness/2, rayThickness, rayThickness, Black);
    // ↗
    fillRect(x + dx - rayThickness/2, y - dy - rayThickness/2, rayThickness, rayThickness, Black);
    // ↙
    fillRect(x - dx - rayThickness/2, y + dy - rayThickness/2, rayThickness, rayThickness, Black);
    // ↖
    fillRect(x - dx - rayThickness/2, y - dy - rayThickness/2, rayThickness, rayThickness, Black);
  }
}

void addwind(int x, int y, float scale, float gust_value) {
  // if (IconSize == SmallIcon) linesize = 3;
  auto fatLine = [&](int x0, int y0, int x1, int y1) {
    drawLine(x0, y0,     x1, y1,     Black);
    drawLine(x0, y0 + 1, x1, y1 + 1, Black);
    drawLine(x0, y0 - 1, x1, y1 - 1, Black);
  };
  float gust_ms = gust_value;
  if (Units == "I") {
      // mph → m/s
      gust_ms = gust_value * 0.44704f;  // точный коэффициент 1 mph ≈ 0.44704 m/s
  }
  // === Определяем сколько базовых волн рисовать ===
  int waveCount = 3;       // обычно 3
  if (gust_ms < 5.0)          waveCount = 2; // слабый ветер
  else if (gust_ms < 10.0)    waveCount = 3; // средний ветер
  else                        waveCount = 4; // сильный ветер
  
  // === Базовые волны ===
  for (int i = 0; i < waveCount; i++) {
    float offset_y = i * scale * 1.15;
    float len      = scale * (3.4 + i * 0.6);
    fatLine(x, y + offset_y, x + len * 0.55, y + offset_y - scale * 0.4);
    fatLine(x + len * 0.55, y + offset_y - scale * 0.4, x + len * 0.82, y + offset_y + scale * 0.35);
    fatLine(x + len * 0.82, y + offset_y + scale * 0.35, x + len, y + offset_y - scale * 0.25);
  }
  // === Ветер >= 15 м/с - Верхний всплеск ===
  if (gust_ms >= 15.0) {
    fatLine(x + scale * 2.8, y - scale * 1.1, x + scale * 4.2, y - scale * 2.2);
    fatLine(x + scale * 4.0, y - scale * 2.0, x + scale * 4.8, y - scale * 1.4);
  }
  // // === Ветер >= 20 м/с — ещё верхний акцент и нижний порыв ===
  // if (gust_ms >= 20.0) {
  //   fatLine(x + scale * 0.8, y - scale * 0.3, x + scale * 2.4, y - scale * 1.8);
  //   fatLine(x + scale * 1.8, y + scale * 3.4, x + scale * 3.5, y + scale * 5.0);
  //   fatLine(x + scale * 3.3, y + scale * 4.7, x + scale * 4.6, y + scale * 3.9);
  // }
}

void Visibility(int x, int y, String Visibility) {
  float start_angle = 0.52, end_angle = 2.61, Offset = 10;
  int r = 14;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    drawPixel(x + r * cos(i), y - r / 2 + r * sin(i) + Offset, Black);
    drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i) + Offset, Black);
  }
  start_angle = 3.61; end_angle = 5.78;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    drawPixel(x + r * cos(i), y + r / 2 + r * sin(i) + Offset, Black);
    drawPixel(x + r * cos(i), 1 + y + r / 2 + r * sin(i) + Offset, Black);
  }
  fillCircle(x, y + Offset, r / 4, Black);
  drawString(x + 20, y, Visibility, LEFT);
}

void ClearSkyCover(int x, int y, int CloudCover) {
  addcoversun(x, y + 5, Small); // Main sun
  drawString(x + 30, y, String(CloudCover) + "%", LEFT);
}

void FewCloudsCover(int x, int y, int CloudCover) {
  addcoversun(x, y + 5, Small); // Main sun
  addcloud(x - 15, y, Small * 0.4, 2); // Cloud top left
  drawString(x + 30, y, String(CloudCover) + "%", LEFT);
}

void PartlyCloudyCover(int x, int y, int CloudCover) {
  addcoversun(x - 9, y, Small * 0.7); // Sun top left
  addcloud(x, y + 15, Small * 0.6, 2); // Main cloud
  drawString(x + 30, y, String(CloudCover) + "%", LEFT);
}

void MostlyCloudyCover(int x, int y, int CloudCover) {
  addcloud(x - 9, y,     Small * 0.3, 2); // Cloud top left
  addcloud(x, y + 15,    Small * 0.6, 2); // Main cloud
  drawString(x + 30, y, String(CloudCover) + "%", LEFT);
}

void CloudCover(int x, int y, int CloudCover) {
  addcloud(x - 9, y,     Small * 0.3, 2); // Cloud top left
  addcloud(x + 3, y - 2, Small * 0.3, 2); // Cloud top right
  addcloud(x, y + 15,    Small * 0.6, 2); // Main cloud
  drawString(x + 30, y, String(CloudCover) + "%", LEFT);
}

void WindGust(int x, int y, float Windgust) {
  addwind(x - 9, y, Small * 0.6, Windgust);
  int unitYOffset = 0;
  if (Units == "I") unitYOffset = 7;
  drawString(x + 25, y - unitYOffset, String(Windgust, 1) + " " +
              (Language.equalsIgnoreCase("ru") ? ((Units == "R" || Units == "M") ? "м/с" : "миль/ч") : ((Units == "R" || Units == "M") ? "m/s" : "mph")), LEFT);
}

// ################# MOON SECTION ###########################################

void DrawMoonImage(int x, int y) {
  Rect_t area = {
    .x = x, .y = y, .width  = moon_width, .height =  moon_height
  };
  epd_draw_grayscale_image(area, (uint8_t *) moon_data);
}

void DrawSunriseImage(int x, int y) {
  Rect_t area = {
    .x = x, .y = y, .width  = sunrise_width, .height =  sunrise_height
  };
  epd_draw_grayscale_image(area, (uint8_t *) sunrise_data);
}

void DrawSunsetImage(int x, int y) {
  Rect_t area = {
    .x = x, .y = y, .width  = sunset_width, .height =  sunset_height
  };
  epd_draw_grayscale_image(area, (uint8_t *) sunset_data);
}

void addMoonIcon(int x, int y) {
  int r = 8;
  fillCircle(x, y, r, Black);
  fillCircle(x + 4, y - 1, r - 1, White);
  drawCircle(x, y, r, Black);
}

// ################# GRAPH SECTION ###########################################

/* (C) D L BIRD
    This function will draw a graph on a ePaper/TFT/LCD display using data from an array containing data to be graphed.
    The variable 'max_readings' determines the maximum number of data elements for each array. Call it with the following parametric data:
    x_pos-the x axis top-left position of the graph
    y_pos-the y-axis top-left position of the graph, e.g. 100, 200 would draw the graph 100 pixels along and 200 pixels down from the top-left of the screen
    width-the width of the graph in pixels
    height-height of the graph in pixels
    Y1_Max-sets the scale of plotted data, for example 5000 would scale all data to a Y-axis of 5000 maximum
    data_array1 is parsed by value, externally they can be called anything else, e.g. within the routine it is called data_array1, but externally could be temperature_readings
    auto_scale-a logical value (TRUE or FALSE) that switches the Y-axis autoscale On or Off
    barchart_on-a logical value (TRUE or FALSE) that switches the drawing mode between barhcart and line graph
    barchart_colour-a sets the title and graph plotting colour
    If called with Y!_Max value of 500 and the data never goes above 500, then autoscale will retain a 0-500 Y scale, if on, the scale increases/decreases to match the data.
    auto_scale_margin, e.g. if set to 1000 then autoscale increments the scale by 1000 steps.
*/

void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int readings, boolean auto_scale, boolean barchart_mode, bool percip_type_array[], bool non_negative) {

#define auto_scale_margin 0 // Sets the autoscale increment, so axis steps up fter a change of e.g. 3
#define y_minor_axis 5      // 5 y-axis division markers
  setFont(OpenSans10B);
  float maxYscale = -10000;
  float minYscale =  10000;
  int last_x, last_y;
  int days = max_readings / 8;
  float x2, y2;
  if (auto_scale == true) {
    for (int i = 0; i < readings; i++ ) {
      if (DataArray[i] >= maxYscale) maxYscale = DataArray[i];
      if (DataArray[i] <= minYscale) minYscale = DataArray[i];
    }
    float range = maxYscale - minYscale;
    if (range < 0.1f) {
        range = 0.1f;
        maxYscale = minYscale + range;
    }
    float margin = range * 0.1f;
    Y1Max = maxYscale + margin;
    if (non_negative) {
        Y1Min = minYscale;
    } else {
        Y1Min = minYscale - margin;
    }
    if (non_negative && Y1Min < 0.0f) Y1Min = 0.0f;
  }
  // Draw the graph
  last_x = x_pos + 1;
  last_y = y_pos + (Y1Max - constrain(DataArray[1], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;
  drawRect(x_pos, y_pos, gwidth + 3, gheight + 2, Grey);
  drawString(x_pos - 20 + gwidth / 2, y_pos - 28, title, CENTER);
  for (int gx = 1; gx < readings; gx++) {
    x2 = x_pos + gx * gwidth / (readings - 1) - 1 ; // max_readings is the global variable that sets the maximum data that can be plotted
    y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
    if (barchart_mode) {
      if (percip_type_array == NULL) {
          fillRect(last_x + 2, y2, (gwidth / readings) - 1, y_pos + gheight - y2 + 2, Black);
      } else {
          if (percip_type_array[gx]) {
              fillRect(last_x + 2, y2, (gwidth / readings) - 1, y_pos + gheight - y2 + 2, Black);
          } else {
              drawRect(last_x + 2, y2, (gwidth / readings) - 1, y_pos + gheight - y2 + 2, Grey);
          }
      }
    } else {
      drawLine(last_x, last_y - 1, x2, y2 - 1, Black); // Three lines for hi-res display
      drawLine(last_x, last_y, x2, y2, Black);
      drawLine(last_x, last_y + 1, x2, y2 + 1, Black);
    }
    last_x = x2;
    last_y = y2;
  }
  //Draw the Y-axis scale
#define number_of_dashes 20
  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
    for (int j = 0; j < number_of_dashes; j++) { // Draw dashed graph grid lines
      if (spacing < y_minor_axis) drawFastHLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes), Grey);
    }

    float value = Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing;
    
    if (abs(value) < 0.05) value = 0.0;

    if (title == TXT_HUMIDITY_PERCENT) {
        drawString(x_pos - 7, y_pos + gheight * spacing / y_minor_axis - 5, String((int)value), RIGHT);
    }
    // Imperial осадки → 2 знака
    else if ((title == TXT_PRECIP_IN || title == TXT_SNOWFALL_IN) && Units == "I") {
        drawString(x_pos - 10, y_pos + gheight * spacing / y_minor_axis - 5, String(value, 2), RIGHT);
    }
    // Давление в дюймах → 2 знака
    else if (title == TXT_PRESSURE_IN) {
        drawString(x_pos - 10, y_pos + gheight * spacing / y_minor_axis - 5, String(value, 1), RIGHT);
    }
    // Малые значения → 1 знак
    else if (value < 10) {
        drawString(x_pos - 10, y_pos + gheight * spacing / y_minor_axis - 5, String(value, 1), RIGHT);
    }
    // Остальное → целые
    else {
        drawString(x_pos - 7, y_pos + gheight * spacing / y_minor_axis - 5, String(value, 0), RIGHT);
    }
  }
  for (int i = 0; i < days; i++) {
    drawString(x_pos + gwidth / days / 2 + gwidth / days * i, y_pos + gheight + 10, String(i + 1) + "d", CENTER);
    if (i < days - 1) drawFastVLine(x_pos + gwidth / days * i + gwidth / days, y_pos, gheight, LightGrey);
  }
}

void drawString(int32_t x, int32_t y, String text, alignment align) {
  char * data  = const_cast<char*>(text.c_str());
  int32_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  int32_t w, h;
  int32_t xx = x, yy = y;
  get_text_bounds(&currentFont, data, &xx, &yy, &x1, &y1, &w, &h, NULL);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  int32_t cursor_y = y + h;
  write_string(&currentFont, data, &x, &cursor_y, framebuffer);
}

void fillCircle(int x, int y, int r, uint8_t color) {
  epd_fill_circle(x, y, r, color, framebuffer);
}

void drawFastHLine(int16_t x0, int16_t y0, int length, uint16_t color) {
  epd_draw_hline(x0, y0, length, color, framebuffer);
}

void drawFastVLine(int16_t x0, int16_t y0, int length, uint16_t color) {
  epd_draw_vline(x0, y0, length, color, framebuffer);
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  epd_write_line(x0, y0, x1, y1, color, framebuffer);
}

void drawCircle(int x0, int y0, int r, uint8_t color) {
  epd_draw_circle(x0, y0, r, color, framebuffer);
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  epd_draw_rect(x, y, w, h, color, framebuffer);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  epd_fill_rect(x, y, w, h, color, framebuffer);
}

void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  int16_t x2, int16_t y2, uint16_t color) {
  epd_fill_triangle(x0, y0, x1, y1, x2, y2, color, framebuffer);
}

void drawPixel(int x, int y, uint8_t color) {
  epd_draw_pixel(x, y, color, framebuffer);
}

void setFont(GFXfont const & font) {
  currentFont = font;
}

void epd_update() {
  epd_draw_grayscale_image(epd_full_screen(), framebuffer); // Update the screen
}
