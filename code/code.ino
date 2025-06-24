// ====== LIBRARY ======
#include <WiFi.h>               // Library koneksi WiFi
#include <PubSubClient.h>       // Library MQTT
#include <OneWire.h>            // Untuk komunikasi sensor DS18B20
#include <DallasTemperature.h>  // Untuk sensor suhu
#include <HX711.h>              // Untuk sensor berat HX711
#include <ArduinoJson.h>        // Untuk membuat JSON

// ====== KONFIGURASI WIFI & MQTT ======
const char* ssid = ""; // Masukkan nama WiFi
const char* password = ""; // Masukkan password WiFi

const char* mqtt_server = "broker.emqx.io";       // Server MQTT
const int mqtt_port = 1883;                       // Port default
const char* mqtt_client_id = "225510000";         // ID client unik
const char* mqtt_topic = "nugra/data/kolam";      // Topik untuk data sensor
const char* mqtt_control_topic = "nugra/kontrol/kolam"; // Topik kontrol manual

// ====== PIN SENSOR ======
#define ONE_WIRE_BUS 4    // Pin data DS18B20
#define DO_PIN 34         // Pin sensor DO (Analog)
#define PH_PIN 35         // Pin sensor pH (Analog)
#define HX711_DOUT 5      // Data pin HX711
#define HX711_SCK 18      // Clock pin HX711
#define TRIG_PIN 13       // Trigger Ultrasonik
#define ECHO_PIN 12       // Echo Ultrasonik

// ====== PIN AKTUATOR ======
#define POMPA_MASUK_PIN 14   // Pompa untuk memasukkan air
#define POMPA_KELUAR_PIN 33  // Pompa untuk mengeluarkan air
#define AERATOR_PIN 27       // Aerator (oksigenasi)
#define PAKAN_PIN 25         // Motor penyebar pakan

// ====== VARIABEL KONTROL ======
bool suhuAktif = true;
bool doAktif = true;
bool phAktif = true;
bool beratAktif = true;
bool levelAktif = true;
bool pakanAktif = true;

// ====== OBJEK SENSOR ======
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
HX711 scale;
WiFiClient espClient;
PubSubClient client(espClient);

// ====== KONFIGURASI SENSOR ======
const int pond_id = 1;
const float DO_VOLTAGE_MIN = 0.0;
const float DO_VOLTAGE_MAX = 3.3;
const float PH_VOLTAGE_MIN = 0.0;
const float PH_VOLTAGE_MAX = 3.3;
const float TANK_HEIGHT = 100.0;       // Tinggi tangki dalam cm
const float SCALE_CALIBRATION = -7050.0; // Kalibrasi HX711

// ====== TIMER NON-BLOCKING ======
unsigned long lastPublish = 0;
const unsigned long publishInterval = 10000; // 10 detik

// ====== SETUP ======
void setup() {
  Serial.begin(115200); // Memulai Serial Monitor

  sensors.begin(); // Mulai sensor suhu
  scale.begin(HX711_DOUT, HX711_SCK); // Mulai HX711
  scale.set_scale(SCALE_CALIBRATION); // Set kalibrasi berat
  scale.tare(); // Nol-kan sensor berat

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(POMPA_MASUK_PIN, OUTPUT);
  pinMode(POMPA_KELUAR_PIN, OUTPUT);
  pinMode(AERATOR_PIN, OUTPUT);
  pinMode(PAKAN_PIN, OUTPUT);

  setup_wifi(); // Hubungkan ke WiFi
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); // Set fungsi terima pesan
}

