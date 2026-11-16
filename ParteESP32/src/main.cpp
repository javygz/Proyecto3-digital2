#define CUSTOM_SETTINGS
#define INCLUDE_TERMINAL_MODULE

#include <Arduino.h>
#include <DabbleESP32.h>


#define RX2_PIN 16
#define TX2_PIN 17
#define TIMEOUT_MS 2500   // tiempo maximo de espera de la respuesta

String lineaNucleo = "";
bool esperando = false;
unsigned long tEnvio = 0;

// Envia un comando a la Nucleo 1 (termina en salto de linea) y avisa en la app.
void enviarANucleo(const char *cmd, const char *aviso) {
    Serial2.print(cmd);
    Serial2.print('\n');
    esperando = true;
    tEnvio = millis();

    Serial.print("-> Nucleo 1: ");
    Serial.println(cmd);
    Terminal.println(aviso);
}

// Devuelve el campo n (separado por comas) de una linea.
// Ejemplo: campo("DATA,36.8,55.2", 1) -> "36.8"
String campo(const String &s, int n) {
    int ini = 0;
    for (int i = 0; i < n; i++) {
        ini = s.indexOf(',', ini);
        if (ini < 0) return "";
        ini++;
    }
    int fin = s.indexOf(',', ini);
    return (fin < 0) ? s.substring(ini) : s.substring(ini, fin);
}

// Interpreta la respuesta que llega de la Nucleo 1 y la muestra en la Terminal de Dabble.
void procesarRespuesta(String r) {
    r.trim();
    if (r.length() == 0) {
        return;
    }
    esperando = false;
    Serial.println("<- Nucleo 1: " + r);

    String tipo = campo(r, 0);

    if (tipo == "DATA") {
        Terminal.println("Temp: " + campo(r, 1) + " C   Hum: " + campo(r, 2) + " %");
    } else if (tipo == "OK") {
        Terminal.println("Guardado en SD: " + campo(r, 2) + " C, " + campo(r, 3) + " %");
    } else if (r == "ERR,SD") {
        Terminal.println("ERROR: revisa la tarjeta SD");
    } else if (r == "ERR,NODATA") {
        Terminal.println("Aun no hay medicion");
    } else {
        Terminal.println("Respuesta: " + r);
    }
}

// Lee lo que manda la Nucleo 1, linea por linea, sin bloquear.
void recibirNucleo() {
    while (Serial2.available() > 0) {
        char c = Serial2.read();
        if (c == '\n') {
            procesarRespuesta(lineaNucleo);
            lineaNucleo = "";
        } else if (c != '\r' && lineaNucleo.length() < 60) {
            lineaNucleo += c;
        }
    }

    if (esperando && (millis() - tEnvio) > TIMEOUT_MS) {
        esperando = false;
        Terminal.println("Sin respuesta de la Nucleo");
        Serial.println("Timeout esperando a la Nucleo 1");
    }
}

// Procesa un mensaje completo de la Terminal de Dabble.
//   L -> pide una lectura  (READ)
//   G -> guarda en la SD   (SAVE)
void procesarComando(String comando) {
    comando.trim();
    comando.toUpperCase();

    if (comando.length() == 0) {
        return;
    }

    if (comando == "L") {
        Serial.println("Solicitud de lectura recibida.");
        enviarANucleo("READ", "Leyendo...");
    } else if (comando == "G") {
        Serial.println("Solicitud de guardado recibida.");
        enviarANucleo("SAVE", "Guardando...");
    } else {
        Serial.println("Comando desconocido.");
        Terminal.println("ERROR: envia L o G");
    }
}

// Lee el mensaje que Dabble ya recibio y decodifico.
void recibirBluetooth() {
    if (Terminal.available() > 0) {
        String comando = Terminal.readString();
        procesarComando(comando);
    }
}

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, RX2_PIN, TX2_PIN);
    Dabble.begin("Licht_ESP32");

    Serial.println("ESP32 lista.");
    Serial.println("Envia L o G.");
}

void loop() {
    Dabble.processInput();
    recibirBluetooth();
    recibirNucleo();
    delay(5);
}
