import { BleManager } from 'react-native-ble-plx';

let manager: BleManager | undefined;

export function getBleManager() {
  manager ??= new BleManager();
  return manager;
}
