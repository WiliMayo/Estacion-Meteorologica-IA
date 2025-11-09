/*
 * PROYECTO: ESTACIÓN METEOROLÓGICA (IMPLEMENTACIÓN FÍSICA COMPLETA)
 * Componentes: ESP32, DHT22, BH1750, MQ-135, OLED, LEDs, Ventilador (con TIP122)
 * Autor: Tu Nombre
*/

// 1. LIBRERÍAS
#include <Arduino.h> // Necesario para PlatformIO
#include <U8g2lib.h> // Para OLED
#include <Wire.h>    // Para I2C (OLED y BH1750)
#include "DHT.h"     // Para Temp/Humedad
#include <BH1750.h>  // Librería real para el sensor físico

// 2. DEFINICIONES GLOBALES

// --- Sensores ---
#define DHTPIN 4
#define DHTTYPE DHT11 // Asegúrate que sea DHT22, si es DHT11 cámbialo
DHT dht(DHTPIN, DHTTYPE); 

BH1750 lightMeter; // Objeto de la librería BH1750 real

const int MQ135_PIN = 36; // Pin analógico (VP / SVP)

// --- Pantalla (Asegúrate de que los pines I2C sean 21 y 22) ---
// SDA=21, SCL=22
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- Actuadores ---
const int LED_ROJO_PIN = 14; 
const int LED_VERDE_PIN = 12; 
const int LED_AZUL_PIN = 13;  
const int FAN_PIN = 27;       // Pin que va a la resistencia del TIP122

// --- Umbrales de Temperatura ---
const float TEMP_CALIDA = 30.0;
const float TEMP_FRIA = 18.0;

// 3. PROTOTIPOS DE FUNCIONES (Buena práctica en C++)
void controlarActuadores(float temperatura);
void actualizarOLED(float t, float h, float lux, int airQuality);
void imprimirSerial(float t, float h, float lux, int airQuality);


// 4. SETUP
void setup() {
  Serial.begin(115200);

  // Inicia I2C (SDA=21, SCL=22)
  Wire.begin(21, 22); 
  
  // Inicia Sensor DHT
  dht.begin();
  
  // Inicia Pantalla OLED
  u8g2.begin();

  // Inicia Sensor BH1750
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 físico OK!"));
  } else {
    Serial.println(F("Error al iniciar BH1750"));
  }

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

  Serial.println("--- Estación Meteorológica Física Iniciada ---");
}

// 5. LOOP
void loop() {
  // Espera 3 segundos entre lecturas
  delay(3000); 

  // --- LECTURA DE SENSORES ---
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int airQuality = analogRead(MQ135_PIN);
  float lux = lightMeter.readLightLevel();

  // --- Manejo de lecturas fallidas ---
  if (isnan(t)) t = 0.0;
  if (isnan(h)) h = 0.0;
  if (lux < 0) lux = 0.0; 

  // --- LÓGICA DE CONTROL ---
  controlarActuadores(t);

  // --- MOSTRAR EN PANTALLA OLED ---
  actualizarOLED(t, h, lux, airQuality);
  
  // --- MOSTRAR EN MONITOR SERIAL ---
  imprimirSerial(t, h, lux, airQuality);
}

// --- Implementación de Funciones Auxiliares ---

/**
 * Controla los LEDs y el ventilador basado en la temperatura.
 * La lógica del ventilador es ACTIVE HIGH (HIGH = ON) para el transistor.
 */
void controlarActuadores(float temperatura) {
  // Resetea todos los LEDs
  digitalWrite(LED_ROJO_PIN, LOW);
  digitalWrite(LED_VERDE_PIN, LOW);
  digitalWrite(LED_AZUL_PIN, LOW);

  if (temperatura > TEMP_CALIDA) {
    // CALIENTE
    digitalWrite(LED_ROJO_PIN, HIGH);
    digitalWrite(FAN_PIN, HIGH);     // ENCIENDE el ventilador
  } else if (temperatura < TEMP_FRIA) {
    // FRÍO
    digitalWrite(LED_AZUL_PIN, HIGH);
    digitalWrite(FAN_PIN, LOW);      // APAGA el ventilador
  } else {
    // NORMAL
    digitalWrite(LED_VERDE_PIN, HIGH); 
    digitalWrite(FAN_PIN, LOW);      // APAGA el ventilador
  }
}

/**
 * Actualiza la pantalla OLED con los valores de los sensores.
 */
void actualizarOLED(float t, float h, float lux, int airQuality) {
  char buffer[32]; // Un buffer para formatear texto

  u8g2.clearBuffer(); 
  u8g2.setFont(u8g2_font_profont11_tr); // Una fuente clara y pequeña
  u8g2.drawStr(0, 10, "ESTACION METEOROLOGICA");

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