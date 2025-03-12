#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define WIFI_SSID "Act"  // Replace with your WiFi SSID
#define WIFI_PASSWORD "1234567890"  // Replace with your WiFi Password
#define THINGSPEAK_API_KEY "3O9LY8ND0MM0VF9D"  // Replace with your API Key
#define THINGSPEAK_URL "http://api.thingspeak.com/update"

#define FLOW_SENSOR_MAIN 34  // GPIO34 - Main water supply sensor
#define FLOW_SENSOR_USER 35  // GPIO35 - User-end sensor

// Initialize LCD (0x27 is the default I2C address, adjust if needed)
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set the LCD address to 0x27 for a 16 chars and 2 line display

// Flow sensor variables
volatile int mainPulseCount = 0;
volatile int userPulseCount = 0;
float mainFlowRate = 0.0;
float userFlowRate = 0.0;
unsigned long oldTime = 0;

// Interrupt service routines for pulse counting
void IRAM_ATTR mainFlowInterrupt() {
  mainPulseCount++;
}

void IRAM_ATTR userFlowInterrupt() {
  userPulseCount++;
}

void setup() {
  Serial.begin(115200);

  // Initialize LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Water Flow Monitor");
  delay(2000);

  // Configure pins
  pinMode(FLOW_SENSOR_MAIN, INPUT_PULLUP);
  pinMode(FLOW_SENSOR_USER, INPUT_PULLUP);

  // Attach interrupts for flow sensors
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_MAIN), mainFlowInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_USER), userFlowInterrupt, FALLING);

  connectWiFi();
  
  Serial.println("ESP32 Water Theft Detection Initialized...");
  oldTime = millis();
}

void loop() {
  if((millis() - oldTime) > 1000) {    // Update every 1 second
    // Disable interrupts temporarily while calculating flow rate
    detachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_MAIN));
    detachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_USER));
    
    // Calculate flow rate (L/min)
    // Flow rate (L/min) = (Pulse frequency × 60) ÷ 7.5
    mainFlowRate = ((float)mainPulseCount / 7.5) * 60;
    userFlowRate = ((float)userPulseCount / 7.5) * 60;
    
    // Reset counters
    mainPulseCount = 0;
    userPulseCount = 0;
    
    // Re-enable interrupts
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_MAIN), mainFlowInterrupt, FALLING);
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_USER), userFlowInterrupt, FALLING);

    // Print flow rates to Serial
    Serial.print("Main Flow: "); Serial.print(mainFlowRate); Serial.println(" L/min");
    Serial.print("User Flow: "); Serial.print(userFlowRate); Serial.println(" L/min");

    // Detect Theft (if difference is more than 2.0 L/min)
    float flowDifference = mainFlowRate - userFlowRate;
    int theftDetected = (flowDifference > 2.0 && mainFlowRate > 0.5) ? 1 : 0;

    // Update LCD Display
    updateLCDDisplay(mainFlowRate, userFlowRate, theftDetected);

    // Send data to ThingSpeak
    sendToThingSpeak(mainFlowRate, userFlowRate, theftDetected);
    
    oldTime = millis();
  }
}

void updateLCDDisplay(float mainFlow, float userFlow, int theft) {
  // First row: Main Flow Rate
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Main:");
  lcd.print(mainFlow, 1);
  lcd.print("L/min");
  
  // Second row: User Flow & Status
  lcd.setCursor(0, 1);
  lcd.print("User:");
  lcd.print(userFlow, 1);
  lcd.print("L ");
  
  if (theft) {
    lcd.print("ALERT!");
  } else {
    lcd.print("OK");
  }
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\n✅ WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void sendToThingSpeak(float mainFlow, float userFlow, int theft) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = THINGSPEAK_URL;
    url += "?api_key=" + String(THINGSPEAK_API_KEY);
    url += "&field4=" + String(mainFlow);
    url += "&field5=" + String(userFlow);
    url += "&field6=" + String(theft); // 1 = Theft, 0 = Normal

    Serial.println("Sending data to ThingSpeak...");
    
    http.begin(url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      Serial.println("✅ Data Sent Successfully!");
    } else {
      Serial.println("❌ Error Sending Data");
    }

    http.end();
  } else {
    Serial.println("⚠️ WiFi Not Connected!");
  }
}
