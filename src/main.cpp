/*
 * PROYECTO: ESTACIÓN METEOROLÓGICA (FÍSICA - SIN PANTALLA OLED)
 * Salida: Únicamente por Monitor Serial
 * Componentes: ESP32, DHT11, BH1750, MQ-135
*/

// 1. LIBRERÍAS
#include <Wire.h>    // Para I2C (BH1750)
#include "DHT.h"     // Para Temp/Humedad
#include <BH1750.h>  // Librería real para el sensor físico

// 2. DEFINICIONES GLOBALES DE PINES Y OBJETOS

// --- DHT11 (Temp/Humedad) ---
#define DHTPIN 4
#define DHTTYPE DHT11   // <-- CAMBIO AQUÍ (Antes era DHT22)
DHT dht(DHTPIN, DHTTYPE); 

// --- BH1750 (Luminosidad) ---
BH1750 lightMeter; 

// --- MQ-135 (Calidad del Aire) ---
const int MQ135_PIN = 36; // Pin analógico (VP)

// 3. SETUP: Se ejecuta una vez al inicio
void setup() {
  Serial.begin(115200); 
  Serial.println("\n--- Estación Meteorológica Física Iniciada (DHT11) ---");

  // Inicia I2C (SDA=21, SCL=22)
  Wire.begin(21, 22); 
  
  // Inicia Sensor DHT
  dht.begin();
  
  // Inicia Sensor BH1750 FÍSICO
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 físico OK!"));
  } else {
    Serial.println(F("Error al iniciar BH1750"));
  }
}

// 4. LOOP: Se ejecuta repetidamente
void loop() {
  delay(3000); 

  // --- LECTURA DHT ---
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  
  // --- LECTURA MQ-135 ---
  int airQuality = analogRead(MQ135_PIN);

  // --- LECTURA BH1750 ---
  float lux = lightMeter.readLightLevel();

  // --- Manejo de lecturas fallidas ---
  if (isnan(t)) t = 0.0;
  if (isnan(h)) h = 0.0;
  if (lux < 0) lux = 0.0; 

  // --- MOSTRAR EN MONITOR SERIAL ---
  Serial.println("----------------------------------------");
  Serial.print("Temperatura: ");
  Serial.print(t, 1); 
  Serial.println(" *C");
  
  Serial.print("Humedad: ");
  Serial.print(h, 0); 
  Serial.println(" %");

  Serial.print("Luminosidad: ");
  Serial.print(lux, 0);
  Serial.println(" lux");

  Serial.print("Calidad del Aire (Valor Analógico): ");
  Serial.println(airQuality);
}