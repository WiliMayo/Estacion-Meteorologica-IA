/*
 * PROYECTO: ESTACIÓN METEOROLÓGICA INTELIGENTE CON GEMINI AI
 * Autores: Guillermo Mero, Luis Cordova, William Mayorga
 *
 * MODIFICACIÓN: Cambio de pines OLED a 16, 17, 5, 18
*/

// 1. LIBRERÍAS
#include <Arduino.h>
#include <U8g2lib.h>          
#include <Wire.h>             
#include "DHT.h"              
#include <BH1750.h>           
#include "wifi_manager.h"     
#include <HTTPClient.h>       
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h>      
#include "secrets.h"          

struct DecisionIA {
  String recomendacion;
  bool encenderVentilador;
  String colorLed; 
};

DecisionIA ultimaDecision = {"Esperando IA...", false, "VERDE"};

// 2. DEFINICIONES GLOBALES

// --- Sensores ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE); 
BH1750 lightMeter; 
const int MQ135_PIN = 36; 

// --- PANTALLA OLED (NUEVA CONFIGURACIÓN DE PINES) ---
// Configuración solicitada: DC:16, RES:17, SDA:5, SCK:18
#define OLED_DC   16   // Data/Command
#define OLED_RES  17   // Reset
#define OLED_SDA  5    // MOSI / Data
#define OLED_SCK  18   // Clock
#define OLED_CS   U8X8_PIN_NONE 

// Constructor (Software SPI sigue siendo el más flexible para estos pines)
U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, 
    /* clock=*/ OLED_SCK, 
    /* data=*/ OLED_SDA, 
    /* cs=*/ OLED_CS, 
    /* dc=*/ OLED_DC, 
    /* reset=*/ OLED_RES);

// --- Actuadores ---
const int LED_ROJO_PIN = 14; 
const int LED_VERDE_PIN = 12; 
const int LED_AZUL_PIN = 13;  
const int FAN_PIN = 27;       

// --- Configuración de API ---
const String GEMINI_API_URL = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(API_KEY);

const long INTERVALO_SENSORES = 3000;    
const long INTERVALO_GEMINI = 60000; // 60s para evitar error 429
unsigned long tiempoAnteriorSensores = 0;
unsigned long tiempoAnteriorGemini = 0;

// 3. PROTOTIPOS
void aplicarDecisionIA(DecisionIA decision);
void actualizarOLED_Sensores(float t, float h, float lux, int airQuality);
void actualizarOLED_AP_Mode();
DecisionIA llamarAGemini(float t, float h, float lux, int airQuality);
void actualizarOLED_RespuestaIA(String texto);

// 4. SETUP
void setup() {
  Serial.begin(115200);
  
  Wire.begin(21, 22); 
  dht.begin();
  
  // Intento robusto BH1750
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23)) {
     if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C)) {
        Serial.println(F("[ERROR] BH1750 no encontrado."));
     }
  }
  
  u8g2.begin(); // Inicia OLED con los nuevos pines

  pinMode(LED_ROJO_PIN, OUTPUT);
  pinMode(LED_VERDE_PIN, OUTPUT);
  pinMode(LED_AZUL_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  
  digitalWrite(LED_ROJO_PIN, LOW);
  digitalWrite(LED_VERDE_PIN, LOW);
  digitalWrite(LED_AZUL_PIN, LOW);
  digitalWrite(FAN_PIN, LOW); 

  EEPROM.begin(512); 

  if (!lastRed()) { 
      initAP("EstacionMeteo-Config", "12345678"); 
  } else {
      Serial.println("WiFi Conectado.");
  }
}

// 5. LOOP PRINCIPAL
void loop() {
  unsigned long tiempoActual = millis();

  if (WiFi.status() != WL_CONNECTED) {
    loopAP(); 
    actualizarOLED_AP_Mode(); 
    digitalWrite(FAN_PIN, LOW);
    digitalWrite(LED_ROJO_PIN, LOW);
    digitalWrite(LED_VERDE_PIN, LOW);
    digitalWrite(LED_AZUL_PIN, LOW);
  } else {
    
    // Tarea 1: Sensores
    if (tiempoActual - tiempoAnteriorSensores >= INTERVALO_SENSORES) {
      tiempoAnteriorSensores = tiempoActual;

      float t = dht.readTemperature();
      float h = dht.readHumidity();
      int airQuality = analogRead(MQ135_PIN);
      float lux = lightMeter.readLightLevel();

      if (isnan(t)) t = 0.0; if (isnan(h)) h = 0.0; if (lux < 0) lux = 0.0; 

      aplicarDecisionIA(ultimaDecision);
      actualizarOLED_Sensores(t, h, lux, airQuality);
    }

    // Tarea 2: Gemini
    if (tiempoActual - tiempoAnteriorGemini >= INTERVALO_GEMINI) {
      tiempoAnteriorGemini = tiempoActual;

      Serial.println("\n[GEMINI] Consultando a la IA...");
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_profont12_tr);
      u8g2.drawStr(20, 32, "IA Pensando...");
      u8g2.sendBuffer();

      float t = dht.readTemperature();
      float h = dht.readHumidity();
      int airQuality = analogRead(MQ135_PIN);
      float lux = lightMeter.readLightLevel();
      if (isnan(t)) t = 0.0; if (isnan(h)) h = 0.0; if (lux < 0) lux = 0.0; 

      ultimaDecision = llamarAGemini(t, h, lux, airQuality);
      
      actualizarOLED_RespuestaIA(ultimaDecision.recomendacion);
      aplicarDecisionIA(ultimaDecision);

      delay(4000); 
    }
  }
}

