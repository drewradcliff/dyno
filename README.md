# dyno

Finger-strength dynamometer

## Hardware

- Seeed Studio XIAO ESP32C3
- HX711 configured for 80 samples per second
- Load cell sized for the expected finger force

The firmware expects HX711 data on `D4` and clock on `D5`.

## Library

- [HX711 by bogde](https://github.com/bogde/HX711)

## Serial output

Open the serial port at 115200 baud. Each available HX711 conversion is read
once, timestamped, low-pass filtered, and emitted as CSV:

```csv
timestamp_ms,raw,force_newtons
1250,25802,0.406
1263,26143,8.104
1275,26994,27.314
```

Lines beginning with `#` are status messages or effort events and can be
ignored.

The `raw` column is the unmodified HX711 reading. `force_newtons` is calculated
from the tared, exponentially filtered reading.

## Tare and calibration

At startup, the device collects 16 unloaded samples to establish its tare. Keep
the load cell unloaded until `# tare_complete` appears. Send these newline-
terminated commands over the same serial port:

```text
tare
factor 1461.258
calibrate 98.0665
thresholds 20 10 1000
last
status
help
```

- `tare` starts a fresh 16-sample unloaded tare.
- `factor <counts_per_newton>` directly sets the signed calibration factor.
- `calibrate <known_newtons>` calculates the factor from the force currently
  applied. Tare with no load, apply a known load, wait for the reading to
  settle, then send this command. A 10 kg calibration mass applies about
  `98.0665 N`.
- `thresholds <start_n> <end_n> <hold_ms>` configures effort detection. The
  start threshold must be greater than the end threshold.
- `last` reports the most recently completed effort.
- `status` reports calibration, current effort state and peak, and thresholds.

Calibration commands persist the factor in ESP32 flash. The default
`1461.258 counts/N` value is converted from the initial bench calibration of
`6500 counts/lbf`:

```text
6500 counts/lbf / 4.448221615 N/lbf = 1461.258 counts/N
```

Send `factor 1461.258` once after flashing if an older calibration value is
already stored in the device. A negative factor is valid when the raw reading
falls as force is applied.

The `thresholds` command also persists its values in ESP32 flash.

## Efforts and peak force

An **effort** is one detected grip or pull. Effort detection uses filtered
force:

- Start when force rises above 20 N.
- Assign a monotonically increasing effort ID for the current device boot.
- Track the maximum filtered force as the effort peak.
- End when force remains below 10 N for one second.

Every completed effort records its ID, start timestamp, end timestamp,
duration, and peak force. The last completed result remains available in RAM
through the `last` command until the device restarts. Taring, calibrating, or
changing thresholds during an effort emits an `effort_cancelled` event rather
than treating partial data as a completed effort.

The separate start and end thresholds provide hysteresis: force must cross 20
N to begin, but it can fluctuate anywhere above 10 N without ending. If force
rises to 10 N or more during the one-second release period, the end timer is
reset.
