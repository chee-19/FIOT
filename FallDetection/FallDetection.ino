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

// ------------ USER SETTINGS ------------
const char* WIFI_SSID = "eee-iot";
const char* WIFI_PASS = "I0t@mar2026!";
const char* THINGSPEAK_API_KEY = "WE5YLRWF7A3ZCR09";
// ---------------------------------------

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
SoftwareSerial esp(2, 3); // RX=D2, TX=D3 (to ESP01)

// timing
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 16000; // 16 s


// fall detection params
const float FREEFALL_THR = 0.9;   // g
const float IMPACT_THR   = 1.2;   // g
const unsigned long FALL_WINDOW = 1000; // ms
bool freefall = false;
unsigned long freefallTime = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  esp.begin(9600);
  delay(2000);

  Serial.println(F("\n--- Wireless Tracker: ADXL345 + ESP01 ---"));

  // init ADXL345
  if (!accel.begin()) {
    Serial.println(F("No ADXL345 detected. Check wiring!"));
    while (1);
  }
  accel.setRange(ADXL345_RANGE_2_G);
  Serial.println(F("ADXL345 ready."));

  // check ESP8266
  if (!sendAT("AT", "OK", 2000)) {
    Serial.println(F("ESP not responding. Check power/wiring."));
  } else {
    Serial.println(F("ESP8266 OK"));
  }

  sendAT("AT+CWMODE=1", "OK", 2000);
  connectWiFi();
  sendAT("AT+CIPMUX=0", "OK", 2000);
}

void loop() {
  sensors_event_t event;
  accel.getEvent(&event);

  float z_g = event.acceleration.z / 9.80665; // convert m/s^2 → g

  // ---- Fall detection ----
  int fallFlag = 0;
  unsigned long now = millis();


  if (fabs(z_g) < FREEFALL_THR && !freefall) {
    freefall = true;
    freefallTime = now;
  }

  if (freefall) {
    if (fabs(z_g) > IMPACT_THR && (now - freefallTime) <= FALL_WINDOW) {
      fallFlag = 1;
      freefall = false;
    }
    if ((now - freefallTime) > FALL_WINDOW) freefall = false;
  }

  Serial.print("Z(g): ");
  Serial.print(z_g, 3);
  Serial.print("  Fall: ");
  Serial.println(fallFlag);

  // ---- Upload to ThingSpeak ----
  if (millis() - lastSend > SEND_INTERVAL || fallFlag == 1) {
    sendToThingSpeak(z_g, fallFlag);
    lastSend = millis();
  }

  delay(100);
}

// =======================================================
//                WiFi + ThingSpeak helpers
// =======================================================

bool sendAT(const String& cmd, const String& expect, unsigned long timeout) {
  while (esp.available()) esp.read(); // flush
  esp.println(cmd);
  unsigned long t0 = millis();
  String resp;
  while (millis() - t0 < timeout) {
    if (esp.available()) {
      char c = esp.read();
      resp += c;
      if (resp.indexOf(expect) != -1) return true;
    }
  }
  Serial.print(F("AT resp: ")); Serial.println(resp);
  return false;
}

void connectWiFi() {
  Serial.print(F("Connecting to WiFi "));
  Serial.println(WIFI_SSID);
  String cmd = String("AT+CWJAP=\"") + WIFI_SSID + "\",\"" + WIFI_PASS + "\"";
  if (sendAT(cmd, "WIFI GOT IP", 20000)) {
    Serial.println(F("WiFi connected."));
  } else {
    Serial.println(F("WiFi failed."));
  }
}

bool sendToThingSpeak(float z, int fallFlag) {
  const char* host = "api.thingspeak.com";
  String path = String("/update?api_key=") + THINGSPEAK_API_KEY +
                "&field1=" + String(z, 3) +
                "&field2=" + String(fallFlag);

  String startCmd = String("AT+CIPSTART=\"TCP\",\"") + host + "\",80";
  if (!sendAT(startCmd, "OK", 5000)) {
    Serial.println(F("CIPSTART failed"));
    return false;
  }

  String req = String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n";

  String lenCmd = String("AT+CIPSEND=") + req.length();
  if (!sendAT(lenCmd, ">", 3000)) {
    Serial.println(F("CIPSEND prompt fail"));
    sendAT("AT+CIPCLOSE", "OK", 2000);
    return false;
  }

  esp.print(req);
  bool ok = sendAT("", "SEND OK", 8000);
  sendAT("AT+CIPCLOSE", "OK", 2000);
  if (ok) Serial.println(F("→ ThingSpeak update OK"));
  return ok;
}