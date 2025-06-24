// Library yang diperlukan untuk fungsi WiFi, MQTT, sensor suhu, load cell, dan JSON
#include <WiFi.h> // Library untuk koneksi WiFi
#include <PubSubClient.h> // Library untuk komunikasi MQTT
#include <OneWire.h> // Library untuk komunikasi dengan sensor DS18B20
#include <DallasTemperature.h> // Library untuk membaca suhu dari sensor DS18B20
#include <HX711.h> // Library untuk sensor berat (load cell) HX711
#include <ArduinoJson.h> // Library untuk membuat dan mengelola data JSON

// Pengaturan kredensial WiFi
const char* ssid = ""; // Ganti dengan nama SSID WiFi Anda
const char* password = ""; // Ganti dengan kata sandi WiFi Anda

// Pengaturan MQTT
const char* mqtt_server = "broker.emqx.io"; // Alamat server MQTT
const int mqtt_port = 1883; // Port default untuk MQTT
const char* mqtt_client_id = "225510000"; // ID unik untuk klien MQTT
const char* mqtt_topic = "nugra/data/kolam"; // Topik MQTT untuk mengirim data

// Definisi pin untuk sensor
#define ONE_WIRE_BUS 4 // Pin data untuk sensor suhu DS18B20
#define DO_PIN 34 // Pin analog untuk sensor DO (dissolved oxygen, simulasi)
#define PH_PIN 35 // Pin analog untuk sensor pH (simulasi)
#define HX711_DOUT 5 // Pin data untuk sensor berat HX711
#define HX711_SCK 18 // Pin clock untuk sensor berat HX711
#define TRIG_PIN 13 // Pin trigger untuk sensor ultrasonik
#define ECHO_PIN 12 // Pin echo untuk sensor ultrasonik

// Inisialisasi sensor DS18B20
OneWire oneWire(ONE_WIRE_BUS); // Objek untuk komunikasi OneWire
DallasTemperature sensors(&oneWire); // Objek untuk sensor suhu DS18B20

// Inisialisasi sensor berat HX711
HX711 scale; // Objek untuk sensor berat HX711

// Inisialisasi klien WiFi dan MQTT
WiFiClient espClient; // Objek untuk koneksi WiFi
PubSubClient client(espClient); // Objek untuk komunikasi MQTT

// ID kolam
const int pond_id = 1; // ID kolam, ubah sesuai kolam (misal: 1, 2, 3)

// Nilai kalibrasi sensor
const float DO_VOLTAGE_MIN = 0.0; // Tegangan minimum untuk sensor DO (0 mg/L)
const float DO_VOLTAGE_MAX = 3.3; // Tegangan maksimum untuk sensor DO
const float PH_VOLTAGE_MIN = 0.0; // Tegangan minimum untuk sensor pH (pH 0)
const float PH_VOLTAGE_MAX = 3.3; // Tegangan maksimum untuk sensor pH (pH 14)
const float TANK_HEIGHT = 100.0; // Tinggi tangki dalam cm untuk level air
const float SCALE_CALIBRATION = -7050.0; // Faktor kalibrasi untuk sensor berat HX711

// Fungsi setup: dijalankan sekali saat mikrokontroler dinyalakan
void setup() {
  Serial.begin(115200); // Memulai komunikasi serial dengan baud rate 115200
  
  // Inisialisasi sensor
  sensors.begin(); // Memulai sensor suhu DS18B20
  scale.begin(HX711_DOUT, HX711_SCK); // Memulai sensor berat HX711
  scale.set_scale(SCALE_CALIBRATION); // Mengatur faktor kalibrasi untuk sensor berat
  scale.tare(); // Mengatur ulang sensor berat ke nol
  pinMode(TRIG_PIN, OUTPUT); // Mengatur pin trigger ultrasonik sebagai output
  pinMode(ECHO_PIN, INPUT); // Mengatur pin echo ultrasonik sebagai input
  
  // Menghubungkan ke WiFi
  setup_wifi(); // Memanggil fungsi untuk koneksi WiFi
  
  // Mengatur server MQTT
  client.setServer(mqtt_server, mqtt_port); // Menghubungkan ke server MQTT
}

