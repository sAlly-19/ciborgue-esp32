import { Ionicons } from '@expo/vector-icons';
import type { Tabs } from 'expo-router';
import { useEffect, useState, type ComponentProps } from 'react';
import { AccessibilityInfo, Animated, Keyboard, Pressable, StyleSheet, View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { usePalette } from './theme';
import { Txt, type IconName } from './ui';

export const dockTabs: { name: string; title: string; icon: IconName }[] = [
  { name: 'index', title: 'Início', icon: 'home-outline' },
  { name: 'tempo', title: 'Rotina', icon: 'time-outline' },
  { name: 'chat', title: 'IA', icon: 'sparkles-outline' },
  { name: 'configuracoes', title: 'Ajustes', icon: 'settings-outline' },
];
const parentTab: Record<string, string> = { leds: 'index', expressoes: 'index', memoria: 'tempo', avancado: 'configuracoes', bluetooth: 'configuracoes' };
type TabBarProps = Parameters<NonNullable<ComponentProps<typeof Tabs>['tabBar']>>[0];

export function Dock({ state, navigation }: TabBarProps) {
  const c = usePalette();
  const insets = useSafeAreaInsets();
  const [keyboard, setKeyboard] = useState(() => Keyboard.isVisible());
  const [reduced, setReduced] = useState(true);
  useEffect(() => {
    let mounted = true;
    void AccessibilityInfo.isReduceMotionEnabled().then(value => { if (mounted) setReduced(value); }).catch(() => {});
    const motion = AccessibilityInfo.addEventListener('reduceMotionChanged', setReduced);
    const show = Keyboard.addListener('keyboardDidShow', () => setKeyboard(true));
    const hide = Keyboard.addListener('keyboardDidHide', () => setKeyboard(false));
    return () => { mounted = false; motion.remove(); show.remove(); hide.remove(); };
  }, []);
  if (keyboard) return null;
  const current = state.routes[state.index]?.name;
  return <View style={[styles.base, { backgroundColor: c.bg, paddingBottom: Math.max(insets.bottom, 8),
    paddingLeft: Math.max(insets.left, 12), paddingRight: Math.max(insets.right, 12) }]}>
    <View accessibilityRole="tablist" style={[styles.dock, {
      backgroundColor: c.dark ? '#253338df' : '#ffffffd9', borderColor: c.dark ? '#5b737a88' : '#ffffff',
      shadowColor: c.dark ? '#000000' : '#30565d',
    }]}>
      {dockTabs.map(tab => {
        const route = state.routes.find(item => item.name === tab.name);
        if (!route) return null;
        const focused = current === tab.name;
        const selected = focused || (current && parentTab[current] === tab.name);
        return <DockItem key={route.key} title={tab.title} icon={tab.icon} reduced={reduced} selected={!!selected}
          onLongPress={() => navigation.emit({ type: 'tabLongPress', target: route.key })}
          onPress={() => {
            const event = navigation.emit({ type: 'tabPress', target: route.key, canPreventDefault: true });
            if (!focused && !event.defaultPrevented) navigation.navigate(route.name);
          }} />;
      })}
    </View>
  </View>;
}
function DockItem({ icon, title, reduced, selected, onPress, onLongPress }: {
  icon: IconName; title: string; selected: boolean; reduced: boolean; onPress: () => void; onLongPress: () => void;
}) {
  const c = usePalette();
  const [scale] = useState(() => new Animated.Value(1));
  useEffect(() => { if (reduced) { scale.stopAnimation(); scale.setValue(1); } }, [reduced, scale]);
  const animate = (toValue: number) => {
    if (!reduced) Animated.timing(scale, { toValue, duration: 125, useNativeDriver: true }).start();
  };
  return <Pressable accessibilityRole="tab" accessibilityLabel={title} accessibilityState={{ selected }}
    onPress={onPress} onPressIn={() => animate(0.88)} onPressOut={() => animate(1)} onLongPress={onLongPress} style={styles.item}>
    <Animated.View style={[styles.icon, {
      transform: [{ scale }], backgroundColor: selected ? c.tint : c.dark ? '#384950' : '#eef5f5',
    }]}>
      <Ionicons name={icon} size={22} color={selected ? c.accent : c.text} />
    </Animated.View>
    <Txt style={[styles.label, { color: selected ? c.text : c.muted }]}>{title}</Txt>
    <View style={[styles.dot, { backgroundColor: selected ? c.accent : 'transparent' }]} />
  </Pressable>;
}
const styles = StyleSheet.create({
  base: { alignItems: 'center', paddingTop: 4 },
  dock: { width: '100%', maxWidth: 374, flexDirection: 'row', borderWidth: 1, borderRadius: 25,
    paddingHorizontal: 6, paddingVertical: 5, shadowOpacity: 0.16, shadowRadius: 12, shadowOffset: { width: 0, height: 5 }, elevation: 5 },
  item: { flex: 1, minWidth: 44, minHeight: 57, alignItems: 'center', justifyContent: 'center', gap: 2 },
  icon: { width: 36, height: 36, borderRadius: 12, alignItems: 'center', justifyContent: 'center' },
  label: { fontSize: 11, lineHeight: 15, fontWeight: '700', textAlign: 'center' },
  dot: { width: 4, height: 4, borderRadius: 2 },
});
