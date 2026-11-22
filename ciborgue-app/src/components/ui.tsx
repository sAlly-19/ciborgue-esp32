import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { type ComponentProps, type ReactNode, useEffect, useMemo, useRef, useState } from 'react';
import {
  ActivityIndicator, Animated, PanResponder, Pressable, RefreshControl, ScrollView, StyleSheet, Text, TextInput, View,
} from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { errorText, refresh, useConnectionValue } from '../stores/robot';
import { usePalette } from './theme';
import { useFeedback } from './feedback-context';

export { usePalette } from './theme';
export type IconName = ComponentProps<typeof Ionicons>['name'];

export function Txt({ children, muted, style }: { children: ReactNode; muted?: boolean; style?: ComponentProps<typeof Text>['style'] }) {
  const c = usePalette();
  return <Text style={[styles.text, { color: muted ? c.muted : c.text }, style]}>{children}</Text>;
}
export function Row({ children }: { children: ReactNode }) {
  return <View style={styles.row}>{children}</View>;
}
export function Grid({ children }: { children: ReactNode }) {
  return <View style={styles.grid}>{children}</View>;
}
export function Card({ title, children, style }: { title: string; children: ReactNode; style?: ComponentProps<typeof View>['style'] }) {
  const c = usePalette();
  return <View style={[styles.card, { backgroundColor: c.surface, borderColor: c.line }, style]}>
    <Txt style={styles.cardTitle}>{title}</Txt>{children}
  </View>;
}
export function Action({ title, onPress, active, disabled, icon, variant, pending }: {
  title: string; onPress: () => void; active?: boolean; disabled?: boolean; icon?: IconName;
  variant?: 'primary' | 'secondary' | 'danger'; pending?: boolean;
}) {
  const c = usePalette();
  const kind = variant ?? (active ? 'primary' : 'secondary');
  const foreground = kind === 'primary' ? c.ink : kind === 'danger' ? c.danger : c.text;
  const background = kind === 'primary' ? c.accent : kind === 'danger' ? c.dangerTint : c.surface;
  const unavailable = disabled || pending;
  return <Pressable accessibilityRole="button" accessibilityState={{ disabled: unavailable, selected: active, busy: pending }}
    disabled={unavailable} onPress={onPress} style={({ pressed }) => [styles.button,
      { borderColor: kind === 'secondary' ? c.line : background, backgroundColor: background, opacity: unavailable && !pending ? 0.5 : pressed ? 0.72 : 1 }]}>
    {pending ? <ActivityIndicator size="small" color={foreground} /> : icon && <Ionicons name={icon} size={17} color={foreground} />}
    <Text style={{ color: foreground, fontWeight: '700', flexShrink: 1 }}>{title}</Text>
  </Pressable>;
}
export function Choices<T extends string>({ items, selected, onSelect, disabled, disabledKeys }: {
  items: Record<T, string>; selected?: string; onSelect: (key: T) => void;
  disabled?: boolean; disabledKeys?: readonly T[];
}) {
  if (selected === undefined) {
    return <Row>{(Object.keys(items) as T[]).map(key => <Action key={key} title={items[key]}
      onPress={() => onSelect(key)} disabled={disabled || disabledKeys?.includes(key)} />)}</Row>;
  }
  return <View accessibilityRole="radiogroup" style={styles.row}>
    {(Object.keys(items) as T[]).map(key => <Selection key={key} label={items[key]} kind="radio"
      checked={key === selected} onPress={() => onSelect(key)} disabled={disabled || disabledKeys?.includes(key)} />)}
  </View>;
}
export function Selection({ label, checked, onPress, disabled, kind }: {
  label: string; checked: boolean; onPress: () => void; disabled?: boolean; kind: 'radio' | 'checkbox';
}) {
  const c = usePalette();
  return <Pressable accessibilityRole={kind} accessibilityState={{ checked, disabled }} disabled={disabled} onPress={onPress}
    style={({ pressed }) => [styles.button, { borderColor: checked ? c.accent : c.line, backgroundColor: checked ? c.tint : c.surface,
      opacity: disabled ? 0.5 : pressed ? 0.75 : 1 }]}>
    <Ionicons name={kind === 'radio' ? checked ? 'radio-button-on-outline' : 'radio-button-off-outline'
      : checked ? 'checkbox-outline' : 'square-outline'} size={17} color={checked ? c.accent : c.muted} />
    <Text style={{ flexShrink: 1, color: c.text, fontWeight: checked ? '700' : '500' }}>{label}</Text>
  </Pressable>;
}
export function Field({ label, onFocus, onBlur, ...props }: ComponentProps<typeof TextInput> & { label: string }) {
  const c = usePalette();
  const [focused, setFocused] = useState(false);
  return <View style={styles.field}>
    <Txt muted>{label}</Txt>
    <TextInput accessibilityLabel={label} placeholderTextColor={c.muted} autoCorrect={false}
      underlineColorAndroid="transparent" selectionColor={c.accent} {...props}
      onFocus={event => { setFocused(true); onFocus?.(event); }}
      onBlur={event => { setFocused(false); onBlur?.(event); }}
      style={[styles.input, { color: c.text, backgroundColor: c.bg, borderColor: focused ? c.accent : c.line,
        opacity: props.editable === false ? 0.58 : 1 }, props.style]} />
  </View>;
}
export function Toggle({ label, value, onChange, disabled }: { label: string; value: boolean; onChange: (next: boolean) => void; disabled?: boolean }) {
  const c = usePalette();
  return <Pressable accessibilityRole="switch" accessibilityLabel={label} accessibilityState={{ checked: value, disabled }}
    disabled={disabled} onPress={() => onChange(!value)} style={({ pressed }) => [styles.toggle, { opacity: disabled ? 0.5 : pressed ? 0.75 : 1 }]}>
    <Txt style={{ flex: 1 }}>{label}</Txt>
    <View style={[styles.track, { backgroundColor: value ? c.accent : c.line, alignItems: value ? 'flex-end' : 'flex-start' }]}>
      <View style={[styles.thumb, { backgroundColor: value ? c.ink : c.surface }]}>
        {value && <Ionicons name="checkmark" size={13} color={c.accent} />}
      </View>
    </View>
  </Pressable>;
}
export function Skeleton() {
  const c = usePalette();
  const [opacity] = useState(() => new Animated.Value(0.5));
  useEffect(() => {
    const animation = Animated.loop(Animated.sequence([
      Animated.timing(opacity, { toValue: 1, duration: 650, useNativeDriver: true }),
      Animated.timing(opacity, { toValue: 0.5, duration: 650, useNativeDriver: true }),
    ]));
    animation.start();
    return () => animation.stop();
  }, [opacity]);
  return <Animated.View accessibilityLabel="Carregando" style={{ opacity, gap: 10 }}>
    {[92, 68, 83].map(width => <View key={width} style={{ backgroundColor: c.line, height: 16, borderRadius: 8, width: `${width}%` }} />)}
  </Animated.View>;
}
type HeaderStatus = { label: string; tone: 'active' | 'idle' | 'error'; icon?: IconName };

