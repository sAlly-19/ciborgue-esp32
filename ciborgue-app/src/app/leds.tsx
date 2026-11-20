import { useRouter } from 'expo-router';
import { LedControls } from '../components/robot-controls';
import { Screen } from '../components/ui';

export default function Leds() {
  const router = useRouter();
  return <Screen title="LEDs" onBack={() => router.navigate('/')} backLabel="Voltar ao início"><LedControls /></Screen>;
}
