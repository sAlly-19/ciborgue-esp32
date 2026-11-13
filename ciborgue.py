import os
import sys
import time
import json
import threading
import winsound
from datetime import datetime
import serial
import requests
import speech_recognition as sr
import win32com.client
from dotenv import load_dotenv

# Carrega configurações
load_dotenv()

GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "").strip()
SERIAL_PORT = os.getenv("SERIAL_PORT", "COM4")
BAUD_RATE = int(os.getenv("BAUD_RATE", "115200"))
WAKE_WORDS = ["ciborgue", "ciborg", "cyborg", "ciborgi", "sebo"]

# Inicializa o sintetizador de voz nativo do Windows (zero latência)
speaker = win32com.client.Dispatch("SAPI.SpVoice")
for v in speaker.GetVoices():
    if "Portuguese" in v.GetDescription() or "Maria" in v.GetDescription():
        speaker.Voice = v
        break
speaker.Rate = 2

# Conexão Serial com o ESP32-S3
ser = None
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.5)
    time.sleep(1.0)
    print(f"[OK] Conectado ao ESP32-S3 na porta {SERIAL_PORT}")
except Exception as e:
    print(f"[AVISO] Porta {SERIAL_PORT} nao disponivel ({e}). Modo de simulacao ativo.")

def send_esp_cmd(cmd: str):
    """Envia comandos para a placa ESP32."""
    global ser
    if ser and ser.is_open:
        try:
            ser.write((cmd.strip() + "\n").encode())
            time.sleep(0.04)
            while ser.in_waiting:
                ser.readline()
        except Exception as e:
            print(f"[ERRO SERIAL] {e}")

def set_state(state: str):
    """Muda estado visual no ESP32: IDLE, LISTEN, THINK, SPEAK, ALERT"""
    print(f"[ESTADO] -> {state}")
    send_esp_cmd(f"STATE:{state}")

def set_rgb(r: int, g: int, b: int):
    """Define cor customizada no LED RGB onboard (GPIO 48)."""
    print(f"[RGB] -> R:{r} G:{g} B:{b}")
    send_esp_cmd(f"RGB:{r},{g},{b}")

def falar(texto: str):
    """Fala o texto em voz alta e anima o LED do ESP32."""
    print(f"\n[CIBORGUE]: {texto}\n")
    set_state("SPEAK")
    try:
        speaker.Speak(texto)
    except Exception as e:
        print(f"[ERRO VOZ] {e}")
    set_state("IDLE")

# Mapeamento de cores
COLOR_MAP = {
    "vermelho": (255, 0, 0),
    "vermelha": (255, 0, 0),
    "verde": (0, 255, 0),
    "azul": (0, 0, 255),
    "amarelo": (255, 200, 0),
    "amarela": (255, 200, 0),
    "roxo": (200, 0, 255),
    "roxa": (200, 0, 255),
    "ciano": (0, 255, 255),
    "branco": (255, 255, 255),
    "branca": (255, 255, 255),
    "laranja": (255, 80, 0),
    "rosa": (255, 20, 147),
    "apagado": (0, 0, 0),
    "desligado": (0, 0, 0),
    "desligar": (0, 0, 0)
}

def tool_mudar_cor_led(cor: str) -> str:
    c = cor.lower().strip()
    if c in COLOR_MAP:
        r, g, b = COLOR_MAP[c]
        set_rgb(r, g, b)
        return f"Cor alterada para {c} com sucesso."
    else:
        set_rgb(120, 120, 120)
        return f"Cor {cor} colocada em branco suave."

def tool_obter_hora() -> str:
    agora = datetime.now()
    return agora.strftime("Agora sao %H horas e %M minutos.")

# Declaração de ferramentas para a API REST do Gemini
GEMINI_TOOLS = [{
    "function_declarations": [
        {
            "name": "mudar_cor_led",
            "description": "Altera a cor do LED RGB no corpo do robô no ESP32.",
            "parameters": {
                "type": "OBJECT",
                "properties": {
                    "cor": {"type": "STRING", "description": "Nome da cor em português (ex: vermelho, verde, azul, amarelo, roxo, ciano, rosa, desligar)"}
                },
                "required": ["cor"]
            }
        },
        {
            "name": "obter_hora",
            "description": "Obtém as horas e minutos atuais.",
            "parameters": {"type": "OBJECT", "properties": {}}
        }
    ]
}]

