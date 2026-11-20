import { ApiError, record, type Body, type Decoder } from './protocol.ts';

export function normalizeHost(input: string) {
  const host = input.trim().toLowerCase().replace(/^http:\/\//, '').replace(/\/$/, '').replace(/:80$/, '');
  const parts = host.split('.');
  const ipv4 = parts.length === 4 && parts.every(part => /^(0|[1-9][0-9]{0,2})$/.test(part) && Number(part) <= 255);
  const mdns = /^(?:[a-z\d](?:[a-z\d-]{0,61}[a-z\d])?\.)+local$/.test(host);
  if (!ipv4 && !mdns) throw new ApiError('Informe um IPv4 ou um endereço .local na porta 80.');
  return `http://${host}`;
}

export async function request<T>(host: string, path: string, decode: Decoder<T>, body?: Body,
  options: { signal?: AbortSignal; timeout?: number } = {}) {
  const controller = new AbortController();
  const cancel = () => controller.abort();
  options.signal?.addEventListener('abort', cancel, { once: true });
  if (options.signal?.aborted) cancel();
  const timeout = setTimeout(cancel, options.timeout ?? 4200);
  try {
    const response = await fetch(`${host}/api/${path}`, {
      method: body ? 'POST' : 'GET',
      headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
      body: body ? JSON.stringify(body) : undefined,
      redirect: 'error',
      signal: controller.signal,
    });
    if (!response.ok) throw new ApiError(`O Ciborgue respondeu com HTTP ${response.status}.`);
    let data: unknown;
    try { data = await response.json(); }
    catch {
      if (controller.signal.aborted) throw new ApiError('O Ciborgue demorou para responder.');
      throw new ApiError('O endereço retornou JSON inválido.');
    }
    if (!Array.isArray(data) && record(data).ok === false) throw new ApiError('O Ciborgue recusou a solicitação.');
    return decode(data);
  } catch (error) {
    if (error instanceof ApiError) throw error;
    if (controller.signal.aborted) throw new ApiError('O Ciborgue demorou para responder.');
    throw new ApiError('Ciborgue inacessível. Confira o Wi-Fi e o endereço.');
  } finally {
    clearTimeout(timeout);
    options.signal?.removeEventListener('abort', cancel);
  }
}
