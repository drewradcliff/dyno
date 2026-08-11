import { useEffect, useState } from 'react';
import type { Device } from 'react-native-ble-plx';

import {
  DYNO_FORCE_CHARACTERISTIC_UUID,
  DYNO_SERVICE_UUID,
} from '@/features/ble/dyno-ble';

const BASE64_ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

function decodeBase64Ascii(value: string) {
  let decoded = '';
  let buffer = 0;
  let bitCount = 0;

  for (const character of value) {
    if (character === '=') {
      break;
    }

    const index = BASE64_ALPHABET.indexOf(character);
    if (index === -1) {
      continue;
    }

    buffer = (buffer << 6) | index;
    bitCount += 6;

    if (bitCount >= 8) {
      bitCount -= 8;
      decoded += String.fromCharCode((buffer >> bitCount) & 0xff);
    }
  }

  return decoded;
}

export function useDynoForce(device: Device | null) {
  const [currentForce, setCurrentForce] = useState<number | null>(null);
  const [peakForce, setPeakForce] = useState<number | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    setCurrentForce(null);
    setPeakForce(null);
    setError(null);

    if (!device) {
      return;
    }

    let isActive = true;
    const subscription = device.monitorCharacteristicForService(
      DYNO_SERVICE_UUID,
      DYNO_FORCE_CHARACTERISTIC_UUID,
      (monitorError, characteristic) => {
        if (!isActive) {
          return;
        }

        if (monitorError) {
          setCurrentForce(null);
          setError('Unable to read force data.');
          return;
        }

        if (!characteristic?.value) {
          return;
        }

        const force = Number.parseFloat(decodeBase64Ascii(characteristic.value));
        if (!Number.isFinite(force)) {
          return;
        }

        setError(null);
        setCurrentForce(force);
        setPeakForce((currentPeak) =>
          currentPeak === null ? force : Math.max(currentPeak, force),
        );
      },
    );

    return () => {
      isActive = false;
      subscription.remove();
    };
  }, [device]);

  return {
    currentForce,
    error,
    peakForce,
  };
}
