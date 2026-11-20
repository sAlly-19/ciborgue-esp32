import { PermissionsAndroid, Platform } from 'react-native';
import type { BleManager, Device, Subscription } from 'react-native-ble-plx';
import { BLE_SERVICE, BLE_SETTINGS, commitBleSettings, decodeBleSettings, type BleSettings } from './ble-protocol';
import { ApiError } from './protocol';

async function permissions() {
  if (Platform.OS === 'ios') return;
  if (Platform.OS !== 'android') throw new ApiError('Abra o app nativo para usar Bluetooth.');
  const p = PermissionsAndroid.PERMISSIONS;
  const required = Number(Platform.Version) >= 31
    ? [p.BLUETOOTH_SCAN, p.BLUETOOTH_CONNECT]
    : [Number(Platform.Version) >= 29 ? p.ACCESS_FINE_LOCATION : p.ACCESS_COARSE_LOCATION];
  const grants = await PermissionsAndroid.requestMultiple(required);
  if (required.some(permission => grants[permission] !== PermissionsAndroid.RESULTS.GRANTED)) {
    throw new ApiError('Autorize o acesso a dispositivos próximos nas permissões do app. No Android 11 ou anterior, permita localização e ative-a durante a busca.');
  }
}

export async function openBluetooth() {
  await permissions();
  try {
    const { BleManager } = await import('react-native-ble-plx');
    return new BluetoothSettingsClient(new BleManager());
  } catch {
    throw new ApiError('Este cliente Expo precisa ser atualizado com o módulo Bluetooth.');
  }
}

export function bluetoothError(error: unknown) {
  if (error instanceof ApiError) return error;
  if (error && typeof error === 'object') {
    const ble = error as { attErrorCode?: number };
    if (ble.attErrorCode === 5 || ble.attErrorCode === 15) {
      return new ApiError('Confirme o pareamento no sistema e use o PIN atual do Ciborgue.');
    }
  }
  return new ApiError('Falha na comunicação Bluetooth. Confira o pareamento e aproxime-se do Ciborgue.');
}

export class BluetoothSettingsClient {
  private device?: Device;
  private lost?: Subscription;
  private scanTimeout?: ReturnType<typeof setTimeout>;
  private closed = false;
  private scanning = false;

  constructor(private manager: BleManager) {}

  async scan(onDevice: (device: Device) => void, finish: (error?: unknown) => void) {
    const state = await this.manager.state();
    if (this.closed) return;
    if (state !== 'PoweredOn') throw new ApiError(
      state === 'PoweredOff' ? 'Ative o Bluetooth do celular para buscar o robô.' :
      state === 'Unauthorized' ? 'Autorize o Bluetooth nas configurações do app.' : 'Bluetooth indisponível. Tente novamente.');
    this.scanning = true;
    const complete = (error?: unknown) => {
      if (!this.scanning || this.closed) return;
      void this.stopScan().catch(() => {});
      finish(error);
    };
    this.scanTimeout = setTimeout(complete, 8500);
    try {
      await this.manager.startDeviceScan([BLE_SERVICE], null, (error, device) => {
        if (error) complete(error);
        else if (device && !this.closed && this.scanning) onDevice(device);
      });
    } catch (error) {
      complete(error);
    }
  }

  private async stopScan() {
    if (!this.scanning) return;
    this.scanning = false;
    clearTimeout(this.scanTimeout);
    await this.manager.stopDeviceScan();
  }

  async connect(id: string, onLost: () => void) {
    await this.stopScan();
    const device = await this.manager.connectToDevice(id, { timeout: 15000 });
    if (this.closed) {
      await device.cancelConnection().catch(() => {});
      throw new ApiError('A sessão Bluetooth foi encerrada.');
    }
    this.device = device;
    this.lost = device.onDisconnected(() => {
      this.device = undefined;
      this.lost?.remove();
      this.lost = undefined;
      if (!this.closed) onLost();
    });
    try {
      await device.discoverAllServicesAndCharacteristics();
      return await this.read();
    } catch (error) {
      await this.disconnect().catch(() => {});
      throw error;
    }
  }

  async disconnect() {
    const device = this.device;
    this.device = undefined;
    this.lost?.remove();
    this.lost = undefined;
    if (device) await device.cancelConnection();
  }

  private connected() {
    if (!this.device || this.closed) throw new ApiError('Conecte o Ciborgue novamente.');
    return this.device;
  }

  async read() {
    const characteristic = await this.connected().readCharacteristicForService(BLE_SERVICE, BLE_SETTINGS);
    return decodeBleSettings(characteristic.value);
  }

  save(patch: Partial<BleSettings>) {
    return commitBleSettings(() => this.read(), async value => {
      await this.connected().writeCharacteristicWithResponseForService(BLE_SERVICE, BLE_SETTINGS, value);
    }, patch);
  }

  async close() {
    this.closed = true;
    await this.stopScan().catch(() => {});
    await this.disconnect().catch(() => {});
    await this.manager.destroy();
  }
}
