<div align="center">

# Arduino Light Monitor
### Dual-Relay Automated Light Control System

<br/>

[![Arduino](https://img.shields.io/badge/Arduino-00878A?style=for-the-badge&logo=arduino&logoColor=white)](https://arduino.cc)
[![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org)
[![DS1302](https://img.shields.io/badge/DS1302_RTC-555555?style=for-the-badge)]()
[![I2C LCD](https://img.shields.io/badge/I2C_LCD_16x2-1e3a5f?style=for-the-badge)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow?style=for-the-badge)](https://opensource.org/licenses/MIT)

<br/>

> **An Arduino-based automated light controller — scheduling relay switching via a real-time clock and reacting to ambient light levels through an LDR sensor, all configurable from a 4x4 keypad with live LCD feedback.**

<br/>

</div>

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Required](#hardware-required)
- [Pin Connections](#pin-connections)
  - [DS1302 RTC Module](#ds1302-rtc-module)
  - [I2C LCD Display](#i2c-lcd-display)
  - [4x4 Matrix Keypad](#4x4-matrix-keypad)
  - [LDR Sensor](#ldr-sensor)
  - [Relay Modules](#relay-modules)
- [Libraries Required](#libraries-required)
- [First-Time RTC Setup](#first-time-rtc-setup)
- [How to Use](#how-to-use)
  - [Normal Screen](#normal-screen)
  - [Keypad Reference](#keypad-reference)
  - [Setting a Schedule](#setting-a-schedule-relay-1)
  - [Setting the LDR Threshold](#setting-the-ldr-threshold-relay-2)
  - [Manual Override](#manual-override-relay-1)
- [Serial Monitor Output](#serial-monitor-output)
- [Project Structure](#project-structure)
- [License](#license)

---

## Overview

Arduino Light Monitor connects two independent relay channels to two separate control strategies — time and light. Relay 1 follows a user-defined ON/OFF schedule stored against the DS1302 real-time clock. Relay 2 reads ambient brightness from an LDR and switches automatically when darkness falls below a configurable threshold. Both channels are fully configurable at runtime from a 4x4 keypad, with live status shown on a 16x2 I2C LCD. No PC connection, no re-flashing required — everything is set in the field.

---

## Features

| Feature | Description |
|---|---|
| Time-Scheduled Relay | Relay 1 follows a custom ON/OFF time set via keypad against the DS1302 RTC |
| Ambient Light Relay | Relay 2 switches ON when dark and OFF when bright using an LDR |
| Adjustable LDR Threshold | Darkness sensitivity configured from the keypad — no code changes needed |
| Overnight Schedule Support | Correctly handles schedules that cross midnight (e.g. 22:00 to 06:00) |
| Manual Override | Relay 1 can be toggled instantly from the keypad at any time |
| Backspace Input | Press C while entering numbers to delete the last digit |
| Live LCD Status | Current time, date, and both relay states always visible on screen |
| Serial Monitor Logging | All sensor readings and relay events printed at 9600 baud for debugging |

---

## Hardware Required

| Component | Quantity |
|---|---|
| Arduino Uno or Mega | 1 |
| DS1302 RTC Module | 1 |
| 16x2 I2C LCD (address 0x27) | 1 |
| 4x4 Matrix Keypad | 1 |
| LDR (Light Dependent Resistor) | 1 |
| 10k Ohm Resistor (voltage divider) | 1 |
| 5V Relay Module — Normally Open x2 | 2 |
| Jumper Wires | Several |
| Breadboard | 1 |

---

## Pin Connections

### DS1302 RTC Module

| DS1302 Pin | Arduino Pin |
|---|---|
| DAT (IO) | D7 |
| CLK | D6 |
| RST (CE) | D8 |
| VCC | 5V |
| GND | GND |

---

### I2C LCD Display

| LCD Pin | Arduino Pin |
|---|---|
| SDA | A4 |
| SCL | A5 |
| VCC | 5V |
| GND | GND |

> The I2C address is set to `0x27`. If your display stays blank on startup, try `0x3F` and update the address in the code on line 4.

---

### 4x4 Matrix Keypad

| Keypad Pin | Arduino Pin |
|---|---|
| R1 | D9 |
| R2 | D10 |
| R3 | D11 |
| R4 | D12 |
| C1 | D5 |
| C2 | D4 |
| C3 | D3 |
| C4 | D2 |

---

### LDR Sensor

Wire the LDR as a voltage divider with a 10k Ohm resistor:

```
5V ──── LDR ──── A0 ──── 10kOhm ──── GND
```

| Connection | Arduino Pin |
|---|---|
| LDR junction (middle point) | A0 |

> Raw analog values range from **0 (very dark)** to **1023 (very bright)**. Relay 2 activates when the reading falls **below** your configured threshold.

---

### Relay Modules

| Relay | Arduino Pin | Function |
|---|---|---|
| Relay 1 | D13 | Time-scheduled load |
| Relay 2 | A1 | LDR ambient light load |

> Both relays must be wired as **Normally Open (NO)**. The relay closes when the Arduino sends `HIGH`, completing the circuit to the load.

**Safety:** If switching mains voltage AC loads, use an optocoupler-isolated relay module and ensure all high-voltage wiring is handled by a qualified electrician.

---

## Libraries Required

Install via the Arduino IDE Library Manager — `Sketch > Include Library > Manage Libraries`:

| Library | Purpose |
|---|---|
| `LiquidCrystal_I2C` | I2C LCD display driver |
| `ThreeWire` | DS1302 three-wire communication |
| `Rtc by Makuna` | DS1302 RTC driver |
| `Keypad` | 4x4 matrix keypad input |
| `Wire` | I2C bus (built-in, no install needed) |

---

## First-Time RTC Setup

The DS1302 must have its date and time initialized before use. If the LCD shows **"RTC ERROR!! Upload Code 1!"** on startup, upload the sketch below once to set the module, then immediately re-upload the main project sketch.

```cpp
#include <ThreeWire.h>
#include <RtcDS1302.h>

ThreeWire myWire(7, 6, 8); // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);
  Rtc.SetIsWriteProtected(false);
  Rtc.SetIsRunning(true);
}

void loop() {}
```

---

## How to Use

### Normal Screen

The LCD shows live status at all times:

```
12:45:30 R1:OFF
D:01/05/26 R2:ON
```

| Row | Content |
|---|---|
| Row 0 | Current time + Relay 1 state |
| Row 1 | Current date + Relay 2 state |

---

### Keypad Reference

| Key | Action |
|---|---|
| `A` | Open schedule setup for Relay 1 |
| `B` | Set LDR darkness threshold for Relay 2 |
| `D` | Manually toggle Relay 1 ON / OFF |
| `#` | Confirm input / advance to next step |
| `*` | Cancel and return to normal screen |
| `C` | Backspace — delete last entered digit |
| `0 – 9` | Numeric input |

---

### Setting a Schedule (Relay 1)

1. Press **A** from the normal screen.
2. Enter the **ON hour** (00–23) then press `#`.
3. Enter the **ON minute** (00–59) then press `#`.
4. Enter the **OFF hour** (00–23) then press `#`.
5. Enter the **OFF minute** (00–59) then press `#`.
6. Review the schedule summary shown on the LCD.
7. Press `#` to save or `*` to cancel.

> Overnight schedules are fully supported. Setting ON=22:00 and OFF=06:00 will correctly keep the relay on through midnight.

> Press **C** at any input step to delete the last typed digit.

---

### Setting the LDR Threshold (Relay 2)

1. Press **B** from the normal screen.
2. The current threshold value is shown on the LCD.
3. Enter a new value between **0 and 1023** then press `#`.
4. Press `*` to cancel without saving.

**Threshold reference:**

| Environment | Typical Raw LDR Value |
|---|---|
| Bright sunlight | 800 – 1023 |
| Indoor lighting | 400 – 700 |
| Dusk / dim | 150 – 400 |
| Dark / night | 0 – 150 |

A threshold of **400** means Relay 2 activates when the raw reading drops below 400. Adjust to suit your specific LDR and environment — every LDR behaves slightly differently.

---

### Manual Override (Relay 1)

Press **D** from the normal screen to immediately flip Relay 1. This disables the active schedule until a new one is saved via **A**.

---

## Serial Monitor Output

Connect at **9600 baud** to view live logs:

```
Time=12:45:30  Light=312  Thresh=400  R1=OFF  R2=ON
RELAY2 ON
ON=18:00 OFF=06:00
RELAY1 ON
```

---

## Project Structure

```
arduino-light-monitor/
├── light_monitor.ino    # Main sketch
└── README.md            # This file
```

---

## License

This project is open source under the [MIT License](https://opensource.org/licenses/MIT). Feel free to use, modify, and share.

---

<div align="center">

Built for reliable, fieldworthy light automation.

</div>