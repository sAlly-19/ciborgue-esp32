import { act, create, type ReactTestRenderer } from 'react-test-renderer';
import type { ComponentProps } from 'react';
import { afterEach, expect, test, vi } from 'vitest';
import { Countdown } from '../src/components/countdown';
import { Dock } from '../src/components/dock';
import { FeedbackProvider } from '../src/components/feedback';
import { useConfirm } from '../src/components/feedback-context';
import { ExpressionControls } from '../src/components/robot-controls';
import Settings from '../src/app/configuracoes';
import Chat from '../src/app/chat';
import { Action, Card, ConnectionLine, Screen, Selection, Toggle, Txt } from '../src/components/ui';

vi.mock('react-native', () => ({
  Text: 'Text',
  TextInput: 'TextInput',
  View: 'View',
  Modal: 'Modal',
  ScrollView: 'ScrollView',
  RefreshControl: 'RefreshControl',
  Pressable: 'Pressable',
  Platform: { OS: 'web' },
  Animated: { View: 'AnimatedView', Value: class { stopAnimation() {} setValue() {} } },
  AccessibilityInfo: { isReduceMotionEnabled: async () => true, addEventListener: () => ({ remove() {} }) },
  Keyboard: { isVisible: () => false, addListener: (event: string, handler: () => void) => {
    keyboardListeners.set(event, handler);
    return { remove: () => keyboardListeners.delete(event) };
  } },
  ActivityIndicator: 'ActivityIndicator',
  PanResponder: {
    create: (config: unknown) => ({ panHandlers: { _panConfig: config } }),
  },
  StyleSheet: { create: (style: unknown) => style },
  useColorScheme: () => 'dark',
}));
vi.mock('expo-router', () => ({ useRouter: () => ({ navigate: vi.fn() }) }));
vi.mock('@expo/vector-icons', () => ({ Ionicons: 'Ionicons' }));
vi.mock('@react-native-async-storage/async-storage', () => ({ default: {} }));
vi.mock('@react-native-community/slider', () => ({ default: 'Slider' }));
vi.mock('expo-haptics', () => ({
  notificationAsync: async () => {},
  selectionAsync: async () => {},
  impactAsync: async () => {},
  NotificationFeedbackType: { Success: 'success', Error: 'error' },
  ImpactFeedbackStyle: { Light: 'light', Medium: 'medium', Heavy: 'heavy' },
}));
vi.mock('react-native-safe-area-context', () => ({ useSafeAreaInsets: () => ({ top: 0, bottom: 0, left: 0, right: 0 }) }));
let mockTelemetry: { data?: { timerTotal?: number; pomo?: boolean; ringing?: boolean }; receivedAt?: number } | null = null;
vi.mock('../src/stores/robot', () => ({
  useConnection: () => mockConnection,
  useConnectionValue: (select: (state: typeof mockConnection) => unknown) => select(mockConnection),
  useTelemetry: () => mockTelemetry,
  useRobot: (select: (state: typeof robotStatus) => unknown) => select(robotStatus),
  send: (...args: unknown[]) => sendCommand(...args),
}));

Object.assign(globalThis, { IS_REACT_ACT_ENVIRONMENT: true });
let mockConnection = { online: false, syncing: false, ready: true, error: 'Confira o Wi-Fi.' };
const robotStatus = { state: 'IDLE', emotion: 'NEUTRAL', revision: 2 };
const sendCommand = vi.fn();
const keyboardListeners = new Map<string, () => void>();
let component: ReactTestRenderer | undefined;

afterEach(() => {
  if (component) act(() => component?.unmount());
  component = undefined;
  vi.useRealTimers();
});

test('a linha de conexão mantém o estado online estável durante a consulta periódica', () => {
  act(() => { component = create(<ConnectionLine />); });
  expect(JSON.stringify(component?.toJSON())).toContain('Offline');
  mockConnection = { online: true, syncing: true, ready: true, error: '' };
  act(() => component?.update(<ConnectionLine />));
  expect(JSON.stringify(component?.toJSON())).toContain('Online');
  expect(JSON.stringify(component?.toJSON())).not.toContain('ActivityIndicator');
});

