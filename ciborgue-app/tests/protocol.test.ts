import assert from 'node:assert/strict';
import { afterEach, test } from 'node:test';
import { normalizeHost, request } from '../src/api/client.ts';
import { ack, alarms, chatReply, displaySettings, integer, memories, proposalResolution, reply, status, tasks } from '../src/api/protocol.ts';
import { knownCommand, lipSyncCommand } from '../src/utils/command.ts';
import { commitBleSettings, decodeBleSettings, encodeBleSettings } from '../src/api/ble-protocol.ts';
import { duration } from '../src/utils/format.ts';

const original = globalThis.fetch;
afterEach(() => { globalThis.fetch = original; });

test('o pacote BLE v1 usa cinco bytes e valida limites e versão', () => {
  const settings = { brightness: 255, volume: 100, mute: true, chirps: false };
  assert.deepEqual([...Buffer.from(encodeBleSettings(settings), 'base64')], [1, 255, 100, 1, 0]);
  assert.deepEqual(decodeBleSettings(encodeBleSettings(settings)), settings);
  assert.throws(() => encodeBleSettings({ ...settings, volume: 101 }));
  for (const packet of [[2, 80, 40, 0, 1], [1, 80, 40, 2, 1], [1, 80, 40]]) {
    assert.throws(() => decodeBleSettings(Buffer.from(packet).toString('base64')));
  }
});

test('a escrita BLE preserva ajustes não editados e aguarda a confirmação por leitura', async () => {
  const current = { brightness: 128, volume: 35, mute: false, chirps: true };
  const saved = { ...current, mute: true };
  const reads = [current, current, saved];
  const writes: string[] = [];
  const result = await commitBleSettings(async () => reads.shift()!, async value => { writes.push(value); }, { mute: true });
  assert.equal(writes.length, 1);
  assert.deepEqual(decodeBleSettings(writes[0]), saved);
  assert.deepEqual(result, saved);
});

test('a placa antiga tem fallback e a revisão nova informa faces e valores de tela', () => {
  const old = status({ ok: true, state: 'IDLE' });
  assert.equal(old.capabilities.emotions, null);
  assert.equal(old.display, null);
  const display = { rotation: 3, inverted: true, brightness: 35 };
  const updated = status({ ok: true, state: 'IDLE', api_revision: 3, display,
    capabilities: { display: true, lip_sync: true, memory_proposals: true, emotions: ['NEUTRAL', 'CURIOUS', 'BORED', 'SILENT', 'ANGEL', 'COOL', 'CRAZY', 'LIAR'] } });
  assert.deepEqual(updated.display, display);
  assert.equal(updated.capabilities.emotions?.at(-1), 'LIAR');
  assert.deepEqual(displaySettings({ ok: true, ...display }), display);
  assert.throws(() => displaySettings({ ok: true, ...display, brightness: 256 }));
});

test('a proposta distingue respostas antigas, pendências e resolução expirada', () => {
  assert.equal(chatReply({ ok: true, reply: 'Olá' }).proposal, undefined);
  assert.equal(chatReply({ ok: true, reply: 'Olá', memory_proposal: null }).proposal, null);
  const proposal = { id: '0123456789abcdef', fact: 'Prefere chá' };
  assert.deepEqual(chatReply({ ok: true, reply: 'Confirmar?', memory_proposal: proposal }).proposal, proposal);
  assert.throws(() => chatReply({ ok: true, reply: 'Confirmar?', memory_proposal: { ...proposal, id: 'antigo' } }));
  assert.deepEqual(proposalResolution({ ok: true, resolved: false, message: 'Expirou' }), { resolved: false, message: 'Expirou' });
});

test('sincronização labial usa formato aceito pelo firmware e valida limites', () => {
  assert.equal(lipSyncCommand('0,5', '1'), 'CMD:LIPSYNC=0.500000,1.000000');
  assert.ok(knownCommand('CMD:LIPSYNC=0,1'));
  assert.ok(!knownCommand('CMD:LIPSYNC=0,1,2'));
  assert.throws(() => lipSyncCommand('', '1'));
  assert.throws(() => lipSyncCommand('0', '1.01'));
});

