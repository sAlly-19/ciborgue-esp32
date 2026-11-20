import { useConfirm } from '../components/feedback-context';
import { useRouter } from 'expo-router';
import { useState } from 'react';
import { View } from 'react-native';
import { ack, alarms, ApiError, integer, type Body } from '../api/protocol';
import { Countdown } from '../components/countdown';
import { Sheet } from '../components/sheet';
import { Action, Card, Field, Grid, Notice, Row, Screen, Selection, Skeleton, Toggle, Txt, useAction, useDisabled, usePalette } from '../components/ui';
import { days } from '../constants/robot';
import { useResource } from '../hooks/use-resource';
import { mutate, refresh, send, trackTimer, useConnectionValue, useTelemetry } from '../stores/robot';
import { duration, pad } from '../utils/format';

type Timer = { seconds: number; label: string };
type Session = { focus: number; rest: number; goal: string };
type Panel = 'Timer' | 'Pomodoro' | 'Novo alarme';
const timerPresets = [1, 5, 10, 25];
const sessionPresets = [{ focus: 25, rest: 5 }, { focus: 15, rest: 3 }, { focus: 50, rest: 10 }];

export default function Time() {
  const confirm = useConfirm('danger');
  const router = useRouter();
  const c = usePalette();
  const snapshot = useTelemetry();
  const s = snapshot?.data;
  const a = useAction();
  const busy = useConnectionValue(s => s.busy);
  const disabled = useDisabled();
  const list = useResource('alarms', alarms);
  const [timer, setTimer] = useState<Timer>({ seconds: 300, label: '' });
  const [session, setSession] = useState<Session>({ focus: 25, rest: 5, goal: '' });
  const [panel, setPanel] = useState<Panel>();
  const [alarmDraft, setAlarmDraft] = useState({ hour: '7', minute: '30', label: '', mask: 127 });
  const [failedStart, setFailedStart] = useState<'Timer' | 'Pomodoro'>();
  const alarmFeedback = useAction();
  const alarmForm = useAction();

  function startTimer() {
    setFailedStart('Timer');
    void a.run(async () => {
      const label = timer.label.trim() || 'Timer';
      const message = await mutate('timer', { seconds: timer.seconds, label }, value => {
        const accepted = ack(value);
        trackTimer(label);
        return accepted;
      });
      await refresh();
      setFailedStart(undefined);
      return message;
    });
  }
  function startPomodoro() {
    setFailedStart('Pomodoro');
    void a.run(async () => {
      const message = await mutate('pomodoro', {
        action: 'iniciar', focus_min: session.focus, break_min: session.rest, goal: session.goal.trim() || 'Foco',
      }, ack);
      await refresh();
      setFailedStart(undefined);
      return message;
    });
  }

  async function alarmAction(body: Body) {
    const message = await mutate('alarms', body, ack);
    await list.reload();
    return message;
  }
  return <Screen title="Rotina" icon="time-outline" onRefresh={list.reload}>
    <Notice text={a.notice} failed={a.failed} />
    <Grid>
      <Card title="Timer">
        <Notice text={snapshot?.timerFinished ? `Tempo encerrado · ${snapshot.timerFinished}` : ''} />
        {s?.timerTotal && snapshot ? <>
          <Txt muted>{s.timerLabel || 'Em andamento'}</Txt>
          <Countdown seconds={s.timer} receivedAt={snapshot.receivedAt} />
          <Action title="Cancelar timer" disabled={disabled} onPress={() => void a.run(async () => {
            await send('command', { cmd: 'CMD:TIMER_STOP' });
            trackTimer();
          })} />
        </> : <>
          <Txt style={{ fontSize: 27, lineHeight: 32, fontWeight: '800', fontVariant: ['tabular-nums'] }}>{duration(timer.seconds)}</Txt>
          {!!timer.label && <Txt muted>{timer.label}</Txt>}
          <View accessibilityRole="radiogroup" accessibilityLabel="Duração do timer"><Row>
            {timerPresets.map(minutes => <Selection key={minutes} label={`${minutes} min`} kind="radio" checked={timer.seconds === minutes * 60}
              onPress={() => { setTimer({ seconds: minutes * 60, label: '' }); setFailedStart(undefined); }} />)}
          </Row></View>
          <Row>
            <Action title={failedStart === 'Timer' && a.failed ? 'Tentar novamente' : 'Iniciar timer'} icon="play-outline" active disabled={disabled} pending={a.pending && failedStart === 'Timer'} onPress={startTimer} />
            <Action title="Personalizar" onPress={() => setPanel('Timer')} />
          </Row>
        </>}
      </Card>
      <Card title="Pomodoro">
        {s?.pomo && snapshot ? <>
          <Txt muted>{s.goal || 'Em andamento'}</Txt>
          <Countdown seconds={s.pomoRemaining} receivedAt={snapshot.receivedAt} />
          <Action title="Parar sessão" disabled={disabled} onPress={() => void a.run(() => mutate('pomodoro', { action: 'parar' }, ack))} />
        </> : <>
          <Txt style={{ fontWeight: '800' }}>{session.focus} min de foco · {session.rest} min de pausa</Txt>
          {!!session.goal && <Txt muted>{session.goal}</Txt>}
          <View accessibilityRole="radiogroup" accessibilityLabel="Duração do Pomodoro"><Row>
            {sessionPresets.map(({ focus, rest }) => <Selection key={focus} label={`${focus}/${rest}`} kind="radio"
              checked={focus === session.focus && rest === session.rest} onPress={() => { setSession({ focus, rest, goal: '' }); setFailedStart(undefined); }} />)}
          </Row></View>
          <Row>
            <Action title={failedStart === 'Pomodoro' && a.failed ? 'Tentar novamente' : 'Iniciar sessão'} icon="play-outline" active disabled={disabled} pending={a.pending && failedStart === 'Pomodoro'} onPress={startPomodoro} />
            <Action title="Personalizar" onPress={() => setPanel('Pomodoro')} />
          </Row>
        </>}
      </Card>
    </Grid>
    <Card title="Alarmes">
      <Notice text={alarmFeedback.notice} failed={alarmFeedback.failed} />
      <Row>
        <Action title="Novo alarme" icon="add-outline" onPress={() => setPanel('Novo alarme')} />
        {s?.ringing && <Action title="Silenciar" active disabled={disabled} onPress={() => void alarmFeedback.run(() => alarmAction({ action: 'stop' }))} />}
        {!!list.value?.length && <Action title="Limpar todos" disabled={disabled} onPress={() => confirm('Limpar alarmes', 'Excluir todos os alarmes do Ciborgue?', () => void alarmFeedback.run(() => alarmAction({ action: 'clear' })))} />}
      </Row>
      {list.pending && !list.value && <Skeleton />}
      <Notice text={list.error} failed />
      {list.value?.length === 0 && <Txt muted>Nenhum alarme cadastrado.</Txt>}
      {list.value?.map(item => <View key={item.id} style={{ borderTopWidth: 1, borderTopColor: c.line, paddingTop: 10, gap: 4 }}>
        <Txt style={{ fontWeight: '800' }}>{pad(item.hour)}:{pad(item.minute)} · {item.label}</Txt>
        <Txt muted>{item.daysMask ? days.filter((_, i) => Boolean(item.daysMask & (1 << i))).join(' · ') : 'Uma vez'}</Txt>
        <Toggle label={item.enabled ? 'Ativado' : 'Desativado'} value={item.enabled} disabled={disabled || list.pending}
          onChange={enabled => void alarmFeedback.run(() => alarmAction({ action: 'toggle', id: item.id, enabled }))} />
        <Action title="Excluir" disabled={disabled || list.pending} onPress={() => confirm('Excluir alarme', `Excluir “${item.label}”?`, () => void alarmFeedback.run(() => alarmAction({ action: 'del', id: item.id })))} />
      </View>)}
    </Card>
    <Action title="Tarefas e memórias" icon="albums-outline" onPress={() => router.navigate('/memoria')} />
    {panel === 'Timer' && <TimerEditor initial={timer} onClose={() => setPanel(undefined)} onSave={next => { setTimer(next); setFailedStart(undefined); setPanel(undefined); }} />}
    {panel === 'Pomodoro' && <SessionEditor initial={session} onClose={() => setPanel(undefined)} onSave={next => { setSession(next); setFailedStart(undefined); setPanel(undefined); }} />}
    {panel === 'Novo alarme' && <Sheet title="Novo alarme" busy={busy || alarmForm.pending} onClose={() => setPanel(undefined)}>
      <Row>
        <Field label="Hora" value={alarmDraft.hour} onChangeText={hour => setAlarmDraft(d => ({ ...d, hour }))} keyboardType="number-pad" maxLength={2} />
        <Field label="Minuto" value={alarmDraft.minute} onChangeText={minute => setAlarmDraft(d => ({ ...d, minute }))} keyboardType="number-pad" maxLength={2} />
      </Row>
      <Field label="Nome (opcional)" value={alarmDraft.label} onChangeText={label => setAlarmDraft(d => ({ ...d, label }))} />
      <Row>{days.map((day, i) => <Selection key={day} label={day} kind="checkbox" checked={Boolean(alarmDraft.mask & (1 << i))}
        onPress={() => setAlarmDraft(d => ({ ...d, mask: d.mask ^ (1 << i) }))} />)}</Row>
      <Txt muted>{alarmDraft.mask ? 'Repete nos dias selecionados' : 'Uma vez'}</Txt>
      <Notice text={alarmForm.notice} failed={alarmForm.failed} />
      <Action title={alarmForm.failed ? 'Tentar novamente' : 'Criar alarme'} active disabled={disabled} pending={alarmForm.pending} onPress={() => void alarmForm.run(async () => {
        const message = await alarmAction({
          action: 'add', hour: integer(alarmDraft.hour, 0, 23, 'Hora'), minute: integer(alarmDraft.minute, 0, 59, 'Minuto'),
          daysMask: alarmDraft.mask, label: alarmDraft.label.trim() || 'Despertador',
        });
        if (/limite|hora.*inv.lid/i.test(message)) throw new ApiError(message);
        setAlarmDraft({ hour: '7', minute: '30', label: '', mask: 127 });
        setPanel(undefined);
        return message;
      })} />
    </Sheet>}
  </Screen>;
}

