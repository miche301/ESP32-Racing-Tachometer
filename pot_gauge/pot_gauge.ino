#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

const int POT_PIN = 34;
Adafruit_GC9A01A tft(5, 16, 17);   // CS, DC, RST

int lastPercent = -1;

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.fillScreen(GC9A01A_BLACK);
}

void loop() {
  // 1. Read the pot: the ESP32 gives a number from 0 to 4095
  int raw = analogRead(POT_PIN);

  // 2. Convert that number to volts (4095 = about 3.3 V)
  float volts = raw * 3.3 / 4095.0;

  // 3. Convert to a percent for the ring
  int percent = raw * 100 / 4095;

  // Print to the Serial Monitor so you can compare with a multimeter
  Serial.print("raw: ");
  Serial.print(raw);
  Serial.print("   volts: ");
  Serial.println(volts, 2);

  // Only redraw when the value changes (avoids flicker)
  if (percent != lastPercent) {
    // Ring: draw 100 dots around the circle, lit up to the current percent
    for (int i = 0; i < 100; i++) {
      float angle = i * 3.6 * DEG_TO_RAD;          // 100 dots x 3.6 degrees = 360
      int x = 120 + 105 * sin(angle);
      int y = 120 - 105 * cos(angle);
      uint16_t color = (i < percent) ? GC9A01A_GREEN : GC9A01A_DARKGREY;
      tft.fillCircle(x, y, 4, color);
    }

    // Voltage in the middle
    tft.fillRect(40, 95, 160, 50, GC9A01A_BLACK);  // erase old number
    tft.setTextColor(GC9A01A_WHITE);
    tft.setTextSize(4);
    tft.setCursor(60, 100);
    tft.print(volts, 2);
    tft.print("V");

    lastPercent = percent;
  }

  delay(50);
}
