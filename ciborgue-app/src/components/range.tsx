import Slider from '@react-native-community/slider';
import type { ComponentProps } from 'react';
import { usePalette } from './theme';

export function Range(props: ComponentProps<typeof Slider>) {
  const c = usePalette();
  return <Slider minimumTrackTintColor={c.accent} maximumTrackTintColor={c.line} thumbTintColor={c.accent} {...props} />;
}
