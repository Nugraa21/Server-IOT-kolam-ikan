const mqtt = require('mqtt');
const admin = require('firebase-admin');

// ===== Firebase Service Account Langsung di sini =====
const serviceAccount = {
  type: "service_account",
  project_id: "iotkolam-c5bf2",
  private_key_id: "your_private_key_id",
  private_key: `-----BEGIN PRIVATE KEY-----MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBK......-----END PRIVATE KEY-----\n`,
  client_email: "firebase-adminsdk-xxxxx@iotkolam-c5bf2.iam.gserviceaccount.com",
  client_id: "your_client_id",
  auth_uri: "https://accounts.google.com/o/oauth2/auth",
  token_uri: "https://oauth2.googleapis.com/token",
  auth_provider_x509_cert_url: "https://www.googleapis.com/oauth2/v1/certs",
  client_x509_cert_url: "https://www.googleapis.com/robot/v1/metadata/x509/firebase-adminsdk-xxxxx%40iotkolam-c5bf2.iam.gserviceaccount.com"
};


// ===== Inisialisasi Firebase =====
admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  projectId: 'iotkolam-c5bf2',
});
const db = admin.firestore();

// ===== Konfigurasi MQTT langsung di sini =====
const mqttOptions = {
  host: 'broker.emqx.io',
  port: 1883,
  clientId: 'nodejs-server-225510017',
};

const MQTT_TOPIC = 'nugra/data/kolam';

// ===== Koneksi MQTT =====
const client = mqtt.connect(mqttOptions);

client.on('connect', () => {
  console.log('Terhubung ke broker MQTT');
  client.subscribe(MQTT_TOPIC, (err) => {
    if (err) {
      console.error('Gagal berlangganan ke topik:', err.message);
    } else {
      console.log(`Berlangganan ke topik: ${MQTT_TOPIC}`);
    }
  });
});

client.on('message', (topic, message) => {
  try {
    const data = JSON.parse(message.toString());
    console.log('Data diterima:', data);

    if (!data.kolam) {
      console.error('Data ga valid: kolam ga ada');
      return;
    }

    const pondId = `pond_${data.kolam}`;
    const sensorData = {
      suhu: parseFloat(data.suhu) || 0.0,
      do: parseFloat(data.do) || 0.0,
      ph: parseFloat(data.ph) || 0.0,
      berat_pakan: parseFloat(data.berat_pakan) || 0.0,
      level_air: parseFloat(data.level_air) || 0.0,
      timestamp: admin.firestore.FieldValue.serverTimestamp(),
    };

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

client.on('error', (err) => {
  console.error('Error MQTT:', err.message);
});

client.on('close', () => {
  console.log('Putus dari broker MQTT');
});
