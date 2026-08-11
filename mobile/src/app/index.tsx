import * as ExpoDevice from 'expo-device';
import { StatusBar } from 'expo-status-bar';
import { useMemo } from 'react';
import { Platform, Pressable, ScrollView, StyleSheet, View } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { State } from 'react-native-ble-plx';

import { ThemedText } from '@/components/themed-text';
import { ThemedView } from '@/components/themed-view';
import { DYNO_DEVICE_NAME } from '@/features/ble/dyno-ble';
import { useDynoConnection } from '@/features/ble/use-dyno-connection';
import { useDynoForce } from '@/features/ble/use-dyno-force';
import { useDynoScanner } from '@/features/ble/use-dyno-scanner';
import { useTheme } from '@/hooks/use-theme';

const POUNDS_PER_NEWTON = 0.2248089431;

function adapterStatus(state: State) {
  switch (state) {
    case State.PoweredOn:
      return 'Bluetooth ready';
    case State.PoweredOff:
      return 'Bluetooth is off';
    case State.Unauthorized:
      return 'Bluetooth access denied';
    case State.Unsupported:
      return 'Bluetooth unavailable';
    default:
      return 'Checking Bluetooth…';
  }
}

function formatForce(force: number | null) {
  return force === null ? '—' : (force * POUNDS_PER_NEWTON).toFixed(1);
}

