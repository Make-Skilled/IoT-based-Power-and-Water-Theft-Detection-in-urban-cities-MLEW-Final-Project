#include <WiFi.h>
#include <WiFiClient.h>
#include <ThingSpeak.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// WiFi Credentials
char ssid[] = "Act";
char pass[] = "Madhumakeskilled";

// Buzzer Pin
#define BUZZER_PIN 25  // Define buzzer pin (change as needed)
#define BUZZER_DURATION 1000  // Buzzer duration in milliseconds
#define BUZZER_FREQUENCY 2000 // Buzzer frequency in Hz

// LCD I2C Setup
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD address to 0x27 for a 16 chars and 2 line display

// ThingSpeak API
WiFiClient client;
unsigned long channelid = 2853641;  
char thingSpeakWriteAPIKey[] = "3O9LY8ND0MM0VF9D";  

// // Server API
// const char* serverName = "http://yourserver.com/api/data";

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
    Serial.print(abs(currentValue), 2);  // Use abs() to show positive value
    Serial.println(" A");
    
    Serial.print("Voltage:     ");
    Serial.print(ASSUMED_VOLTAGE, 1);
    Serial.println(" V");
    
    Serial.print("Power:       ");
    Serial.print(abs(powerValue), 2);  // Use abs() to show positive value
    Serial.println(" W");
    
    Serial.print("Energy:      ");
    Serial.print(abs(kWh), 3);  // Use abs() to show positive value
    Serial.println(" kWh");
    
    Serial.print("Avg Power:   ");
    Serial.print(abs(powerMovingAverage), 2);  // Use abs() to show positive value
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

// Function to trigger buzzer alert
void triggerBuzzer() {
    tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_DURATION);
}

// Function to display message on LCD
void displayLCD(const char* line1, const char* line2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
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
        displayLCD("Warning!", "Power Change");
    }

    // 2. Check absolute power threshold
    if (abs(powerMovingAverage) > powerThreshold) {  // Use abs() to check positive threshold
        anomalyDetected = true;
        Serial.println("⚠️ High power consumption detected!");
        String powerStr = String(abs(powerMovingAverage), 1) + "W";  // Use abs() for display
        displayLCD("High Power Use", powerStr.c_str());
    }

    // 3. Check pattern deviation
    if (abs(powerMovingAverage) > patternThresholds[currentHour]) {  // Use abs() for comparison
        anomalyDetected = true;
        Serial.println("⚠️ Unusual consumption pattern for this time!");
        displayLCD("Unusual Pattern", "Check Power");
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
            
            // Display alert on LCD
            displayLCD("THEFT DETECTED!", "Alert Sent");
            
            // Trigger buzzer
            triggerBuzzer();
            
            // Send alert to ThingSpeak
            ThingSpeak.setField(4, 1); // Alert field
            return true;
        }
    } else {
        if(consecutiveAnomalies > 0) {
            Serial.println("✅ Normal power consumption resumed");
            String powerStr = String(abs(powerValue), 1) + "W";  // Use abs() for display
            displayLCD("Power Normal", powerStr.c_str());
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
  ThingSpeak.setField(4, isTheftAlertActive ? 1 : 0); // Status: 1 for Theft Detected, 0 for Normal
  
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

// Function to send JSON data to server
void sendJsonToServer() {
    // Removed server API code
}

void setup() {
  Serial.begin(115200);
    
    // Initialize LCD
    lcd.init();
    lcd.backlight();
    displayLCD("Power Monitor", "Starting...");
    
    // Initialize Buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    
    // Initialize arrays
    for (int i = 0; i < MOVING_AVERAGE_WINDOW; i++) {
        powerReadings[i] = 0;
    }
    for (int i = 0; i < PATTERN_SAMPLES; i++) {
        normalPatterns[i] = 0;
        patternThresholds[i] = powerThreshold;
    }
  
  // Connect to Wi-Fi
    displayLCD("Connecting to", ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");
    displayLCD("WiFi Connected", "Starting...");
  ThingSpeak.begin(client);

  // Calibrate ACS712
    displayLCD("Calibrating", "Sensor...");
  calibrateACS712();
    displayLCD("Ready!", "Monitoring...");
}

void loop() {
    measurePower();
    
    unsigned long currentMillis = millis();
    
    // Check for theft every second
    if (currentMillis - lastTheftCheckTime >= THEFT_CHECK_INTERVAL) {
        if (detectTheft()) {
            Serial.println("🚨 THEFT DETECTED - Immediate Alert!");
            // Alert already handled in detectTheft()
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
        
        // Update LCD with current power
        if (!isTheftAlertActive) {  // Don't overwrite theft alert
            String powerStr = "Power: " + String(abs(powerValue), 1) + "W";  // Use abs() for display
            String energyStr = "Energy: " + String(abs(kWh), 3) + "kWh";  // Use abs() for display
            displayLCD(powerStr.c_str(), energyStr.c_str());
        }
    }
    
    delay(2000); // Shorter delay for more responsive detection
}
