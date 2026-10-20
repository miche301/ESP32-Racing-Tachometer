# 🏁 ESP32 Racing Tachometer

A racing-style tachometer built on an ESP32 with a 1.28" round TFT display. A potentiometer acts as the throttle, and two buttons work as sequential paddle shifters. The project was built in levels, starting with reading a single analog voltage and ending with a simulated 6-speed car with a rev limiter, shift light, and downshift protection.

<img width="440" height="248" alt="tachometer_demo" src="https://github.com/user-attachments/assets/85f8adc2-2aec-43a9-800e-46ce9f320b3d" />

Down shifting is the same concept: Lowering the potentiometer, then pressing the right button

## Project Levels

| Level | Sketch | What it does | What it taught me |
|---|---|---|---|
| 1 | [`pot_gauge/`](pot_gauge/) | Turn the pot and a ring fills while the screen shows the voltage | How an ADC turns a voltage into a number |
| 3 | [`pot_gauge_v3/`](pot_gauge_v3/) | A button switches the display between volts and percent | Button debouncing and screen modes |
| 4 | [`tachometer/`](tachometer/) | Full tachometer: throttle, 6 gears, upshift and downshift buttons | Putting it all together, plus a simple vehicle physics model |

*(Level 2 was a measurement exercise comparing ESP32 readings to a multimeter, with no separate sketch. See [Testing & Debugging](#testing--debugging).)*

### Level 1: Voltage Gauge
<img width="480" height="270" alt="pot_gauge_demo" src="https://github.com/user-attachments/assets/13d15531-2175-49ed-b1aa-fc1234d52821" />


### Level 3: Volts / Percent Mode Button
<img width="480" height="270" alt="pot_gauge_v3_demo" src="https://github.com/user-attachments/assets/34b823b1-08fa-4b13-b145-d03ec755cd96" />

## Tachometer Features

- **Potentiometer throttle:** turn the knob to accelerate and back off to coast with engine braking
- **6-speed sequential gearbox:** each gear has its own ratio. Low gears pull hard and hit the limiter fast; high gears pull slower but reach higher speeds (about 190 mph in 6th).
- **Upshift and downshift buttons** like paddle shifters
- **Downshift protection:** a downshift that would over-rev the engine is refused and the screen shows "TOO FAST"
- **Rev limiter** at 7,200 rpm, with a flickering "bounce" like a real car
- **Shift light:** the built-in blue LED flashes and the screen shows "SHIFT!" above 6,500 rpm
- **Segmented RPM bar** around the edge of the round screen (green → yellow → red redline), with labels 0–8 (×1000 rpm), plus the gear number, rpm, and mph in the center
- **Flicker-free drawing:** only the bar segments and text that changed are redrawn each frame

## Hardware

| Part | Notes |
|---|---|
| ESP32 dev board | HiLetgo ESP-WROOM-32 (30-pin DevKit) |
| 1.28" round TFT display | 240×240, GC9A01 driver, SPI |
| Potentiometer | Throttle input |
| 2 tactile push buttons | Upshift and downshift |
| Breadboard + jumper wires | No soldering required |
| Built-in LED | Shift light on GPIO 2 |

## Wiring

**Display**

| Display pin | ESP32 pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL | D18 (SPI clock) |
| SDA | D23 (SPI data) |
| RES | TX2 (GPIO 17) |
| DC | RX2 (GPIO 16) |
| CS | D5 |
| BLK | 3V3 (backlight always on) |

**Potentiometer**

| Pot leg | Connection |
|---|---|
| Outer leg | 3V3 |
| Middle leg (wiper) | D34 |
| Other outer leg | GND |

The pot's wiper connects to **GPIO 34** because it's on ADC1. The ESP32's ADC2 pins can't be used for analog reads while Wi-Fi is active, so ADC1 (GPIO 32–39) keeps the design compatible with future Wi-Fi features. The pot runs from 3.3 V, never 5 V, since the ESP32's inputs are only rated for 3.3 V.

**Buttons**

| Button | ESP32 pin |
|---|---|
| Upshift | D4 ↔ GND |
| Downshift | D21 ↔ GND |

Both buttons use the ESP32's internal pull-up resistors (`INPUT_PULLUP`), so no external resistors are needed. Each pin reads `HIGH` normally and `LOW` when pressed.

## How It Works

- **Reading the throttle:** `analogRead()` returns 0–4095 for 0–3.3 V. An exponential moving average smooths ADC jitter, and the readings are mapped to 0–100% throttle, ignoring the unreliable last few percent at each end of the ADC's range.
- **Vehicle model:** every 20 ms, acceleration is computed from throttle × gear ratio, minus air drag (proportional to speed²) and engine braking. Engine rpm = speed × gear ratio × a constant. At low speed, a simulated slipping clutch lets throttle raise rpm above idle.
- **Shift logic:** before a downshift, the code calculates what the rpm *would* be in the lower gear and refuses the shift if it's above the rev limit.
- **RPM bar:** the arc is 90 segments, each drawn as two filled triangles. The code tracks how many segments are lit and only redraws the ones that changed.
- **Debouncing:** a reusable `pressed()` function only accepts a button state after it has been stable for 30 ms, and returns true once per press.

All the car settings (gear ratios, redline, rev limit, power, drag) are constants at the top of `tachometer.ino`, so the "car" is easy to tune.

## Setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and add ESP32 support (Boards Manager → **esp32** by Espressif).
2. Install **Adafruit GC9A01A** from the Library Manager and accept its dependencies (Adafruit GFX, Adafruit BusIO).
3. Open the sketch for the level you want, e.g. `tachometer/tachometer.ino`.
4. Select **Tools → Board → ESP32 Dev Module** and the correct port.
5. Click **Upload**. If it hangs on `Connecting.....`, hold the **BOOT** button until the upload starts.

## Testing & Debugging

- **ADC range check (Level 2):** sweeping the pot end to end, the ESP32 read from about 0.19 V at the bottom up to 3.30 V at the top, rather than starting at 0 V. A full multimeter comparison across the range is planned to separate the pot's own limits from the ESP32 ADC's known dead zone near 0 V.
- **Button stuck "pressed":** at first the mode button did nothing when pressed but triggered when it was pulled *out* of the breadboard. A 4-pin tactile switch is two pairs of legs that are always connected internally, and the D4 and GND wires were on the same pair, so the pin was permanently shorted to ground. Fixed by placing the button across the breadboard's center gap and wiring diagonally opposite legs.
- **"Redefinition" compile errors:** two sketches were saved in the same folder, and Arduino compiles every `.ino` file in a sketch folder together. Each sketch now lives in its own folder named after its file.

## Acknowledgments

Code was drafted with help from Claude (AI). Wiring, hardware testing, debugging, and modifications were done by me.

## Author

Michelle Xie
