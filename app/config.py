import os
from pathlib import Path
from dotenv import load_dotenv

# Caminho raiz do projeto e carregamento do .env
PROJECT_ROOT = Path(__file__).resolve().parent.parent
ENV_PATH = PROJECT_ROOT / ".env"
load_dotenv(ENV_PATH)

# Palavras de ativação reconhecidas (incluindo variações fonéticas comuns do STT)
WAKE_WORDS = [
    "ciborgue", "ciborg", "cyborg", "ciborgi", "ciborgui",
    "siborg", "se borg", "sebo", "borg", "robô", "robo"
]

# Vozes expressivas (Neural Ultra-Rápida PT-BR + Gemini 3.8 Flash TTS)
GEMINI_VOICES = [
    ("Edge-Antonio", "⚡ Antonio Neural (Ultra-Rápido ~0.8s - Sem Limite de Cota)"),
    ("Edge-Francisca", "⚡ Francisca Neural (Ultra-Rápida ~0.8s - Sem Limite de Cota)"),
    ("Puck", "🤖 Puck (Gemini 3.8 Studio - Carismático)"),
    ("Kore", "✨ Kore (Gemini 3.8 Studio - Feminina Doce)"),
    ("Fenrir", "🦾 Fenrir (Gemini 3.8 Studio - Estilo JARVIS)"),
    ("Aoede", "🌟 Aoede (Gemini 3.8 Studio - Alegre & Calorosa)"),
    ("Charon", "🧠 Charon (Gemini 3.8 Studio - Calma & Inteligente)")
]

# Estilos emocionais enviados nos metadados do Gemini 3.8 TTS
EMOTION_SPEECH_STYLES = {
    "NEUTRAL": "friendly, lively, charismatic and natural",
    "HAPPY": "cheerful, enthusiastic, playful and smiling",
    "LOVE": "warm, affectionate, sweet and delighted",
    "SURPRISED": "amazed, excited and energetic",
    "WINK": "playful, witty and charming",
    "SAD": "soft, gentle and slightly melancholic",
    "ANGRY": "dramatic, firm and energetic",
    "SLEEPY": "calm, sleepy, soft and slow",
    "FEAR": "nervous, trembling, hesitant and scared",
    "DISGUST": "repulsed, grimacing, dramatic and reluctant",
    "CONTEMPT": "dry, sarcastic, aloof and unimpressed",
    "SHAME": "shy, bashful, embarrassed and soft-spoken",
    "DESPAIR": "dramatic, overwhelmed, distressed and urgent",
    "MALICE": "mischievous, teasing, sly and playful"
}

# Prosódia emocional (velocidade e tom) para o motor Neural Ultra-Rápido (Edge-TTS)
EDGE_EMOTION_PROSODY = {
    "NEUTRAL":   ("+14%", "+5Hz"),
    "HAPPY":     ("+18%", "+9Hz"),
    "LOVE":      ("+12%", "+7Hz"),
    "SURPRISED": ("+20%", "+11Hz"),
    "WINK":      ("+16%", "+7Hz"),
    "SAD":       ("+4%",  "-3Hz"),
    "ANGRY":     ("+18%", "+2Hz"),
    "SLEEPY":    ("-4%",  "-4Hz"),
    "FEAR":      ("+22%", "+8Hz"),
    "DISGUST":   ("+2%",  "-5Hz"),
    "CONTEMPT":  ("+0%",  "-4Hz"),
    "SHAME":     ("-2%",  "+2Hz"),
    "DESPAIR":   ("+24%", "+4Hz"),
    "MALICE":    ("+10%", "-2Hz")
}

# Dimensões lógicas da tela física Estardyn 2.01" TFT SPI (JD9853 em modo Paisagem)
SCREEN_LOGICAL_W = 296
SCREEN_LOGICAL_H = 240
SCREEN_PREVIEW_SCALE = 2
