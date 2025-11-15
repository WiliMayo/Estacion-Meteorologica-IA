/*
 * PROYECTO: ESTACIÓN METEOROLÓGICA INTELIGENTE CON GEMINI AI
 * Autores: Guillermo Mero, Luis Cordova, William Mayorga
 *
 * Integración de WiFi Manager, Sensores, Actuadores y API de Google Gemini
*/

// 1. LIBRERÍAS
#include <Arduino.h>
#include <U8g2lib.h>          // Para OLED
#include <Wire.h>             // Para I2C (BH1750)
#include "DHT.h"              // Para Temp/Humedad
#include <BH1750.h>           // Para Sensor de Luz
#include "wifi_manager.h"     // Nuestro administrador de WiFi
#include <HTTPClient.h>       // Para hacer llamadas a la API
#include <ArduinoJson.h>      // Para construir y leer datos de la API
#include "secrets.h"          // Nuestro archivo con la clave de API

// 2. DEFINICIONES GLOBALES

// --- Sensores ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE); 
BH1750 lightMeter; 
const int MQ135_PIN = 36; // Pin analógico (VP / SVP)

// --- Pantalla (SPI de 4 hilos) ---
#define OLED_SCK  18
#define OLED_SDA  23
#define OLED_DC   19
#define OLED_RES  5
#define OLED_CS   U8X8_PIN_NONE 
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
const int FAN_PIN = 27;       // Pin que va a la resistencia del TIP122

// --- Umbrales de Temperatura ---
const float TEMP_CALIDA = 30.0;
const float TEMP_FRIA = 18.0;

// --- Configuración de API y Timers ---
// URL de la API de Gemini (modelo gemini-pro)
const String GEMINI_API_URL = "https://generative-ai.googleapis.com/v1beta/models/gemini-pro:generateContent?key=" + String(API_KEY);
const long INTERVALO_SENSORES = 3000;    // Lee sensores y actúa cada 3 seg
const long INTERVALO_GEMINI = 30000;   // Llama a Gemini cada 30 seg
unsigned long tiempoAnteriorSensores = 0;
unsigned long tiempoAnteriorGemini = 0;

// 3. PROTOTIPOS DE FUNCIONES
void controlarActuadores(float temperatura);
void actualizarOLED_Sensores(float t, float h, float lux, int airQuality);
void imprimirSerial(float t, float h, float lux, int airQuality);
void actualizarOLED_AP_Mode();
String llamarAGemini(float t, float h, float lux, int airQuality);
void actualizarOLED_RespuestaIA(String texto);

// 4. SETUP
void setup() {
  Serial.begin(115200);
  Serial.println("--- Estacion Meteorologica Inteligente con Gemini ---");

  // Inicia I2C (SDA=21, SCL=22) para el BH1750
  Wire.begin(21, 22); 
  
  // Inicia Sensores
  dht.begin();
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 físico OK!"));
  } else {
    Serial.println(F("Error al iniciar BH1750"));
  }
  
  // Inicia Pantalla OLED
  u8g2.begin();

  // Configurar Actuadores como SALIDA
  pinMode(LED_ROJO_PIN, OUTPUT);
  pinMode(LED_VERDE_PIN, OUTPUT);
  pinMode(LED_AZUL_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  
  // Apagar todo al iniciar
  digitalWrite(LED_ROJO_PIN, LOW);
  digitalWrite(LED_VERDE_PIN, LOW);
  digitalWrite(LED_AZUL_PIN, LOW);
  digitalWrite(FAN_PIN, LOW); // Lógica Active HIGH (LOW = Apagado)

  // --- INICIO DE LA LÓGICA WIFI ---
  EEPROM.begin(512); // Inicializa EEPROM (512 bytes)

  if (!lastRed()) { // Intenta conectar con redes guardadas
      // Si falla, inicia el modo Access Point
      initAP("EstacionMeteo-Config", "12345678"); 
  } else {
      Serial.println("Conexion WiFi exitosa en el arranque.");
  }
}

