require('dotenv').config();
const mqtt = require('mqtt');
const admin = require('firebase-admin');

// Inisialisasi Firebase Admin SDK
const serviceAccount = require(process.env.FIREBASE_SERVICE_ACCOUNT_PATH);
admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  projectId: 'iotkolam-c5bf2',
});
const db = admin.firestore();

// Konfigurasi MQTT
const mqttOptions = {
  host: process.env.MQTT_BROKER,
  port: parseInt(process.env.MQTT_PORT),
  clientId: process.env.MQTT_CLIENT_ID,
};

// Koneksi ke broker MQTT
const client = mqtt.connect(mqttOptions);

// Saat terhubung
client.on('connect', () => {
  console.log('Terhubung ke broker MQTT');
  client.subscribe(process.env.MQTT_TOPIC, (err) => {
    if (err) {
      console.error('Gagal berlangganan ke topik:', err.message);
    } else {
      console.log(`Berlangganan ke topik: ${process.env.MQTT_TOPIC}`);
    }
  });
});

// Saat menerima pesan
client.on('message', (topic, message) => {
  try {
    // Parse JSON
    const data = JSON.parse(message.toString());
    console.log('Data diterima:', data);

    // Cek kolam
    if (!data.kolam) {
      console.error('Data ga valid: kolam ga ada');
      return;
    }

    // Siapin data buat Firestore
    const pondId = `pond_${data.kolam}`;
    const sensorData = {
      suhu: parseFloat(data.suhu) || 0.0,
      do: parseFloat(data.do) || 0.0,
      ph: parseFloat(data.ph) || 0.0,
      berat_pakan: parseFloat(data.berat_pakan) || 0.0,
      level_air: parseFloat(data.level_air) || 0.0,
      timestamp: admin.firestore.FieldValue.serverTimestamp(),
    };

    // Simpen ke Firestore
    db.collection('ponds')
      .doc(pondId)
      .collection('sensor_data')
      .add(sensorData)
      .then(() => {
        console.log(`Data disimpen ke Firestore buat ${pondId}`);
      })
      .catch((error) => {
        console.error('Gagal simpen ke Firestore:', error.message);
      });
  } catch (error) {
    console.error('Gagal proses data MQTT:', error.message);
  }
});

// Error MQTT
client.on('error', (err) => {
  console.error('Error MQTT:', err.message);
});

// Koneksi putus
client.on('close', () => {
  console.log('Putus dari broker MQTT');
});