function TimerEditor({ initial, onClose, onSave }: { initial: Timer; onClose: () => void; onSave: (draft: Timer) => void }) {
  const [hour, setHour] = useState(String(Math.floor(initial.seconds / 3600)));
  const [minute, setMinute] = useState(String(Math.floor(initial.seconds / 60) % 60));
  const [second, setSecond] = useState(String(initial.seconds % 60));
  const [label, setLabel] = useState(initial.label);
  const [error, setError] = useState('');
  return <Sheet title="Duração do timer" onClose={onClose}>
    <Row>
      <Field label="Horas" value={hour} onChangeText={setHour} maxLength={2} keyboardType="number-pad" />
      <Field label="Minutos" value={minute} onChangeText={setMinute} maxLength={2} keyboardType="number-pad" />
      <Field label="Segundos" value={second} onChangeText={setSecond} maxLength={2} keyboardType="number-pad" />
    </Row>
    <Field label="Nome (opcional)" value={label} onChangeText={setLabel} />
    <Notice text={error} failed />
    <Action title="Usar duração" active onPress={() => {
      try {
        const total = integer(hour, 0, 24, 'Horas') * 3600 + integer(minute, 0, 59, 'Minutos') * 60 + integer(second, 0, 59, 'Segundos');
        onSave({ seconds: integer(String(total), 1, 86400, 'Timer'), label: label.trim() });
      } catch (e) { if (e instanceof Error) setError(e.message); }
    }} />
  </Sheet>;
}
function SessionEditor({ initial, onClose, onSave }: { initial: Session; onClose: () => void; onSave: (draft: Session) => void }) {
  const [focus, setFocus] = useState(String(initial.focus));
  const [rest, setRest] = useState(String(initial.rest));
  const [goal, setGoal] = useState(initial.goal);
  const [error, setError] = useState('');
  return <Sheet title="Configurar Pomodoro" onClose={onClose}>
    <Row>
      <Field label="Foco (min)" value={focus} onChangeText={setFocus} maxLength={3} keyboardType="number-pad" />
      <Field label="Pausa (min)" value={rest} onChangeText={setRest} maxLength={2} keyboardType="number-pad" />
    </Row>
    <Field label="Objetivo (opcional)" value={goal} onChangeText={setGoal} />
    <Notice text={error} failed />
    <Action title="Usar sessão" active onPress={() => {
      try { onSave({ focus: integer(focus, 1, 180, 'Foco'), rest: integer(rest, 1, 60, 'Pausa'), goal: goal.trim() }); }
      catch (e) { if (e instanceof Error) setError(e.message); }
    }} />
  </Sheet>;
}
