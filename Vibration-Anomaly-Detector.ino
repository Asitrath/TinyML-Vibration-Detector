#include <Wire.h>
#include <MPU6050_light.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// --- SETTINGS ---
const char* ssid = "INSERT_YOUR_SSID_HERE";
const char* password = "INSERT_YOUR_PASSWORD_HERE";
const char* webhookURL = "https://webhook.site/YOUR_UNIQUE_ID";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MPU6050 mpu(Wire);

// TinyML & Logic Variables
float meanVibration = 0, totalVibration = 0;
int samples = 0;
bool learningDone = false;
unsigned long learningStartTime;
unsigned long lastAlertTime = 0;
const unsigned long alertCooldown = 5000; // Wait 5 seconds between notifications
const float threshold = 0.40; // Sensitivity

void setup() {
  Serial.begin(9600);
  
  // 1. WIFI START (Power-priority)
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected!");

  // 2. SENSORS START
  Wire.begin();
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0,10);
    display.println("WiFi: OK");
    display.println("CALIBRATING...");
    display.display();
  }

  mpu.begin();
  delay(1000);
  mpu.calcOffsets(true, true);
  
  learningStartTime = millis();
  Serial.println("System Ready!");
}

void loop() {
  mpu.update();
  float x = mpu.getAccX(), y = mpu.getAccY(), z = mpu.getAccZ();
  float magnitude = sqrt(x*x + y*y + z*z);

  if (!learningDone) {
    // --- LEARNING PHASE (10 Seconds) ---
    totalVibration += magnitude;
    samples++;
    if (millis() - learningStartTime > 10000) {
      meanVibration = totalVibration / samples;
      learningDone = true;
    }
  } else {
    // --- MONITORING PHASE ---
    float anomalyScore = abs(magnitude - meanVibration);

    if (anomalyScore > threshold) {
      // Check if we are allowed to send another alert yet
      if (millis() - lastAlertTime > alertCooldown) {
        sendWebhook(anomalyScore);
        lastAlertTime = millis();
      }
    }
  }

  // UI Refresh
  static unsigned long lastUI = 0;
  if (millis() - lastUI > 200) {
    updateDisplay(magnitude);
    lastUI = millis();
  }
}

void sendWebhook(float score) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    
    // We send the score as a query parameter so you can see it on the website
    String fullURL = String(webhookURL) + "?score=" + String(score);
    
    http.begin(client, fullURL);
    int httpResponseCode = http.GET();
    
    Serial.print("Webhook Sent! Response: ");
    Serial.println(httpResponseCode);
    
    display.clearDisplay();
    display.setCursor(0,10);
    display.println("!!! ALERT !!!");
    display.print("Sent to Cloud");
    display.display();
    
    http.end();
  }
}

void updateDisplay(float vib) {
  display.clearDisplay();
  display.setCursor(0,0);
  if(!learningDone) {
    display.println("PHASE: LEARNING");
    display.print("Vib: "); display.println(vib);
  } else {
    display.println("STATUS: MONITORING");
    display.print("Vib: "); display.println(vib);
    display.print("Last IP: "); display.println(WiFi.localIP());
  }
  display.display();
}