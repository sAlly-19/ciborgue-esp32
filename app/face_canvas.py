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
    - Moldura/Cabeça 3D do Ciborgue com auriculares laterais RGB e visor de alto contraste
    - Olhos 3D estilo Cozmo/Eilik com recorte booleano real (QPainterPath.subtracted) sem máscaras escuras
    - Bochechas vibrantes estilo Eilik/BMO com brilho rosado e traços de rubor
    - Boca BMO com LipSync estritamente acoplado ao áudio PCM
    - Layout Dividido Híbrido (Mini-Widgets Holográficos à direita)
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
        self.led_rgb = (0, 180, 255)
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
        return QColor(0, 238, 255)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.scale(self.SCALE, self.SCALE)

        # 1. Fundo Externo do Display 296x240
        bg_path = QPainterPath()
        bg_path.addRoundedRect(QRectF(2, 2, self.LOGICAL_W - 4, self.LOGICAL_H - 4), 22, 22)
        bg_grad = QLinearGradient(QPointF(0, 0), QPointF(0, self.LOGICAL_H))
        bg_grad.setColorAt(0.0, QColor(15, 23, 38))
        bg_grad.setColorAt(1.0, QColor(8, 12, 22))
        painter.fillPath(bg_path, QBrush(bg_grad))

        painter.setPen(QPen(QColor(56, 82, 118), 2))
        painter.drawPath(bg_path)

        # 2. Barra Superior
        painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
        wifi_ok = self.telemetry.get("wifi_ok", False)
        painter.setPen(QColor(0, 235, 125) if wifi_ok else QColor(255, 105, 85))
        ssid = self.telemetry.get("wifi_ssid") or "Offline"
        painter.drawText(14, 16, f"● Wi-Fi: {ssid}"[:22] if wifi_ok else "○ Wi-Fi: Offline")

        temp_c = self.telemetry.get("temp_c", 0.0)
        rem_window = max(0.0, self.continuous_until - time.time())
        if rem_window > 0 and self.state == "IDLE":
            painter.setPen(QColor(74, 222, 128))
            painter.drawText(122, 16, f"🎤 Escuta Ativa ({rem_window:.0f}s)")
        else:
            painter.setPen(QColor(175, 195, 220))
            painter.drawText(138, 16, f"{temp_c:.1f}°C | {self.state}")

        lr, lg, lb = self.led_rgb
        if lr == 0 and lg == 0 and lb == 0:
            lr, lg, lb = (0, 195, 255)
        painter.setPen(QPen(QColor(140, 165, 195), 1))
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

        # 3a. Moldura do Rosto / Capacete Ciborgue (destaca claramente o rosto do fundo!)
        self._draw_cyborg_visor_frame(painter, 148.0, 112.0 + breath_y * 0.5, face_color, (lr, lg, lb))

        left_persp = 1.0 - (gx / 130.0)
        right_persp = 1.0 + (gx / 130.0)

        squash = self.sp_eye_squash.val
        blink_open = 1.0 - self.blink_phase
        stretch_w = 1.0 + (1.0 - blink_open) * 0.16

        base_w = 64.0 * stretch_w / max(0.8, squash * 0.98)
        base_h = 70.0 * squash * blink_open

        if self.emotion == "SURPRISED":
            base_w *= 1.08
            base_h *= 1.12
        elif self.emotion == "SLEEPY":
            base_h *= 0.35

        left_cx = 94.0 + gx
        right_cx = 202.0 + gx
        eye_cy = 88.0 + gy + breath_y

        # 3b. Olhos 3D com Recorte Booleano Real (sem círculos escuros cobrindo as bochechas!)
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

        # 3c. Bochechas Coradas Vibrantes (exibidas apenas no modo Feliz e Apaixonado)
        if self.emotion in ("HAPPY", "LOVE"):
            self._draw_blush_cheeks(painter, left_cx - 12, right_cx + 12, eye_cy + 44, 1.0)

        # 4. Boca Expressiva Eilik / BMO
        mouth_cx = 148.0 + gx * 0.45
        mouth_cy = 152.0 + gy * 0.35 + breath_y
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
        painter.setPen(QPen(QColor(38, 54, 78), 1))
        painter.drawLine(14, 194, self.LOGICAL_W - 14, 194)

        painter.setFont(QFont("Segoe UI", 8))
        painter.setPen(QColor(225, 238, 255))
        painter.drawText(QRectF(12, 197, self.LOGICAL_W - 24, 36), Qt.AlignCenter | Qt.TextWordWrap, self.subtitle)

    def _draw_cyborg_visor_frame(self, painter: QPainter, cx: float, cy: float, face_color: QColor, led_rgb: tuple):
        """Desenha a cabeça/visor estilizado do Ciborgue para destacar o rosto do fundo da tela."""
        head_rect = QRectF(cx - 118, cy - 78, 236, 152)
        visor_rect = QRectF(cx - 110, cy - 71, 220, 138)

        # Auriculares laterais iluminados com a cor do LED RGB
        lr, lg, lb = led_rgb
        ear_color = QColor(lr, lg, lb, 210)
        painter.setPen(QPen(QColor(110, 155, 205), 1.5))
        painter.setBrush(QBrush(ear_color))
        painter.drawRoundedRect(QRectF(head_rect.left() - 6, cy - 26, 10, 52), 4, 4)
        painter.drawRoundedRect(QRectF(head_rect.right() - 4, cy - 26, 10, 52), 4, 4)

        # Moldura metálica Prata-Gelo / Branco Pérola da cabeça (estilo Eilik / Astro Bot / EVE)
        shell_grad = QLinearGradient(head_rect.topLeft(), head_rect.bottomRight())
        shell_grad.setColorAt(0.0, QColor(238, 244, 252))
        shell_grad.setColorAt(0.55, QColor(198, 212, 230))
        shell_grad.setColorAt(1.0, QColor(142, 162, 188))
        painter.setPen(QPen(QColor(face_color.red(), face_color.green(), face_color.blue(), 210), 2.2))
        painter.setBrush(QBrush(shell_grad))
        painter.drawRoundedRect(head_rect, 38, 38)

        # Visor interno Azul-Cibernético profundo (destaca do fundo escuro externo)
        visor_grad = QRadialGradient(QPointF(cx, cy - 10), 130)
        visor_grad.setColorAt(0.0, QColor(28, 52, 88))
        visor_grad.setColorAt(0.7, QColor(18, 34, 60))
        visor_grad.setColorAt(1.0, QColor(12, 22, 42))
        painter.setPen(QPen(QColor(56, 189, 248, 160), 1.6))
        painter.setBrush(QBrush(visor_grad))
        painter.drawRoundedRect(visor_rect, 31, 31)

        # Reflexo de vidro sutil no topo do visor
        shine_path = QPainterPath()
        shine_path.addRoundedRect(QRectF(visor_rect.left() + 18, visor_rect.top() + 5, visor_rect.width() - 36, 12), 6, 6)
        painter.fillPath(shine_path, QBrush(QColor(255, 255, 255, 34)))

    def _draw_blush_cheeks(self, painter: QPainter, lx: float, rx: float, cy: float, intensity: float = 1.0):
        """Desenha bochechas rosadas vibrantes e suaves estilo Eilik/BMO."""
        pulse = math.sin(self.anim_tick * 1.6)
        core_alpha = int((215 + 35 * pulse) * max(0.8, intensity))
        mid_alpha = int((130 + 25 * pulse) * max(0.8, intensity))

        for cx in (lx, rx):
            painter.setPen(Qt.NoPen)
            grad = QRadialGradient(QPointF(cx, cy), 25)
            grad.setColorAt(0.0, QColor(255, 80, 155, min(255, core_alpha)))
            grad.setColorAt(0.45, QColor(255, 65, 140, min(255, mid_alpha)))
            grad.setColorAt(0.85, QColor(255, 65, 140, 25))
            grad.setColorAt(1.0, QColor(255, 65, 140, 0))
            painter.setBrush(QBrush(grad))
            painter.drawEllipse(QPointF(cx, cy), 25, 15)

            # Tracinhos luminosos nas bochechas (estilo anime / Eilik)
            line_alpha = int(240 * max(0.85, intensity))
            painter.setPen(QPen(QColor(255, 205, 230, line_alpha), 1.8, Qt.SolidLine, Qt.RoundCap))
            for offset_x in (-5.0, 0.0, 5.0):
                painter.drawLine(
                    QPointF(cx + offset_x - 2.0, cy + 3.2),
                    QPointF(cx + offset_x + 2.0, cy - 3.2)
                )

    def _draw_single_eye(self, painter: QPainter, cx: float, cy: float, w: float, h: float, color: QColor, is_left: bool):
        rect = QRectF(cx - w / 2, cy - h / 2, w, h)

        # Constrói o caminho base do olho e aplica subtração booleana real para as expressões!
        eye_path = QPainterPath()
        eye_path.addRoundedRect(rect, 20, 20)

        if self.emotion == "HAPPY" and h > 18:
            cut_path = QPainterPath()
            cut_path.addEllipse(QRectF(cx - w * 0.68, cy + h * 0.14, w * 1.36, h * 0.96))
            eye_path = eye_path.subtracted(cut_path)
        elif self.emotion == "ANGRY" and h > 18:
            cut_path = QPainterPath()
            if is_left:
                cut_path.moveTo(rect.left() - 6, rect.top() - 6)
                cut_path.lineTo(rect.right() + 6, rect.top() - 6)
                cut_path.lineTo(rect.right() + 6, rect.top() + h * 0.46)
                cut_path.lineTo(rect.left() - 6, rect.top() + h * 0.06)
            else:
                cut_path.moveTo(rect.left() - 6, rect.top() - 6)
                cut_path.lineTo(rect.right() + 6, rect.top() - 6)
                cut_path.lineTo(rect.right() + 6, rect.top() + h * 0.06)
                cut_path.lineTo(rect.left() - 6, rect.top() + h * 0.46)
            cut_path.closeSubpath()
            eye_path = eye_path.subtracted(cut_path)
        elif self.emotion == "SAD" and h > 18:
            cut_path = QPainterPath()
            if is_left:
                cut_path.moveTo(rect.left() - 6, rect.top() - 6)
                cut_path.lineTo(rect.right() + 6, rect.top() - 6)
                cut_path.lineTo(rect.right() + 6, rect.top() + h * 0.06)
                cut_path.lineTo(rect.left() - 6, rect.top() + h * 0.45)
            else:
                cut_path.moveTo(rect.left() - 6, rect.top() - 6)
                cut_path.lineTo(rect.right() + 6, rect.top() - 6)
                cut_path.lineTo(rect.right() + 6, rect.top() + h * 0.45)
                cut_path.lineTo(rect.left() - 6, rect.top() + h * 0.06)
            cut_path.closeSubpath()
            eye_path = eye_path.subtracted(cut_path)

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

        # Reflexos especulares (recortados dentro do olho)
        if h > 22:
            painter.save()
            painter.setClipPath(eye_path)
            shift_x = self.sp_gaze_x.val * 0.18
            shift_y = self.sp_gaze_y.val * 0.18
            painter.setBrush(QBrush(QColor(255, 255, 255, 225)))
            painter.drawEllipse(QPointF(cx - w * 0.20 + shift_x, cy - h * 0.22 + shift_y), w * 0.13, h * 0.11)
            painter.setBrush(QBrush(QColor(255, 255, 255, 135)))
            painter.drawEllipse(QPointF(cx + w * 0.18 + shift_x, cy + h * 0.20 + shift_y), w * 0.06, h * 0.06)

            if self.emotion == "LOVE" and h > 24:
                s = 11.0 + 2.0 * math.sin(self.anim_tick * 2.5)
                self._draw_heart_shape(painter, cx, cy + 2, s, QColor(255, 240, 248, 240))
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
        mouth_inner_bg = QColor(42, 14, 34)
        tongue_color = QColor(255, 105, 145)
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
