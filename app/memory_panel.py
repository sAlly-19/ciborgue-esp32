from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QGroupBox, QVBoxLayout, QHBoxLayout, QTabWidget, QWidget,
    QListWidget, QListWidgetItem, QLineEdit, QPushButton, QLabel, QSpinBox
)


class AssistantFeaturesPanel(QGroupBox):
    """
    Painel Modular de Assistente Pessoal (sincronizado com a Flash NVS do ESP32-S3):
    - Aba 1: Lista de Tarefas (To-Do) na NVS + Botão de Widget na Tela 296x240
    - Aba 2: Memórias de Longo Prazo do Usuário na NVS
    - Aba 3: Companheiro de Estudo Pomodoro + Botão de Carinho na Cabeça (GPIO 1 Touch)
    """
    def __init__(self, send_serial_fn, trigger_widget_fn, parent=None):
        super().__init__("🧠 Assistente Pessoal NVS (Tarefas, Memórias, Pomodoro & Carinho GPIO 1)", parent)
        self.send_serial = send_serial_fn
        self.trigger_widget = trigger_widget_fn
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
