import os
import time
from dotenv import set_key
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QTextEdit, QGroupBox, QGridLayout,
    QComboBox, QProgressBar
)

from app.config import ENV_PATH, GEMINI_VOICES
from app.signals import BridgeSignals
from app.face_canvas import FaceCanvas296x240
from app.serial_bridge import SerialBridgeManager
from app.audio_tts import AudioTtsManager
from app.mic_listener import MicrophoneListener, list_input_microphones
from app.pc_controller import PcController
from app.memory_panel import AssistantFeaturesPanel
from app.sfx import play_sfx_async


class CiborgueWorkbenchWindow(QMainWindow):
    """Janela principal da Bancada Virtual & Simulador Estardyn 2.01\" (296x240)."""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("🤖 Ciborgue ESP32-S3 — Assistente Completo & Simulador 296x240")
        self.resize(1180, 810)
        self._apply_theme()

        self.signals = BridgeSignals()
        self.serial_bridge = SerialBridgeManager(self.signals)
        self.pc_controller = PcController(self.signals)

        self._build_ui()

        self.tts_manager = AudioTtsManager(
            signals=self.signals,
            send_serial_fn=self.serial_bridge.send_cmd,
            get_api_key_fn=self._get_current_api_key,
            get_emotion_fn=lambda: self.face_canvas.emotion
        )
        self.mic_listener = MicrophoneListener(
            signals=self.signals,
            send_serial_fn=self.serial_bridge.send_cmd,
            is_tts_busy_fn=lambda: self.tts_manager.is_busy() or self.face_canvas.state == "SPEAK",
            get_api_key_fn=self._get_current_api_key
        )

        self._connect_signals()

        # Carrega estado espelhado local enquanto o ESP32-S3 conecta
        self.assistant_panel.update_memories_list(self.serial_bridge.mirror_state.get("memories", []))
        self.assistant_panel.update_tasks_list(self.serial_bridge.mirror_state.get("tasks", []))
        self.face_canvas.tasks_cache = self.serial_bridge.mirror_state.get("tasks", [])
        self.on_alarms_updated(self.serial_bridge.mirror_state.get("alarms", []))
        if isinstance(self.serial_bridge.mirror_state.get("personality"), dict):
            self.on_personality_updated(self.serial_bridge.mirror_state["personality"])

        self.serial_bridge.start()
        self.tts_manager.start()
        self.mic_listener.start()

    def _apply_theme(self):
        self.setStyleSheet("""
            QMainWindow, QWidget {
                background-color: #0f141d;
                color: #e2e8f0;
                font-family: 'Segoe UI', sans-serif;
            }
            QGroupBox {
                border: 1px solid #263248;
                border-radius: 8px;
                margin-top: 10px;
                padding-top: 12px;
                font-weight: bold;
                color: #38bdf8;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 6px;
            }
            QLineEdit, QTextEdit, QComboBox, QListWidget, QSpinBox {
                background-color: #171f2e;
                border: 1px solid #2e3d56;
                border-radius: 6px;
                padding: 4px;
                color: #f8fafc;
            }
            QPushButton {
                background-color: #1e293b;
                border: 1px solid #334155;
                border-radius: 6px;
                padding: 5px 9px;
                color: #f1f5f9;
                font-weight: 600;
            }
            QPushButton:hover {
                background-color: #2563eb;
                border-color: #3b82f6;
            }
            QProgressBar {
                background-color: #171f2e;
                border: 1px solid #2e3d56;
                border-radius: 5px;
                text-align: center;
                color: #f8fafc;
                font-size: 10px;
            }
            QProgressBar::chunk {
                background-color: #22c55e;
                border-radius: 4px;
            }
        """)

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)
        main_layout.setContentsMargins(12, 10, 12, 10)
        main_layout.setSpacing(14)

        # ================= COLUNA ESQUERDA: TELA 296x240 + EXPRESSÕES + WIDGETS =================
        left_col = QVBoxLayout()
        left_col.setSpacing(6)

        header_lbl = QLabel("🖥️ Tela Estardyn 2.01\" TFT SPI (296x240 Paisagem) — Clique no Topo p/ Fazer Carinho 🤚")
        header_lbl.setStyleSheet("color: #94a3b8; font-size: 12px; font-weight: bold;")
        left_col.addWidget(header_lbl, alignment=Qt.AlignCenter)

        self.face_canvas = FaceCanvas296x240(
            on_pet_callback=lambda: self.serial_bridge.send_cmd("CMD:PET"),
            on_tap_callback=self.on_canvas_tap,
            parent=self
        )
        left_col.addWidget(self.face_canvas, alignment=Qt.AlignCenter)

        emo_group = QGroupBox("🎭 Expressões, Cores do LED RGB & Mini-Widgets 296x240")
        emo_grid = QGridLayout(emo_group)
        emo_grid.setSpacing(5)

        emotions = [
            ("😊 Feliz", "HAPPY"), ("😐 Neutro", "NEUTRAL"),
            ("😮 Surpreso", "SURPRISED"), ("😉 Piscadinha", "WINK"),
            ("😍 Apaixonado", "LOVE"), ("😢 Triste", "SAD"),
            ("😠 Bravo", "ANGRY"), ("😴 Sonolento", "SLEEPY"),
            ("😨 Medo", "FEAR"), ("🤢 Nojo", "DISGUST"),
            ("😒 Desprezo", "CONTEMPT"), ("😳 Vergonha", "SHAME"),
            ("😱 Desespero", "DESPAIR"), ("😈 Malícia", "MALICE")
        ]
        for idx, (label, code) in enumerate(emotions):
            btn = QPushButton(label)
            btn.clicked.connect(lambda _, c=code: self.serial_bridge.send_cmd(f"CMD:EMOTION={c}"))
            emo_grid.addWidget(btn, idx // 4, idx % 4)

        colors = [
            ("🔴 Vermelho", "vermelho,solido"),
            ("🟢 Verde", "verde,solido"),
            ("🔵 Azul", "azul,pulsar"),
            ("🟣 Roxo", "roxo,pulsar"),
            ("🌈 Arco-Íris", "ciano,arco_iris"),
            ("🟠 Alerta", "laranja,alerta"),
            ("⚪ Branco", "branco,solido"),
            ("⚫ Apagar LED", "desligar,solido")
        ]
        for idx, (label, cmd_val) in enumerate(colors):
            btn = QPushButton(label)
            btn.clicked.connect(lambda _, v=cmd_val: self.serial_bridge.send_cmd(f"CMD:COLOR={v}"))
            emo_grid.addWidget(btn, 4 + (idx // 4), idx % 4)

        # Linha rápida de Widgets / Relógio de Mesa / Humor / Carinho
        self.btn_desk_clock = QPushButton("🕰️ Relógio Mesa")
        self.btn_desk_clock.clicked.connect(self.toggle_desk_clock)
        emo_grid.addWidget(self.btn_desk_clock, 6, 0)

        btn_w_tasks = QPushButton("📋 Tarefas")
        btn_w_tasks.clicked.connect(lambda: self.face_canvas.show_holographic_widget({"type": "TASKS"}))
        emo_grid.addWidget(btn_w_tasks, 6, 1)

        btn_w_mood = QPushButton("💛 Humor")
        btn_w_mood.clicked.connect(self.on_show_mood_card)
        emo_grid.addWidget(btn_w_mood, 6, 2)

        btn_pet_quick = QPushButton("🤚 Carinho")
        btn_pet_quick.setStyleSheet("background-color: #db2777; color: white;")
        btn_pet_quick.clicked.connect(lambda: self.serial_bridge.send_cmd("CMD:PET"))
        emo_grid.addWidget(btn_pet_quick, 6, 3)

        left_col.addWidget(emo_group)
        left_col.addStretch(1)
        main_layout.addLayout(left_col)

        # ================= COLUNA DIREITA: TELEMETRIA, VOZ, ASSISTENTE NVS E CHAT =================
        right_col = QVBoxLayout()
        right_col.setSpacing(6)

        # 1. Telemetria do ESP32-S3
        tel_group = QGroupBox("📊 Telemetria em Tempo Real do ESP32-S3 (COM4 & API HTTP Porta 80)")
        tel_layout = QGridLayout(tel_group)
        self.lbl_serial = QLabel("🔌 Serial: Procurando ESP32-S3...")
        self.lbl_wifi = QLabel("📶 Wi-Fi ESP32: Aguardando...")
        self.lbl_temp = QLabel("🌡️ Temp Chip: -- °C")
        self.lbl_mem = QLabel("🧠 PSRAM / Heap: -- KB")
        tel_layout.addWidget(self.lbl_serial, 0, 0)
        tel_layout.addWidget(self.lbl_wifi, 0, 1)
        tel_layout.addWidget(self.lbl_temp, 1, 0)
        tel_layout.addWidget(self.lbl_mem, 1, 1)
        right_col.addWidget(tel_group)

        # 2. Voz Expressiva + Microfone Ao Vivo
        audio_group = QGroupBox("🎙️ Voz Neural Ultra-Rápida (~0.8s) / Gemini 3.8 TTS & Microfone")
        audio_layout = QGridLayout(audio_group)

        audio_layout.addWidget(QLabel("Voz do Robô:"), 0, 0)
        self.cmb_voice = QComboBox()
        for v_code, v_label in GEMINI_VOICES:
            self.cmb_voice.addItem(v_label, userData=v_code)
        self.cmb_voice.currentIndexChanged.connect(self.on_voice_changed)
        audio_layout.addWidget(self.cmb_voice, 0, 1)

        btn_test_voice = QPushButton("🔊 Testar Voz + LipSync")
        btn_test_voice.clicked.connect(
            lambda: self.signals.say_requested.emit(
                "Oi! Estou com resposta ultra rápida, memória na Flash NVS, Pomodoro e controle total do seu PC!"
            )
        )
        audio_layout.addWidget(btn_test_voice, 0, 2)

        audio_layout.addWidget(QLabel("Microfone:"), 1, 0)
        self.cmb_mic = QComboBox()
        self.mic_list = list_input_microphones()
        for dev_id, dev_name in self.mic_list:
            self.cmb_mic.addItem(dev_name, userData=dev_id)
        self.cmb_mic.currentIndexChanged.connect(self.on_mic_changed)
        audio_layout.addWidget(self.cmb_mic, 1, 1)

        self.btn_ptt = QPushButton("🎤 Ouvir Agora (Sem Wake Word)")
        self.btn_ptt.setStyleSheet("background-color: #059669; color: white; font-weight: bold; padding: 5px;")
        self.btn_ptt.clicked.connect(self.on_trigger_listen_now)
        audio_layout.addWidget(self.btn_ptt, 1, 2)

        audio_layout.addWidget(QLabel("Volume Voz:"), 2, 0)
        self.bar_mic = QProgressBar()
        self.bar_mic.setRange(0, 100)
        self.bar_mic.setValue(0)
        self.bar_mic.setFormat("Nível do Microfone: %p%")
        audio_layout.addWidget(self.bar_mic, 2, 1, 1, 2)

        right_col.addWidget(audio_group)

        # 3. Painel Modular de Assistente Pessoal (Tarefas NVS, Memórias, Pomodoro e Carinho)
        self.assistant_panel = AssistantFeaturesPanel(
            send_serial_fn=self.serial_bridge.send_cmd,
            trigger_widget_fn=self.face_canvas.show_holographic_widget,
            parent=self
        )
        right_col.addWidget(self.assistant_panel)

        # 4. Provisionamento NVS Compacto
        nvs_group = QGroupBox("🔐 Provisionamento NVS (Wi-Fi, Gemini API Key e Cidade)")
        nvs_layout = QGridLayout(nvs_group)

        nvs_layout.addWidget(QLabel("SSID:"), 0, 0)
        self.inp_ssid = QLineEdit(os.getenv("WIFI_SSID", ""))
        nvs_layout.addWidget(self.inp_ssid, 0, 1)

        nvs_layout.addWidget(QLabel("Senha:"), 0, 2)
        self.inp_pass = QLineEdit(os.getenv("WIFI_PASSWORD", ""))
        self.inp_pass.setEchoMode(QLineEdit.Password)
        nvs_layout.addWidget(self.inp_pass, 0, 3)

        nvs_layout.addWidget(QLabel("Key:"), 1, 0)
        self.inp_key = QLineEdit(os.getenv("GEMINI_API_KEY", ""))
        self.inp_key.setEchoMode(QLineEdit.Password)
        nvs_layout.addWidget(self.inp_key, 1, 1)

        nvs_layout.addWidget(QLabel("Cidade:"), 1, 2)
        self.inp_city = QLineEdit(os.getenv("DEFAULT_CITY", "Sao Paulo"))
        nvs_layout.addWidget(self.inp_city, 1, 3)

        btn_save_nvs = QPushButton("💾 Gravar no .env e na Flash NVS")
        btn_save_nvs.setStyleSheet("background-color: #0284c7; color: white; font-weight: bold;")
        btn_save_nvs.clicked.connect(self.save_and_provision_nvs)
        nvs_layout.addWidget(btn_save_nvs, 1, 4)

        right_col.addWidget(nvs_group)

        # 5. Console de Conversa e Logs
        chat_group = QGroupBox("💬 Conversa com o Cérebro Gemini no ESP32-S3 & Logs")
        chat_layout = QVBoxLayout(chat_group)

        self.txt_log = QTextEdit()
        self.txt_log.setReadOnly(True)
        chat_layout.addWidget(self.txt_log)

        inp_row = QHBoxLayout()
        self.inp_chat = QLineEdit()
        self.inp_chat.setPlaceholderText("Ex: 'Anote a tarefa estudar C++', 'Aumente o volume do PC', 'Inicie um Pomodoro'...")
        self.inp_chat.returnPressed.connect(self.on_send_chat)
        inp_row.addWidget(self.inp_chat)

        btn_send = QPushButton("Enviar ao ESP32 🚀")
        btn_send.setStyleSheet("background-color: #16a34a; color: white; font-weight: bold;")
        btn_send.clicked.connect(self.on_send_chat)
        inp_row.addWidget(btn_send)

        chat_layout.addLayout(inp_row)
        right_col.addWidget(chat_group, stretch=1)

        main_layout.addLayout(right_col, stretch=1)

    def _connect_signals(self):
        self.signals.state_changed.connect(self.on_state_changed)
        self.signals.emotion_changed.connect(self.on_emotion_changed)
        self.signals.led_changed.connect(self.on_led_changed)
        self.signals.say_requested.connect(self.on_say_requested)
        self.signals.speech_playback_started.connect(self.on_speech_playback_started)
        self.signals.telemetry_received.connect(self.on_telemetry_received)
        self.signals.log_message.connect(self.append_log)
        self.signals.serial_status.connect(self.on_serial_status)
        self.signals.mic_level_changed.connect(self.bar_mic.setValue)
        self.signals.conv_window_changed.connect(self.on_conv_window_changed)
        self.signals.audio_lipsync_frame.connect(self.face_canvas.set_audio_lipsync)

        # Sinais dos novos módulos
        self.signals.memories_updated.connect(self.assistant_panel.update_memories_list)
        self.signals.tasks_updated.connect(self.on_tasks_updated)
        self.signals.pomodoro_updated.connect(self.on_pomodoro_updated)
        self.signals.widget_triggered.connect(self.face_canvas.show_holographic_widget)
        self.signals.pc_action_requested.connect(self.pc_controller.execute_from_esp32)
        self.signals.pet_triggered.connect(self.on_pet_triggered)
        self.signals.alarms_updated.connect(self.on_alarms_updated)
        self.signals.personality_updated.connect(self.on_personality_updated)
        self.signals.desk_clock_changed.connect(self.on_desk_clock_changed)
        self.signals.timer_updated.connect(self.on_timer_updated)

    def _get_current_api_key(self) -> str:
        return self.inp_key.text().strip() or os.getenv("GEMINI_API_KEY", "").strip()

    def on_canvas_tap(self, action: str):
        if action == "ALARM_STOP":
            self.serial_bridge.send_cmd("CMD:ALARM_STOP")
            self.append_log("🔕 <b>[Alarme]</b> Alarme silenciado pelo toque na tela.")
        elif action == "DESK_CLOCK_OFF":
            self.serial_bridge.send_cmd("CMD:DESK_CLOCK=0")
            self.on_desk_clock_changed(False)
        elif action == "WIDGET_CLOSE":
            self.serial_bridge.send_cmd("CMD:WIDGET_CLOSE")

    def toggle_desk_clock(self):
        nxt = not self.face_canvas.desk_clock_active
        self.serial_bridge.send_cmd(f"CMD:DESK_CLOCK={'1' if nxt else '0'}")
        self.on_desk_clock_changed(nxt)

    def on_show_mood_card(self):
        self.serial_bridge.send_cmd("CMD:MOOD_CARD")
        self.face_canvas.show_holographic_widget({"type": "MOOD", **self.face_canvas.personality_state})

    def on_alarms_updated(self, alarms: list):
        self.face_canvas.update_alarms_cache(alarms)
        self.assistant_panel.update_alarms_list(alarms)

    def on_personality_updated(self, pet: dict):
        self.face_canvas.update_personality_state(pet)
        self.assistant_panel.update_personality_status(pet)

    def on_desk_clock_changed(self, active: bool):
        self.face_canvas.set_desk_clock(active)
        self.assistant_panel.set_desk_clock_ui(active)
        if active:
            self.btn_desk_clock.setStyleSheet("background-color: #0284c7; color: white;")
            self.btn_desk_clock.setText("🕰️ Sair Relógio")
        else:
            self.btn_desk_clock.setStyleSheet("")
            self.btn_desk_clock.setText("🕰️ Relógio Mesa")

    def on_timer_updated(self, tdata: dict):
        self.face_canvas.update_timer_state(tdata)

    def on_tasks_updated(self, tasks: list):
        self.face_canvas.tasks_cache = tasks
        self.assistant_panel.update_tasks_list(tasks)

    def on_pomodoro_updated(self, pomo: dict):
        self.face_canvas.update_pomodoro_overlay(pomo)
        self.assistant_panel.update_pomodoro_status(pomo)

    def on_pet_triggered(self):
        self.face_canvas.trigger_emotion_impulse("LOVE")
        self.append_log("💖 <b>[Carinho Detectado]</b> O Ciborgue recebeu carinho na cabeça (GPIO 1 / Tela)!")

    def on_voice_changed(self, idx: int):
        voice_code = self.cmb_voice.itemData(idx) or "Edge-Antonio"
        self.tts_manager.selected_voice = voice_code
        self.append_log(f"🎙️ Voz do Ciborgue alterada para: <b>{self.cmb_voice.currentText()}</b>")

    def on_mic_changed(self, idx: int):
        dev_id = self.cmb_mic.itemData(idx)
        self.mic_listener.change_device(dev_id)
        self.append_log(f"🎤 Alterando microfone para: <b>{self.cmb_mic.currentText()}</b>")

    def on_trigger_listen_now(self):
        self.mic_listener.trigger_push_to_talk()
        self.append_log("🎤 <b>[Push-to-Talk]</b> Microfone ativado — pode falar seu comando!")

    def on_conv_window_changed(self, until_ts: float):
        self.face_canvas.continuous_until = until_ts
        self.mic_listener.set_continuous_window(until_ts)

    def append_log(self, msg: str):
        ts = time.strftime("%H:%M:%S")
        self.txt_log.append(f"<span style='color:#64748b'>[{ts}]</span> {msg}")

    def on_serial_status(self, connected: bool, port_info: str):
        if connected:
            self.lbl_serial.setText(f"🟢 Serial: Conectado ({port_info})")
            self.lbl_serial.setStyleSheet("color: #4ade80; font-weight: bold;")
        else:
            self.lbl_serial.setText(f"🔴 Serial: {port_info}")
            self.lbl_serial.setStyleSheet("color: #f87171; font-weight: bold;")

    def on_state_changed(self, state: str):
        if state == "SPEAK" and not self.tts_manager.is_playing:
            return
        self.face_canvas.state = state
        if state == "THINK":
            play_sfx_async("think")

    def on_emotion_changed(self, emotion: str):
        self.face_canvas.trigger_emotion_impulse(emotion)

    def on_led_changed(self, r: int, g: int, b: int, mode: str):
        self.face_canvas.led_rgb = (r, g, b)
        self.face_canvas.led_mode = mode

    def on_say_requested(self, text: str):
        if self.face_canvas.state != "SPEAK":
            self.face_canvas.state = "THINK"
        self.tts_manager.enqueue(text)

    def on_speech_playback_started(self, text: str):
        self.face_canvas.state = "SPEAK"
        self.append_log(f"<b style='color:#38bdf8'>🤖 CIBORGUE ({self.tts_manager.selected_voice}):</b> {text}")

    def on_telemetry_received(self, data: dict):
        self.face_canvas.telemetry.update(data)
        new_state = data.get("state", self.face_canvas.state)
        if not self.tts_manager.is_busy() and new_state != "SPEAK":
            self.face_canvas.state = new_state

        new_emo = data.get("emotion", self.face_canvas.emotion)
        if new_emo != self.face_canvas.emotion:
            self.face_canvas.trigger_emotion_impulse(new_emo)

        self.face_canvas.led_rgb = (
            data.get("led_r", 0),
            data.get("led_g", 80),
            data.get("led_b", 255)
        )

        if "timer_rem_s" in data:
            rem_t = int(data.get("timer_rem_s", 0))
            self.face_canvas.update_timer_state({
                "active": rem_t > 0,
                "rem_s": rem_t,
                "total_s": int(data.get("timer_total_s", max(60, rem_t))),
                "label": data.get("timer_label", "Timer")
            })
        if "desk_clock" in data:
            self.on_desk_clock_changed(bool(data.get("desk_clock", False)))
        if "alarm_ringing" in data:
            is_ring = bool(data.get("alarm_ringing", False))
            if not is_ring and self.face_canvas.alarm_ringing:
                self.face_canvas.show_holographic_widget({"type": "NONE"})
            self.face_canvas.alarm_ringing = is_ring
        if "affection" in data:
            self.on_personality_updated({
                "affection": data.get("affection", 65),
                "energy": data.get("energy", 85),
                "mood": data.get("mood", 15),
                "label": data.get("mood_label", "Animado")
            })

        wifi_ok = data.get("wifi_ok", False)
        ssid = data.get("wifi_ssid", "")
        ip = data.get("ip", "0.0.0.0")
        rssi = data.get("wifi_rssi", 0)
        has_key = data.get("has_key", False)

        if wifi_ok:
            self.lbl_wifi.setText(f"🟢 Wi-Fi: {ssid} ({ip} | {rssi} dBm)")
            self.lbl_wifi.setStyleSheet("color: #4ade80;")
        else:
            self.lbl_wifi.setText(f"🟠 Wi-Fi ESP32: Desconectado (Chave: {'OK' if has_key else 'Pendente'})")
            self.lbl_wifi.setStyleSheet("color: #fbbf24;")

        temp_c = data.get("temp_c", 0.0)
        uptime_s = data.get("uptime_s", 0)
        self.lbl_temp.setText(f"🌡️ Chip ESP32-S3: {temp_c:.1f} °C (Uptime: {uptime_s}s)")

        heap_kb = data.get("heap_kb", 0)
        psram_kb = data.get("psram_kb", 0)
        self.lbl_mem.setText(f"🧠 RAM: {heap_kb} KB | PSRAM: {psram_kb} KB")

    def on_send_chat(self):
        text = self.inp_chat.text().strip()
        if not text:
            return
        self.inp_chat.clear()
        self.append_log(f"<b style='color:#a7f3d0'>🧑 VOCÊ:</b> {text}")
        self.serial_bridge.send_cmd(f"MIC:{text}")

    def save_and_provision_nvs(self):
        ssid = self.inp_ssid.text().strip()
        pw = self.inp_pass.text().strip()
        key = self.inp_key.text().strip()
        city = self.inp_city.text().strip() or "Sao Paulo"

        if not ENV_PATH.exists():
            ENV_PATH.write_text("", encoding="utf-8")
        set_key(str(ENV_PATH), "WIFI_SSID", ssid)
        set_key(str(ENV_PATH), "WIFI_PASSWORD", pw)
        set_key(str(ENV_PATH), "GEMINI_API_KEY", key)
        set_key(str(ENV_PATH), "DEFAULT_CITY", city)

        self.serial_bridge.provision_nvs(ssid, pw, key, city)
        self.append_log("<span style='color:#38bdf8'>✅ Credenciais salvas no .env e enviadas para a NVS do ESP32-S3!</span>")

    def closeEvent(self, event):
        self.mic_listener.stop()
        self.tts_manager.stop()
        self.serial_bridge.stop()
        super().closeEvent(event)
