/*
 * PROYECTO: ESTACIÓN METEOROLÓGICA
 * Autores: Guillermo Mero, Luis Cordova, William Mayorga
*/

// 1. LIBRERÍAS
#include <Arduino.h> // Necesario para PlatformIO
#include <U8g2lib.h> // Para OLED
#include <Wire.h>    // Para I2C (BH1750)
#include "DHT.h"     // Para Temp/Humedad
#include <BH1750.h>  // Librería real para el sensor físico

// 2. DEFINICIONES GLOBALES

// --- Sensores ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE); 

BH1750 lightMeter; // Objeto de la librería BH1750 real

const int MQ135_PIN = 36; // Pin analógico (VP / SVP)

// --- Pantalla (SPI de 4 hilos) ---
#define OLED_SCK  18   // Conecta al pin 'SCK' (o SCL) de tu pantalla
#define OLED_SDA  23   // Conecta al pin 'SDA' (o MOSI) de tu pantalla
#define OLED_DC   19   // Conecta al pin 'DC' de tu pantalla
#define OLED_RES  5    // Conecta al pin 'RES' de tu pantalla
#define OLED_CS   U8X8_PIN_NONE // No usamos Chip Select, es común en estas pantallas

// Constructor de U8G2 para SPI de 4 hilos por Software (SW_SPI)
U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, 
    /* clock=*/ OLED_SCK, 
    /* data=*/ OLED_SDA, 
    /* cs=*/ OLED_CS, 
    /* dc=*/ OLED_DC, 
    /* reset=*/ OLED_RES);

// --- Actuadores ---
const int LED_ROJO_PIN = 14; 
const int LED_AMARILLO_PIN = 12; 
const int LED_VERDE_PIN = 13;  
const int FAN_PIN = 27;

// --- Umbrales de Temperatura ---
const float TEMP_CALIDA = 30.0;
const float TEMP_FRIA = 18.0;

// 3. PROTOTIPOS DE FUNCIONES
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
  pinMode(LED_AMARILLO_PIN, OUTPUT);
  pinMode(LED_VERDE_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  
  // Apagar todo al iniciar
  digitalWrite(LED_ROJO_PIN, LOW);
  digitalWrite(LED_AMARILLO_PIN, LOW);
  digitalWrite(LED_VERDE_PIN, LOW);
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
  
  // --- MOSTRAR EN MONITOR SERIAL (Para debug) ---
  //imprimirSerial(t, h, lux, airQuality);
}

// --- Implementación de Funciones Auxiliares ---

/**
 * Controla los LEDs y el ventilador basado en la temperatura.
 */
void controlarActuadores(float temperatura) {
  // Resetea todos los LEDs
  digitalWrite(LED_ROJO_PIN, LOW);
  digitalWrite(LED_AMARILLO_PIN, LOW);
  digitalWrite(LED_VERDE_PIN, LOW);

  if (temperatura > TEMP_CALIDA) {
    // CALIENTE
    digitalWrite(LED_ROJO_PIN, HIGH);
    digitalWrite(FAN_PIN, HIGH);
  } else if (temperatura < TEMP_FRIA) {
    // FRÍO
    digitalWrite(LED_VERDE_PIN, HIGH);
    digitalWrite(FAN_PIN, LOW);
  } else {
    // NORMAL
    digitalWrite(LED_AMARILLO_PIN, HIGH); 
    digitalWrite(FAN_PIN, LOW);
  }
}

/**
 * Actualiza la pantalla OLED con los valores de los sensores.
 */
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