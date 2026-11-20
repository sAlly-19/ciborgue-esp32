export type Body = Record<string, string | number | boolean>;
export type Decoder<T> = (value: unknown) => T;
export class ApiError extends Error {}

export function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new ApiError('Resposta inesperada do Ciborgue.');
  return value as Record<string, unknown>;
}
export const str = (v: unknown, fallback = '') => typeof v === 'string' ? v : fallback;
export const num = (v: unknown, fallback = 0) => typeof v === 'number' && Number.isFinite(v) ? v : fallback;
export const bool = (v: unknown) => v === true;
const clamp = (v: unknown, min: number, max: number) => Math.min(max, Math.max(min, Math.trunc(num(v))));

function readDisplay(value: unknown) {
  const display = record(value);
  const rotation = num(display.rotation, -1);
  const brightness = num(display.brightness, -1);
  if (!Number.isInteger(rotation) || rotation < 0 || rotation > 3 ||
      !Number.isInteger(brightness) || brightness < 0 || brightness > 255 ||
      typeof display.inverted !== 'boolean') throw new ApiError('A placa devolveu ajustes de tela inválidos.');
  return { rotation, brightness, inverted: display.inverted };
}
export function displaySettings(value: unknown) {
  ack(value);
  return readDisplay(value);
}

export function status(value: unknown) {
  const s = record(value);
  if (s.ok !== true || typeof s.state !== 'string') throw new ApiError('O endereço não retornou um status do Ciborgue.');
  const capabilities = s.capabilities == null ? {} : record(s.capabilities);
  return {
    revision: num(s.api_revision, 1),
    capabilities: {
      emotions: Array.isArray(capabilities.emotions) ? capabilities.emotions.filter((name): name is string => typeof name === 'string') : null,
      display: bool(capabilities.display), lipSync: bool(capabilities.lip_sync), memoryProposals: bool(capabilities.memory_proposals),
    },
    display: s.display == null ? null : readDisplay(s.display),
    configuredBrightness: Number.isInteger(s.tft_brightness) && num(s.tft_brightness) >= 0 && num(s.tft_brightness) <= 255
      ? num(s.tft_brightness) : null,
    state: s.state, emotion: str(s.emotion, '—'), power: str(s.power_stage, '—'),
    temp: num(s.temp_c, NaN), wifi: bool(s.wifi_ok), ssid: str(s.wifi_ssid), rssi: num(s.wifi_rssi),
    ip: str(s.ip), hasKey: bool(s.has_key), city: str(s.city),
    heap: num(s.heap_kb, NaN), psram: num(s.psram_kb, NaN), uptime: num(s.uptime_s, NaN),
    rgb: [clamp(s.led_r, 0, 255), clamp(s.led_g, 0, 255), clamp(s.led_b, 0, 255)],
    timer: clamp(s.timer_rem_s, 0, 86400), timerTotal: clamp(s.timer_total_s, 0, 86400), timerLabel: str(s.timer_label),
    pomo: bool(s.pomo_active), pomoRemaining: clamp(s.pomo_rem_s, 0, 10800), goal: str(s.pomo_goal),
    network: str(s.net_mode, '—'), portal: bool(s.portal_active), apIp: str(s.ap_ip), clients: num(s.ap_clients),
    volume: clamp(s.sfx_vol ?? 75, 0, 100), mute: bool(s.sfx_mute), chirps: bool(s.sfx_idle), fast: bool(s.fast_path),
    clock: bool(s.desk_clock), ringing: bool(s.alarm_ringing), alarmCount: num(s.alarms_count),
    affection: num(s.affection, NaN), energy: num(s.energy, NaN), mood: num(s.mood, NaN), moodLabel: str(s.mood_label, '—'),
  };
}
export type Status = ReturnType<typeof status>;
export function ack(value: unknown) {
  const data = record(value);
  if (data.ok !== true) throw new ApiError('O Ciborgue não confirmou a solicitação.');
  return str(data.message);
}
export function reply(value: unknown) {
  ack(value);
  const text = str(record(value).reply).trim();
  if (!text) throw new ApiError('O Ciborgue devolveu uma resposta vazia.');
  return text;
}
export type MemoryProposal = { id: string; fact: string };
export function chatReply(value: unknown) {
  const text = reply(value);
  const raw = record(value).memory_proposal;
  if (raw == null) return { text, proposal: raw === null ? null : undefined };
  const proposal = record(raw);
  const id = str(proposal.id);
  const fact = str(proposal.fact).trim();
  if (!/^[0-9a-f]{16}$/.test(id) || !fact) throw new ApiError('A placa devolveu uma proposta de memória inválida.');
  return { text, proposal: { id, fact } as MemoryProposal };
}
export function proposalResolution(value: unknown) {
  const message = ack(value);
  const { resolved } = record(value);
  if (typeof resolved !== 'boolean') throw new ApiError('A placa não confirmou o estado da proposta.');
  return { resolved, message };
}
function list<T>(value: unknown, decode: Decoder<T>) {
  if (!Array.isArray(value)) throw new ApiError('O Ciborgue devolveu uma lista inválida.');
  return value.map(decode);
}
export function integer(value: string, min: number, max: number, label: string) {
  const n = Number(value);
  if (!/^\d+$/.test(value.trim()) || !Number.isInteger(n) || n < min || n > max) throw new ApiError(`${label} deve ser um inteiro de ${min} a ${max}.`);
  return n;
}
function idOf(value: unknown) {
  const id = num(value);
  if (!Number.isSafeInteger(id) || id < 1) throw new ApiError('A lista contém um identificador inválido.');
  return id;
}
export const tasks = (value: unknown) => list(value, item => {
  const t = record(item);
  return { id: idOf(t.id), title: str(t.title, 'Sem título'), done: bool(t.done) };
});
export const memories = (value: unknown) => list(value, item => {
  if (typeof item !== 'string') throw new ApiError('A lista contém uma memória inválida.');
  return item;
});
export const alarms = (value: unknown) => list(value, item => {
  const a = record(item);
  return { id: idOf(a.id), hour: clamp(a.hour, 0, 23), minute: clamp(a.minute, 0, 59),
    daysMask: clamp(a.daysMask ?? 127, 0, 127), label: str(a.label, 'Despertador'), enabled: bool(a.enabled) };
});
export function personality(value: unknown) {
  const p = record(value);
  return { affection: num(p.affection, NaN), energy: num(p.energy, NaN), mood: num(p.mood, NaN),
    label: str(p.label, '—'), pets: num(p.pet_count, NaN), chats: num(p.chat_count, NaN) };
}
