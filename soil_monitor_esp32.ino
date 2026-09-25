/*
  Soil Field Monitor — ESP32 firmware

  Reads soil moisture (capacitive analog sensor) and soil temperature
  (DS18B20 waterproof probe), then pushes each reading to a Supabase
  table over WiFi.

  Wiring:
    Soil moisture sensor  -> AOUT to GPIO34 (ADC1), VCC to 3V3, GND to GND
    DS18B20 temp probe    -> Data to GPIO4 (with a 4.7k pull-up resistor
                              between Data and 3V3), VCC to 3V3, GND to GND

  Libraries needed (Arduino Library Manager):
    - OneWire
    - DallasTemperature
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---- Fill these in ----
const char* WIFI_SSID          = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD      = "YOUR_WIFI_PASSWORD";
const char* SUPABASE_URL       = "https://YOUR_PROJECT.supabase.co";
const char* SUPABASE_ANON_KEY  = "YOUR_ANON_KEY";
// ------------------------

const int SOIL_PIN      = 34;   // ADC1 pin, capacitive soil sensor
const int ONE_WIRE_PIN  = 4;    // DS18B20 data pin

// Calibrate against your own sensor: read raw values in dry air and fully in water,
// then put those numbers here.
const int SOIL_DRY_RAW = 3000;
const int SOIL_WET_RAW = 1200;

const unsigned long SEND_INTERVAL_MS = 60000; // send a reading every 60s

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature tempSensor(&oneWire);

unsigned long lastSend = 0;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
}

float readSoilMoisturePercent() {
  int raw = analogRead(SOIL_PIN);
  float pct = (float)(SOIL_DRY_RAW - raw) / (SOIL_DRY_RAW - SOIL_WET_RAW) * 100.0;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

float readSoilTemperatureC() {
  tempSensor.requestTemperatures();
  return tempSensor.getTempCByIndex(0);
}

void sendReading(float moisture, float temperature) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/readings";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);

  String body = "{\"soil_moisture\":" + String(moisture, 1) +
                ",\"temperature\":" + String(temperature, 1) + "}";

  int code = http.POST(body);
  Serial.printf("POST %d: %s\n", code, http.getString().c_str());
  http.end();
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // 0-4095
  tempSensor.begin();
  connectWiFi();
}

void loop() {
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    float moisture = readSoilMoisturePercent();
    float temperature = readSoilTemperatureC();
    Serial.printf("Moisture: %.1f%%  Temp: %.1fC\n", moisture, temperature);
    sendReading(moisture, temperature);
  }
}
