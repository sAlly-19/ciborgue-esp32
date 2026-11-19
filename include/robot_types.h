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
    STATE_CUSTOM,   // Cor/Efeito customizado escolhido pelo usuário
    STATE_PORTAL    // Portal Cativo Wi-Fi aberto (Ciborgue-Setup)
};

enum NetMode {
    NET_OFFLINE = 0,
    NET_PORTAL_AP,
    NET_WIFI_ONLY,
    NET_ONLINE
};

enum PowerStage {
    PWR_AWAKE = 0,
    PWR_ECO_SLEEPY,
    PWR_LIGHT_SLEEP
};

enum SfxKind {
    SFX_BOOT = 0,
    SFX_WAKE,
    SFX_HAPPY,
    SFX_LOVE,
    SFX_SAD,
    SFX_ANGRY,
    SFX_SURPRISED,
    SFX_WINK,
    SFX_THINK,
    SFX_ALERT,
    SFX_POMO_FOCUS,
    SFX_POMO_BREAK,
    SFX_SLEEP,
    SFX_OFFLINE,
    SFX_ONLINE,
    SFX_IDLE_CHIRP,
    SFX_FEAR,
    SFX_DISGUST,
    SFX_CONTEMPT,
    SFX_SHAME,
    SFX_DESPAIR,
    SFX_MALICE
};

enum FaceEmotion {
    EMO_NEUTRAL,    // Olhos normais atentos
    EMO_HAPPY,      // Olhos arqueados felizes (^^)
    EMO_SAD,        // Olhos caídos tristes
    EMO_ANGRY,      // Sobrancelhas franzidas bravas
    EMO_SURPRISED,  // Olhos bem arregalados (!)
    EMO_WINK,       // Piscadinha amigável (;D)
    EMO_LOVE,       // Olhos apaixonados / vibrantes
    EMO_SLEEPY,     // Olhos semicerrados de sono (-_-)
    EMO_FEAR,       // Olhos arregalados trêmulos de medo
    EMO_DISGUST,    // Olhos assimétricos espremidos de nojo
    EMO_CONTEMPT,   // Olhar lateral e pálpebra caída de desprezo
    EMO_SHAME,      // Olhos encabulados + rubor de vergonha
    EMO_DESPAIR,    // Espirais de agonia e lágrimas de desespero
    EMO_MALICE      // Meia-lua inclinada e sorriso de malícia
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
        case STATE_PORTAL: return "PORTAL";
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
    if (s == "PORTAL") return STATE_PORTAL;
    return STATE_IDLE;
}

inline const char* netModeToString(NetMode m) {
    switch (m) {
        case NET_PORTAL_AP: return "PORTAL_AP";
        case NET_WIFI_ONLY: return "WIFI_ONLY";
        case NET_ONLINE:    return "ONLINE";
        default:            return "OFFLINE";
    }
}

inline const char* powerStageToString(PowerStage p) {
    switch (p) {
        case PWR_ECO_SLEEPY:  return "ECO";
        case PWR_LIGHT_SLEEP: return "LIGHT_SLEEP";
        default:              return "AWAKE";
    }
}

inline const char* sfxKindToString(SfxKind k) {
    switch (k) {
        case SFX_BOOT:       return "boot";
        case SFX_WAKE:       return "wake";
        case SFX_HAPPY:      return "happy";
        case SFX_LOVE:       return "love";
        case SFX_SAD:        return "sad";
        case SFX_ANGRY:      return "angry";
        case SFX_SURPRISED:  return "surprised";
        case SFX_WINK:       return "wink";
        case SFX_THINK:      return "think";
        case SFX_ALERT:      return "alert";
        case SFX_POMO_FOCUS: return "pomo_focus";
        case SFX_POMO_BREAK: return "pomo_break";
        case SFX_SLEEP:      return "sleep";
        case SFX_OFFLINE:    return "offline";
        case SFX_ONLINE:     return "online";
        case SFX_IDLE_CHIRP: return "idle_chirp";
        case SFX_FEAR:       return "fear";
        case SFX_DISGUST:    return "disgust";
        case SFX_CONTEMPT:   return "contempt";
        case SFX_SHAME:      return "shame";
        case SFX_DESPAIR:    return "despair";
        case SFX_MALICE:     return "malice";
        default:             return "wake";
    }
}

