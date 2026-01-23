/* =========================================================
   Wireless Tracker (UNO + ADXL345 + ESP-01) → ThingSpeak
   ---------------------------------------------------------
   Field1: Geofence flag (0 = safe, 1 = outside)
   Field2: Event flag (0=normal, 1=fall, 2=manual SOS)
   Field3: Latitude
   Field4: Longitude
   ========================================================= */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <SoftwareSerial.h>
#include <math.h>

// ------------ USER SETTINGS ------------
const char* WIFI_SSID = "eee-iot";
const char* WIFI_PASS = "I0t@mar2026!";
const char* THINGSPEAK_API_KEY = "WE5YLRWF7A3ZCR09";

// -------- GEOFENCE SAFE ZONE --------
const double SAFE_LAT_MIN = 1.5580;
const double SAFE_LAT_MAX = 1.5600;
const double SAFE_LON_MIN = 103.7620;
const double SAFE_LON_MAX = 103.7650;
// ------------------------------------

// Fixed coordinates (simulate GPS for now)
const double FIXED_LAT = 1.5590;
const double FIXED_LON = 103.7637;   // ✅ fixed (was wrong 104.x)
// ---------------------------------------

// ---- BUZZER (0 = OFF, 1 = ON) ----
#define BUZZER_ENABLED 0

const int LED_PIN = 8;
const int BUZZER_PIN = 9;
const int DIP1_PIN = 7;
const int DIP2_PIN = 6;

void buzzerOn()  { if (BUZZER_ENABLED) digitalWrite(BUZZER_PIN, HIGH); }
void buzzerOff() { digitalWrite(BUZZER_PIN, LOW); }

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
SoftwareSerial esp(2, 3); // RX=D2, TX=D3

// Timing
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 15000;

// Fall detection
const float IMPACT_THR = 1.15;
const unsigned long FALL_COOLDOWN = 1000;

bool fallLatched = false;
bool pendingFallEvent = false;
unsigned long lastFallTime = 0;

// Manual SOS
const unsigned long BLINK_INTERVAL = 250;
bool manualSent = false;
unsigned long lastBlink = 0;
bool blinkState = false;

int lastMode = -1;

// =======================================================
int readMode() {
  int b1 = (digitalRead(DIP1_PIN) == HIGH) ? 1 : 0;
  int b2 = (digitalRead(DIP2_PIN) == HIGH) ? 1 : 0;
  return (b1 << 1) | b2;
}

bool isInsideSafeZone(double lat, double lon) {
  return (lat >= SAFE_LAT_MIN && lat <= SAFE_LAT_MAX &&
          lon >= SAFE_LON_MIN && lon <= SAFE_LON_MAX);
}
// =======================================================

void setup() {
  Serial.begin(9600);
  Wire.begin();
  esp.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(DIP1_PIN, INPUT_PULLUP);
  pinMode(DIP2_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN, LOW);
  buzzerOff();

  delay(2000);

  if (!accel.begin()) {
    Serial.println("No ADXL345 detected!");
    while (1);
  }
  accel.setRange(ADXL345_RANGE_2_G);

  // ESP WiFi init
  sendAT("AT", "OK", 2000);
  sendAT("AT+CWMODE=1", "OK", 2000);
  connectWiFi();
  sendAT("AT+CIPMUX=0", "OK", 2000);

  Serial.println("System Ready.");
}

void loop() {
  int mode = readMode();
  unsigned long now = millis();

  // ===================== MODE CHANGED =====================
  if (mode != lastMode) {

    // Allow manual SOS retrigger after leaving mode 01
    if (lastMode == 1 && mode != 1) {
      manualSent = false;
    }

    // Reset outputs
    digitalWrite(LED_PIN, LOW);
    buzzerOff();

    // Reset fall state
    fallLatched = false;
    pendingFallEvent = false;

    // Reset blinking
    blinkState = false;
    lastBlink = now;

    if (mode == 1) Serial.println("Entered MANUAL SOS mode (01)");
    if (mode == 2) Serial.println("Entered GEOFENCE mode (10)");
    if (mode == 0) Serial.println("Entered FALL mode (00)");

    lastMode = mode;
  }

  // ===================== MODE 11 HARD STOP =====================
  if (mode == 3) {
    digitalWrite(LED_PIN, LOW);
    buzzerOff();
    pendingFallEvent = false;
    fallLatched = false;
    return;
  }

  // ===================== MODE 10 = GEOFENCE =====================
  if (mode == 2) {

    bool inside = isInsideSafeZone(FIXED_LAT, FIXED_LON);
    int geoFlag = inside ? 0 : 1;      // ✅ Field1 = geofence

    // Alarm behavior
    if (geoFlag == 1) {
      digitalWrite(LED_PIN, HIGH);
      buzzerOn();
    } else {
      digitalWrite(LED_PIN, LOW);
      buzzerOff();
    }

    // Send every 15s
    if (now - lastSend >= SEND_INTERVAL) {
      sendToThingSpeak(geoFlag, 0, FIXED_LAT, FIXED_LON);
      lastSend = now;
    }

    delay(200);
    return;
  }

  // ===================== MODE 00 = FALL MODE =====================
  if (mode == 0) {

    sensors_event_t event;
    accel.getEvent(&event);

    float ax_g = event.acceleration.x / 9.80665;
    float ay_g = event.acceleration.y / 9.80665;
    float az_g = event.acceleration.z / 9.80665;

    float aMag = sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);

    if (!fallLatched) {
      if (aMag > IMPACT_THR && (now - lastFallTime > FALL_COOLDOWN)) {
        lastFallTime = now;

        fallLatched = true;
        pendingFallEvent = true;

        digitalWrite(LED_PIN, HIGH);
        buzzerOn();

        Serial.println("Fall detected!");
      }
    }

    // Send every 15s
    if (now - lastSend >= SEND_INTERVAL) {

      int eventFlag = pendingFallEvent ? 1 : 0;

      // ✅ Field1 no longer magnitude → just 0 (not geofence mode)
      sendToThingSpeak(0, eventFlag, FIXED_LAT, FIXED_LON);

      pendingFallEvent = false;
      lastSend = now;
    }

    delay(100);
    return;
  }

  // ===================== MODE 01 = MANUAL SOS =====================
  if (mode == 1) {

    if (manualSent) {
      digitalWrite(LED_PIN, LOW);
      buzzerOff();
      delay(100);
      return;
    }

    // Blink while pending
    if (now - lastBlink >= BLINK_INTERVAL) {
      lastBlink = now;
      blinkState = !blinkState;
      digitalWrite(LED_PIN, blinkState);
      if (blinkState) buzzerOn();
      else buzzerOff();
    }

    // Send ONCE immediately
    bool ok = sendToThingSpeak(0, 2, FIXED_LAT, FIXED_LON);

    if (ok) {
      manualSent = true;
      buzzerOff();
      digitalWrite(LED_PIN, HIGH);
      delay(3000);
      digitalWrite(LED_PIN, LOW);
      Serial.println("Manual SOS SENT ✅");
    } else {
      Serial.println("Manual SOS FAILED ❌");
      delay(2000);
    }

    return;
  }
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

bool sendToThingSpeak(float field1, int flag, double lat, double lon) {
  const char* host = "api.thingspeak.com";

  String path = String("/update?api_key=") + THINGSPEAK_API_KEY +
                "&field1=" + String(field1) +
                "&field2=" + String(flag) +
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
