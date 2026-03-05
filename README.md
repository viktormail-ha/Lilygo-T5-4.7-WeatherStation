## This project is a modified fork of the original project by CybDis

Weather Station for LilyGO T5 4.7 inch e-paper display
=======================================

This project works with LilyGO T5 4.7 inch e-paper EPD display and [OpenWeatherMap (OMW)](https://home.openweathermap.org) as ESP32 weather display.

### Screen Comparison (same weather data)

| Original Firmware                                               | Modified Version                                                   |
| --------------------------------------------------------------- | ------------------------------------------------------------------ |
| <img src="assets/Original.jpg" width="420"><br>Original version | <img src="assets/Modified.jpg" width="420"><br>My modified version |

## Key Features & Improvements

This repository is a fork of:
https://github.com/CybDis/Lilygo-T5-4.7-WeatherStation-with-HomeAssistant

Changes made in this fork:

- Added wind gust display with a dynamic icon
- Added a dynamic cloudiness icon
- Added the ability to display atmospheric pressure in mmHg (in user_settings.h)
- Fixed snowfall chart (not working in the original firmware in my case). See Screen Comparison
- Fixed weather icon display (for example, Overcast Clouds previously showed a sun icon; now it correctly shows overcast clouds)
- Reworked weather icons and added new icons
- For precipitation three icon levels are used:
  - light precipitation – 2 snowflakes or raindrops
  - moderate precipitation – 3 snowflakes or raindrops
  - heavy precipitation – 4 snowflakes or raindrops
- Modified precipitation chart: if both snow and rain are present in the forecast, the chart displays light bars for snow and black bars for rain (if only snow or only rain is present, all bars are black)
- Default: 5-day forecast displayed in charts
- Default: data updates every 30 minutes
- Minor UI adjustments

## Compiling and flashing

To compile you will need following libraries.
- https://github.com/Xinyuan-LilyGO/LilyGo-EPD47
- https://github.com/bblanchon/ArduinoJson  

## Quick Flash Instructions

1. Connect the LilyGO T5 4.7" to your PC via USB.
2. Open main folder of this project in **Visual Studio Code** with the **PlatformIO** extension installed.
3. Edit user_settings.h and enter your WiFi credentials, your own OpenWeatherMap API key and your location for which you want to display the weather data.
4. Press **Build** (checkmark icon) to compile the firmware.
5. Press **Upload** (right arrow icon) to flash the device.
6. Wait for the upload to finish and the device will start automatically.

# License

[GNU GENERAL PUBLIC LICENSE](./LICENSE)

## History & Credits
- Forked from [CybDis/Lilygo-T5-4.7-WeatherStation-with-HomeAssistant](https://github.com/CybDis/Lilygo-T5-4.7-WeatherStation-with-HomeAssistant)
- Based on [DzikuVx/LilyGo-EPD-4-7-OWM-Weather-Display](https://github.com/DzikuVx/LilyGo-EPD-4-7-OWM-Weather-Display)
- Original concept and code by [G6EJD](https://github.com/G6EJD/)
- Licensed under GPLv3 due to the required use of the GPLv3 LilyGo-EPD47 library. Full attribution to all prior authors is maintained.
