#include <Arduino.h>

#define RGB_PIN 48

enum RobotState {
    STATE_TEST,
    STATE_IDLE,       // Azul respirando (esperando wake word)
    STATE_LISTEN,     // Verde aceso (ouvindo o usuário)
    STATE_THINK,      // Roxo pulsante (processando IA)
    STATE_SPEAK,      // Ciano oscilante (respondendo com voz)
    STATE_ALERT       // Laranja alerta
};

RobotState currentState = STATE_TEST;
unsigned long lastStateChange = 0;
unsigned long lastAnimUpdate = 0;
int animStep = 0;
bool animUp = true;

void setLED(uint8_t r, uint8_t g, uint8_t b) {
    neopixelWrite(RGB_PIN, r, g, b);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- ROBO ESP32-S3 INICIALIZADO ---");
    Serial.println("Pino RGB: GPIO 48");
    Serial.println("Pronto para receber comandos de estado!");
    
    // Teste inicial rápido: Pisca Vermelho, Verde, Azul
    setLED(50, 0, 0); delay(300);
    setLED(0, 50, 0); delay(300);
    setLED(0, 0, 50); delay(300);
    setLED(0, 0, 0);
    
    currentState = STATE_IDLE;
    lastStateChange = millis();
}

void updateAnimation() {
    unsigned long now = millis();
    if (now - lastAnimUpdate < 20) return;
    lastAnimUpdate = now;

    switch (currentState) {
        case STATE_IDLE: {
            // Efeito respiração suave em Azul (Dormindo / Esperando Wake Word)
            if (animUp) {
                animStep += 1;
                if (animStep >= 50) animUp = false;
            } else {
                animStep -= 1;
                if (animStep <= 4) animUp = true;
            }
            setLED(0, (uint8_t)(animStep / 4), (uint8_t)animStep);
            break;
        }

        case STATE_LISTEN: {
            // Verde claro contínuo (Ouvindo)
            setLED(0, 80, 15);
            break;
        }

        case STATE_THINK: {
            // Roxo pulsando rápido (Pensando)
            if (animUp) {
                animStep += 4;
                if (animStep >= 100) animUp = false;
            } else {
                animStep -= 4;
                if (animStep <= 10) animUp = true;
            }
            setLED((uint8_t)animStep, 0, (uint8_t)(animStep * 1.2));
            break;
        }

        case STATE_SPEAK: {
            // Ciano oscilante dinâmico (Falando)
            int val = 20 + (rand() % 70);
            setLED(0, (uint8_t)val, (uint8_t)(val * 1.2));
            break;
        }

        case STATE_ALERT: {
            // Laranja
            setLED(80, 30, 0);
            break;
        }

        default:
            break;
    }
}

void processCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd == "PING") {
        Serial.println("PONG");
    } else if (cmd == "STATE:IDLE" || cmd == "IDLE") {
        currentState = STATE_IDLE;
        animStep = 5;
        animUp = true;
        Serial.println("OK:STATE=IDLE");
    } else if (cmd == "STATE:LISTEN" || cmd == "LISTEN") {
        currentState = STATE_LISTEN;
        Serial.println("OK:STATE=LISTEN");
    } else if (cmd == "STATE:THINK" || cmd == "THINK") {
        currentState = STATE_THINK;
        animStep = 10;
        animUp = true;
        Serial.println("OK:STATE=THINK");
    } else if (cmd == "STATE:SPEAK" || cmd == "SPEAK") {
        currentState = STATE_SPEAK;
        Serial.println("OK:STATE=SPEAK");
    } else if (cmd == "STATE:ALERT" || cmd == "ALERT") {
        currentState = STATE_ALERT;
        Serial.println("OK:STATE=ALERT");
    } else if (cmd.startsWith("RGB:")) {
        // Ex: RGB:255,100,50
        String vals = cmd.substring(4);
        int c1 = vals.indexOf(',');
        int c2 = vals.lastIndexOf(',');
        if (c1 != -1 && c2 != -1 && c2 > c1) {
            int r = vals.substring(0, c1).toInt();
            int g = vals.substring(c1 + 1, c2).toInt();
            int b = vals.substring(c2 + 1).toInt();
            currentState = STATE_TEST;
            setLED(r, g, b);
            Serial.printf("OK:RGB=%d,%d,%d\n", r, g, b);
        }
    } else {
        Serial.printf("UNKNOWN_CMD:%s\n", cmd.c_str());
    }
}

void loop() {
    updateAnimation();

    if (Serial.available() > 0) {
        String line = Serial.readStringUntil('\n');
        processCommand(line);
    }
}
