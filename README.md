## This project is a modified fork of the original project by CybDis

Weather Station for LilyGO T5 4.7 inch e-paper display
=======================================

This project works with LilyGO T5 4.7 inch e-paper EPD display and [OpenWeatherMap (OMW)](https://home.openweathermap.org) as ESP32 weather display.

![](./assets/Modified.jpg)

## Modifications

This repository is a fork of:
https://github.com/CybDis/Lilygo-T5-4.7-WeatherStation-with-HomeAssistant

Changes made in this fork:

- Added wind gust display with a dynamic icon
- Added a dynamic cloudiness icon
- Added the ability to display atmospheric pressure in mmHg (in user_settings.h)
- Fixed snowfall chart (it didn’t work in my case in the original firmware) - you can find screenshots of the original and modified firmware in the assets folder
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

## Remarks 
_(forked from [DzikuVx/LilyGo-EPD-4-7-OWM-Weather-Display](https://github.com/DzikuVx/LilyGo-EPD-4-7-OWM-Weather-Display))_  

The original code created by https://github.com/G6EJD/ is using the GPLv3 https://github.com/Xinyuan-LilyGO/LilyGo-EPD47 library to handle the display and as such falls into the GPLv3 license itself. This situation is described in the https://www.gnu.org/licenses/gpl-faq.html#IfLibraryIsGPL

> If a library is released under the GPL (not the LGPL), does that mean that any software which uses it has to be under the GPL or a GPL-compatible license?

> Yes, because the program actually links to the library. As such, the terms of the GPL apply to the entire combination. The software modules that link with the library may be under various GPL compatible licenses, but the work as a whole must be licensed under the GPL.

This means that the original proprietary license that G6EJD tried to enforce is unlawful as it is not compatible with the GPLv3 and removed from this fork, while keeping the attribution and all the copyright of the original creator.

## Original project by CybDis
If you like the original work consider supporting the author:

<a href="https://www.buymeacoffee.com/cybdis" target="_blank">
  <img src="https://raw.githubusercontent.com/CybDis/CybDis/main/bmc-yellow-button.png" height="60px"/></a>
