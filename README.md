==Inkplate 10 Wise Clock==

 ![alt text](https://github.com/nokel/Inkplate10-Smart-Clock/blob/main/clock.jpg?raw=true)

This project runs on the Inkplate 10 e-paper display.
It pulls local MQTT information and shows:

  Current time (auto-updates with NTP, includes Sydney DST)
  Current date (updates at midnight or on boot)
  temperature (mqtt/address)
  humidity (mqtt/address)

To make the clock more visually appealing (and update faster), partial screen refreshing is used around figures that change.

==Features==

  - Connect to wifi (via creds page) to update real-time clock and temperature sensors that use MQTT

  - Auto-reconnect for MQTT if the broker restarts

  - Syncs with NTP server and stores time in RTC module

  - Fonts: Verdana (https://online-fonts.com/fonts/verdana), Triforce (https://online-fonts.com/fonts/triforce), and chiaro std (https://online-fonts.com/fonts/fot-chiaro-std)
    converted with Adafruitgfx and truetype2gfx (https://github.com/ropg/truetype2gfx)

  - Clear partial refresh
  
  - Boot loading screen and debug variable for locating partial refresh boundry boxes

  - Uses both CPU's of the esp32 on the inkplate for parallel processing

==Requirements==

  -Libraries-

    -  Inkplate Library (https://github.com/SolderedElectronics/Inkplate-Arduino-library)

    -  PubSubClient (https://github.com/knolleary/pubsubclient)

  -Fonts-

The following fonts were generated using truetype2gfx (which in turn uses Adafruit GFX font converter) from ttf or otf files and should be placed in the same folder as the main sketch

  Verdana10pt7b.h
  Triforce20pt7b.h
  Triforce40pt7b.h
  Triforce60pt7b.h
  Triforce90pt7b.h
  Triforce120pt7b.h
  chiarostd_b20pt7b.h
  chiarostd_b30pt7b.h
  chiarostd_b40pt7b.h

 -Credentials (creds.ino)-

Edit this file for the clock to gain information from wifi, and mqtt.

==Usage==

Install all necessary library files into the arduino ide and edit the creds.ino file to match the information to suite your wifi+mqtt configuration

Select Inkplate 10 board in Arduino IDE.

Upload sketch via USB.

==Example Layout==

The display shows:

Top left: temperature

Top right: humidity

Bottom center: Current time

Bottom centre: Date

Partial updates are used for the changed data.

==Notes==

If MQTT values don’t show, check that your broker is reachable at the IP you set in creds.ino.

Fonts are required — without the .h files, compilation will fail.

Debug mode draws thin rectangles around partial update areas (disable by setting debug = false;).