test('o cabeçalho fica fora da rolagem e mostra a conexão própria do Bluetooth', () => {
  const back = vi.fn();
  act(() => { component = create(<Screen title="Bluetooth" showConnection={false} onBack={back}
    status={{ label: 'Conectado', tone: 'active', icon: 'bluetooth-outline' }}><Txt>Brilho</Txt></Screen>); });
  const root = component!.root;
  const scroll = root.findByProps({ keyboardShouldPersistTaps: 'handled' });
  expect(scroll.findAllByProps({ accessibilityRole: 'header' })).toHaveLength(0);
  expect(root.findByProps({ accessibilityRole: 'header' }).props.children).toBe('Bluetooth');
  expect(JSON.stringify(component?.toJSON())).toContain('Conectado');
  act(() => root.findByProps({ accessibilityLabel: 'Voltar' }).props.onPress());
  expect(back).toHaveBeenCalledOnce();
});

test('o botão indisponível tem semântica acessível', () => {
  act(() => { component = create(<Action title="Salvar" disabled onPress={vi.fn()} />); });
  expect(component?.toJSON()).toMatchObject({ props: { accessibilityRole: 'button', accessibilityState: { disabled: true }, disabled: true } });
});

test('a prévia de expressão mantém o rascunho offline e só envia após Aplicar', async () => {
  mockConnection = { online: false, syncing: false, ready: true, error: '' };
  sendCommand.mockResolvedValue({ emotion: 'HAPPY' });
  act(() => { component = create(<ExpressionControls />); });
  const action = (title: string) => component!.root.findAllByType(Action).find(item => item.props.title === title)!;
  const happy = () => component!.root.findAllByType(Selection).find(item => item.props.label === 'Feliz')!;
  act(() => happy().props.onPress());
  expect(happy().props.checked).toBe(true);
  expect(action('Aplicar').props.disabled).toBe(true);
  expect(sendCommand).not.toHaveBeenCalled();
  mockConnection = { ...mockConnection, online: true };
  act(() => component?.update(<ExpressionControls />));
  expect(happy().props.checked).toBe(true);
  await act(async () => action('Aplicar').props.onPress());
  expect(sendCommand).toHaveBeenCalledExactlyOnceWith('emotion', { emotion: 'HAPPY' });
});

test('os controles personalizados expõem papéis e estados de acessibilidade', () => {
  const onChange = vi.fn();
  act(() => { component = create(<>
    <Toggle label="Sons" value={false} onChange={onChange} />
    <Selection label="Seg" kind="checkbox" checked onPress={() => {}} />
    <Selection label="Claro" kind="radio" checked={false} onPress={() => {}} />
  </>); });
  const find = (role: string) => component!.root.findByProps({ accessibilityRole: role });
  expect(find('checkbox').props.accessibilityState.checked).toBe(true);
  expect(find('radio').props.accessibilityState.checked).toBe(false);
  act(() => find('switch').props.onPress());
  expect(onChange).toHaveBeenCalledWith(true);
});

test('o diálogo do app executa a ação apenas após confirmação', () => {
  const onDelete = vi.fn();
  function Example() {
    const confirm = useConfirm('danger');
    return <Action title="Excluir" onPress={() => confirm('Excluir registro', 'Apagar o registro?', onDelete)} />;
  }
  act(() => { component = create(<FeedbackProvider><Example /></FeedbackProvider>); });
  const press = (title: string) => act(() => component!.root.findAllByType(Action).find(item => item.props.title === title)!.props.onPress());
  press('Excluir');
  expect(onDelete).not.toHaveBeenCalled();
  press('Cancelar');
  expect(onDelete).not.toHaveBeenCalled();
  press('Excluir');
  press('Confirmar');
  expect(onDelete).toHaveBeenCalledTimes(1);
});

test('o Dock seleciona o destino pai e desaparece com o teclado aberto', async () => {
  const routes = ['index', 'tempo', 'chat', 'configuracoes', 'memoria'].map(name => ({ key: name, name }));
  const navigate = vi.fn();
  const props = {
    state: { index: 4, routes },
    navigation: { navigate, emit: () => ({ defaultPrevented: false }) },
  } as unknown as ComponentProps<typeof Dock>;
  await act(async () => { component = create(<Dock {...props} />); });
  const tab = (label: string) => component!.root.findByProps({ accessibilityRole: 'tab', accessibilityLabel: label });
  expect(tab('Rotina').props.accessibilityState.selected).toBe(true);
  act(() => tab('Início').props.onPress());
  expect(navigate).toHaveBeenCalledWith('index');
  act(() => keyboardListeners.get('keyboardDidShow')?.());
  expect(component?.toJSON()).toBe(null);
  act(() => keyboardListeners.get('keyboardDidHide')?.());
  expect(tab('Rotina').props.accessibilityState.selected).toBe(true);
});

