import { useFocusEffect, useRouter } from 'expo-router';
import { useCallback, useRef, useState } from 'react';
import { Linking, Platform } from 'react-native';
import type { Device } from 'react-native-ble-plx';
import { bluetoothError, openBluetooth, type BluetoothSettingsClient } from '../api/bluetooth';
import type { BleSettings } from '../api/ble-protocol';
import { Range } from '../components/range';
import { Action, Card, Notice, Row, Screen, Toggle, Txt, useAction } from '../components/ui';

export default function BluetoothScreen() {
  const router = useRouter();
  const transport = useRef<BluetoothSettingsClient | undefined>(undefined);
  const generation = useRef(0);
  const searching = useRef(false);
  const [devices, setDevices] = useState<Device[]>([]);
  const [scan, setScan] = useState<'idle' | 'searching' | 'finished'>('idle');
  const [scanError, setScanError] = useState('');
  const [device, setDevice] = useState<Device>();
  const [confirmed, setConfirmed] = useState<BleSettings>();
  const [draft, setDraft] = useState<Partial<BleSettings>>({});
  const a = useAction();

  useFocusEffect(useCallback(() => {
    return () => {
      generation.current++;
      searching.current = false;
      const current = transport.current;
      transport.current = undefined;
      if (current) void current.close().catch(() => {});
      setScan('idle');
      setScanError('');
      setDevices([]);
      setDevice(undefined);
      setConfirmed(undefined);
      setDraft({});
    };
  }, []));

  async function withClient<T>(job: (client: BluetoothSettingsClient) => Promise<T>) {
    const started = generation.current;
    try {
      const client = transport.current ?? await openBluetooth();
      if (started !== generation.current) {
        await client.close().catch(() => {});
        return undefined;
      }
      transport.current = client;
      const result = await job(client);
      return started === generation.current ? result : undefined;
    } catch (error) {
      if (started === generation.current) throw bluetoothError(error);
      return undefined;
    }
  }

  function find() {
    if (searching.current) return;
    searching.current = true;
    setScanError('');
    setDevices([]);
    setScan('searching');
    void withClient(client => client.scan(found => {
      if (client !== transport.current) return;
      setDevices(items => items.some(item => item.id === found.id) ? items : [...items, found]);
    }, error => {
      if (client !== transport.current) return;
      searching.current = false;
      if (error) setScanError(bluetoothError(error).message);
      setScan('finished');
    })).catch(error => {
      searching.current = false;
      setScanError(bluetoothError(error).message);
      setScan('finished');
    });
  }

  function connect(item: Device) {
    const started = generation.current;
    setScan('idle');
    searching.current = false;
    setScanError('');
    void a.run(async () => {
      const settings = await withClient(client => client.connect(item.id, () => {
        if (client !== transport.current) return;
        setDevice(undefined);
        setConfirmed(undefined);
        setScanError('Conexão Bluetooth perdida. Busque o Ciborgue para reconectar.');
      }));
      if (started !== generation.current || !settings) return '';
      setDevice(item);
      setConfirmed(settings);
      setDraft({});
    }, 'Ciborgue conectado por Bluetooth.');
  }

  const values = confirmed && { ...confirmed, ...draft };
  const dirty = !!confirmed && (Object.keys(draft) as (keyof BleSettings)[]).some(key => draft[key] !== confirmed[key]);
  const edit = <K extends keyof BleSettings>(key: K, value: BleSettings[K]) => setDraft(previous => ({ ...previous, [key]: value }));

  return <Screen title="Bluetooth" showConnection={false} onBack={() => router.navigate('/configuracoes')} backLabel="Voltar aos ajustes"
    status={{
      label: device ? 'Conectado' : scan === 'searching' ? 'Buscando' : a.pending ? 'Conectando' : 'Sem conexão',
      tone: device ? 'active' : scanError ? 'error' : 'idle', icon: 'bluetooth-outline',
    }}>
    <Notice text={a.notice} failed={a.failed} />
    <Notice text={scanError} failed />
    {Platform.OS === 'web' ? <Card title="Bluetooth do Ciborgue"><Txt>Abra o app nativo no celular para configurar o robô por Bluetooth.</Txt></Card>
      : !device || !values ? <Card title="Ciborgue por perto">
        <Txt muted>Se o Android pedir um PIN, consulte os seis dígitos atuais na serial do robô com CMD:BLE_PIN e informe na janela do sistema.</Txt>
        <Row>
          <Action title={scan === 'searching' ? 'Buscando…' : 'Buscar Ciborgue'} icon="bluetooth-outline" active
            pending={scan === 'searching'} disabled={a.pending} onPress={find} />
          {!!scanError && <Action title="Permissões do celular" onPress={() => void Linking.openSettings().catch(() => {})} />}
        </Row>
        {scan === 'finished' && !scanError && !devices.length && <Txt muted>Nenhum Ciborgue encontrado. Confirme se o Bluetooth está habilitado no robô e tente novamente.</Txt>}
        {devices.map(item => <Action key={item.id} title={`${item.localName ?? item.name ?? 'Ciborgue'} · ${item.id.slice(-8)}`}
          icon="bluetooth-outline" disabled={a.pending} onPress={() => connect(item)} />)}
        {a.pending && <Txt muted>Aguardando a conexão e o pareamento do sistema…</Txt>}
      </Card> : <Card title="Conectado por Bluetooth">
        <Txt muted>{device.localName ?? device.name ?? 'Ciborgue'}</Txt>
        <Txt>Brilho · {values.brightness}/255</Txt>
        <Range accessibilityLabel="Brilho da tela" minimumValue={0} maximumValue={255} step={1} value={values.brightness}
          disabled={a.pending} style={{ height: 44 }} onSlidingComplete={value => edit('brightness', value)} />
        <Txt>Volume · {values.volume}%</Txt>
        <Range accessibilityLabel="Volume" minimumValue={0} maximumValue={100} step={1} value={values.volume}
          disabled={a.pending} style={{ height: 44 }} onSlidingComplete={value => edit('volume', value)} />
        <Toggle label="Mudo" value={values.mute} disabled={a.pending} onChange={value => edit('mute', value)} />
        <Toggle label="Chirps ociosos" value={values.chirps} disabled={a.pending} onChange={value => edit('chirps', value)} />
        <Row>
          <Action title="Salvar no robô" active disabled={!dirty} pending={a.pending} onPress={() => void a.run(async () => {
            const patch = Object.fromEntries(Object.entries(draft).filter(([key, value]) => value !== confirmed?.[key as keyof BleSettings]));
            const settings = await withClient(client => client.save(patch));
            if (settings) { setConfirmed(settings); setDraft({}); }
          }, 'Ajustes Bluetooth confirmados no Ciborgue.')} />
          <Action title="Reler valores" disabled={a.pending} onPress={() => void a.run(async () => {
            const settings = await withClient(client => client.read());
            if (settings) setConfirmed(settings);
          }, 'Leitura Bluetooth atualizada.')} />
          {dirty && <Action title="Descartar mudanças" disabled={a.pending} onPress={() => setDraft({})} />}
          <Action title="Desconectar" disabled={a.pending} onPress={() => void a.run(async () => {
            await withClient(client => client.disconnect());
            setDevice(undefined);
            setConfirmed(undefined);
            setDraft({});
          }, 'Bluetooth desconectado.')} />
        </Row>
      </Card>}
  </Screen>;
}