// 5. LOOP PRINCIPAL
void loop() {
  unsigned long tiempoActual = millis();

  // --- MODO 1: CONFIGURACIÓN WIFI ---
  // Si se pierde la conexión o no se conectó al inicio...
  if (WiFi.status() != WL_CONNECTED) {
    loopAP(); // Ejecuta el servidor web del portal cautivo
    actualizarOLED_AP_Mode(); // Muestra instrucciones en OLED
    
    // Apaga actuadores por seguridad
    digitalWrite(FAN_PIN, LOW);
    digitalWrite(LED_ROJO_PIN, LOW);
    digitalWrite(LED_VERDE_PIN, LOW);
    digitalWrite(LED_AZUL_PIN, LOW);

  // --- MODO 2: OPERACIÓN NORMAL (CONECTADO) ---
  } else {
    
    // Tarea 1: Leer Sensores y Actuar (Cada 3 segundos)
    if (tiempoActual - tiempoAnteriorSensores >= INTERVALO_SENSORES) {
      tiempoAnteriorSensores = tiempoActual;

      // Realizar lecturas
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      int airQuality = analogRead(MQ135_PIN);
      float lux = lightMeter.readLightLevel();

      // Validar lecturas
      if (isnan(t)) t = 0.0;
      if (isnan(h)) h = 0.0;
      if (lux < 0) lux = 0.0; 

      // Lógica de control físico (¡respuesta rápida!)
      controlarActuadores(t);

      // Mostrar pantalla de sensores
      actualizarOLED_Sensores(t, h, lux, airQuality);
      
      //imprimirSerial(t, h, lux, airQuality); // Descomentar para debug
    }

    // Tarea 2: Llamar a Gemini (Cada 30 segundos)
    if (tiempoActual - tiempoAnteriorGemini >= INTERVALO_GEMINI) {
      tiempoAnteriorGemini = tiempoActual;

      Serial.println("\n[GEMINI] Solicitando análisis de IA...");

      // Obtenemos los datos más recientes
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      int airQuality = analogRead(MQ135_PIN);
      float lux = lightMeter.readLightLevel();

      if (isnan(t)) t = 0.0; if (isnan(h)) h = 0.0; if (lux < 0) lux = 0.0; 

      // Pantalla en modo "Cargando"
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_profont12_tr);
      u8g2.drawStr(20, 32, "Analizando...");
      u8g2.sendBuffer();

      // Llamar a la API (¡esto es lento!)
      String respuestaIA = llamarAGemini(t, h, lux, airQuality);
      
      Serial.println("[GEMINI] Respuesta recibida:");
      Serial.println(respuestaIA);

      // Mostrar respuesta en la OLED
      actualizarOLED_RespuestaIA(respuestaIA);
      
      // Esperar 5 segundos mostrando la respuesta
      delay(5000);
      
      // El loop volverá a la pantalla de sensores en el próximo ciclo de 3 seg
    }
  }
}

// --- Implementación de Funciones Auxiliares ---

/**
 * Llama a la API de Gemini y retorna la respuesta.
 * --- ¡VERSIÓN ACTUALIZADA CON SINTAXIS ArduinoJson v7! ---
 */
String llamarAGemini(float t, float h, float lux, int airQuality) {
  // 1. Crear el Prompt (La pregunta a la IA)
  String prompt = "Eres un asistente de estación meteorológica. Los datos actuales son: " + 
                  String(t, 1) + "°C, " + String(h, 0) + "% de humedad, " + String(lux, 0) + 
                  " lux, y calidad de aire (raw) de " + String(airQuality) + 
                  ". Dame una recomendación corta (máx 15 palabras) para el usuario. " +
                  "Ejemplo: 'Cálido y seco. Bebe agua.'";

  // 2. Crear el cuerpo JSON de la solicitud (¡Sintaxis v7!)
  // JsonDocument (en lugar de StaticJsonDocument) se usa en v7
  JsonDocument jsonReq; 
  
  // La nueva sintaxis es más limpia:
  JsonObject contents = jsonReq["contents"].to<JsonObject>();
  JsonArray parts = contents["parts"].to<JsonArray>();
  JsonObject textPart = parts.add<JsonObject>(); // .add() en lugar de .createNestedObject()
  textPart["text"] = prompt;

  String payload;
  serializeJson(jsonReq, payload);

  // 3. Hacer la llamada HTTP
  HTTPClient http;
  http.begin(GEMINI_API_URL);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    String respuesta = http.getString();

    // 4. Parsear la respuesta JSON (¡Sintaxis v7!)
    JsonDocument jsonResp; // Usamos JsonDocument
    DeserializationError error = deserializeJson(jsonResp, respuesta);

    if (error) {
      Serial.print(F("Error al parsear JSON de respuesta: "));
      Serial.println(error.c_str());
      return "Error: JSON";
    }

    // Navegar la estructura de respuesta (¡Sintaxis v7!)
    // Esta forma es más robusta y simple que el .containsKey() múltiple.
    // Intenta obtener el valor directamente.
    JsonVariant textVariant = jsonResp["candidates"][0]["content"]["parts"][0]["text"];

    if (!textVariant.isNull()) {
      // ¡Éxito!
      String textoRespuesta = textVariant.as<String>();
      textoRespuesta.replace("\n", " "); // Quitar saltos de línea
      return textoRespuesta;
    } else {
      // El JSON no tenía la estructura que esperábamos
      Serial.println("Error: Estructura JSON no esperada.");
      Serial.println("Respuesta cruda: " + respuesta); // Imprime la respuesta para debug
      return "Error: IA";
    }

  } else {
    Serial.printf("Error en llamada HTTP: %s\n", http.errorToString(httpCode).c_str());
    return "Error: HTTP";
  }

  http.end();
  return "Error";
}

