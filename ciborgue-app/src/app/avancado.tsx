import { useConfirm } from '../components/feedback-context';
import { useRouter } from 'expo-router';
import { useState } from 'react';
import { ApiError } from '../api/protocol';
import { Action, Card, Field, Notice, Row, Screen, Txt, useAction, useDisabled } from '../components/ui';
import { fixedCommands } from '../constants/robot';
import { send, useRobot } from '../stores/robot';
import { knownCommand, lipSyncCommand } from '../utils/command';

export default function Advanced() {
  const confirm = useConfirm();
  const router = useRouter();
  const [text, setText] = useState('');
  const [amplitude, setAmplitude] = useState('0,5');
  const [crossings, setCrossings] = useState('0,2');
  const display = useRobot(s => s && s.revision >= 3 ? s.capabilities.display : undefined);
  const lipSync = useRobot(s => s?.capabilities?.lipSync);
  const disabled = useDisabled();
  const a = useAction();
  const cmd = text.trim();
  function submit() {
    const run = () => void a.run(async () => {
      if (cmd.startsWith('CMD:LIPSYNC=') && (!lipSync || !knownCommand(cmd))) {
        throw new ApiError('Confira os valores e o suporte à sincronização labial.');
      }
      const result = await send('command', { cmd });
      return `Status após envio: ${result.state} · ${result.emotion} · ${result.power}`;
    });
    if (cmd === 'PING' || cmd === 'CMD:GET_STATUS') run();
    else confirm(knownCommand(cmd) ? 'Executar comando' : 'Comando não reconhecido no app',
      `Enviar ${cmd} ao firmware? O comando pode alterar dados, a tela ou a conexão.`, run);
  }
  return <Screen title="Avançado" onBack={() => router.navigate('/configuracoes')} backLabel="Voltar aos ajustes">
    <Notice text={a.notice} failed={a.failed} />
    <Card title="Diagnóstico de tela">
      <Action title="Teste RGB" icon="color-palette-outline" disabled={disabled || display === false}
        onPress={() => confirm('Teste RGB', 'O teste ocupa a tela do robô por um instante.', () =>
          void a.run(() => send('command', { cmd: 'CMD:TFT_TEST' }), 'Comando de teste RGB enviado.'))} />
      {lipSync && <>
        <Row>
          <Field label="Amplitude (0 a 1)" value={amplitude} onChangeText={setAmplitude} keyboardType="decimal-pad" />
          <Field label="Cruzamentos por zero (0 a 1)" value={crossings} onChangeText={setCrossings} keyboardType="decimal-pad" />
        </Row>
        <Action title="Testar sincronização labial" disabled={disabled} pending={a.pending}
          onPress={() => void a.run(() => send('command', { cmd: lipSyncCommand(amplitude, crossings) }), 'Comando labial enviado.')} />
      </>}
    </Card>
    <Card title="Comando do firmware">
      <Field label="Comando" value={text} onChangeText={setText} autoCapitalize="characters" maxLength={160} placeholder="CMD:GET_STATUS" />
      <Action title="Enviar comando" disabled={disabled || !cmd} onPress={submit} active />
      <Txt muted>O servidor retorna o status do robô. Detalhes de diagnóstico como AP_DEBUG aparecem na serial.</Txt>
      <Row>{[...fixedCommands, 'CMD:TFT_ROT=0', 'CMD:TFT_INV=1', 'CMD:TFT_BRI=255', 'RGB:0,80,255', 'EMOTION:HAPPY', 'STATE:IDLE'].map(value =>
        <Action key={value} title={value} onPress={() => setText(value)} />)}</Row>
    </Card>
  </Screen>;
}
