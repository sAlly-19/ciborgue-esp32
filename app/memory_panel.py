from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QGroupBox, QVBoxLayout, QHBoxLayout, QTabWidget, QWidget,
    QListWidget, QListWidgetItem, QLineEdit, QPushButton, QLabel, QSpinBox,
    QComboBox, QProgressBar
)


class AssistantFeaturesPanel(QGroupBox):
    """
    Painel Modular de Assistente Pessoal (sincronizado com a Flash NVS do ESP32-S3):
    - Aba 1: Lista de Tarefas (To-Do) na NVS + Botão de Widget na Tela 296x240
    - Aba 2: Memórias de Longo Prazo do Usuário na NVS
    - Aba 3: Companheiro de Estudo Pomodoro + Botão de Carinho na Cabeça (GPIO 1 Touch)
    - Aba 4: Despertadores Recorrentes NVS + Humor/Vínculo & Relógio de Mesa
    """
    def __init__(self, send_serial_fn, trigger_widget_fn, parent=None):
        super().__init__("🧠 Assistente Pessoal NVS (Tarefas, Memórias, Pomodoro & Carinho GPIO 1)", parent)
        self.send_serial = send_serial_fn
        self.trigger_widget = trigger_widget_fn
        self._last_pet = {"affection": 65, "energy": 85, "mood": 15, "label": "Animado", "pet_count": 0, "chat_count": 0}
        self._desk_clock_on = False
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(6)

        self.tabs = QTabWidget()
        self.tabs.setStyleSheet("""
            QTabWidget::pane { border: 1px solid #263248; border-radius: 6px; background: #131b28; }
            QTabBar::tab { background: #1e293b; color: #cbd5e1; padding: 5px 10px; border-top-left-radius: 5px; border-top-right-radius: 5px; margin-right: 3px; }
            QTabBar::tab:selected { background: #0284c7; color: white; font-weight: bold; }
        """)

        # ================= ABA 1: TAREFAS (TO-DO NVS) =================
        tab_tasks = QWidget()
        t_lay = QVBoxLayout(tab_tasks)
        t_lay.setContentsMargins(6, 6, 6, 6)

        self.list_tasks = QListWidget()
        self.list_tasks.setMaximumHeight(95)
        t_lay.addWidget(self.list_tasks)

        row_add_t = QHBoxLayout()
        self.inp_task = QLineEdit()
        self.inp_task.setPlaceholderText("Nova tarefa para gravar na Flash NVS...")
        self.inp_task.returnPressed.connect(self._on_add_task)
        row_add_t.addWidget(self.inp_task)

        btn_add_t = QPushButton("➕ Adicionar")
        btn_add_t.clicked.connect(self._on_add_task)
        row_add_t.addWidget(btn_add_t)

        btn_done_t = QPushButton("✅ Concluir Selecionada")
        btn_done_t.clicked.connect(self._on_done_task)
        row_add_t.addWidget(btn_done_t)

        btn_show_t = QPushButton("📺 Ver Card na Tela")
        btn_show_t.clicked.connect(lambda: self.trigger_widget({"type": "TASKS"}))
        row_add_t.addWidget(btn_show_t)

        t_lay.addLayout(row_add_t)
        self.tabs.addTab(tab_tasks, "📋 Tarefas NVS")

        # ================= ABA 2: MEMÓRIAS DE LONGO PRAZO NVS =================
        tab_mem = QWidget()
        m_lay = QVBoxLayout(tab_mem)
        m_lay.setContentsMargins(6, 6, 6, 6)

        self.list_memories = QListWidget()
        self.list_memories.setMaximumHeight(95)
        m_lay.addWidget(self.list_memories)

        row_add_m = QHBoxLayout()
        self.inp_mem = QLineEdit()
        self.inp_mem.setPlaceholderText("Ex: Minha prova de Cálculo é dia 22 / Gosto de café sem açúcar...")
        self.inp_mem.returnPressed.connect(self._on_add_memory)
        row_add_m.addWidget(self.inp_mem)

        btn_add_m = QPushButton("💾 Gravar Memória")
        btn_add_m.clicked.connect(self._on_add_memory)
        row_add_m.addWidget(btn_add_m)

        btn_del_m = QPushButton("🗑️ Remover")
        btn_del_m.clicked.connect(self._on_del_memory)
        row_add_m.addWidget(btn_del_m)

        m_lay.addLayout(row_add_m)
        self.tabs.addTab(tab_mem, "🧠 Memórias de Longo Prazo")

        # ================= ABA 3: POMODORO & SENSOR DE CARINHO =================
        tab_pomo = QWidget()
        p_lay = QVBoxLayout(tab_pomo)
        p_lay.setContentsMargins(6, 6, 6, 6)

        row_pomo = QHBoxLayout()
        row_pomo.addWidget(QLabel("Meta:"))
        self.inp_pomo_goal = QLineEdit("Programar Projeto ESP32")
        row_pomo.addWidget(self.inp_pomo_goal)

        row_pomo.addWidget(QLabel("Foco (min):"))
        self.spn_focus = QSpinBox()
        self.spn_focus.setRange(1, 120)
        self.spn_focus.setValue(25)
        row_pomo.addWidget(self.spn_focus)

        row_pomo.addWidget(QLabel("Pausa (min):"))
        self.spn_break = QSpinBox()
        self.spn_break.setRange(1, 60)
        self.spn_break.setValue(5)
        row_pomo.addWidget(self.spn_break)
        p_lay.addLayout(row_pomo)

        row_pomo_btns = QHBoxLayout()
        btn_start_pomo = QPushButton("🍅 Iniciar Pomodoro no ESP32")
        btn_start_pomo.setStyleSheet("background-color: #0284c7; color: white; font-weight: bold;")
        btn_start_pomo.clicked.connect(self._on_start_pomodoro)
        row_pomo_btns.addWidget(btn_start_pomo)

        btn_stop_pomo = QPushButton("⏹️ Parar Pomodoro")
        btn_stop_pomo.clicked.connect(lambda: self.send_serial("CMD:POMO_STOP"))
        row_pomo_btns.addWidget(btn_stop_pomo)

        btn_pet = QPushButton("🤚 Fazer Carinho na Cabeça (Touch GPIO 1)")
        btn_pet.setStyleSheet("background-color: #db2777; color: white; font-weight: bold;")
        btn_pet.clicked.connect(lambda: self.send_serial("CMD:PET"))
        row_pomo_btns.addWidget(btn_pet)

        p_lay.addLayout(row_pomo_btns)

        self.lbl_pomo_status = QLabel(
            "💡 Dica: Você também pode fazer carinho passando o mouse no topo da tela 296x240 ou encostando no GPIO 1 do ESP32-S3!"
        )
        self.lbl_pomo_status.setStyleSheet("color: #94a3b8; font-size: 11px;")
        p_lay.addWidget(self.lbl_pomo_status)

        self.tabs.addTab(tab_pomo, "🍅 Pomodoro & 🤚 Carinho")

        # ================= ABA 4: ALARMES NVS + HUMOR & RELÓGIO DE MESA =================
        tab_alm = QWidget()
        a_lay = QVBoxLayout(tab_alm)
        a_lay.setContentsMargins(6, 6, 6, 6)
        a_lay.setSpacing(5)

        row_add_a = QHBoxLayout()
        self.spn_alm_hour = QSpinBox()
        self.spn_alm_hour.setRange(0, 23)
        self.spn_alm_hour.setValue(7)
        self.spn_alm_hour.setPrefix("H:")
        self.spn_alm_min = QSpinBox()
        self.spn_alm_min.setRange(0, 59)
        self.spn_alm_min.setValue(0)
        self.spn_alm_min.setPrefix("M:")

        self.cmb_alm_mask = QComboBox()
        self.cmb_alm_mask.addItem("Todos os dias", 0x7F)
        self.cmb_alm_mask.addItem("Seg a Sex", 0x3E)
        self.cmb_alm_mask.addItem("Fim de semana", 0x41)
        self.cmb_alm_mask.addItem("Único", 0)

        self.inp_alm_label = QLineEdit()
        self.inp_alm_label.setPlaceholderText("Rótulo (ex: Acordar)")

        btn_alm_add = QPushButton("➕ Alarme")
        btn_alm_add.setStyleSheet("background-color: #0284c7; color: white;")
        btn_alm_add.clicked.connect(self._on_add_alarm)

        row_add_a.addWidget(self.spn_alm_hour)
        row_add_a.addWidget(self.spn_alm_min)
        row_add_a.addWidget(self.cmb_alm_mask)
        row_add_a.addWidget(self.inp_alm_label, stretch=1)
        row_add_a.addWidget(btn_alm_add)
        a_lay.addLayout(row_add_a)

        self.list_alarms = QListWidget()
        self.list_alarms.setMaximumHeight(68)
        a_lay.addWidget(self.list_alarms)

        row_alm_btns = QHBoxLayout()
        btn_alm_del = QPushButton("🗑️ Remover")
        btn_alm_del.clicked.connect(self._on_del_alarm)
        btn_alm_stop = QPushButton("🔕 Silenciar Alarme")
        btn_alm_stop.setStyleSheet("background-color: #b91c1c; color: white;")
        btn_alm_stop.clicked.connect(lambda: self.send_serial("CMD:ALARM_STOP"))
        btn_mood_card = QPushButton("💛 Ver Card Humor")
        btn_mood_card.clicked.connect(self._on_show_mood_card)
        self.btn_tab_desk_clock = QPushButton("🕰️ Relógio de Mesa")
        self.btn_tab_desk_clock.clicked.connect(self._on_toggle_desk_clock)
        row_alm_btns.addWidget(btn_alm_del)
        row_alm_btns.addWidget(btn_alm_stop)
        row_alm_btns.addWidget(btn_mood_card)
        row_alm_btns.addWidget(self.btn_tab_desk_clock)
        a_lay.addLayout(row_alm_btns)

        row_mood = QHBoxLayout()
        self.bar_affection = QProgressBar()
        self.bar_affection.setRange(0, 100)
        self.bar_affection.setValue(65)
        self.bar_affection.setFormat("Afeto: %p%")
        self.bar_energy = QProgressBar()
        self.bar_energy.setRange(0, 100)
        self.bar_energy.setValue(85)
        self.bar_energy.setFormat("Energia: %p%")
        self.lbl_mood_state = QLabel("Humor: Animado (+15)")
        self.lbl_mood_state.setStyleSheet("color: #f472b6; font-weight: bold; font-size: 11px;")
        row_mood.addWidget(self.bar_affection)
        row_mood.addWidget(self.bar_energy)
        row_mood.addWidget(self.lbl_mood_state)
        a_lay.addLayout(row_mood)

        self.tabs.addTab(tab_alm, "⏰ Alarmes & 💛 Humor")

        layout.addWidget(self.tabs)

    def _on_add_task(self):
        txt = self.inp_task.text().strip()
        if txt:
            self.inp_task.clear()
            self.send_serial(f"CMD:TASK_ADD={txt}")

    def _on_done_task(self):
        item = self.list_tasks.currentItem()
        if item:
            tid = item.data(Qt.UserRole)
            if tid is not None:
                self.send_serial(f"CMD:TASK_DONE={tid}")

    def _on_add_memory(self):
        txt = self.inp_mem.text().strip()
        if txt:
            self.inp_mem.clear()
            self.send_serial(f"CMD:MEM_ADD={txt}")

    def _on_del_memory(self):
        row = self.list_memories.currentRow()
        if row >= 0:
            self.send_serial(f"CMD:MEM_DEL={row + 1}")

    def _on_start_pomodoro(self):
        f_min = self.spn_focus.value()
        b_min = self.spn_break.value()
        goal = self.inp_pomo_goal.text().strip() or "Foco Total"
        self.send_serial(f"CMD:POMO_START={f_min}|{b_min}|{goal}")
        self.trigger_widget({"type": "POMODORO"})

    def update_tasks_list(self, tasks: list):
        self.list_tasks.clear()
        for t in tasks:
            tid = t.get("id", 1)
            title = t.get("title", "")
            done = t.get("done", False)
            prefix = "✅" if done else "⬜"
            it = QListWidgetItem(f"{prefix} [#{tid}] {title}")
            it.setData(Qt.UserRole, tid)
            self.list_tasks.addItem(it)

    def update_memories_list(self, memories: list):
        self.list_memories.clear()
        for idx, m in enumerate(memories, start=1):
            self.list_memories.addItem(f"📌 [{idx}] {m}")

    def update_pomodoro_status(self, pomo: dict):
        if pomo.get("active", False):
            rem = int(pomo.get("rem_s", 0))
            phase = "FOCO" if pomo.get("phase") == "FOCUS" else "PAUSA"
            goal = pomo.get("goal", "")
            self.lbl_pomo_status.setText(f"🍅 Pomodoro Ativo ({phase}): {rem // 60:02d}:{rem % 60:02d} restantes — Meta: {goal}")
            self.lbl_pomo_status.setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 11px;")
        else:
            self.lbl_pomo_status.setText(
                "💡 Dica: Faça carinho clicando no topo do rosto 296x240 ou encostando no pino GPIO 1 do ESP32-S3!"
            )
            self.lbl_pomo_status.setStyleSheet("color: #94a3b8; font-size: 11px;")

    def _on_add_alarm(self):
        hh = self.spn_alm_hour.value()
        mm = self.spn_alm_min.value()
        mask = self.cmb_alm_mask.currentData()
        lbl = self.inp_alm_label.text().strip() or "Despertador"
        self.inp_alm_label.clear()
        self.send_serial(f"CMD:ALARM_ADD={hh}|{mm}|{mask}|{lbl}")

    def _on_del_alarm(self):
        item = self.list_alarms.currentItem()
        if item:
            aid = item.data(Qt.UserRole)
            if aid is not None:
                self.send_serial(f"CMD:ALARM_DEL={aid}")

    def _on_show_mood_card(self):
        self.send_serial("CMD:MOOD_CARD")
        self.trigger_widget({"type": "MOOD", **self._last_pet})

    def _on_toggle_desk_clock(self):
        nxt = not self._desk_clock_on
        self.set_desk_clock_ui(nxt)
        self.send_serial(f"CMD:DESK_CLOCK={'1' if nxt else '0'}")
        self.trigger_widget({"type": "DESK_CLOCK" if nxt else "NONE"})

    def set_desk_clock_ui(self, active: bool):
        self._desk_clock_on = bool(active)
        if self._desk_clock_on:
            self.btn_tab_desk_clock.setStyleSheet("background-color: #0284c7; color: white;")
            self.btn_tab_desk_clock.setText("🕰️ Sair do Relógio")
        else:
            self.btn_tab_desk_clock.setStyleSheet("")
            self.btn_tab_desk_clock.setText("🕰️ Relógio de Mesa")

    def update_alarms_list(self, alarms: list):
        self.list_alarms.clear()
        mask_map = {
            0x7F: "Todos os dias",
            0x3E: "Seg a Sex",
            0x41: "Fim de semana",
            0: "Único",
        }
        for a in (alarms or []):
            aid = a.get("id", 1)
            hhmm = a.get("time") or f"{int(a.get('hour', 7)):02d}:{int(a.get('minute', 0)):02d}"
            mask = int(a.get("daysMask", 0x7F))
            en = bool(a.get("enabled", True))
            lbl = a.get("label", "Alarme")
            prefix = "🔔" if en else "⏸️"
            m_txt = mask_map.get(mask, "Recorrente")
            it = QListWidgetItem(f"{prefix} [#{aid}] {hhmm} · {lbl} ({m_txt})")
            it.setData(Qt.UserRole, aid)
            self.list_alarms.addItem(it)

    def update_personality_status(self, pet: dict):
        if not isinstance(pet, dict):
            return
        self._last_pet.update(pet)
        aff = max(0, min(100, int(self._last_pet.get("affection", 65))))
        eng = max(0, min(100, int(self._last_pet.get("energy", 85))))
        mood = int(self._last_pet.get("mood", 15))
        lbl = str(self._last_pet.get("label", "Animado"))
        self.bar_affection.setValue(aff)
        self.bar_energy.setValue(eng)
        self.lbl_mood_state.setText(f"Humor: {lbl} ({mood:+d})")
