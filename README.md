# Dyno

Dyno is an open-source finger-strength dynamometer. The project combines a
custom load-cell PCB, firmware for a Seeed Studio XIAO ESP32C3, and a mobile
app that displays force measurements over Bluetooth Low Energy.

## Repository

- [`hardware/`](hardware/) — KiCad schematic, PCB, and project-local libraries
- [`firmware/`](firmware/) — ESP32C3 firmware, calibration, and telemetry
- [`mobile/`](mobile/) — Expo/React Native companion app

The project is under active development. The PCB is currently an unverified
prototype and should be reviewed before fabrication.

## License

Original project work is licensed under the [MIT License](LICENSE). Third-party
components retain their upstream licenses and are attributed in the relevant
component directories.
