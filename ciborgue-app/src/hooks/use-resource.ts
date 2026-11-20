import { useFocusEffect } from 'expo-router';
import { useCallback, useRef, useState } from 'react';
import type { Decoder } from '../api/protocol';
import { errorText, read, useConnectionValue } from '../stores/robot';

export function useResource<T>(path: string, decode: Decoder<T>) {
  const host = useConnectionValue(s => s.host);
  const online = useConnectionValue(s => s.online);
  const [entry, setEntry] = useState<{ host: string; value: T }>();
  const [error, setError] = useState('');
  const [pending, setPending] = useState(false);
  const current = useRef<AbortController | null>(null);

  const reload = useCallback(async () => {
    current.current?.abort();
    const controller = new AbortController();
    current.current = controller;
    setPending(true);
    setError('');
    try {
      const value = await read(path, decode, controller.signal);
      if (!controller.signal.aborted) setEntry({ host, value });
    } catch (e) {
      if (!controller.signal.aborted) setError(errorText(e));
    } finally {
      if (!controller.signal.aborted) setPending(false);
    }
  }, [decode, host, path]);

  useFocusEffect(useCallback(() => {
    if (online) void reload();
    return () => { current.current?.abort(); setPending(false); };
  }, [online, reload]));

  return {
    value: entry?.host === host ? entry.value : undefined,
    reload, error, pending,
    setValue(value: T) {
      current.current?.abort();
      setPending(false);
      setError('');
      setEntry({ host, value });
    },
  };
}
