/*
 * PROYECTO: ESTACIÓN METEOROLÓGICA INTELIGENTE
 * Autores: Guillermo Mero, Luis Cordova, William Mayorga
*/

// 1. LIBRERÍAS
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h> 
#include "DHT.h" 
#include <BH1750.h>
#include "wifi_manager.h"

// ... (Todas tus definiciones de pines y objetos (DHT, BH1750, OLED, LEDs) NO CAMBIAN) ...
// (Asegúrate de que esta sección esté completa)
// --- Sensores ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE); 
BH1750 lightMeter; 
const int MQ135_PIN = 36;
// --- Pantalla (SPI) ---
#define OLED_SCK  18
#define OLED_SDA  23
#define OLED_DC   19
#define OLED_RES  5
#define OLED_CS   U8X8_PIN_NONE 
U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, OLED_SCK, OLED_SDA, OLED_CS, OLED_DC, OLED_RES);
// --- Actuadores ---
const int LED_ROJO_PIN = 14; 
const int LED_VERDE_PIN = 12; 
const int LED_AZUL_PIN = 13;  
const int FAN_PIN = 27;
// --- Umbrales ---
const float TEMP_CALIDA = 30.0;
const float TEMP_FRIA = 18.0;


// --- Timers (para no usar delay()) ---
unsigned long tiempoAnteriorSensores = 0;
const long INTERVALO_SENSORES = 3000; // 3 segundos

// 3. PROTOTIPOS DE FUNCIONES
void controlarActuadores(float temperatura);
void actualizarOLED(float t, float h, float lux, int airQuality);
void imprimirSerial(float t, float h, float lux, int airQuality);
void actualizarOLED_AP_Mode(); // ¡Nueva función para el OLED!


// 4. SETUP
void setup() {
  Serial.begin(115200);
  Serial.println("--- Estacion Meteorologica Iniciando ---");

  // Inicia I2C (SDA=21, SCL=22)
  Wire.begin(21, 22); 
  
  // Inicia Sensores
  dht.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  
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
  digitalWrite(FAN_PIN, LOW);

  // --- INICIO DE LA LÓGICA WIFI ---
  EEPROM.begin(512); // Inicializa EEPROM

  if (!lastRed()) { // Intenta conectar con redes guardadas
      // Si falla, inicia el modo Access Point
      initAP("EstacionMeteo-Config", "12345678"); 
  } else {
      Serial.println("Conexion WiFi exitosa en el arranque.");
  }
}

// 5. LOOP (¡EL NUEVO LOOP INTELIGENTE!)
void loop() {
  // Comprueba el estado del WiFi en CADA ciclo
  if (WiFi.status() != WL_CONNECTED) {
    // MODO "CONFIGURACIÓN WIFI"
    // El WiFi no está (o se perdió). Ejecuta el servidor de AP.
    loopAP(); 
    
    // Muestra instrucciones en la pantalla OLED
    actualizarOLED_AP_Mode();
    
    // Apaga los actuadores por seguridad
    digitalWrite(FAN_PIN, LOW);
    digitalWrite(LED_ROJO_PIN, LOW);
    digitalWrite(LED_VERDE_PIN, LOW);
    digitalWrite(LED_AZUL_PIN, LOW);

  } else {
    // MODO "ESTACIÓN METEOROLÓGICA"
    // El WiFi está OK. Ejecuta la lógica normal.

    unsigned long tiempoActual = millis();

    // Lógica sin delay() para que el loop no se bloquee
    if (tiempoActual - tiempoAnteriorSensores >= INTERVALO_SENSORES) {
      tiempoAnteriorSensores = tiempoActual;

      float t = dht.readTemperature();
      float h = dht.readHumidity();
      int airQuality = analogRead(MQ135_PIN);
      float lux = lightMeter.readLightLevel();

      if (isnan(t)) t = 0.0;
      if (isnan(h)) h = 0.0;
      if (lux < 0) lux = 0.0; 

      controlarActuadores(t);
      actualizarOLED(t, h, lux, airQuality);
      //imprimirSerial(t, h, lux, airQuality); // Descomenta si necesitas debug
    }

    // (Aquí es donde pondremos la lógica de IA en el futuro)
    // if (tiempoActual - tiempoAnteriorGemini >= INTERVALO_GEMINI) { ... }
  }
}

// --- Implementación de Funciones Auxiliares ---

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


// ... (Pega aquí tus funciones originales SIN CAMBIOS) ...
/*
void controlarActuadores(float temperatura) { ... }
void actualizarOLED(float t, float h, float lux, int airQuality) { ... }
void imprimirSerial(float t, float h, float lux, int airQuality) { ... }
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

void actualizarOLED(float t, float h, float lux, int airQuality) {
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

void imprimirSerial(float t, float h, float lux, int airQuality) {
  Serial.println("---------------------------------");
  Serial.print("Temperatura: "); Serial.print(t, 1); Serial.println(" *C");
  Serial.print("Humedad:     "); Serial.print(h, 0); Serial.println(" %");
  Serial.print("Luminosidad: "); Serial.print(lux, 0); Serial.println(" lux");
  Serial.print("Cal. de Aire: "); Serial.println(airQuality);
}