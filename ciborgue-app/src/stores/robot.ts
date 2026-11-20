import AsyncStorage from '@react-native-async-storage/async-storage';
import NetInfo, { type NetInfoState } from '@react-native-community/netinfo';
import { useSyncExternalStore } from 'react';
import { AppState, Platform } from 'react-native';
import { normalizeHost, request } from '../api/client';
import { ApiError, displaySettings, status, type Body, type Decoder, type Status } from '../api/protocol';

const key = 'ciborgue.host';
const defaultHost = 'http://ciborgue.local';
function initialHost() {
  try { return normalizeHost(process.env.EXPO_PUBLIC_CIBORGUE_HOST ?? defaultHost); }
  catch { return defaultHost; }
}

type Connection = {
  host: string;
  online: boolean;
  syncing: boolean;
  ready: boolean;
  error: string;
  wifi: boolean | null;
  busy: boolean;
};
let connection: Connection = { host: initialHost(), online: false, syncing: false, ready: false, error: '', wifi: null, busy: false };
let telemetry: { data: Status; receivedAt: number; timerFinished: string } | null = null;
let timerLabel: string | undefined;
const listeners = new Set<() => void>();
let poll: { controller: AbortController; promise: Promise<void> } | undefined;
let started = false;
let foreground = true;
let generation = 0;
let testRequest: AbortController | undefined;

function subscribe(listener: () => void) {
  listeners.add(listener);
  return () => { listeners.delete(listener); };
}
function publish() { listeners.forEach(listener => listener()); }
function update(fields: Partial<Connection>) {
  if (Object.entries(fields).every(([field, value]) => connection[field as keyof Connection] === value)) return false;
  connection = { ...connection, ...fields };
  publish();
  return true;
}
function accept(data: Status) {
  let timerFinished = telemetry?.timerFinished ?? '';
  if (data.timerTotal) {
    timerLabel = data.timerLabel || 'Timer';
    timerFinished = '';
  } else if (timerLabel) {
    if (data.state === 'ALERT' && !data.ringing) timerFinished = timerLabel;
    timerLabel = undefined;
  }
  telemetry = { data, receivedAt: Date.now(), timerFinished };
  if (!update({ online: true, error: '' })) publish();
}
export function trackTimer(label?: string) { timerLabel = label; }
export const useConnection = () => useSyncExternalStore(subscribe, () => connection, () => connection);
export const useConnectionValue = <T,>(select: (data: Connection) => T) =>
  useSyncExternalStore(subscribe, () => select(connection), () => select(connection));
export const useRobot = <T,>(select: (data: Status | undefined) => T) =>
  useSyncExternalStore(subscribe, () => select(telemetry?.data), () => select(undefined));
export const useTelemetry = () => useSyncExternalStore(subscribe, () => telemetry, () => null);
export const errorText = (error: unknown) => error instanceof ApiError ? error.message : 'Não foi possível concluir a operação.';

function checkWifi() {
  if (connection.wifi === false) throw new ApiError('Conecte o celular ao Wi-Fi do Ciborgue.');
}
export function refresh(): Promise<void> {
  if (poll) return poll.promise;
  if (!started || !foreground || !connection.ready || connection.busy || connection.wifi === false) return Promise.resolve();
  const current = generation;
  const controller = new AbortController();
  const host = connection.host;
  update({ syncing: true });
  const promise = request(host, 'status', status, undefined, { signal: controller.signal })
    .then(data => { if (current === generation && !controller.signal.aborted) accept(data); })
    .catch(error => {
      if (current === generation && !controller.signal.aborted) update({ online: false, error: errorText(error) });
    }).finally(() => {
      if (poll?.controller === controller) {
        poll = undefined;
        update({ syncing: false });
      }
    });
  poll = { controller, promise };
  return promise;
}

function network(state: NetInfoState) {
  const wifi = state.isConnected === false ? false : Platform.OS === 'android' && state.type === 'cellular' ? false
    : state.type === 'wifi' ? state.isConnected : null;
  const returned = connection.wifi === false && wifi !== false;
  update({ wifi });
  if (wifi === false) {
    poll?.controller.abort();
    update({ online: false, error: 'Conecte o celular ao Wi-Fi do Ciborgue.' });
  } else if (returned) void refresh();
}

export function startRobot() {
  started = true;
  foreground = AppState.currentState !== 'background';
  let active = true;
  void AsyncStorage.getItem(key).then(saved => {
    if (saved && active) update({ host: normalizeHost(saved) });
  }).catch(() => {}).finally(() => {
    if (active) { update({ ready: true }); void refresh(); }
  });
  const unsubscribe = NetInfo.addEventListener(network);
  const subscription = AppState.addEventListener('change', state => {
    foreground = state === 'active';
    if (foreground) void refresh();
    else poll?.controller.abort();
  });
  const interval = setInterval(() => { void refresh(); }, 4500);
  return () => {
    active = false;
    started = false;
    clearInterval(interval);
    unsubscribe();
    subscription.remove();
    poll?.controller.abort();
    testRequest?.abort();
  };
}

export async function connect(input: string) {
  const host = normalizeHost(input);
  checkWifi();
  if (connection.busy) throw new ApiError('Aguarde a operação atual.');
  const controller = new AbortController();
  testRequest = controller;
  update({ busy: true });
  try {
    const data = await request(host, 'status', status, undefined, { signal: controller.signal });
    try { await AsyncStorage.setItem(key, host); }
    catch { throw new ApiError('Conexão confirmada, mas o Android não salvou o endereço.'); }
    generation++;
    poll?.controller.abort();
    if (host !== connection.host) {
      telemetry = null;
      timerLabel = undefined;
    }
    update({ host });
    accept(data);
  } finally {
    testRequest = undefined;
    update({ busy: false });
  }
}

export async function read<T>(path: string, decode: Decoder<T>, signal?: AbortSignal) {
  checkWifi();
  return request(connection.host, path, decode, undefined, { signal });
}

export async function mutate<T>(path: string, body: Body, decode: Decoder<T>, timeout?: number) {
  checkWifi();
  if (connection.busy) throw new ApiError('Aguarde a operação atual.');
  if (!connection.online) throw new ApiError('O Ciborgue está offline.');
  update({ busy: true });
  try {
    const pending = poll;
    if (pending) {
      pending.controller.abort();
      await pending.promise;
    }
    return await request(connection.host, path, decode, body, { timeout });
  } finally {
    update({ busy: false });
    void refresh();
  }
}

export const send = (path: string, body: Body = {}) => mutate(path, body, value => {
  const data = status(value);
  accept(data);
  return data;
});

export const setDisplay = (change: Partial<NonNullable<Status['display']>>) => mutate('display', change, value => {
  const display = displaySettings(value);
  if (telemetry) {
    telemetry = { ...telemetry, data: { ...telemetry.data, display } };
    publish();
  }
  return display;
});
