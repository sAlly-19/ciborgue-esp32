import { useEffect, useState } from 'react';
import { duration } from '../utils/format';
import { Txt } from './ui';

export function Countdown({ seconds, receivedAt }: { seconds: number; receivedAt: number }) {
  const [now, setNow] = useState(Date.now);
  useEffect(() => {
    if (!seconds) return;
    const id = setInterval(() => setNow(Date.now()), 350);
    return () => clearInterval(id);
  }, [seconds]);
  return <Txt style={{ fontSize: 29, lineHeight: 36, fontWeight: '800', fontVariant: ['tabular-nums'] }}>
    {duration(seconds - Math.max(0, Math.floor((now - receivedAt) / 1000)))}
  </Txt>;
}
