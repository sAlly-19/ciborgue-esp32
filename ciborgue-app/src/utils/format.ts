export const pad = (n: number) => String(n).padStart(2, '0');
export function duration(seconds: number) {
  if (!Number.isFinite(seconds)) return '—';
  const s = Math.max(0, Math.floor(seconds));
  return `${pad(Math.floor(s / 3600))}:${pad(Math.floor(s / 60) % 60)}:${pad(s % 60)}`;
}
export const metric = (n: number, suffix = '') => Number.isFinite(n) ? `${n}${suffix}` : '—';
export const rgb = (values: number[]) => `rgb(${values.join(',')})`;
export const label = (labels: Record<string, string>, value: string) => labels[value] ?? value;
