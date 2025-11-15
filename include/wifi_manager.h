#pragma once

#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

WebServer server(80);

// --- Funciones de EEPROM ---
String leerStringDeEEPROM(int direccion) {
    String cadena = "";
    char caracter = EEPROM.read(direccion);
    int i = 0;
    while (caracter != '\0' && i < 100) { // Límite de 100 chars
        cadena += caracter;
        i++;
        caracter = EEPROM.read(direccion + i);
    }
    return cadena;
}

void escribirStringEnEEPROM(int direccion, String cadena) {
    int longitudCadena = cadena.length();
    for (int i = 0; i < longitudCadena; i++) {
        EEPROM.write(direccion + i, cadena[i]);
    }
    EEPROM.write(direccion + longitudCadena, '\0'); // Fin de string
    EEPROM.commit();
}

// --- Lógica del Servidor Web (Portal Cautivo) ---

int posW = 50; // Variable para alternar el slot de guardado

void handleRoot() {
    String html = "<html><body>";
    html += "<h2>Configurar WiFi - Estacion Meteorologica</h2>";
    html += "<form method='POST' action='/wifi'>";
    html += "Red (SSID): <input type='text' name='ssid'><br>";
    html += "Clave: <input type='password' name='password'><br><br>";
    html += "<input type='submit' value='Conectar'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
}

void handleWifi() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    Serial.print("Recibido. Intentando conectar a: ");
    Serial.println(ssid);

    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());

    int cnt = 0;
    while (WiFi.status() != WL_CONNECTED && cnt < 20) { // 20 seg de espera
        delay(500);
        Serial.print(".");
        cnt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConexion establecida!");
        Serial.print("Guardando credenciales en EEPROM...");
        
        // Lógica para alternar entre dos slots (0 y 50)
        String varsave = leerStringDeEEPROM(300);
        if (varsave == "a") {
            posW = 0;
            escribirStringEnEEPROM(300, "b");
        } else {
            posW = 50;
            escribirStringEnEEPROM(300, "a");
        }
        escribirStringEnEEPROM(0 + posW, ssid);
        escribirStringEnEEPROM(100 + posW, password);
        Serial.println(" Guardado.");

        server.send(200, "text/plain", "Conexion OK. El ESP32 se reiniciara.");
        delay(1000);
        ESP.restart(); // Forzamos reinicio para que arranque en modo normal
    } else {
        Serial.println("\nConexion Fallida.");
        server.send(200, "text/plain", "Conexion fallida. Revisa los datos.");
    }
}

// --- Funciones de Conexión ---

bool lastRed() {
    Serial.println("Probando redes guardadas...");
    for (int psW = 0; psW <= 50; psW += 50) {
        String usu = leerStringDeEEPROM(0 + psW);
        String cla = leerStringDeEEPROM(100 + psW);

        if (usu.length() > 0) { // Si hay algo guardado
            Serial.print("Probando: "); Serial.println(usu);
            WiFi.disconnect();
            WiFi.begin(usu.c_str(), cla.c_str());
            
            int cnt = 0;
            while (WiFi.status() != WL_CONNECTED && cnt < 10) { // 5 seg
                delay(500);
                Serial.print(".");
                cnt++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\nConectado a Red WiFi!");
                Serial.println(WiFi.localIP());
                return true;
            }
        }
    }
    Serial.println("No se pudo conectar a redes guardadas.");
    return false;
}

void initAP(const char *apSsid, const char *apPassword) {
    Serial.println("Iniciando Modo Access Point (AP)...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, apPassword);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    server.on("/", handleRoot);
    server.on("/wifi", handleWifi);
    server.begin();
    Serial.println("Servidor web iniciado.");
}

// Esta es la función que debe llamarse en el loop() principal
void loopAP() {
    server.handleClient();
}