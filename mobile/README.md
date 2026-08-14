# Dyno mobile

Expo/React Native companion app for the Dyno force sensor. It discovers the
device over Bluetooth Low Energy, reconnects to known devices, and displays
live force measurements.

## Development

```bash
npm install
npx expo run:ios
```

Use `npx expo run:android` for Android. BLE requires a development build and a
physical device; it is not available in Expo Go or a typical simulator.

After installing the development build, start Metro with:

```bash
npm start
```
