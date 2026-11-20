import { useConfirm } from '../components/feedback-context';
import { useRouter } from 'expo-router';
import { useState } from 'react';
import { View } from 'react-native';
import { ApiError, memories, tasks, type Body } from '../api/protocol';
import { Action, Card, Field, Grid, Notice, Row, Screen, Skeleton, Txt, useAction, useDisabled } from '../components/ui';
import { useResource } from '../hooks/use-resource';
import { mutate, useRobot } from '../stores/robot';

export default function Memory() {
  const router = useRouter();
  const confirm = useConfirm('danger');
  const [title, setTitle] = useState('');
  const [fact, setFact] = useState('');
  const items = useResource('tasks', tasks);
  const facts = useResource('memories', memories);
  const a = useAction();
  const disabled = useDisabled();
  const revision = useRobot(s => s?.revision ?? 1);
  async function changeTask(body: Body) {
    const next = await mutate('tasks', body, tasks);
    items.setValue(next);
    return next;
  }
  async function changeMemory(body: Body) {
    const next = await mutate('memories', body, memories);
    facts.setValue(next);
    return next;
  }
  return <Screen title="Memória" onBack={() => router.navigate('/tempo')} backLabel="Voltar à rotina"
    onRefresh={async () => { await Promise.all([items.reload(), facts.reload()]); }}>
    <Notice text={a.notice} failed={a.failed} />
    <Grid>
      <Card title="Tarefas">
        <Field label="Nova tarefa" value={title} onChangeText={setTitle} maxLength={160} />
        <Row>
          <Action title="Adicionar tarefa" active disabled={disabled || !title.trim()} onPress={() => void a.run(async () => {
            await changeTask({ action: 'add', title: title.trim() });
            setTitle('');
          })} />
          <Action title="Limpar tarefas" disabled={disabled || !items.value?.length} onPress={() => confirm('Limpar tarefas', 'Excluir as tarefas pendentes e concluídas?', () => void a.run(async () => {
            await changeTask({ action: 'limpar' });
          }))} />
        </Row>
        {items.pending && !items.value && <Skeleton />}
        <Notice text={items.error} failed />
        {items.value && [false, true].map(done => {
          const group = items.value?.filter(item => item.done === done) ?? [];
          return <View key={String(done)} style={{ gap: 8 }}>
            <Txt style={{ fontWeight: '800' }}>{done ? 'Concluídas' : 'Pendentes'}</Txt>
            {!group.length && <Txt muted>Nenhuma tarefa.</Txt>}
            {group.map(item => <View key={item.id} style={{ gap: 6, paddingVertical: 7 }}>
              <Txt>{item.title}</Txt>
              <Row>
                {!done && <Action title="Concluir" disabled={disabled} onPress={() => void a.run(async () => { await changeTask({ action: 'done', id: item.id }); })} />}
                <Action title="Excluir" disabled={disabled} onPress={() => confirm('Excluir tarefa', `Excluir “${item.title}”?`, () => void a.run(async () => {
                  await changeTask({ action: 'del', id: item.id });
                }))} />
              </Row>
            </View>)}
          </View>;
        })}
      </Card>
      <Card title="Memórias de longo prazo">
        <Field label="O que o Ciborgue deve lembrar?" value={fact} onChangeText={setFact} multiline maxLength={300} />
        <Row>
          <Action title="Salvar memória" active disabled={disabled || !fact.trim()} onPress={() => void a.run(async () => {
            await changeMemory({ action: 'adicionar', fact: fact.trim() });
            setFact('');
          })} />
          <Action title="Limpar memórias" disabled={disabled || !facts.value?.length} onPress={() => confirm('Limpar memórias', 'Apagar todas as memórias de longo prazo gravadas no Ciborgue?', () => void a.run(async () => {
            await changeMemory({ action: 'limpar' });
          }))} />
        </Row>
        {facts.pending && !facts.value && <Skeleton />}
        <Notice text={facts.error} failed />
        {facts.value?.length === 0 && <Txt muted>Nenhuma memória cadastrada.</Txt>}
        {facts.value?.map(fact => <View key={fact} style={{ gap: 6, paddingVertical: 7 }}>
          <Txt>{fact}</Txt>
          <Action title="Remover memória" disabled={disabled || revision < 2} onPress={() => confirm('Remover memória', `Apagar “${fact}”?`, () => void a.run(async () => {
            const next = await changeMemory({ action: 'remover', fact });
            if (next.includes(fact)) throw new ApiError('A memória continua no robô. Confira a versão do firmware.');
          }))} />
        </View>)}
        {revision < 2 && <Txt muted>A remoção individual requer a revisão 2 da API.</Txt>}
      </Card>
    </Grid>
  </Screen>;
}
