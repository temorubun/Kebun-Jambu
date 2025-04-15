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

// Track previous moisture state and pump status
int previousMoistureState1 = -1;
int previousMoistureState2 = -1;
String initialPumpStatus1 = "off";
String initialPumpStatus2 = "off";

// Function to calculate moisture percentage with calibration
float calculateMoisturePercentage(int sensorValue, int dryValue, int wetValue) {
  int moisturePercentage = map(sensorValue, dryValue, wetValue, 0, 100);
  moisturePercentage = constrain(moisturePercentage, 0, 100);
  return moisturePercentage;
}

// Function to set initial pump state
void setInitialPumpState() {
  // Retrieve initial pump status from Firebase
  if (Firebase.RTDB.getString(&fbdo, "/Kebun-Jambu/pompa/pompa1")) {
    initialPumpStatus1 = fbdo.stringData();
    Serial.print("Initial Pump 1 Status from Firebase: ");
    Serial.println(initialPumpStatus1);
  } else {
    Serial.println("Failed to read initial pump status 1 from Firebase!");
    Serial.println(fbdo.errorReason().c_str());
    initialPumpStatus1 = "off"; // Default to off if read fails
  }

  if (Firebase.RTDB.getString(&fbdo, "/Kebun-Jambu/pompa/pompa2")) {
    initialPumpStatus2 = fbdo.stringData();
    Serial.print("Initial Pump 2 Status from Firebase: ");
    Serial.println(initialPumpStatus2);
  } else {
    Serial.println("Failed to read initial pump status 2 from Firebase!");
    Serial.println(fbdo.errorReason().c_str());
    initialPumpStatus2 = "off"; // Default to off if read fails
  }

  // Set pump pins based on retrieved status
  digitalWrite(PUMP_PIN1, initialPumpStatus1 == "on" ? HIGH : LOW);
  digitalWrite(PUMP_PIN2, initialPumpStatus2 == "on" ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize pump pins as output
  pinMode(PUMP_PIN1, OUTPUT);
  pinMode(PUMP_PIN2, OUTPUT);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    
    // Configure Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    
    // Sign in with email/password
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    
    // Initialize Firebase with sign in
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Check Firebase connection with timeout
    unsigned long timeout = millis();
    while (!Firebase.ready() && millis() - timeout < 10000) {
      Serial.println("Waiting for Firebase connection...");
      delay(1000);
    }

    if (Firebase.ready()) {
      Serial.println("Connected to Firebase successfully!");
      
      // Set initial pump state after successful Firebase connection
      setInitialPumpState();
    } else {
      Serial.println("Failed to connect to Firebase - timeout!");
      
      // Default to OFF if Firebase connection fails
      digitalWrite(PUMP_PIN1, LOW);
      digitalWrite(PUMP_PIN2, LOW);
    }
  } else {
    Serial.println("\nFailed to connect to WiFi!");
    
    // Default to OFF if WiFi connection fails
    digitalWrite(PUMP_PIN1, LOW);
    digitalWrite(PUMP_PIN2, LOW);
  }
}

