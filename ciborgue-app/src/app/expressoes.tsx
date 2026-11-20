import { useRouter } from 'expo-router';
import { ExpressionControls } from '../components/robot-controls';
import { Screen } from '../components/ui';

export default function Expressions() {
  const router = useRouter();
  return <Screen title="Expressões" onBack={() => router.navigate('/')} backLabel="Voltar ao início"><ExpressionControls /></Screen>;
}
