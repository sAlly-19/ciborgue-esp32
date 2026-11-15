import math
import array
import threading
import sounddevice as sd


def synthesize_robot_sfx(kind: str, sample_rate: int = 24000) -> bytes:
    """Gera chirps e bipes musicais expressivos em PCM 16-bit usando síntese FM suave."""
    if kind == "wake":
        notes = [(520, 780, 0.07), (820, 1280, 0.11)]
    elif kind == "happy":
        notes = [(587, 659, 0.06), (784, 987, 0.06), (1046, 1318, 0.10)]
    elif kind == "love":
        notes = [(523, 659, 0.08), (659, 784, 0.08), (784, 1174, 0.14)]
    elif kind == "sad":
        notes = [(620, 440, 0.14), (430, 290, 0.20)]
    elif kind == "angry":
        notes = [(220, 160, 0.10), (190, 140, 0.14)]
    elif kind == "surprised":
        notes = [(440, 1380, 0.14)]
    elif kind == "wink":
        notes = [(660, 990, 0.06), (990, 1480, 0.09)]
    elif kind == "think":
        notes = [(480, 620, 0.05), (720, 540, 0.05), (640, 860, 0.06)]
    else:
        notes = [(600, 880, 0.08)]

    buf = array.array("h")
    for f0, f1, dur in notes:
        n_samples = int(sample_rate * dur)
        phase = 0.0
        for i in range(n_samples):
            t = i / max(1, n_samples)
            freq = f0 + (f1 - f0) * (t ** 0.85)
            freq += math.sin(t * math.pi * 6.0) * 18.0
            phase += (2.0 * math.pi * freq) / sample_rate
            env = math.sin(t * math.pi) ** 0.65
            val = math.sin(phase) + 0.25 * math.sin(phase * 2.0)
            sample = int(max(-32000, min(32000, val * env * 6500)))
            buf.append(sample)
        for _ in range(int(sample_rate * 0.015)):
            buf.append(0)
    return buf.tobytes()


def play_sfx_async(kind: str):
    """Reproduz efeito sonoro de robô em background sem bloquear a interface."""
    def _run():
        try:
            pcm = synthesize_robot_sfx(kind, 24000)
            with sd.RawOutputStream(samplerate=24000, channels=1, dtype="int16") as out:
                out.write(pcm)
        except Exception:
            pass
    threading.Thread(target=_run, daemon=True).start()
