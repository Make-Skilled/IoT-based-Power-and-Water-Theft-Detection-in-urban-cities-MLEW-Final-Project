#include <WiFi.h>
#include <WiFiClient.h>
#include <ThingSpeak.h>
#include <EEPROM.h>

// WiFi Credentials
char ssid[] = "Act";
char pass[] = "Madhumakeskilled";

// ThingSpeak API
WiFiClient client;
unsigned long channelid = 2853641;  
char thingSpeakWriteAPIKey[] = "3O9LY8ND0MM0VF9D";  

// Sensor Pin
#define CURRENT_SENSOR_PIN  35   // ACS712 Sensor (A0)

// Constants for ACS712
#define ASSUMED_VOLTAGE 230.0    // Assume constant voltage (230V for AC mains)
#define ACS_SENSITIVITY 0.066    // For ACS712-30A (0.066V/A)
float ACS_OFFSET = 0.0;         // Will be calibrated

// Variables for power measurement
float currentValue = 0.0;
float powerValue = 0.0;
float kWh = 0.0;

// Timing variables
unsigned long lastPrintTime = 0;
unsigned long lastPowerAlertTime = 0;
unsigned long lastThingSpeakUpdate = 0;
#define PRINT_INTERVAL 5000  // Print every 5 seconds

// Enhanced Theft Detection Parameters
#define MOVING_AVERAGE_WINDOW 10
float powerReadings[MOVING_AVERAGE_WINDOW];
int readIndex = 0;
float powerMovingAverage = 0.0;

// Time-based consumption patterns (24 hours, readings every hour)
#define PATTERN_SAMPLES 24
float normalPatterns[PATTERN_SAMPLES];
float patternThresholds[PATTERN_SAMPLES];
unsigned long lastPatternUpdate = 0;

// Theft Detection Timing
#define THEFT_CHECK_INTERVAL 1000   // Check every 1 second (1000ms)
#define CHANGE_DETECTION_SAMPLES 5   // Reduced samples for faster response
unsigned long lastTheftCheckTime = 0;
float previousPowerReadings[CHANGE_DETECTION_SAMPLES];
int sampleIndex = 0;

// Theft Detection Thresholds
float powerThreshold = 100.0;           // Base power threshold
float suddenChangeThreshold = 20.0;     // 20% change triggers alert
float patternDeviationThreshold = 20.0; // 20% deviation from normal
#define MAX_ANOMALIES 3  // Adjusted for 1-second checks
int consecutiveAnomalies = 0;  // Track consecutive anomalies

// Alert Status
bool isTheftAlertActive = false;

// Change Detection Variables
float lastPowerAverage = 0.0;
bool isFirstCheck = true;

// Update Moving Average
void updateMovingAverage(float newPower) {
  powerMovingAverage -= powerReadings[readIndex] / MOVING_AVERAGE_WINDOW;
  powerReadings[readIndex] = newPower;
  powerMovingAverage += newPower / MOVING_AVERAGE_WINDOW;
  readIndex = (readIndex + 1) % MOVING_AVERAGE_WINDOW;
}

// Print Power Consumption Values
void printPowerConsumption() {
    Serial.println("\n=== Power Consumption Report ===");
    Serial.println("--------------------------------");
    Serial.print("Current:     "); 
    Serial.print(currentValue, 2);
    Serial.println(" A");
    
    Serial.print("Voltage:     ");
    Serial.print(ASSUMED_VOLTAGE, 1);
    Serial.println(" V");
    
    Serial.print("Power:       ");
    Serial.print(powerValue, 2);
    Serial.println(" W");
    
    Serial.print("Energy:      ");
    Serial.print(kWh, 3);
    Serial.println(" kWh");
    
    Serial.print("Avg Power:   ");
    Serial.print(powerMovingAverage, 2);
    Serial.println(" W");
    Serial.println("--------------------------------\n");
}

