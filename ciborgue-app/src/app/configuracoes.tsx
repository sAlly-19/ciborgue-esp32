import { useConfirm } from '../components/feedback-context';
import { useRouter } from 'expo-router';
import { useState } from 'react';
import { Linking } from 'react-native';
import { ApiError } from '../api/protocol';
import { AudioControls } from '../components/robot-controls';
import { DisplayControls } from '../components/display-controls';
import { themes, useTheme } from '../components/theme';
import { Action, Card, Choices, Field, Grid, Notice, Row, Screen, Toggle, Txt, useAction, useDisabled } from '../components/ui';
import { connect, send, useConnection, useRobot } from '../stores/robot';
import { metric } from '../utils/format';

export type SettingsCategory = 'todas' | 'rede' | 'dispositivo' | 'sistema';

const categories: Record<SettingsCategory, string> = {
  todas: 'Todas',
  rede: 'Rede',
  dispositivo: 'Dispositivo',
  sistema: 'Sistema',
};

export default function Settings() {
  const confirm = useConfirm();
  const router = useRouter();
  const { host, busy, ready } = useConnection();
  const s = useRobot(s => s);
  const disabled = useDisabled();
  const a = useAction();
  const [category, setCategory] = useState<SettingsCategory>('todas');
  const [address, setAddress] = useState<string>();
  const [city, setCity] = useState<string>();
  const theme = useTheme();
  const [ssid, setSsid] = useState('');
  const [password, setPassword] = useState('');
  const [key, setKey] = useState('');
  function test(input: string) {
    void a.run(async () => { await connect(input); setAddress(undefined); }, 'Conexão confirmada e endereço salvo.');
  }

  const showNet = category === 'todas' || category === 'rede';
  const showDev = category === 'todas' || category === 'dispositivo';
  const showSys = category === 'todas' || category === 'sistema';

  return <Screen title="Ajustes" icon="settings-outline">
    <Notice text={a.notice} failed={a.failed} />
    <Choices<SettingsCategory> items={categories} selected={category} onSelect={setCategory} />
    <Grid>
      {showNet && <Card title="Endereço do Ciborgue">
        <Field label="IPv4 ou endereço .local" value={address ?? host} onChangeText={setAddress} autoCapitalize="none" keyboardType="url" />
        <Row>
          <Action title="Testar e salvar" disabled={busy || !ready} active onPress={() => test(address ?? host)} />
          <Action title="Usar ciborgue.local" disabled={busy || !ready} onPress={() => test('ciborgue.local')} />
        </Row>
        <Txt>Rede {s?.network ?? '—'} · {s?.ssid || 'SSID indisponível'}</Txt>
        <Txt muted>{s?.ip || 'IP indisponível'} · {s?.wifi ? metric(s.rssi, ' dBm') : 'RSSI indisponível'}</Txt>
      </Card>}
      {showNet && <Card title="Wi-Fi do robô">
        <Field label="Novo SSID" value={ssid} onChangeText={setSsid} autoCapitalize="none" />
        <Field label="Nova senha (vazia para rede aberta)" value={password} onChangeText={setPassword} autoCapitalize="none" secureTextEntry autoComplete="off" />
        <Action title="Salvar credenciais" disabled={disabled || !ssid.trim()} onPress={() => confirm('Alterar Wi-Fi', 'As credenciais serão enviadas por HTTP. O endereço do robô pode mudar depois de reconectar.', () => void a.run(async () => {
          await send('config', { wifi_ssid: ssid.trim(), wifi_password: password });
          setPassword('');
          setSsid('');
        }, 'Credenciais gravadas no robô. Confira o IP após reconectar.'))} />
      </Card>}
      {showNet && <Card title="Portal cativo">
        <Txt>{s?.portal ? 'Aberto' : 'Fechado'} · {s?.clients ?? '—'} cliente(s)</Txt>
        <Txt muted>Com o portal aberto, conecte-se ao Wi-Fi Ciborgue-Setup e acesse {s?.apIp || '192.168.4.1'} no navegador.</Txt>
        <Row>
          <Action title="Abrir portal" disabled={disabled || s?.portal} onPress={() => void a.run(() => send('portal', { action: 'start' }))} />
          <Action title="Parar portal" disabled={disabled || !s?.portal} onPress={() => confirm('Parar portal', 'A conexão com o ponto de acesso será fechada.', () => void a.run(() => send('portal', { action: 'stop' })))} />
          <Action title="Abrir no navegador" disabled={!s?.portal} onPress={() => void a.run(() => Linking.openURL('http://192.168.4.1'), 'Portal aberto no navegador.')} />
        </Row>
      </Card>}
      {showNet && <Card title="Bluetooth">
        <Txt muted>Ajuste brilho e áudio diretamente pelo celular.</Txt>
        <Action title="Abrir Bluetooth" icon="bluetooth-outline" onPress={() => router.navigate('/bluetooth')} />
      </Card>}

      {showDev && <Card title="Tela"><DisplayControls /></Card>}
      {showDev && <Card title="Áudio"><AudioControls /></Card>}
      {showDev && <Card title="Preferências do robô">
        <Field label="Cidade padrão" value={city ?? s?.city ?? ''} onChangeText={setCity} />
        <Action title="Salvar cidade" disabled={disabled || !city?.trim()} onPress={() => void a.run(async () => {
          await send('config', { city: city?.trim() ?? '' });
          setCity(undefined);
        })} />
        <Toggle label="Fast path" value={s?.fast ?? false} disabled={disabled} onChange={fast_path => void a.run(() => send('config', { fast_path }))} />
        <Txt muted>{s?.hasKey ? 'Chave Gemini cadastrada.' : 'Chave Gemini não detectada.'}</Txt>
        <Txt muted>O firmware recebe credenciais por HTTP. Outros dispositivos na rede podem ler esse tráfego. Use uma rede de confiança.</Txt>
        <Field label="Nova chave Gemini" value={key} onChangeText={setKey} autoCapitalize="none" secureTextEntry autoComplete="off" />
        <Action title="Salvar chave" disabled={disabled || !key.trim()} onPress={() => {
          const value = key.trim();
          if (value.length <= 10) {
            void a.run(async () => { throw new ApiError('A chave Gemini deve ter mais de 10 caracteres.'); });
            return;
          }
          confirm('Salvar chave Gemini', 'Enviar a chave ao robô por HTTP nesta rede?', () => void a.run(async () => {
            await send('config', { gemini_key: value });
            setKey('');
          }));
        }} />
      </Card>}

      {showSys && theme && <Card title="Aparência">
        <Choices<keyof typeof themes> items={themes} selected={theme.preference} disabled={a.pending} onSelect={value => void a.run(async () => {
          await theme.choose(value);
        }, 'Tema salvo neste celular.')} />
      </Card>}
      {showSys && <Card title="Manutenção">
        <Action title="Avançado" icon="terminal-outline" onPress={() => router.navigate('/avancado')} />
      </Card>}
    </Grid>
  </Screen>;
}
