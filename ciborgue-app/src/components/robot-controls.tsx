import { Ionicons } from '@expo/vector-icons';
import { useState } from 'react';
import { Pressable, View } from 'react-native';
import { ApiError, integer } from '../api/protocol';
import { colors, effects, emotions, legacyEmotions, namedEffects, sounds, states } from '../constants/robot';
import { send, useRobot } from '../stores/robot';
import { rgb } from '../utils/format';
import { Range } from './range';
import { Action, Choices, Field, Notice, Row, Toggle, Txt, useAction, useDisabled, usePalette, type IconName } from './ui';

const expressionIcons: Partial<Record<keyof typeof emotions, IconName>> = {
  HAPPY: 'happy-outline', SAD: 'sad-outline', LOVE: 'heart-outline', SURPRISED: 'alert-circle-outline',
  SLEEPY: 'moon-outline', FEAR: 'eye-outline', DESPAIR: 'sad-outline', WINK: 'eye-off-outline',
  ANGRY: 'thunderstorm-outline', DISGUST: 'close-circle-outline', CONTEMPT: 'eye-outline',
  SHAME: 'hand-left-outline', MALICE: 'flash-outline',
  CURIOUS: 'search-outline', BORED: 'remove-outline', SILENT: 'volume-mute-outline', ANGEL: 'sunny-outline',
  COOL: 'glasses-outline', CRAZY: 'sync-outline', LIAR: 'flag-outline',
};

export function ExpressionControls() {
  const state = useRobot(s => s?.state);
  const emotion = useRobot(s => s?.emotion);
  const revision = useRobot(s => s?.revision ?? 1);
  const supported = useRobot(s => s?.capabilities?.emotions);
  const available = supported
    ? Object.fromEntries(Object.entries(emotions).filter(([key]) => supported.includes(key)))
    : legacyEmotions;
  const disabled = useDisabled();
  const a = useAction();
  const c = usePalette();
  const [nextState, setState] = useState<string>();
  const [nextEmotion, setEmotion] = useState<string>();
  const selectedState = nextState ?? state;
  const selectedEmotion = nextEmotion ?? emotion;
  return <>
    <View style={{ borderRadius: 14, padding: 14, backgroundColor: c.bg }}>
      <Row>
        <Ionicons name={expressionIcons[selectedEmotion as keyof typeof emotions] ?? 'scan-outline'} size={28} color={c.accent} />
        <View>
          <Txt muted>Prévia</Txt>
          <Txt>{states[selectedState as keyof typeof states] ?? '—'} · {emotions[selectedEmotion as keyof typeof emotions] ?? '—'}</Txt>
        </View>
      </Row>
    </View>
    <Txt muted>Estado</Txt>
    <Choices<keyof typeof states> items={states} selected={selectedState ?? ''} disabledKeys={revision < 2 ? ['BOOT'] : undefined} onSelect={setState} />
    <Txt muted>Emoção</Txt>
    <Choices items={available} selected={selectedEmotion ?? ''} onSelect={setEmotion} />
    <Notice text={a.notice} failed={a.failed} />
    <Action title={a.failed ? 'Tentar novamente' : 'Aplicar'} active pending={a.pending} disabled={disabled || (!nextState && !nextEmotion)}
      onPress={() => void a.run(async () => {
        if (nextState && nextState !== state) {
          const result = await send('state', { state: nextState });
          if (result.state !== nextState) throw new ApiError('O robô não confirmou o estado selecionado.');
        }
        if (nextEmotion && nextEmotion !== emotion) {
          if (!Object.hasOwn(available, nextEmotion)) throw new ApiError('Esta expressão não está disponível na placa conectada.');
          const result = await send('emotion', { emotion: nextEmotion });
          if (result.emotion !== nextEmotion) throw new ApiError('O robô não confirmou a expressão selecionada.');
        }
        setState(undefined);
        setEmotion(undefined);
      })} />
  </>;
}

const channels = ['Vermelho', 'Verde', 'Azul'];

