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
from app.physics import SpringValue
from app.sfx import play_sfx_async
from app.widgets_overlay import WidgetsOverlayRenderer


class FaceCanvas296x240(QWidget):
    """
    Simulador na proporção exata da tela Estardyn 2.01" TFT SPI (296x240 Paisagem).
    Renderiza:
    - Olhos 3D estilo Cozmo/Eilik + Boca BMO com LipSync estritamente acoplado ao áudio PCM
    - Layout Dividido Híbrido (Estilo Eilik/Divoom): quando um Mini-Widget Holográfico ativa,
      os olhos e a boca encolhem suavemente para o lado esquerdo e o Card aparece à direita.
    - Interação de Carinho na Cabeça (Mouse ou GPIO 1 Touch Capacitivo).
    """
    LOGICAL_W = SCREEN_LOGICAL_W
    LOGICAL_H = SCREEN_LOGICAL_H
    SCALE = SCREEN_PREVIEW_SCALE

    def __init__(self, on_pet_callback=None, parent=None):
        super().__init__(parent)
        self.setFixedSize(self.LOGICAL_W * self.SCALE, self.LOGICAL_H * self.SCALE)
        self.setCursor(Qt.PointingHandCursor)
        self.on_pet_callback = on_pet_callback
        self.last_mouse_pet_ts = 0.0

        self.state = "IDLE"
        self.emotion = "NEUTRAL"
        self.subtitle = "Ciborgue Pronto — Diga 'Ciborgue' ou faça carinho no topo da tela!"
        self.telemetry = {
            "temp_c": 0.0,
            "wifi_ok": False,
            "wifi_ssid": "",
            "wifi_rssi": 0,
            "ip": "0.0.0.0",
            "psram_kb": 0,
            "timer_rem_s": 0,
            "timer_label": ""
        }
        self.led_rgb = (0, 120, 255)
        self.led_mode = "STATE"
        self.continuous_until = 0.0

        # Estado dos Mini-Widgets Holográficos e Pomodoro
        self.active_widget = {}
        self.widget_until_ts = 0.0
        self.tasks_cache = []
        self.pomodoro_state = {"active": False, "phase": "FOCUS", "rem_s": 0, "total_s": 1500, "goal": ""}
        self.sp_split_ratio = SpringValue(0.0, stiffness=0.16, damping=0.76)

        # Controle de Piscada com Squash & Stretch
        self.anim_tick = 0.0
        self.blink_phase = 0.0
        self.is_blinking = False
        self.double_blink_pending = False
        self.next_blink_time = time.time() + 2.8

        # Molas Físicas
        self.sp_gaze_x = SpringValue(0.0, stiffness=0.16, damping=0.74)
        self.sp_gaze_y = SpringValue(0.0, stiffness=0.16, damping=0.74)
        self.sp_tilt = SpringValue(0.0, stiffness=0.14, damping=0.76)
        self.sp_eye_squash = SpringValue(1.0, stiffness=0.28, damping=0.66)
        self.sp_mouth_w = SpringValue(36.0, stiffness=0.35, damping=0.66)
        self.sp_mouth_h = SpringValue(5.0, stiffness=0.38, damping=0.64)

        # Comportamento Autônomo em IDLE
        self.next_idle_action_time = time.time() + 2.2
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
        logical_x = event.position().x() / self.SCALE
        # Clicar no card à direita fecha o widget antecipadamente; clicar na cabeça faz carinho
        if self.sp_split_ratio.val > 0.4 and logical_x > 138:
            self.widget_until_ts = 0.0
            return
        if logical_y < 115 and (time.time() - self.last_mouse_pet_ts) > 2.5:
            self.last_mouse_pet_ts = time.time()
            if self.on_pet_callback:
                self.on_pet_callback()
            else:
                self.trigger_emotion_impulse("LOVE")

    def show_holographic_widget(self, widget_data: dict, duration_sec: float = 7.5):
        self.active_widget = widget_data
        self.widget_until_ts = time.time() + duration_sec
        self.sp_eye_squash.set(1.0, impulse=0.20)

    def update_pomodoro_overlay(self, pomo_data: dict):
        self.pomodoro_state = pomo_data
        if pomo_data.get("show_card", False):
            self.show_holographic_widget({"type": "POMODORO"}, duration_sec=7.5)

    def trigger_emotion_impulse(self, emotion: str):
        self.emotion = emotion
        self.sp_eye_squash.set(1.0, impulse=0.32)
        if emotion == "HAPPY":
            self.sp_tilt.set(random.choice([-4.5, 4.5]), impulse=2.0)
            self._spawn_particles("star", 6)
            play_sfx_async("happy")
        elif emotion == "LOVE":
            self.sp_tilt.set(5.5, impulse=2.0)
            self._spawn_particles("heart", 8)
            play_sfx_async("love")
        elif emotion == "WINK":
            self.sp_tilt.set(-7.0, impulse=-2.5)
            self._spawn_particles("star", 4)
            play_sfx_async("wink")
        elif emotion == "SURPRISED":
            self.sp_eye_squash.set(1.12, impulse=0.45)
            self.sp_tilt.set(0.0, impulse=1.5)
            play_sfx_async("surprised")
        elif emotion == "SAD":
            self.sp_tilt.set(-4.0)
            self.sp_gaze_y.set(7.0)
            self._spawn_particles("tear", 3)
            play_sfx_async("sad")
        elif emotion == "ANGRY":
            self.sp_tilt.set(0.0, impulse=3.0)
            play_sfx_async("angry")
        elif emotion == "SLEEPY":
            self.sp_tilt.set(4.0)
            self.sp_gaze_y.set(5.0)
            self._spawn_particles("zzz", 3)
        else:
            self.sp_tilt.set(0.0)

    def set_audio_lipsync(self, amp: float, zcr: float):
        self.audio_amp = amp
        self.audio_zcr = zcr
        self.last_audio_frame_ts = time.time()

    def _spawn_particles(self, ptype: str, count: int):
        for _ in range(count):
            self.particles.append({
                "type": ptype,
                "x": random.uniform(35, 260),
                "y": random.uniform(45, 145),
                "vx": random.uniform(-0.6, 0.6),
                "vy": random.uniform(-1.3, -0.5) if ptype != "tear" else random.uniform(0.8, 1.5),
                "size": random.uniform(5.0, 9.5),
                "alpha": 255.0,
                "phase": random.uniform(0, 6.28)
            })

    def on_frame(self):
        now = time.time()
        self.anim_tick += 0.075

        # 0. Controle do Layout Dividido Híbrido (Mini-Widgets Holográficos)
        if now < self.widget_until_ts and self.active_widget:
            self.sp_split_ratio.set(1.0)
        else:
            self.sp_split_ratio.set(0.0)
        self.sp_split_ratio.step()

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
            if self.state == "IDLE":
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
                    self.sp_tilt.set(0.0)
                    self.idle_curious_squint = 0.0
                else:
                    self.sp_eye_squash.set(1.0, impulse=0.20)
                    self.sp_tilt.set(random.uniform(-4.0, 4.0))
            self.next_idle_action_time = now + random.uniform(1.6, 3.4)

        if self.emotion == "LOVE" and random.random() < 0.08 and len(self.particles) < 8:
            self._spawn_particles("heart", 1)
        elif self.emotion == "SLEEPY" and random.random() < 0.05 and len(self.particles) < 5:
            self._spawn_particles("zzz", 1)
        elif self.emotion == "HAPPY" and random.random() < 0.05 and len(self.particles) < 6:
            self._spawn_particles("star", 1)

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

        self.sp_gaze_x.step()
        self.sp_gaze_y.step()
        self.sp_tilt.step()
        self.sp_eye_squash.step()
        self.sp_mouth_w.step()
        self.sp_mouth_h.step()

        alive = []
        for p in self.particles:
            p["x"] += p["vx"] + math.sin(self.anim_tick + p["phase"]) * 0.25
            p["y"] += p["vy"]
            p["alpha"] -= 4.2
            if p["alpha"] > 8:
                alive.append(p)
        self.particles = alive

        self.update()

    def get_face_color(self) -> QColor:
        if self.state == "LISTEN":
            return QColor(0, 245, 130)
        if self.state == "THINK":
            pulse = int(175 + 65 * math.sin(self.anim_tick * 1.8))
            return QColor(pulse, 80, 255)
        if self.state == "ALERT":
            flash = (int(self.anim_tick * 3) % 2) == 0
            return QColor(255, 115, 0) if flash else QColor(255, 210, 0)
        if self.emotion == "ANGRY":
            return QColor(255, 50, 65)
        if self.emotion == "LOVE":
            return QColor(255, 80, 180)
        if self.emotion == "SAD":
            return QColor(75, 150, 255)
        return QColor(0, 228, 255)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.scale(self.SCALE, self.SCALE)

        # 1. Fundo com Vignette Radial
        bg_path = QPainterPath()
        bg_path.addRoundedRect(QRectF(2, 2, self.LOGICAL_W - 4, self.LOGICAL_H - 4), 22, 22)
        bg_grad = QRadialGradient(QPointF(148, 110), 170)
        bg_grad.setColorAt(0.0, QColor(14, 21, 34))
        bg_grad.setColorAt(1.0, QColor(6, 9, 15))
        painter.fillPath(bg_path, QBrush(bg_grad))

        painter.setPen(QPen(QColor(45, 64, 92), 2))
        painter.drawPath(bg_path)

        # 2. Barra Superior
        painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
        wifi_ok = self.telemetry.get("wifi_ok", False)
        painter.setPen(QColor(0, 230, 120) if wifi_ok else QColor(255, 95, 75))
        ssid = self.telemetry.get("wifi_ssid") or "Offline"
        painter.drawText(14, 16, f"● Wi-Fi: {ssid}"[:22] if wifi_ok else "○ Wi-Fi: Offline")

        temp_c = self.telemetry.get("temp_c", 0.0)
        rem_window = max(0.0, self.continuous_until - time.time())
        if rem_window > 0 and self.state == "IDLE":
            painter.setPen(QColor(74, 222, 128))
            painter.drawText(122, 16, f"🎤 Escuta Ativa ({rem_window:.0f}s)")
        else:
            painter.setPen(QColor(155, 175, 200))
            painter.drawText(138, 16, f"{temp_c:.1f}°C | {self.state}")

        lr, lg, lb = self.led_rgb
        painter.setPen(QPen(QColor(110, 125, 145), 1))
        painter.setBrush(QBrush(QColor(lr, lg, lb)))
        painter.drawEllipse(QPointF(274, 12), 5.5, 5.5)

        # 2b. Barra Neon Discreta de Pomodoro (quando ativo)
        WidgetsOverlayRenderer.draw_pomodoro_top_bar(painter, self.pomodoro_state, self.anim_tick)

        # 3. Transformação 3D do Rosto (com suporte a Layout Dividido Híbrido)
        painter.save()
        split = max(0.0, min(1.0, self.sp_split_ratio.val))
        face_scale = 1.0 - (0.37 * split)
        target_center_x = 148.0 * (1.0 - split) + 72.0 * split
        target_center_y = 112.0 * (1.0 - split) + 114.0 * split

        painter.translate(target_center_x, target_center_y)
        painter.scale(face_scale, face_scale)
        painter.translate(-148.0, -112.0)

        breath_y = math.sin(self.anim_tick * 0.9) * 2.2
        shake_x = random.uniform(-1.8, 1.8) if self.emotion == "ANGRY" else 0.0
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

        squash = self.sp_eye_squash.val
        blink_open = 1.0 - self.blink_phase
        stretch_w = 1.0 + (1.0 - blink_open) * 0.16

        base_w = 68.0 * stretch_w / max(0.8, squash * 0.98)
        base_h = 74.0 * squash * blink_open

        if self.emotion == "SURPRISED":
            base_w *= 1.08
            base_h *= 1.12
        elif self.emotion == "SLEEPY":
            base_h *= 0.35

        left_cx = 92.0 + gx
        right_cx = 204.0 + gx
        eye_cy = 84.0 + gy + breath_y

        if self.emotion in ("HAPPY", "LOVE", "WINK") or self.state == "SPEAK":
            self._draw_blush_cheeks(painter, left_cx - 10, right_cx + 10, eye_cy + 44)

        self._draw_single_eye(
            painter, left_cx, eye_cy,
            base_w * left_persp, max(5.0, base_h * left_persp),
            face_color, is_left=True
        )
        right_h = 6.0 if self.emotion == "WINK" else max(5.0, base_h * right_persp * (1.0 - self.idle_curious_squint))
        self._draw_single_eye(
            painter, right_cx, eye_cy,
            base_w * right_persp, right_h,
            face_color, is_left=False
        )

        # 4. Boca Expressiva Eilik / BMO
        mouth_cx = 148.0 + gx * 0.45
        mouth_cy = 155.0 + gy * 0.35 + breath_y
        self._draw_expressive_mouth(painter, mouth_cx, mouth_cy, face_color)

        painter.restore()

        # 4b. Mini-Widget Holográfico na Direita (quando ativo)
        WidgetsOverlayRenderer.draw_holographic_card(
            painter,
            self.active_widget,
            self.tasks_cache,
            self.pomodoro_state,
            split,
            self.anim_tick
        )

        # 5. Partículas Flutuantes
        self._draw_particles(painter)

        # 6. Barra Inferior de Legenda
        painter.setPen(QPen(QColor(30, 42, 60), 1))
        painter.drawLine(14, 194, self.LOGICAL_W - 14, 194)

        painter.setFont(QFont("Segoe UI", 8))
        painter.setPen(QColor(215, 232, 250))
        painter.drawText(QRectF(12, 197, self.LOGICAL_W - 24, 36), Qt.AlignCenter | Qt.TextWordWrap, self.subtitle)

    def _draw_blush_cheeks(self, painter: QPainter, lx: float, rx: float, cy: float):
        painter.setPen(Qt.NoPen)
        alpha = int(75 + 25 * math.sin(self.anim_tick * 1.5))
        for cx in (lx, rx):
            grad = QRadialGradient(QPointF(cx, cy), 18)
            grad.setColorAt(0.0, QColor(255, 85, 145, alpha))
            grad.setColorAt(1.0, QColor(255, 85, 145, 0))
            painter.setBrush(QBrush(grad))
            painter.drawEllipse(QPointF(cx, cy), 20, 11)

    def _draw_single_eye(self, painter: QPainter, cx: float, cy: float, w: float, h: float, color: QColor, is_left: bool):
        rect = QRectF(cx - w / 2, cy - h / 2, w, h)

        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(QColor(color.red(), color.green(), color.blue(), 48)))
        painter.drawRoundedRect(rect.adjusted(-6, -6, 6, 6), 24, 24)

        grad = QLinearGradient(rect.topLeft(), rect.bottomRight())
        grad.setColorAt(0.0, color.lighter(128))
        grad.setColorAt(0.55, color)
        grad.setColorAt(1.0, color.darker(125))
        painter.setBrush(QBrush(grad))
        painter.drawRoundedRect(rect, 20, 20)

        if h > 22:
            shift_x = self.sp_gaze_x.val * 0.18
            shift_y = self.sp_gaze_y.val * 0.18
            painter.setBrush(QBrush(QColor(255, 255, 255, 210)))
            painter.drawEllipse(QPointF(cx - w * 0.20 + shift_x, cy - h * 0.22 + shift_y), w * 0.13, h * 0.11)
            painter.setBrush(QBrush(QColor(255, 255, 255, 125)))
            painter.drawEllipse(QPointF(cx + w * 0.18 + shift_x, cy + h * 0.20 + shift_y), w * 0.06, h * 0.06)

        bg_mask = QColor(11, 16, 26)
        painter.setBrush(QBrush(bg_mask))

        if self.emotion == "HAPPY" and h > 18:
            mask_rect = QRectF(cx - w * 0.68, cy + h * 0.12, w * 1.36, h * 0.98)
            painter.drawEllipse(mask_rect)
        elif self.emotion == "ANGRY" and h > 18:
            path = QPainterPath()
            if is_left:
                path.moveTo(rect.left() - 4, rect.top() - 4)
                path.lineTo(rect.right() + 4, rect.top() - 4)
                path.lineTo(rect.right() + 4, rect.top() + h * 0.46)
                path.lineTo(rect.left() - 4, rect.top() + h * 0.06)
            else:
                path.moveTo(rect.left() - 4, rect.top() - 4)
                path.lineTo(rect.right() + 4, rect.top() - 4)
                path.lineTo(rect.right() + 4, rect.top() + h * 0.06)
                path.lineTo(rect.left() - 4, rect.top() + h * 0.46)
            path.closeSubpath()
            painter.drawPath(path)
        elif self.emotion == "SAD" and h > 18:
            path = QPainterPath()
            if is_left:
                path.moveTo(rect.left() - 4, rect.top() - 4)
                path.lineTo(rect.right() + 4, rect.top() - 4)
                path.lineTo(rect.right() + 4, rect.top() + h * 0.06)
                path.lineTo(rect.left() - 4, rect.top() + h * 0.45)
            else:
                path.moveTo(rect.left() - 4, rect.top() - 4)
                path.lineTo(rect.right() + 4, rect.top() - 4)
                path.lineTo(rect.right() + 4, rect.top() + h * 0.45)
                path.lineTo(rect.left() - 4, rect.top() + h * 0.06)
            path.closeSubpath()
            painter.drawPath(path)
        elif self.emotion == "LOVE" and h > 24:
            s = 11.0 + 2.0 * math.sin(self.anim_tick * 2.5)
            self._draw_heart_shape(painter, cx, cy + 2, s, QColor(255, 235, 245, 230))

    def _draw_heart_shape(self, painter: QPainter, cx: float, cy: float, size: float, color: QColor):
        path = QPainterPath()
        path.moveTo(cx, cy + size * 0.65)
        path.cubicTo(cx - size * 1.2, cy - size * 0.1, cx - size * 0.6, cy - size * 0.95, cx, cy - size * 0.35)
        path.cubicTo(cx + size * 0.6, cy - size * 0.95, cx + size * 1.2, cy - size * 0.1, cx, cy + size * 0.65)
        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(color))
        painter.drawPath(path)

    def _draw_expressive_mouth(self, painter: QPainter, cx: float, cy: float, color: QColor):
        mouth_inner_bg = QColor(36, 12, 28)
        tongue_color = QColor(255, 105, 140)
        teeth_color = QColor(245, 250, 255)

        if self.state == "SPEAK":
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

        if self.state == "LISTEN":
            pulse_h = 9.0 + 4.5 * abs(math.sin(self.anim_tick * 2.2))
            m_rect = QRectF(cx - 16, cy - pulse_h / 2, 32, pulse_h)
            painter.setPen(QPen(color, 2.5))
            painter.setBrush(QBrush(mouth_inner_bg))
            painter.drawRoundedRect(m_rect, 6, 6)
            return

        if self.state == "THINK":
            painter.setPen(QPen(color, 3.2, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            path = QPainterPath()
            path.moveTo(cx - 15, cy + 2)
            path.cubicTo(cx - 5, cy - 5, cx + 5, cy + 7, cx + 15, cy - 1)
            painter.drawPath(path)
            return

        if self.emotion in ("HAPPY", "LOVE"):
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

        elif self.emotion == "SURPRISED":
            painter.setPen(QPen(color, 2.8))
            painter.setBrush(QBrush(mouth_inner_bg))
            painter.drawEllipse(QPointF(cx, cy + 2), 12, 15)

        elif self.emotion == "SAD":
            painter.setPen(QPen(color, 3.4, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            path = QPainterPath()
            path.moveTo(cx - 20, cy + 8)
            path.quadTo(cx, cy - 7, cx + 20, cy + 8)
            painter.drawPath(path)

        elif self.emotion == "ANGRY":
            painter.setPen(QPen(color, 3.2, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            path = QPainterPath()
            path.moveTo(cx - 22, cy + 5)
            path.lineTo(cx - 8, cy - 2)
            path.lineTo(cx + 8, cy + 5)
            path.lineTo(cx + 22, cy - 2)
            painter.drawPath(path)

        elif self.emotion == "WINK":
            painter.setPen(QPen(color, 3.4, Qt.SolidLine, Qt.RoundCap))
            painter.setBrush(Qt.NoBrush)
            path = QPainterPath()
            path.moveTo(cx - 18, cy + 2)
            path.quadTo(cx + 3, cy + 13, cx + 22, cy - 6)
            painter.drawPath(path)

        elif self.emotion == "SLEEPY":
            r = 5.0 + 1.8 * math.sin(self.anim_tick * 0.8)
            painter.setPen(QPen(color, 2.4))
            painter.setBrush(QBrush(mouth_inner_bg))
            painter.drawEllipse(QPointF(cx, cy + 2), r, r)

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
