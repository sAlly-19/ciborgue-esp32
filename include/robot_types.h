#ifndef CIBORGUE_ROBOT_TYPES_H
#define CIBORGUE_ROBOT_TYPES_H

#include <Arduino.h>

enum RobotState {
    STATE_BOOT,
    STATE_IDLE,     // Dormindo / Esperando wake word (Azul respirando)
    STATE_LISTEN,   // Ouvindo o usuário (Verde)
    STATE_THINK,    // Pensando / Consultando Gemini (Roxo pulsante)
    STATE_SPEAK,    // Respondendo em voz alta (Ciano oscilante)
    STATE_ALERT,    // Alerta / Timer disparado (Laranja piscante)
    STATE_CUSTOM    // Cor/Efeito customizado escolhido pelo usuário
};

enum FaceEmotion {
    EMO_NEUTRAL,    // Olhos normais atentos
    EMO_HAPPY,      // Olhos arqueados felizes (^^)
    EMO_SAD,        // Olhos caídos tristes
    EMO_ANGRY,      // Sobrancelhas franzidas bravas
    EMO_SURPRISED,  // Olhos bem arregalados (!)
    EMO_WINK,       // Piscadinha amigável (;D)
    EMO_LOVE,       // Olhos apaixonados / vibrantes
    EMO_SLEEPY      // Olhos semicerrados de sono (-_-)
};

enum LedEffectMode {
    LED_MODE_STATE,     // Segue automaticamente o RobotState atual
    LED_MODE_SOLID,     // Cor fixa customizada
    LED_MODE_PULSE,     // Cor customizada pulsando suave
    LED_MODE_RAINBOW,   // Arco-íris dinâmico
    LED_MODE_ALERT      // Pisca rápido de alerta
};

struct RobotTimer {
    bool active = false;
    unsigned long targetMillis = 0;
    unsigned long durationSec = 0;
    String label = "";
};

inline const char* stateToString(RobotState s) {
    switch (s) {
        case STATE_BOOT:   return "BOOT";
        case STATE_IDLE:   return "IDLE";
        case STATE_LISTEN: return "LISTEN";
        case STATE_THINK:  return "THINK";
        case STATE_SPEAK:  return "SPEAK";
        case STATE_ALERT:  return "ALERT";
        case STATE_CUSTOM: return "CUSTOM";
        default:           return "IDLE";
    }
}

inline RobotState stringToState(String s) {
    s.toUpperCase();
    s.trim();
    if (s == "LISTEN") return STATE_LISTEN;
    if (s == "THINK")  return STATE_THINK;
    if (s == "SPEAK")  return STATE_SPEAK;
    if (s == "ALERT")  return STATE_ALERT;
    if (s == "CUSTOM") return STATE_CUSTOM;
    return STATE_IDLE;
}

inline const char* emotionToString(FaceEmotion e) {
    switch (e) {
        case EMO_NEUTRAL:   return "NEUTRAL";
        case EMO_HAPPY:     return "HAPPY";
        case EMO_SAD:       return "SAD";
        case EMO_ANGRY:     return "ANGRY";
        case EMO_SURPRISED: return "SURPRISED";
        case EMO_WINK:      return "WINK";
        case EMO_LOVE:      return "LOVE";
        case EMO_SLEEPY:    return "SLEEPY";
        default:            return "NEUTRAL";
    }
}

inline FaceEmotion stringToEmotion(String e) {
    e.toLowerCase();
    e.trim();
    if (e == "happy" || e == "feliz" || e == "alegre") return EMO_HAPPY;
    if (e == "sad" || e == "triste") return EMO_SAD;
    if (e == "angry" || e == "bravo" || e == "irritado" || e == "raiva") return EMO_ANGRY;
    if (e == "surprised" || e == "surpreso" || e == "assustado" || e == "espantado") return EMO_SURPRISED;
    if (e == "wink" || e == "piscadinha" || e == "piscar") return EMO_WINK;
    if (e == "love" || e == "apaixonado" || e == "amor" || e == "coracao") return EMO_LOVE;
    if (e == "sleepy" || e == "sonolento" || e == "sono" || e == "cansado") return EMO_SLEEPY;
    return EMO_NEUTRAL;
}

#endif // CIBORGUE_ROBOT_TYPES_H