export function LedControls() {
  const actual = useRobot(s => s?.rgb);
  const revision = useRobot(s => s?.revision ?? 1);
  const [draft, setDraft] = useState<string[]>();
  const [named, setNamed] = useState('');
  const [mode, setMode] = useState<keyof typeof effects>('SOLID');
  const [manual, setManual] = useState(false);
  const disabled = useDisabled();
  const a = useAction();
  const c = usePalette();
  const values = draft ?? (actual ?? [0, 0, 0]).map(String);
  const preview = values.map(value => Math.max(0, Math.min(255, Number(value) || 0)));
  return <>
    <View accessibilityLabel={`Prévia RGB ${preview.join(', ')}`} style={{ height: 66, backgroundColor: rgb(preview), borderColor: c.line, borderRadius: 14, borderWidth: 1 }} />
    <Row>{Object.entries(colors).map(([name, value]) => <Pressable key={name} accessibilityLabel={name} accessibilityRole="button"
      accessibilityState={{ selected: named === name }} onPress={() => { setNamed(name); setDraft(value.map(String)); }}
      style={{ width: 44, height: 44, borderRadius: 22, borderWidth: named === name ? 3 : 1, borderColor: named === name ? c.text : c.line, backgroundColor: rgb(value) }} />)}</Row>
    <Choices<keyof typeof effects> items={effects} selected={mode} onSelect={setMode} />
    <Action title={manual ? 'Ocultar RGB' : 'Ajustar RGB'} onPress={() => setManual(value => !value)} />
    {manual && <Row>{channels.map((label, i) => <Field key={label} label={label} value={values[i]} keyboardType="number-pad" maxLength={3}
      onChangeText={value => { setNamed(''); setDraft(values.map((old, index) => i === index ? value : old)); }} />)}</Row>}
    <Notice text={a.notice} failed={a.failed} />
    <Row>
      <Action title={a.failed ? 'Tentar novamente' : 'Aplicar'} active pending={a.pending} disabled={disabled} onPress={() => void a.run(async () => {
        const [r, g, b] = values.map((v, i) => integer(v, 0, 255, channels[i]));
        await send('led', named ? { color: named, effect: namedEffects[mode] } : { r, g, b, mode });
      })} />
      {revision >= 2 && <Action title="Restaurar automático" disabled={disabled} onPress={() => void a.run(async () => {
        await send('led', { mode: 'STATE' });
        setDraft(undefined);
        setNamed('');
      })} />}
    </Row>
  </>;
}

export function AudioControls() {
  const volume = useRobot(s => s?.volume ?? 0);
  const mute = useRobot(s => s?.mute ?? false);
  const chirps = useRobot(s => s?.chirps ?? false);
  const disabled = useDisabled();
  const a = useAction();
  const [draft, setDraft] = useState<number>();
  const [retry, setRetry] = useState<{ volume: number } | { mute: boolean } | { idle_chirps: boolean } | { play: string }>();
  const [showSounds, setShowSounds] = useState(false);
  const [dragging, setDragging] = useState(false);
  function apply(body: NonNullable<typeof retry>) {
    setRetry(body);
    void a.run(async () => {
      await send('audio', body);
      setRetry(undefined);
      setDraft(undefined);
    });
  }
  return <>
    <Txt>Volume · {draft ?? volume}%</Txt>
    <Range accessibilityLabel="Volume" minimumValue={0} maximumValue={100} step={1} value={draft ?? volume}
      style={{ height: 44 }} disabled={a.pending} onSlidingStart={() => setDragging(true)} onValueChange={setDraft}
      onSlidingComplete={value => {
        setDragging(false);
        setDraft(value);
        if (!disabled) apply({ volume: value });
      }} />
    {!dragging && !a.pending && draft !== undefined && !retry &&
      <Action title="Enviar volume" disabled={disabled} onPress={() => apply({ volume: draft })} />}
    <Toggle label="Mudo" value={mute} disabled={disabled} onChange={value => apply({ mute: value })} />
    <Toggle label="Chirps ociosos" value={chirps} disabled={disabled} onChange={value => apply({ idle_chirps: value })} />
    <Action title={showSounds ? 'Ocultar sons' : 'Testar um som'} onPress={() => setShowSounds(value => !value)} />
    {showSounds && <Choices items={sounds} disabled={disabled} onSelect={play => apply({ play })} />}
    <Notice text={a.notice} failed={a.failed} />
    {retry && a.failed && <Action title="Tentar novamente" disabled={disabled} onPress={() => apply(retry)} />}
  </>;
}
