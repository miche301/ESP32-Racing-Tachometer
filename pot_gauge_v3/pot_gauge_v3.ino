#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

const int POT_PIN    = 34;
const int BUTTON_PIN = 4;
Adafruit_GC9A01A tft(5, 16, 17);   // CS, DC, RST

// The two display modes
const int MODE_VOLTS   = 0;
const int MODE_PERCENT = 1;
int mode = MODE_VOLTS;

int lastPercent = -1;
bool needRedraw = true;     // forces a redraw at startup and after a mode change

// Button debounce
int lastReading = HIGH;
int stableState = HIGH;
unsigned long lastChangeMs = 0;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);   // pressed = LOW
  tft.begin();
  tft.fillScreen(GC9A01A_BLACK);
}

// Returns true once each time the button is pressed
bool buttonPressed() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) {        // something changed: start the timer
    lastChangeMs = millis();
    lastReading = reading;
  }
  if (millis() - lastChangeMs > 30 && reading != stableState) {
    stableState = reading;             // it's been steady for 30 ms: accept it
    return stableState == LOW;
  }
  return false;
}

void loop() {
  // 1. Switch modes when the button is pressed
  if (buttonPressed()) {
    mode = (mode == MODE_VOLTS) ? MODE_PERCENT : MODE_VOLTS;
    needRedraw = true;
  }

  // 2. Read the pot
  int raw = analogRead(POT_PIN);
  float volts = raw * 3.3 / 4095.0;
  int percent = raw * 100 / 4095;

  // 3. Only redraw when something changed (avoids flicker)
  if (percent != lastPercent || needRedraw) {
    // Ring color shows the mode: green = volts, blue = percent
    uint16_t ringColor = (mode == MODE_VOLTS) ? GC9A01A_GREEN : GC9A01A_CYAN;

    for (int i = 0; i < 100; i++) {
      float angle = i * 3.6 * DEG_TO_RAD;
      int x = 120 + 105 * sin(angle);
      int y = 120 - 105 * cos(angle);
      uint16_t color = (i < percent) ? ringColor : GC9A01A_DARKGREY;
      tft.fillCircle(x, y, 4, color);
    }

    // Big number in the middle
    tft.fillRect(40, 95, 160, 50, GC9A01A_BLACK);   // erase old number
    tft.setTextColor(GC9A01A_WHITE);
    tft.setTextSize(4);
    if (mode == MODE_VOLTS) {
      tft.setCursor(60, 100);
      tft.print(volts, 2);
      tft.print("V");
    } else {
      tft.setCursor(percent < 10 ? 96 : (percent < 100 ? 84 : 72), 100);
      tft.print(percent);
      tft.print("%");
    }

    // Small label underneath
    tft.fillRect(70, 150, 100, 16, GC9A01A_BLACK);
    tft.setTextColor(GC9A01A_DARKGREY);
    tft.setTextSize(2);
    if (mode == MODE_VOLTS) {
      tft.setCursor(90, 150);
      tft.print("volts");
    } else {
      tft.setCursor(78, 150);
      tft.print("percent");
    }

    lastPercent = percent;
    needRedraw = false;
  }

  delay(20);
}