void updatePumpStatus(const char* newStatus, const char* path) {
  if (Firebase.RTDB.setString(&fbdo, path, newStatus)) {
    char msg[50];
    snprintf(msg, sizeof(msg), "Successfully updated pump status to: %s", newStatus);
    Serial.println(msg);
  } else {
    Serial.println("Failed to update pump status!");
    Serial.println(fbdo.errorReason().c_str());
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost! Attempting to reconnect...");
    WiFi.begin(ssid, password);
    delay(5000);
    return;
  }
  
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready! Attempting to reconnect...");
    // Full reconnect attempt
    Firebase.begin(&config, &auth);
    unsigned long timeout = millis();
    while (!Firebase.ready() && millis() - timeout < 10000) {
      Serial.println("Waiting for Firebase reconnection...");
      delay(1000);
    }
    if (!Firebase.ready()) {
      Serial.println("Firebase reconnection failed - timeout!");
      delay(5000);
      return;
    }
  }

  Serial.println("\n----- Reading Firebase Data -----");
  
  // Read soil moisture data from pin 1
  int rawSensorValue1 = analogRead(MOISTURE_PIN1);
  float soilMoisturePercent1 = calculateMoisturePercentage(rawSensorValue1, DRY_VALUE1, WET_VALUE1);
  
  if (Firebase.RTDB.setFloat(&fbdo, "/Kebun-Jambu/Sensor-Kelembaban-tanah/kelembaban-tanah1", soilMoisturePercent1)) {
    char msg[50];
    snprintf(msg, sizeof(msg), "Successfully updated Soil Moisture Level 1 to: %.1f%%", soilMoisturePercent1);
    Serial.println(msg);
    Serial.print("Raw Sensor Value 1: ");
    Serial.println(rawSensorValue1);
  } else {
    Serial.println("Failed to update Soil Moisture Level 1!");
    Serial.println(fbdo.errorReason().c_str());
  }
  
  // Calculate current moisture state
  int currentMoistureState1 = (soilMoisturePercent1 < 60) ? 0 : 1;
  char msg[50];
  snprintf(msg, sizeof(msg), "Moisture State 1: %d (0=Dry, 1=Wet)", currentMoistureState1);
  Serial.println(msg);
  
  // Only update pump status if moisture state has changed
  if (currentMoistureState1 != previousMoistureState1) {
    if (currentMoistureState1 == 0) {
      updatePumpStatus("on", "/Kebun-Jambu/pompa/pompa1");
    } else {
      updatePumpStatus("off", "/Kebun-Jambu/pompa/pompa1");
    }
    previousMoistureState1 = currentMoistureState1;
  }

  // Get current pump status
  if (Firebase.RTDB.getString(&fbdo, "/Kebun-Jambu/pompa/pompa1")) {
    String pumpStatus1 = fbdo.stringData();
    snprintf(msg, sizeof(msg), "Current Pump Status 1: %s", pumpStatus1.c_str());
    Serial.println(msg);
    
    // Control pump based on status
    if (pumpStatus1 == "on") {
      digitalWrite(PUMP_PIN1, HIGH);
      Serial.println("Pump 1 is turned ON");
    } else {
      digitalWrite(PUMP_PIN1, LOW);
      Serial.println("Pump 1 is turned OFF");
    }
  } else {
    Serial.println("Failed to read pump status 1 from Firebase!");
    Serial.println(fbdo.errorReason().c_str());
  }

  // Repeat the process for the second soil moisture sensor and pump
  int rawSensorValue2 = analogRead(MOISTURE_PIN2);
  float soilMoisturePercent2 = calculateMoisturePercentage(rawSensorValue2, DRY_VALUE2, WET_VALUE2);
  
  if (Firebase.RTDB.setFloat(&fbdo, "/Kebun-Jambu/Sensor-Kelembaban-tanah/kelembaban-tanah2", soilMoisturePercent2)) {
    snprintf(msg, sizeof(msg), "Successfully updated Soil Moisture Level 2 to: %.1f%%", soilMoisturePercent2);
    Serial.println(msg);
    Serial.print("Raw Sensor Value 2: ");
    Serial.println(rawSensorValue2);
  } else {
    Serial.println("Failed to update Soil Moisture Level 2!");
    Serial.println(fbdo.errorReason().c_str());
  }
  
  // Calculate current moisture state
  int currentMoistureState2 = (soilMoisturePercent2 < 60) ? 0 : 1;
  snprintf(msg, sizeof(msg), "Moisture State 2: %d (0=Dry, 1=Wet)", currentMoistureState2);
  Serial.println(msg);
  
  // Only update pump status if moisture state has changed
  if (currentMoistureState2 != previousMoistureState2) {
    if (currentMoistureState2 == 0) {
      updatePumpStatus("on", "/Kebun-Jambu/pompa/pompa2");
    } else {
      updatePumpStatus("off", "/Kebun-Jambu/pompa/pompa2");
    }
    previousMoistureState2 = currentMoistureState2;
  }

  // Get current pump status
  if (Firebase.RTDB.getString(&fbdo, "/Kebun-Jambu/pompa/pompa2")) {
    String pumpStatus2 = fbdo.stringData();
    snprintf(msg, sizeof(msg), "Current Pump Status 2: %s", pumpStatus2.c_str());
    Serial.println(msg);
    
    // Control pump based on status
    if (pumpStatus2 == "on") {
      digitalWrite(PUMP_PIN2, HIGH);
      Serial.println("Pump 2 is turned ON");
    } else {
      digitalWrite(PUMP_PIN2, LOW);
      Serial.println("Pump 2 is turned OFF");
    }
  } else {
    Serial.println("Failed to read pump status 2 from Firebase!");
    Serial.println(fbdo.errorReason().c_str());
  }

  delay(1000); // Delay for 1 second before reading and sending data again
}
