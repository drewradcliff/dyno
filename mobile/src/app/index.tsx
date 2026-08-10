import * as ExpoDevice from 'expo-device';
import { StatusBar } from 'expo-status-bar';
import { Platform, Pressable, ScrollView, StyleSheet, View } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { State } from 'react-native-ble-plx';

import { ThemedText } from '@/components/themed-text';
import { ThemedView } from '@/components/themed-view';
import { DYNO_DEVICE_NAME } from '@/features/ble/dyno-ble';
import { useDynoScanner } from '@/features/ble/use-dyno-scanner';
import { useTheme } from '@/hooks/use-theme';

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

export default function HomeScreen() {
  const theme = useTheme();
  const { adapterState, devices, error, isScanning, startScan, stopScan } = useDynoScanner();
  const isNativeDevice = Platform.OS !== 'web' && ExpoDevice.isDevice;
  const canScan = isNativeDevice && adapterState === State.PoweredOn;

  return (
    <ThemedView style={styles.screen}>
      <StatusBar style="auto" />
      <SafeAreaView style={styles.safeArea} edges={['top', 'left', 'right']}>
        <ScrollView
          contentContainerStyle={styles.content}
          keyboardShouldPersistTaps="handled">
          <View style={styles.header}>
            <ThemedText style={styles.eyebrow}>FORCE SENSOR</ThemedText>
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
                    adapterState === State.PoweredOn ? theme.success : theme.textSecondary,
                },
              ]}
            />
            <View style={styles.statusCopy}>
              <ThemedText style={styles.statusTitle}>{adapterStatus(adapterState)}</ThemedText>
            </View>
          </ThemedView>

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

          {devices.length === 0 ? (
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
              {devices.map((device) => (
                <ThemedView key={device.id} type="backgroundElement" style={styles.deviceCard}>
                  <View style={[styles.deviceIcon, { backgroundColor: theme.accentMuted }]}>
                    <ThemedText style={{ color: theme.accent }}>D</ThemedText>
                  </View>
                  <View style={styles.deviceCopy}>
                    <ThemedText style={styles.deviceName}>
                      {device.localName ?? device.name ?? DYNO_DEVICE_NAME}
                    </ThemedText>
                  </View>
                </ThemedView>
              ))}
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
  eyebrow: {
    fontSize: 12,
    fontWeight: '700',
    letterSpacing: 1.5,
    marginBottom: 6,
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
});