/**
 * Muestra la respuesta de Gemini en la OLED,
 * manejando el salto de línea.
 */
void actualizarOLED_RespuestaIA(String texto) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont11_tr); 
  u8g2.drawStr(0, 10, "RECOMENDACION IA:");
  
  // Lógica simple para cortar el texto en líneas
  int maxCharsPorLinea = 21; // 128px / (ancho aprox_letra 6px)
  String linea1 = texto;
  String linea2 = "";
  String linea3 = "";

  if (texto.length() > maxCharsPorLinea) {
    // Busca un espacio para cortar limpiamente
    int corte1 = -1;
    for (int i = maxCharsPorLinea; i > 0; i--) {
        if (texto.charAt(i) == ' ') {
            corte1 = i;
            break;
        }
    }
    if (corte1 == -1) corte1 = maxCharsPorLinea; // Corta la palabra si no hay espacio

    linea1 = texto.substring(0, corte1);
    String resto = texto.substring(corte1 + 1); // +1 para saltar el espacio

    if (resto.length() > maxCharsPorLinea) {
        int corte2 = -1;
        for (int i = maxCharsPorLinea; i > 0; i--) {
            if (resto.charAt(i) == ' ') {
                corte2 = i;
                break;
            }
        }
        if (corte2 == -1) corte2 = maxCharsPorLinea;

        linea2 = resto.substring(0, corte2);
        linea3 = resto.substring(corte2 + 1);
    } else {
        linea2 = resto;
    }
  }
  
  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(0, 26, linea1.c_str());
  u8g2.drawStr(0, 42, linea2.c_str());
  u8g2.drawStr(0, 58, linea3.c_str());
  
  u8g2.sendBuffer();
}

/**
 * Muestra las instrucciones del modo AP en el OLED.
 */
void actualizarOLED_AP_Mode() {
    u8g2.clearBuffer(); 
    u8g2.setFont(u8g2_font_profont11_tr); 
    u8g2.drawStr(0, 10, "MODO CONFIGURACION");

    u8g2.setFont(u8g2_font_profont12_tr);
    u8g2.drawStr(0, 26, "1. Conectate a la red:");
    u8g2.drawStr(0, 42, "   'EstacionMeteo-Config'");
    u8g2.drawStr(0, 58, "2. Ve a 192.168.4.1");
    
    u8g2.sendBuffer(); 
}

/**
 * Controla los LEDs y el ventilador basado en la temperatura.
 */
void controlarActuadores(float temperatura) {
  // Resetea todos los LEDs
  digitalWrite(LED_ROJO_PIN, LOW);
  digitalWrite(LED_VERDE_PIN, LOW);
  digitalWrite(LED_AZUL_PIN, LOW);

  if (temperatura > TEMP_CALIDA) {
    // CALIENTE
    digitalWrite(LED_ROJO_PIN, HIGH);
    digitalWrite(FAN_PIN, HIGH);
  } else if (temperatura < TEMP_FRIA) {
    // FRÍO
    digitalWrite(LED_AZUL_PIN, HIGH);
    digitalWrite(FAN_PIN, LOW);
  } else {
    // NORMAL
    digitalWrite(LED_VERDE_PIN, HIGH); 
    digitalWrite(FAN_PIN, LOW);
  }
}

/**
 * Actualiza la pantalla OLED con los valores de los sensores.
 */
void actualizarOLED_Sensores(float t, float h, float lux, int airQuality) {
  char buffer[32]; // Un buffer para formatear texto

  u8g2.clearBuffer(); 
  u8g2.setFont(u8g2_font_profont11_tr); // Una fuente clara y pequeña
  u8g2.drawStr(0, 10, "EST. METEOROLOGICA");

  u8g2.setFont(u8g2_font_profont12_tr);
  
  // Fila 1: Temperatura y Humedad
  sprintf(buffer, "T: %.1f C", t);
  u8g2.drawStr(0, 26, buffer);
  sprintf(buffer, "H: %.0f %%", h);
  u8g2.drawStr(70, 26, buffer);

  // Fila 2: Luz
  sprintf(buffer, "Luz: %.0f lux", lux);
  u8g2.drawStr(0, 42, buffer);

  // Fila 3: Calidad de Aire
  sprintf(buffer, "Aire (raw): %d", airQuality);
  u8g2.drawStr(0, 58, buffer);
  
  u8g2.sendBuffer(); 
}

/**
 * Imprime los valores de los sensores al Monitor Serial.
 */
void imprimirSerial(float t, float h, float lux, int airQuality) {
  Serial.println("---------------------------------");
  Serial.print("Temperatura: "); Serial.print(t, 1); Serial.println(" *C");
  Serial.print("Humedad:     "); Serial.print(h, 0); Serial.println(" %");
  Serial.print("Luminosidad: "); Serial.print(lux, 0); Serial.println(" lux");
  Serial.print("Cal. de Aire: "); Serial.println(airQuality);
}