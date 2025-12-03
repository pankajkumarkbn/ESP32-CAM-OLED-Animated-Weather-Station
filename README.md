# ESP32‑CAM Animated Weather Station 🌦️

An animated, offline weather station for the ESP32‑CAM that uses a DHT11 and BMP280 to show real‑time temperature, humidity, pressure, altitude and a local short‑term forecast on a 0.96" SSD1306 OLED. The UI is compact, animated, and designed specifically around the ESP32‑CAM’s awkward pinout.[1][2]

## Features

- Live readings from:
  - **DHT11** – ambient temperature & humidity near the device.[3]
  - **BMP280** – barometric pressure, temperature & calculated altitude.[4][5]
- **Dual temperature display**: shows both DHT11 and BMP280 temperatures side‑by‑side to highlight sensor differences.
- **Animated OLED UI**:
  - Small weather icon (sun, clouds, rain, storm) on the left.
  - Clean right‑side data column (no overlapping text).
  - Forecast text banner at the bottom.
- **Local forecast logic**:
  - Uses **absolute pressure bands**, **pressure trend over time**, and **humidity** to classify likely conditions (sunny, cloudy, rain, storm).[6][7][8]
- Fully offline: no Wi‑Fi, no cloud dependency, ideal for a desk, balcony, or workbench.

## Preview
<img src="ESP32CAM_WEBUI_OLED_Weather-Station1.jpeg" width="320" height="320">

***

## Hardware

- **ESP32‑CAM (AI‑Thinker)** module.[9]
- **SSD1306 128×64 OLED** in SPI mode.
- **DHT11** temperature & humidity sensor.
- **BMP280** temperature & barometric pressure sensor (I²C).

### Wiring

```text
ESP32‑CAM  →  OLED (SPI SSD1306)
--------------------------------
GPIO1      →  SCL / CLK
GPIO2      →  SDA / MOSI
GPIO15     →  CS
GPIO13     →  DC
GPIO12     →  RST
3V3        →  VCC
GND        →  GND

ESP32‑CAM  →  BMP280 (I²C)
--------------------------
GPIO0      →  SDA
GPIO16     →  SCL
3V3        →  VCC
GND        →  GND

ESP32‑CAM  →  DHT11
-------------------
GPIO14     →  DHT11 DATA
3V3        →  VCC
GND        →  GND
```

Pins are chosen to avoid ESP32‑CAM camera, PSRAM, and flash LED conflicts while still allowing Serial debugging.[10][9]

## Libraries

Install these via the Arduino Library Manager:

- **U8g2** – `U8g2lib` (SSD1306 graphics on OLED).[11]
- **Adafruit BMP280 Library**.[5][4]
- **Adafruit Unified Sensor** (dependency of BMP280).[12]
- **DHT sensor library** by Adafruit (for DHT11).[3]

Board settings (Arduino IDE):

- Board: `ESP32 Wrover Module` or `AI Thinker ESP32‑CAM` (depending on your core).
- Partition: any that fits your sketch (e.g. “Huge APP”).
- Upload speed: 115200 or 921600 baud.

## How it works

- Every few seconds, the ESP32‑CAM reads:
  - `T(DHT)` and `H` from the DHT11.
  - `T(BMP)`, `P` and `Alt` from the BMP280.
- Every 10 minutes, it:
  - Compares the new pressure to the last sample.
  - Low‑pass filters that delta into a **pressure trend** variable.
  - Classifies conditions into one of: `Sunny`, `Partly Cloudy`, `Cloudy`, `Rain Likely`, or `Storm Risk`, using simple heuristics from typical barometric forecasting.[13][14][8]
- The OLED renders:
  - A small animated icon on the left (sun / clouds / rain / lightning).
  - A right‑hand column with:
    - `DHT: xx.xC`
    - `BMP: xx.xC`
    - `Hum: xx%`
    - `P: xxxx.xhPa`
    - `Alt: xxxm`
  - A bottom one‑line forecast text like “Rain Likely” or “Sunny / Clear”.

## Usage

