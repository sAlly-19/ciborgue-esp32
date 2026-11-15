import os
import json
import time
import threading
import serial
import serial.tools.list_ports

from app.signals import BridgeSignals


def find_esp32_port(preferred: str = "AUTO") -> str:
    """Localiza automaticamente a porta serial do ESP32-S3 (CH343 / CP210x / USB JTAG)."""
    ports = list(serial.tools.list_ports.comports())
    if preferred and preferred.upper() != "AUTO":
        for p in ports:
            if p.device.upper() == preferred.upper():
                return p.device

    for p in ports:
        desc = f"{p.description} {p.manufacturer or ''} {p.hwid}".lower()
        if any(k in desc for k in ["ch343", "ch340", "cp210", "espressif", "usb-enhanced-serial", "jtag", "uart"]):
            return p.device

    for p in ports:
        if p.device.upper() != "COM1":
            return p.device
    return "COM4"


class SerialBridgeManager:
    """
    Gerencia a conexão bidirecional USB Serial com o ESP32-S3,
    auto-reconexão, provisionamento inicial NVS e decodificação de eventos EVT:*.
    """
    def __init__(self, signals: BridgeSignals):
        self.signals = signals
        self.ser = None
        self.lock = threading.Lock()
        self.running = True
        self.provisioned_once = False

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
        with self.lock:
            if self.ser and self.ser.is_open:
                try:
                    self.ser.write((cmd.strip() + "\n").encode("utf-8"))
                except Exception as e:
                    self.signals.log_message.emit(f"⚠️ Erro ao enviar serial: {e}")

    def provision_nvs(self, ssid: str, pw: str, key: str, city: str):
        if key and key != "sua_chave_gemini_aqui":
            self.send_cmd(f"CFG:KEY={key}")
            time.sleep(0.12)
        if city:
            self.send_cmd(f"CFG:CITY={city}")
            time.sleep(0.12)
        if ssid and ssid != "seu_wifi_2.4ghz_aqui":
            self.send_cmd(f"CFG:WIFI={ssid}|{pw}")

    def _worker_loop(self):
        preferred_port = os.getenv("SERIAL_PORT", "AUTO")
        baud = int(os.getenv("BAUD_RATE", "115200"))

        while self.running:
            if not self.ser or not self.ser.is_open:
                port = find_esp32_port(preferred_port)
                try:
                    s = serial.Serial()
                    s.port = port
                    s.baudrate = baud
                    s.timeout = 0.2
                    s.dtr = False
                    s.rts = False
                    s.open()
                    self.ser = s
                    self.signals.serial_status.emit(True, port)
                    self.signals.log_message.emit(f"🔌 Conectado ao ESP32-S3 na porta <b>{port}</b>.")
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
                    self.signals.serial_status.emit(False, f"Aguardando {port}...")
                    time.sleep(2.0)
                    continue

            try:
                if self.ser.in_waiting > 0:
                    raw = self.ser.readline().decode("utf-8", errors="ignore").strip()
                    if raw:
                        self._handle_line(raw)
                else:
                    time.sleep(0.015)
            except Exception:
                self.signals.serial_status.emit(False, "Reconectando...")
                try:
                    self.ser.close()
                except Exception:
                    pass
                self.ser = None
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
        elif line.startswith("EVT:LOG="):
            msg = line.split("=", 1)[1].strip()
            self.signals.log_message.emit(f"<span style='color:#94a3b8'>[ESP32] {msg}</span>")
        elif line.startswith("EVT:TIMER="):
            info = line.split("=", 1)[1].strip()
            self.signals.log_message.emit(f"<span style='color:#fbbf24'>⏰ Timer agendado no ESP32: {info}</span>")
