#include <Wire.h>
#include <Adafruit_INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

// Definición de pines
#define ONE_WIRE_BUS 2   // Pin de datos para el sensor DS18B20
#define RELAY_PIN 3      // Pin de control para relé de protección/corte

// Instancias de los sensores y pantalla
Adafruit_INA219 ina219;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Dirección I2C común (0x27 o 0x3F)

// Parámetros de la Batería
const float BATT_CAPACITY_AH = 10.0; // Capacidad nominal en Amperios-hora (Ah)
const float V_MAX = 13.8;            // Voltaje de batería llena (ej. Plomo-Ácido o LiFePO4 12V)
const float V_MIN = 10.8;            // Voltaje de corte por sobredescarga
const float TEMP_MAX = 45.0;         // Temperatura máxima segura en °C

// Variables globales para el Estado de Carga (SoC) y tiempo
float remaining_Ah = 10.0;
float soc = 100.0;
unsigned long lastTime = 0;

void setup() {
  Serial.begin(9600);
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Conectar carga/panel por defecto

  // Inicialización de la pantalla LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Monitoreo Solar");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  // Inicialización del sensor INA219
  if (!ina219.begin()) {
    Serial.println("Error: No se detecto el modulo INA219.");
    lcd.setCursor(0, 1);
    lcd.print("Error INA219    ");
    while (1);
  }

  // Inicialización del sensor de temperatura
  tempSensor.begin();

  delay(1500);
  lcd.clear();

  // Estimación inicial del SoC basada en Voltaje en Circuito Abierto (OCV)
  float busVoltage = ina219.getBusVoltage_V();
  if (busVoltage >= V_MAX) {
    soc = 100.0;
  } else if (busVoltage <= V_MIN) {
    soc = 0.0;
  } else {
    soc = ((busVoltage - V_MIN) / (V_MAX - V_MIN)) * 100.0;
  }
  
  remaining_Ah = (soc / 100.0) * BATT_CAPACITY_AH;
  lastTime = millis();
}

void loop() {
  unsigned long currentTime = millis();
  float deltaTime = (currentTime - lastTime) / 1000.0; // Tiempo transcurrido en segundos

  // Muestreo cada 1 segundo (1000 ms) sin bloquear el microcontrolador
  if (deltaTime >= 1.0) {
    lastTime = currentTime;

    // 1. Lectura de variables físicas
    float busVoltage = ina219.getBusVoltage_V();
    float current_mA = ina219.getCurrent_mA(); // Positivo: carga | Negativo: descarga
    float current_A = current_mA / 1000.0;
    
    tempSensor.requestTemperatures();
    float temperatureC = tempSensor.getTempCByIndex(0);

    // 2. Algoritmo de Conteo de Culombios (Coulomb Counting) para SoC
    // Delta Ah = Corriente (A) * Tiempo (Horas)
    float delta_Ah = current_A * (deltaTime / 3600.0);
    remaining_Ah += delta_Ah;

    // Limitar los valores dentro del rango real (0 a Capacidad Máxima)
    if (remaining_Ah > BATT_CAPACITY_AH) remaining_Ah = BATT_CAPACITY_AH;
    if (remaining_Ah < 0.0) remaining_Ah = 0.0;

    soc = (remaining_Ah / BATT_CAPACITY_AH) * 100.0;

    // 3. Evaluación de Salud (SoH) y Alertas de Seguridad
    String healthStatus = "SALUD: BUENA";
    bool tripProtection = false;

    if (temperatureC >= TEMP_MAX) {
      healthStatus = "ALERTA: T. ALTA";
      tripProtection = true;
    } else if (busVoltage <= V_MIN) {
      healthStatus = "BATERIA AGOTADA";
      tripProtection = true;
    }

    // Control del relé de protección
    if (tripProtection) {
      digitalWrite(RELAY_PIN, LOW); // Desconecta la línea para proteger el sistema
    } else {
      digitalWrite(RELAY_PIN, HIGH);
    }

    // 4. Salida por Puerto Serie (Debug)
    Serial.print("V: "); Serial.print(busVoltage); Serial.print("V | ");
    Serial.print("I: "); Serial.print(current_A); Serial.print("A | ");
    Serial.print("SoC: "); Serial.print(soc, 1); Serial.print("% | ");
    Serial.print("Temp: "); Serial.print(temperatureC); Serial.print("C | ");
    Serial.println(healthStatus);

    // 5. Visualización en Pantalla LCD (Alterna la información cada ciclo)
    static bool toggleDisplay = false;
    toggleDisplay = !toggleDisplay;

    lcd.clear();
    if (toggleDisplay) {
      // Pantalla 1: Voltaje, Corriente y SoC
      lcd.setCursor(0, 0);
      lcd.print("V:"); lcd.print(busVoltage, 1); 
      lcd.print("V I:"); lcd.print(current_A, 1); lcd.print("A");
      lcd.setCursor(0, 1);
      lcd.print("SoC: "); lcd.print(soc, 1); lcd.print("%");
    } else {
      // Pantalla 2: Temperatura y Estado del Sistema
      lcd.setCursor(0, 0);
      lcd.print("Temp: "); lcd.print(temperatureC, 1); lcd.print((char)223); lcd.print("C");
      lcd.setCursor(0, 1);
      lcd.print(healthStatus);
    }
  }
}