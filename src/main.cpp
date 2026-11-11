#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "MAX30105.h"
#include "heartRate.h"

// ===================================================================
// 1. CẤU HÌNH WIFI & MQTT BROKER
// ===================================================================
const char* ssid         = "P21";
const char* password     = "05122005";
const char* mqtt_server  = "broker.hivemq.com";
const int   mqtt_port    = 1883;

// Topic MQTT đẩy dữ liệu lên Node-RED
const char* topic_telemetry = "smart_room/telemetry"; 

WiFiClient espClient;
PubSubClient client(espClient);

// ===================================================================
// 2. CẤU HÌNH PHẦN CỨNG (PINOUT MAPPING)
// ===================================================================
#define BUZZER_PIN     19        // Còi báo động (GPIO 19)
#define LED_PIN        18        // Đèn LED cảnh báo (GPIO 18)
#define DHTPIN         32       
#define DHTTYPE        DHT22

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

// Khởi tạo các đối tượng điều khiển phần cứng
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MAX30105 particleSensor;
DHT dht(DHTPIN, DHTTYPE);

// ===================================================================
// 3. KHAI BÁO BIẾN TOÀN CỤC & THUẬT TOÁN
// ===================================================================
// --- Biến đo nhịp tim ---
const byte RATE_SIZE = 5;
byte rates[RATE_SIZE]; 
byte rateSpot = 0;
long lastBeat = 0; 
float beatsPerMinute;
int beatAvg = 0;
byte rateCount = 0; 
bool isFingerPlaced = false;
long lastUpdateOLED = 0;
bool firstBeatDetected = false; 

// --- Biến đo môi trường ---
float tempC = 0.0;
float humidity = 0.0;

// --- Biến chống nháy màn hình & Warm-up ngón tay ---
long lastFingerDetectedTime = 0;
long fingerTouchTime = 0; 
const int FINGER_TIMEOUT = 1000; 

// --- Thuật toán Lọc Kalman Filter cho MAX30102 ---
float Q_kalman = 0.2; 
float R_kalman = 0.5;  
float P_kalman = 1.0;  
float X_kalman = 0.0;  
float K_kalman = 0.0;  

// --- Quản lý thời gian gửi MQTT & Còi (Không dùng Delay) ---
unsigned long lastMqttSend = 0;
const long intervalMqtt = 1000; // Gửi dữ liệu qua MQTT mỗi 1 giây

unsigned long alarmMillis = 0;
bool alarmState = false;

// ===================================================================
// 4. CÁC HÀM KẾT NỐI MẠNG (WIFI & MQTT)
// ===================================================================
void setup_wifi() {
  delay(10);
  Serial.println("\nDang ket noi WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi da ket noi!");
  Serial.print("Dia chi IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Dang ket noi MQTT Broker...");
    String clientId = "ESP32_Healthcare_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(" Thanh cong!");
    } else {
      Serial.print(" Loi, rc=");
      Serial.print(client.state());
      Serial.println(" Thu lai sau 3 giay...");
      delay(3000);
    }
  }
}

// ===================================================================
// 5. HÀM ĐÓNG GÓI JSON VÀ GỬI DỮ LIỆU MQTT
// ===================================================================
void sendDataToMqtt() {
  humidity = dht.readHumidity();
  tempC    = dht.readTemperature();

  if (isnan(humidity) || isnan(tempC)) {
    tempC = 0.0;
    humidity = 0.0;
  }

  // Tạo chuỗi định dạng JSON gửi lên Node-RED
  String payload = "{";
  payload += "\"bpm\":" + String(isFingerPlaced ? beatAvg : 0) + ",";
  payload += "\"temp\":" + String(tempC, 1) + ",";
  payload += "\"hum\":" + String(humidity, 1) + ",";
  payload += "\"finger\":" + String(isFingerPlaced ? 1 : 0);
  payload += "}";

  client.publish(topic_telemetry, payload.c_str());
}

// ===================================================================
// 6. HÀM KHỞI TẠO (SETUP)
// ===================================================================
void setup() {
  Serial.begin(115200);

  // Cấu hình chân Output Còi & LED
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  // Khởi tạo giao tiếp I2C (SDA = GPIO 21, SCL = GPIO 22)
  Wire.begin(21, 22);
  Wire.setClock(400000); 

  // Khởi tạo OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Loi khoi tao OLED!"));
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print("Khoi tao he thong...");
  display.display();

  // Khởi tạo WiFi & MQTT Broker
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // Khởi tạo cảm biến DHT22
  dht.begin();

  // Khởi tạo cảm biến MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("Loi MAX30102!");
    display.display();
    Serial.println(F("Khong tim thay cảm bien MAX30102!"));
    while (1);
  }

  byte ledBrightness = 0x24; 
  byte sampleAverage = 4;    
  byte ledMode = 2;          
  int sampleRate = 100;      
  int pulseWidth = 411; 
  int adcRange = 4096; 

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  Serial.println("He thong da sang sang!");
}

