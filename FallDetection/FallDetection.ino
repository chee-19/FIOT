/* =========================================================
   Wireless Tracker (UNO + ADXL345 + ESP-01) → ThingSpeak
   ---------------------------------------------------------
   Field1: Z-axis (g)
   Field2: Fall flag (0/1)
   ========================================================= */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <math.h>

// ------------ USER SETTINGS ------------
const char* WIFI_SSID = "eee-iot";
const char* WIFI_PASS = "I0t@mar2026!";
const char* THINGSPEAK_API_KEY = "WE5YLRWF7A3ZCR09";
// ---------------------------------------

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
SoftwareSerial esp(2, 3); // RX=D2, TX=D3 (to ESP01)
SoftwareSerial gpsSerial(4, 5);

// timing
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 15000; // 16 s

const int LED_PIN = 8;
const int BUZZER_PIN = 9;
const int DIP1_PIN = 7;   // ARM / DISARM
const int DIP2_PIN = 6;   // RESET (acknowledge fall)


const float EASY_IMPACT_THR = 1.15;     // lower = easier trigger
const unsigned long FALL_COOLDOWN = 1000; // 1s cooldown to prevent repeated triggers

bool fallLatched = false;       // once true -> LED stays ON, detection stops
bool pendingFallEvent = false;  // send "1" once when possible
unsigned long lastFallTime = 0;

const double FIXED_LAT = 1.309800;
const double FIXED_LON = 103.777300;


TinyGPSPlus gps;
double lastLat = 0.0;
double lastLon = 0.0;
bool hasFix = false;

int readMode() {
  int b1 = (digitalRead(DIP1_PIN) == HIGH) ? 1 : 0; // DIP1 ON -> 1
  int b2 = (digitalRead(DIP2_PIN) == HIGH) ? 1 : 0; // DIP2 ON -> 1
  return (b1 << 1) | b2;
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  esp.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(DIP1_PIN, INPUT_PULLUP);
  pinMode(DIP2_PIN, INPUT_PULLUP);

  delay(2000);

  if (!accel.begin()) {
    Serial.println("No ADXL345 detected. Check wiring!");
    while (1);
  }
  accel.setRange(ADXL345_RANGE_2_G);

  sendAT("AT", "OK", 2000);
  sendAT("AT+CWMODE=1", "OK", 2000);
  connectWiFi();
  sendAT("AT+CIPMUX=0", "OK", 2000);
}

void loop() {
  int mode = readMode();

  // Mode must be 00 to run fall alarm mode
  if (mode != 0) {
    fallLatched = false;
    pendingFallEvent = false;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    return;
  }

  // Read accelerometer
  sensors_event_t event;
  accel.getEvent(&event);

  float ax_g = event.acceleration.x / 9.80665;
  float ay_g = event.acceleration.y / 9.80665;
  float az_g = event.acceleration.z / 9.80665;

  float aMag = sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);

  unsigned long now = millis();
  int fallEvent = 0;

  if (!fallLatched) {
    if (aMag > EASY_IMPACT_THR && (now - lastFallTime > FALL_COOLDOWN)) {
      fallEvent = 1;
      lastFallTime = now;

      fallLatched = true;
      pendingFallEvent = true;

      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);

      // ✅ Only print when fall happens
      Serial.println("Fall is detected");
      Serial.print("Latitude: ");
      Serial.print(FIXED_LAT, 6);
      Serial.print(" Longitude: ");
      Serial.println(FIXED_LON, 6);
    }
  }

  // ThingSpeak update every 15s
  if (now - lastSend >= SEND_INTERVAL) {
    int sendFallFlag = pendingFallEvent ? 1 : 0;

    sendToThingSpeak(aMag, sendFallFlag, FIXED_LAT, FIXED_LON);

    pendingFallEvent = false;
    lastSend = now;
  }

  delay(100);
}

// =======================================================
// ESP + ThingSpeak helpers
// =======================================================

bool sendAT(const String& cmd, const String& expect, unsigned long timeout) {
  while (esp.available()) esp.read();
  if (cmd.length() > 0) esp.println(cmd);

  unsigned long t0 = millis();
  String resp;

  while (millis() - t0 < timeout) {
    if (esp.available()) {
      char c = esp.read();
      resp += c;
      if (resp.indexOf(expect) != -1) return true;
    }
  }
  return false;
}

void connectWiFi() {
  String cmd = String("AT+CWJAP=\"") + WIFI_SSID + "\",\"" + WIFI_PASS + "\"";
  sendAT(cmd, "WIFI GOT IP", 20000);
}

bool sendToThingSpeak(float field1, int fallFlag, double lat, double lon) {
  const char* host = "api.thingspeak.com";

  String path = String("/update?api_key=") + THINGSPEAK_API_KEY +
                "&field1=" + String(field1, 3) +
                "&field2=" + String(fallFlag) +
                "&field3=" + String(lat, 6) +
                "&field4=" + String(lon, 6);

  String startCmd = String("AT+CIPSTART=\"TCP\",\"") + host + "\",80";
  if (!sendAT(startCmd, "OK", 5000)) return false;

  String req = String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n";

  String lenCmd = String("AT+CIPSEND=") + req.length();
  if (!sendAT(lenCmd, ">", 3000)) {
    sendAT("AT+CIPCLOSE", "OK", 2000);
    return false;
  }

  esp.print(req);
  bool ok = sendAT("", "SEND OK", 8000);
  sendAT("AT+CIPCLOSE", "OK", 2000);
  return ok;
}