// Fungsi untuk menghubungkan ke jaringan WiFi
void setup_wifi() {
  delay(10); // Penundaan singkat untuk stabilisasi
  Serial.println("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password); // Memulai koneksi WiFi dengan SSID dan password
  while (WiFi.status() != WL_CONNECTED) { // Loop sampai terhubung
    delay(500);
    Serial.print("."); // Cetak tanda titik untuk menunjukkan proses koneksi
  }
  Serial.println("\nWiFi terhubung");
}

// Fungsi untuk menghubungkan ulang ke server MQTT jika koneksi terputus
void reconnect() {
  while (!client.connected()) { // Loop sampai terhubung ke MQTT
    Serial.print("Mencoba koneksi MQTT...");
    if (client.connect(mqtt_client_id)) { // Jika koneksi berhasil
      Serial.println("terhubung");
    } else { // Jika gagal
      Serial.print("gagal, rc=");
      Serial.print(client.state()); // Cetak kode status kegagalan
      Serial.println(" coba lagi dalam 5 detik");
      delay(5000); // Tunggu 5 detik sebelum mencoba lagi
    }
  }
}

// Fungsi untuk membaca suhu dari sensor DS18B20
float readTemperature() {
  sensors.requestTemperatures(); // Meminta data suhu dari sensor
  float temp = sensors.getTempCByIndex(0); // Membaca suhu dalam Celsius
  if (temp == DEVICE_DISCONNECTED_C) { // Jika sensor tidak terdeteksi
    Serial.println("Gagal membaca suhu");
    return 0.0; // Kembalikan 0 jika gagal
  }
  return temp; // Kembalikan nilai suhu
}

// Fungsi untuk membaca kadar oksigen terlarut (DO, simulasi)
float readDO() {
  int analogValue = analogRead(DO_PIN); // Membaca nilai analog dari pin DO
  float voltage = analogValue * (3.3 / 4095.0); // Konversi ke tegangan (ADC 12-bit)
  // Mengonversi tegangan ke nilai DO (0-8 mg/L, sesuaikan dengan spesifikasi sensor)
  float doValue = mapFloat(voltage, DO_VOLTAGE_MIN, DO_VOLTAGE_MAX, 0.0, 8.0);
  return constrain(doValue, 0.0, 8.0); // Batasi nilai antara 0 dan 8
}

// Fungsi untuk membaca pH (simulasi)
float readPH() {
  int analogValue = analogRead(PH_PIN); // Membaca nilai analog dari pin pH
  float voltage = analogValue * (3.3 / 4095.0); // Konversi ke tegangan (ADC 12-bit)
  // Mengonversi tegangan ke nilai pH (0-14, sesuaikan dengan spesifikasi sensor)
  float phValue = mapFloat(voltage, PH_VOLTAGE_MIN, PH_VOLTAGE_MAX, 0.0, 14.0);
  return constrain(phValue, 0.0, 14.0); // Batasi nilai antara 0 dan 14
}

// Fungsi untuk membaca berat dari sensor HX711
float readWeight() {
  if (scale.is_ready()) { // Jika sensor berat siap
    float weight = scale.get_units(10); // Ambil rata-rata dari 10 pembacaan
    if (weight < 0) weight = 0.0; // Abaikan nilai negatif
    return weight; // Kembalikan nilai berat
  }
  return 0.0; // Kembalikan 0 jika sensor tidak siap
}

// Fungsi untuk membaca level air menggunakan sensor ultrasonik
float readWaterLevel() {
  digitalWrite(TRIG_PIN, LOW); // Matikan pin trigger
  delayMicroseconds(2); // Tunggu 2 mikrosdetik
  digitalWrite(TRIG_PIN, HIGH); // Aktifkan pin trigger
  delayMicroseconds(10); // Tahan selama 10 mikrosdetik
  digitalWrite(TRIG_PIN, LOW); // Matikan kembali pin trigger
  
  long duration = pulseIn(ECHO_PIN, HIGH); // Baca durasi pulsa dari pin echo
  float distance = duration * 0.034 / 2; // Hitung jarak dalam cm
  float level = ((TANK_HEIGHT - distance) / TANK_HEIGHT) * 100.0; // Hitung persentase level air
  return constrain(level, 0.0, 100.0); // Batasi nilai antara 0 dan 100
}

// Fungsi untuk memetakan nilai float dari satu rentang ke rentang lain
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Fungsi untuk mengirim data sensor ke server MQTT
void publishSensorData() {
  if (!client.connected()) { // Jika tidak terhubung ke MQTT
    reconnect(); // Hubungkan ulang
  }
  client.loop(); // Proses komunikasi MQTT
  
  // Membaca nilai dari semua sensor
  float suhu = readTemperature(); // Baca suhu
  float doValue = readDO(); // Baca kadar oksigen terlarut
  float phValue = readPH(); // Baca pH
  float berat_pakan = readWeight(); // Baca berat pakan
  float level_air = readWaterLevel(); // Baca level air
  
  // Membuat objek JSON untuk menyimpan data
  StaticJsonDocument<200> doc; // Objek JSON dengan kapasitas 200 byte
  doc["kolam"] = pond_id; // Tambahkan ID kolam
  doc["suhu"] = suhu; // Tambahkan data suhu
  doc["do"] = doValue; // Tambahkan data DO
  doc["ph"] = phValue; // Tambahkan data pH
  doc["berat_pakan"] = berat_pakan; // Tambahkan data berat pakan
  doc["level_air"] = level_air; // Tambahkan data level air
  
  // Mengonversi JSON ke string
  char buffer[256]; // Buffer untuk menyimpan string JSON
  serializeJson(doc, buffer); // Serialisasi JSON ke buffer
  
  // Mengirim data ke topik MQTT
  if (client.publish(mqtt_topic, buffer)) { // Jika pengiriman berhasil
    Serial.println("Data berhasil dikirim: ");
    Serial.println(buffer); // Cetak data yang dikirim
  } else { // Jika gagal
    Serial.println("Gagal mengirim data");
  }
}

// Fungsi loop: dijalankan berulang-ulang setelah setup
void loop() {
  publishSensorData(); // Kirim data sensor
  delay(10000); // Tunggu 10 detik sebelum pengiriman berikutnya
}