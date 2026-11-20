import time
import math
import random
from PySide6.QtCore import Qt, QTimer, QRectF, QPointF
from PySide6.QtGui import (
    QPainter, QColor, QPen, QBrush, QFont, QPainterPath,
    QLinearGradient, QRadialGradient
)
from PySide6.QtWidgets import QWidget

from app.config import SCREEN_LOGICAL_W, SCREEN_LOGICAL_H, SCREEN_PREVIEW_SCALE
from app.physics import SpringValue, clamp01, ease_out_quad, ease_out_back
from app.sfx import play_sfx_async
from app.widgets_overlay import WidgetsOverlayRenderer


_BASE_POSE = {
    "w_scale": 1.0,
    "lh_scale": 1.0,
    "rh_scale": 1.0,
    "l_top_l": 0.0,
    "l_top_r": 0.0,
    "r_top_l": 0.0,
    "r_top_r": 0.0,
    "fear_corner": 0.0,
    "bot_arc": 0.0,
    "l_bot_l": 0.0,
    "l_bot_r": 0.0,
    "r_bot_l": 0.0,
    "r_bot_r": 0.0,
    "shake_amp": 0.0,
    "blush_alpha": 0.0,
    "blush_intensity": 1.0,
    "blush_y_off": 44.0,
    "sweat_alpha": 0.0,
    "sweat_scale": 1.0,
    "gaze_x": 0.0,
    "gaze_y": 0.0,
    "tilt": 0.0,
    "squash_imp": 0.14,
    "allow_idle_gaze": False,
}


def _pose(**overrides) -> dict:
    p = dict(_BASE_POSE)
    p.update(overrides)
    return p


EMOTION_POSES = {
    "NEUTRAL": _pose(allow_idle_gaze=True, squash_imp=0.14),
    "HAPPY": _pose(
        bot_arc=1.0, blush_alpha=1.0, blush_intensity=1.0, blush_y_off=44.0,
        squash_imp=0.20, allow_idle_gaze=True
    ),
    "LOVE": _pose(
        blush_alpha=1.0, blush_intensity=1.0, blush_y_off=44.0,
        tilt=5.5, squash_imp=0.22, allow_idle_gaze=True
    ),
    "SURPRISED": _pose(
        w_scale=1.08, lh_scale=1.12, rh_scale=1.12,
        squash_imp=0.32, allow_idle_gaze=True
    ),
    "WINK": _pose(rh_scale=6.0 / 70.0, tilt=-7.0, squash_imp=0.18),
    "SAD": _pose(
        l_top_l=0.45, l_top_r=0.06, r_top_l=0.06, r_top_r=0.45,
        gaze_y=7.0, tilt=-4.0, squash_imp=-0.08
    ),
    "ANGRY": _pose(
        l_top_l=0.06, l_top_r=0.46, r_top_l=0.46, r_top_r=0.06,
        shake_amp=1.8, squash_imp=-0.10
    ),
    "SLEEPY": _pose(
        lh_scale=0.35, rh_scale=0.35, gaze_y=5.0, tilt=4.0, squash_imp=-0.12
    ),
    "FEAR": _pose(
        w_scale=0.96, lh_scale=1.10, rh_scale=1.10, fear_corner=1.0,
        shake_amp=1.5, sweat_alpha=1.0, sweat_scale=1.0,
        gaze_y=-4.0, tilt=-3.0, squash_imp=0.24
    ),
    "DISGUST": _pose(
        w_scale=0.94, lh_scale=0.50, rh_scale=0.81,
        l_top_l=0.18, l_top_r=0.42, r_top_l=0.12, r_top_r=0.25,
        l_bot_l=0.10, l_bot_r=0.24, r_bot_l=0.20, r_bot_r=0.08,
        gaze_x=-9.0, gaze_y=-4.0, tilt=3.5, squash_imp=-0.06
    ),
    "CONTEMPT": _pose(
        w_scale=1.00, lh_scale=0.92, rh_scale=0.72,
        l_top_l=0.42, l_top_r=0.27, r_top_l=0.47, r_top_r=0.47,
        l_bot_l=0.02, l_bot_r=0.04, r_bot_l=0.06, r_bot_r=0.06,
        gaze_x=13.0, gaze_y=-1.0, tilt=-3.0, squash_imp=-0.05
    ),
    "SHAME": _pose(
        w_scale=0.90, lh_scale=0.80, rh_scale=0.80,
        l_top_l=0.28, l_top_r=0.06, r_top_l=0.06, r_top_r=0.28,
        l_bot_l=0.10, l_bot_r=0.10, r_bot_l=0.10, r_bot_r=0.10,
        blush_alpha=1.0, blush_intensity=1.35, blush_y_off=40.0,
        sweat_alpha=1.0, sweat_scale=0.75,
        gaze_x=-10.0, gaze_y=8.0, tilt=6.0, squash_imp=-0.08
    ),
    "DESPAIR": _pose(
        w_scale=1.04, lh_scale=1.08, rh_scale=1.08,
        l_top_l=0.52, l_top_r=0.04, r_top_l=0.04, r_top_r=0.52,
        shake_amp=2.0, gaze_y=4.0, tilt=-6.0, squash_imp=0.14
    ),
    "MALICE": _pose(
        w_scale=1.04, lh_scale=0.82, rh_scale=0.82,
        l_top_l=0.08, l_top_r=0.44, r_top_l=0.44, r_top_r=0.08,
        bot_arc=0.7619, gaze_x=8.0, gaze_y=-3.0, tilt=-7.5, squash_imp=0.10
    ),
    "CURIOUS": _pose(lh_scale=1.12, rh_scale=0.79, gaze_x=10.0, gaze_y=-4.0, tilt=-5.0, allow_idle_gaze=True),
    "BORED": _pose(lh_scale=0.58, rh_scale=0.54, l_top_l=0.28, l_top_r=0.30,
                    r_top_l=0.30, r_top_r=0.26, gaze_y=-8.0, tilt=3.0, allow_idle_gaze=True),
    "SILENT": _pose(w_scale=0.92, lh_scale=0.96, rh_scale=0.96),
    "ANGEL": _pose(bot_arc=0.82, blush_alpha=0.6, tilt=-2.0, allow_idle_gaze=True),
    "COOL": _pose(lh_scale=0.82, rh_scale=0.82, gaze_x=5.0, tilt=-6.0),
    "CRAZY": _pose(w_scale=0.90, lh_scale=1.16, rh_scale=0.82, tilt=6.0, shake_amp=1.4),
    "LIAR": _pose(lh_scale=0.82, rh_scale=0.92, gaze_x=-12.0, gaze_y=-3.0, tilt=4.0),

}


