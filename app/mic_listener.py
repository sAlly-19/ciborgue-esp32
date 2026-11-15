import os
import time
import math
import array
import base64
import subprocess
import threading
import requests
import sounddevice as sd
import speech_recognition as sr
import speech_recognition.audio as sr_audio

from app.config import WAKE_WORDS
from app.signals import BridgeSignals
from app.sfx import play_sfx_async


# Corrige [WinError 50] no Windows ao executar flac-win32.exe em subprocessos GUI sem console stderr
def _safe_get_flac_data(self, convert_rate=None, convert_width=None):
    wav_data = self.get_wav_data(convert_rate, convert_width)
    base_path = os.path.dirname(os.path.abspath(sr_audio.__file__))
    flac_converter = os.path.join(base_path, "flac-win32.exe")
    creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0x08000000)
    process = subprocess.Popen(
        [flac_converter, "--stdout", "--totally-silent", "--best", "-"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        creationflags=creationflags,
    )
    flac_data, _ = process.communicate(wav_data)
    return flac_data


sr.AudioData.get_flac_data = _safe_get_flac_data


def list_input_microphones():
    """Retorna lista de tuplas (device_index_ou_None, nome_amigavel) para os microfones do Windows."""
    result = [(None, "⭐ Microfone Padrão do Windows")]
    try:
        devices = sd.query_devices()
        seen_names = set()
        for idx, d in enumerate(devices):
            if d.get("max_input_channels", 0) > 0:
                name = d.get("name", f"Device {idx}").strip()
                if "mapeador de som" in name.lower() or "driver de captura" in name.lower():
                    continue
                if name not in seen_names:
                    seen_names.add(name)
                    result.append((idx, f"[{idx}] {name}"))
    except Exception:
        pass
    return result


class MicrophoneListener:
    """
    Capturador de microfone em tempo real via sounddevice (16kHz PCM 16-bit),
    com VU Meter ao vivo, Wake Word ('Ciborgue'), Janela de Conversa Contínua (8s)
    e Fallback Automático de Transcrição via Gemini WAV.
    """
    def __init__(self, signals: BridgeSignals, send_serial_fn, is_tts_busy_fn, get_api_key_fn):
        self.signals = signals
        self.send_serial = send_serial_fn
        self.is_tts_busy = is_tts_busy_fn
        self.get_api_key = get_api_key_fn

        self.selected_device = None
        self.reopen_requested = False
        self.force_listen_now = False
        self.continuous_window_until = 0.0
        self.running = True

    def start(self):
        threading.Thread(target=self._worker_loop, daemon=True).start()

    def stop(self):
        self.running = False

    def change_device(self, device_id):
        self.selected_device = device_id
        self.reopen_requested = True

    def trigger_push_to_talk(self):
        self.force_listen_now = True
        self.continuous_window_until = time.time() + 10.0
        self.signals.conv_window_changed.emit(self.continuous_window_until)
        self.send_serial("CMD:STATE=LISTEN")
        play_sfx_async("wake")

    def set_continuous_window(self, until_ts: float):
        self.continuous_window_until = until_ts

    def _worker_loop(self):
        recognizer = sr.Recognizer()
        sample_rate = 16000
        block_samples = 1200  # 75ms por bloco para detecção de fim de fala mais rápida

        while self.running:
            self.reopen_requested = False
            dev_id = self.selected_device

            try:
                with sd.RawInputStream(
                    samplerate=sample_rate,
                    blocksize=block_samples,
                    device=dev_id,
                    channels=1,
                    dtype="int16"
                ) as stream:
                    self.signals.log_message.emit(
                        "🎤 <b>Microfone pronto!</b> Diga <b>'Ciborgue'</b> (ou clique em '🎤 Ouvir Agora')."
                    )

                    noise_floor = 180.0
                    recording_chunks = []
                    is_speaking_user = False
                    silence_blocks = 0

                    while self.running and not self.reopen_requested:
                        data, _ = stream.read(block_samples)
                        raw_bytes = bytes(data)

                        # Enquanto o robô fala ou sintetiza fala, ignora o microfone
                        if self.is_tts_busy():
                            self.signals.mic_level_changed.emit(0)
                            recording_chunks.clear()
                            is_speaking_user = False
                            continue

                        samples = array.array("h", raw_bytes)
                        rms = math.sqrt(sum(s * s for s in samples) / len(samples)) if samples else 0.0

                        ui_level = min(100, int((rms / 2200.0) * 100))
                        self.signals.mic_level_changed.emit(ui_level)

                        if not is_speaking_user:
                            noise_floor = 0.96 * noise_floor + 0.04 * min(rms, 600.0)

                        threshold = max(230.0, noise_floor * 2.3)

                        if rms > threshold:
                            if not is_speaking_user:
                                is_speaking_user = True
                                silence_blocks = 0
                            recording_chunks.append(raw_bytes)
                            silence_blocks = 0
                            if len(recording_chunks) >= 75:
                                self._process_utterance(recognizer, b"".join(recording_chunks), sample_rate)
                                recording_chunks.clear()
                                is_speaking_user = False
                        else:
                            if is_speaking_user:
                                recording_chunks.append(raw_bytes)
                                silence_blocks += 1
                                if silence_blocks >= 6:
                                    if len(recording_chunks) >= 6:
                                        self._process_utterance(
                                            recognizer, b"".join(recording_chunks), sample_rate
                                        )
                                    recording_chunks.clear()
                                    is_speaking_user = False
                                    silence_blocks = 0

            except Exception as e:
                self.signals.log_message.emit(f"⚠️ Erro no microfone ({e}). Tentando novamente em 3s...")
                time.sleep(3.0)

    def _transcribe_fallback_gemini(self, audio_data: sr.AudioData) -> str:
        key = self.get_api_key()
        if not key:
            return ""
        wav_b64 = base64.b64encode(audio_data.get_wav_data()).decode("ascii")
        payload = {
            "contents": [{
                "parts": [
                    {"text": "Transcreva exatamente o que foi falado neste áudio em português do Brasil. Retorne apenas a frase transcrita, sem comentários. Se houver apenas silêncio ou ruído, retorne vazio."},
                    {"inline_data": {"mime_type": "audio/wav", "data": wav_b64}}
                ]
            }]
        }
        for model_name in ("gemini-3.5-flash-lite", "gemini-3.1-flash-lite", "gemini-3-flash-preview"):
            url = f"https://generativelanguage.googleapis.com/v1beta/models/{model_name}:generateContent?key={key}"
            r = requests.post(url, json=payload, timeout=6)
            if r.status_code == 200:
                parts = r.json().get("candidates", [{}])[0].get("content", {}).get("parts", [])
                for p in parts:
                    if "text" in p:
                        return p["text"].strip()
            elif r.status_code not in (429, 500, 503):
                break
        return ""

    def _process_utterance(self, recognizer: sr.Recognizer, pcm_bytes: bytes, sample_rate: int):
        try:
            audio_data = sr.AudioData(pcm_bytes, sample_rate, 2)
            try:
                texto = recognizer.recognize_google(audio_data, language="pt-BR").strip()
            except sr.UnknownValueError:
                return
            except Exception:
                texto = self._transcribe_fallback_gemini(audio_data)
            if not texto:
                return

            texto_lower = texto.lower()
            now = time.time()
            in_continuous_mode = (now < self.continuous_window_until) or self.force_listen_now
            wake_matched = any(w in texto_lower for w in WAKE_WORDS)

            if wake_matched or in_continuous_mode:
                self.force_listen_now = False
                play_sfx_async("wake")

                comando = texto_lower
                for w in WAKE_WORDS:
                    comando = comando.replace(w, "").strip(" ,.!?")

                if len(comando) <= 2 and wake_matched:
                    self.continuous_window_until = time.time() + 9.0
                    self.signals.conv_window_changed.emit(self.continuous_window_until)
                    self.send_serial("CMD:STATE=LISTEN")
                    self.signals.say_requested.emit("Oi! Estou te ouvindo, pode mandar!")
                    return

                final_cmd = comando if wake_matched else texto
                self.signals.log_message.emit(f"<b style='color:#a7f3d0'>🎤 VOCÊ (Voz):</b> {final_cmd}")
                self.send_serial(f"MIC:{final_cmd}")
            else:
                self.signals.log_message.emit(
                    f"<span style='color:#64748b'>[Voz ignorada sem Wake Word: \"{texto}\" — diga 'Ciborgue' antes ou clique em '🎤 Ouvir Agora']</span>"
                )
        except Exception as e:
            self.signals.log_message.emit(f"⚠️ Erro no reconhecimento de voz: {e}")
