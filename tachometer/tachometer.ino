#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <math.h>

// ---------- Pins ----------
const int TFT_CS   = 5;
const int TFT_DC   = 16;
const int TFT_RST  = 17;
const int POT_PIN  = 34;
const int UP_PIN   = 4;
const int DOWN_PIN = 21;
const int LED_PIN  = 2;

// ---------- Engine & car settings (fun to tweak) ----------
const float MAX_RPM    = 8000;   // end of the gauge
const float REV_LIMIT  = 7200;   // limiter cuts power here
const float SHIFT_RPM  = 6500;   // shift light + redline start
const float IDLE_RPM   = 850;
const int   NUM_GEARS  = 6;
const float RATIO[NUM_GEARS] = {3.60, 2.20, 1.50, 1.15, 0.92, 0.75};
const float RPM_PER_MPH = 48.6;  // rpm = mph * ratio * this (1st tops out ~40 mph)
const float POWER      = 3.7;    // acceleration strength
const float DRAG       = 0.00008;// air resistance
const float COAST      = 1.5;    // engine braking when off throttle

// Pot readings near the ends are unreliable on the ESP32's ADC, so ignore them
const int POT_MIN = 150;
const int POT_MAX = 3950;

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// ---------- Car state ----------
int gear = 0;            // 0 = 1st gear
float speedMph = 0;
float rpm = IDLE_RPM;
float rpmShown = IDLE_RPM;
float throttle = 0;
float potSmooth = 0;
bool limiterOn = false;
unsigned long msgUntil = 0;
unsigned long lastSimMs = 0;
String message = "";

// ---------- Buttons (debounced) ----------
struct Button {
  int pin;
  int lastReading = HIGH;
  int stable = HIGH;
  unsigned long lastChange = 0;
};
Button upBtn{UP_PIN};
Button downBtn{DOWN_PIN};

// What's currently drawn, so only changed parts get redrawn (no flicker)
struct Shown {
  String text = "\x01";
  uint16_t color = 0;
};
Shown shownGear, shownRpm, shownMph, shownMsg;

// Returns true once per press
bool pressed(Button& b) {
  int r = digitalRead(b.pin);
  if (r != b.lastReading) {
    b.lastChange = millis();
    b.lastReading = r;
  }
  if (millis() - b.lastChange > 30 && r != b.stable) {
    b.stable = r;
    return r == LOW;
  }
  return false;
}

// ---------- Screen layout ----------
const int CX = 120, CY = 120;
const float START_ANGLE = -135;  // degrees, 0 = top, clockwise
const float SWEEP = 270;
const int SEGS = 90;             // RPM bar is made of 90 segments (3 degrees each)
const int BAR_INNER = 94, BAR_OUTER = 110;

uint16_t C_BG, C_TEXT, C_DIM, C_TRACK, C_TRACK_RED, C_GREEN, C_YELLOW, C_RED;
int shownLit = -1;

void polar(float deg, float r, int& x, int& y) {
  float a = deg * DEG_TO_RAD;
  x = CX + r * sin(a);
  y = CY - r * cos(a);
}

float angleForRpm(float r) { return START_ANGLE + SWEEP * r / MAX_RPM; }

uint16_t segColor(int i) {
  float r = (i + 0.5) * MAX_RPM / SEGS;
  if (r >= SHIFT_RPM) return C_RED;
  if (r >= 5000) return C_YELLOW;
  return C_GREEN;
}

uint16_t trackColor(int i) {
  float r = (i + 0.5) * MAX_RPM / SEGS;
  return r >= SHIFT_RPM ? C_TRACK_RED : C_TRACK;
}

void drawSeg(int i, uint16_t color) {
  float a0 = START_ANGLE + i * SWEEP / SEGS;
  float a1 = a0 + SWEEP / SEGS - 0.8;   // small gap gives a segmented look
  int x0, y0, x1, y1, x2, y2, x3, y3;
  polar(a0, BAR_OUTER, x0, y0);
  polar(a0, BAR_INNER, x1, y1);
  polar(a1, BAR_OUTER, x2, y2);
  polar(a1, BAR_INNER, x3, y3);
  tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
  tft.fillTriangle(x1, y1, x3, y3, x2, y2, color);
}

// Only draws the segments that changed since last time
void drawBar(float r) {
  int lit = constrain((int)(r * SEGS / MAX_RPM), 0, SEGS);
  if (shownLit < 0) {                       // first draw: whole bar
    for (int i = 0; i < SEGS; i++) drawSeg(i, i < lit ? segColor(i) : trackColor(i));
  } else if (lit > shownLit) {
    for (int i = shownLit; i < lit; i++) drawSeg(i, segColor(i));
  } else {
    for (int i = lit; i < shownLit; i++) drawSeg(i, trackColor(i));
  }
  shownLit = lit;
}