def processar_ia(pergunta: str):
    """Consulta o modelo Gemini 3 Flash com suporte nativo a ferramentas."""
    if not GEMINI_API_KEY:
        falar("Chave de API do Gemini não encontrada no arquivo .env.")
        return

    set_state("THINK")
    url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-3-flash-preview:generateContent?key={GEMINI_API_KEY}"
    
    system_prompt = (
        "Você é o 'Ciborgue', um robôzinho de mesa inteligente, amigável e prestativo. "
        "Você é executado em um ESP32-S3 (com LED RGB integrado no pino GPIO 48). "
        "Suas respostas serão faladas em voz alta, então seja breve, direto e conversacional. "
        "Quando o usuário pedir para mudar a cor da luz ou do LED, chame a função mudar_cor_led. "
        "Quando o usuário perguntar as horas, chame a função obter_hora."
    )

    payload = {
        "system_instruction": {"parts": [{"text": system_prompt}]},
        "contents": [{"parts": [{"text": pergunta}]}],
        "tools": GEMINI_TOOLS
    }

    try:
        r = requests.post(url, json=payload, timeout=8)
        if r.status_code != 200:
            print(f"[ERRO API] {r.status_code}: {r.text[:200]}")
            falar("Desculpe, tive uma oscilação na minha conexão de inteligência.")
            return

        data = r.json()
        candidates = data.get("candidates", [])
        if not candidates:
            falar("Não consegui formular uma resposta.")
            return

        parts = candidates[0].get("content", {}).get("parts", [])
        
        # Verifica se houve chamada de ferramenta
        tool_call = None
        texto_resposta = ""
        for p in parts:
            if "functionCall" in p:
                tool_call = p["functionCall"]
            elif "text" in p:
                texto_resposta = p["text"]

        if tool_call:
            fn_name = tool_call.get("name")
            fn_args = tool_call.get("args", {})
            print(f"[TOOL CALL] {fn_name}({fn_args})")

            resultado = ""
            if fn_name == "mudar_cor_led":
                resultado = tool_mudar_cor_led(fn_args.get("cor", "azul"))
            elif fn_name == "obter_hora":
                resultado = tool_obter_hora()

            # Envia o retorno da ferramenta de volta para gerar a fala final
            followup_payload = {
                "system_instruction": {"parts": [{"text": system_prompt}]},
                "contents": [
                    {"role": "user", "parts": [{"text": pergunta}]},
                    {"role": "model", "parts": [{"functionCall": tool_call}]},
                    {"role": "user", "parts": [{"functionResponse": {"name": fn_name, "response": {"result": resultado}}}]}
                ]
            }
            r_followup = requests.post(url, json=followup_payload, timeout=8)
            if r_followup.status_code == 200:
                f_data = r_followup.json()
                f_parts = f_data.get("candidates", [{}])[0].get("content", {}).get("parts", [])
                for fp in f_parts:
                    if "text" in fp and fp["text"].strip():
                        falar(fp["text"].strip())
                        return

            falar(resultado)
        elif texto_resposta:
            falar(texto_resposta.strip())

    except Exception as e:
        print(f"[ERRO PROCESSAMENTO] {e}")
        falar("Tive um imprevisto ao processar.")

def beep_wake():
    try:
        winsound.Beep(1200, 100)
    except:
        pass

def ciclo_escuta_microfone():
    recognizer = sr.Recognizer()
    recognizer.dynamic_energy_threshold = True
    recognizer.pause_threshold = 0.8

    mic_index = None
    for idx, name in enumerate(sr.Microphone.list_microphone_names()):
        if any(term in name.lower() for term in ["microfone", "mic", "input", "realtek"]):
            if "mixagem" not in name.lower():
                mic_index = idx
                print(f"[MIC] Utilizando microfone: {name} (index {idx})")
                break

    mic = sr.Microphone(device_index=mic_index) if mic_index is not None else sr.Microphone()

    print("\n=======================================================")
    print("      ROBO CIBORGUE PRONTO E AGUARDANDO VOCE!         ")
    print(" Diga 'Ciborgue' para acorda-lo ou digite no terminal! ")
    print("=======================================================\n")
    set_state("IDLE")

    with mic as source:
        print("[MIC] Calibrando ruido ambiente...")
        recognizer.adjust_for_ambient_noise(source, duration=1.0)
        print("[MIC] Calibracao concluida! Escutando...")

        while True:
            try:
                audio = recognizer.listen(source, timeout=None, phrase_time_limit=8)
                try:
                    texto = recognizer.recognize_google(audio, language="pt-BR").lower()
                    print(f"[VOCE DISSE]: {texto}")

                    wake_found = any(w in texto for w in WAKE_WORDS)
                    if wake_found:
                        beep_wake()
                        set_state("LISTEN")

                        comando = texto
                        for w in WAKE_WORDS:
                            comando = comando.replace(w, "").strip()

                        if len(comando) > 2:
                            processar_ia(comando)
                        else:
                            falar("Sim? O que você manda?")
                            set_state("LISTEN")
                            audio_cmd = recognizer.listen(source, timeout=5, phrase_time_limit=8)
                            comando_seguinte = recognizer.recognize_google(audio_cmd, language="pt-BR").strip()
                            print(f"[COMANDO]: {comando_seguinte}")
                            processar_ia(comando_seguinte)

                except sr.UnknownValueError:
                    pass
                except sr.RequestError as e:
                    print(f"[ERRO RECONHECIMENTO] {e}")

            except Exception:
                time.sleep(0.5)

def loop_terminal():
    while True:
        try:
            texto = input("\n[DIGITAR COMANDO]: ").strip()
            if not texto:
                continue
            if texto.lower() in ["sair", "exit"]:
                os._exit(0)
            processar_ia(texto)
        except (KeyboardInterrupt, EOFError):
            break

if __name__ == "__main__":
    set_state("IDLE")
    t_term = threading.Thread(target=loop_terminal, daemon=True)
    t_term.start()

    try:
        ciclo_escuta_microfone()
    except KeyboardInterrupt:
        print("\nEncerrando Ciborgue...")
        set_state("IDLE")
        if ser:
            ser.close()
