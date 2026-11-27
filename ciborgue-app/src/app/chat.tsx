import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { useConfirm } from '../components/feedback-context';
import { useState } from 'react';
import { ActivityIndicator, Pressable, ScrollView, View } from 'react-native';
import { ack, ApiError, chatReply, proposalResolution, type MemoryProposal } from '../api/protocol';
import { Action, Card, Field, Notice, Row, Screen, Txt, useAction, useDisabled, usePalette } from '../components/ui';
import { mutate, useConnectionValue, useRobot } from '../stores/robot';

type Message = { role: 'user' | 'robot'; text: string; id: string };
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
        const now = Date.now();
        setHistory(previous => [
          ...previous,
          { role: 'user', text: message, id: `user-${now}` },
          ...(answer.proposal ? [] : [{ role: 'robot' as const, text: answer.text, id: `robot-${now + 1}` }]),
        ]);
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
      setHistory(previous => [...previous, {
        role: 'robot',
        text: result.resolved ? result.message : 'Esta proposta expirou ou já foi resolvida.',
        id: `proposal-${Date.now()}`,
      }]);
    }, action === 'keep' ? 'Proposta processada.' : 'Proposta dispensada.');
  }

  return <Screen title="Chat" icon="sparkles-outline">
    <Notice text={a.notice} failed={a.failed} />
    <Card title="Conversa">
      {key === false && <Txt muted>Configure a chave Gemini em Ajustes para usar o assistente online.</Txt>}
      {history.length === 0 && <Txt muted>Envie uma mensagem para conversar com o Ciborgue.</Txt>}
      {history.map(item => (
        <View key={item.id} style={{
          backgroundColor: item.role === 'user' ? c.accent : c.bg,
          alignSelf: item.role === 'user' ? 'flex-end' : 'flex-start',
          maxWidth: '93%',
          padding: 12,
          borderRadius: 13,
          marginBottom: 8,
        }}>
          <Txt style={{ color: item.role === 'user' ? c.ink : c.text }}>{item.text}</Txt>
        </View>
      ))}
      {proposal && <View style={{ backgroundColor: c.tint, padding: 14, borderRadius: 14, gap: 8 }}>
        <Txt style={{ fontWeight: '800' }}>Guardar na memória?</Txt>
        <Txt>{proposal.fact}</Txt>
        <Row>
          <Action title="Guardar" active disabled={disabled || a.pending} onPress={() => resolve('keep')} />
          <Action title="Dispensar" disabled={disabled || a.pending} onPress={() => resolve('dismiss')} />
        </Row>
      </View>}
      {sending && <View style={{ flexDirection: 'row', alignItems: 'center', gap: 8, paddingVertical: 6 }}>
        <ActivityIndicator size="small" color={c.accent} />
        <Txt muted style={{ fontSize: 13 }}>Ciborgue pensando…</Txt>
      </View>}
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
      <ScrollView
        horizontal
        showsHorizontalScrollIndicator={false}
        keyboardShouldPersistTaps="handled"
        accessibilityLabel="Sugestões de perguntas rápidas"
        contentContainerStyle={{ gap: 8, paddingVertical: 4 }}>
        {quick.map(message => (
          <Pressable
            key={message}
            accessibilityRole="button"
            accessibilityLabel={message}
            accessibilityState={{ disabled: sending }}
            disabled={sending}
            onPress={() => {
              setText(message);
              void Haptics.selectionAsync?.().catch(() => {});
            }}
            style={({ pressed }) => [{
              flexDirection: 'row',
              alignItems: 'center',
              gap: 6,
              paddingHorizontal: 12,
              paddingVertical: 7,
              borderRadius: 16,
              borderWidth: 1,
              borderColor: c.line,
              backgroundColor: c.bg,
              opacity: sending ? 0.5 : pressed ? 0.72 : 1,
            }]}>
            <Ionicons name="sparkles-outline" size={13} color={c.accent} />
            <Txt style={{ fontSize: 13, color: c.text }}>{message}</Txt>
          </Pressable>
        ))}
      </ScrollView>
    </Card>
  </Screen>;
}