// Measure Current & Calculate Power
void measurePower() {
    float totalCurrent = 0;
    int numSamples = 100;  // Noise reduction

    for (int i = 0; i < numSamples; i++) {
        float currentSensorValue = analogRead(CURRENT_SENSOR_PIN);
        float currentVoltage = (currentSensorValue / 4095.0) * 3.3;
        totalCurrent += (currentVoltage - ACS_OFFSET) / ACS_SENSITIVITY;
        delayMicroseconds(100);
    }
    currentValue = totalCurrent / numSamples;

    // Calculate Power (P = V * I)
    powerValue = ASSUMED_VOLTAGE * currentValue;
    updateMovingAverage(powerValue);
    
    // Update kWh
    unsigned long currentMillis = millis();
    kWh += powerValue * (currentMillis - lastPowerAlertTime) / 3600000.0;
    lastPowerAlertTime = currentMillis;

    // Print values at regular intervals
    if (currentMillis - lastPrintTime >= PRINT_INTERVAL) {
        printPowerConsumption();
        lastPrintTime = currentMillis;
    }
}

// Update Normal Consumption Pattern
void updateConsumptionPattern() {
  int currentHour = (millis() / 3600000) % 24;
  
  // Update pattern for current hour
  normalPatterns[currentHour] = (normalPatterns[currentHour] * 0.7) + (powerMovingAverage * 0.3);
  patternThresholds[currentHour] = normalPatterns[currentHour] * (1 + patternDeviationThreshold/100.0);
}

// Enhanced Change Detection
bool detectSignificantChange() {
    float currentAverage = 0.0;
    float previousAverage = 0.0;
    
    // Calculate current average
    for(int i = 0; i < CHANGE_DETECTION_SAMPLES/2; i++) {
        currentAverage += previousPowerReadings[(sampleIndex - i + CHANGE_DETECTION_SAMPLES) % CHANGE_DETECTION_SAMPLES];
    }
    currentAverage /= (CHANGE_DETECTION_SAMPLES/2);
    
    // Calculate previous average
    for(int i = CHANGE_DETECTION_SAMPLES/2; i < CHANGE_DETECTION_SAMPLES; i++) {
        previousAverage += previousPowerReadings[(sampleIndex - i + CHANGE_DETECTION_SAMPLES) % CHANGE_DETECTION_SAMPLES];
    }
    previousAverage /= (CHANGE_DETECTION_SAMPLES/2);
    
    // Calculate percentage change
    float percentageChange = abs(currentAverage - previousAverage) / previousAverage * 100.0;
    
    if(percentageChange > suddenChangeThreshold) {
        Serial.print("⚠️ Significant change detected: ");
        Serial.print(percentageChange);
        Serial.println("% change in power consumption");
        return true;
    }
    return false;
}