1. Clone the repo and open the `.ino` file in Arduino IDE.
2. Select the correct ESP32‑CAM board and COM port.
3. Flash the code.
4. Power the ESP32‑CAM and watch the OLED:
   - First, it will show an init message.
   - After a few seconds, readings and the icon animate.
   - After the first 10‑minute window, the forecast becomes trend‑aware.

If readings look off, verify wiring and check the Serial Monitor at 115200 baud for sensor init messages and BMP280 I²C address (0x76 or 0x77).[15][16]

## Tuning and customization

- **Forecast sensitivity**:
  - Adjust `PRESSURE_SAMPLE_INTERVAL` (default 10 min) and `TREND_ALPHA` in the code to balance responsiveness and noise.
  - Modify pressure thresholds in `calcForecast()` if you live at significantly higher altitude or in very humid/dry climates.[17][7]
- **UI tweaks**:
  - Change fonts by swapping `u8g2_font_5x8_tf` for other U8g2 fonts.
  - Reposition values or add more labels if you change sensors.
- **Sensor swaps**:
  - BMP280 → BME280: adjust the library include and humidity usage accordingly.
  - DHT11 → DHT22: change `DHTTYPE` and enjoy better resolution.[3]

## Roadmap / ideas

- Wi‑Fi web dashboard mirroring OLED data.
- Logging to SPIFFS / SD (if SD card is free).
- Zambretti‑style forecast index for more nuanced predictions.[18]
- Optional NTP time display and day/night‑aware icons.

***

Feel free to fork, tweak pin mappings for your own ESP32 variant, or extend the forecast logic. If you build your own version, screenshots or hardware photos in the issues/PRs would be very welcome.

[1](https://iotdesignpro.com/projects/iot-based-esp32-wi-fi-weather-station-using-dht11-and-bmp-180-sensor)
[2](https://www.visuino.com/weather-station-using-bmp280-dht11-temperature-humidity-and-pressure/)
[3](https://randomnerdtutorials.com/esp32-dht11-dht22-temperature-humidity-sensor-arduino-ide/)
[4](https://randomnerdtutorials.com/esp32-bme280-arduino-ide-pressure-temperature-humidity/)
[5](https://www.adafruit.com/product/2651)
[6](https://science.howstuffworks.com/nature/climate-weather/atmospheric/barometer.htm)
[7](https://tempest.earth/what-is-air-pressure/)
[8](https://forum.arduino.cc/t/weather-forecast-with-bmp085-pressure-sensor/193017)
[9](https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/)
[10](https://lastminuteengineers.com/esp32-cam-pinout-reference/)
[11](https://randomnerdtutorials.com/esp32-ssd1306-oled-display-arduino-ide/)
[12](https://lastminuteengineers.com/bme280-esp32-weather-station/)
[13](https://www.youtube.com/watch?v=rbOzlLA3Ikw)
[14](https://support.acurite.com/hc/en-us/articles/360009475694-Barometric-Pressure)
[15](https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/)
[16](https://www.circuitschools.com/interfacing-bmp280-with-esp-32-on-i2c-with-errors-and-solutions/)
[17](https://kestrelinstruments.com/blog/what-is-barometric-pressure-and-how-can-you-measure-it)
[18](https://www.instructables.com/DIY-Arduino-Zambretti-Weather-Forecaster-on-VFD-Di/)
[19](https://randomnerdtutorials.com/esp32-web-server-with-bme280-mini-weather-station/)
[20](https://www.instructables.com/DIY-Weather-Station-With-ESP32/)
[21](https://github.com/DigiTorus86/ESP32Badge)
[22](https://arduinoyard.com/weather-station-with-esp32-and-dht11/)
[23](https://github.com/dcoldeira/ESP32-IoT-Weather-Station)
[24](https://github.com/topics/esp32-weather-station)
[25](https://www.facebook.com/groups/874336100066868/posts/1463517577815381/)
[26](https://github.com/RuiSantosdotme/Cloud-Weather-Station-ESP32-ESP8266/blob/master/README.md)
[27](https://www.youtube.com/watch?v=KefhStxTNzw)
[28](https://github.com/HWHardsoft/ESP32_Weather_Station/blob/master/README.md)
[29](https://github.com/VincentKobz/diy-weather-station)
