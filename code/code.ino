#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HX711.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID"; // Replace with your WiFi SSID
const char* password = "YOUR_WIFI_PASSWORD"; // Replace with your WiFi password

// MQTT settings
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_client_id = "225510017";
const char* mqtt_topic = "nugra/data/kolam";

// Sensor pins
#define ONE_WIRE_BUS 4 // DS18B20 data pin
#define DO_PIN 34      // Analog pin for DO sensor (simulated)
#define PH_PIN 35      // Analog pin for pH sensor (simulated)
#define HX711_DOUT 5   // HX711 data pin
#define HX711_SCK 18   // HX711 clock pin
#define TRIG_PIN 13    // Ultrasonic sensor trigger pin
#define ECHO_PIN 12    // Ultrasonic sensor echo pin

// DS18B20 setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// HX711 setup
HX711 scale;

// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

// Pond ID
const int pond_id = 1; // Change this for different ponds (e.g., 1, 2, 3)

// Calibration values
const float DO_VOLTAGE_MIN = 0.0;  // Voltage at 0 mg/L
const float DO_VOLTAGE_MAX = 3.3;  // Voltage at max DO
const float PH_VOLTAGE_MIN = 0.0;  // Voltage at pH 0
const float PH_VOLTAGE_MAX = 3.3;  // Voltage at pH 14
const float TANK_HEIGHT = 100.0;   // Tank height in cm for water level
const float SCALE_CALIBRATION = -7050.0; // Calibration factor for HX711

void setup() {
  Serial.begin(115200);
  
  // Initialize sensors
  sensors.begin();
  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(SCALE_CALIBRATION);
  scale.tare(); // Reset scale to zero
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Connect to WiFi
  setup_wifi();
  
  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
}

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

float readTemperature() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  if (temp == DEVICE_DISCONNECTED_C) {
    Serial.println("Error reading temperature");
    return 0.0;
  }
  return temp;
}

float readDO() {
  int analogValue = analogRead(DO_PIN);
  float voltage = analogValue * (3.3 / 4095.0); // ESP32 ADC is 12-bit
  // Map voltage to DO (0-8 mg/L, adjust based on sensor specs)
  float doValue = mapFloat(voltage, DO_VOLTAGE_MIN, DO_VOLTAGE_MAX, 0.0, 8.0);
  return constrain(doValue, 0.0, 8.0);
}

float readPH() {
  int analogValue = analogRead(PH_PIN);
  float voltage = analogValue * (3.3 / 4095.0);
  // Map voltage to pH (0-14, adjust based on sensor specs)
  float phValue = mapFloat(voltage, PH_VOLTAGE_MIN, PH_VOLTAGE_MAX, 0.0, 14.0);
  return constrain(phValue, 0.0, 14.0);
}

float readWeight() {
  if (scale.is_ready()) {
    float weight = scale.get_units(10); // Average of 10 readings
    if (weight < 0) weight = 0.0; // Ignore negative values
    return weight;
  }
  return 0.0;
}

float readWaterLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2; // Distance in cm
  float level = ((TANK_HEIGHT - distance) / TANK_HEIGHT) * 100.0; // Percentage
  return constrain(level, 0.0, 100.0);
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void publishSensorData() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Read sensor values
  float suhu = readTemperature();
  float doValue = readDO();
  float phValue = readPH();
  float berat_pakan = readWeight();
  float level_air = readWaterLevel();
  
  // Create JSON object
  StaticJsonDocument<200> doc;
  doc["kolam"] = pond_id;
  doc["suhu"] = suhu;
  doc["do"] = doValue;
  doc["ph"] = phValue;
  doc["berat_pakan"] = berat_pakan;
  doc["level_air"] = level_air;
  
  // Serialize JSON to string
  char buffer[256];
  serializeJson(doc, buffer);
  
  // Publish to MQTT
  if (client.publish(mqtt_topic, buffer)) {
    Serial.println("Data published: ");
    Serial.println(buffer);
  } else {
    Serial.println("Failed to publish data");
  }
}

void loop() {
  publishSensorData();
  delay(10000); // Publish every 10 seconds
}