export default function HomeScreen() {
  const theme = useTheme();
  const {
    adapterState,
    devices,
    error: scanError,
    isScanning,
    startScan,
    stopScan,
  } = useDynoScanner();
  const {
    connect,
    connectedDevice,
    connectingDeviceId,
    disconnect,
    error: connectionError,
    rememberedDevice,
  } = useDynoConnection(stopScan);
  const { currentForce, error: forceError, peakForce } = useDynoForce(connectedDevice);
  const isNativeDevice = Platform.OS !== 'web' && ExpoDevice.isDevice;
  const canScan =
    isNativeDevice &&
    adapterState === State.PoweredOn &&
    !connectedDevice &&
    !connectingDeviceId;
  const error = connectionError ?? forceError ?? scanError;
  const displayedDevices = useMemo(() => {
    const discoveredDevices = devices.map((device) => ({
      id: device.id,
      name: (device.localName ?? device.name ?? DYNO_DEVICE_NAME).toLowerCase(),
      isRemembered: device.id === rememberedDevice?.id,
    }));

    if (rememberedDevice && !discoveredDevices.some((device) => device.id === rememberedDevice.id)) {
      discoveredDevices.unshift({
        ...rememberedDevice,
        isRemembered: true,
      });
    }

    return discoveredDevices.sort(
      (left, right) => Number(right.isRemembered) - Number(left.isRemembered),
    );
  }, [devices, rememberedDevice]);
  const status = connectedDevice
    ? 'dyno connected'
    : connectingDeviceId
      ? 'Connecting to dyno…'
      : adapterStatus(adapterState);

  return (
    <ThemedView style={styles.screen}>
      <StatusBar style="auto" />
      <SafeAreaView style={styles.safeArea} edges={['top', 'left', 'right']}>
        <ScrollView
          contentContainerStyle={styles.content}
          keyboardShouldPersistTaps="handled">
          <View style={styles.header}>
            <ThemedText type="title" style={styles.title}>
              dyno
            </ThemedText>
          </View>

          <ThemedView type="backgroundElement" style={styles.statusCard}>
            <View
              style={[
                styles.statusDot,
                {
                  backgroundColor:
                    connectedDevice || adapterState === State.PoweredOn
                      ? theme.success
                      : theme.textSecondary,
                },
              ]}
            />
            <View style={styles.statusCopy}>
              <ThemedText style={styles.statusTitle}>{status}</ThemedText>
            </View>
          </ThemedView>

          <View style={styles.metrics}>
            <ThemedView type="backgroundElement" style={styles.metricCard}>
              <ThemedText type="small" themeColor="textSecondary">
                Current force
              </ThemedText>
              <View style={styles.metricValueRow}>
                <ThemedText style={styles.metricValue}>{formatForce(currentForce)}</ThemedText>
                {currentForce !== null && (
                  <ThemedText type="small" themeColor="textSecondary">
                    lbs
                  </ThemedText>
                )}
              </View>
            </ThemedView>
            <ThemedView type="backgroundElement" style={styles.metricCard}>
              <ThemedText type="small" themeColor="textSecondary">
                Peak force
              </ThemedText>
              <View style={styles.metricValueRow}>
                <ThemedText style={styles.metricValue}>{formatForce(peakForce)}</ThemedText>
                {peakForce !== null && (
                  <ThemedText type="small" themeColor="textSecondary">
                    lbs
                  </ThemedText>
                )}
              </View>
            </ThemedView>
          </View>

          {!isNativeDevice && (
            <ThemedView
              lightColor="#FFF7E6"
              darkColor="#3A2C12"
              style={styles.notice}>
              <ThemedText type="small">
                BLE scanning requires a physical iOS or Android device. This simulator can still be
                used to review the interface.
              </ThemedText>
            </ThemedView>
          )}

          {error && (
            <ThemedView lightColor="#FFF0F0" darkColor="#3B1717" style={styles.notice}>
              <ThemedText type="small">{error}</ThemedText>
            </ThemedView>
          )}

          <Pressable
            accessibilityRole="button"
            disabled={!canScan}
            onPress={() => void (isScanning ? stopScan() : startScan())}
            style={({ pressed }) => [
              styles.scanButton,
              { backgroundColor: theme.accent },
              !canScan && styles.disabled,
              pressed && styles.pressed,
            ]}>
            <ThemedText style={styles.scanButtonText}>
              {isScanning ? 'Stop scanning' : 'Scan for device'}
            </ThemedText>
          </Pressable>

          <View style={styles.resultsHeader}>
            <ThemedText style={styles.resultsTitle}>Nearby devices</ThemedText>
            {isScanning && <ThemedText type="small">Scanning…</ThemedText>}
          </View>

          {displayedDevices.length === 0 ? (
            <ThemedView type="backgroundElement" style={styles.emptyState}>
              <ThemedText style={styles.emptyTitle}>
                {isScanning ? 'Looking for dyno…' : 'No devices found yet'}
              </ThemedText>
              <ThemedText type="small" themeColor="textSecondary" style={styles.emptyCopy}>
                Power on your dyno and keep it nearby, then start a scan.
              </ThemedText>
            </ThemedView>
          ) : (
            <View style={styles.deviceList}>
              {displayedDevices.map((device) => {
                const isConnected = connectedDevice?.id === device.id;
                const isConnecting = connectingDeviceId === device.id;
                const connectionIsBusy = Boolean(connectingDeviceId || connectedDevice);

                return (
                  <ThemedView key={device.id} type="backgroundElement" style={styles.deviceCard}>
                    <View style={[styles.deviceIcon, { backgroundColor: theme.accentMuted }]}>
                      <ThemedText style={{ color: theme.accent }}>D</ThemedText>
                    </View>
                    <View style={styles.deviceCopy}>
                      <ThemedText style={styles.deviceName}>{device.name}</ThemedText>
                    </View>
                    <Pressable
                      accessibilityRole="button"
                      accessibilityState={{
                        busy: isConnecting,
                        disabled: !isConnected && connectionIsBusy,
                      }}
                      disabled={!isConnected && connectionIsBusy}
                      onPress={() =>
                        void (isConnected ? disconnect() : connect(device.id, device.name))
                      }
                      style={({ pressed }) => [
                        styles.deviceButton,
                        { borderColor: theme.accent },
                        !isConnected && connectionIsBusy && styles.disabled,
                        pressed && styles.pressed,
                      ]}>
                      <ThemedText style={[styles.deviceButtonText, { color: theme.accent }]}>
                        {isConnected ? 'Disconnect' : isConnecting ? 'Connecting…' : 'Connect'}
                      </ThemedText>
                    </Pressable>
                  </ThemedView>
                );
              })}
            </View>
          )}
        </ScrollView>
      </SafeAreaView>
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  screen: {
    flex: 1,
  },
  safeArea: {
    flex: 1,
  },
  content: {
    width: '100%',
    maxWidth: 640,
    alignSelf: 'center',
    paddingHorizontal: 24,
    paddingTop: 32,
    paddingBottom: 48,
    gap: 16,
  },
  header: {
    marginBottom: 12,
  },
  title: {
    fontSize: 44,
    lineHeight: 50,
    marginBottom: 4,
  },
  statusCard: {
    minHeight: 72,
    borderRadius: 18,
    paddingHorizontal: 18,
    paddingVertical: 14,
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
  },
  statusDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
  },
  statusCopy: {
    flex: 1,
  },
  statusTitle: {
    fontWeight: '700',
  },
  metrics: {
    flexDirection: 'row',
    gap: 12,
  },
  metricCard: {
    flex: 1,
    minHeight: 112,
    borderRadius: 18,
    padding: 16,
    justifyContent: 'space-between',
  },
  metricValueRow: {
    flexDirection: 'row',
    alignItems: 'baseline',
    gap: 5,
  },
  metricValue: {
    fontSize: 32,
    lineHeight: 38,
    fontWeight: '700',
    fontVariant: ['tabular-nums'],
  },
  notice: {
    borderRadius: 14,
    padding: 14,
  },
  scanButton: {
    minHeight: 54,
    borderRadius: 17,
    alignItems: 'center',
    justifyContent: 'center',
    marginTop: 2,
  },
  scanButtonText: {
    color: '#FFFFFF',
    fontWeight: '700',
  },
  disabled: {
    opacity: 0.4,
  },
  pressed: {
    opacity: 0.8,
  },
  resultsHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginTop: 16,
  },
  resultsTitle: {
    fontSize: 20,
    fontWeight: '700',
  },
  emptyState: {
    minHeight: 156,
    borderRadius: 18,
    alignItems: 'center',
    justifyContent: 'center',
    padding: 24,
  },
  emptyTitle: {
    fontWeight: '700',
    marginBottom: 5,
  },
  emptyCopy: {
    textAlign: 'center',
    maxWidth: 300,
  },
  deviceList: {
    gap: 10,
  },
  deviceCard: {
    minHeight: 76,
    borderRadius: 18,
    padding: 14,
    flexDirection: 'row',
    alignItems: 'center',
    gap: 14,
  },
  deviceIcon: {
    width: 46,
    height: 46,
    borderRadius: 14,
    alignItems: 'center',
    justifyContent: 'center',
  },
  deviceCopy: {
    flex: 1,
  },
  deviceName: {
    fontWeight: '700',
  },
  deviceButton: {
    minHeight: 40,
    borderWidth: 1,
    borderRadius: 12,
    paddingHorizontal: 14,
    alignItems: 'center',
    justifyContent: 'center',
  },
  deviceButtonText: {
    fontWeight: '700',
  },
});
