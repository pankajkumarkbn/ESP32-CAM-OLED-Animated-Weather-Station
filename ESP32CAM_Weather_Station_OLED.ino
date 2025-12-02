#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>

// ================= DISPLAY (SPI) =================
// OLED wired: CLK=1, MOSI=2, CS=15, DC=13, RST=12
U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2(
  U8G2_R0,
  /* clock=*/ 1,
  /* data=*/ 2,
  /* cs=*/ 15,
  /* dc=*/ 13,
  /* reset=*/ 12
);

// ================= SENSORS =======================
// DHT11 on GPIO14
#define DHTPIN  14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// BMP280 on I2C using custom pins SDA=0, SCL=16
#define I2C_SDA 0
#define I2C_SCL 16
Adafruit_BMP280 bmp;   // I2C

// ============ FORECAST STATE =====================
float lastPressure = 1013.25;
unsigned long lastPressureSample = 0;
const unsigned long PRESSURE_SAMPLE_INTERVAL = 10UL * 60UL * 1000UL; // 10 min
const float SEA_LEVEL_HPA = 1013.25;

// Smoothed pressure trend (hPa per interval)
float pressureTrend = 0.0;
const float TREND_ALPHA = 0.4;  // low‑pass filter weight

enum ForecastType {
  FORECAST_SUNNY,
  FORECAST_PARTLY_CLOUDY,
  FORECAST_CLOUDY,
  FORECAST_RAIN,
  FORECAST_STORM,
  FORECAST_UNKNOWN
};
ForecastType currentForecast = FORECAST_UNKNOWN;

uint8_t animFrame = 0;
unsigned long lastAnimUpdate = 0;
const unsigned long ANIM_INTERVAL = 180;

// ============ FORECAST HEURISTIC =================
// Uses absolute pressure bands + smoothed trend + humidity.[web:77][web:78]
ForecastType calcForecast(float pressure, float temp, float hum, float trend) {
  if (isnan(pressure) || isnan(temp) || isnan(hum)) return FORECAST_UNKNOWN;

  bool veryHigh = pressure >= 1022.0;
  bool highP    = pressure >= 1015.0 && pressure < 1022.0;
  bool lowP     = pressure <= 1008.0;
  bool veryLow  = pressure <= 1000.0;

  bool rising   = trend > 0.6;
  bool falling  = trend < -0.6;
  bool steady   = !rising && !falling;

  if (veryLow && falling && hum > 75.0) return FORECAST_STORM;
  if (lowP && falling && hum > 70.0)    return FORECAST_RAIN;

  if ((lowP || veryLow) && hum > 65.0)  return FORECAST_RAIN;

  if ((highP || veryHigh) && rising && hum < 55.0) return FORECAST_SUNNY;
  if (highP && steady && hum < 60.0)               return FORECAST_PARTLY_CLOUDY;

  if (steady && hum > 60.0)      return FORECAST_CLOUDY;
  if (rising && hum > 65.0)      return FORECAST_CLOUDY;

  if (veryHigh && hum < 50.0)    return FORECAST_SUNNY;

  return FORECAST_PARTLY_CLOUDY;
}

// ============ ICON DRAWING (small, left) =========
void drawSunSmall(uint8_t x, uint8_t y) {
  u8g2.drawCircle(x, y, 5, U8G2_DRAW_ALL);
  u8g2.drawLine(x, y-8, x, y-5);
  u8g2.drawLine(x, y+5, x, y+8);
  u8g2.drawLine(x-8, y, x-5, y);
  u8g2.drawLine(x+5, y, x+8, y);
}

void drawCloudSmall(uint8_t x, uint8_t y) {
  u8g2.drawRBox(x, y, 22, 9, 3);
  u8g2.drawCircle(x+5,  y,   4, U8G2_DRAW_ALL);
  u8g2.drawCircle(x+15, y-1, 5, U8G2_DRAW_ALL);
}

void drawRainSmall(uint8_t x, uint8_t y, uint8_t frame) {
  uint8_t offset = frame % 6;
  for (uint8_t i = 0; i < 2; i++) {
    uint8_t dx = x + i * 6;
    uint8_t dy = y + (offset + i * 3) % 10;
    u8g2.drawLine(dx, dy, dx + 1, dy + 3);
  }
}

void drawStormSmall(uint8_t x, uint8_t y, uint8_t frame) {
  if (frame & 0x01) {
    u8g2.drawLine(x,   y,   x+3, y+4);
    u8g2.drawLine(x+3, y+4, x+1, y+4);
    u8g2.drawLine(x+1, y+4, x+5, y+10);
  }
}

void drawForecastIcon(ForecastType f, uint8_t frame) {
  uint8_t baseX = 0;
  uint8_t baseY = 18;

  switch (f) {
    case FORECAST_SUNNY:
      drawSunSmall(baseX + 10, baseY - 2);
      break;

    case FORECAST_PARTLY_CLOUDY:
      drawSunSmall(baseX + 8, baseY - 4);
      drawCloudSmall(baseX + 10, baseY);
      break;

    case FORECAST_CLOUDY:
      drawCloudSmall(baseX + 8, baseY);
      drawCloudSmall(baseX + 2, baseY + 4);
      break;

    case FORECAST_RAIN:
      drawCloudSmall(baseX + 10, baseY);
      drawRainSmall(baseX + 10, baseY + 8, frame);
      break;

    case FORECAST_STORM:
      drawCloudSmall(baseX + 10, baseY);
      drawStormSmall(baseX + 12, baseY + 8, frame);
      break;

    default:
      u8g2.setFont(u8g2_font_4x6_tf);
      u8g2.drawStr(2, 18, "NO");
      u8g2.drawStr(2, 26, "DATA");
      break;
  }
}

