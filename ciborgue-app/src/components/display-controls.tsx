import { useState } from 'react';
import { ApiError } from '../api/protocol';
import { setDisplay, useRobot } from '../stores/robot';
import { Range } from './range';
import { Action, Choices, Notice, Toggle, Txt, useAction, useDisabled } from './ui';

const rotations = { '0': 'Posição 1', '1': 'Posição 2', '2': 'Posição 3', '3': 'Posição 4' };
type Adjustment = { rotation: number } | { inverted: boolean } | { brightness: number };

export function DisplayControls() {
  const supported = useRobot(s => s?.capabilities?.display);
  const display = useRobot(s => s?.display);
  const configuredBrightness = useRobot(s => s?.configuredBrightness);
  const disabled = useDisabled();
  const a = useAction();
  const [draft, setDraft] = useState<number>();
  const [retry, setRetry] = useState<Adjustment>();

  function apply(change: Adjustment) {
    setRetry(change);
    void a.run(async () => {
      const confirmed = await setDisplay(change);
      if (('brightness' in change && change.brightness !== confirmed.brightness) ||
          ('rotation' in change && change.rotation !== confirmed.rotation) ||
          ('inverted' in change && change.inverted !== confirmed.inverted)) {
        throw new ApiError('O robô não confirmou o ajuste de tela.');
      }
      setDraft(undefined);
      setRetry(undefined);
    }, 'Ajuste de tela aplicado.');
  }

  if (!supported || !display) {
    return <Txt muted>{supported === undefined ? 'Conecte o Ciborgue para consultar a tela.' : 'A placa conectada não oferece calibração de tela pela API.'}</Txt>;
  }

  return <>
    <Txt muted>{configuredBrightness == null ? 'Os ajustes valem até reiniciar.' : 'O brilho é salvo no robô. Rotação e inversão valem até reiniciar.'} Eco e sono ajustam o brilho efetivo.</Txt>
    <Txt>Rotação</Txt>
    <Choices items={rotations} selected={String(display.rotation)} disabled={disabled || a.pending}
      onSelect={rotation => apply({ rotation: Number(rotation) })} />
    <Toggle label="Inverter cores" value={display.inverted} disabled={disabled || a.pending}
      onChange={inverted => apply({ inverted })} />
    <Txt>Brilho · {Math.round((draft ?? configuredBrightness ?? display.brightness) * 100 / 255)}%</Txt>
    {configuredBrightness != null && configuredBrightness !== display.brightness &&
      <Txt muted>Efetivo agora · {Math.round(display.brightness * 100 / 255)}%</Txt>}
    <Range accessibilityLabel="Brilho da tela" minimumValue={0} maximumValue={255} step={1} value={draft ?? configuredBrightness ?? display.brightness}
      disabled={disabled || a.pending} style={{ height: 44 }} onValueChange={setDraft}
      onSlidingComplete={brightness => apply({ brightness })} />
    <Notice text={a.notice} failed={a.failed} />
    {retry && a.failed && <Action title="Tentar novamente" disabled={disabled} pending={a.pending} onPress={() => apply(retry)} />}
  </>;
}
