// Incluir las librerías necesarias 
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include "Arduino.h"

// --- Configuración del Sensor ---

// Define el pin donde conectaste el sensor DATA
// Tu documento especifica GPIO 4 
#define DHTPIN 4

// Define el tipo de sensor que estás usando
// En tu caso, DHT11 [cite: 11, 36]
#define DHTTYPE DHT11

// Inicializar el objeto 'dht' con el pin y el tipo
DHT dht(DHTPIN, DHTTYPE);

// --- Fin de la Configuración ---


void setup() {
  // Inicia la comunicación serial para ver los resultados en el monitor [cite: 49]
  // 115200 es una velocidad estándar para ESP32
  Serial.begin(115200); 
  Serial.println("Prueba del sensor DHT11 en ESP32");

  // Inicia el sensor
  dht.begin();
}

void loop() {
  // Los sensores DHT son lentos. Se recomienda esperar al menos 2 segundos 
  // entre lecturas para no obtener datos erróneos.
  delay(2000); 

  // --- Lectura de Sensores ---

  // Lee la humedad
  float h = dht.readHumidity();
  // Lee la temperatura en grados Celsius
  float t = dht.readTemperature();

  // --- Verificación de Errores ---
  
  // Comprueba si alguna de las lecturas falló (retorna 'nan' - Not a Number)
  if (isnan(h) || isnan(t)) {
    Serial.println("Error al leer el sensor DHT11!");
    return; // Sal del loop y vuelve a intentarlo
  }

  // --- Impresión de Datos ---
  
  // Si todo salió bien, imprime los valores en el Monitor Serial [cite: 49]
  Serial.print("Humedad: ");
  Serial.print(h);
  Serial.print(" %  |  ");
  Serial.print("Temperatura: ");
  Serial.print(t);
  Serial.println(" °C");
}