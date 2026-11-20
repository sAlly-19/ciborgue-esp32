import { useConfirm } from '../components/feedback-context';
import { useState } from 'react';
import { ActivityIndicator, View } from 'react-native';
import { ack, ApiError, chatReply, proposalResolution, type MemoryProposal } from '../api/protocol';
import { Action, Card, Field, Notice, Row, Screen, Txt, useAction, useDisabled, usePalette } from '../components/ui';
import { mutate, useConnectionValue, useRobot } from '../stores/robot';

type Message = { role: 'user' | 'robot'; text: string };
const quick = ['Como você está?', 'Que horas são?', 'Qual é a previsão do tempo?'];

export default function Chat() {
  const host = useConnectionValue(s => s.host);
  return <Conversation key={host} />;
}

function Conversation() {
  const confirm = useConfirm('danger');
  const [text, setText] = useState('');
  const [history, setHistory] = useState<Message[]>([]);
  const [sending, setSending] = useState(false);
  const [proposal, setProposal] = useState<MemoryProposal | null>(null);
  const key = useRobot(s => s?.hasKey);
  const disabled = useDisabled();
  const a = useAction();
  const c = usePalette();
  function submit() {
    void a.run(async () => {
      const message = text.trim();
      if (!message) throw new ApiError('Digite uma mensagem.');
      setSending(true);
      try {
        const answer = await mutate('chat', { message }, chatReply, 30000);
        setHistory(previous => [...previous, { role: 'user', text: message },
          ...(answer.proposal ? [] : [{ role: 'robot' as const, text: answer.text }])]);
        if (answer.proposal !== undefined) setProposal(answer.proposal);
        setText('');
      } finally {
        setSending(false);
      }
    }, 'Resposta recebida.');
  }
  function resolve(action: 'keep' | 'dismiss') {
    if (!proposal) return;
    void a.run(async () => {
      const result = await mutate('memory_proposal', { id: proposal.id, action }, proposalResolution);
      setProposal(null);
      setHistory(previous => [...previous, { role: 'robot', text: result.resolved ? result.message : 'Esta proposta expirou ou já foi resolvida.' }]);
    }, action === 'keep' ? 'Proposta processada.' : 'Proposta dispensada.');
  }
  return <Screen title="Chat" icon="sparkles-outline">
    <Notice text={a.notice} failed={a.failed} />
    <Card title="Conversa">
      {key === false && <Txt muted>Configure a chave Gemini em Ajustes para usar o assistente online.</Txt>}
      {history.length === 0 && <Txt muted>Envie uma mensagem para conversar com o Ciborgue.</Txt>}
      {history.map((message, index) => <View key={index} style={{
        backgroundColor: message.role === 'user' ? c.accent : c.bg, alignSelf: message.role === 'user' ? 'flex-end' : 'flex-start',
        maxWidth: '93%', padding: 12, borderRadius: 13,
      }}><Txt style={{ color: message.role === 'user' ? c.ink : c.text }}>{message.text}</Txt></View>)}
      {proposal && <View style={{ backgroundColor: c.tint, padding: 14, borderRadius: 14, gap: 8 }}>
        <Txt style={{ fontWeight: '800' }}>Guardar na memória?</Txt>
        <Txt>{proposal.fact}</Txt>
        <Row>
          <Action title="Guardar" active disabled={disabled || a.pending} onPress={() => resolve('keep')} />
          <Action title="Dispensar" disabled={disabled || a.pending} onPress={() => resolve('dismiss')} />
        </Row>
      </View>}
      {sending && <Row><ActivityIndicator color={c.muted} /><Txt>Pensando…</Txt></Row>}
      <Field label="Mensagem" value={text} onChangeText={setText} editable={!sending} multiline maxLength={800} />
      <Row>
        <Action title="Enviar" icon="send-outline" active disabled={disabled || !text.trim()} onPress={submit} />
        <Action title="Limpar histórico" disabled={disabled} onPress={() => confirm('Limpar histórico', 'Apagar a conversa local e o histórico mantido pelo Ciborgue?', () => void a.run(async () => {
          const message = await mutate('clear_history', {}, ack);
          setHistory([]);
          setProposal(null);
          return message;
        }))} />
      </Row>
      <Row>{quick.map(message => <Action key={message} title={message} disabled={sending} onPress={() => setText(message)} />)}</Row>
    </Card>
  </Screen>;
}