inline SfxKind stringToSfxKind(String s) {
    s.toLowerCase();
    s.trim();
    if (s == "boot") return SFX_BOOT;
    if (s == "happy" || s == "feliz") return SFX_HAPPY;
    if (s == "love" || s == "carinho" || s == "apaixonado") return SFX_LOVE;
    if (s == "sad" || s == "triste") return SFX_SAD;
    if (s == "angry" || s == "bravo") return SFX_ANGRY;
    if (s == "surprised" || s == "surpreso") return SFX_SURPRISED;
    if (s == "wink" || s == "piscadinha") return SFX_WINK;
    if (s == "think" || s == "pensando") return SFX_THINK;
    if (s == "alert" || s == "alerta") return SFX_ALERT;
    if (s == "pomo_focus" || s == "foco") return SFX_POMO_FOCUS;
    if (s == "pomo_break" || s == "pausa") return SFX_POMO_BREAK;
    if (s == "sleep" || s == "dormir") return SFX_SLEEP;
    if (s == "offline") return SFX_OFFLINE;
    if (s == "online") return SFX_ONLINE;
    if (s == "idle" || s == "idle_chirp" || s == "chirp") return SFX_IDLE_CHIRP;
    if (s == "fear" || s == "medo") return SFX_FEAR;
    if (s == "disgust" || s == "nojo") return SFX_DISGUST;
    if (s == "contempt" || s == "desprezo") return SFX_CONTEMPT;
    if (s == "shame" || s == "vergonha") return SFX_SHAME;
    if (s == "despair" || s == "desespero") return SFX_DESPAIR;
    if (s == "malice" || s == "malicia" || s == "malícia") return SFX_MALICE;
    return SFX_WAKE;
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
        case EMO_FEAR:      return "FEAR";
        case EMO_DISGUST:   return "DISGUST";
        case EMO_CONTEMPT:  return "CONTEMPT";
        case EMO_SHAME:     return "SHAME";
        case EMO_DESPAIR:   return "DESPAIR";
        case EMO_MALICE:    return "MALICE";
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
    if (e == "fear" || e == "medo" || e == "apavorado" || e == "amedrontado" || e == "pavor" || e == "susto") return EMO_FEAR;
    if (e == "disgust" || e == "nojo" || e == "nojento" || e == "eca" || e == "repulsa" || e == "enojado") return EMO_DISGUST;
    if (e == "contempt" || e == "desprezo" || e == "desdem" || e == "desdém" || e == "desdenhoso" || e == "esnobe") return EMO_CONTEMPT;
    if (e == "shame" || e == "vergonha" || e == "envergonhado" || e == "envergonhada" || e == "timido" || e == "tímido" || e == "shy" || e == "corado") return EMO_SHAME;
    if (e == "despair" || e == "desespero" || e == "desesperado" || e == "desesperada" || e == "panico" || e == "pânico" || e == "aflito") return EMO_DESPAIR;
    if (e == "malice" || e == "malicia" || e == "malícia" || e == "malicioso" || e == "maliciosa" || e == "travesso" || e == "sapeca" || e == "mischief") return EMO_MALICE;
    return EMO_NEUTRAL;
}

enum DisplayWidgetType {
    WIDGET_NONE = 0,
    WIDGET_CLOCK,
    WIDGET_WEATHER,
    WIDGET_TASKS,
    WIDGET_POMODORO,
    WIDGET_TIMER,
    WIDGET_DESK_CLOCK,
    WIDGET_MOOD,
    WIDGET_ALARM_RING,
    WIDGET_PC_CONTROL
};

struct AlarmItem {
    uint8_t id = 0;
    uint8_t hour = 7;
    uint8_t minute = 0;
    uint8_t daysMask = 0x7F; // bit 0=Dom .. bit 6=Sab; 0 = alarme único
    bool enabled = true;
    String label = "";
};

struct PetState {
    int8_t affection = 65;   // 0..100
    int8_t energy = 85;      // 0..100
    int8_t mood = 15;        // -50..+50
    uint32_t petCount = 0;
    uint32_t chatCount = 0;
};

struct WeatherCache {
    bool valid = false;
    String city = "";
    float temp = 0.0f;
    float feels = 0.0f;
    int hum = 0;
    int code = 0;
    String desc = "";
};

#endif // CIBORGUE_ROBOT_TYPES_H