// Enhanced Theft Detection
bool detectTheft() {
    unsigned long currentMillis = millis();
    int currentHour = (currentMillis / 3600000) % 24;
    bool anomalyDetected = false;
    
    // Store current power reading
    previousPowerReadings[sampleIndex] = powerValue;
    sampleIndex = (sampleIndex + 1) % CHANGE_DETECTION_SAMPLES;

    // Skip first few readings to collect enough samples
    if(isFirstCheck) {
        if(sampleIndex == CHANGE_DETECTION_SAMPLES-1) {
            isFirstCheck = false;
        }
        return false;
    }

    // 1. Check for significant changes
    if(detectSignificantChange()) {
        anomalyDetected = true;
        Serial.println("⚠️ Abnormal power change pattern detected!");
    }

    // 2. Check absolute power threshold
    if (powerMovingAverage > powerThreshold) {
        anomalyDetected = true;
        Serial.println("⚠️ High power consumption detected!");
        Serial.print("Current Power: ");
        Serial.print(powerMovingAverage);
        Serial.print("W, Threshold: ");
        Serial.print(powerThreshold);
        Serial.println("W");
    }

    // 3. Check pattern deviation
    if (powerMovingAverage > patternThresholds[currentHour]) {
        anomalyDetected = true;
        Serial.println("⚠️ Unusual consumption pattern for this time!");
        Serial.print("Expected max: ");
        Serial.print(patternThresholds[currentHour]);
        Serial.print("W, Current: ");
        Serial.print(powerMovingAverage);
        Serial.println("W");
    }

    // Update consecutive anomalies with more detailed logging
    if (anomalyDetected) {
        consecutiveAnomalies++;
        Serial.print("⚠️ Anomaly count: ");
        Serial.print(consecutiveAnomalies);
        Serial.print("/");
        Serial.println(MAX_ANOMALIES);
        
        if (consecutiveAnomalies >= MAX_ANOMALIES && !isTheftAlertActive) {
            isTheftAlertActive = true;
            Serial.println("🚨 POWER THEFT ALERT! Multiple anomalies confirmed!");
            Serial.println("Details:");
            Serial.print("- Average Power: ");
            Serial.print(powerMovingAverage);
            Serial.println("W");
            Serial.print("- Current Reading: ");
            Serial.print(powerValue);
            Serial.println("W");
            
            // Send alert to ThingSpeak
            ThingSpeak.setField(4, 1); // Alert field
            return true;
        }
    } else {
        if(consecutiveAnomalies > 0) {
            Serial.println("✅ Normal power consumption resumed");
        }
        consecutiveAnomalies = 0;
        if (isTheftAlertActive) {
            isTheftAlertActive = false;
            ThingSpeak.setField(4, 0); // Clear alert
        }
    }

    return false;
}

// Send Data to ThingSpeak
void sendDataToThingSpeak() {
  ThingSpeak.setField(1, currentValue);
  ThingSpeak.setField(2, powerValue);
  ThingSpeak.setField(3, kWh);
  
  int responseCode = ThingSpeak.writeFields(channelid, thingSpeakWriteAPIKey);
  if (responseCode == 200) {
    Serial.println("✅ Data sent to ThingSpeak!");
  } else {
    Serial.print("❌ Error sending data: ");
    Serial.println(responseCode);
  }
}

// Dynamic Calibration for ACS712
void calibrateACS712() {
  Serial.println("Calibrating ACS712 sensor...");
  float total = 0;
  int numSamples = 1000;

  for (int i = 0; i < numSamples; i++) {
    total += (analogRead(CURRENT_SENSOR_PIN) / 4095.0) * 3.3;
    delay(1);
  }

  ACS_OFFSET = total / numSamples;
  Serial.print("ACS712 Offset: ");
  Serial.println(ACS_OFFSET, 3);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize arrays
  for (int i = 0; i < MOVING_AVERAGE_WINDOW; i++) {
    powerReadings[i] = 0;
  }
  for (int i = 0; i < PATTERN_SAMPLES; i++) {
    normalPatterns[i] = 0;
    patternThresholds[i] = powerThreshold;
  }
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");
  ThingSpeak.begin(client);

  // Calibrate ACS712
  calibrateACS712();
}

void loop() {
    measurePower();
    
    unsigned long currentMillis = millis();
    
    // Check for theft every second
    if (currentMillis - lastTheftCheckTime >= THEFT_CHECK_INTERVAL) {
        if (detectTheft()) {
            Serial.println("🚨 THEFT DETECTED - Immediate Alert!");
            // Send immediate alert to ThingSpeak
            sendDataToThingSpeak();
        }
        lastTheftCheckTime = currentMillis;
    }
    
    // Update consumption pattern every hour
    if (currentMillis - lastPatternUpdate >= 3600000) {
        updateConsumptionPattern();
        lastPatternUpdate = currentMillis;
    }
    
    // Regular ThingSpeak updates
    if (currentMillis - lastThingSpeakUpdate >= 15000) {
        sendDataToThingSpeak();
        lastThingSpeakUpdate = currentMillis;
    }
    
    delay(100); // Shorter delay for more responsive detection
}
