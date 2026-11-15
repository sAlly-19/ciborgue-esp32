import os
import io
import re
import time
import math
import wave
import array
import base64
import queue
import asyncio
import threading
import concurrent.futures
import requests
import edge_tts
import miniaudio
import sounddevice as sd
import win32com.client
import pythoncom

from app.config import EMOTION_SPEECH_STYLES, EDGE_EMOTION_PROSODY
from app.signals import BridgeSignals


class AudioTtsManager:
    """
    Gerenciador de Voz Expressiva de Baixa Latência com Pipeline de Cláusulas:
    - Divide respostas longas na primeira pontuação para tocar a 1ª frase em ~0.8s
      enquanto sintetiza o restante em paralelo em background.
    - Suporta Neural Ultra-Rápido (Edge-TTS + miniaudio PCM 24kHz) e Gemini 3.8 TTS
      com cooldown automático caso a cota do Gemini 3.8 retorne HTTP 429.
    - O estado SPEAK, a legenda e o LipSync só iniciam no exato milissegundo em que
      o primeiro pacote de áudio é enviado ao alto-falante.
    """
    def __init__(self, signals: BridgeSignals, send_serial_fn, get_api_key_fn, get_emotion_fn):
        self.signals = signals
        self.send_serial = send_serial_fn
        self.get_api_key = get_api_key_fn
        self.get_emotion = get_emotion_fn

        self.tts_queue = queue.Queue()
        self.selected_voice = os.getenv("GEMINI_VOICE", "Edge-Antonio")
        self.running = True
        self.is_playing = False
        self.http_session = requests.Session()
        self.gemini_tts_cooldown_until = 0.0

    def start(self):
        threading.Thread(target=self._worker_loop, daemon=True).start()

    def stop(self):
        self.running = False
        self.tts_queue.put(None)

    def enqueue(self, text: str):
        if text and text.strip():
            self.tts_queue.put(text.strip())

    def is_busy(self) -> bool:
        return self.is_playing or (not self.tts_queue.empty())

    def _split_into_fast_clauses(self, text: str) -> list[str]:
        """Divide na primeira pontuação para tocar a 1ª frase imediatamente (~0.8s)."""
        text = text.strip()
        if len(text) <= 55:
            return [text]
        parts = re.split(r"(?<=[.!?])\s+", text, maxsplit=1)
        clauses = [p.strip() for p in parts if p and p.strip()]
        return clauses if clauses else [text]

    def _synthesize_edge_neural_wav(self, text: str, female: bool = False) -> bytes | None:
        emotion = self.get_emotion()
        rate, pitch = EDGE_EMOTION_PROSODY.get(emotion, ("+14%", "+5Hz"))
        voice_id = "pt-BR-FranciscaNeural" if female else "pt-BR-AntonioNeural"

        async def _fetch_mp3():
            comm = edge_tts.Communicate(text, voice_id, rate=rate, pitch=pitch)
            buf = bytearray()
            async for chunk in comm.stream():
                if chunk["type"] == "audio":
                    buf.extend(chunk["data"])
            return bytes(buf)

        try:
            mp3_bytes = asyncio.run(_fetch_mp3())
            if not mp3_bytes:
                return None
            decoded = miniaudio.decode(
                mp3_bytes,
                output_format=miniaudio.SampleFormat.SIGNED16,
                nchannels=1,
                sample_rate=24000,
            )
            wav_io = io.BytesIO()
            with wave.open(wav_io, "wb") as wf:
                wf.setnchannels(1)
                wf.setsampwidth(2)
                wf.setframerate(24000)
                wf.writeframes(decoded.samples.tobytes())
            return wav_io.getvalue()
        except Exception as e:
            self.signals.log_message.emit(f"⚠️ Aviso Edge Neural TTS: {e}")
            return None

    def _synthesize_clause_wav(self, clause: str) -> bytes | None:
        voice_name = self.selected_voice or "Edge-Antonio"

        # 1. Modo Neural Ultra-Rápido (~0.8s, sem limite de cota)
        if voice_name.startswith("Edge-"):
            is_female = "Francisca" in voice_name
            return self._synthesize_edge_neural_wav(clause, female=is_female)

        # 2. Se o Gemini 3.8 TTS estiver em cooldown por HTTP 429 recente, vai direto pro Neural Ultra-Rápido
        is_female_gemini = voice_name in ("Kore", "Aoede")
        if time.time() < self.gemini_tts_cooldown_until:
            return self._synthesize_edge_neural_wav(clause, female=is_female_gemini)

        key = self.get_api_key()
        if not key:
            return self._synthesize_edge_neural_wav(clause, female=is_female_gemini)

        emotion = self.get_emotion()
        style = EMOTION_SPEECH_STYLES.get(emotion, "cheerful and friendly")
        url = f"https://generativelanguage.googleapis.com/v1beta/interactions?key={key}"
        payload = {
            "model": "gemini-3.8-flash-tts",
            "input": [{
                "type": "user_input",
                "content": [{
                    "type": "text",
                    "text": clause,
                    "annotations": [{
                        "type": "speech_metadata",
                        "style": style
                    }]
                }]
            }],
            "response_format": {"type": "audio"},
            "generation_config": {
                "speech_config": [{"voice": voice_name}]
            }
        }
        try:
            r = self.http_session.post(url, json=payload, timeout=4.5)
            if r.status_code == 200:
                data = r.json()
                for step in data.get("steps", []):
                    for item in step.get("content", []):
                        if item.get("type") == "audio" and item.get("data"):
                            return base64.b64decode(item["data"])
            elif r.status_code == 429:
                # Ativa cooldown de 60s para não desperdiçar tempo na próxima frase
                self.gemini_tts_cooldown_until = time.time() + 60.0
        except Exception:
            pass

        # Fallback imediato para Neural PT-BR (~0.8s)
        return self._synthesize_edge_neural_wav(clause, female=is_female_gemini)

    def _play_wav_stream(self, wav_bytes: bytes, full_text: str, is_first_clause: bool):
        with wave.open(io.BytesIO(wav_bytes), "rb") as wf:
            sr_rate = wf.getframerate()
            channels = wf.getnchannels()
            chunk_frames = max(256, int(sr_rate * 0.032))  # Blocos de 32ms

            with sd.RawOutputStream(samplerate=sr_rate, channels=channels, dtype="int16") as out_stream:
                first_chunk = is_first_clause
                while self.running:
                    pcm_chunk = wf.readframes(chunk_frames)
                    if not pcm_chunk:
                        break

                    if first_chunk:
                        first_chunk = False
                        self.send_serial("CMD:STATE=SPEAK")
                        self.signals.speech_playback_started.emit(full_text)

                    samples = array.array("h", pcm_chunk)
                    n = len(samples)
                    if n > 0:
                        rms = math.sqrt(sum(s * s for s in samples) / n)
                        amp = min(1.0, (rms / 6800.0) ** 0.72)

                        zc = 0
                        prev = samples[0]
                        for s in samples[1:]:
                            if (s >= 0 > prev) or (s < 0 <= prev):
                                zc += 1
                            prev = s
                        zcr = min(1.0, (zc / n) * 4.2)
                        self.signals.audio_lipsync_frame.emit(amp, zcr)

                    out_stream.write(pcm_chunk)

    def _worker_loop(self):
        pythoncom.CoInitialize()
        fallback_speaker = None
        try:
            fallback_speaker = win32com.client.Dispatch("SAPI.SpVoice")
            for v in fallback_speaker.GetVoices():
                if "Portuguese" in v.GetDescription() or "Maria" in v.GetDescription():
                    fallback_speaker.Voice = v
                    break
            fallback_speaker.Rate = 2
        except Exception:
            pass

        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            while self.running:
                text = self.tts_queue.get()
                if text is None:
                    break

                self.is_playing = True
                try:
                    clauses = self._split_into_fast_clauses(text)
                    futures = [pool.submit(self._synthesize_clause_wav, c) for c in clauses]

                    played_any = False
                    for idx, fut in enumerate(futures):
                        wav_bytes = fut.result(timeout=8.0)
                        if wav_bytes:
                            self._play_wav_stream(wav_bytes, text, is_first_clause=(not played_any))
                            played_any = True

                    if not played_any and fallback_speaker:
                        self.send_serial("CMD:STATE=SPEAK")
                        self.signals.speech_playback_started.emit(text)
                        fallback_speaker.Speak(text)
                except Exception as e:
                    self.signals.log_message.emit(f"⚠️ Erro ao reproduzir voz: {e}")
                finally:
                    self.is_playing = False
                    self.signals.audio_lipsync_frame.emit(0.0, 0.2)
                    conv_until = time.time() + 8.0
                    self.signals.conv_window_changed.emit(conv_until)
                    self.send_serial("TTS:DONE")