test('a contagem desce com o relógio e nunca fica negativa', () => {
  vi.useFakeTimers();
  vi.setSystemTime(100_000);
  act(() => { component = create(<Countdown seconds={2} receivedAt={100_000} />); });
  expect(JSON.stringify(component?.toJSON())).toContain('00:00:02');
  act(() => { vi.advanceTimersByTime(3500); });
  expect(JSON.stringify(component?.toJSON())).toContain('00:00:00');
});

test('a tela de ajustes permite filtrar cards por categoria de forma organizada', () => {
  act(() => { component = create(<Settings />); });
  const titles = () => component!.root.findAllByType(Card).map(card => card.props.title);

  const redeBtn = component!.root.findAllByType(Selection).find(item => item.props.label.includes('Rede'))
    ?? component!.root.findAllByType(Action).find(item => item.props.title?.includes('Rede'));
  expect(redeBtn).toBeDefined();

  act(() => redeBtn!.props.onPress());
  expect(titles()).toContain('Endereço do Ciborgue');
  expect(titles()).toContain('Wi-Fi do robô');
  expect(titles()).not.toContain('Áudio');

  const dispBtn = component!.root.findAllByType(Selection).find(item => item.props.label.includes('Dispositivo'))
    ?? component!.root.findAllByType(Action).find(item => item.props.title?.includes('Dispositivo'));
  act(() => dispBtn!.props.onPress());
  expect(titles()).toContain('Áudio');
  expect(titles()).toContain('Tela');
  expect(titles()).not.toContain('Wi-Fi do robô');
});

test('o Dock exibe indicador de atividade na aba Rotina quando houver timer ou alarme ativo', async () => {
  mockTelemetry = { data: { timerTotal: 120 } };
  const routes = ['index', 'tempo', 'chat', 'configuracoes'].map(name => ({ key: name, name }));
  const props = {
    state: { index: 0, routes },
    navigation: { navigate: vi.fn(), emit: () => ({ defaultPrevented: false }) },
  } as unknown as ComponentProps<typeof Dock>;
  await act(async () => { component = create(<Dock {...props} />); });
  const tempoTab = component!.root.findByProps({ accessibilityRole: 'tab', accessibilityLabel: 'Rotina (Atividade ativa)' });
  expect(tempoTab).toBeDefined();
  mockTelemetry = null;
});

test('o Chat exibe chips deslizantes de atalho que preenchem a mensagem ao tocar', () => {
  act(() => { component = create(<FeedbackProvider><Chat /></FeedbackProvider>); });
  const scroll = component!.root.findByProps({ accessibilityLabel: 'Sugestões de perguntas rápidas' });
  expect(scroll.props.horizontal).toBe(true);

  const chip = component!.root.findAllByProps({ accessibilityRole: 'button' }).find(item => item.props.accessibilityLabel === 'Como você está?');
  expect(chip).toBeDefined();

  act(() => chip!.props.onPress());
  const input = component!.root.findByProps({ accessibilityLabel: 'Mensagem' });
  expect(input.props.value).toBe('Como você está?');
});

test('o Screen suporta gesto de deslizar da borda esquerda para voltar', () => {
  const onBack = vi.fn();
  act(() => {
    component = create(<Screen title="Detalhes" onBack={onBack}><Txt>Conteúdo</Txt></Screen>);
  });
  const root = component!.root.children[0] as unknown as { props: { _panConfig?: {
    onMoveShouldSetPanResponder?: (e: unknown, g: { x0: number; dx: number; dy: number }) => boolean;
    onPanResponderRelease?: (e: unknown, g: { dx: number }) => void;
  } } };
  const panConfig = root.props._panConfig;
  expect(panConfig).toBeDefined();
  expect(panConfig!.onMoveShouldSetPanResponder!(null, { x0: 20, dx: 35, dy: 5 })).toBe(true);
  expect(panConfig!.onMoveShouldSetPanResponder!(null, { x0: 120, dx: 35, dy: 5 })).toBe(false);

  panConfig!.onPanResponderRelease!(null, { dx: 80 });
  expect(onBack).toHaveBeenCalledOnce();
});

