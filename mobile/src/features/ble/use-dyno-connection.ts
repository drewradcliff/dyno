import AsyncStorage from '@react-native-async-storage/async-storage';
import { useCallback, useEffect, useRef, useState } from 'react';
import { Platform } from 'react-native';
import type { Device, Subscription } from 'react-native-ble-plx';

import { getBleManager } from '@/features/ble/ble-manager';
import { DYNO_DEVICE_NAME } from '@/features/ble/dyno-ble';

const REMEMBERED_DEVICE_KEY = '@dyno/remembered-device';
const CONNECTION_TIMEOUT_MS = 10_000;

export type RememberedDynoDevice = {
  id: string;
  name: string;
};

function isRememberedDevice(value: unknown): value is RememberedDynoDevice {
  if (!value || typeof value !== 'object') {
    return false;
  }

  const device = value as Partial<RememberedDynoDevice>;
  return typeof device.id === 'string' && typeof device.name === 'string';
}

function connectionErrorMessage(error: unknown) {
  return error instanceof Error ? error.message : 'Unable to connect to dyno.';
}

export function useDynoConnection(stopScan: () => Promise<void>) {
  const bleManager = Platform.OS === 'web' ? null : getBleManager();
  const disconnectionSubscription = useRef<Subscription | null>(null);
  const activeConnectionAttempt = useRef(0);
  const [rememberedDevice, setRememberedDevice] = useState<RememberedDynoDevice | null>(null);
  const [connectedDevice, setConnectedDevice] = useState<Device | null>(null);
  const [connectingDeviceId, setConnectingDeviceId] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let isMounted = true;

    async function loadRememberedDevice() {
      try {
        const storedDevice = await AsyncStorage.getItem(REMEMBERED_DEVICE_KEY);
        if (!storedDevice || !isMounted) {
          return;
        }

        const parsedDevice: unknown = JSON.parse(storedDevice);
        if (isRememberedDevice(parsedDevice)) {
          setRememberedDevice(parsedDevice);
        }
      } catch {
        if (isMounted) {
          setError('Unable to load the previously selected device.');
        }
      }
    }

    void loadRememberedDevice();

    return () => {
      isMounted = false;
      activeConnectionAttempt.current += 1;
      disconnectionSubscription.current?.remove();
    };
  }, []);

  const connect = useCallback(
    async (deviceId: string, deviceName?: string | null) => {
      if (!bleManager || connectingDeviceId || connectedDevice) {
        return;
      }

      const attempt = activeConnectionAttempt.current + 1;
      activeConnectionAttempt.current = attempt;
      setError(null);
      setConnectingDeviceId(deviceId);
      await stopScan();

      let pendingDevice: Device | null = null;

      try {
        pendingDevice = await bleManager.connectToDevice(deviceId, {
          timeout: CONNECTION_TIMEOUT_MS,
        });
        const discoveredDevice = await pendingDevice.discoverAllServicesAndCharacteristics();

        if (activeConnectionAttempt.current !== attempt) {
          await discoveredDevice.cancelConnection();
          return;
        }

        disconnectionSubscription.current?.remove();
        disconnectionSubscription.current = discoveredDevice.onDisconnected((disconnectError) => {
          setConnectedDevice(null);
          disconnectionSubscription.current?.remove();
          disconnectionSubscription.current = null;

          if (disconnectError) {
            setError('The dyno connection was lost.');
          }
        });

        setConnectedDevice(discoveredDevice);
        pendingDevice = null;

        const selectedDevice = {
          id: discoveredDevice.id,
          name: (
            deviceName ??
            discoveredDevice.localName ??
            discoveredDevice.name ??
            DYNO_DEVICE_NAME
          ).toLowerCase(),
        };
        setRememberedDevice(selectedDevice);

        try {
          await AsyncStorage.setItem(REMEMBERED_DEVICE_KEY, JSON.stringify(selectedDevice));
        } catch {
          setError('Connected, but unable to remember this device.');
        }
      } catch (connectionError) {
        if (pendingDevice) {
          try {
            await pendingDevice.cancelConnection();
          } catch {
            // Preserve the original connection error.
          }
        }
        setError(connectionErrorMessage(connectionError));
      } finally {
        if (activeConnectionAttempt.current === attempt) {
          setConnectingDeviceId(null);
        }
      }
    },
    [bleManager, connectedDevice, connectingDeviceId, stopScan],
  );

  const disconnect = useCallback(async () => {
    if (!connectedDevice) {
      return;
    }

    setError(null);

    try {
      await connectedDevice.cancelConnection();
      setConnectedDevice(null);
    } catch (disconnectError) {
      setError(connectionErrorMessage(disconnectError));
    }
  }, [connectedDevice]);

  return {
    connect,
    connectedDevice,
    connectingDeviceId,
    disconnect,
    error,
    rememberedDevice,
  };
}
