import { useConfirm } from '../components/feedback-context';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { useRouter } from 'expo-router';
import { useEffect, useState } from 'react';
import { AccessibilityInfo, Animated, Pressable, StyleSheet, View } from 'react-native';
import { ack, personality } from '../api/protocol';
import { Countdown } from '../components/countdown';
import { AudioControls, ExpressionControls, LedControls } from '../components/robot-controls';
import { Sheet } from '../components/sheet';
import { Action, Card, Notice, Row, Screen, Toggle, Txt, useAction, useDisabled, usePalette, type IconName } from '../components/ui';
import { emotions, states } from '../constants/robot';
import { useResource } from '../hooks/use-resource';
import { mutate, send, useConnectionValue, useTelemetry } from '../stores/robot';
import { duration, label, metric, rgb } from '../utils/format';

const powerStages = { AWAKE: 'Acordado', ECO: 'Eco', LIGHT_SLEEP: 'Sono leve' };
type Panel = 'Expressões' | 'LED' | 'Áudio' | 'Sistema';

export default function Dashboard() {
  const confirm = useConfirm();
  const router = useRouter();
  const c = usePalette();
  const snapshot = useTelemetry();
  const busy = useConnectionValue(s => s.busy);
  const disabled = useDisabled();
  const a = useAction();
  const p = useResource('personality', personality);
  const [panel, setPanel] = useState<Panel>();
  const s = snapshot?.data;
  return <Screen title="Ciborgue" icon="hardware-chip-outline" onRefresh={p.reload}
    onConnectionPress={() => router.navigate('/configuracoes')}>
    <View
      accessibilityRole="summary"
      accessibilityLabel={`Telemetria do robô: Estado ${s ? label(states, s.state) : 'indisponível'}, Emoção ${s ? label(emotions, s.emotion) : 'indisponível'}, Afeto ${metric(s?.affection ?? NaN, '%')}`}
      style={[styles.telemetry, { backgroundColor: c.surface }]}>
      <Reading title="Estado" value={s ? label(states, s.state) : '—'} />
      <Reading title="Emoção" value={s ? label(emotions, s.emotion) : '—'} />
      <Reading title="Afeto" value={metric(s?.affection ?? NaN, '%')} />
    </View>
    <View style={styles.shortcuts}>
      <Shortcut icon="heart-outline" title="Carinho" detail="Enviar agora" disabled={disabled} onPress={() => {
        void Haptics.impactAsync?.(Haptics.ImpactFeedbackStyle.Medium).catch(() => {});
        void a.run(() => send('pet'), 'Carinho enviado.');
      }} />
      <Shortcut icon="happy-outline" title="Expressões" detail="Rosto e estado" onPress={() => setPanel('Expressões')} />
      <Shortcut icon="color-palette-outline" title="LED" detail={s ? 'Cor e efeitos' : 'Sem leitura'} color={s && rgb(s.rgb)} onPress={() => setPanel('LED')} />
      <Shortcut icon="volume-medium-outline" title="Áudio" detail={s ? s.mute ? 'Mudo' : `Volume ${s.volume}%` : 'Volume e sons'} onPress={() => setPanel('Áudio')} />
    </View>
    <Notice text={a.notice} failed={a.failed} />
    <Card title="Dispositivo">
      <Toggle label="Relógio de mesa" value={s?.clock ?? false} disabled={disabled} onChange={active => void a.run(() => send('desk_clock', { active }))} />
      <Row><Txt muted>Energia · {s ? label(powerStages, s.power) : '—'}</Txt>
        <Action title="Sistema" icon="options-outline" onPress={() => setPanel('Sistema')} /></Row>
    </Card>
    {snapshot && s && (!!s.timerTotal || s.pomo || s.ringing || !!snapshot.timerFinished) && <Card title="Atividade atual">
      {!!s.timerTotal && <><Txt>{s.timerLabel || 'Timer'}</Txt><Countdown seconds={s.timer} receivedAt={snapshot.receivedAt} /></>}
      {s.pomo && <><Txt>{s.goal || 'Pomodoro'}</Txt><Countdown seconds={s.pomoRemaining} receivedAt={snapshot.receivedAt} /></>}
      {s.ringing && <Txt>Despertador tocando</Txt>}
      {!!snapshot.timerFinished && <Txt>Tempo encerrado · {snapshot.timerFinished}</Txt>}
      <Action title="Abrir Rotina" onPress={() => router.navigate('/tempo')} />
    </Card>}
    {panel && <Sheet title={panel} busy={busy} onClose={() => setPanel(undefined)}>
      {panel === 'Expressões' && <ExpressionControls />}
      {panel === 'LED' && <LedControls />}
      {panel === 'Áudio' && <AudioControls />}
      {panel === 'Sistema' && <>
        <Txt>{s?.ssid || 'SSID indisponível'} · {s?.ip || 'IP indisponível'}</Txt>
        <Txt muted>{metric(s?.temp ?? NaN, ' °C')} · {s?.wifi ? metric(s.rssi, ' dBm') : 'RSSI indisponível'} · {s?.network ?? '—'}</Txt>
        <Txt muted>Heap livre {metric(s?.heap ?? NaN, ' KB')} · PSRAM livre {metric(s?.psram ?? NaN, ' KB')}</Txt>
        <Txt muted>Ligado há {duration(s?.uptime ?? NaN)} · Portal {s?.portal ? 'aberto' : 'fechado'}</Txt>
        <Txt>Humor {s?.moodLabel ?? p.value?.label ?? '—'} · Energia {metric(s?.energy ?? NaN, '%')}</Txt>
        {p.value && <Txt muted>{metric(p.value.pets)} carinhos · {metric(p.value.chats)} conversas</Txt>}
        <Notice text={p.error} failed />
        <Notice text={a.notice} failed={a.failed} />
        <Row>
          <Action title="Acordar" disabled={disabled} onPress={() => void a.run(() => send('sleep', { mode: 'wake' }))} />
          <Action title="Eco" disabled={disabled} onPress={() => void a.run(() => send('sleep', { mode: 'eco' }))} />
          <Action title="Dormir" disabled={disabled} onPress={() => confirm('Sono leve', 'O robô pode precisar de toque ou botão para acordar.', () => void a.run(() => mutate('sleep', { mode: 'light' }, ack)))} />
          <Action title="Abrir portal" disabled={disabled || s?.portal} onPress={() => void a.run(() => send('portal', { action: 'start' }))} />
          <Action title="Mostrar humor" disabled={disabled} onPress={() => void a.run(async () => p.setValue(await mutate('personality', {}, personality)))} />
        </Row>
      </>}
    </Sheet>}
  </Screen>;
}
function Reading({ title, value }: { title: string; value: string }) {
  return <View style={{ flex: 1, gap: 3 }}>
    <Txt muted style={styles.small}>{title}</Txt>
    <Txt style={{ fontWeight: '800' }}>{value}</Txt>
  </View>;
}
function Shortcut({ icon, title, detail, color, disabled, onPress }: {
  icon: IconName; title: string; detail: string; color?: string; disabled?: boolean; onPress: () => void;
}) {
  const c = usePalette();
  const [scale] = useState(() => new Animated.Value(1));
  const [reduced, setReduced] = useState(true);
  useEffect(() => {
    let mounted = true;
    void AccessibilityInfo.isReduceMotionEnabled().then(value => { if (mounted) setReduced(value); }).catch(() => {});
    const sub = AccessibilityInfo.addEventListener('reduceMotionChanged', setReduced);
    return () => { mounted = false; sub.remove(); };
  }, []);
  const animatePulse = () => {
    if (reduced) return;
    Animated.sequence([
      Animated.timing(scale, { toValue: 1.25, duration: 150, useNativeDriver: true }),
      Animated.timing(scale, { toValue: 1, duration: 150, useNativeDriver: true }),
    ]).start();
  };
  const handlePress = () => {
    animatePulse();
    onPress();
  };
  return <Pressable accessibilityRole="button" accessibilityLabel={`${title}. ${detail}`} accessibilityState={{ disabled }} disabled={disabled} onPress={handlePress}
    style={({ pressed }) => [styles.tile, { backgroundColor: c.surface, opacity: disabled || pressed ? 0.65 : 1 }]}>
    <View style={styles.tileTop}>
      <Animated.View style={{ transform: [{ scale }] }}>
        <Ionicons name={icon} color={c.accent} size={24} />
      </Animated.View>
      {!!color && <View style={[styles.dot, { backgroundColor: color, borderWidth: 1, borderColor: c.line }]} />}
    </View>
    <Txt style={styles.tileTitle}>{title}</Txt><Txt muted style={styles.small}>{detail}</Txt>
  </Pressable>;
}
const styles = StyleSheet.create({
  dot: { width: 10, height: 10, borderRadius: 5 },
  small: { fontSize: 13, lineHeight: 18 },
  telemetry: { flexDirection: 'row', gap: 12, borderRadius: 16, padding: 13 },
  shortcuts: { flexDirection: 'row', flexWrap: 'wrap', gap: 10 },
  tile: { flexBasis: '46%', flexGrow: 1, borderRadius: 16, padding: 15, minHeight: 102, gap: 4 },
  tileTop: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  tileTitle: { fontSize: 17, lineHeight: 23, fontWeight: '800', marginTop: 4 },
});
