import math
from datetime import datetime
from PySide6.QtCore import Qt, QRectF, QPointF
from PySide6.QtGui import (
    QPainter, QColor, QPen, QBrush, QFont, QFontMetrics,
    QPainterPath, QLinearGradient
)


_PT_WEEKDAYS = ["Segunda", "Terça", "Quarta", "Quinta", "Sexta", "Sábado", "Domingo"]
_PT_WEEKDAYS_SHORT = ["SEG", "TER", "QUA", "QUI", "SEX", "SÁB", "DOM"]
_PT_MONTHS = ["Jan", "Fev", "Mar", "Abr", "Mai", "Jun", "Jul", "Ago", "Set", "Out", "Nov", "Dez"]


class WidgetsOverlayRenderer:
    """
    Renderizador Híbrido v2 (100% vetorial, zero emojis de texto) para a tela Estardyn 296x240:
    1. Pílula minimalista superior (`draw_top_pill`): visível APENAS durante Alarme, Pomodoro, Timer, Portal AP ou Offline.
    2. Cards de Tela Cheia (`draw_fullscreen_card`): substituem o rosto com fade suave sem cortar a tela ao meio.
    """

    @staticmethod
    def _pen(color: QColor, width: float = 1.8) -> QPen:
        return QPen(color, width, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin)

    @staticmethod
    def _format_mm_ss(rem_s: int) -> str:
        s = max(0, int(rem_s))
        mins, secs = divmod(s, 60)
        hours, mins = divmod(mins, 60)
        if hours > 0:
            return f"{hours:02d}:{mins:02d}"
        return f"{mins:02d}:{secs:02d}"

    @staticmethod
    def _elide(painter: QPainter, text: str, max_width: float) -> str:
        fm = QFontMetrics(painter.font())
        return fm.elidedText(str(text), Qt.ElideRight, max(20, int(max_width)))

    # =========================================================================
    # ÍCONES VETORIAIS PUROS (SEM EMOJIS / SEM BITMAPS)
    # =========================================================================
    @classmethod
    def _draw_icon_checklist(cls, painter: QPainter, cx: float, cy: float, r: float, color: QColor):
        painter.save()
        painter.setPen(cls._pen(color, 1.7))
        painter.setBrush(Qt.NoBrush)
        box = QRectF(cx - r * 0.72, cy - r * 0.82, r * 1.44, r * 1.64)
        painter.drawRoundedRect(box, 2.5, 2.5)
        clip = QRectF(cx - r * 0.34, cy - r * 0.96, r * 0.68, r * 0.28)
        painter.drawRoundedRect(clip, 1.5, 1.5)
        for dy in (-0.20, 0.28):
            y = cy + r * dy
            painter.drawLine(QPointF(cx - r * 0.42, y), QPointF(cx - r * 0.28, y))
            painter.drawLine(QPointF(cx - r * 0.12, y), QPointF(cx + r * 0.44, y))
        painter.restore()

    @classmethod
    def _draw_icon_check_circle(cls, painter: QPainter, cx: float, cy: float, r: float, color: QColor):
        painter.save()
        painter.setPen(cls._pen(color, 2.2))
        painter.setBrush(QBrush(QColor(color.red(), color.green(), color.blue(), 26)))
        painter.drawEllipse(QPointF(cx, cy), r, r)
        check = QPainterPath()
        check.moveTo(cx - r * 0.42, cy + r * 0.04)
        check.lineTo(cx - r * 0.10, cy + r * 0.36)
        check.lineTo(cx + r * 0.46, cy - r * 0.30)
        painter.setBrush(Qt.NoBrush)
        painter.drawPath(check)
        painter.restore()

    @classmethod
    def _draw_icon_clock(cls, painter: QPainter, cx: float, cy: float, r: float, color: QColor):
        painter.save()
        painter.setPen(cls._pen(color, 1.8))
        painter.setBrush(Qt.NoBrush)
        painter.drawEllipse(QPointF(cx, cy), r * 0.85, r * 0.85)
        painter.drawLine(QPointF(cx, cy), QPointF(cx, cy - r * 0.48))
        painter.drawLine(QPointF(cx, cy), QPointF(cx + r * 0.38, cy + r * 0.16))
        painter.restore()

    @classmethod
    def _draw_icon_timer(cls, painter: QPainter, cx: float, cy: float, r: float, color: QColor):
        painter.save()
        painter.setPen(cls._pen(color, 1.7))
        painter.setBrush(Qt.NoBrush)
        painter.drawLine(QPointF(cx - r * 0.28, cy - r * 0.92), QPointF(cx + r * 0.28, cy - r * 0.92))
        painter.drawEllipse(QPointF(cx, cy + r * 0.08), r * 0.74, r * 0.74)
        painter.drawLine(QPointF(cx, cy + r * 0.08), QPointF(cx + r * 0.36, cy - r * 0.24))
        painter.restore()

    @classmethod
    def _draw_icon_bell(cls, painter: QPainter, cx: float, cy: float, r: float, color: QColor, tilt_deg: float = 0.0):
        painter.save()
        painter.translate(cx, cy)
        painter.rotate(tilt_deg)
        painter.setPen(cls._pen(color, 1.8))
        painter.setBrush(QBrush(QColor(color.red(), color.green(), color.blue(), 42)))

        bell = QPainterPath()
        bell.moveTo(-r * 0.65, r * 0.42)
        bell.cubicTo(-r * 0.56, r * 0.10, -r * 0.52, -r * 0.62, 0.0, -r * 0.68)
        bell.cubicTo(r * 0.52, -r * 0.62, r * 0.56, r * 0.10, r * 0.65, r * 0.42)
        bell.lineTo(-r * 0.65, r * 0.42)
        bell.closeSubpath()
        painter.drawPath(bell)
        painter.drawLine(QPointF(-r * 0.80, r * 0.42), QPointF(r * 0.80, r * 0.42))
        painter.drawArc(QRectF(-r * 0.22, r * 0.36, r * 0.44, r * 0.36), 180 * 16, 180 * 16)
        painter.drawPoint(QPointF(0.0, -r * 0.82))
        painter.restore()

    @classmethod
    def _draw_icon_weather(cls, painter: QPainter, cx: float, cy: float, r: float, desc: str = ""):
        painter.save()
        d = (desc or "").lower()
        is_rain = any(k in d for k in ("chuv", "tempest", "garoa", "rain"))
        is_cloud = any(k in d for k in ("nubl", "nuv", "cloud", "encoberto", "nebl"))

        if is_cloud or is_rain:
            sun_cx = cx - r * 0.28
            sun_cy = cy - r * 0.28
            sun_r = r * 0.34
        else:
            sun_cx = cx
            sun_cy = cy
            sun_r = r * 0.44
        sun_col = QColor(251, 191, 36)

        painter.setPen(cls._pen(sun_col, 1.8))
        painter.setBrush(QBrush(QColor(251, 191, 36, 190)))
        painter.drawEllipse(QPointF(sun_cx, sun_cy), sun_r, sun_r)

        ray_in = sun_r + r * 0.14
        ray_out = sun_r + r * 0.30
        for i in range(8):
            ang = i * (math.pi / 4.0)
            painter.drawLine(
                QPointF(sun_cx + math.cos(ang) * ray_in, sun_cy + math.sin(ang) * ray_in),
                QPointF(sun_cx + math.cos(ang) * ray_out, sun_cy + math.sin(ang) * ray_out)
            )

        if is_cloud or is_rain:
            cloud = QPainterPath()
            ccx, ccy = cx + r * 0.12, cy + r * 0.22
            cloud.addRoundedRect(QRectF(ccx - r * 0.62, ccy - r * 0.16, r * 1.24, r * 0.44), r * 0.22, r * 0.22)
            cloud.addEllipse(QPointF(ccx - r * 0.18, ccy - r * 0.18), r * 0.30, r * 0.26)
            cloud.addEllipse(QPointF(ccx + r * 0.18, ccy - r * 0.12), r * 0.22, r * 0.20)
            painter.setPen(cls._pen(QColor(15, 24, 43), 2.4))
            painter.setBrush(QBrush(QColor(226, 232, 240, 242)))
            painter.drawPath(cloud)
            painter.setPen(cls._pen(QColor(226, 232, 240), 1.0))
            painter.setBrush(Qt.NoBrush)
            painter.drawPath(cloud)

        if is_rain:
            painter.setPen(cls._pen(QColor(56, 189, 248), 1.8))
            for dx in (-0.30, 0.05, 0.40):
                painter.drawLine(
                    QPointF(cx + r * dx, cy + r * 0.62),
                    QPointF(cx + r * (dx - 0.12), cy + r * 0.88)
                )
        painter.restore()

    @classmethod
    def _draw_icon_heart(cls, painter: QPainter, cx: float, cy: float, r: float, color: QColor):
        painter.save()
        s = r * 0.92
        path = QPainterPath()
        path.moveTo(cx, cy + s * 0.58)
        path.cubicTo(cx - s * 1.15, cy - s * 0.05, cx - s * 0.60, cy - s * 0.95, cx, cy - s * 0.32)
        path.cubicTo(cx + s * 0.60, cy - s * 0.95, cx + s * 1.15, cy - s * 0.05, cx, cy + s * 0.58)
        painter.setPen(cls._pen(color, 1.4))
        painter.setBrush(QBrush(color))
        painter.drawPath(path)
        painter.restore()

    @classmethod
    def _draw_icon_bolt(cls, painter: QPainter, cx: float, cy: float, r: float, color: QColor):
        painter.save()
        bolt = QPainterPath()
        bolt.moveTo(cx + r * 0.14, cy - r * 0.85)
        bolt.lineTo(cx - r * 0.46, cy + r * 0.06)
        bolt.lineTo(cx + r * 0.02, cy + r * 0.06)
        bolt.lineTo(cx - r * 0.16, cy + r * 0.85)
        bolt.lineTo(cx + r * 0.48, cy - r * 0.06)
        bolt.lineTo(cx - r * 0.02, cy - r * 0.06)
        bolt.closeSubpath()
        painter.setPen(cls._pen(color, 1.2))
        painter.setBrush(QBrush(color))
        painter.drawPath(bolt)
        painter.restore()

    @classmethod
    def _draw_icon_star(cls, painter: QPainter, cx: float, cy: float, r: float, color: QColor):
        painter.save()
        star = QPainterPath()
        for i in range(10):
            rad = r * 0.88 if (i % 2 == 0) else r * 0.38
            ang = -math.pi / 2.0 + i * (math.pi / 5.0)
            pt = QPointF(cx + math.cos(ang) * rad, cy + math.sin(ang) * rad)
            if i == 0:
                star.moveTo(pt)
            else:
                star.lineTo(pt)
        star.closeSubpath()
        painter.setPen(cls._pen(color, 1.2))
        painter.setBrush(QBrush(color))
        painter.drawPath(star)
        painter.restore()

    @classmethod
    def _draw_icon_monitor(cls, painter: QPainter, cx: float, cy: float, r: float, color: QColor):
        painter.save()
        painter.setPen(cls._pen(color, 1.7))
        painter.setBrush(QBrush(QColor(color.red(), color.green(), color.blue(), 26)))
        painter.drawRoundedRect(QRectF(cx - r * 0.82, cy - r * 0.68, r * 1.64, r * 1.06), 2.5, 2.5)
        painter.drawLine(QPointF(cx, cy + r * 0.38), QPointF(cx, cy + r * 0.70))
        painter.drawLine(QPointF(cx - r * 0.44, cy + r * 0.72), QPointF(cx + r * 0.44, cy + r * 0.72))
        painter.restore()

    @classmethod
    def _draw_icon_wifi(cls, painter: QPainter, cx: float, cy: float, r: float, color: QColor):
        painter.save()
        painter.setPen(cls._pen(color, 1.6))
        painter.setBrush(Qt.NoBrush)
        painter.drawArc(QRectF(cx - r * 0.78, cy - r * 0.38, r * 1.56, r * 1.56), 45 * 16, 90 * 16)
        painter.drawArc(QRectF(cx - r * 0.44, cy - r * 0.04, r * 0.88, r * 0.88), 45 * 16, 90 * 16)
        painter.setBrush(QBrush(color))
        painter.drawEllipse(QPointF(cx, cy + r * 0.48), r * 0.14, r * 0.14)
        painter.restore()

    # =========================================================================
    # 1. PÍLULA MINIMALISTA SUPERIOR (SÓ QUANDO ALGO ATIVO REQUER ATENÇÃO)
    # =========================================================================
    @classmethod
    def draw_top_pill(cls, painter: QPainter, pomodoro_state: dict, timer_state: dict,
                      alarm_ringing: bool, telemetry: dict, anim_tick: float):
        kind = ""
        text = ""
        accent = QColor(56, 189, 248)

        if alarm_ringing:
            kind = "BELL"
            text = "ALARME ATIVO"
            flash = (int(anim_tick * 4) % 2) == 0
            accent = QColor(239, 68, 68) if flash else QColor(245, 158, 11)
        elif pomodoro_state and pomodoro_state.get("active", False):
            kind = "TIMER"
            phase = pomodoro_state.get("phase", "FOCUS")
            rem_s = int(pomodoro_state.get("rem_s", 0))
            accent = QColor(251, 113, 133) if phase == "FOCUS" else QColor(52, 211, 153)
            label = "FOCO" if phase == "FOCUS" else "PAUSA"
            text = f"{label} {cls._format_mm_ss(rem_s)}"
        elif timer_state and timer_state.get("active", False) and int(timer_state.get("rem_s", 0)) > 0:
            kind = "TIMER"
            rem_s = int(timer_state.get("rem_s", 0))
            accent = QColor(251, 191, 36)
            lbl = str(timer_state.get("label", "")).strip()
            t_str = cls._format_mm_ss(rem_s)
            text = f"TIMER {t_str}" if not lbl else f"{t_str} · {lbl[:12]}"
        elif telemetry and telemetry.get("portal_active", False):
            kind = "WIFI"
            accent = QColor(56, 189, 248)
            ap_ip = telemetry.get("ap_ip") or "192.168.4.1"
            text = f"AP {ap_ip}"
        elif telemetry and ("uptime_s" in telemetry or telemetry.get("net_mode") == "OFFLINE"):
            is_offline = (telemetry.get("net_mode") == "OFFLINE") or (not telemetry.get("wifi_ok", True))
            if is_offline:
                kind = "DOT"
                accent = QColor(245, 158, 11)
                text = "MODO OFFLINE"

        if not kind:
            return

        painter.save()
        font = QFont("Segoe UI", 7, QFont.Bold)
        painter.setFont(font)
        fm = QFontMetrics(font)
        text_w = fm.horizontalAdvance(text)
        pill_w = min(230.0, max(108.0, float(text_w) + 34.0))
        pill_h = 20.0
        pill_x = (296.0 - pill_w) / 2.0
        pill_y = 6.0

        rect = QRectF(pill_x, pill_y, pill_w, pill_h)
        bg = QPainterPath()
        bg.addRoundedRect(rect, 10.0, 10.0)
        painter.fillPath(bg, QBrush(QColor(9, 15, 28, 224)))
        painter.setPen(cls._pen(QColor(accent.red(), accent.green(), accent.blue(), 190), 1.3))
        painter.drawPath(bg)

        icx = pill_x + 12.0
        icy = pill_y + pill_h / 2.0
        if kind == "BELL":
            cls._draw_icon_bell(painter, icx, icy, 5.2, accent, math.sin(anim_tick * 5.0) * 12.0)
        elif kind == "TIMER":
            cls._draw_icon_timer(painter, icx, icy, 5.2, accent)
        elif kind == "WIFI":
            cls._draw_icon_wifi(painter, icx, icy, 5.2, accent)
        else:
            painter.setPen(Qt.NoPen)
            painter.setBrush(QBrush(accent))
            painter.drawEllipse(QPointF(icx, icy), 3.0, 3.0)

        painter.setPen(QColor(241, 245, 249))
        painter.drawText(
            QRectF(pill_x + 22.0, pill_y, pill_w - 28.0, pill_h),
            Qt.AlignVCenter | Qt.AlignLeft,
            text
        )
        painter.restore()

    # =========================================================================
    # 2. CARDS DE TELA CHEIA (SOBREPÕEM O ROSTO INTEIRO COM ALPHA SUAVE)
    # =========================================================================
    @classmethod
    def draw_fullscreen_card(cls, painter: QPainter, active_widget: dict, tasks_cache: list,
                             pomodoro_state: dict, timer_state: dict, personality_state: dict,
                             weather_cache: dict, alarms_cache: list,
                             overlay_alpha: float, remaining_ratio: float, anim_tick: float):
        if overlay_alpha <= 0.02 or not active_widget:
            return

        wtype = str(active_widget.get("type", "")).upper()
        if not wtype or wtype == "NONE":
            return

        alpha = max(0.0, min(1.0, float(overlay_alpha)))
        painter.save()
        painter.setOpacity(alpha)

        # Escurece suavemente o fundo do visor 296x240 atrás do card
        scrim = QPainterPath()
        scrim.addRoundedRect(QRectF(2, 2, 292, 236), 22, 22)
        painter.fillPath(scrim, QBrush(QColor(5, 9, 18, 196)))

        # Cores de destaque e configuração por tipo de card
        pulse = 0.5 + 0.5 * math.sin(anim_tick * 4.5)
        accent = QColor(56, 189, 248)
        title = "INFORMAÇÃO"
        badge = ""
        persistent = False

        if wtype == "TASKS":
            accent = QColor(56, 189, 248)
            title = "TAREFAS"
            pending = [t for t in (tasks_cache or []) if not t.get("done", False)]
            badge = f"{len(pending)} PEND." if pending else "EM DIA"
        elif wtype == "WEATHER":
            accent = QColor(251, 191, 36)
            title = "CLIMA AGORA"
            city = str(active_widget.get("city") or weather_cache.get("city") or "Brasil")
            badge = city.upper()[:14]
        elif wtype == "CLOCK":
            accent = QColor(167, 139, 250)
            title = "HORA & DATA"
            badge = "LOCAL"
        elif wtype == "POMODORO":
            phase = str(pomodoro_state.get("phase") or active_widget.get("phase") or "FOCUS").upper()
            accent = QColor(251, 113, 133) if phase == "FOCUS" else QColor(52, 211, 153)
            title = "POMODORO"
            badge = "FOCO" if phase == "FOCUS" else "PAUSA"
        elif wtype == "TIMER":
            accent = QColor(245, 158, 11)
            title = "TIMER"
            t_active = bool(timer_state.get("active", active_widget.get("active", True)))
            badge = "ATIVO" if t_active else "CONCLUÍDO"
        elif wtype == "DESK_CLOCK":
            accent = QColor(56, 189, 248)
            persistent = True
        elif wtype == "MOOD":
            accent = QColor(244, 114, 182)
            title = "HUMOR & VÍNCULO"
            pet = personality_state or active_widget
            badge = str(pet.get("label", "Animado")).upper()
        elif wtype == "ALARM_RING":
            accent = QColor(239, 68, 68) if pulse > 0.45 else QColor(245, 158, 11)
            persistent = True
        elif wtype == "PC_CONTROL":
            accent = QColor(34, 211, 238)
            title = "CONTROLE DO PC"
            badge = "OK"

        # Container principal do Card (280x224 centralizado)
        card_rect = QRectF(8.0, 8.0, 280.0, 224.0)
        card_path = QPainterPath()
        card_path.addRoundedRect(card_rect, 18.0, 18.0)

        card_grad = QLinearGradient(card_rect.topLeft(), card_rect.bottomRight())
        card_grad.setColorAt(0.0, QColor(18, 28, 48, 246))
        card_grad.setColorAt(1.0, QColor(9, 14, 26, 250))
        painter.fillPath(card_path, QBrush(card_grad))

        border_alpha = 235 if wtype == "ALARM_RING" else 175
        border_w = 2.2 if wtype == "ALARM_RING" else 1.5
        painter.setPen(cls._pen(QColor(accent.red(), accent.green(), accent.blue(), border_alpha), border_w))
        painter.drawPath(card_path)

        # Conteúdo específico do Card
        if wtype == "DESK_CLOCK":
            cls._draw_body_desk_clock(painter, weather_cache, alarms_cache)
        elif wtype == "ALARM_RING":
            cls._draw_body_alarm_ring(painter, active_widget, accent, pulse, anim_tick)
        else:
            cls._draw_card_header(painter, wtype, title, badge, accent, active_widget, weather_cache)
            if wtype == "TASKS":
                cls._draw_body_tasks(painter, tasks_cache or [])
            elif wtype == "WEATHER":
                cls._draw_body_weather(painter, active_widget, weather_cache)
            elif wtype == "CLOCK":
                cls._draw_body_clock(painter, active_widget)
            elif wtype == "POMODORO":
                cls._draw_body_pomodoro(painter, pomodoro_state, active_widget, accent)
            elif wtype == "TIMER":
                cls._draw_body_timer(painter, timer_state, active_widget, accent)
            elif wtype == "MOOD":
                cls._draw_body_mood(painter, personality_state, active_widget)
            elif wtype == "PC_CONTROL":
                cls._draw_body_pc_control(painter, active_widget, accent)

        # Barra fina de auto-dispensa na base (apenas para cards temporários)
        if not persistent:
            ratio = max(0.0, min(1.0, float(remaining_ratio)))
            track = QPainterPath()
            track.addRoundedRect(QRectF(24.0, 218.0, 248.0, 3.5), 1.75, 1.75)
            painter.fillPath(track, QBrush(QColor(30, 41, 59, 190)))

            if ratio > 0.02:
                fill_bar = QPainterPath()
                fill_bar.addRoundedRect(QRectF(24.0, 218.0, 248.0 * ratio, 3.5), 1.75, 1.75)
                painter.fillPath(fill_bar, QBrush(QColor(accent.red(), accent.green(), accent.blue(), 220)))

            painter.setFont(QFont("Segoe UI", 6))
            painter.setPen(QColor(100, 116, 139))
            painter.drawText(QRectF(24.0, 203.0, 248.0, 13.0), Qt.AlignCenter, "Toque no visor para fechar")

        painter.restore()

    @classmethod
    def _draw_card_header(cls, painter: QPainter, wtype: str, title: str, badge: str,
                          accent: QColor, active_widget: dict, weather_cache: dict):
        # Medalhão do ícone à esquerda
        pill = QPainterPath()
        pill.addRoundedRect(QRectF(20.0, 18.0, 26.0, 26.0), 8.0, 8.0)
        painter.fillPath(pill, QBrush(QColor(accent.red(), accent.green(), accent.blue(), 34)))
        painter.setPen(cls._pen(QColor(accent.red(), accent.green(), accent.blue(), 130), 1.0))
        painter.drawPath(pill)

        icx, icy = 33.0, 31.0
        if wtype == "TASKS":
            cls._draw_icon_checklist(painter, icx, icy, 8.2, accent)
        elif wtype == "WEATHER":
            desc = str(active_widget.get("desc") or weather_cache.get("desc") or "")
            cls._draw_icon_weather(painter, icx, icy, 8.2, desc)
        elif wtype == "CLOCK":
            cls._draw_icon_clock(painter, icx, icy, 8.2, accent)
        elif wtype == "POMODORO" or wtype == "TIMER":
            cls._draw_icon_timer(painter, icx, icy, 8.2, accent)
        elif wtype == "MOOD":
            cls._draw_icon_heart(painter, icx, icy, 7.6, accent)
        elif wtype == "PC_CONTROL":
            cls._draw_icon_monitor(painter, icx, icy, 8.0, accent)

        bw = 0.0
        if badge:
            bfont = QFont("Segoe UI", 7, QFont.Bold)
            painter.setFont(bfont)
            fm = QFontMetrics(bfont)
            bw = min(98.0, float(fm.horizontalAdvance(badge)) + 16.0)
            brect = QRectF(276.0 - bw, 22.0, bw, 18.0)
            bpath = QPainterPath()
            bpath.addRoundedRect(brect, 9.0, 9.0)
            painter.fillPath(bpath, QBrush(QColor(accent.red(), accent.green(), accent.blue(), 36)))
            painter.setPen(cls._pen(QColor(accent.red(), accent.green(), accent.blue(), 150), 1.0))
            painter.drawPath(bpath)
            painter.setPen(accent)
            painter.drawText(brect, Qt.AlignCenter, cls._elide(painter, badge, bw - 8.0))

        title_max_w = (276.0 - bw - 8.0 - 54.0) if bw > 0 else 222.0
        painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
        painter.setPen(QColor(241, 245, 249))
        painter.drawText(
            QRectF(54.0, 18.0, max(40.0, title_max_w), 26.0),
            Qt.AlignVCenter | Qt.AlignLeft,
            cls._elide(painter, title, title_max_w)
        )

        # Linha separadora sutil
        painter.setPen(cls._pen(QColor(51, 65, 85, 135), 1.0))
        painter.drawLine(QPointF(20.0, 51.0), QPointF(276.0, 51.0))

    # =========================================================================
    # CORPOS DOS 9 CARDS DE TELA CHEIA
    # =========================================================================
    @classmethod
    def _draw_body_tasks(cls, painter: QPainter, tasks: list):
        pending = [t for t in tasks if not t.get("done", False)]
        if not pending:
            cls._draw_icon_check_circle(painter, 148.0, 100.0, 21.0, QColor(52, 211, 153))
            painter.setFont(QFont("Segoe UI", 9, QFont.Bold))
            painter.setPen(QColor(226, 232, 240))
            painter.drawText(QRectF(24.0, 130.0, 248.0, 22.0), Qt.AlignCenter, "Nenhuma tarefa pendente")
            painter.setFont(QFont("Segoe UI", 7))
            painter.setPen(QColor(148, 163, 184))
            painter.drawText(QRectF(24.0, 152.0, 248.0, 20.0), Qt.AlignCenter, 'Diga "Adicione uma tarefa..."')
            return

        y = 60.0
        for t in pending[:4]:
            row = QRectF(20.0, y, 256.0, 30.0)
            rpath = QPainterPath()
            rpath.addRoundedRect(row, 8.0, 8.0)
            painter.fillPath(rpath, QBrush(QColor(255, 255, 255, 12)))
            painter.setPen(cls._pen(QColor(56, 189, 248, 60), 1.0))
            painter.drawPath(rpath)

            painter.setPen(cls._pen(QColor(56, 189, 248), 1.6))
            painter.setBrush(Qt.NoBrush)
            painter.drawEllipse(QPointF(34.0, y + 15.0), 5.5, 5.5)

            painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
            painter.setPen(QColor(241, 245, 249))
            title_txt = cls._elide(painter, t.get("title", "Tarefa"), 216.0)
            painter.drawText(QRectF(47.0, y, 220.0, 30.0), Qt.AlignVCenter | Qt.AlignLeft, title_txt)
            y += 35.0

    @classmethod
    def _draw_body_weather(cls, painter: QPainter, active_widget: dict, weather_cache: dict):
        data = weather_cache if (active_widget.get("temp") is None and weather_cache) else active_widget
        temp = data.get("temp", 25.0)
        desc = str(data.get("desc", "Ensolarado"))
        humidity = data.get("humidity", 55)
        feels = data.get("feels_like", round(float(temp)))

        # Hero esquerdo: Ícone climático vetorial grande
        cls._draw_icon_weather(painter, 68.0, 102.0, 28.0, desc)

        # Hero direito: Temperatura + Descrição
        painter.setFont(QFont("Segoe UI", 26, QFont.Bold))
        painter.setPen(QColor(248, 250, 252))
        painter.drawText(QRectF(112.0, 62.0, 162.0, 44.0), Qt.AlignLeft | Qt.AlignVCenter, f"{float(temp):.0f}°C")

        painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
        painter.setPen(QColor(251, 191, 36))
        painter.drawText(QRectF(114.0, 108.0, 158.0, 22.0), Qt.AlignLeft | Qt.AlignVCenter, cls._elide(painter, desc, 156.0))

        # Duas cápsulas inferiores (Umidade e Sensação Térmica)
        for idx, (lbl, val, col) in enumerate((
            ("UMIDADE", f"{int(humidity)}%", QColor(56, 189, 248)),
            ("SENSAÇÃO", f"{int(feels)}°C", QColor(251, 191, 36)),
        )):
            bx = 20.0 + idx * 132.0
            brect = QRectF(bx, 146.0, 124.0, 46.0)
            bp = QPainterPath()
            bp.addRoundedRect(brect, 10.0, 10.0)
            painter.fillPath(bp, QBrush(QColor(255, 255, 255, 11)))
            painter.setPen(cls._pen(QColor(col.red(), col.green(), col.blue(), 65), 1.0))
            painter.drawPath(bp)

            painter.setFont(QFont("Segoe UI", 6, QFont.Bold))
            painter.setPen(QColor(148, 163, 184))
            painter.drawText(QRectF(bx, 151.0, 124.0, 14.0), Qt.AlignCenter, lbl)

            painter.setFont(QFont("Segoe UI", 11, QFont.Bold))
            painter.setPen(col)
            painter.drawText(QRectF(bx, 166.0, 124.0, 22.0), Qt.AlignCenter, val)

    @classmethod
    def _draw_body_clock(cls, painter: QPainter, active_widget: dict):
        now = datetime.now()
        time_str = str(active_widget.get("time") or now.strftime("%H:%M"))
        sec_str = now.strftime(":%S")
        date_str = str(active_widget.get("date") or f"{now.day:02d} de {_PT_MONTHS[now.month - 1]}")
        wd_str = str(active_widget.get("weekday") or _PT_WEEKDAYS[now.weekday()])

        f_big = QFont("Segoe UI", 28, QFont.Bold)
        f_sec = QFont("Segoe UI", 11, QFont.Bold)
        big_w = QFontMetrics(f_big).horizontalAdvance(time_str)
        sec_w = QFontMetrics(f_sec).horizontalAdvance(sec_str)
        start_x = 148.0 - ((big_w + 4.0 + sec_w) / 2.0)

        painter.setFont(f_big)
        painter.setPen(QColor(248, 250, 252))
        painter.drawText(QRectF(start_x, 68.0, big_w + 4.0, 56.0), Qt.AlignLeft | Qt.AlignVCenter, time_str)

        painter.setFont(f_sec)
        painter.setPen(QColor(167, 139, 250))
        painter.drawText(QRectF(start_x + big_w + 4.0, 82.0, sec_w + 6.0, 34.0), Qt.AlignLeft | Qt.AlignVCenter, sec_str)

        cap = QRectF(32.0, 142.0, 232.0, 42.0)
        cp = QPainterPath()
        cp.addRoundedRect(cap, 12.0, 12.0)
        painter.fillPath(cp, QBrush(QColor(167, 139, 250, 22)))
        painter.setPen(cls._pen(QColor(167, 139, 250, 85), 1.0))
        painter.drawPath(cp)

        painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
        painter.setPen(QColor(226, 232, 240))
        painter.drawText(cap, Qt.AlignCenter, cls._elide(painter, f"{wd_str}, {date_str}", 216.0))

    @classmethod
    def _draw_progress_ring_card(cls, painter: QPainter, cx: float, cy: float, radius: float,
                                 rem_s: int, total_s: int, accent: QColor,
                                 kicker: str, headline: str, foot_note: str):
        ratio = max(0.0, min(1.0, float(rem_s) / max(1.0, float(total_s))))
        ring_rect = QRectF(cx - radius, cy - radius, radius * 2.0, radius * 2.0)

        painter.setPen(cls._pen(QColor(30, 41, 59, 220), 6.0))
        painter.setBrush(Qt.NoBrush)
        painter.drawEllipse(ring_rect)

        if ratio > 0.01:
            painter.setPen(cls._pen(accent, 6.0))
            painter.drawArc(ring_rect, 90 * 16, int(-360.0 * ratio * 16.0))

        painter.setFont(QFont("Segoe UI", 11, QFont.Bold))
        painter.setPen(QColor(248, 250, 252))
        painter.drawText(ring_rect, Qt.AlignCenter, cls._format_mm_ss(rem_s))

        # Coluna de informação à direita
        rx = cx + radius + 16.0
        rw = 274.0 - rx
        painter.setFont(QFont("Segoe UI", 6, QFont.Bold))
        painter.setPen(accent)
        painter.drawText(QRectF(rx, 74.0, rw, 16.0), Qt.AlignLeft | Qt.AlignVCenter, kicker)

        painter.setFont(QFont("Segoe UI", 9, QFont.Bold))
        painter.setPen(QColor(241, 245, 249))
        painter.drawText(QRectF(rx, 94.0, rw, 60.0), Qt.AlignLeft | Qt.AlignTop | Qt.TextWordWrap, headline)

        painter.setFont(QFont("Segoe UI", 6))
        painter.setPen(QColor(148, 163, 184))
        painter.drawText(QRectF(rx, 162.0, rw, 18.0), Qt.AlignLeft | Qt.AlignVCenter, cls._elide(painter, foot_note, rw))

    @classmethod
    def _draw_body_pomodoro(cls, painter: QPainter, pomodoro_state: dict, active_widget: dict, accent: QColor):
        src = pomodoro_state if (pomodoro_state and "rem_s" in pomodoro_state) else active_widget
        rem_s = int(src.get("rem_s", 1500))
        total_s = max(1, int(src.get("total_s", 1500)))
        goal = str(src.get("goal") or "Foco Total")
        active = bool(src.get("active", True))
        cls._draw_progress_ring_card(
            painter, 74.0, 128.0, 40.0, rem_s, total_s, accent,
            "META ATUAL", goal, "Em andamento" if active else "Pausado"
        )

    @classmethod
    def _draw_body_timer(cls, painter: QPainter, timer_state: dict, active_widget: dict, accent: QColor):
        src = timer_state if (timer_state and timer_state.get("active")) else active_widget
        rem_s = int(src.get("rem_s", active_widget.get("rem_s", 0)))
        total_s = max(1, int(src.get("total_s", active_widget.get("total_s", 60))))
        label = str(src.get("label") or active_widget.get("label") or "Timer")
        active = bool(src.get("active", active_widget.get("active", True)))
        cls._draw_progress_ring_card(
            painter, 74.0, 128.0, 40.0, rem_s, total_s, accent,
            "LEMBRETE", label, "Contagem ativa" if active else "Encerrado"
        )

    @classmethod
    def _draw_body_desk_clock(cls, painter: QPainter, weather_cache: dict, alarms_cache: list):
        now = datetime.now()
        wd = _PT_WEEKDAYS_SHORT[now.weekday()]
        mon = _PT_MONTHS[now.month - 1].upper()
        date_txt = f"{wd} · {now.day:02d} {mon} {now.year}"

        # Barra superior discreta do relógio de mesa (Data à esq, Próximo alarme/clima à dir)
        painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
        painter.setPen(QColor(56, 189, 248))
        painter.drawText(QRectF(24.0, 22.0, 150.0, 20.0), Qt.AlignLeft | Qt.AlignVCenter, date_txt)

        next_alm = next((a for a in (alarms_cache or []) if a.get("enabled", True)), None)
        if next_alm:
            a_time = str(next_alm.get("time") or f"{int(next_alm.get('hour', 7)):02d}:{int(next_alm.get('minute', 0)):02d}")
            cls._draw_icon_bell(painter, 224.0, 32.0, 5.2, QColor(251, 191, 36))
            painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
            painter.setPen(QColor(251, 191, 36))
            painter.drawText(QRectF(233.0, 22.0, 44.0, 20.0), Qt.AlignLeft | Qt.AlignVCenter, a_time)
        elif weather_cache and weather_cache.get("temp") is not None:
            tval = float(weather_cache.get("temp", 25))
            painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
            painter.setPen(QColor(251, 191, 36))
            painter.drawText(QRectF(192.0, 22.0, 84.0, 20.0), Qt.AlignRight | Qt.AlignVCenter, f"{tval:.0f}°C")

        # Relógio gigante central medido dinamicamente para nunca cortar borda direita
        hhmm = now.strftime("%H:%M")
        ss_txt = now.strftime(":%S")
        f_big = QFont("Segoe UI", 34, QFont.Bold)
        f_sec = QFont("Segoe UI", 12, QFont.Bold)
        big_w = QFontMetrics(f_big).horizontalAdvance(hhmm)
        sec_w = QFontMetrics(f_sec).horizontalAdvance(ss_txt)
        start_x = 148.0 - ((big_w + 5.0 + sec_w) / 2.0)

        painter.setFont(f_big)
        painter.setPen(QColor(248, 250, 252))
        painter.drawText(QRectF(start_x, 66.0, big_w + 4.0, 84.0), Qt.AlignLeft | Qt.AlignVCenter, hhmm)

        painter.setFont(f_sec)
        painter.setPen(QColor(56, 189, 248))
        painter.drawText(QRectF(start_x + big_w + 5.0, 84.0, sec_w + 6.0, 48.0), Qt.AlignLeft | Qt.AlignVCenter, ss_txt)

        # Rodapé de permanência e toque para sair
        hint_rect = QRectF(56.0, 178.0, 184.0, 26.0)
        hp = QPainterPath()
        hp.addRoundedRect(hint_rect, 13.0, 13.0)
        painter.fillPath(hp, QBrush(QColor(56, 189, 248, 20)))
        painter.setPen(cls._pen(QColor(56, 189, 248, 70), 1.0))
        painter.drawPath(hp)

        painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
        painter.setPen(QColor(148, 163, 184))
        painter.drawText(hint_rect, Qt.AlignCenter, "TOQUE PARA VOLTAR AO ROSTO")

    @classmethod
    def _draw_body_mood(cls, painter: QPainter, personality_state: dict, active_widget: dict):
        src = personality_state if (personality_state and "affection" in personality_state) else active_widget
        aff = max(0, min(100, int(src.get("affection", 65))))
        eng = max(0, min(100, int(src.get("energy", 85))))
        mood = max(-50, min(50, int(src.get("mood", 15))))
        mood_pct = int(round(((mood + 50.0) / 100.0) * 100.0))
        pets = int(src.get("pet_count", 0))
        chats = int(src.get("chat_count", 0))

        rows = (
            ("HEART", "AFETO", aff, f"{aff}%", QColor(244, 114, 182)),
            ("BOLT", "ENERGIA", eng, f"{eng}%", QColor(56, 189, 248)),
            ("STAR", "HUMOR", mood_pct, f"{mood:+d}", QColor(251, 191, 36)),
        )
        y = 64.0
        for icon_kind, label, pct, val_txt, col in rows:
            if icon_kind == "HEART":
                cls._draw_icon_heart(painter, 30.0, y + 14.0, 6.2, col)
            elif icon_kind == "BOLT":
                cls._draw_icon_bolt(painter, 30.0, y + 14.0, 6.4, col)
            else:
                cls._draw_icon_star(painter, 30.0, y + 14.0, 6.4, col)

            painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
            painter.setPen(QColor(226, 232, 240))
            painter.drawText(QRectF(44.0, y + 2.0, 56.0, 24.0), Qt.AlignLeft | Qt.AlignVCenter, label)

            bar_rect = QRectF(104.0, y + 9.0, 126.0, 10.0)
            track = QPainterPath()
            track.addRoundedRect(bar_rect, 5.0, 5.0)
            painter.fillPath(track, QBrush(QColor(30, 41, 59, 220)))

            fill_w = max(8.0, 126.0 * (float(pct) / 100.0))
            fill_p = QPainterPath()
            fill_p.addRoundedRect(QRectF(104.0, y + 9.0, fill_w, 10.0), 5.0, 5.0)
            painter.fillPath(fill_p, QBrush(col))

            painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
            painter.setPen(col)
            painter.drawText(QRectF(234.0, y + 2.0, 42.0, 24.0), Qt.AlignRight | Qt.AlignVCenter, val_txt)
            y += 36.0

        # Estatísticas de interação
        for idx, (lbl, val) in enumerate((("CARINHOS", pets), ("CONVERSAS", chats))):
            bx = 20.0 + idx * 132.0
            brect = QRectF(bx, 174.0, 124.0, 24.0)
            bp = QPainterPath()
            bp.addRoundedRect(brect, 8.0, 8.0)
            painter.fillPath(bp, QBrush(QColor(255, 255, 255, 10)))
            painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
            painter.setPen(QColor(148, 163, 184))
            painter.drawText(brect, Qt.AlignCenter, f"{lbl}: {val}")

    @classmethod
    def _draw_body_alarm_ring(cls, painter: QPainter, active_widget: dict, accent: QColor, pulse: float, anim_tick: float):
        wobble = math.sin(anim_tick * 6.5) * 14.0
        cls._draw_icon_bell(painter, 148.0, 46.0, 18.0, accent, tilt_deg=wobble)

        time_str = str(active_widget.get("time") or datetime.now().strftime("%H:%M"))
        label_str = str(active_widget.get("label") or "Alarme Disparado!")

        painter.setFont(QFont("Segoe UI", 32, QFont.Bold))
        painter.setPen(QColor(255, 255, 255))
        painter.drawText(QRectF(20.0, 74.0, 256.0, 54.0), Qt.AlignCenter, time_str)

        painter.setFont(QFont("Segoe UI", 9, QFont.Bold))
        painter.setPen(QColor(254, 240, 138))
        painter.drawText(QRectF(24.0, 130.0, 248.0, 28.0), Qt.AlignCenter, cls._elide(painter, label_str, 240.0))

        btn_rect = QRectF(38.0, 174.0, 220.0, 36.0)
        bp = QPainterPath()
        bp.addRoundedRect(btn_rect, 18.0, 18.0)
        btn_alpha = int(190 + 60 * pulse)
        painter.fillPath(bp, QBrush(QColor(accent.red(), accent.green(), accent.blue(), btn_alpha)))
        painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
        painter.setPen(QColor(255, 255, 255))
        painter.drawText(btn_rect, Qt.AlignCenter, "TOQUE PARA SILENCIAR")

    @classmethod
    def _draw_body_pc_control(cls, painter: QPainter, active_widget: dict, accent: QColor):
        cls._draw_icon_monitor(painter, 148.0, 92.0, 20.0, accent)

        headline = str(active_widget.get("title") or "Comando Executado")
        detail = str(active_widget.get("detail") or "Operação aplicada com sucesso no PC")

        painter.setFont(QFont("Segoe UI", 10, QFont.Bold))
        painter.setPen(QColor(248, 250, 252))
        painter.drawText(QRectF(24.0, 126.0, 248.0, 24.0), Qt.AlignCenter, cls._elide(painter, headline, 240.0))

        painter.setFont(QFont("Segoe UI", 8))
        painter.setPen(QColor(148, 163, 184))
        painter.drawText(QRectF(28.0, 152.0, 240.0, 44.0), Qt.AlignHCenter | Qt.AlignTop | Qt.TextWordWrap, detail)
