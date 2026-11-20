import { Ionicons } from '@expo/vector-icons';
import { useEffect, type ReactNode } from 'react';
import { KeyboardAvoidingView, Modal, Platform, Pressable, ScrollView, StyleSheet, View, useWindowDimensions } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { Notice, Row, Txt, usePalette } from './ui';
import { FeedbackLayer } from './feedback';
import { useFeedback } from './feedback-context';
import { useConnectionValue } from '../stores/robot';

export function Sheet({ title, children, onClose, busy }: { title: string; children: ReactNode; onClose: () => void; busy?: boolean }) {
  const c = usePalette();
  const { bottom } = useSafeAreaInsets();
  const { height } = useWindowDimensions();
  const online = useConnectionValue(s => s.online);
  const feedback = useFeedback();
  const registerSheet = feedback?.registerSheet;
  useEffect(() => registerSheet?.(), [registerSheet]);
  const close = () => { if (!busy) onClose(); };
  const back = () => {
    if (feedback?.confirmation) feedback.setConfirmation(null);
    else close();
  };
  return <Modal visible transparent animationType="slide" onRequestClose={back} statusBarTranslucent>
    <KeyboardAvoidingView behavior={Platform.OS === 'ios' ? 'padding' : undefined} style={styles.overlay}>
      <Pressable style={StyleSheet.absoluteFill} onPress={close} accessible={false} />
      <View accessibilityViewIsModal accessibilityElementsHidden={!!feedback?.confirmation}
        importantForAccessibility={feedback?.confirmation ? 'no-hide-descendants' : 'auto'} style={[styles.sheet, {
        backgroundColor: c.surface, maxHeight: height * 0.87, paddingBottom: Math.max(18, bottom),
      }]}>
        <View style={[styles.handle, { backgroundColor: c.line }]} />
        <Row><Txt style={styles.title}>{title}</Txt>
          <Pressable accessibilityRole="button" accessibilityLabel="Fechar painel" accessibilityState={{ disabled: busy }}
            disabled={busy} onPress={close} style={[styles.close, { backgroundColor: c.bg, opacity: busy ? 0.5 : 1 }]}>
            <Ionicons name="close" size={22} color={c.text} />
          </Pressable>
        </Row>
        <ScrollView keyboardShouldPersistTaps="handled" contentContainerStyle={styles.content}>
          {!online && <Notice failed text="Ciborgue offline. Você pode continuar editando e enviar quando reconectar." />}
          {children}
        </ScrollView>
      </View>
      <FeedbackLayer inSheet />
    </KeyboardAvoidingView>
  </Modal>;
}
const styles = StyleSheet.create({
  overlay: { flex: 1, justifyContent: 'flex-end', alignItems: 'center', backgroundColor: '#07121799' },
  sheet: { borderTopLeftRadius: 26, borderTopRightRadius: 26, width: '100%', maxWidth: 640, padding: 20, paddingTop: 10, gap: 12 },
  handle: { width: 36, height: 4, borderRadius: 2, alignSelf: 'center' },
  close: { width: 44, height: 44, borderRadius: 22, alignItems: 'center', justifyContent: 'center' },
  title: { flex: 1, fontSize: 23, lineHeight: 30, fontWeight: '800' },
  content: { gap: 14, paddingBottom: 6 },
});
