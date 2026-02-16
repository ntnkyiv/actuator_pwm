# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Compass-controlled antenna rotator/actuator system on **XIAO ESP32-S3**. Combines stepper motor (azimuth), linear actuator (elevation), ICM-20948 compass, WiFi web interface, and Ethernet (CH9120 serial-to-ethernet bridge) control.

## Build & Upload

**Platform:** Arduino IDE (no PlatformIO)
**Board:** `esp32:esp32:XIAO_ESP32S3` at 240MHz, 8MB flash
**Upload speed:** 921600

Build output lands in `build/esp32.esp32.XIAO_ESP32S3/`.

OTA firmware upload via Ethernet:
```
python ota_update.py
```

## Architecture

**Main loop** (`actuator_pwm.ino`) runs a priority-ordered polling loop:
1. OTA update check (blocks everything)
2. `stepper.run()` — every iteration for smooth pulses
3. WiFi/WebSocket handling
4. Serial JSON command parsing
5. Linear actuator auto-brake timer
6. Compass read — **only when stepper is idle** (`distanceToGo() == 0`) to prevent I2C blocking from disrupting step pulses
7. Calibration routine (flag-triggered)
8. LED heartbeat

**Modules:**

| File | Role |
|------|------|
| `config.h` | Pin definitions, global state variables, extern declarations |
| `Compass.h/cpp` | ICM-20948 init (5 retries, I2C recovery), calibration (360° rotation), pitch/roll/yaw with smoothing buffer, EMI low-pass filter, NVS storage of offsets |
| `StepperControl.h/cpp` | AccelStepper wrapper. Degree-to-step conversion: `steps = degrees × (microstep × reductor / stepsize)`. Compass-mode auto-correction loop |
| `LinearActuator.h/cpp` | PWM driver (25 kHz, 8-bit). Speed ±255. Auto-brake timeout (default 20s) |
| `WiFiManager.h/cpp` | STA/AP dual mode, AsyncWebServer:80, WebSocket `/ws`, captive portal, NVS credentials |
| `WebPages.h/cpp` | HTML/JS stored in PROGMEM |
| `SerialProtocol.h/cpp` | JSON command protocol over UART1 (40+ commands for stepper, compass, WiFi, actuator, system) |
| `fw_update.h/cpp` | OTA via TCP:5000, MD5 verification, 512-byte chunks with ACK |

## Hardware Pins

| Function | GPIO |
|----------|------|
| Stepper STEP/DIR/EN | 3 / 4 / 10 |
| Linear actuator IN1/IN2 | 1 / 2 (PWM ch 0,1) |
| I2C SDA/SCL | 5 / 6 |
| UART1 RX/TX (CH9120) | 44 / 43 |
| CH9120 RESET | 7 |

Compass I2C address: 0x68 or 0x69 (auto-detected).

## Key Dependencies

AccelStepper, Adafruit_ICM20948, ESPAsyncWebServer, AsyncTCP, ArduinoJson, DNSServer, Preferences (NVS), Wire, Update, WiFi — all from Arduino Library Manager.

## NVS Persistence

Namespace `"compass"`: WiFi creds, stepper params (maxspeed, acceleration, pulsewidth, microstep, reductor, stepsize), mag calibration offsets, smoothing window, EMI filter alpha.
Namespace `"linear"`: auto-brake timeout.

## Serial Protocol

UART1 at 115200 baud. JSON format: `{"cmd":"degree","value":90}`. See `SerialProtocol.cpp` for the full command reference.

## Language

Comments and commit messages are in Ukrainian.