// ====== SETUP WIFI NON-BLOCKING DENGAN TIMEOUT ======
void setup_wifi() {
  Serial.println("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 60000) {
    delay(100);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("\nWiFi Terhubung!");
  else
    Serial.println("\nWiFi gagal terhubung.");
}

// ====== REKONEKSI MQTT ======
void reconnect() {
  while (!client.connected()) {
    Serial.print("Mencoba konek MQTT...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("Berhasil!");
      client.subscribe(mqtt_control_topic);
    } else {
      Serial.print("Gagal, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

// ====== CALLBACK UNTUK KONTROL MANUAL DARI MQTT ======
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println("Kontrol MQTT: " + msg);

  if (msg == "suhu_on") suhuAktif = true;
  else if (msg == "suhu_off") suhuAktif = false;
  else if (msg == "do_on") doAktif = true;
  else if (msg == "do_off") doAktif = false;
  else if (msg == "ph_on") phAktif = true;
  else if (msg == "ph_off") phAktif = false;
  else if (msg == "berat_on") beratAktif = true;
  else if (msg == "berat_off") beratAktif = false;
  else if (msg == "level_on") levelAktif = true;
  else if (msg == "level_off") levelAktif = false;
  else if (msg == "pakan_on") pakanAktif = true;
  else if (msg == "pakan_off") pakanAktif = false;
}

// ====== FUNGSI PEMBACA SENSOR ======
float readTemperature() {
  if (!suhuAktif) return -1;
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);
  return (t == DEVICE_DISCONNECTED_C) ? -1 : t;
}

float readDO() {
  if (!doAktif) return -1;
  int analogValue = analogRead(DO_PIN);
  float voltage = analogValue * (3.3 / 4095.0);
  return constrain(mapFloat(voltage, DO_VOLTAGE_MIN, DO_VOLTAGE_MAX, 0.0, 8.0), 0.0, 8.0);
}

float readPH() {
  if (!phAktif) return -1;
  int analogValue = analogRead(PH_PIN);
  float voltage = analogValue * (3.3 / 4095.0);
  return constrain(mapFloat(voltage, PH_VOLTAGE_MIN, PH_VOLTAGE_MAX, 0.0, 14.0), 0.0, 14.0);
}

float readWeight() {
  if (!beratAktif) return -1;
  if (scale.is_ready()) {
    float weight = scale.get_units(10);
    return weight < 0 ? 0 : weight;
  }
  return -1;
}

float readWaterLevel() {
  if (!levelAktif) return -1;
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2;
  float level = ((TANK_HEIGHT - distance) / TANK_HEIGHT) * 100.0;
  return constrain(level, 0.0, 100.0);
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ====== KONTROL AKTUATOR OTOMATIS ======
void kontrolAktuator(float suhu, float doVal, float level, float berat) {
  // Aerator aktif jika DO < 3
  digitalWrite(AERATOR_PIN, (doVal < 3.0 && doVal >= 0) ? HIGH : LOW);

  // Pompa masuk aktif jika level rendah
  if (level < 50.0 && level >= 0) {
    digitalWrite(POMPA_MASUK_PIN, HIGH);
  } else {
    digitalWrite(POMPA_MASUK_PIN, LOW);
  }

  // Ganti air jika suhu terlalu panas
  if (suhu > 30.0 && suhu < 100.0) {
    digitalWrite(POMPA_KELUAR_PIN, HIGH); delay(5000);
    digitalWrite(POMPA_KELUAR_PIN, LOW);
    digitalWrite(POMPA_MASUK_PIN, HIGH); delay(5000);
    digitalWrite(POMPA_MASUK_PIN, LOW);
  }

  // Motor pakan otomatis jika berat < 50g
  if (pakanAktif && berat < 50.0 && berat >= 0) {
    digitalWrite(PAKAN_PIN, HIGH); delay(1000);
    digitalWrite(PAKAN_PIN, LOW);
  }
}

// ====== KIRIM DATA SENSOR KE MQTT ======
void publishSensorData() {
  if (!client.connected()) reconnect();
  client.loop();

  float suhu = readTemperature();
  float doVal = readDO();
  float ph = readPH();
  float berat = readWeight();
  float level = readWaterLevel();

  kontrolAktuator(suhu, doVal, level, berat); // Kontrol otomatis

  StaticJsonDocument<256> doc;
  doc["kolam"] = pond_id;
  doc["suhu"] = suhu;
  doc["do"] = doVal;
  doc["ph"] = ph;
  doc["berat_pakan"] = berat;
  doc["level_air"] = level;

  char buffer[256];
  serializeJson(doc, buffer);

  if (client.publish(mqtt_topic, buffer)) {
    Serial.println("Data terkirim:");
    Serial.println(buffer);
  } else {
    Serial.println("Gagal kirim data");
  }
}

// ====== LOOP UTAMA ======
void loop() {
  if (WiFi.status() != WL_CONNECTED) setup_wifi();
  if (millis() - lastPublish >= publishInterval) {
    lastPublish = millis();
    publishSensorData();
  }
  client.loop();
}