const char* forecastToText(ForecastType f) {
  switch (f) {
    case FORECAST_SUNNY:          return "Sunny / Clear";
    case FORECAST_PARTLY_CLOUDY:  return "Partly Cloudy";
    case FORECAST_CLOUDY:         return "Cloudy";
    case FORECAST_RAIN:           return "Rain Likely";
    case FORECAST_STORM:          return "Storm Risk";
    default:                      return "No Forecast";
  }
}

// ================== SETUP =========================
void setup() {
  Serial.begin(115200);
  delay(200);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(0, 8, "ESP32-CAM Weather");
  u8g2.drawStr(0, 18, "Init sensors...");
  u8g2.sendBuffer();

  dht.begin();

  Wire.begin(I2C_SDA, I2C_SCL);  // use SDA=0, SCL=16 for BMP280[web:22]
  delay(100);

  bool bmpOk = false;
  if (bmp.begin(0x76)) {
    Serial.println("BMP280 found at 0x76");
    bmpOk = true;
  } else if (bmp.begin(0x77)) {
    Serial.println("BMP280 found at 0x77");
    bmpOk = true;
  } else {
    Serial.println("BMP280 NOT FOUND!");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(0, 10, "BMP280 NOT FOUND!");
    u8g2.sendBuffer();
  }

  if (bmpOk) {
    lastPressure = bmp.readPressure() / 100.0F;
  }
  lastPressureSample = millis();
}

// ================== LOOP ==========================
void loop() {
  static unsigned long lastSensorRead = 0;
  const unsigned long SENSOR_INTERVAL = 3000;

  static float tDHT = NAN, h = NAN;
  static float tBMP = NAN, p = NAN, alt = NAN;

  unsigned long now = millis();

  // ---- Sensor reads ----
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    float ht = dht.readTemperature();
    float hh = dht.readHumidity();
    if (!isnan(ht)) tDHT = ht;
    if (!isnan(hh)) h = hh;

    float pPa = bmp.readPressure();
    float tt  = bmp.readTemperature();
    if (!isnan(pPa)) p = pPa / 100.0F;
    if (!isnan(tt))  tBMP = tt;
    alt = bmp.readAltitude(SEA_LEVEL_HPA);

    // Pressure trend every PRESSURE_SAMPLE_INTERVAL
    if (!isnan(p) && (now - lastPressureSample >= PRESSURE_SAMPLE_INTERVAL)) {
      float rawDelta = p - lastPressure;   // hPa over interval
      float newTrend = rawDelta;
      pressureTrend = (1.0 - TREND_ALPHA) * pressureTrend + TREND_ALPHA * newTrend;
      lastPressure = p;
      lastPressureSample = now;
      currentForecast = calcForecast(p, tBMP, h, pressureTrend);
    }

    static bool firstForecastDone = false;
    if (!firstForecastDone && !isnan(p) && !isnan(tBMP) && !isnan(h)) {
      currentForecast = calcForecast(p, tBMP, h, 0.0);
      firstForecastDone = true;
    }
  }

  // ---- Animation ----
  if (now - lastAnimUpdate >= ANIM_INTERVAL) {
    lastAnimUpdate = now;
    animFrame++;
  }

  // ---- Draw UI ----
  u8g2.clearBuffer();

  // Title at top left
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(0, 8, "ESP32-CAM Weather");

  // Icon on left
  drawForecastIcon(currentForecast, animFrame);

  char buf[32];

  // Right‑hand data column, lines spaced to avoid overlap
  u8g2.setFont(u8g2_font_5x8_tf);

  // Line 1: T(DHT)
  u8g2.setCursor(40, 16);
  u8g2.print("DHT:");
  u8g2.setCursor(72, 16);
  if (!isnan(tDHT)) snprintf(buf, sizeof(buf), "%.1fC", tDHT);
  else strcpy(buf, "--.-C");
  u8g2.print(buf);

  // Line 2: T(BMP)
  u8g2.setCursor(40, 26);
  u8g2.print("BMP:");
  u8g2.setCursor(72, 26);
  if (!isnan(tBMP)) snprintf(buf, sizeof(buf), "%.1fC", tBMP);
  else strcpy(buf, "--.-C");
  u8g2.print(buf);

  // Line 3: Humidity
  u8g2.setCursor(40, 36);
  u8g2.print("Hum:");
  u8g2.setCursor(72, 36);
  if (!isnan(h)) snprintf(buf, sizeof(buf), "%.0f%%", h);
  else strcpy(buf, "--%");
  u8g2.print(buf);

  // Line 4: Pressure
  u8g2.setCursor(40, 46);
  u8g2.print("P:");
  u8g2.setCursor(72, 46);
  if (!isnan(p)) snprintf(buf, sizeof(buf), "%.1fh", p);
  else strcpy(buf, "----");
  u8g2.print(buf);

  // Line 5: Altitude
  u8g2.setCursor(40, 56);
  u8g2.print("Alt:");
  u8g2.setCursor(72, 56);
  if (!isnan(alt)) snprintf(buf, sizeof(buf), "%.0fm", alt);
  else strcpy(buf, "---m");
  u8g2.print(buf);

  // Bottom: forecast text
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(0, 64);
  u8g2.print(forecastToText(currentForecast));

  u8g2.sendBuffer();

  delay(5);
}
