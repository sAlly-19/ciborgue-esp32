from PySide6.QtCore import QObject, Signal


class BridgeSignals(QObject):
    """Barramento central de sinais Qt para comunicação segura entre threads e GUI."""
    state_changed = Signal(str)
    emotion_changed = Signal(str)
    led_changed = Signal(int, int, int, str)
    say_requested = Signal(str)             # Disparado quando o ESP32 envia EVT:SAY=<texto>
    speech_playback_started = Signal(str)   # Disparado no exato ms em que o áudio começa a sair no alto-falante
    telemetry_received = Signal(dict)
    log_message = Signal(str)
    serial_status = Signal(bool, str)
    mic_level_changed = Signal(int)
    conv_window_changed = Signal(float)
    audio_lipsync_frame = Signal(float, float)  # (amplitude 0..1, zcr 0..1)
