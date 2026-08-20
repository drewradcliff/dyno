# Dyno firmware

Arduino firmware for the Seeed Studio XIAO ESP32C3. It reads an HX711 load-cell
ADC, calculates force, detects completed efforts, and publishes measurements
over serial and Bluetooth Low Energy.

## Setup

Install the ESP32 Arduino core and the
[HX711 library by bogde](https://github.com/bogde/HX711), then open
[`dyno/dyno.ino`](dyno/dyno.ino) in the Arduino IDE.

The HX711 connections expected by the firmware are:

- `D1`: active-low sleep/wake button
- `D3`: active-high status LED
- `D4`: HX711 `DOUT`
- `D5`: HX711 `PD_SCK`

The status LED is off while the dyno is idle, solid while an effort is active,
and blinks every 500 ms if the HX711 is not detected at startup.

## Sleep and wake button

The schematic's R2 pulls XIAO `D1` (GPIO 3) high, and SW1 connects it to ground
when pressed. Hold SW1 for two seconds and then release it to enter deep sleep.
Releasing before sleep prevents the active-low wake source from immediately
waking the board again. Press SW1 once while the board is asleep to wake it.

After a button wake, the firmware waits for SW1 to be released before arming
the hold-to-sleep behavior. This prevents a long wake press from creating a
sleep/wake loop. Deep sleep can also be requested with the `sleep` serial or
BLE command; if SW1 is held, sleep begins when it is released.

## BLE

The XIAO advertises as `Dyno` with an open GATT service. Connecting does not
require pairing or bonding.

| Item | UUID | Properties | Value |
| --- | --- | --- | --- |
| Dyno service | `7b7e1000-6ba3-4d8f-9e2f-4f2f0c7a0000` | — | — |
| Force | `7b7e1001-6ba3-4d8f-9e2f-4f2f0c7a0000` | Read, Notify | UTF-8 newtons with three decimal places, such as `12.345` |
| Command | `7b7e1002-6ba3-4d8f-9e2f-4f2f0c7a0000` | Write, Write Without Response | UTF-8 command, with or without a trailing newline |

Force notifications contain the latest filtered reading and are limited to
40 updates per second.

## Serial output

Open the serial port at 115200 baud. Each available HX711 conversion is read
once, timestamped, low-pass filtered, and emitted as CSV:

```csv
timestamp_ms,raw,force_newtons
1250,25802,0.406
1263,26143,8.104
1275,26994,27.314
```

Lines beginning with `#` are status messages or effort events. The `raw`
column is the unmodified HX711 reading; `force_newtons` is calculated from the
tared, filtered reading.

## Tare and calibration

At startup, the device collects 16 unloaded samples to establish its tare.
Keep the load cell unloaded until `# tare_complete` appears. Available serial
commands are:

```text
tare
factor 1461.258
calibrate 98.0665
thresholds 20 10 1000
last
status
sleep
help
```

- `tare` starts a fresh 16-sample unloaded tare.
- `factor <counts_per_newton>` sets the signed calibration factor.
- `calibrate <known_newtons>` calculates the factor from the currently applied
  force after an unloaded tare.
- `thresholds <start_n> <end_n> <hold_ms>` configures effort detection. The
  start threshold must be greater than the end threshold.
- `last` reports the most recently completed effort.
- `status` reports calibration, effort state, peak, and thresholds.
- `sleep` enters deep sleep with SW1 on `D1` configured as the active-low wake
  source.

Calibration and threshold settings persist in ESP32 flash. The default factor
converts the initial bench calibration of 6500 counts per pound-force:

```text
6500 counts/lbf / 4.448221615 N/lbf = 1461.258 counts/N
```

A negative calibration factor is valid when the raw reading falls as force is
applied.

## Efforts and peak force

An effort begins when filtered force rises above 20 N. The firmware tracks its
peak and ends it after force remains below 10 N for one second. This hysteresis
prevents small fluctuations from prematurely ending an effort.

Every completed effort records its ID, timestamps, duration, and peak force.
The last result remains available through `last` until restart. Taring,
calibrating, or changing thresholds during an effort cancels that partial
effort.