function HeaderBadge({ status, onPress }: { status: HeaderStatus; onPress?: () => void }) {
  const c = usePalette();
  const tone = status.tone;
  const color = tone === 'active' ? c.accent : tone === 'error' ? c.danger : c.muted;
  const style = [styles.badge, { backgroundColor: tone === 'active' ? c.tint : tone === 'error' ? c.dangerTint : c.bg }];
  const content = <>
    {status.icon ? <Ionicons name={status.icon} size={15} color={color} /> : <View style={[styles.statusDot, { backgroundColor: color }]} />}
    <Text numberOfLines={1} maxFontSizeMultiplier={1.3} style={[styles.badgeText, { color }]}>{status.label}</Text>
  </>;
  return onPress ? <Pressable accessibilityRole="button" accessibilityLabel={`${status.label}. Abrir ajustes de conexão`}
    onPress={onPress} style={({ pressed }) => [...style, { opacity: pressed ? 0.65 : 1 }]}>{content}</Pressable>
    : <View accessibilityLiveRegion="polite" style={style}>{content}</View>;
}
export function ConnectionLine({ onPress }: { onPress?: () => void }) {
  const online = useConnectionValue(data => data.online);
  const ready = useConnectionValue(data => data.ready);
  return <HeaderBadge onPress={onPress} status={online ? { label: 'Online', tone: 'active' }
    : ready ? { label: 'Offline', tone: 'error' } : { label: 'Conectando', tone: 'idle' }} />;
}
function ConnectionProblem() {
  const error = useConnectionValue(data => data.error);
  return <Notice text={error} failed />;
}
export function Screen({ title, icon, children, onRefresh, showConnection = true, status, onConnectionPress,
  onBack, backLabel = 'Voltar' }: {
  title: string; icon?: IconName; children: ReactNode; onRefresh?: () => Promise<unknown>; showConnection?: boolean;
  status?: HeaderStatus; onConnectionPress?: () => void; onBack?: () => void; backLabel?: string;
}) {
  const c = usePalette();
  const insets = useSafeAreaInsets();
  const [pulling, setPulling] = useState(false);
  const panResponder = useMemo(() => onBack ? PanResponder.create({
    onMoveShouldSetPanResponder: (_, g) => {
      const startX = g.moveX !== undefined ? g.moveX - g.dx : g.x0;
      return startX <= 40 && g.dx > 25 && Math.abs(g.dy) < 30;
    },
    onPanResponderRelease: (_, g) => {
      if (g.dx > 60 && Math.abs(g.dy) < 50) {
        void Haptics.impactAsync?.(Haptics.ImpactFeedbackStyle.Light).catch(() => {});
        onBack();
      }
    },
  }) : undefined, [onBack]);

  const paddingHorizontal = { paddingLeft: Math.max(18, insets.left), paddingRight: Math.max(18, insets.right) };
  return <View style={[styles.screen, { backgroundColor: c.bg }]} {...(panResponder?.panHandlers ?? {})}>
    <View style={[styles.header, { paddingTop: insets.top, backgroundColor: c.surface, borderBottomColor: c.line }]}>
      <View style={[styles.headerRow, paddingHorizontal]}>
        {onBack ? <Pressable accessibilityRole="button" accessibilityLabel={backLabel} onPress={onBack}
          style={({ pressed }) => [styles.headerIcon, { backgroundColor: c.bg, opacity: pressed ? 0.65 : 1 }]}>
          <Ionicons name="arrow-back" size={23} color={c.text} />
        </Pressable> : icon && <View style={[styles.headerIcon, { backgroundColor: c.tint }]}>
          <Ionicons name={icon} size={22} color={c.accent} />
        </View>}
        <Text accessibilityRole="header" style={[styles.title, { color: c.text }]}>{title}</Text>
        {showConnection ? <ConnectionLine onPress={onConnectionPress} /> : status && <HeaderBadge status={status} />}
      </View>
    </View>
    <ScrollView style={styles.screen} keyboardShouldPersistTaps="handled"
    contentContainerStyle={[styles.content, paddingHorizontal]}
    refreshControl={(showConnection || onRefresh) ? <RefreshControl refreshing={pulling} onRefresh={() => {
      setPulling(true);
      void Promise.allSettled([refresh(), onRefresh?.()]).finally(() => setPulling(false));
    }} tintColor={c.accent} colors={[c.accent]} progressBackgroundColor={c.surface} /> : undefined}>
      {showConnection && <ConnectionProblem />}
      {children}
    </ScrollView>
  </View>;
}
export function useAction() {
  const feedback = useFeedback();
  const [notice, setNotice] = useState('');
  const [failed, setFailed] = useState(false);
  const [pending, setPending] = useState(false);
  const running = useRef(false);
  const timeout = useRef<ReturnType<typeof setTimeout> | undefined>(undefined);
  useEffect(() => () => clearTimeout(timeout.current), []);
  async function run(job: () => Promise<unknown>, success = 'Atualizado no Ciborgue.') {
    if (running.current) return;
    running.current = true;
    setPending(true);
    clearTimeout(timeout.current);
    setNotice('');
    try {
      const result = await job();
      setFailed(false);
      const message = typeof result === 'string' && result ? result : success;
      if (feedback && (typeof result !== 'string' || !result)) feedback.showToast({ message, tone: 'success' });
      else setNotice(message);
      void Haptics.notificationAsync(Haptics.NotificationFeedbackType.Success).catch(() => {});
      timeout.current = setTimeout(() => setNotice(''), 4500);
    } catch (error) {
      setFailed(true);
      setNotice(errorText(error));
      void Haptics.notificationAsync(Haptics.NotificationFeedbackType.Error).catch(() => {});
    } finally {
      running.current = false;
      setPending(false);
    }
  }
  return { notice, failed, pending, run };
}
export function Notice({ text, failed }: { text: string; failed?: boolean }) {
  const c = usePalette();
  return text ? <View accessibilityLiveRegion="polite" style={[styles.notice, { backgroundColor: failed ? c.dangerTint : c.tint }]}>
    <Ionicons name={failed ? 'alert-circle-outline' : 'information-circle-outline'} size={20} color={failed ? c.danger : c.accent} />
    <Txt style={{ flex: 1, color: failed ? c.danger : c.text }}>{text}</Txt>
  </View> : null;
}
export function useDisabled() {
  return useConnectionValue(s => s.busy || !s.online);
}
const styles = StyleSheet.create({
  text: { fontSize: 15, lineHeight: 22 },
  screen: { flex: 1 },
  header: { borderBottomWidth: StyleSheet.hairlineWidth },
  headerRow: { width: '100%', maxWidth: 840, alignSelf: 'center', flexDirection: 'row', alignItems: 'center',
    minHeight: 66, paddingVertical: 10, columnGap: 10 },
  headerIcon: { width: 44, height: 44, borderRadius: 14, alignItems: 'center', justifyContent: 'center' },
  title: { flex: 1, fontSize: 26, lineHeight: 32, fontWeight: '800', letterSpacing: -0.8 },
  badge: { minHeight: 44, maxWidth: '43%', paddingHorizontal: 10, borderRadius: 22, flexDirection: 'row',
    alignItems: 'center', justifyContent: 'center', gap: 6 },
  badgeText: { flexShrink: 1, fontSize: 12, fontWeight: '700' },
  statusDot: { width: 7, height: 7, borderRadius: 4 },
  cardTitle: { fontSize: 18, lineHeight: 24, fontWeight: '800' },
  content: { width: '100%', maxWidth: 840, alignSelf: 'center', paddingTop: 16, paddingBottom: 28, gap: 14 },
  card: { padding: 17, borderRadius: 16, gap: 12, flexGrow: 1, flexBasis: 280 },
  row: { flexDirection: 'row', flexWrap: 'wrap', alignItems: 'center', gap: 8 },
  grid: { flexDirection: 'row', flexWrap: 'wrap', gap: 12 },
  button: { paddingHorizontal: 13, paddingVertical: 9, minHeight: 44, borderWidth: 1, borderRadius: 12, flexDirection: 'row', gap: 7, alignItems: 'center' },
  field: { minWidth: 76, flexGrow: 1, gap: 5 },
  input: { padding: 11, borderWidth: 1, borderRadius: 11, fontSize: 16, minHeight: 46 },
  toggle: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', minHeight: 44, gap: 10 },
  track: { width: 46, height: 26, borderRadius: 13, padding: 3, justifyContent: 'center' },
  thumb: { width: 20, height: 20, borderRadius: 10, alignItems: 'center', justifyContent: 'center' },
  notice: { padding: 12, borderRadius: 12, gap: 9, flexDirection: 'row', alignItems: 'flex-start' },
});
