import { Ionicons } from '@expo/vector-icons';
import { Tabs } from 'expo-router';
import { StatusBar } from 'expo-status-bar';
import { useEffect } from 'react';
import { useWindowDimensions } from 'react-native';
import { Dock, dockTabs } from '../components/dock';
import { FeedbackProvider } from '../components/feedback';
import { ThemeContext, usePalette, useThemePreference } from '../components/theme';
import { startRobot } from '../stores/robot';

export default function Layout() {
  useEffect(startRobot, []);
  const theme = useThemePreference();
  return <ThemeContext.Provider value={theme}><FeedbackProvider><Navigation /></FeedbackProvider></ThemeContext.Provider>;
}
function Navigation() {
  const c = usePalette();
  const wide = useWindowDimensions().width >= 760;
  return <>
    <StatusBar style={c.dark ? 'light' : 'dark'} />
    <Tabs tabBar={wide ? undefined : props => <Dock {...props} />} screenOptions={{
      headerShown: false, sceneStyle: { backgroundColor: c.bg },
      tabBarPosition: wide ? 'left' : 'bottom',
      tabBarActiveTintColor: c.accent, tabBarInactiveTintColor: c.muted,
      tabBarStyle: { backgroundColor: c.surface, borderColor: c.line, elevation: 0 },
      tabBarLabelStyle: { fontSize: 12, fontWeight: '700' },
    }}>
      {dockTabs.map(({ name, title, icon }) => <Tabs.Screen key={name} name={name} options={{
        title, tabBarIcon: ({ color }) => <Ionicons name={icon} color={color} size={23} />,
      }} />)}
      {['expressoes', 'leds', 'memoria', 'avancado', 'bluetooth'].map(name => <Tabs.Screen key={name} name={name} options={{ href: null }} />)}
    </Tabs>
  </>;
}