test('aceita mDNS e IPv4 e rejeita URLs que mudariam o destino', () => {
  assert.equal(normalizeHost(' HTTP://Ciborgue.Local:80/ '), 'http://ciborgue.local');
  assert.equal(normalizeHost('192.168.4.1'), 'http://192.168.4.1');
  for (const host of ['https://ciborgue.local', '256.0.0.1', 'x.local:81', 'x.local/api', 'abc@ciborgue.local']) {
    assert.throws(() => normalizeHost(host));
  }
});

test('valida status e usa valores seguros para telemetria incompleta', () => {
  const data = status({ ok: true, state: 'IDLE', timer_total_s: 200000, sfx_mute: 'false', led_b: 300 });
  assert.equal(data.timerTotal, 86400);
  assert.equal(data.mute, false);
  assert.equal(data.rgb[2], 255);
  assert.ok(Number.isNaN(data.temp));
  assert.throws(() => status({ ok: true }));
});

test('decodifica listas reais e preserva alarme único e desativado', () => {
  assert.deepEqual(tasks([{ id: 1, title: 'Teste', done: true }]), [{ id: 1, title: 'Teste', done: true }]);
  assert.deepEqual(memories(['uma memória']), ['uma memória']);
  assert.deepEqual(alarms([{ id: 1, hour: 9, minute: 5, enabled: false, daysMask: 0 }]),
    [{ id: 1, hour: 9, minute: 5, enabled: false, daysMask: 0, label: 'Despertador' }]);
  assert.throws(() => tasks([{ id: '1' }]));
  assert.throws(() => memories({ memories: [] }));
});

test('valida limites numéricos, duração e comandos parametrizados', () => {
  assert.equal(integer('086', 1, 86400, 'Timer'), 86);
  assert.throws(() => integer('1e2', 1, 86400, 'Timer'));
  assert.throws(() => integer('86401', 1, 86400, 'Timer'));
  assert.equal(duration(86400), '24:00:00');
  assert.equal(duration(-4), '00:00:00');
  assert.equal(knownCommand('CMD:TFT_BRI=255'), true);
  assert.equal(knownCommand('RGB:0,80,256'), false);
  assert.equal(knownCommand('STATE:PORTAL'), true);
  assert.equal(knownCommand('EMOTION:MALICE'), true);
});

test('envia GET e POST em JSON e usa o decodificador', async () => {
  globalThis.fetch = async (url, init) => {
    assert.equal(String(url), 'http://ciborgue.local/api/chat');
    assert.equal(init?.method, 'POST');
    assert.equal(new Headers(init?.headers).get('content-type'), 'application/json');
    assert.deepEqual(JSON.parse(String(init?.body)), { message: 'Olá' });
    return Response.json({ ok: true, reply: 'Olá!' });
  };
  assert.equal(await request('http://ciborgue.local', 'chat', reply, { message: 'Olá' }), 'Olá!');
  globalThis.fetch = async (_, init) => {
    assert.equal(init?.method, 'GET');
    assert.equal(init?.body, undefined);
    return Response.json({ ok: true, state: 'IDLE' });
  };
  assert.equal((await request('http://ciborgue.local', 'status', status)).state, 'IDLE');
});

test('trata HTTP, recusa, JSON inválido e timeout', async () => {
  globalThis.fetch = async () => new Response('offline', { status: 503 });
  await assert.rejects(request('http://ciborgue.local', 'status', status), /HTTP 503/);
  globalThis.fetch = async () => new Response('<html>');
  await assert.rejects(request('http://ciborgue.local', 'status', status), /JSON inválido/);
  globalThis.fetch = async () => Response.json({ ok: false });
  await assert.rejects(request('http://ciborgue.local', 'portal', ack, {}), /recusou/);
  globalThis.fetch = (_, init) => new Promise((_, reject) => {
    init?.signal?.addEventListener('abort', () => reject(new Error('aborted')));
  });
  await assert.rejects(request('http://ciborgue.local', 'status', status, undefined, { timeout: 5 }), /demorou/);
});
