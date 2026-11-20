import { fromByteArray, toByteArray } from 'base64-js';
import { ApiError } from './protocol.ts';

export const BLE_SERVICE = '6c730001-18af-4d4f-9bdc-c034e99e1400';
export const BLE_SETTINGS = '6c730002-18af-4d4f-9bdc-c034e99e1400';
export type BleSettings = { brightness: number; volume: number; mute: boolean; chirps: boolean };

export function decodeBleSettings(value: string | null): BleSettings {
  if (!value || !/^[A-Za-z0-9+/]{7}=$/.test(value)) throw new ApiError('O robô devolveu um pacote Bluetooth inválido.');
  const [version, brightness, volume, mute, chirps] = toByteArray(value);
  if (version !== 1) throw new ApiError('A versão Bluetooth do firmware é incompatível com este app.');
  if (volume > 100 || mute > 1 || chirps > 1) throw new ApiError('O robô devolveu ajustes Bluetooth inválidos.');
  return { brightness, volume, mute: mute === 1, chirps: chirps === 1 };
}

export function encodeBleSettings(value: BleSettings) {
  const { brightness, volume, mute, chirps } = value;
  if (!Number.isInteger(brightness) || brightness < 0 || brightness > 255 ||
      !Number.isInteger(volume) || volume < 0 || volume > 100 ||
      typeof mute !== 'boolean' || typeof chirps !== 'boolean') throw new ApiError('Confira os valores antes de salvar.');
  return fromByteArray(Uint8Array.of(1, brightness, volume, Number(mute), Number(chirps)));
}

export async function commitBleSettings(read: () => Promise<BleSettings>,
  write: (value: string) => Promise<void>, patch: Partial<BleSettings>) {
  const current = await read();
  const fields = Object.keys(patch) as (keyof BleSettings)[];
  const matches = (value: BleSettings) => fields.every(key => value[key] === patch[key]);
  if (matches(current)) return current;
  await write(encodeBleSettings({ ...current, ...patch }));
  for (let attempt = 0; attempt < 7; attempt++) {
    const value = await read();
    if (matches(value)) return value;
    if (attempt < 6) await new Promise(resolve => setTimeout(resolve, 200));
  }
  throw new ApiError('O robô recebeu a escrita, mas ainda não confirmou os ajustes. Releia ou tente salvar novamente.');
}
