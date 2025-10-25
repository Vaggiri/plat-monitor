// ---------------------------------------------------------------- //
//  ESP32 Plant Monitor to Firebase (CORRECTED VERSION)
//  - Logs /current
//  - Manages /history as an ARRAY (for web app)
//  - Trims history to MAX_HISTORY_POINTS
// ---------------------------------------------------------------- //

#include <WiFi.h>
#include <time.h>                 
#include "Firebase_ESP_Client.h"  
#include "addons/TokenHelper.h"   
#include "addons/RTDBHelper.h"    
#include <DHT.h>                  

// --- User Configuration ---
// Wi-Fi Credentials
#define WIFI_SSID "giri"
#define WIFI_PASSWORD "11223344"

// Firebase Config
#define API_KEY "AIzaSyDci_KmZqZhX1-4TYe1eElbhulAeJZJ7a4"
#define DATABASE_URL "https://plant-6e094-default-rtdb.firebaseio.com"
#define DATABASE_SECRET "he5qGAK4IQJbm7sRj3Lr9Xn88o6BeQd8VY1bmMpO"

// Sensor Pins
#define DHT_PIN 32      
#define DHT_TYPE DHT11
#define MOISTURE_PIN 34  // Analog input

// LED Pins (Alert LEDs)
#define LED_TEMP 25      
#define LED_HUMID 26     
#define LED_MOIST 27     

// Thresholds
#define TEMP_THRESHOLD_F 90.0     
#define HUMID_THRESHOLD 80.0      
#define MOISTURE_LOW_THRESHOLD 30 

// Logging settings
const long LOG_INTERVAL_MS = 5000; // log every 5 sec
const int MAX_HISTORY_POINTS = 50; // Max points to keep in history array

// --- Global ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
DHT dht(DHT_PIN, DHT_TYPE);
unsigned long lastLogTime = 0;

// --- Helper: Get Timestamp ---
// ** FIXED: Returns milliseconds, which JavaScript needs **
unsigned long long getTimestamp() {
  time_t now = time(nullptr);
  // Return as milliseconds (which JavaScript Date() objects use)
  return (unsigned long long)now * 1000ULL;
}


// --- Helper: Update History Array ---
// ** FIXED: This function now manages an array [ts, val] and trims it **
// --- Helper: Update History Array ---
// ** FIXED: This function now uses the correct library syntax **
void updateHistoryArray(const char* key, unsigned long long ts, float value) {
  String path = "/history/";
  path += key;
  FirebaseJsonArray historyArray;

  Serial.printf("Updating history for %s... ", key);

  // 1. Get the current history array from Firebase
  if (Firebase.RTDB.getJSON(&fbdo, path.c_str())) {
    // **FIX 1: Assign the returned array to your variable**
    historyArray = fbdo.jsonArray(); // Parse the existing array
  } else {
    Serial.print(" (No existing data, creating new array) ");
    // If it fails (e.g., first time), we just use the empty historyArray object
  }

  // 2. Create the new data point as an array: [timestamp, value]
  FirebaseJsonArray newPoint;
  newPoint.add(ts);
  newPoint.add(value);

  // 3. Add the new point to the end of the main array
  historyArray.add(newPoint);

  // 4. **CRITICAL:** Trim the array if it's too long
  while (historyArray.size() > MAX_HISTORY_POINTS) {
    historyArray.remove(0); // Remove the oldest data point (from the front)
  }

  // 5. Write the modified array back to Firebase
  // **FIX 2: Use setArray for a FirebaseJsonArray**
  if (Firebase.RTDB.setArray(&fbdo, path.c_str(), &historyArray)) {
    Serial.println("OK");
  } else {
    Serial.printf("Failed to write history %s : %s\n", path.c_str(), fbdo.errorReason().c_str());
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Plant Monitor] Booting...");

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected!");

  // Init LEDs
  pinMode(LED_TEMP, OUTPUT);
  pinMode(LED_HUMID, OUTPUT);
  pinMode(LED_MOIST, OUTPUT);
  digitalWrite(LED_TEMP, LOW);
  digitalWrite(LED_HUMID, LOW);
  digitalWrite(LED_MOIST, LOW);

  // Init DHT22 with internal pull-up
  pinMode(DHT_PIN, INPUT_PULLUP);
  dht.begin();
  delay(3000); // wait for DHT to stabilize

  // Init Time + Firebase
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for time sync");
  unsigned long start = millis();
  while (time(nullptr) < 1600000000UL && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  // Firebase config using legacy token
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET; 
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Setup complete.");
}

// --- Loop ---
void loop() {
  if (millis() - lastLogTime > LOG_INTERVAL_MS) {
    lastLogTime = millis();
    if (Firebase.ready()) {
      logSensorData();
    } else {
      Serial.println("Firebase not ready yet.");
      // Don't log if Firebase isn't ready, it will fail
    }
  }
}

// --- Sensor + LED Logic ---
void logSensorData() {
  float h = dht.readHumidity();
  float t_f = dht.readTemperature(true);
  int m_raw = analogRead(MOISTURE_PIN);
  const int MOISTURE_MIN_RAW = 1000;
  const int MOISTURE_MAX_RAW = 4095;
  int m_percent = constrain(map(m_raw, MOISTURE_MAX_RAW, MOISTURE_MIN_RAW, 0, 100), 0, 100);

  if (isnan(h) || isnan(t_f)) {
    Serial.println("DHT read failed!");
    return;
  }

  // ** FIXED: ts is now in milliseconds **
  unsigned long long ts = getTimestamp();

  Serial.printf("Temp: %.1f°F | Humidity: %.1f%% | Moisture: %d%% | ts: %llu\n", t_f, h, m_percent, ts);

  // LED alerts
  digitalWrite(LED_TEMP, (t_f > TEMP_THRESHOLD_F) ? HIGH : LOW);
  digitalWrite(LED_HUMID, (h > HUMID_THRESHOLD) ? HIGH : LOW);
  digitalWrite(LED_MOIST, (m_percent < MOISTURE_LOW_THRESHOLD) ? HIGH : LOW);

  // Firebase /current
  FirebaseJson currentData;
  currentData.set("temp", t_f);
  currentData.set("humidity", h);
  currentData.set("moisture", m_percent);
  currentData.set("ts", ts); // This timestamp is just for reference

  if (!Firebase.RTDB.setJSON(&fbdo, "/current", &currentData)) {
    Serial.printf("Failed to set /current : %s\n", fbdo.errorReason().c_str());
  }

  // Update history (This now uses the new, correct function)
  updateHistoryArray("temp", ts, t_f);
  updateHistoryArray("humidity", ts, h);
  updateHistoryArray("moisture", ts, m_percent);
}