class FaceCanvas296x240(QWidget):
    """
    Simulador na proporção exata da tela Estardyn 2.01" TFT SPI (296x240 Paisagem).
    Renderiza:
    - Rosto em tela cheia limpa com fundo OLED de alto contraste (sem moldura ou orelhas)
    - Olhos 3D estilo Cozmo/Eilik com recorte booleano real (QPainterPath.subtracted) sem máscaras escuras
    - Bochechas vibrantes estilo Eilik/BMO com brilho rosado e traços de rubor
    - Boca BMO com LipSync estritamente acoplado ao áudio PCM
    - Modo Híbrido v2: Tela 100% Pura + Pílula Superior Condicional + Cards de Tela Cheia
    """
    LOGICAL_W = SCREEN_LOGICAL_W
    LOGICAL_H = SCREEN_LOGICAL_H
    SCALE = SCREEN_PREVIEW_SCALE

    def __init__(self, on_pet_callback=None, on_tap_callback=None, parent=None):
        super().__init__(parent)
        self.setFixedSize(self.LOGICAL_W * self.SCALE, self.LOGICAL_H * self.SCALE)
        self.setCursor(Qt.PointingHandCursor)
        self.on_pet_callback = on_pet_callback
        self.on_tap_callback = on_tap_callback
        self.last_mouse_pet_ts = 0.0

        self.state = "IDLE"
        self.emotion = "NEUTRAL"
        self.telemetry = {
            "temp_c": 0.0,
            "wifi_ok": False,
            "wifi_ssid": "",
            "wifi_rssi": 0,
            "ip": "0.0.0.0",
            "psram_kb": 0,
            "timer_rem_s": 0,
            "timer_label": "",
            "portal_active": False,
            "ap_ip": "",
            "net_mode": ""
        }
        self.led_rgb = (0, 180, 255)
        self.led_mode = "STATE"
        self.continuous_until = 0.0

        # Estado do Modo Híbrido (Cards de Tela Cheia, Pílula Condicional, Relógio de Mesa, Alarmes & Humor)
        self.active_widget = {}
        self.widget_until_ts = 0.0
        self.widget_duration_sec = 8.0
        self.desk_clock_active = False
        self.alarm_ringing = False
        self.tasks_cache = []
        self.pomodoro_state = {"active": False, "phase": "FOCUS", "rem_s": 0, "total_s": 1500, "goal": ""}
        self.pomodoro_end_ts = 0.0
        self.timer_state = {"active": False, "rem_s": 0, "total_s": 60, "label": ""}
        self.timer_end_ts = 0.0
        self.personality_state = {
            "affection": 65, "energy": 85, "mood": 15,
            "label": "Animado", "pet_count": 0, "chat_count": 0
        }
        self.weather_cache = {}
        self.alarms_cache = []
        self.sp_overlay_alpha = SpringValue(0.0, stiffness=0.20, damping=0.78)

        # Controle de Piscada com Squash & Stretch
        self.anim_tick = 0.0
        self.blink_phase = 0.0
        self.is_blinking = False
        self.double_blink_pending = False
        self.next_blink_time = time.time() + 2.8

        # Molas Físicas (Olhar, Tilt, Squash & LipSync)
        self.sp_gaze_x = SpringValue(0.0, stiffness=0.16, damping=0.74)
        self.sp_gaze_y = SpringValue(0.0, stiffness=0.16, damping=0.74)
        self.sp_tilt = SpringValue(0.0, stiffness=0.14, damping=0.76)
        self.sp_eye_squash = SpringValue(1.0, stiffness=0.28, damping=0.66)
        self.sp_mouth_w = SpringValue(36.0, stiffness=0.35, damping=0.66)
        self.sp_mouth_h = SpringValue(5.0, stiffness=0.38, damping=0.64)

        # Molas de Morph Paramétrico Suave (~280-350ms com overshoot orgânico)
        self.sp_col_r = SpringValue(0.0, stiffness=0.12, damping=0.78)
        self.sp_col_g = SpringValue(238.0, stiffness=0.12, damping=0.78)
        self.sp_col_b = SpringValue(255.0, stiffness=0.12, damping=0.78)

        self.sp_eye_w = SpringValue(1.0, stiffness=0.16, damping=0.68)
        self.sp_eye_lh = SpringValue(1.0, stiffness=0.16, damping=0.68)
        self.sp_eye_rh = SpringValue(1.0, stiffness=0.16, damping=0.68)
        self.sp_l_top_l = SpringValue(0.0, stiffness=0.16, damping=0.68)
        self.sp_l_top_r = SpringValue(0.0, stiffness=0.16, damping=0.68)
        self.sp_r_top_l = SpringValue(0.0, stiffness=0.16, damping=0.68)
        self.sp_r_top_r = SpringValue(0.0, stiffness=0.16, damping=0.68)
        self.sp_fear_corner = SpringValue(0.0, stiffness=0.16, damping=0.68)
        self.sp_bot_arc = SpringValue(0.0, stiffness=0.16, damping=0.68)
        self.sp_l_bot_l = SpringValue(0.0, stiffness=0.16, damping=0.68)
        self.sp_l_bot_r = SpringValue(0.0, stiffness=0.16, damping=0.68)
        self.sp_r_bot_l = SpringValue(0.0, stiffness=0.16, damping=0.68)
        self.sp_r_bot_r = SpringValue(0.0, stiffness=0.16, damping=0.68)

        self.sp_shake_amp = SpringValue(0.0, stiffness=0.14, damping=0.76)
        self.sp_blush_alpha = SpringValue(0.0, stiffness=0.14, damping=0.76)
        self.sp_blush_intensity = SpringValue(1.0, stiffness=0.14, damping=0.76)
        self.sp_blush_y_off = SpringValue(44.0, stiffness=0.14, damping=0.76)
        self.sp_sweat_alpha = SpringValue(0.0, stiffness=0.14, damping=0.76)
        self.sp_sweat_scale = SpringValue(1.0, stiffness=0.14, damping=0.76)

        self.overlay_alphas = {"LOVE": 0.0, "FEAR": 0.0, "SHAME": 0.0, "DESPAIR": 0.0, "MALICE": 0.0}
        self.mouth_curr_key = "NEUTRAL"
        self.mouth_prev_key = "NEUTRAL"
        self.mouth_trans_t = 1.0
        self.mouth_trans_speed = 0.06
        self._last_pose_emotion = "NEUTRAL"
        self._last_pose_state = "IDLE"

        # Comportamento Autônomo em IDLE
        self.next_idle_action_time = time.time() + 2.2
        self.expression_started = time.monotonic()
        self.idle_curious_squint = 0.0

        # LipSync estritamente acoplado ao áudio real
        self.audio_amp = 0.0
        self.audio_zcr = 0.3
        self.last_audio_frame_ts = 0.0

        # Partículas Flutuantes
        self.particles = []

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.on_frame)
        self.timer.start(16)  # ~60 FPS

    def mousePressEvent(self, event):
        logical_y = event.position().y() / self.SCALE
        if self.alarm_ringing:
            self.alarm_ringing = False
            if self.active_widget.get("type") == "ALARM_RING":
                self.active_widget = {}
            if self.on_tap_callback:
                self.on_tap_callback("ALARM_STOP")
            return

        if self.desk_clock_active:
            self.desk_clock_active = False
            if self.active_widget.get("type") == "DESK_CLOCK":
                self.active_widget = {}
            if self.on_tap_callback:
                self.on_tap_callback("DESK_CLOCK_OFF")
            return

        if self.sp_overlay_alpha.val > 0.25 and self.active_widget:
            self.widget_until_ts = 0.0
            self.active_widget = {}
            if self.on_tap_callback:
                self.on_tap_callback("WIDGET_CLOSE")
            return

        if logical_y < 140 and (time.time() - self.last_mouse_pet_ts) > 2.5:
            self.last_mouse_pet_ts = time.time()
            if self.on_pet_callback:
                self.on_pet_callback()
            else:
                self.trigger_emotion_impulse("LOVE")

    def show_holographic_widget(self, widget_data: dict, duration_sec: float = 8.0):
        data = dict(widget_data) if isinstance(widget_data, dict) else {}
        wtype = str(data.get("type", "")).upper()
        if not wtype or wtype == "NONE":
            self.active_widget = {}
            self.widget_until_ts = 0.0
            self.alarm_ringing = False
            self.desk_clock_active = False
            return

        if wtype == "ALARM_RING":
            self.alarm_ringing = True
            self.active_widget = data
            return

        if wtype == "DESK_CLOCK":
            self.desk_clock_active = True
            self.active_widget = data
            return

        if wtype == "WEATHER":
            self.weather_cache = dict(data)
        elif wtype == "MOOD":
            self.personality_state.update(data)
        elif wtype == "TIMER":
            self.update_timer_state(data)

        self.active_widget = data
        self.widget_duration_sec = max(1.0, float(duration_sec))
        self.widget_until_ts = time.time() + self.widget_duration_sec
        self.sp_eye_squash.set(1.0, impulse=0.20)

    def update_pomodoro_overlay(self, pomo_data: dict):
        if not isinstance(pomo_data, dict):
            return
        self.pomodoro_state = dict(pomo_data)
        if pomo_data.get("active", False) and int(pomo_data.get("rem_s", 0)) > 0:
            self.pomodoro_end_ts = time.time() + int(pomo_data.get("rem_s", 0))
        else:
            self.pomodoro_end_ts = 0.0
        if pomo_data.get("show_card", False):
            self.show_holographic_widget({"type": "POMODORO", **pomo_data}, duration_sec=8.0)

    def set_desk_clock(self, active: bool):
        self.desk_clock_active = bool(active)
        if self.desk_clock_active:
            self.active_widget = {"type": "DESK_CLOCK"}
        elif self.active_widget.get("type") == "DESK_CLOCK":
            self.active_widget = {}

    def update_timer_state(self, timer_data: dict):
        if not isinstance(timer_data, dict):
            return
        rem_s = max(0, int(timer_data.get("rem_s", 0)))
        active = bool(timer_data.get("active", rem_s > 0)) and rem_s > 0
        total_s = max(rem_s, int(timer_data.get("total_s", self.timer_state.get("total_s", max(60, rem_s)))))
        label = str(timer_data.get("label", self.timer_state.get("label", "Timer")) or "Timer")
        self.timer_state = {
            "active": active,
            "rem_s": rem_s,
            "total_s": max(1, total_s),
            "label": label
        }
        self.timer_end_ts = (time.time() + rem_s) if active else 0.0

    def update_personality_state(self, pet_data: dict):
        if isinstance(pet_data, dict):
            self.personality_state.update(pet_data)

    def update_alarms_cache(self, alarms: list):
        if isinstance(alarms, list):
            self.alarms_cache = list(alarms)

    def _apply_emotion_pose(self, emotion: str, impulse: bool = False):
        pose = EMOTION_POSES.get(emotion, _BASE_POSE)
        self._last_pose_emotion = emotion

        self.sp_eye_w.set(pose["w_scale"])
        self.sp_eye_lh.set(pose["lh_scale"])
        self.sp_eye_rh.set(pose["rh_scale"])
        self.sp_l_top_l.set(pose["l_top_l"])
        self.sp_l_top_r.set(pose["l_top_r"])
        self.sp_r_top_l.set(pose["r_top_l"])
        self.sp_r_top_r.set(pose["r_top_r"])
        self.sp_fear_corner.set(pose["fear_corner"])
        self.sp_bot_arc.set(pose["bot_arc"])
        self.sp_l_bot_l.set(pose["l_bot_l"])
        self.sp_l_bot_r.set(pose["l_bot_r"])
        self.sp_r_bot_l.set(pose["r_bot_l"])
        self.sp_r_bot_r.set(pose["r_bot_r"])
        self.sp_shake_amp.set(pose["shake_amp"])

        if pose["blush_alpha"] > 0.05 and self.sp_blush_alpha.val < 0.05:
            self.sp_blush_y_off.snap(pose["blush_y_off"])
            self.sp_blush_intensity.snap(pose["blush_intensity"])
        else:
            self.sp_blush_y_off.set(pose["blush_y_off"])
            self.sp_blush_intensity.set(pose["blush_intensity"])
        self.sp_blush_alpha.set(pose["blush_alpha"])

        if pose["sweat_alpha"] > 0.05 and self.sp_sweat_alpha.val < 0.05:
            self.sp_sweat_scale.snap(pose["sweat_scale"])
        else:
            self.sp_sweat_scale.set(pose["sweat_scale"])
        self.sp_sweat_alpha.set(pose["sweat_alpha"])

        if self.state not in ("THINK", "LISTEN"):
            self.sp_gaze_x.set(pose["gaze_x"])
            self.sp_gaze_y.set(pose["gaze_y"])
            self.sp_tilt.set(pose["tilt"])
        if not pose["allow_idle_gaze"]:
            self.idle_curious_squint = 0.0

        if impulse:
            self.sp_eye_squash.set(1.0, impulse=pose["squash_imp"])

    def _get_target_mouth_key(self) -> str:
        if self.state == "SPEAK":
            return "SPEAK"
        if self.state == "LISTEN":
            return "LISTEN"
        if self.state == "THINK":
            return "THINK"
        if self.emotion in ("HAPPY", "LOVE"):
            return "HAPPY_OPEN"
        if self.emotion in (
            "SURPRISED", "SAD", "ANGRY", "WINK", "SLEEPY",
            "FEAR", "DISGUST", "CONTEMPT", "SHAME", "DESPAIR", "MALICE",
            "CURIOUS", "BORED", "SILENT", "ANGEL", "COOL", "CRAZY", "LIAR"
        ):
            return self.emotion
        return "NEUTRAL"

    def snap_current_pose(self):
        """Assenta todas as molas e cores instantaneamente no estado/emoção atuais."""
        pose = EMOTION_POSES.get(self.emotion, _BASE_POSE)
        self._last_pose_emotion = self.emotion
        self._last_pose_state = self.state

        col = self.get_target_face_color()
        self.sp_col_r.snap(float(col.red()))
        self.sp_col_g.snap(float(col.green()))
        self.sp_col_b.snap(float(col.blue()))

        self.sp_eye_w.snap(pose["w_scale"])
        self.sp_eye_lh.snap(pose["lh_scale"])
        self.sp_eye_rh.snap(pose["rh_scale"])
        self.sp_l_top_l.snap(pose["l_top_l"])
        self.sp_l_top_r.snap(pose["l_top_r"])
        self.sp_r_top_l.snap(pose["r_top_l"])
        self.sp_r_top_r.snap(pose["r_top_r"])
        self.sp_fear_corner.snap(pose["fear_corner"])
        self.sp_bot_arc.snap(pose["bot_arc"])
        self.sp_l_bot_l.snap(pose["l_bot_l"])
        self.sp_l_bot_r.snap(pose["l_bot_r"])
        self.sp_r_bot_l.snap(pose["r_bot_l"])
        self.sp_r_bot_r.snap(pose["r_bot_r"])
        self.sp_shake_amp.snap(pose["shake_amp"])
        self.sp_blush_alpha.snap(pose["blush_alpha"])
        self.sp_blush_intensity.snap(pose["blush_intensity"])
        self.sp_blush_y_off.snap(pose["blush_y_off"])
        self.sp_sweat_alpha.snap(pose["sweat_alpha"])
        self.sp_sweat_scale.snap(pose["sweat_scale"])
        self.sp_gaze_x.snap(pose["gaze_x"])
        self.sp_gaze_y.snap(pose["gaze_y"])
        self.sp_tilt.snap(pose["tilt"])
        self.sp_eye_squash.snap(1.0)
        self.idle_curious_squint = 0.0

        for k in self.overlay_alphas:
            self.overlay_alphas[k] = 1.0 if (k == self.emotion) else 0.0

        mkey = self._get_target_mouth_key()
        self.mouth_curr_key = mkey
        self.mouth_prev_key = mkey
        self.mouth_trans_t = 1.0

    def trigger_emotion_impulse(self, emotion: str):
        self.expression_started = time.monotonic()
        if emotion in ("ANGEL", "CRAZY"):
            self._spawn_particles("star", 5)
        self.emotion = emotion
        self._apply_emotion_pose(emotion, impulse=True)
        if emotion == "HAPPY":
            if self.state not in ("THINK", "LISTEN"):
                self.sp_tilt.set(random.choice([-4.5, 4.5]), impulse=1.6)
            self._spawn_particles("star", 6)
            play_sfx_async("happy")
        elif emotion == "LOVE":
            self.sp_tilt.set(5.5, impulse=1.5)
            self._spawn_particles("heart", 8)
            play_sfx_async("love")
        elif emotion == "WINK":
            self.sp_tilt.set(-7.0, impulse=-1.8)
            self._spawn_particles("star", 4)
            play_sfx_async("wink")
        elif emotion == "SURPRISED":
            self.sp_tilt.set(0.0, impulse=1.2)
            play_sfx_async("surprised")
        elif emotion == "SAD":
            self._spawn_particles("tear", 3)
            play_sfx_async("sad")
        elif emotion == "ANGRY":
            self.sp_tilt.set(0.0, impulse=2.0)
            play_sfx_async("angry")
        elif emotion == "SLEEPY":
            self._spawn_particles("zzz", 3)
        elif emotion == "FEAR":
            self._spawn_particles("sweat", 4)
            play_sfx_async("fear")
        elif emotion == "DISGUST":
            self._spawn_particles("nausea", 4)
            play_sfx_async("disgust")
        elif emotion == "CONTEMPT":
            play_sfx_async("contempt")
        elif emotion == "SHAME":
            self._spawn_particles("sweat", 2)
            play_sfx_async("shame")
        elif emotion == "DESPAIR":
            self._spawn_particles("tear", 6)
            play_sfx_async("despair")
        elif emotion == "MALICE":
            self._spawn_particles("spark", 6)
            play_sfx_async("malice")

    def set_audio_lipsync(self, amp: float, zcr: float):
        self.audio_amp = amp
        self.audio_zcr = zcr
        self.last_audio_frame_ts = time.time()

    def _spawn_particles(self, ptype: str, count: int):
        for _ in range(min(count, max(0, 8 - len(self.particles)))):
            self.particles.append({
                "type": ptype,
                "x": random.uniform(35, 260),
                "y": random.uniform(45, 145),
                "vx": random.uniform(-0.6, 0.6),
                "vy": random.uniform(-1.3, -0.5) if ptype not in ("tear", "sweat") else random.uniform(0.8, 1.5),
                "size": random.uniform(5.0, 9.5),
                "alpha": 255.0,
                "phase": random.uniform(0, 6.28)
            })

    def on_frame(self):
        now = time.time()
        self.anim_tick += 0.075

        if self.emotion != self._last_pose_emotion or self.state != self._last_pose_state:
            self._last_pose_state = self.state
            self._apply_emotion_pose(self.emotion, impulse=False)
            self.next_idle_action_time = now + 1.2

        if self.timer_state.get("active", False) and self.timer_end_ts > 0.0:
            rem_t = max(0, int(round(self.timer_end_ts - now)))
            self.timer_state["rem_s"] = rem_t
            if rem_t == 0:
                self.timer_state["active"] = False
        if self.pomodoro_state.get("active", False) and self.pomodoro_end_ts > 0.0:
            self.pomodoro_state["rem_s"] = max(0, int(round(self.pomodoro_end_ts - now)))

        has_temp = (now < self.widget_until_ts) and bool(self.active_widget)
        has_overlay = self.alarm_ringing or self.desk_clock_active or has_temp
        self.sp_overlay_alpha.set(1.0 if has_overlay else 0.0)
        self.sp_overlay_alpha.step()

        if not has_overlay and self.sp_overlay_alpha.val < 0.02 and self.active_widget:
            self.active_widget = {}

        # 1. Piscada com Squash & Stretch
        if not self.is_blinking and self.blink_phase == 0.0 and now >= self.next_blink_time:
            self.is_blinking = True
            self.sp_eye_squash.set(1.0, impulse=-0.18)

        if self.is_blinking:
            self.blink_phase += 0.36
            if self.blink_phase >= 1.0:
                self.blink_phase = 1.0
                self.is_blinking = False
                self.sp_eye_squash.set(1.0, impulse=0.22)
                if self.double_blink_pending:
                    self.double_blink_pending = False
                    self.next_blink_time = now + 0.14
                else:
                    if random.random() < 0.28:
                        self.double_blink_pending = True
                        self.next_blink_time = now + 0.16
                    else:
                        self.next_blink_time = now + random.uniform(2.8, 5.2)
        elif self.blink_phase > 0.0:
            self.blink_phase = max(0.0, self.blink_phase - 0.30)

        # 2. Vida Autônoma e Olhar
        pose = EMOTION_POSES.get(self.emotion, _BASE_POSE)
        if self.state == "THINK":
            self.sp_gaze_x.set(math.sin(self.anim_tick * 0.8) * 12.0)
            self.sp_gaze_y.set(-11.0)
            self.sp_tilt.set(math.sin(self.anim_tick * 0.5) * 4.5)
            if random.random() < 0.08 and len(self.particles) < 6:
                self._spawn_particles("think_dot", 1)
        elif self.state == "LISTEN":
            self.sp_gaze_x.set(0.0)
            self.sp_gaze_y.set(-2.0)
            self.sp_tilt.set(3.5 * math.sin(self.anim_tick * 0.6))
        elif now >= self.next_idle_action_time:
            if self.state == "IDLE" and pose["allow_idle_gaze"]:
                choice = random.random()
                if choice < 0.45:
                    gx = random.uniform(-14.0, 14.0)
                    gy = random.uniform(-7.0, 6.0)
                    self.sp_gaze_x.set(gx)
                    self.sp_gaze_y.set(gy)
                    self.sp_tilt.set(gx * 0.38)
                    self.idle_curious_squint = random.choice([0.0, 0.0, 0.22])
                elif choice < 0.75:
                    self.sp_gaze_x.set(0.0)
                    self.sp_gaze_y.set(0.0)
                    self.sp_tilt.set(pose["tilt"])
                    self.idle_curious_squint = 0.0
                else:
                    self.sp_eye_squash.set(1.0, impulse=0.20)
                    self.sp_tilt.set(random.uniform(-4.0, 4.0))
            elif self.state == "IDLE":
                self.sp_gaze_x.set(pose["gaze_x"])
                self.sp_gaze_y.set(pose["gaze_y"])
                self.sp_tilt.set(pose["tilt"])
                self.idle_curious_squint = 0.0
            self.next_idle_action_time = now + random.uniform(1.6, 3.4)

        if self.emotion == "LOVE" and random.random() < 0.08 and len(self.particles) < 8:
            self._spawn_particles("heart", 1)
        elif self.emotion == "SLEEPY" and random.random() < 0.05 and len(self.particles) < 5:
            self._spawn_particles("zzz", 1)
        elif self.emotion == "HAPPY" and random.random() < 0.05 and len(self.particles) < 6:
            self._spawn_particles("star", 1)
        elif self.emotion == "FEAR" and random.random() < 0.06 and len(self.particles) < 5:
            self._spawn_particles("sweat", 1)
        elif self.emotion == "DISGUST" and random.random() < 0.05 and len(self.particles) < 5:
            self._spawn_particles("nausea", 1)
        elif self.emotion == "SHAME" and random.random() < 0.03 and len(self.particles) < 3:
            self._spawn_particles("sweat", 1)
        elif self.emotion == "DESPAIR" and random.random() < 0.12 and len(self.particles) < 10:
            self._spawn_particles("tear", 1)
        elif self.emotion == "MALICE" and random.random() < 0.06 and len(self.particles) < 6:
            self._spawn_particles("spark", 1)

        # 3. LipSync Estritamente Sincronizado com o Som Real
        has_live_audio = (self.state == "SPEAK") and ((now - self.last_audio_frame_ts) < 0.12)
        if has_live_audio and self.audio_amp > 0.04:
            amp = self.audio_amp
            zcr = self.audio_zcr
            target_w = 26.0 + (zcr * 28.0) + (amp * 14.0)
            target_h = 6.5 + (amp * 28.0) * (1.15 - 0.35 * zcr)
            self.sp_mouth_w.set(min(54.0, target_w))
            self.sp_mouth_h.set(min(30.0, target_h))
            if amp > 0.55:
                self.sp_eye_squash.set(1.03)
        else:
            self.sp_mouth_w.set(32.0)
            self.sp_mouth_h.set(4.5)

        # 4. Passo das molas de cor, olhar, geometria dos olhos e pálpebras
        t_col = self.get_target_face_color()
        self.sp_col_r.set(float(t_col.red()))
        self.sp_col_g.set(float(t_col.green()))
        self.sp_col_b.set(float(t_col.blue()))
        self.sp_col_r.step()
        self.sp_col_g.step()
        self.sp_col_b.step()

        self.sp_gaze_x.step()
        self.sp_gaze_y.step()
        self.sp_tilt.step()
        self.sp_eye_squash.step()
        self.sp_mouth_w.step()
        self.sp_mouth_h.step()

        self.sp_eye_w.step()
        self.sp_eye_lh.step()
        self.sp_eye_rh.step()
        self.sp_l_top_l.step()
        self.sp_l_top_r.step()
        self.sp_r_top_l.step()
        self.sp_r_top_r.step()
        self.sp_fear_corner.step()
        self.sp_bot_arc.step()
        self.sp_l_bot_l.step()
        self.sp_l_bot_r.step()
        self.sp_r_bot_l.step()
        self.sp_r_bot_r.step()

        self.sp_shake_amp.step()
        self.sp_blush_alpha.step()
        self.sp_blush_intensity.step()
        self.sp_blush_y_off.step()
        self.sp_sweat_alpha.step()
        self.sp_sweat_scale.step()

        for k, cur_a in self.overlay_alphas.items():
            target_a = 1.0 if (k == self.emotion) else 0.0
            nxt_a = cur_a + (target_a - cur_a) * 0.22
            if nxt_a < 0.01:
                nxt_a = 0.0
            elif nxt_a > 0.99:
                nxt_a = 1.0
            self.overlay_alphas[k] = nxt_a

        target_mkey = self._get_target_mouth_key()
        if target_mkey != self.mouth_curr_key:
            self.mouth_prev_key = self.mouth_curr_key
            self.mouth_curr_key = target_mkey
            self.mouth_trans_t = 0.0
            self.mouth_trans_speed = 0.12 if target_mkey == "SPEAK" else 0.06
        if self.mouth_trans_t < 1.0:
            self.mouth_trans_t = min(1.0, self.mouth_trans_t + self.mouth_trans_speed)

        alive = []
        for p in self.particles:
            p["x"] += p["vx"] + math.sin(self.anim_tick + p["phase"]) * 0.25
            p["y"] += p["vy"]
            p["alpha"] -= 4.2
            if p["alpha"] > 8:
                alive.append(p)
        self.particles = alive

        self.update()

    def get_target_face_color(self) -> QColor:
        if self.state == "LISTEN":
            return QColor(0, 250, 145)
        if self.state == "THINK":
            pulse = int(185 + 60 * math.sin(self.anim_tick * 1.8))
            return QColor(pulse, 95, 255)
        if self.state == "ALERT":
            flash = (int(self.anim_tick * 3) % 2) == 0
            return QColor(255, 125, 0) if flash else QColor(255, 220, 0)
        if self.emotion == "ANGRY":
            return QColor(255, 55, 75)
        if self.emotion == "LOVE":
            return QColor(255, 90, 190)
        if self.emotion == "SAD":
            return QColor(85, 165, 255)
        if self.emotion == "FEAR":
            return QColor(150, 215, 255)
        if self.emotion == "DISGUST":
            return QColor(150, 235, 65)
        if self.emotion == "CONTEMPT":
            return QColor(155, 175, 225)
        if self.emotion == "SHAME":
            return QColor(255, 155, 185)
        if self.emotion == "DESPAIR":
            return QColor(115, 115, 255)
        if self.emotion == "MALICE":
            return QColor(195, 70, 255)
        if self.emotion == "CURIOUS":
            return QColor(65, 214, 255)
        if self.emotion == "BORED":
            return QColor(146, 158, 220)
        if self.emotion == "SILENT":
            return QColor(132, 222, 211)
        if self.emotion == "ANGEL":
            return QColor(255, 219, 115)
        if self.emotion == "COOL":
            return QColor(64, 200, 255)
        if self.emotion == "CRAZY":
            return QColor(255, 128, 92)
        if self.emotion == "LIAR":
            return QColor(255, 170, 90)
        return QColor(0, 238, 255)

    def get_face_color(self) -> QColor:
        r = max(0, min(255, int(round(self.sp_col_r.val))))
        g = max(0, min(255, int(round(self.sp_col_g.val))))
        b = max(0, min(255, int(round(self.sp_col_b.val))))
        return QColor(r, g, b)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.scale(self.SCALE, self.SCALE)

        # 1. Fundo Externo OLED do Display 296x240 (sem moldura interna ou orelhas)
        bg_path = QPainterPath()
        bg_path.addRoundedRect(QRectF(2, 2, self.LOGICAL_W - 4, self.LOGICAL_H - 4), 22, 22)
        bg_grad = QRadialGradient(QPointF(148, 118), 175)
        bg_grad.setColorAt(0.0, QColor(16, 26, 46))
        bg_grad.setColorAt(1.0, QColor(6, 10, 18))
        painter.fillPath(bg_path, QBrush(bg_grad))

        painter.setPen(QPen(QColor(38, 56, 84, 150), 1.5))
        painter.drawPath(bg_path)

        # 2. Rosto Puro 296x240 Centralizado (sem barras, borda de capacete ou orelhas)
        painter.save()
        painter.translate(148.0, 124.0)
        painter.scale(1.16, 1.16)
        painter.translate(-148.0, -112.0)

        breath_y = math.sin(self.anim_tick * 0.9) * 2.2
        shake_amp = max(0.0, self.sp_shake_amp.val)
        shake_x = random.uniform(-shake_amp, shake_amp) if shake_amp > 0.05 else 0.0
        gx = self.sp_gaze_x.val
        gy = self.sp_gaze_y.val
        tilt = self.sp_tilt.val

        face_cx, face_cy = 148.0 + shake_x, 112.0 + breath_y
        painter.translate(face_cx, face_cy)
        painter.rotate(tilt)
        painter.translate(-face_cx, -face_cy)

        face_color = self.get_face_color()

        left_persp = 1.0 - (gx / 130.0)
        right_persp = 1.0 + (gx / 130.0)

        squash = max(0.55, self.sp_eye_squash.val)
        blink_open = 1.0 - self.blink_phase
        stretch_w = 1.0 + (1.0 - blink_open) * 0.16

        base_w = 64.0 * stretch_w * max(0.4, self.sp_eye_w.val) / max(0.8, squash * 0.98)
        base_h = 70.0 * squash * blink_open

        left_cx = 94.0 + gx
        right_cx = 202.0 + gx
        eye_cy = 88.0 + gy + breath_y

        # 3b. Olhos 3D com Recorte Booleano Contínuo (Morph Paramétrico Suave)
        left_h = max(5.0, base_h * max(0.06, self.sp_eye_lh.val) * left_persp)
        self._draw_single_eye(
            painter, left_cx, eye_cy,
            base_w * left_persp, left_h,
            face_color, is_left=True
        )
        right_h = max(5.0, base_h * max(0.06, self.sp_eye_rh.val) * right_persp * (1.0 - self.idle_curious_squint))
        self._draw_single_eye(
            painter, right_cx, eye_cy,
            base_w * right_persp, right_h,
            face_color, is_left=False
        )

        # 3c. Bochechas Coradas & Gota de Suor com Fade Suave
        b_alpha = clamp01(self.sp_blush_alpha.val)
        if b_alpha > 0.02:
            self._draw_blush_cheeks(
                painter,
                left_cx - 12.0 + (44.0 - self.sp_blush_y_off.val) * 0.5,
                right_cx + 12.0 - (44.0 - self.sp_blush_y_off.val) * 0.5,
                eye_cy + self.sp_blush_y_off.val,
                max(0.5, self.sp_blush_intensity.val),
                b_alpha
            )

        s_alpha = clamp01(self.sp_sweat_alpha.val)
        if s_alpha > 0.02:
            drop_scale = max(0.3, self.sp_sweat_scale.val) * (0.65 + 0.35 * s_alpha)
            drop_y = eye_cy - 18.0 + math.sin(self.anim_tick * 2.2) * 2.2
            self._draw_sweat_drop(painter, right_cx + 33.0, drop_y, drop_scale, s_alpha)

        # 4. Boca Expressiva Eilik / BMO com Morph Elástico
        mouth_cx = 148.0 + gx * 0.45
        mouth_cy = 152.0 + gy * 0.35 + breath_y
        self._draw_expressive_mouth(painter, mouth_cx, mouth_cy, face_color)

        self._draw_expression_details(painter, left_cx, right_cx, eye_cy)

        painter.restore()

        # 5. Partículas Flutuantes (sempre ATRÁS dos cards para não poluir a leitura)
        self._draw_particles(painter)

        # 6. Modo Híbrido v2: Card de Tela Cheia (com fade suave) OU Pílula Superior Condicional
        overlay_alpha = max(0.0, min(1.0, self.sp_overlay_alpha.val))
        effective_widget = self.active_widget
        if self.alarm_ringing:
            effective_widget = {"type": "ALARM_RING", **self.active_widget}
        elif self.desk_clock_active:
            effective_widget = {"type": "DESK_CLOCK"}

        if overlay_alpha > 0.02 and effective_widget:
            rem_ratio = max(0.0, (self.widget_until_ts - time.time()) / max(1.0, self.widget_duration_sec))
            WidgetsOverlayRenderer.draw_fullscreen_card(
                painter,
                effective_widget,
                self.tasks_cache,
                self.pomodoro_state,
                self.timer_state,
                self.personality_state,
                self.weather_cache,
                self.alarms_cache,
                overlay_alpha,
                rem_ratio,
                self.anim_tick
            )
        else:
            WidgetsOverlayRenderer.draw_top_pill(
                painter,
                self.pomodoro_state,
                self.timer_state,
                self.alarm_ringing,
                self.telemetry,
                self.anim_tick
            )

    def _draw_expression_details(self, painter, lx, rx, y):
        cx = (lx + rx) / 2
        age = min(1.0, (time.monotonic() - self.expression_started) / 0.65)
        if self.emotion == "ANGEL":
            painter.setBrush(Qt.NoBrush)
            painter.setPen(QPen(QColor(255, 215, 85), 3.0))
            painter.drawEllipse(QPointF(cx, y - 50), 27, 7)
        elif self.emotion == "COOL":
            painter.setPen(QPen(QColor(65, 130, 165), 2.0))
            painter.setBrush(QColor(8, 18, 34))
            painter.drawLine(QPointF(lx, y - 4), QPointF(rx, y - 4))
            for x in (lx, rx):
                painter.drawRoundedRect(QRectF(x - 34, y - 18, 68, 36), 8, 8)
                painter.drawLine(QPointF(x - 23, y + 5), QPointF(x - 7, y - 10))
        elif self.emotion == "CRAZY":
            painter.setPen(QPen(QColor(67, 24, 52), 2.6, Qt.SolidLine, Qt.RoundCap))
            for x, radius, direction in ((lx, 21, 1), (rx, 16, -1)):
                spiral = QPainterPath(QPointF(x, y))
                for i in range(1, 29):
                    angle = direction * i * 0.39 + self.anim_tick * direction
                    r = radius * i / 28
                    spiral.lineTo(x + math.cos(angle) * r, y + math.sin(angle) * r)
                painter.drawPath(spiral)
        elif self.emotion == "LIAR":
            length = 10 + 31 * ease_out_quad(age)
            painter.setPen(QPen(QColor(255, 170, 90), 3.0, Qt.SolidLine, Qt.RoundCap))
            painter.drawLine(QPointF(cx, y + 39), QPointF(cx + length, y + 43))
            painter.drawLine(QPointF(cx + length, y + 43), QPointF(cx + length - 8, y + 47))

    def _draw_blush_cheeks(self, painter: QPainter, lx: float, rx: float, cy: float,
                           intensity: float = 1.0, alpha_weight: float = 1.0):
        """Desenha bochechas rosadas vibrantes e suaves estilo Eilik/BMO com suporte a fade."""
        aw = clamp01(alpha_weight)
        pulse = math.sin(self.anim_tick * 1.6)
        core_alpha = int((215 + 35 * pulse) * max(0.8, intensity) * aw)
        mid_alpha = int((130 + 25 * pulse) * max(0.8, intensity) * aw)
        edge_alpha = int(25 * aw)

        for cx in (lx, rx):
            painter.setPen(Qt.NoPen)
            grad = QRadialGradient(QPointF(cx, cy), 25)
            grad.setColorAt(0.0, QColor(255, 80, 155, min(255, core_alpha)))
            grad.setColorAt(0.45, QColor(255, 65, 140, min(255, mid_alpha)))
            grad.setColorAt(0.85, QColor(255, 65, 140, edge_alpha))
            grad.setColorAt(1.0, QColor(255, 65, 140, 0))
            painter.setBrush(QBrush(grad))
            painter.drawEllipse(QPointF(cx, cy), 25, 15)

            # Tracinhos luminosos nas bochechas (estilo anime / Eilik)
            line_alpha = min(255, int(240 * max(0.85, intensity) * aw))
            painter.setPen(QPen(QColor(255, 205, 230, line_alpha), 1.8, Qt.SolidLine, Qt.RoundCap))
            for offset_x in (-5.0, 0.0, 5.0):
                painter.drawLine(
                    QPointF(cx + offset_x - 2.0, cy + 3.2),
                    QPointF(cx + offset_x + 2.0, cy - 3.2)
                )

    def _draw_single_eye(self, painter: QPainter, cx: float, cy: float, w: float, h: float, color: QColor, is_left: bool):
        rect = QRectF(cx - w / 2, cy - h / 2, w, h)

        # Constrói o caminho base do olho e aplica recortes booleanos paramétricos contínuos
        eye_path = QPainterPath()
        eye_path.addRoundedRect(rect, 20, 20)

        if h > 14:
            if is_left:
                tl = max(0.0, min(0.85, self.sp_l_top_l.val))
                tr = max(0.0, min(0.85, self.sp_l_top_r.val))
                bl = max(0.0, min(0.85, self.sp_l_bot_l.val))
                br = max(0.0, min(0.85, self.sp_l_bot_r.val))
            else:
                tl = max(0.0, min(0.85, self.sp_r_top_l.val))
                tr = max(0.0, min(0.85, self.sp_r_top_r.val))
                bl = max(0.0, min(0.85, self.sp_r_bot_l.val))
                br = max(0.0, min(0.85, self.sp_r_bot_r.val))

            if max(tl, tr) > 0.008:
                top_cut = QPainterPath()
                top_cut.moveTo(rect.left() - 6, rect.top() - 6)
                top_cut.lineTo(rect.right() + 6, rect.top() - 6)
                top_cut.lineTo(rect.right() + 6, rect.top() + h * tr)
                top_cut.lineTo(rect.left() - 6, rect.top() + h * tl)
                top_cut.closeSubpath()
                eye_path = eye_path.subtracted(top_cut)

            fc = clamp01(self.sp_fear_corner.val)
            if fc > 0.008:
                fear_cut = QPainterPath()
                if is_left:
                    fear_cut.moveTo(rect.left() - 6, rect.top() - 6)
                    fear_cut.lineTo(rect.left() + w * 0.58 * fc, rect.top() - 6)
                    fear_cut.lineTo(rect.left() - 6, rect.top() + h * 0.30 * fc)
                else:
                    fear_cut.moveTo(rect.right() + 6, rect.top() - 6)
                    fear_cut.lineTo(rect.right() - w * 0.58 * fc, rect.top() - 6)
                    fear_cut.lineTo(rect.right() + 6, rect.top() + h * 0.30 * fc)
                fear_cut.closeSubpath()
                eye_path = eye_path.subtracted(fear_cut)

            ba = max(0.0, min(1.12, self.sp_bot_arc.val))
            if ba > 0.008:
                bot_arc_cut = QPainterPath()
                arc_y_factor = 0.56 - 0.42 * ba
                arc_h_factor = 0.88 + 0.08 * min(1.0, ba)
                bot_arc_cut.addEllipse(QRectF(cx - w * 0.68, cy + h * arc_y_factor, w * 1.36, h * arc_h_factor))
                eye_path = eye_path.subtracted(bot_arc_cut)

            if max(bl, br) > 0.008:
                bot_cut = QPainterPath()
                bot_cut.moveTo(rect.left() - 6, rect.bottom() + 6)
                bot_cut.lineTo(rect.right() + 6, rect.bottom() + 6)
                bot_cut.lineTo(rect.right() + 6, rect.bottom() - h * br)
                bot_cut.lineTo(rect.left() - 6, rect.bottom() - h * bl)
                bot_cut.closeSubpath()
                eye_path = eye_path.subtracted(bot_cut)

        # Halo neon externo suave
        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 65), 4.0))
        painter.setBrush(Qt.NoBrush)
        painter.drawPath(eye_path)

        # Preenchimento 3D brilhante do olho
        grad = QLinearGradient(rect.topLeft(), rect.bottomRight())
        grad.setColorAt(0.0, color.lighter(135))
        grad.setColorAt(0.55, color)
        grad.setColorAt(1.0, color.darker(120))
        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(grad))
        painter.drawPath(eye_path)

        # Reflexos especulares + Overlays internos com crossfade contínuo
        if h > 20:
            painter.save()
            painter.setClipPath(eye_path)
            shift_x = self.sp_gaze_x.val * 0.18
            shift_y = self.sp_gaze_y.val * 0.18
            painter.setBrush(QBrush(QColor(255, 255, 255, 225)))
            painter.drawEllipse(QPointF(cx - w * 0.20 + shift_x, cy - h * 0.22 + shift_y), w * 0.13, h * 0.11)
            painter.setBrush(QBrush(QColor(255, 255, 255, 135)))
            painter.drawEllipse(QPointF(cx + w * 0.18 + shift_x, cy + h * 0.20 + shift_y), w * 0.06, h * 0.06)

            if h > 24:
                a_love = self.overlay_alphas.get("LOVE", 0.0)
                if a_love > 0.02:
                    sc_love = max(0.3, 0.55 + 0.45 * ease_out_back(a_love, 1.3))
                    painter.save()
                    painter.setOpacity(a_love)
                    s = (11.0 + 2.0 * math.sin(self.anim_tick * 2.5)) * sc_love
                    self._draw_heart_shape(painter, cx, cy + 2, s, QColor(255, 240, 248, 240))
                    painter.restore()

                a_fear = self.overlay_alphas.get("FEAR", 0.0)
                if a_fear > 0.02:
                    sc_fear = max(0.3, 0.55 + 0.45 * ease_out_back(a_fear, 1.2))
                    painter.save()
                    painter.setOpacity(a_fear)
                    jitter = math.sin(self.anim_tick * 5.0) * 1.4
                    px, py = cx + shift_x + jitter + 2.0, cy + 7.0 + shift_y
                    painter.setPen(Qt.NoPen)
                    painter.setBrush(QBrush(QColor(12, 22, 42, 242)))
                    painter.drawEllipse(QPointF(px, py), 7.5 * sc_fear, 7.5 * sc_fear)
                    painter.setBrush(QBrush(QColor(255, 255, 255, 235)))
                    painter.drawEllipse(QPointF(px - 2.4 * sc_fear, py - 2.4 * sc_fear), 2.3 * sc_fear, 2.3 * sc_fear)
                    painter.restore()

                a_desp = self.overlay_alphas.get("DESPAIR", 0.0)
                if a_desp > 0.02:
                    painter.save()
                    painter.setOpacity(a_desp)
                    ring_pulse = 1.0 + 0.08 * math.sin(self.anim_tick * 3.5)
                    painter.setBrush(Qt.NoBrush)
                    for rr, lw in ((7.0, 2.2), (14.5, 2.0), (22.0, 1.8)):
                        painter.setPen(QPen(QColor(18, 24, 58, 210), lw))
                        painter.drawEllipse(QPointF(cx + shift_x * 0.5, cy + 5), rr * ring_pulse, rr * 0.92 * ring_pulse)
                    painter.setPen(Qt.NoPen)
                    painter.setBrush(QBrush(QColor(175, 225, 255, 195)))
                    painter.drawEllipse(QPointF(cx, cy + h * 0.30), w * 0.24, h * 0.08)
                    painter.restore()

            if h > 22:
                a_shame = self.overlay_alphas.get("SHAME", 0.0)
                if a_shame > 0.02:
                    painter.save()
                    painter.setOpacity(a_shame)
                    painter.setPen(Qt.NoPen)
                    painter.setBrush(QBrush(QColor(255, 240, 248, 220)))
                    painter.drawEllipse(QPointF(cx - w * 0.12 + shift_x, cy + h * 0.18 + shift_y), w * 0.17, h * 0.10)
                    painter.restore()

            if h > 20:
                a_mal = self.overlay_alphas.get("MALICE", 0.0)
                if a_mal > 0.02:
                    painter.save()
                    painter.setOpacity(a_mal)
                    painter.setPen(Qt.NoPen)
                    painter.setBrush(QBrush(QColor(255, 220, 255, 210)))
                    painter.drawEllipse(QPointF(cx + shift_x + (4.0 if is_left else -4.0), cy + 2), 4.5, h * 0.22 * (0.5 + 0.5 * a_mal))
                    painter.restore()

            painter.restore()

    def _draw_sweat_drop(self, painter: QPainter, cx: float, cy: float, scale: float = 1.0, alpha_weight: float = 1.0):
        painter.save()
        if alpha_weight < 0.999:
            painter.setOpacity(clamp01(alpha_weight))
        painter.translate(cx, cy)
        painter.scale(scale, scale)
        drop = QPainterPath()
        drop.moveTo(0, -10)
        drop.cubicTo(6, -1, 7, 6, 0, 9)
        drop.cubicTo(-7, 6, -6, -1, 0, -10)
        painter.setPen(QPen(QColor(210, 245, 255, 220), 1.3))
        painter.setBrush(QBrush(QColor(95, 215, 255, 195)))
        painter.drawPath(drop)
        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(QColor(255, 255, 255, 220)))
        painter.drawEllipse(QPointF(-1.5, 2.0), 1.4, 2.4)
        painter.restore()

    def _draw_heart_shape(self, painter: QPainter, cx: float, cy: float, size: float, color: QColor):
        path = QPainterPath()
        path.moveTo(cx, cy + size * 0.65)
        path.cubicTo(cx - size * 1.2, cy - size * 0.1, cx - size * 0.6, cy - size * 0.95, cx, cy - size * 0.35)
        path.cubicTo(cx + size * 0.6, cy - size * 0.95, cx + size * 1.2, cy - size * 0.1, cx, cy + size * 0.65)
        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(color))
        painter.drawPath(path)

    def _draw_expressive_mouth(self, painter: QPainter, cx: float, cy: float, color: QColor):
        t = clamp01(self.mouth_trans_t)
        if t >= 0.999 or self.mouth_prev_key == self.mouth_curr_key:
            self._draw_mouth_shape(painter, self.mouth_curr_key, cx, cy, color)
            return

        # Morph Elástico: boca anterior encolhe e desaparece enquanto a nova entra com pop suave
        if t < 0.55:
            u = clamp01(t / 0.55)
            op_out = max(0.0, 1.0 - ease_out_quad(u))
            if op_out > 0.02:
                sx_out = 1.0 - 0.25 * u
                sy_out = max(0.15, 1.0 - 0.55 * u)
                painter.save()
                painter.setOpacity(op_out)
                painter.translate(cx, cy)
                painter.scale(sx_out, sy_out)
                painter.translate(-cx, -cy)
                self._draw_mouth_shape(painter, self.mouth_prev_key, cx, cy, color)
                painter.restore()

        if t > 0.22:
            v = clamp01((t - 0.22) / 0.78)
            op_in = ease_out_quad(v)
            sc_in = max(0.25, 0.52 + 0.48 * ease_out_back(v, 1.35))
            if op_in > 0.02:
                painter.save()
                painter.setOpacity(op_in)
                painter.translate(cx, cy)
                painter.scale(sc_in, sc_in)
                painter.translate(-cx, -cy)
                self._draw_mouth_shape(painter, self.mouth_curr_key, cx, cy, color)
                painter.restore()

    def _draw_mouth_shape(self, painter: QPainter, mouth_key: str, cx: float, cy: float, color: QColor):
        mouth_inner_bg = QColor(42, 14, 34)
        tongue_color = QColor(255, 105, 145)
        teeth_color = QColor(245, 250, 255)

        if mouth_key == "SPEAK":
            mw = self.sp_mouth_w.val
            mh = self.sp_mouth_h.val

            if mh <= 6.2:
                painter.setPen(QPen(color, 3.4, Qt.SolidLine, Qt.RoundCap))
                painter.setBrush(Qt.NoBrush)
                path = QPainterPath()
                path.moveTo(cx - mw * 0.48, cy - 1)
                path.quadTo(cx, cy + 5, cx + mw * 0.48, cy - 1)
                painter.drawPath(path)
                return

            m_rect = QRectF(cx - mw / 2, cy - mh / 2, mw, mh)
            radius = min(mw, mh) * 0.46

            clip_path = QPainterPath()
            clip_path.addRoundedRect(m_rect, radius, radius)

            painter.save()
            painter.setClipPath(clip_path)
            painter.fillPath(clip_path, QBrush(mouth_inner_bg))

            painter.setPen(Qt.NoPen)
            painter.setBrush(QBrush(tongue_color))
            painter.drawEllipse(QRectF(cx - mw * 0.38, cy + mh * 0.08, mw * 0.76, mh * 0.68))

            if mh >= 11.0 and mw >= 30.0:
                painter.setBrush(QBrush(teeth_color))
                painter.drawRoundedRect(QRectF(cx - mw * 0.38, cy - mh / 2 - 2, mw * 0.76, mh * 0.28), 3, 3)

            painter.restore()

            painter.setPen(QPen(color, 2.6))
            painter.setBrush(Qt.NoBrush)
            painter.drawPath(clip_path)
            return

        if mouth_key == "LISTEN":
            pulse_h = 9.0 + 4.5 * abs(math.sin(self.anim_tick * 2.2))
            m_rect = QRectF(cx - 16, cy - pulse_h / 2, 32, pulse_h)
            painter.setPen(QPen(color, 2.5))
            painter.setBrush(QBrush(mouth_inner_bg))
            painter.drawRoundedRect(m_rect, 6, 6)
            return

        if mouth_key == "THINK":
            painter.setPen(QPen(color, 3.2, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            path = QPainterPath()
            path.moveTo(cx - 15, cy + 2)
            path.cubicTo(cx - 5, cy - 5, cx + 5, cy + 7, cx + 15, cy - 1)
            painter.drawPath(path)
            return

        if mouth_key == "HAPPY_OPEN":
            mw, mh = 46.0, 24.0
            path = QPainterPath()
            path.moveTo(cx - mw / 2, cy - 5)
            path.lineTo(cx + mw / 2, cy - 5)
            path.arcTo(QRectF(cx - mw / 2, cy - 5 - mh / 2, mw, mh), 0, -180)
            path.closeSubpath()

            painter.save()
            painter.setClipPath(path)
            painter.fillPath(path, QBrush(mouth_inner_bg))
            painter.setPen(Qt.NoPen)
            painter.setBrush(QBrush(tongue_color))
            painter.drawEllipse(QRectF(cx - 14, cy + 1, 28, 14))
            painter.setBrush(QBrush(teeth_color))
            painter.drawRect(QRectF(cx - 18, cy - 6, 36, 4.5))
            painter.restore()

            painter.setPen(QPen(color, 2.6))
            painter.setBrush(Qt.NoBrush)
            painter.drawPath(path)

        elif mouth_key == "SURPRISED":
            painter.setPen(QPen(color, 2.8))
            painter.setBrush(QBrush(mouth_inner_bg))
            painter.drawEllipse(QPointF(cx, cy + 2), 12, 15)

        elif mouth_key == "SAD":
            painter.setPen(QPen(color, 3.4, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            path = QPainterPath()
            path.moveTo(cx - 20, cy + 8)
            path.quadTo(cx, cy - 7, cx + 20, cy + 8)
            painter.drawPath(path)

        elif mouth_key == "ANGRY":
            painter.setPen(QPen(color, 3.2, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            path = QPainterPath()
            path.moveTo(cx - 22, cy + 5)
            path.lineTo(cx - 8, cy - 2)
            path.lineTo(cx + 8, cy + 5)
            path.lineTo(cx + 22, cy - 2)
            painter.drawPath(path)

        elif mouth_key == "WINK":
            painter.setPen(QPen(color, 3.4, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            path = QPainterPath()
            path.moveTo(cx - 18, cy + 2)
            path.quadTo(cx + 3, cy + 13, cx + 22, cy - 6)
            painter.drawPath(path)

        elif mouth_key == "SLEEPY":
            r = 5.0 + 1.8 * math.sin(self.anim_tick * 0.8)
            painter.setPen(QPen(color, 2.4))
            painter.setBrush(QBrush(mouth_inner_bg))
            painter.drawEllipse(QPointF(cx, cy + 2), r, r)

        elif mouth_key == "FEAR":
            mw, mh = 38.0, 15.0
            jitter_y = math.sin(self.anim_tick * 6.0) * 1.1
            m_rect = QRectF(cx - mw / 2, cy - mh / 2 + 2 + jitter_y, mw, mh)
            m_clip = QPainterPath()
            m_clip.addRoundedRect(m_rect, 7.0, 7.0)
            painter.save()
            painter.setClipPath(m_clip)
            painter.fillPath(m_clip, QBrush(mouth_inner_bg))
            painter.setPen(Qt.NoPen)
            painter.setBrush(QBrush(teeth_color))
            painter.drawRect(QRectF(cx - mw * 0.45, m_rect.top(), mw * 0.90, mh * 0.42))
            painter.drawRect(QRectF(cx - mw * 0.45, m_rect.bottom() - mh * 0.42, mw * 0.90, mh * 0.42))
            painter.setPen(QPen(mouth_inner_bg, 1.3))
            for tx in (-9.0, 0.0, 9.0):
                painter.drawLine(QPointF(cx + tx, m_rect.top()), QPointF(cx + tx, m_rect.bottom()))
            painter.restore()
            painter.setPen(QPen(color, 2.6))
            painter.setBrush(Qt.NoBrush)
            painter.drawPath(m_clip)

        elif mouth_key == "DISGUST":
            grimace = QPainterPath()
            grimace.moveTo(cx - 21, cy + 2)
            grimace.cubicTo(cx - 13, cy - 11, cx - 5, cy - 9, cx + 2, cy)
            grimace.quadTo(cx + 9, cy + 8, cx + 21, cy - 1)
            grimace.quadTo(cx + 12, cy + 12, cx + 3, cy + 9)
            grimace.cubicTo(cx - 6, cy + 5, cx - 9, cy - 3, cx - 21, cy + 2)
            grimace.closeSubpath()
            painter.setPen(QPen(color, 2.8, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
            painter.setBrush(QBrush(mouth_inner_bg))
            painter.drawPath(grimace)

            tongue = QPainterPath()
            tongue.moveTo(cx + 3, cy + 7)
            tongue.quadTo(cx + 8, cy + 9, cx + 14, cy + 6)
            tongue.cubicTo(cx + 13, cy + 16, cx + 6, cy + 17, cx + 3, cy + 7)
            tongue.closeSubpath()
            painter.setPen(QPen(color, 1.8))
            painter.setBrush(QBrush(tongue_color))
            painter.drawPath(tongue)
            painter.setPen(QPen(mouth_inner_bg, 1.2, Qt.SolidLine, Qt.RoundCap))
            painter.drawLine(QPointF(cx + 9, cy + 9), QPointF(cx + 8, cy + 13))

        elif mouth_key == "CONTEMPT":
            smirk = QPainterPath()
            smirk.moveTo(cx - 18, cy + 3)
            smirk.cubicTo(cx - 6, cy + 6, cx + 12, cy + 6, cx + 22, cy - 7)
            smirk.cubicTo(cx + 15, cy + 10, cx + 1, cy + 12, cx - 18, cy + 3)
            smirk.closeSubpath()
            painter.setPen(QPen(color, 2.8, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
            painter.setBrush(QBrush(mouth_inner_bg))
            painter.drawPath(smirk)
            painter.setBrush(Qt.NoBrush)
            painter.setPen(QPen(color, 2.0, Qt.SolidLine, Qt.RoundCap))
            painter.drawLine(QPointF(cx + 19, cy - 8), QPointF(cx + 24, cy - 5))

        elif mouth_key == "SHAME":
            path = QPainterPath()
            path.moveTo(cx - 12, cy + 1)
            path.quadTo(cx - 6, cy + 7, cx, cy + 1)
            path.quadTo(cx + 6, cy + 7, cx + 12, cy + 1)
            painter.setPen(QPen(color, 2.8, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            painter.drawPath(path)

        elif mouth_key == "DESPAIR":
            jitter_y = math.sin(self.anim_tick * 5.5) * 1.0
            cy_d = cy + jitter_y
            path = QPainterPath()
            path.moveTo(cx - 21, cy_d + 13)
            path.cubicTo(cx - 14, cy_d - 14, cx + 14, cy_d - 14, cx + 21, cy_d + 13)
            path.cubicTo(cx + 10, cy_d + 5, cx - 10, cy_d + 5, cx - 21, cy_d + 13)
            path.closeSubpath()
            painter.save()
            painter.setClipPath(path)
            painter.fillPath(path, QBrush(mouth_inner_bg))
            painter.setPen(Qt.NoPen)
            painter.setBrush(QBrush(tongue_color))
            painter.drawEllipse(QPointF(cx, cy_d + 8), 11, 6)
            painter.restore()
            painter.setPen(QPen(color, 2.6, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
            painter.setBrush(Qt.NoBrush)
            painter.drawPath(path)

        elif mouth_key == "MALICE":
            path = QPainterPath()
            path.moveTo(cx - 23, cy - 2)
            path.quadTo(cx + 4, cy - 2, cx + 25, cy - 7)
            path.quadTo(cx + 12, cy + 18, cx - 23, cy - 2)
            path.closeSubpath()

            painter.save()
            painter.setClipPath(path)
            painter.fillPath(path, QBrush(mouth_inner_bg))
            painter.setPen(Qt.NoPen)
            painter.setBrush(QBrush(teeth_color))
            fang = QPainterPath()
            fang.moveTo(cx + 10, cy - 6)
            fang.lineTo(cx + 17, cy - 6)
            fang.lineTo(cx + 14, cy + 2)
            fang.closeSubpath()
            painter.drawPath(fang)
            painter.restore()
            painter.setPen(QPen(color, 2.6, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))
            painter.setBrush(Qt.NoBrush)
            painter.drawPath(path)

        elif mouth_key == "SILENT":
            return

        elif mouth_key == "CURIOUS":
            painter.setPen(QPen(color, 2.6))
            painter.setBrush(QBrush(mouth_inner_bg))
            painter.drawEllipse(QPointF(cx + 3, cy), 6.5, 7.5)

        elif mouth_key in ("BORED", "ANGEL", "COOL", "CRAZY", "LIAR"):
            curves = {
                "BORED": (-17, 2, 5, -3, 19, 5),
                "ANGEL": (-14, -1, 0, 13, 14, -1),
                "COOL": (-16, 3, 4, 10, 21, -6),
                "CRAZY": (-20, 0, -5, 19, 13, -5),
                "LIAR": (-17, 3, 0, -9, 15, 4),
            }
            x0, y0, x1, y1, x2, y2 = curves[mouth_key]
            path = QPainterPath(QPointF(cx + x0, cy + y0))
            path.quadTo(cx + x1, cy + y1, cx + x2, cy + y2)
            painter.setPen(QPen(color, 3.3, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            painter.drawPath(path)

        else:
            painter.setPen(QPen(color, 3.4, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            path = QPainterPath()
            path.moveTo(cx - 20, cy - 2)
            path.quadTo(cx, cy + 12, cx + 20, cy - 2)
            painter.drawPath(path)

    def _draw_particles(self, painter: QPainter):
        for p in self.particles:
            alpha = int(max(0, min(255, p["alpha"])))
            ptype = p["type"]
            px, py, sz = p["x"], p["y"], p["size"]

            if ptype == "heart":
                self._draw_heart_shape(painter, px, py, sz, QColor(255, 90, 175, alpha))
            elif ptype == "star":
                painter.setPen(QPen(QColor(255, 230, 90, alpha), 2.0, Qt.SolidLine, Qt.RoundCap))
                painter.drawLine(QPointF(px - sz, py), QPointF(px + sz, py))
                painter.drawLine(QPointF(px, py - sz), QPointF(px, py + sz))
            elif ptype == "zzz":
                painter.setFont(QFont("Segoe UI", int(sz + 3), QFont.Bold))
                painter.setPen(QColor(150, 210, 255, alpha))
                painter.drawText(QPointF(px, py), "Z")
            elif ptype == "tear":
                painter.setPen(Qt.NoPen)
                painter.setBrush(QBrush(QColor(80, 185, 255, alpha)))
                painter.drawEllipse(QPointF(px, py), sz * 0.45, sz * 0.75)
            elif ptype == "think_dot":
                painter.setPen(Qt.NoPen)
                painter.setBrush(QBrush(QColor(200, 120, 255, alpha)))
                painter.drawEllipse(QPointF(px, py), sz * 0.5, sz * 0.5)
            elif ptype == "sweat":
                painter.setPen(QPen(QColor(210, 245, 255, alpha), 1.0))
                painter.setBrush(QBrush(QColor(110, 215, 255, alpha)))
                painter.drawEllipse(QPointF(px, py), sz * 0.36, sz * 0.62)
            elif ptype == "nausea":
                painter.setPen(QPen(QColor(175, 255, 95, alpha), 1.3))
                painter.setBrush(QBrush(QColor(125, 220, 55, int(alpha * 0.55))))
                painter.drawEllipse(QPointF(px, py), sz * 0.52, sz * 0.52)
            elif ptype == "spark":
                painter.setPen(QPen(QColor(235, 160, 255, alpha), 1.6, Qt.SolidLine, Qt.RoundCap))
                painter.drawLine(QPointF(px - sz * 0.7, py), QPointF(px + sz * 0.7, py))
                painter.drawLine(QPointF(px, py - sz * 0.7), QPointF(px, py + sz * 0.7))
