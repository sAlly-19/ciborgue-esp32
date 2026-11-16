import math
from PySide6.QtCore import Qt, QRectF, QPointF
from PySide6.QtGui import QPainter, QColor, QPen, QBrush, QFont, QPainterPath, QLinearGradient


class WidgetsOverlayRenderer:
    """
    Renderizador modular dos Mini-Widgets Holográficos e Barra de Pomodoro
    na tela Estardyn 2.01" (296x240 Paisagem).
    Quando um Widget está ativo, o rosto do Ciborgue ocupa a metade esquerda (x=10..136)
    e o Card Holográfico é desenhado na metade direita (x=140..286, y=28..188).
    """

    @staticmethod
    def draw_pomodoro_top_bar(painter: QPainter, pomo_state: dict, anim_tick: float):
        if not pomo_state.get("active", False):
            return

        rem_s = max(0, int(pomo_state.get("rem_s", 0)))
        total_s = max(1, int(pomo_state.get("total_s", 1500)))
        phase = pomo_state.get("phase", "FOCUS")
        goal = pomo_state.get("goal", "Foco")

        progress = max(0.0, min(1.0, 1.0 - (rem_s / total_s)))
        mins = rem_s // 60
        secs = rem_s % 60

        bar_color = QColor(56, 189, 248) if phase == "FOCUS" else QColor(74, 222, 128)
        bg_rect = QRectF(14, 21, 268, 7)

        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(QColor(22, 32, 48)))
        painter.drawRoundedRect(bg_rect, 3.5, 3.5)

        fill_w = max(6.0, 268.0 * progress)
        painter.setBrush(QBrush(bar_color))
        painter.drawRoundedRect(QRectF(14, 21, fill_w, 7), 3.5, 3.5)

        painter.setFont(QFont("Segoe UI", 6, QFont.Bold))
        painter.setPen(bar_color.lighter(125))
        badge = "🍅 FOCO" if phase == "FOCUS" else "☕ PAUSA"
        painter.drawText(QRectF(14, 29, 268, 12), Qt.AlignCenter, f"{badge}: {mins:02d}:{secs:02d} — {goal[:22]}")

    @staticmethod
    def draw_holographic_card(
        painter: QPainter,
        widget_data: dict,
        tasks_list: list,
        pomo_state: dict,
        alpha_ratio: float,
        anim_tick: float
    ):
        if alpha_ratio <= 0.02 or not widget_data:
            return

        wtype = widget_data.get("type", "")
        alpha = int(245 * min(1.0, alpha_ratio))

        card_rect = QRectF(138, 34, 148, 152)
        path = QPainterPath()
        path.addRoundedRect(card_rect, 14, 14)

        grad = QLinearGradient(card_rect.topLeft(), card_rect.bottomRight())
        grad.setColorAt(0.0, QColor(18, 30, 48, alpha))
        grad.setColorAt(1.0, QColor(10, 17, 29, alpha))
        painter.fillPath(path, QBrush(grad))

        border_col = QColor(56, 189, 248, int(190 * alpha_ratio))
        painter.setPen(QPen(border_col, 1.5))
        painter.drawPath(path)

        if wtype == "WEATHER":
            WidgetsOverlayRenderer._draw_weather_card(painter, card_rect, widget_data, alpha, anim_tick)
        elif wtype == "CLOCK":
            WidgetsOverlayRenderer._draw_clock_card(painter, card_rect, widget_data, alpha, anim_tick)
        elif wtype == "TASKS":
            WidgetsOverlayRenderer._draw_tasks_card(painter, card_rect, tasks_list, alpha)
        elif wtype == "POMODORO":
            WidgetsOverlayRenderer._draw_pomodoro_card(painter, card_rect, pomo_state, alpha, anim_tick)
        elif wtype == "PC_CONTROL":
            WidgetsOverlayRenderer._draw_pc_card(painter, card_rect, widget_data, alpha)

    @staticmethod
    def _draw_weather_card(painter: QPainter, rect: QRectF, data: dict, alpha: int, anim_tick: float):
        city = str(data.get("city", "Cidade"))[:15]
        temp = data.get("temp", "--")
        feels = data.get("feels", "--")
        hum = data.get("hum", "--")
        desc = str(data.get("desc", "")).capitalize()

        painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
        painter.setPen(QColor(56, 189, 248, alpha))
        painter.drawText(QRectF(rect.left() + 10, rect.top() + 8, rect.width() - 20, 16), Qt.AlignLeft, f"🌤️ {city}")

        # Sol/Nuvem estilizado animado
        sun_cx = rect.right() - 28
        sun_cy = rect.top() + 42
        pulse_r = 10.0 + 1.5 * math.sin(anim_tick * 2.0)
        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(QColor(250, 204, 21, alpha)))
        painter.drawEllipse(QPointF(sun_cx, sun_cy), pulse_r, pulse_r)

        painter.setFont(QFont("Segoe UI", 20, QFont.Bold))
        painter.setPen(QColor(248, 250, 252, alpha))
        painter.drawText(QRectF(rect.left() + 10, rect.top() + 26, 95, 34), Qt.AlignLeft | Qt.AlignVCenter, f"{temp}°C")

        painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
        painter.setPen(QColor(148, 163, 184, alpha))
        painter.drawText(QRectF(rect.left() + 10, rect.top() + 68, rect.width() - 16, 16), Qt.AlignLeft, desc[:22])

        painter.setFont(QFont("Segoe UI", 7))
        painter.setPen(QColor(203, 213, 225, alpha))
        painter.drawText(
            QRectF(rect.left() + 10, rect.top() + 90, rect.width() - 16, 48),
            Qt.AlignLeft,
            f"🌡️ Sensação: {feels}°C\n💧 Umidade: {hum}%"
        )

    @staticmethod
    def _draw_clock_card(painter: QPainter, rect: QRectF, data: dict, alpha: int, anim_tick: float):
        time_str = str(data.get("time", "00:00"))
        date_str = str(data.get("date", "--/--/----"))
        weekday = str(data.get("weekday", "")).upper()

        painter.setFont(QFont("Segoe UI", 7, QFont.Bold))
        painter.setPen(QColor(56, 189, 248, alpha))
        painter.drawText(QRectF(rect.left() + 8, rect.top() + 12, rect.width() - 16, 14), Qt.AlignCenter, "⏰ HORÁRIO OFICIAL NTP")

        painter.setFont(QFont("Segoe UI", 24, QFont.Bold))
        painter.setPen(QColor(74, 222, 128, alpha))
        painter.drawText(QRectF(rect.left() + 6, rect.top() + 36, rect.width() - 12, 44), Qt.AlignCenter, time_str)

        painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
        painter.setPen(QColor(241, 245, 249, alpha))
        painter.drawText(QRectF(rect.left() + 8, rect.top() + 88, rect.width() - 16, 18), Qt.AlignCenter, weekday)

        painter.setFont(QFont("Segoe UI", 8))
        painter.setPen(QColor(148, 163, 184, alpha))
        painter.drawText(QRectF(rect.left() + 8, rect.top() + 110, rect.width() - 16, 18), Qt.AlignCenter, date_str)

    @staticmethod
    def _draw_tasks_card(painter: QPainter, rect: QRectF, tasks_list: list, alpha: int):
        painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
        painter.setPen(QColor(56, 189, 248, alpha))
        painter.drawText(QRectF(rect.left() + 10, rect.top() + 8, rect.width() - 20, 16), Qt.AlignLeft, "📋 TAREFAS (NVS)")

        if not tasks_list:
            painter.setFont(QFont("Segoe UI", 8))
            painter.setPen(QColor(148, 163, 184, alpha))
            painter.drawText(
                QRectF(rect.left() + 10, rect.top() + 45, rect.width() - 20, 60),
                Qt.AlignCenter | Qt.TextWordWrap,
                "Nenhuma tarefa pendente!\nDiga 'Ciborgue, adicione a tarefa...'"
            )
            return

        y_off = rect.top() + 28
        painter.setFont(QFont("Segoe UI", 7))
        for item in tasks_list[:5]:
            done = item.get("done", False)
            title = str(item.get("title", ""))[:20]
            icon = "✅" if done else "⬜"
            painter.setPen(QColor(100, 116, 139, alpha) if done else QColor(241, 245, 249, alpha))
            painter.drawText(QRectF(rect.left() + 10, y_off, rect.width() - 16, 18), Qt.AlignLeft, f"{icon} {title}")
            y_off += 22

    @staticmethod
    def _draw_pomodoro_card(painter: QPainter, rect: QRectF, pomo: dict, alpha: int, anim_tick: float):
        rem_s = max(0, int(pomo.get("rem_s", 0)))
        mins = rem_s // 60
        secs = rem_s % 60
        phase = pomo.get("phase", "FOCUS")
        goal = str(pomo.get("goal", "Foco Total"))

        painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
        painter.setPen(QColor(251, 191, 36, alpha))
        title = "🍅 POMODORO FOCO" if phase == "FOCUS" else "☕ PAUSA DESCANSO"
        painter.drawText(QRectF(rect.left() + 6, rect.top() + 10, rect.width() - 12, 16), Qt.AlignCenter, title)

        painter.setFont(QFont("Segoe UI", 22, QFont.Bold))
        painter.setPen(QColor(56, 189, 248, alpha) if phase == "FOCUS" else QColor(74, 222, 128, alpha))
        painter.drawText(QRectF(rect.left() + 6, rect.top() + 40, rect.width() - 12, 42), Qt.AlignCenter, f"{mins:02d}:{secs:02d}")

        painter.setFont(QFont("Segoe UI", 7))
        painter.setPen(QColor(226, 232, 240, alpha))
        painter.drawText(
            QRectF(rect.left() + 10, rect.top() + 92, rect.width() - 20, 44),
            Qt.AlignCenter | Qt.TextWordWrap,
            f"Meta: {goal}"
        )

    @staticmethod
    def _draw_pc_card(painter: QPainter, rect: QRectF, data: dict, alpha: int):
        act = str(data.get("action", "Comando PC"))
        param = str(data.get("param", ""))

        painter.setFont(QFont("Segoe UI", 8, QFont.Bold))
        painter.setPen(QColor(74, 222, 128, alpha))
        painter.drawText(QRectF(rect.left() + 8, rect.top() + 12, rect.width() - 16, 16), Qt.AlignCenter, "💻 CONTROLE DO PC")

        painter.setFont(QFont("Segoe UI", 9, QFont.Bold))
        painter.setPen(QColor(248, 250, 252, alpha))
        painter.drawText(
            QRectF(rect.left() + 10, rect.top() + 42, rect.width() - 20, 38),
            Qt.AlignCenter | Qt.TextWordWrap,
            act
        )

        painter.setFont(QFont("Segoe UI", 7))
        painter.setPen(QColor(148, 163, 184, alpha))
        painter.drawText(
            QRectF(rect.left() + 10, rect.top() + 86, rect.width() - 20, 52),
            Qt.AlignCenter | Qt.TextWordWrap,
            param
        )