void drawDial() {
  tft.setTextSize(2);
  for (int k = 0; k <= 8; k++) {
    float a = angleForRpm(k * 1000);
    int x0, y0, x1, y1;
    polar(a, 113, x0, y0);
    polar(a, 119, x1, y1);
    uint16_t c = (k * 1000 >= SHIFT_RPM) ? C_RED : C_DIM;
    tft.drawLine(x0, y0, x1, y1, c);

    int lx, ly;                              // labels 0-8 (x1000 rpm)
    polar(a, 80, lx, ly);
    tft.setTextColor(c);
    tft.setCursor(lx - 5, ly - 7);
    tft.print(k);
  }
}

// Clears a band and prints centered text, only if it changed
void drawText(Shown& s, const String& text, int y, int size, int halfWidth, uint16_t color) {
  if (s.text == text && s.color == color) return;
  tft.fillRect(CX - halfWidth, y, halfWidth * 2, 8 * size, C_BG);
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.setCursor(CX - (int)text.length() * 3 * size, y);
  tft.print(text);
  s.text = text;
  s.color = color;
}

void showMessage(const String& m, unsigned long ms) {
  message = m;
  msgUntil = millis() + ms;
}

// ---------- Shifting ----------

void upshift() {
  if (gear >= NUM_GEARS - 1) { showMessage("TOP GEAR", 800); return; }
  gear++;
}

void downshift() {
  if (gear == 0) { showMessage("1ST GEAR", 800); return; }
  float newRpm = speedMph * RATIO[gear - 1] * RPM_PER_MPH;
  if (newRpm > REV_LIMIT) {                 // would over-rev: refuse
    showMessage("TOO FAST", 1000);
    return;
  }
  gear--;
}

// ---------- Simulation ----------

void readThrottle() {
  int raw = analogRead(POT_PIN);
  potSmooth += (raw - potSmooth) * 0.2;     // smooth out ADC jitter
  throttle = constrain((potSmooth - POT_MIN) / (POT_MAX - POT_MIN), 0.0, 1.0);
}

void simulate(float dt) {
  float engineRpm = speedMph * RATIO[gear] * RPM_PER_MPH;

  float accel = throttle * POWER * RATIO[gear]
              - DRAG * speedMph * speedMph
              - (1.0 - throttle) * COAST;

  limiterOn = (engineRpm >= REV_LIMIT && throttle > 0.05);
  if (limiterOn && accel > 0) accel = 0;    // limiter cuts power

  speedMph = max(0.0f, speedMph + accel * dt);
  engineRpm = speedMph * RATIO[gear] * RPM_PER_MPH;

  // At low speed the clutch slips, so throttle can still raise rpm
  rpm = max(engineRpm, IDLE_RPM + throttle * 2500);

  // Limiter "bounce" makes the bar flicker at the limit like a real car
  if (limiterOn && (millis() / 70) % 2) rpm -= 300;
}

// ---------- Setup & loop ----------

void setup() {
  Serial.begin(115200);
  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  analogReadResolution(12);                 // 0-4095

  tft.begin();
  C_BG        = tft.color565(0, 0, 0);
  C_TEXT      = tft.color565(255, 255, 255);
  C_DIM       = tft.color565(130, 140, 140);
  C_TRACK     = tft.color565(35, 38, 38);
  C_TRACK_RED = tft.color565(70, 15, 15);
  C_GREEN     = tft.color565(60, 210, 120);
  C_YELLOW    = tft.color565(255, 200, 40);
  C_RED       = tft.color565(255, 50, 40);
  tft.fillScreen(C_BG);
  drawDial();

  potSmooth = analogRead(POT_PIN);
  lastSimMs = millis();
}

void loop() {
  if (pressed(upBtn)) upshift();
  if (pressed(downBtn)) downshift();

  unsigned long now = millis();
  if (now - lastSimMs < 20) return;          // run 50 times per second
  float dt = (now - lastSimMs) / 1000.0;
  lastSimMs = now;

  readThrottle();
  simulate(dt);

  // Ease the shown rpm toward the real one so the bar moves smoothly
  rpmShown += (rpm - rpmShown) * 0.35;
  drawBar(rpmShown);

  // Shift light: fast blink near redline
  bool shiftNow = rpm >= SHIFT_RPM;
  digitalWrite(LED_PIN, shiftNow && (now / 80) % 2 ? HIGH : LOW);

  // Center readouts
  uint16_t gearColor = shiftNow ? C_RED : C_TEXT;
  drawText(shownGear, String(gear + 1), 58, 6, 22, gearColor);
  drawText(shownRpm, String((int)(rpmShown / 50) * 50) + " rpm", 118, 2, 52, C_DIM);
  drawText(shownMph, String((int)speedMph) + " mph", 144, 2, 52, C_TEXT);

  String msg = "";
  uint16_t msgColor = C_RED;
  if (now < msgUntil)   msg = message;
  else if (limiterOn)   msg = "LIMITER";
  else if (shiftNow && gear < NUM_GEARS - 1) msg = "SHIFT!";
  drawText(shownMsg, msg, 188, 2, 60, msgColor);
}
