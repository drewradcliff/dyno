import { useCallback, useEffect, useRef, useState } from 'react';
import { PermissionsAndroid, Platform } from 'react-native';
import { BleManager, type Device, State } from 'react-native-ble-plx';

import { DYNO_SERVICE_UUID, SCAN_DURATION_MS } from '@/features/ble/dyno-ble';

let manager: BleManager | undefined;

function getBleManager() {
  manager ??= new BleManager();
  return manager;
}

async function requestAndroidPermissions() {
  if (Platform.OS !== 'android') {
    return true;
  }

  if (Number(Platform.Version) >= 31) {
    const result = await PermissionsAndroid.requestMultiple([
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
    ]);

    return (
      result[PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN] ===
        PermissionsAndroid.RESULTS.GRANTED &&
      result[PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT] ===
        PermissionsAndroid.RESULTS.GRANTED
    );
  }

  const result = await PermissionsAndroid.request(
    PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
  );
  return result === PermissionsAndroid.RESULTS.GRANTED;
}

function getErrorMessage(error: unknown) {
  return error instanceof Error ? error.message : 'Unable to scan for dyno devices.';
}

export function useDynoScanner() {
  const bleManager = Platform.OS === 'web' ? null : getBleManager();
  const scanTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const scanIsActive = useRef(false);
  const [adapterState, setAdapterState] = useState<State>(
    Platform.OS === 'web' ? State.Unsupported : State.Unknown,
  );
  const [devices, setDevices] = useState<Device[]>([]);
  const [isScanning, setIsScanning] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const clearScanTimer = useCallback(() => {
    if (scanTimer.current) {
      clearTimeout(scanTimer.current);
      scanTimer.current = null;
    }
  }, []);

  const stopScan = useCallback(async () => {
    clearScanTimer();
    scanIsActive.current = false;
    setIsScanning(false);

    if (!bleManager) {
      return;
    }

    try {
      await bleManager.stopDeviceScan();
    } catch (scanError) {
      setError(getErrorMessage(scanError));
    }
  }, [bleManager, clearScanTimer]);

  const startScan = useCallback(async () => {
    if (!bleManager || scanIsActive.current) {
      return;
    }

    setError(null);

    if (adapterState !== State.PoweredOn) {
      setError('Turn on Bluetooth before scanning.');
      return;
    }

    try {
      const hasPermission = await requestAndroidPermissions();
      if (!hasPermission) {
        setError('Bluetooth permission is required to find your dyno.');
        return;
      }

      setDevices([]);
      scanIsActive.current = true;
      setIsScanning(true);

      await bleManager.startDeviceScan([DYNO_SERVICE_UUID], null, (scanError, device) => {
        if (scanError) {
          setError(scanError.message);
          void stopScan();
          return;
        }

        if (!device || !scanIsActive.current) {
          return;
        }

        setDevices((currentDevices) => {
          const nextDevices = currentDevices.filter(
            (currentDevice) => currentDevice.id !== device.id,
          );
          nextDevices.push(device);
          return nextDevices.sort((left, right) => (right.rssi ?? -200) - (left.rssi ?? -200));
        });
      });

      scanTimer.current = setTimeout(() => {
        void stopScan();
      }, SCAN_DURATION_MS);
    } catch (scanError) {
      scanIsActive.current = false;
      setIsScanning(false);
      setError(getErrorMessage(scanError));
    }
  }, [adapterState, bleManager, stopScan]);

  useEffect(() => {
    if (!bleManager) {
      return;
    }

    const subscription = bleManager.onStateChange((nextState) => {
      setAdapterState(nextState);
      if (nextState !== State.PoweredOn && scanIsActive.current) {
        void stopScan();
      }
    }, true);

    return () => {
      subscription.remove();
      clearScanTimer();
      if (scanIsActive.current) {
        scanIsActive.current = false;
        void bleManager.stopDeviceScan();
      }
    };
  }, [bleManager, clearScanTimer, stopScan]);

  return {
    adapterState,
    devices,
    error,
    isScanning,
    startScan,
    stopScan,
  };
}
