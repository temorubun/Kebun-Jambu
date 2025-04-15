#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// Define pins
const int PUMP_PIN1 = 2;
const int PUMP_PIN2 = 4;
const int MOISTURE_PIN1 = 32;
const int MOISTURE_PIN2 = 33;

// Calibration values for soil moisture sensors
const int DRY_VALUE1 = 2559;   // Kering untuk sensor 1
const int WET_VALUE1 = 1123;   // Basah untuk sensor 1
const int DRY_VALUE2 = 2559;   // Kering untuk sensor 2
const int WET_VALUE2 = 1123;   // Basah untuk sensor 2

// WiFi credentials  
const char* ssid = "V20430";
const char* password = "123456789";

// Firebase configuration
#define API_KEY "AIzaSyAOJ--IeEt2l9x9MuPb6DAZHFbSKmeaARQ"
#define DATABASE_URL "https://kebun-jambu-default-rtdb.asia-southeast1.firebasedatabase.app"
#define STORAGE_BUCKET "kebun-jambu.firebasestorage.app"
#define PROJECT_ID "kebun-jambu"

// Firebase user credentials
#define USER_EMAIL "user@iot.com"
#define USER_PASSWORD "user123"

// Define Firebase Data objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variables to store moisture threshold and current moisture levels
float moistureThreshold1 = 60.0;
float moistureThreshold2 = 60.0;
float currentMoistureLevel1 = 0.0;
float currentMoistureLevel2 = 0.0;

// Function to calculate moisture percentage with calibration
float calculateMoisturePercentage(int sensorValue, int dryValue, int wetValue) {
  int moisturePercentage = map(sensorValue, dryValue, wetValue, 0, 100);
  moisturePercentage = constrain(moisturePercentage, 0, 100);
  return moisturePercentage;
}

// Function to update pump status in Firebase
void updatePumpStatus(const char* pumpPath, const char* status) {
  if (Firebase.RTDB.setString(&fbdo, pumpPath, status)) {
    Serial.print("Updated pump status at ");
    Serial.print(pumpPath);
    Serial.print(" to: ");
    Serial.println(status);
  } else {
    Serial.print("Failed to update pump status at ");
    Serial.print(pumpPath);
    Serial.print(". Error: ");
    Serial.println(fbdo.errorReason().c_str());
  }
}

// Function to control pump based on moisture threshold
void controlPump(int pumpPin, float moistureLevel, float threshold, const char* pumpPath) {
  if (moistureLevel < threshold) {
    digitalWrite(pumpPin, HIGH);
    updatePumpStatus(pumpPath, "on");
    Serial.print("Pump turned ON. Moisture level ");
    Serial.print(moistureLevel);
    Serial.print(" is below threshold ");
    Serial.println(threshold);
  } else {
    digitalWrite(pumpPin, LOW);
    updatePumpStatus(pumpPath, "off");
    Serial.print("Pump turned OFF. Moisture level ");
    Serial.print(moistureLevel);
    Serial.print(" is above threshold ");
    Serial.println(threshold);
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize pump pins as output
  pinMode(PUMP_PIN1, OUTPUT);
  pinMode(PUMP_PIN2, OUTPUT);
  digitalWrite(PUMP_PIN1, LOW);
  digitalWrite(PUMP_PIN2, LOW);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  
  // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  // Sign in with email/password
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  
  // Initialize Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();
    delay(5000);
    return;
  }

  // Check Firebase connection
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready. Reconnecting...");
    Firebase.begin(&config, &auth);
    delay(5000);
    return;
  }

  // Read current moisture values from sensors
  int rawSensorValue1 = analogRead(MOISTURE_PIN1);
  int rawSensorValue2 = analogRead(MOISTURE_PIN2);
  
  currentMoistureLevel1 = calculateMoisturePercentage(rawSensorValue1, DRY_VALUE1, WET_VALUE1);
  currentMoistureLevel2 = calculateMoisturePercentage(rawSensorValue2, DRY_VALUE2, WET_VALUE2);

  // Update moisture levels in Firebase
  Firebase.RTDB.setFloat(&fbdo, "/Kebun-Jambu/Sensor-Kelembaban-tanah/kelembaban-tanah1", currentMoistureLevel1);
  Firebase.RTDB.setFloat(&fbdo, "/Kebun-Jambu/Sensor-Kelembaban-tanah/kelembaban-tanah2", currentMoistureLevel2);

  // Read moisture thresholds from Firebase (realtime)
  if (Firebase.RTDB.getFloat(&fbdo, "/Kebun-Jambu/Threshold/moisture_threshold1")) {
    moistureThreshold1 = fbdo.floatData();
  }
  
  if (Firebase.RTDB.getFloat(&fbdo, "/Kebun-Jambu/Threshold/moisture_threshold2")) {
    moistureThreshold2 = fbdo.floatData();
  }

  // Control pumps based on realtime moisture thresholds
  controlPump(PUMP_PIN1, currentMoistureLevel1, moistureThreshold1, "/Kebun-Jambu/pompa/pompa1");
  controlPump(PUMP_PIN2, currentMoistureLevel2, moistureThreshold2, "/Kebun-Jambu/pompa/pompa2");

  // Print debug information
  Serial.println("\n----- Moisture Status -----");
  Serial.print("Moisture Level 1: ");
  Serial.print(currentMoistureLevel1);
  Serial.print("% (Threshold: ");
  Serial.print(moistureThreshold1);
  Serial.println("%)");
  
  Serial.print("Moisture Level 2: ");
  Serial.print(currentMoistureLevel2);
  Serial.print("% (Threshold: ");
  Serial.print(moistureThreshold2);
  Serial.println("%)");

  delay(1); // Wait 5 seconds before next reading
}