// ===================================================================
// 7. VÒNG LẶP CHÍNH (LOOP)
// ===================================================================
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // -----------------------------------------------------------------
  // 7.1. XỬ LÝ LỌC KALMAN & ĐO NHỊP TIM TỪ MAX30102
  // -----------------------------------------------------------------
  long irValue = particleSensor.getIR();

  if (irValue > 30000) { // Có ngón tay chèn vào cảm biến
    lastFingerDetectedTime = millis(); 
    
    if (!isFingerPlaced) {
      isFingerPlaced = true;
      fingerTouchTime = millis(); 
      lastBeat = millis(); 
      rateCount = 0;              
      beatAvg = 0;
      firstBeatDetected = false;
    }

    // Thuật toán Lọc Kalman Filter
    P_kalman = P_kalman + Q_kalman;
    K_kalman = P_kalman / (P_kalman + R_kalman);
    X_kalman = X_kalman + K_kalman * (irValue - X_kalman);
    P_kalman = (1 - K_kalman) * P_kalman;

    if (checkForBeat(irValue) == true) { 
      if (millis() - fingerTouchTime >= 1500) { // Đợi ngón tay ổn định 1.5s
        if (!firstBeatDetected) {
          lastBeat = millis();
          firstBeatDetected = true;
        } else {
          long delta = millis() - lastBeat;
          lastBeat = millis(); 
          beatsPerMinute = 60 / (delta / 1000.0);

          if (beatsPerMinute < 160 && beatsPerMinute > 40) { 
            rates[rateSpot++] = (byte)beatsPerMinute; 
            rateSpot %= RATE_SIZE; 
            
            if (rateCount < RATE_SIZE) rateCount++;

            beatAvg = 0;
            for (byte x = 0 ; x < rateCount ; x++) {
              beatAvg += rates[x];
            }
            beatAvg /= rateCount; 
          }
        }
      }
    }
  }

  // -----------------------------------------------------------------
  // 7.2. CẬP NHẬT MÀN HÌNH OLED VÀ ĐIỀU KHIỂN CẢNH BÁO
  // -----------------------------------------------------------------
  if (millis() - lastFingerDetectedTime < FINGER_TIMEOUT) {
    if (millis() - lastUpdateOLED > 1000) {
      lastUpdateOLED = millis(); 

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      
      if (millis() - fingerTouchTime < 1500) {
        display.print("Dang on dinh..."); 
      } else {
        display.print("Dang do nhip tim..."); 
      }
      
      display.setTextSize(2);
      display.setCursor(0, 16);
      display.print("BPM: ");
      display.print(beatAvg);

      display.setTextSize(1);
      display.setCursor(0, 48);
      display.print("T: "); display.print(tempC, 1); display.print("C  ");
      display.print("H: "); display.print(humidity, 0); display.print("%");
      display.display();
    }
    
    if ((beatAvg > 0 && (beatAvg < 50 || beatAvg > 65)) || tempC > 30.5) {
      if (millis() - alarmMillis >= 300) { // Nhấp nháy mỗi 300ms
        alarmMillis = millis();
        alarmState = !alarmState;
        digitalWrite(BUZZER_PIN, alarmState ? HIGH : LOW);
        digitalWrite(LED_PIN, alarmState ? HIGH : LOW);
      }
    } else {
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      alarmState = false;
    }
    
  } else {
    // Không đặt ngón tay -> Reset 
    isFingerPlaced = false;
    beatAvg = 0; 
    rateCount = 0; 
    X_kalman = 0.0; 
    P_kalman = 1.0; 
    
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    alarmState = false;

    if (millis() - lastUpdateOLED > 1000) {
      lastUpdateOLED = millis();
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("Dat ngon tay vao...");
      
      display.setCursor(0, 25);
      display.print("Nhiet do: "); display.print(tempC, 1); display.println(" C");
      display.setCursor(0, 40);
      display.print("Do am   : "); display.print(humidity, 0); display.println(" %");
      display.display();
    }
  }

  // -----------------------------------------------------------------
  // 7.3. ĐỊNH KỲ GỬI DỮ LIỆU QUA MQTT (MỖI 1 GIÂY)
  // -----------------------------------------------------------------
  if (millis() - lastMqttSend > intervalMqtt) {
    lastMqttSend = millis();
    sendDataToMqtt();
  }
}