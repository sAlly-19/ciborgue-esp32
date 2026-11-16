#ifndef CIBORGUE_POMODORO_MANAGER_H
#define CIBORGUE_POMODORO_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "robot_types.h"
#include "led_controller.h"
#include "display_hal.h"

enum PomodoroPhase {
    POMO_INACTIVE = 0,
    POMO_FOCUS,
    POMO_BREAK
};

class PomodoroManager {
private:
    LedController& led;
    DisplayHAL& display;
    volatile RobotState& currentState;
    FaceEmotion& currentEmotion;

    PomodoroPhase phase = POMO_INACTIVE;
    unsigned long startMillis = 0;
    unsigned long targetMillis = 0;
    int focusDurationSec = 25 * 60;
    int breakDurationSec = 5 * 60;
    String goalLabel = "Foco Total";
    unsigned long lastTickEmitMillis = 0;

public:
    PomodoroManager(LedController& l, DisplayHAL& d, volatile RobotState& st, FaceEmotion& emo)
        : led(l), display(d), currentState(st), currentEmotion(emo) {}

    bool isActive() const { return phase != POMO_INACTIVE; }
    PomodoroPhase getPhase() const { return phase; }
    String getGoal() const { return goalLabel; }

    int getRemainingSec() const {
        if (phase == POMO_INACTIVE || targetMillis <= millis()) return 0;
        return (int)((targetMillis - millis()) / 1000UL);
    }

    int getTotalPhaseSec() const {
        if (phase == POMO_FOCUS) return focusDurationSec;
        if (phase == POMO_BREAK) return breakDurationSec;
        return 0;
    }

    String start(int focusMinutes, int breakMinutes, const String& goal) {
        if (focusMinutes <= 0) focusMinutes = 25;
        if (focusMinutes > 180) focusMinutes = 180;
        if (breakMinutes <= 0) breakMinutes = 5;
        if (breakMinutes > 60) breakMinutes = 60;

        focusDurationSec = focusMinutes * 60;
        breakDurationSec = breakMinutes * 60;
        goalLabel = goal.length() > 0 ? goal : "Sessão de Foco";
        phase = POMO_FOCUS;
        startMillis = millis();
        targetMillis = startMillis + ((unsigned long)focusDurationSec * 1000UL);

        // Olhar concentrado e LED azul ciano suave
        currentEmotion = EMO_NEUTRAL;
        display.setEmotion(EMO_NEUTRAL);
        led.setCustomColor(0, 140, 255, LED_MODE_PULSE);
        Serial.println("EVT:EMOTION=NEUTRAL");
        emitStatusPacket(true);

        return String("Modo Pomodoro iniciado! ") + String(focusMinutes) + " minutos de foco em '" +
               goalLabel + "' seguidos de " + String(breakMinutes) + " minutos de descanso.";
    }

    String stop() {
        if (phase == POMO_INACTIVE) {
            return "O modo Pomodoro já está desligado.";
        }
        phase = POMO_INACTIVE;
        led.setStateMode();
        emitStatusPacket(false);
        return "Modo Pomodoro encerrado.";
    }

    void emitStatusPacket(bool showWidgetCard = false) {
        JsonDocument doc;
        doc["active"] = (phase != POMO_INACTIVE);
        doc["phase"] = (phase == POMO_FOCUS) ? "FOCUS" : ((phase == POMO_BREAK) ? "BREAK" : "INACTIVE");
        doc["rem_s"] = getRemainingSec();
        doc["total_s"] = getTotalPhaseSec();
        doc["goal"] = goalLabel;
        doc["show_card"] = showWidgetCard;
        String out;
        serializeJson(doc, out);
        Serial.printf("EVT:POMODORO=%s\n", out.c_str());
    }

    void update() {
        if (phase == POMO_INACTIVE) return;

        unsigned long now = millis();
        if (now - lastTickEmitMillis >= 1000) {
            lastTickEmitMillis = now;
            emitStatusPacket(false);
        }

        if (now >= targetMillis) {
            if (phase == POMO_FOCUS) {
                // Concluiu o Foco -> Entra na Pausa Comemorando!
                phase = POMO_BREAK;
                startMillis = now;
                targetMillis = now + ((unsigned long)breakDurationSec * 1000UL);
                currentEmotion = EMO_HAPPY;
                display.setEmotion(EMO_HAPPY);
                led.setCustomColor(0, 255, 140, LED_MODE_RAINBOW);
                Serial.println("EVT:EMOTION=HAPPY");
                emitStatusPacket(true);

                int breakMin = max(1, breakDurationSec / 60);
                String msg = String("Parabéns! Você concluiu o foco em ") + goalLabel +
                             "! Hora de esticar o corpo, beber água e descansar por " + String(breakMin) + " minutos!";
                display.setSubtitle(msg);
                Serial.printf("EVT:SAY=%s\n", msg.c_str());
            } else if (phase == POMO_BREAK) {
                // Concluiu a Pausa -> Volta ao IDLE
                phase = POMO_INACTIVE;
                currentEmotion = EMO_WINK;
                display.setEmotion(EMO_WINK);
                led.setStateMode();
                Serial.println("EVT:EMOTION=WINK");
                emitStatusPacket(true);

                String msg = "Pausa do Pomodoro concluída! Baterias recarregadas, estou pronto para o próximo round!";
                display.setSubtitle(msg);
                Serial.printf("EVT:SAY=%s\n", msg.c_str());
            }
        }
    }
};

#endif // CIBORGUE_POMODORO_MANAGER_H
