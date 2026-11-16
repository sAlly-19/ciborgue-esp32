import os
import ctypes
import shutil
import urllib.parse
import webbrowser
import subprocess
import threading

from app.signals import BridgeSignals


# Códigos de teclas virtuais multimídia do Windows (user32.dll)
VK_VOLUME_MUTE = 0xAD
VK_VOLUME_DOWN = 0xAE
VK_VOLUME_UP = 0xAF
VK_MEDIA_NEXT_TRACK = 0xB0
VK_MEDIA_PREV_TRACK = 0xB1
VK_MEDIA_STOP = 0xB2
VK_MEDIA_PLAY_PAUSE = 0xB3


class MEMORYSTATUSEX(ctypes.Structure):
    _fields_ = [
        ("dwLength", ctypes.c_ulong),
        ("dwMemoryLoad", ctypes.c_ulong),
        ("ullTotalPhys", ctypes.c_ulonglong),
        ("ullAvailPhys", ctypes.c_ulonglong),
        ("ullTotalPageFile", ctypes.c_ulonglong),
        ("ullAvailPageFile", ctypes.c_ulonglong),
        ("ullTotalVirtual", ctypes.c_ulonglong),
        ("ullAvailVirtual", ctypes.c_ulonglong),
        ("sullAvailExtendedVirtual", ctypes.c_ulonglong),
    ]


class SYSTEM_POWER_STATUS(ctypes.Structure):
    _fields_ = [
        ("ACLineStatus", ctypes.c_byte),
        ("BatteryFlag", ctypes.c_byte),
        ("BatteryLifePercent", ctypes.c_byte),
        ("SystemStatusFlag", ctypes.c_byte),
        ("BatteryLifeTime", ctypes.c_ulong),
        ("BatteryFullLifeTime", ctypes.c_ulong),
    ]


