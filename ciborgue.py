import os
import sys
import time
import json
import threading
import winsound
from datetime import datetime
import serial
import speech_recognition as sr
import win32com.client
from dotenv import load_dotenv

# Carrega variáveis de ambiente (.env)
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
speaker.Rate = 2  # Fala um pouco mais rápida e dinâmica

# Conexão Serial com o ESP32-S3
ser = None
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.5)
    time.sleep(1.0)
    print(f"[OK] Conectado ao ESP32-S3 na porta {SERIAL_PORT}")
except Exception as e:
    print(f"[AVISO] Nao foi possivel abrir {SERIAL_PORT}: {e}")
    print("Modo de simulacao ativo para o hardware.")

def send_esp_cmd(cmd: str):
    """Envia comandos para a placa ESP32 e recebe confirmacao."""
    global ser
    if ser and ser.is_open:
        try:
            ser.write((cmd.strip() + "\n").encode())
            time.sleep(0.05)
            while ser.in_waiting:
                ser.readline()
        except Exception as e:
            print(f"[ERRO SERIAL] {e}")

def set_state(state: str):
    """Define o estado do robo no ESP32: IDLE, LISTEN, THINK, SPEAK, ALERT"""
    print(f"[ESTADO] -> {state}")
    send_esp_cmd(f"STATE:{state}")

def set_rgb(r: int, g: int, b: int):
    """Define cor customizada no LED RGB onboard."""
    print(f"[RGB] -> R:{r} G:{g} B:{b}")
    send_esp_cmd(f"RGB:{r},{g},{b}")

def falar(texto: str):
    """Fala o texto em voz alta e atualiza o LED do ESP32 para modo fala."""
    print(f"\n[CIBORGUE]: {texto}\n")
    set_state("SPEAK")
    try:
        speaker.Speak(texto)
    except Exception as e:
        print(f"[ERRO VOZ] {e}")
    set_state("IDLE")

# Inicialização da API do Gemini
gemini_client = None
if GEMINI_API_KEY and GEMINI_API_KEY != "SUA_CHAVE_AQUI":
    try:
        from google import genai
        from google.genai import types
        gemini_client = genai.Client(api_key=GEMINI_API_KEY)
        print("[OK] Gemini AI conectado com sucesso!")
    except Exception as e:
        print(f"[ERRO GEMINI] {e}")
else:
    print("\n[ATENCAO] Adicione sua GEMINI_API_KEY no arquivo .env!")

# Definição das Ferramentas (Tool / Function Calling) do Ciborgue
COLOR_MAP = {
    "vermelho": (255, 0, 0),
    "verde": (0, 255, 0),
    "azul": (0, 0, 255),
    "amarelo": (255, 200, 0),
    "roxo": (200, 0, 255),
    "ciano": (0, 255, 255),
    "branco": (255, 255, 255),
    "laranja": (255, 80, 0),
    "rosa": (255, 20, 147),
    "apagado": (0, 0, 0),
    "desligar": (0, 0, 0)
}

def tool_mudar_cor_led(cor: str) -> str:
    """Muda a cor do LED RGB no corpo do robo (ESP32). Ex: vermelho, verde, azul, roxo, amarelo, desligar."""
    c = cor.lower().strip()
    if c in COLOR_MAP:
        r, g, b = COLOR_MAP[c]
        set_rgb(r, g, b)
        return f"Cor do LED alterada para {c} com sucesso."
    else:
        set_rgb(100, 100, 100)
        return f"Cor {cor} nao reconhecida exatamente, coloquei em branco suave."

def tool_obter_hora() -> str:
    """Retorna a hora e data atual."""
    agora = datetime.now()
    return agora.strftime("Agora sao %H horas e %M minutos de %d/%m/%Y.")

def tool_mover_robo(direcao: str, angulo: int = 45) -> str:
    """Prepara o movimento dos servo-motores do robo (cabeca/corpo). Ex: esquerda, direita, frente, cima."""
    print(f"[MOVIMENTO] Movendo para {direcao} no angulo {angulo} graus.")
    return f"Movimento para {direcao} em {angulo} graus executado."

