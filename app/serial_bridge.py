import os
import json
import time
import socket
import threading
import requests
import serial
import serial.tools.list_ports

from app.config import PROJECT_ROOT
from app.signals import BridgeSignals


MIRROR_JSON_PATH = PROJECT_ROOT / "ciborgue_memory_mirror.json"


def find_esp32_serial_port(preferred: str = "AUTO") -> str | None:
    """
    Localiza a porta USB Serial física do ESP32-S3 se estiver conectada no PC.
    Retorna None caso o ESP32 esteja desconectado do USB (ex.: ligado na tomada/carregador).
    """
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        return None

    if preferred and preferred.upper() != "AUTO":
        for p in ports:
            if p.device.upper() == preferred.upper():
                return p.device

    for p in ports:
        desc = f"{p.description} {p.manufacturer or ''} {p.hwid}".lower()
        if any(k in desc for k in ["ch343", "ch340", "cp210", "espressif", "usb-enhanced-serial", "jtag", "uart", "usb"]):
            return p.device

    for p in ports:
        if p.device.upper() != "COM1":
            return p.device
    return None


def discover_esp32_wifi_ip(timeout: float = 1.2) -> str | None:
    """
    Descobre automaticamente o IP do ESP32-S3 na rede Wi-Fi via Broadcast UDP na porta 4210
    (mesmo protocolo usado pelo App Android).
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(timeout)

    # Calcula broadcast da sub-rede local do PC + broadcast global
    targets = ["255.255.255.255"]
    try:
        local_ip = socket.gethostbyname(socket.gethostname())
        parts = local_ip.split(".")
        if len(parts) == 4:
            targets.insert(0, f"{parts[0]}.{parts[1]}.{parts[2]}.255")
    except Exception:
        pass

    found_ip = None
    for addr in targets:
        try:
            sock.sendto(b"CIBORGUE_DISCOVER", (addr, 4210))
            data, sender = sock.recvfrom(1024)
            txt = data.decode("utf-8", errors="ignore")
            if "Ciborgue" in txt or "ip" in txt:
                found_ip = sender[0]
                break
        except Exception:
            continue
    sock.close()
    return found_ip


class SerialBridgeManager:
    """
    Ponte Híbrida Inteligente (USB Serial + Wi-Fi Automático):
    - Se o ESP32-S3 estiver no cabo USB (ex: COM4), comunica via Serial a 115200 bps.
    - Se o ESP32-S3 estiver ligado apenas na tomada/carregador, descobre automaticamente o IP
      dele na rede Wi-Fi (UDP 4210) e sincroniza tudo (Voz, Rosto 296x240, Telemetria, Tarefas,
      Memórias e Comandos) via HTTP REST sem precisar de cabo USB!
    """
    def __init__(self, signals: BridgeSignals):
        self.signals = signals
        self.ser = None
        self.wifi_ip = None
        self.http_session = requests.Session()
        self.lock = threading.Lock()
        self.running = True
        self.provisioned_once = False
        self.last_wifi_reply = None
        self.last_sync_tasks_ts = 0.0
        self.mirror_state = {"memories": [], "tasks": []}
        self._load_local_mirror()

    def _load_local_mirror(self):
        try:
            if MIRROR_JSON_PATH.exists():
                self.mirror_state = json.loads(MIRROR_JSON_PATH.read_text(encoding="utf-8"))
        except Exception:
            pass

    def _save_local_mirror(self):
        try:
            MIRROR_JSON_PATH.write_text(
                json.dumps(self.mirror_state, ensure_ascii=False, indent=2),
                encoding="utf-8"
            )
        except Exception:
            pass

    def start(self):
        threading.Thread(target=self._worker_loop, daemon=True).start()

    def stop(self):
        self.running = False
        with self.lock:
            if self.ser and self.ser.is_open:
                try:
                    self.ser.close()
                except Exception:
                    pass

    def send_cmd(self, cmd: str):
        cmd = cmd.strip()
        if not cmd:
            return

        # 1. Se conectado via Cabo USB Serial
        with self.lock:
            if self.ser and self.ser.is_open:
                try:
                    self.ser.write((cmd + "\n").encode("utf-8"))
                    return
                except Exception as e:
                    self.signals.log_message.emit(f"⚠️ Erro ao enviar serial: {e}")

        # 2. Se conectado sem fio via Wi-Fi (ESP32 ligado na tomada/carregador)
        if self.wifi_ip:
            threading.Thread(target=self._send_wifi_cmd_async, args=(self.wifi_ip, cmd), daemon=True).start()

    def _send_wifi_cmd_async(self, ip: str, cmd: str):
        base_url = f"http://{ip}"
        try:
            if cmd.startswith("MIC:"):
                pergunta = cmd[4:].strip()
                if not pergunta:
                    return
                self.signals.state_changed.emit("THINK")
                r = self.http_session.post(
                    f"{base_url}/api/chat",
                    json={"message": pergunta},
                    timeout=15
                )
                if r.status_code == 200:
                    data = r.json()
                    reply = data.get("reply", "").strip()
                    emo = data.get("emotion", "")
                    if emo:
                        self.signals.emotion_changed.emit(emo)
                    if reply:
                        self.last_wifi_reply = reply
                        self.signals.say_requested.emit(reply)
                    self._sync_wifi_tasks_and_memories(ip)
            else:
                r = self.http_session.post(
                    f"{base_url}/api/command",
                    json={"cmd": cmd},
                    timeout=5
                )
                if r.status_code == 200:
                    self._apply_wifi_status_dict(r.json(), speak_on_new_reply=(cmd == "CMD:PET"))
                    if cmd.startswith("CMD:TASK_") or cmd.startswith("CMD:MEM_"):
                        self._sync_wifi_tasks_and_memories(ip)
        except Exception as e:
            self.signals.log_message.emit(f"⚠️ Aviso Wi-Fi ({ip}): {e}")

    def provision_nvs(self, ssid: str, pw: str, key: str, city: str):
        if self.ser and self.ser.is_open:
            if key and key != "sua_chave_gemini_aqui":
                self.send_cmd(f"CFG:KEY={key}")
                time.sleep(0.12)
            if city:
                self.send_cmd(f"CFG:CITY={city}")
                time.sleep(0.12)
            if ssid and ssid != "seu_wifi_2.4ghz_aqui":
                self.send_cmd(f"CFG:WIFI={ssid}|{pw}")
        elif self.wifi_ip:
            def _post_cfg():
                try:
                    self.http_session.post(
                        f"http://{self.wifi_ip}/api/config",
                        json={
                            "wifi_ssid": ssid,
                            "wifi_password": pw,
                            "gemini_key": key,
                            "city": city
                        },
                        timeout=6
                    )
                except Exception:
                    pass
            threading.Thread(target=_post_cfg, daemon=True).start()

    def _sync_wifi_tasks_and_memories(self, ip: str):
        try:
            rt = self.http_session.get(f"http://{ip}/api/tasks", timeout=3)
            if rt.status_code == 200:
                tasks = rt.json()
                if isinstance(tasks, list):
                    self.mirror_state["tasks"] = tasks
                    self._save_local_mirror()
                    self.signals.tasks_updated.emit(tasks)

            rm = self.http_session.get(f"http://{ip}/api/memories", timeout=3)
            if rm.status_code == 200:
                mems = rm.json()
                if isinstance(mems, list):
                    self.mirror_state["memories"] = mems
                    self._save_local_mirror()
                    self.signals.memories_updated.emit(mems)
        except Exception:
            pass

    def _apply_wifi_status_dict(self, data: dict, speak_on_new_reply: bool = True):
        if not isinstance(data, dict):
            return
        self.signals.telemetry_received.emit(data)

        pomo = {
            "active": data.get("pomo_active", False),
            "phase": "FOCUS",
            "rem_s": data.get("pomo_rem_s", 0),
            "total_s": 1500,
            "goal": data.get("pomo_goal", "Foco")
        }
        self.signals.pomodoro_updated.emit(pomo)

        reply = str(data.get("last_reply", "")).strip()
        if self.last_wifi_reply is None:
            # Inicializa na primeira leitura sem falar a frase antiga do boot
            self.last_wifi_reply = reply
        elif reply and reply != self.last_wifi_reply and speak_on_new_reply:
            # Se você falou com o Ciborgue pelo App Android, o PC também mostra e fala em sincronia!
            self.last_wifi_reply = reply
            self.signals.log_message.emit(f"📱 <b>[Sincronia Wi-Fi / App Android]</b> Nova resposta detectada no ESP32!")
            self.signals.say_requested.emit(reply)

    def _worker_loop(self):
        preferred_port = os.getenv("SERIAL_PORT", "AUTO")
        baud = int(os.getenv("BAUD_RATE", "115200"))

        while self.running:
            # 1. Prioridade 1: Verifica se o cabo USB Serial está conectado
            usb_port = find_esp32_serial_port(preferred_port)
            if usb_port:
                if not self.ser or not self.ser.is_open:
                    try:
                        s = serial.Serial()
                        s.port = usb_port
                        s.baudrate = baud
                        s.timeout = 0.2
                        s.dtr = False
                        s.rts = False
                        s.open()
                        self.ser = s
                        self.wifi_ip = None
                        self.signals.serial_status.emit(True, f"USB {usb_port}")
                        self.signals.log_message.emit(f"🔌 Conectado ao ESP32-S3 via Cabo USB na porta <b>{usb_port}</b>.")
                        time.sleep(0.4)
                        self.send_cmd("PING")

                        if not self.provisioned_once:
                            self.provisioned_once = True
                            key = os.getenv("GEMINI_API_KEY", "").strip()
                            city = os.getenv("DEFAULT_CITY", "Sao Paulo").strip()
                            if key and key != "sua_chave_gemini_aqui":
                                self.send_cmd(f"CFG:KEY={key}")
                                time.sleep(0.1)
                            if city:
                                self.send_cmd(f"CFG:CITY={city}")
                                time.sleep(0.1)
                    except Exception:
                        self.ser = None

                if self.ser and self.ser.is_open:
                    try:
                        if self.ser.in_waiting > 0:
                            raw = self.ser.readline().decode("utf-8", errors="ignore").strip()
                            if raw:
                                self._handle_line(raw)
                        else:
                            time.sleep(0.015)
                        continue
                    except Exception:
                        try:
                            self.ser.close()
                        except Exception:
                            pass
                        self.ser = None

            # 2. Prioridade 2: Sem cabo USB -> Conecta automaticamente ao ESP32-S3 via Rede Wi-Fi!
            if not self.wifi_ip:
                found = discover_esp32_wifi_ip(timeout=1.0)
                if found:
                    self.wifi_ip = found
                    self.last_wifi_reply = None
                    self.signals.serial_status.emit(True, f"Wi-Fi {found} (Sem Fio)")
                    self.signals.log_message.emit(
                        f"📡 <b>ESP32-S3 encontrado na Rede Wi-Fi ({found})!</b> "
                        f"Operando 100% sem fio (conectado na tomada/carregador + sincronizado com o App Android)."
                    )
                    # Destrava estado THINK caso o app Android tenha deixado em THINK
                    try:
                        self.http_session.post(f"http://{found}/api/command", json={"cmd": "TTS:DONE"}, timeout=2)
                    except Exception:
                        pass
                    self._sync_wifi_tasks_and_memories(found)
                else:
                    self.signals.serial_status.emit(False, "Procurando USB ou Wi-Fi (UDP 4210)...")
                    time.sleep(1.5)
                    continue

            # Polling leve via Wi-Fi a cada 0.85s
            try:
                r = self.http_session.get(f"http://{self.wifi_ip}/api/status", timeout=2.5)
                if r.status_code == 200:
                    self._apply_wifi_status_dict(r.json(), speak_on_new_reply=True)
                    if time.time() - self.last_sync_tasks_ts > 8.0:
                        self.last_sync_tasks_ts = time.time()
                        self._sync_wifi_tasks_and_memories(self.wifi_ip)
                time.sleep(0.85)
            except Exception:
                self.signals.serial_status.emit(False, f"Reconectando Wi-Fi ({self.wifi_ip})...")
                self.wifi_ip = None
                time.sleep(1.5)

    def _handle_line(self, line: str):
        if "waiting for download" in line.lower() or "download(usb/uart0)" in line.lower():
            try:
                self.ser.dtr = False
                self.ser.rts = True
                time.sleep(0.12)
                self.ser.rts = False
            except Exception:
                pass
            return

        if line.startswith("EVT:STATE="):
            st = line.split("=", 1)[1].strip()
            self.signals.state_changed.emit(st)
        elif line.startswith("EVT:EMOTION="):
            emo = line.split("=", 1)[1].strip()
            self.signals.emotion_changed.emit(emo)
        elif line.startswith("EVT:LED="):
            parts = line.split("=", 1)[1].split(",")
            if len(parts) >= 4:
                self.signals.led_changed.emit(int(parts[0]), int(parts[1]), int(parts[2]), parts[3])
        elif line.startswith("EVT:SAY="):
            msg = line.split("=", 1)[1].strip()
            self.signals.say_requested.emit(msg)
        elif line.startswith("EVT:TELEMETRY="):
            raw_json = line.split("=", 1)[1].strip()
            try:
                data = json.loads(raw_json)
                self.signals.telemetry_received.emit(data)
            except Exception:
                pass
        elif line.startswith("EVT:MEMORIES="):
            try:
                mems = json.loads(line.split("=", 1)[1].strip())
                self.mirror_state["memories"] = mems
                self._save_local_mirror()
                self.signals.memories_updated.emit(mems)
            except Exception:
                pass
        elif line.startswith("EVT:TASKS="):
            try:
                tasks = json.loads(line.split("=", 1)[1].strip())
                self.mirror_state["tasks"] = tasks
                self._save_local_mirror()
                self.signals.tasks_updated.emit(tasks)
            except Exception:
                pass
        elif line.startswith("EVT:POMODORO="):
            try:
                pomo = json.loads(line.split("=", 1)[1].strip())
                self.signals.pomodoro_updated.emit(pomo)
            except Exception:
                pass
        elif line.startswith("EVT:WIDGET="):
            try:
                wdata = json.loads(line.split("=", 1)[1].strip())
                self.signals.widget_triggered.emit(wdata)
            except Exception:
                pass
        elif line.startswith("EVT:PC="):
            try:
                pc_data = json.loads(line.split("=", 1)[1].strip())
                self.signals.pc_action_requested.emit(pc_data)
            except Exception:
                pass
        elif line.startswith("EVT:PET="):
            self.signals.pet_triggered.emit()
        elif line.startswith("EVT:LOG="):
            msg = line.split("=", 1)[1].strip()
            self.signals.log_message.emit(f"<span style='color:#94a3b8'>[ESP32] {msg}</span>")
        elif line.startswith("EVT:TIMER="):
            info = line.split("=", 1)[1].strip()
            self.signals.log_message.emit(f"<span style='color:#fbbf24'>⏰ Timer agendado no ESP32: {info}</span>")
