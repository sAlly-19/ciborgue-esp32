import { emotions, fixedCommands, states } from '../constants/robot.ts';
import { ApiError } from '../api/protocol.ts';

function unitInterval(value: string) {
  const normalized = value.trim().replace(',', '.');
  if (!/^(?:\d+(?:\.\d+)?|\.\d+)$/.test(normalized)) return null;
  const number = Number(normalized);
  return number >= 0 && number <= 1 ? number : null;
}

export function lipSyncCommand(amplitude: string, crossings: string) {
  const amp = unitInterval(amplitude);
  const zcr = unitInterval(crossings);
  if (amp === null || zcr === null) throw new ApiError('Amplitude e cruzamento por zero devem estar entre 0 e 1.');
  return `CMD:LIPSYNC=${amp.toFixed(6)},${zcr.toFixed(6)}`;
}

export function knownCommand(cmd: string) {
  if (fixedCommands.includes(cmd) || /^CMD:TFT_(ROT=[0-3]|INV=[01])$/.test(cmd)) return true;
  if (cmd.startsWith('EMOTION:')) return Object.hasOwn(emotions, cmd.slice(8));
  if (cmd.startsWith('STATE:')) return Object.hasOwn(states, cmd.slice(6));
  if (/^CMD:TFT_BRI=\d{1,3}$/.test(cmd)) return Number(cmd.slice(12)) <= 255;
  if (/^RGB:\d{1,3},\d{1,3},\d{1,3}$/.test(cmd)) return cmd.slice(4).split(',').every(v => Number(v) <= 255);
  if (cmd.startsWith('CMD:LIPSYNC=')) {
    const values = cmd.slice('CMD:LIPSYNC='.length).split(',');
    return values.length === 2 && values.every(value => unitInterval(value) !== null);
  }
  return false;
}