def processar_ia(pergunta: str):
    """Envia a pergunta para o Gemini com raciocinio e execucao de funcoes."""
    global gemini_client
    if not gemini_client:
        falar("Por favor, configure sua chave de API do Gemini no arquivo .env para que eu possa pensar.")
        return

    set_state("THINK")
    system_instruction = (
        "Você é o 'Ciborgue', um robôzinho de mesa inteligente, amigável e prestativo. "
        "Você é executado em um ESP32-S3 (com LED RGB integrado no pino GPIO 48). "
        "Suas respostas serão faladas em voz alta, então seja breve, objetivo e natural. "
        "Evite listas longas e símbolos complexos. "
        "Quando o usuário pedir para mudar a cor da luz ou do LED, use a função correspondente. "
        "Quando o usuário perguntar as horas, use a função de obter hora."
    )

    try:
        from google.genai import types

        # Configura as ferramentas suportadas
        tools = [
            types.Tool(function_declarations=[
                types.FunctionDeclaration(
                    name="tool_mudar_cor_led",
                    description="Muda a cor do LED RGB do corpo do robô no ESP32.",
                    parameters=types.Schema(
                        type=types.Type.OBJECT,
                        properties={
                            "cor": types.Schema(type=types.Type.STRING, description="Nome da cor em português (ex: vermelho, verde, azul, roxo, amarelo, desligar).")
                        },
                        required=["cor"]
                    )
                ),
                types.FunctionDeclaration(
                    name="tool_obter_hora",
                    description="Obtém o horário e data atual.",
                    parameters=types.Schema(
                        type=types.Type.OBJECT,
                        properties={}
                    )
                ),
                types.FunctionDeclaration(
                    name="tool_mover_robo",
                    description="Move a cabeça ou corpo do robô para uma direção (esquerda, direita, cima, centro).",
                    parameters=types.Schema(
                        type=types.Type.OBJECT,
                        properties={
                            "direcao": types.Schema(type=types.Type.STRING, description="Direção do movimento"),
                            "angulo": types.Schema(type=types.Type.INTEGER, description="Ângulo de movimento de 0 a 180 graus")
                        },
                        required=["direcao"]
                    )
                )
            ])
        ]

        response = gemini_client.models.generate_content(
            model="gemini-2.5-flash",
            contents=pergunta,
            config=types.GenerateContentConfig(
                system_instruction=system_instruction,
                tools=tools,
                temperature=0.7,
            )
        )

        # Checa se a IA pediu para executar uma função (Tool Call)
        has_tool_call = False
        if response.function_calls:
            for call in response.function_calls:
                has_tool_call = True
                fn_name = call.name
                fn_args = call.args or {}
                print(f"[TOOL CALL] {fn_name}({fn_args})")
                
                tool_result = ""
                if fn_name == "tool_mudar_cor_led":
                    tool_result = tool_mudar_cor_led(fn_args.get("cor", "azul"))
                elif fn_name == "tool_obter_hora":
                    tool_result = tool_obter_hora()
                elif fn_name == "tool_mover_robo":
                    tool_result = tool_mover_robo(fn_args.get("direcao", "frente"), int(fn_args.get("angulo", 45)))

                # Envia o resultado da ferramenta de volta para o Gemini gerar a resposta falada
                follow_up = gemini_client.models.generate_content(
                    model="gemini-2.5-flash",
                    contents=[
                        pergunta,
                        types.Part.from_function_response(name=fn_name, response={"result": tool_result})
                    ],
                    config=types.GenerateContentConfig(system_instruction=system_instruction)
                )
                if follow_up.text:
                    falar(follow_up.text.strip())
                    return

        if response.text:
            falar(response.text.strip())

    except Exception as e:
        print(f"[ERRO IA] {e}")
        falar("Tive um problema ao processar sua pergunta.")

def beep_wake():
    """Toca um bipe suave de ativacao."""
    try:
        winsound.Beep(1200, 120)
    except:
        pass

def ciclo_escuta_microfone():
    """Loop contínuo de escuta do microfone com detecção da Wake Word."""
    recognizer = sr.Recognizer()
    recognizer.dynamic_energy_threshold = True
    recognizer.pause_threshold = 0.8

    # Procura pelo microfone físico
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
    print(" Diga 'Ciborgue' para acordá-lo ou digite no terminal! ")
    print("=======================================================\n")
    set_state("IDLE")

    with mic as source:
        print("[MIC] Calibrando ruído ambiente...")
        recognizer.adjust_for_ambient_noise(source, duration=1.0)
        print("[MIC] Calibração concluída! Escutando...")

        while True:
            try:
                audio = recognizer.listen(source, timeout=None, phrase_time_limit=8)
                try:
                    texto = recognizer.recognize_google(audio, language="pt-BR").lower()
                    print(f"[VOCÊ DISSE]: {texto}")

                    # Checa se a palavra de ativação está no texto
                    wake_found = any(w in texto for w in WAKE_WORDS)

                    if wake_found:
                        beep_wake()
                        set_state("LISTEN")

                        # Remove a palavra de ativação para extrair o comando
                        comando = texto
                        for w in WAKE_WORDS:
                            comando = comando.replace(w, "").strip()

                        if len(comando) > 2:
                            # O usuário falou "Ciborgue, acenda a luz" em uma frase só
                            processar_ia(comando)
                        else:
                            # O usuário falou apenas "Ciborgue"
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

            except Exception as e:
                time.sleep(0.5)

def loop_terminal():
    """Permite enviar comandos ou testar digitando diretamente no terminal."""
    while True:
        try:
            texto = input("\n[DIGITAR COMANDO]: ").strip()
            if not texto:
                continue
            if texto.lower() in ["sair", "exit"]:
                os._exit(0)
            if texto.lower().startswith("cor:"):
                tool_mudar_cor_led(texto.split(":", 1)[1])
            else:
                processar_ia(texto)
        except (KeyboardInterrupt, EOFError):
            break

if __name__ == "__main__":
    set_state("IDLE")

    # Inicia a thread do terminal
    t_term = threading.Thread(target=loop_terminal, daemon=True)
    t_term.start()

    # Inicia a escuta do microfone na thread principal
    try:
        ciclo_escuta_microfone()
    except KeyboardInterrupt:
        print("\nEncerrando Ciborgue...")
        set_state("IDLE")
        if ser:
            ser.close()