// --- FUNCIONES AUXILIARES ---

void aplicarDecisionIA(DecisionIA decision) {
  if (decision.encenderVentilador) digitalWrite(FAN_PIN, HIGH); 
  else digitalWrite(FAN_PIN, LOW);

  digitalWrite(LED_ROJO_PIN, LOW);
  digitalWrite(LED_VERDE_PIN, LOW);
  digitalWrite(LED_AZUL_PIN, LOW);

  if (decision.colorLed == "ROJO") digitalWrite(LED_ROJO_PIN, HIGH);
  else if (decision.colorLed == "AZUL") digitalWrite(LED_AZUL_PIN, HIGH);
  else digitalWrite(LED_VERDE_PIN, HIGH); 
}

DecisionIA llamarAGemini(float t, float h, float lux, int airQuality) {
  DecisionIA resultado = {"Error Conexión", false, "VERDE"};

  String prompt = "Actúa como controlador IoT. Datos: Temp=" + String(t, 1) + "C, Hum=" + String(h, 0) + "%, Luz=" + String(lux, 0) + ", Aire=" + String(airQuality) + ". "
                  "Reglas: >29C o Aire > 300 = fan ON. >29C=ROJO, <19C=AZUL, resto=VERDE. "
                  "Responde SOLO JSON: { \"mensaje\": \"texto corto\", \"fan\": true, \"led\": \"ROJO\" }";

  JsonDocument jsonReq; 
  jsonReq["contents"][0]["parts"][0]["text"] = prompt;
  String payload;
  serializeJson(jsonReq, payload);

  WiFiClientSecure client;
  client.setInsecure(); 
  
  HTTPClient http;
  http.begin(client, GEMINI_API_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000); 

  int httpCode = http.POST(payload);
  Serial.printf("[HTTP] Código: %d\n", httpCode);

  if (httpCode == 200) {
    String respuestaRaw = http.getString();
    JsonDocument docGoogle;
    DeserializationError error = deserializeJson(docGoogle, respuestaRaw);

    if (!error) {
      String textoIA = docGoogle["candidates"][0]["content"]["parts"][0]["text"];
      textoIA.replace("```json", "");
      textoIA.replace("```", "");
      
      JsonDocument docIA;
      DeserializationError errorIA = deserializeJson(docIA, textoIA);
      
      if (!errorIA) {
        resultado.recomendacion = docIA["mensaje"].as<String>();
        resultado.encenderVentilador = docIA["fan"].as<bool>();
        resultado.colorLed = docIA["led"].as<String>();
        Serial.println("IA Orden -> Fan: " + String(resultado.encenderVentilador) + " | LED: " + resultado.colorLed);
      } else {
        Serial.println("Error JSON interno IA.");
        resultado.recomendacion = "Error Formato IA";
      }
    }
  } else {
    Serial.printf("Error HTTP: %s\n", http.errorToString(httpCode).c_str());
    if (httpCode == 429) {
        resultado.recomendacion = "Espera 1 min...";
        Serial.println("Limite de API alcanzado.");
    }
  }

  http.end();
  return resultado;
}

void actualizarOLED_RespuestaIA(String texto) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont11_tr); 
  u8g2.drawStr(0, 10, "GEMINI DICE:");
  
  int maxChars = 21; 
  String l1 = texto.substring(0, maxChars);
  String l2 = "";
  if (texto.length() > maxChars) l2 = texto.substring(maxChars, maxChars*2);

  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(0, 26, l1.c_str());
  u8g2.drawStr(0, 42, l2.c_str());
  u8g2.sendBuffer();
}

void actualizarOLED_Sensores(float t, float h, float lux, int airQuality) {
  char buffer[32]; 
  u8g2.clearBuffer(); 
  u8g2.setFont(u8g2_font_profont11_tr); 
  u8g2.drawStr(0, 10, "EST. METEOROLOGICA");
  u8g2.setFont(u8g2_font_profont12_tr);
  
  sprintf(buffer, "T: %.1f C", t); u8g2.drawStr(0, 26, buffer);
  sprintf(buffer, "H: %.0f %%", h); u8g2.drawStr(70, 26, buffer);
  sprintf(buffer, "Luz: %.0f", lux); u8g2.drawStr(0, 42, buffer);
  sprintf(buffer, "Aire: %d", airQuality); u8g2.drawStr(0, 58, buffer);
  u8g2.sendBuffer(); 
}

void actualizarOLED_AP_Mode() {
    u8g2.clearBuffer(); 
    u8g2.setFont(u8g2_font_profont11_tr); 
    u8g2.drawStr(0, 10, "MODO CONFIGURACION");
    u8g2.setFont(u8g2_font_profont12_tr);
    u8g2.drawStr(0, 26, "1. Conectate a:");
    u8g2.drawStr(0, 42, "   'EstacionMeteo-Config'");
    u8g2.drawStr(0, 58, "2. Ve a 192.168.4.1");
    u8g2.sendBuffer(); 
}