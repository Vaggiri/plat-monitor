// ----------------------------------------------------------------
//  ESP32 Plant Monitor to Firebase Realtime Database
//  Reads Temp, Humidity, and Moisture
//  Logs to /current and manages /history array
// ----------------------------------------------------------------

// --- 1. Include Libraries ---
#include <WiFi.h>
#include <time.h>                 // For NTP time
#include <Firebase_ESP_Client.h>  // ✅ new library
#include "DHT.h"

// --- 2. WiFi & Firebase Setup ---
#define WIFI_SSID "giri"
#define WIFI_PASSWORD "11223344"
#define API_KEY "AIzaSyDci_KmZqZhX1-4TYe1eElbhulAeJZJ7a4"
#define DATABASE_URL "https://your-database-url.firebaseio.com/" // change this

// --- 3. DHT & Moisture Sensor Pins ---
#define DHTPIN 4
#define DHTTYPE DHT11
#define MOISTURE_PIN 34

// --- 4. Global Objects ---
DHT dht(DHTPIN, DHTTYPE);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// --- 5. Function Declarations ---
unsigned long long getTimestamp();
void updateHistoryArray(String param, unsigned long long ts, float value);
void logSensorData();

// --- 6. Setup ---
void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi!");

  // Configure time for IST (GMT+5:30)
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("⏰ Time synced via NTP");

  // Firebase setup
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Optional: anonymous sign-in
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("✅ Firebase sign-up success");
  } else {
    Serial.printf("❌ Firebase sign-up failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("🔥 Firebase connected!");
}

// --- 7. Main Loop ---
void loop() {
  logSensorData();
  delay(5000); // log every 5 seconds
}

// --- 8. Function Definitions ---

// 🕒 Get real-time timestamp from NTP
unsigned long long getTimestamp() {
  time_t now;
  time(&now);
  return static_cast<unsigned long long>(now);
}

// 📜 Update history array in Firebase
void updateHistoryArray(String param, unsigned long long ts, float value) {
  String path = "/history/" + param + "/" + String(ts);
  if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), value)) {
    Serial.println("✅ " + param + " logged to history");
  } else {
    Serial.println("❌ Failed to log " + param);
    Serial.println(fbdo.errorReason());
  }
}

// 🌿 Log current sensor readings
void logSensorData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int moistureValue = analogRead(MOISTURE_PIN);
  float moisturePercent = map(moistureValue, 4095, 0, 0, 100);

  if (isnan(t) || isnan(h)) {
    Serial.println("⚠️ Failed to read from DHT sensor!");
    return;
  }

  unsigned long long ts = getTimestamp();

  // Log current values
  Firebase.RTDB.setFloat(&fbdo, "/current/temp", t);
  Firebase.RTDB.setFloat(&fbdo, "/current/humidity", h);
  Firebase.RTDB.setFloat(&fbdo, "/current/moisture", moisturePercent);
  Firebase.RTDB.setInt(&fbdo, "/current/timestamp", ts);

  // Log history
  updateHistoryArray("temp", ts, t);
  updateHistoryArray("humidity", ts, h);
  updateHistoryArray("moisture", ts, moisturePercent);

  Serial.println("🌡️ Temp: " + String(t) + "°C | 💧 Humidity: " + String(h) + "% | 🌱 Moisture: " + String(moisturePercent) + "%");
}
