import AsyncStorage from '@react-native-async-storage/async-storage';
import { createContext, useContext, useEffect, useState } from 'react';
import { useColorScheme } from 'react-native';

const dark = {
  bg: '#101518',
  surface: '#1a2227',
  text: '#f0f7f8',
  muted: '#a4b5bd',
  line: '#314047',
  accent: '#61deed',
  ink: '#052429',
  danger: '#ffb4a9',
  dangerTint: '#492723',
  tint: '#153c44',
  track: '#28444d',
  highlight: '#394a52',
  dockBg: '#253338df',
  dockBorder: '#5b737a88',
  dockShadow: '#000000',
  dockIconBg: '#384950',
  dark: true,
};
const light = {
  bg: '#f3f6f6',
  surface: '#ffffff',
  text: '#162326',
  muted: '#4e6268',
  line: '#dbe5e6',
  accent: '#007d8b',
  ink: '#ffffff',
  danger: '#a83225',
  dangerTint: '#fceae7',
  tint: '#d9f1f3',
  track: '#c5e5e9',
  highlight: '#ffffff',
  dockBg: '#ffffffd9',
  dockBorder: '#ffffff',
  dockShadow: '#30565d',
  dockIconBg: '#eef5f5',
  dark: false,
};

export const themes = { system: 'Sistema', light: 'Claro', dark: 'Escuro' };
type Preference = keyof typeof themes;
const key = 'ciborgue.theme';
export const ThemeContext = createContext<{ dark: boolean; preference: Preference; choose: (value: Preference) => Promise<void> } | null>(null);

export function useThemePreference() {
  const scheme = useColorScheme();
  const [preference, setPreference] = useState<Preference>('system');
  useEffect(() => {
    let active = true;
    void AsyncStorage.getItem(key).then(value => {
      if (active && (value === 'system' || value === 'light' || value === 'dark')) setPreference(value);
    }).catch(() => {});
    return () => { active = false; };
  }, []);
  return {
    preference, dark: preference === 'system' ? scheme === 'dark' : preference === 'dark',
    async choose(value: Preference) {
      await AsyncStorage.setItem(key, value);
      setPreference(value);
    },
  };
}
export const useTheme = () => useContext(ThemeContext);
export function usePalette() {
  const theme = useTheme();
  const scheme = useColorScheme();
  return (theme?.dark ?? scheme === 'dark') ? dark : light;
}
