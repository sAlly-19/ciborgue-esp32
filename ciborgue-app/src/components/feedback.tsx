import { Ionicons } from '@expo/vector-icons';
import { useCallback, useEffect, useRef, useState, type ReactNode } from 'react';
import { AccessibilityInfo, Modal, Platform, Pressable, ScrollView, StyleSheet, View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { useConnectionValue, useTelemetry } from '../stores/robot';
import { FeedbackContext, useFeedback, type Confirmation, type Toast } from './feedback-context';
import { Action, Row, Txt, usePalette } from './ui';

export function FeedbackProvider({ children }: { children: ReactNode }) {
  const [confirmation, setConfirmation] = useState<Confirmation | null>(null);
  const [toast, setToast] = useState<Toast | null>(null);
  const [sheetCount, setSheetCount] = useState(0);
  const timeout = useRef<ReturnType<typeof setTimeout> | undefined>(undefined);
  const registerSheet = useCallback(() => {
    setSheetCount(count => count + 1);
    return () => setSheetCount(count => count - 1);
  }, []);
  const dismissToast = useCallback(() => {
    clearTimeout(timeout.current);
    setToast(null);
  }, []);
  const showToast = useCallback((next: Toast) => {
    clearTimeout(timeout.current);
    setToast(next);
    if (Platform.OS === 'ios') AccessibilityInfo.announceForAccessibility(next.message);
    timeout.current = setTimeout(dismissToast, 5500);
  }, [dismissToast]);
  useEffect(() => () => clearTimeout(timeout.current), []);
  return <FeedbackContext.Provider value={{ confirmation, setConfirmation, toast, showToast, dismissToast, sheetCount, registerSheet }}>
    {children}
    <RobotNotifications />
    <FeedbackLayer />
  </FeedbackContext.Provider>;
}

function RobotNotifications() {
  const showToast = useFeedback()?.showToast;
  const snapshot = useTelemetry();
  const online = useConnectionValue(state => state.online);
  const timer = snapshot?.timerFinished ?? '';
  const ringing = snapshot?.data.ringing ?? false;
  const last = useRef({ timer: '', ringing: false });
  useEffect(() => {
    if (!online) return;
    const newAlarm = ringing && !last.current.ringing;
    const finishedTimer = timer && timer !== last.current.timer;
    last.current = { timer, ringing };
    if (newAlarm) showToast?.({ message: 'O despertador do Ciborgue está tocando.', tone: 'info' });
    else if (finishedTimer) showToast?.({ message: `Tempo encerrado · ${timer}`, tone: 'info' });
  }, [online, ringing, showToast, timer]);
  return null;
}

export function FeedbackLayer({ inSheet = false }: { inSheet?: boolean }) {
  const feedback = useFeedback();
  if (!feedback || (!inSheet && feedback.sheetCount > 0)) return null;
  const { confirmation, setConfirmation, toast, dismissToast } = feedback;
  const cancel = () => setConfirmation(null);
  return <>
    {toast && !confirmation && <ToastBanner toast={toast} dismiss={dismissToast} />}
    {confirmation && (inSheet
      ? <ConfirmationPanel confirmation={confirmation} cancel={cancel} />
      : <Modal transparent animationType="fade" statusBarTranslucent onRequestClose={cancel}>
        <ConfirmationPanel confirmation={confirmation} cancel={cancel} />
      </Modal>)}
  </>;
}
function ToastBanner({ toast, dismiss }: { toast: Toast; dismiss: () => void }) {
  const c = usePalette();
  const { top } = useSafeAreaInsets();
  const color = toast.tone === 'error' ? c.danger : c.accent;
  const icon = toast.tone === 'success' ? 'checkmark-circle-outline' : toast.tone === 'error' ? 'alert-circle-outline' : 'information-circle-outline';
  return <View pointerEvents="box-none" style={[styles.toastPosition, { top: top + 12 }]}>
    <View accessibilityLiveRegion="polite" style={[styles.toast, { backgroundColor: c.surface, borderColor: c.line, shadowColor: c.text }]}>
      <Ionicons name={icon} size={20} color={color} />
      <Txt style={styles.message}>{toast.message}</Txt>
      <Pressable accessibilityRole="button" accessibilityLabel="Dispensar aviso" onPress={dismiss} style={styles.dismiss}>
        <Ionicons name="close" color={c.muted} size={19} />
      </Pressable>
    </View>
  </View>;
}
function ConfirmationPanel({ confirmation, cancel }: { confirmation: Confirmation; cancel: () => void }) {
  const c = usePalette();
  const danger = confirmation.intent === 'danger';
  return <View style={styles.overlay} accessibilityViewIsModal>
    <Pressable accessible={false} style={StyleSheet.absoluteFill} onPress={cancel} />
    <View style={[styles.dialog, { backgroundColor: c.surface }]}>
      <View style={[styles.dialogIcon, { backgroundColor: danger ? c.dangerTint : c.tint }]}>
        <Ionicons name={danger ? 'warning-outline' : 'information-circle-outline'} size={23} color={danger ? c.danger : c.accent} />
      </View>
      <Txt style={styles.heading}>{confirmation.title}</Txt>
      <ScrollView style={styles.dialogBody}><Txt muted>{confirmation.message}</Txt></ScrollView>
      <Row>
        <Action title="Cancelar" onPress={cancel} />
        <Action title="Confirmar" variant={danger ? 'danger' : 'primary'} onPress={() => {
          cancel();
          confirmation.action();
        }} />
      </Row>
    </View>
  </View>;
}
const styles = StyleSheet.create({
  toastPosition: { position: 'absolute', left: 14, right: 14, alignItems: 'center', zIndex: 30 },
  toast: { width: '100%', maxWidth: 540, minHeight: 52, flexDirection: 'row', alignItems: 'center', gap: 9, paddingLeft: 14, paddingRight: 4, paddingVertical: 4, borderRadius: 16, borderWidth: 1, shadowOpacity: 0.18, shadowRadius: 12, elevation: 8 },
  message: { flex: 1, fontSize: 14, lineHeight: 19 },
  dismiss: { width: 44, height: 44, justifyContent: 'center', alignItems: 'center' },
  overlay: { position: 'absolute', top: 0, right: 0, bottom: 0, left: 0, zIndex: 40, justifyContent: 'center', alignItems: 'center', padding: 20, backgroundColor: '#07121799' },
  dialog: { width: '100%', maxWidth: 420, maxHeight: '85%', padding: 22, gap: 12, borderRadius: 24 },
  dialogIcon: { width: 44, height: 44, borderRadius: 14, alignItems: 'center', justifyContent: 'center' },
  heading: { fontSize: 22, lineHeight: 29, fontWeight: '800' },
  dialogBody: { flexGrow: 0 },
});