class PcController:
    """
    Módulo de Automação e Controle Total do PC Windows acionado por voz via ESP32-S3:
    - Mídia (Play/Pause, Próxima, Anterior, Parar)
    - Volume (Subir, Descer, Mutar, Nível 0-100%)
    - Abertura de Programas, Sites e Buscas (VS Code, Chrome, Spotify, Calculadora, YouTube, Google)
    - Gerenciamento de Sessão e Processos (Bloquear tela, Suspender, Desligar, Reiniciar, Fechar processos)
    - Diagnóstico de Hardware do PC (RAM, Disco C:/D:, Bateria)
    """
    def __init__(self, signals: BridgeSignals):
        self.signals = signals

    def execute_from_esp32(self, payload: dict):
        threading.Thread(target=self._run_action, args=(payload,), daemon=True).start()

    def _tap_vk(self, vk_code: int, times: int = 1):
        for _ in range(max(1, times)):
            ctypes.windll.user32.keybd_event(vk_code, 0, 0, 0)
            ctypes.windll.user32.keybd_event(vk_code, 0, 2, 0)

    def _set_volume_approx(self, target_pct: int):
        target_pct = max(0, min(100, target_pct))
        # Zera o volume com 50 toques rápidos de -2% e sobe até o nível desejado
        self._tap_vk(VK_VOLUME_DOWN, 50)
        steps_up = round(target_pct / 2.0)
        if steps_up > 0:
            self._tap_vk(VK_VOLUME_UP, steps_up)

    def get_pc_diagnostics(self) -> dict:
        mem = MEMORYSTATUSEX()
        mem.dwLength = ctypes.sizeof(MEMORYSTATUSEX)
        ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(mem))

        ram_used_pct = int(mem.dwMemoryLoad)
        ram_avail_gb = round(mem.ullAvailPhys / (1024 ** 3), 1)
        ram_total_gb = round(mem.ullTotalPhys / (1024 ** 3), 1)

        c_free_gb = round(shutil.disk_usage("C:\\").free / (1024 ** 3), 1)
        d_free_gb = round(shutil.disk_usage("D:\\").free / (1024 ** 3), 1) if os.path.exists("D:\\") else 0.0

        pwr = SYSTEM_POWER_STATUS()
        ctypes.windll.kernel32.GetSystemPowerStatus(ctypes.byref(pwr))
        bat_pct = int(pwr.BatteryLifePercent) if pwr.BatteryLifePercent <= 100 else None

        return {
            "ram_pct": ram_used_pct,
            "ram_avail_gb": ram_avail_gb,
            "ram_total_gb": ram_total_gb,
            "c_free_gb": c_free_gb,
            "d_free_gb": d_free_gb,
            "battery_pct": bat_pct,
        }

    def _run_action(self, payload: dict):
        action = str(payload.get("action", "")).strip().lower()
        param = str(payload.get("param", "")).strip()
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0x08000000)

        try:
            if action == "media_play_pause":
                self._tap_vk(VK_MEDIA_PLAY_PAUSE, 1)
                self.signals.log_message.emit("💻 <b>[PC Control]</b> Play/Pause de mídia acionado.")

            elif action == "media_next":
                self._tap_vk(VK_MEDIA_NEXT_TRACK, 1)
                self.signals.log_message.emit("💻 <b>[PC Control]</b> Próxima faixa de mídia.")

            elif action == "media_prev":
                self._tap_vk(VK_MEDIA_PREV_TRACK, 1)
                self.signals.log_message.emit("💻 <b>[PC Control]</b> Faixa anterior de mídia.")

            elif action == "volume_up":
                steps = 5
                if param.isdigit():
                    steps = max(1, int(param) // 2)
                self._tap_vk(VK_VOLUME_UP, steps)
                self.signals.log_message.emit(f"💻 <b>[PC Control]</b> Volume aumentado (+{steps * 2}%).")

            elif action == "volume_down":
                steps = 5
                if param.isdigit():
                    steps = max(1, int(param) // 2)
                self._tap_vk(VK_VOLUME_DOWN, steps)
                self.signals.log_message.emit(f"💻 <b>[PC Control]</b> Volume reduzido (-{steps * 2}%).")

            elif action == "volume_mute":
                self._tap_vk(VK_VOLUME_MUTE, 1)
                self.signals.log_message.emit("💻 <b>[PC Control]</b> Mudo alternado no Windows.")

            elif action == "volume_set":
                digits = "".join(ch for ch in param if ch.isdigit())
                lvl = int(digits) if digits else 50
                self._set_volume_approx(lvl)
                self.signals.log_message.emit(f"💻 <b>[PC Control]</b> Volume do PC ajustado para ~{lvl}%.")

            elif action == "open_app":
                app_key = param.lower()
                app_map = {
                    "vscode": "code",
                    "vs code": "code",
                    "code": "code",
                    "chrome": "start chrome",
                    "navegador": "start chrome",
                    "edge": "start msedge",
                    "bloco de notas": "notepad",
                    "notepad": "notepad",
                    "calculadora": "calc",
                    "calc": "calc",
                    "explorer": "explorer",
                    "arquivos": "explorer",
                    "spotify": "start spotify:",
                    "discord": "start discord:",
                    "cmd": "start cmd",
                    "terminal": "start powershell",
                    "gerenciador de tarefas": "taskmgr",
                    "taskmgr": "taskmgr",
                }
                cmd_str = app_map.get(app_key, f"start {param}")
                subprocess.Popen(cmd_str, shell=True)
                self.signals.log_message.emit(f"💻 <b>[PC Control]</b> Abrindo aplicativo: <b>{param}</b>")

            elif action == "open_url":
                url = param if param.startswith("http") else f"https://{param}"
                webbrowser.open(url)
                self.signals.log_message.emit(f"💻 <b>[PC Control]</b> Abrindo URL: <b>{url}</b>")

            elif action == "youtube_search":
                q = urllib.parse.quote_plus(param or "musica lofi")
                webbrowser.open(f"https://www.youtube.com/results?search_query={q}")
                self.signals.log_message.emit(f"💻 <b>[PC Control]</b> Pesquisa no YouTube: <b>{param}</b>")

            elif action == "google_search":
                q = urllib.parse.quote_plus(param)
                webbrowser.open(f"https://www.google.com/search?q={q}")
                self.signals.log_message.emit(f"💻 <b>[PC Control]</b> Pesquisa no Google: <b>{param}</b>")

            elif action == "lock_pc":
                self.signals.log_message.emit("💻 <b>[PC Control]</b> Bloqueando a tela do Windows...")
                ctypes.windll.user32.LockWorkStation()

            elif action == "sleep_pc":
                self.signals.log_message.emit("💻 <b>[PC Control]</b> Suspendendo o Windows...")
                ctypes.windll.PowrProf.SetSuspendState(0, 1, 0)

            elif action == "shutdown_pc":
                self.signals.log_message.emit("⚠️ <b>[PC Control]</b> Desligando o Windows em 10 segundos (use 'cancel_shutdown' para abortar)...")
                subprocess.Popen(["shutdown", "/s", "/t", "10"], creationflags=creationflags)

            elif action == "restart_pc":
                self.signals.log_message.emit("⚠️ <b>[PC Control]</b> Reiniciando o Windows em 10 segundos...")
                subprocess.Popen(["shutdown", "/r", "/t", "10"], creationflags=creationflags)

            elif action == "cancel_shutdown":
                subprocess.Popen(["shutdown", "/a"], creationflags=creationflags)
                self.signals.log_message.emit("✅ <b>[PC Control]</b> Desligamento/reinicialização do Windows cancelado!")

            elif action == "kill_process":
                proc_name = param if param.lower().endswith(".exe") else f"{param}.exe"
                subprocess.Popen(
                    ["taskkill", "/F", "/IM", proc_name],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    creationflags=creationflags
                )
                self.signals.log_message.emit(f"💻 <b>[PC Control]</b> Encerrando processo: <b>{proc_name}</b>")

            elif action == "pc_status":
                diag = self.get_pc_diagnostics()
                bat_txt = f" e bateria em {diag['battery_pct']}%" if diag["battery_pct"] is not None else ""
                summary = (
                    f"No seu PC Windows, a memória RAM está em {diag['ram_pct']}% de uso "
                    f"({diag['ram_avail_gb']} GB livres de {diag['ram_total_gb']} GB), "
                    f"disco C com {diag['c_free_gb']} GB livres e disco D com {diag['d_free_gb']} GB livres{bat_txt}."
                )
                self.signals.log_message.emit(f"💻 <b>[PC Diagnóstico]</b> {summary}")
                self.signals.widget_triggered.emit({
                    "type": "PC_CONTROL",
                    "action": "Diagnóstico do PC",
                    "param": f"RAM: {diag['ram_pct']}% | C: {diag['c_free_gb']}GB | D: {diag['d_free_gb']}GB"
                })
                self.signals.say_requested.emit(summary)

        except Exception as e:
            self.signals.log_message.emit(f"⚠️ Erro ao executar ação no PC ({action}): {e}")
