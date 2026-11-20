export const states = {
  BOOT: 'Inicialização', IDLE: 'Ocioso', LISTEN: 'Ouvindo', THINK: 'Pensando', SPEAK: 'Falando',
  ALERT: 'Alerta', CUSTOM: 'Personalizado', PORTAL: 'Portal',
};
export const legacyEmotions = {
  NEUTRAL: 'Neutro', HAPPY: 'Feliz', SAD: 'Triste', ANGRY: 'Bravo', SURPRISED: 'Surpreso', WINK: 'Piscadinha',
  LOVE: 'Apaixonado', SLEEPY: 'Sonolento', FEAR: 'Medo', DISGUST: 'Nojo', CONTEMPT: 'Desprezo',
  SHAME: 'Vergonha', DESPAIR: 'Desespero', MALICE: 'Travesso',
};
export const emotions = {
  ...legacyEmotions,
  CURIOUS: 'Curioso', BORED: 'Entediado', SILENT: 'Silencioso', ANGEL: 'Anjinho',
  COOL: 'Chavoso', CRAZY: 'Louco', LIAR: 'Mentiroso',
};
export const sounds = {
  boot: 'Inicialização', wake: 'Despertar', happy: 'Feliz', love: 'Carinho', sad: 'Triste', angry: 'Bravo',
  surprised: 'Surpreso', wink: 'Piscadinha', think: 'Pensando', alert: 'Alerta', pomo_focus: 'Foco',
  pomo_break: 'Pausa', sleep: 'Sono', offline: 'Offline', online: 'Online', idle_chirp: 'Chirp ocioso',
  fear: 'Medo', disgust: 'Nojo', contempt: 'Desprezo', shame: 'Vergonha', despair: 'Desespero', malice: 'Travesso',
};
export const effects = { SOLID: 'Fixo', PULSE: 'Pulsar', RAINBOW: 'Arco-íris', ALERT: 'Alerta' };
export const namedEffects = { SOLID: 'solido', PULSE: 'pulsar', RAINBOW: 'arco_iris', ALERT: 'alerta' };
export const colors: Record<string, [number, number, number]> = {
  vermelho: [255, 0, 0], verde: [0, 255, 20], azul: [0, 80, 255], amarelo: [255, 200, 0],
  roxo: [180, 0, 255], ciano: [0, 255, 255], laranja: [255, 70, 0], rosa: [255, 20, 140],
  branco: [220, 220, 220], desligar: [0, 0, 0],
};
export const days = ['Dom', 'Seg', 'Ter', 'Qua', 'Qui', 'Sex', 'Sáb'];
export const fixedCommands = [
  'PING', 'CMD:GET_STATUS', 'CMD:SLEEP', 'CMD:ECO', 'CMD:WAKE', 'CMD:PET', 'CMD:TFT_TEST',
  'CMD:PORTAL', 'CMD:PORTAL_STOP', 'CMD:AP_DEBUG', 'CMD:CLEAR_HISTORY', 'CMD:WIDGET_CLOSE',
  'CMD:TASK_CLEAR', 'CMD:MEM_CLEAR', 'CMD:ALARM_CLEAR', 'CMD:POMO_STOP', 'CMD:TIMER_STOP', 'CMD:MOOD_CARD',
];
