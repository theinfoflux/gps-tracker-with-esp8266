/*
 * ESP8266 + NEO-6M/7M GPS Live Tracker with OLED Display (FREE - No API Key)
 * ------------------------------------------------------------------------
 * - Reads GPS data via SoftwareSerial using TinyGPS++
 * - Hosts a web server on the ESP8266 (Leaflet + OpenStreetMap, no API key)
 * - Shows WiFi IP address + live Lat/Lng on an SSD1306 OLED display
 *   (so you can read the IP address directly off the device, no Serial Monitor needed)
 *
 * IMPORTANT: This sketch has TWO files that must sit together in ONE folder:
 *   - ESP8266_GPS_Tracker_OLED.ino  (this file)
 *   - webpage.h                      (the HTML/JavaScript, kept separate on purpose -
 *                                      see note at the bottom of this comment block)
 * The Arduino IDE will show webpage.h as a second tab automatically when you
 * open the .ino file, as long as both are in the same folder.
 *
 * LIBRARIES NEEDED (Arduino IDE -> Tools -> Manage Libraries):
 *   - TinyGPS++            (by Mikal Hart)
 *   - Adafruit SSD1306      (by Adafruit)
 *   - Adafruit GFX Library  (by Adafruit)
 *   (Wire, ESP8266WiFi, ESP8266WebServer, SoftwareSerial come with the ESP8266 board package)
 *
 * WIRING:
 *   GPS module (NEO-6M/7M):
 *     GPS VCC -> 3.3V (or 5V, check your module)
 *     GPS GND -> GND
 *     GPS TX  -> D6 (GPIO12)  [ESP8266 RX]
 *     GPS RX  -> D5 (GPIO14)  [ESP8266 TX]
 *
 *   OLED (I2C, SSD1306 128x64):
 *     OLED VCC -> 3.3V
 *     OLED GND -> GND
 *     OLED SDA -> D2 (GPIO4)
 *     OLED SCL -> D1 (GPIO5)
 *
 * HOW TO USE:
 * 1. Wire everything as above
 * 2. Fill in your WiFi SSID/password below
 * 3. Upload to your ESP8266
 * 4. OLED shows "Connecting..." then your IP address once connected
 * 5. Connect your phone/laptop to the SAME WiFi network
 * 6. Type that IP address into a browser to see the live map
 * 7. OLED keeps showing IP + live lat/lng continuously, no Serial Monitor needed
 *
 * WHY webpage.h IS SEPARATE:
 * The Arduino IDE auto-generates function prototypes by scanning your .ino
 * file with a simple text parser (ctags) BEFORE real compilation. That parser
 * doesn't understand C++ raw string literals, so if the embedded JavaScript
 * (which contains lines like "function updateData() {") is inside the main
 * .ino file, the parser mistakes it for actual C++ code and compilation
 * fails with an error like "'function' does not name a type". Keeping the
 * HTML/JS in webpage.h avoids this since only .ino files get scanned that way.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "webpage.h"

// ==================== CONFIG ====================
const char* ssid = "Xiaomi13T";
const char* password = "12345678";

// TESTING: uncomment the line below to test everything (web page, map,
// OLED) WITHOUT a real GPS module connected. It feeds in a fixed test
// coordinate automatically. Comment it out again once you wire up the
// real GPS module.
// #define TEST_MODE
// ==================================================

// GPS module pins (SoftwareSerial)
static const int RXPin = 12, TXPin = 14; // D6, D5 on NodeMCU
static const uint32_t GPSBaud = 9600;

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C   // If display stays blank, try 0x3D instead
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

TinyGPSPlus gps;
SoftwareSerial gpsSerial(RXPin, TXPin);
ESP8266WebServer server(80);

double latitude = 0.0;
double longitude = 0.0;
double speedKmph = 0.0;
int satellites = 0;

bool gpsFixValid = false;
bool gpsModuleDetected = true; // becomes false if no NMEA data is seen from the module

unsigned long lastGpsCheckTime = 0;
unsigned long lastCharsProcessed = 0;
const unsigned long gpsCheckInterval = 5000; // check every 5 seconds

String ipAddress = "";

unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 1000; // refresh OLED once per second

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleGPSData() {
  String json = "{";
  json += "\"lat\":" + String(latitude, 6) + ",";
  json += "\"lng\":" + String(longitude, 6) + ",";
  json += "\"speed\":" + String(speedKmph, 1) + ",";
  json += "\"sats\":" + String(satellites) + ",";
   json += "\"alt\":" + String(gps.altitude.meters()) + ",";
  json += "\"valid\":" + String(gpsFixValid ? "true" : "false") + ",";
  json += "\"detected\":" + String(gpsModuleDetected ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

// Lets you manually set a test location by visiting, e.g.:
//   http://<ESP8266_IP>/set?lat=24.8607&lng=67.0011
// Works whether or not a real GPS is attached - useful to confirm the
// web page and map are working correctly.
void handleSetLocation() {
  if (server.hasArg("lat") && server.hasArg("lng")) {
    latitude = server.arg("lat").toDouble();
    longitude = server.arg("lng").toDouble();
    gpsFixValid = true; 
    satellites = 8; // fake value just so the OLED/page show something sensible
    server.send(200, "text/plain", "OK - location set to " + String(latitude, 6) + ", " + String(longitude, 6));
  } else {
    server.send(400, "text/plain", "Missing lat/lng. Use /set?lat=24.8607&lng=67.0011");
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("ESP8266 GPS Tracker");
  display.println("--------------------");
  display.print("IP: ");
  display.println(ipAddress);
  display.println();

  if (!gpsModuleDetected) {
    display.println("GPS MODULE NOT");
    display.println("DETECTED!");
    display.println();
    display.println("Check wiring:");
    display.println("TX->D6  RX->D5");
    display.println("VCC/GND connected?");
  } else if (gpsFixValid) {
    display.print("Lat: ");
    display.println(latitude, 6);
    display.print("Lng: ");
    display.println(longitude, 6);
    display.print("Sats: ");
    display.print(satellites);
    display.print("  Alt:");
    display.println(gps.altitude.meters());
  } else {
    display.println("Waiting for GPS fix...");
    display.print("Sats: ");
    display.println(satellites);
   
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(GPSBaud);

  // Init OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 allocation failed - check wiring/address");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting to WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  ipAddress = WiFi.localIP().toString();
  Serial.print("Connected! Open this IP in your browser: ");
  Serial.println(ipAddress);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Connected!");
  display.print("IP: ");
  display.println(ipAddress);
  display.println();
  display.println("Waiting for GPS...");
  display.display();
  delay(2000);

  server.on("/", handleRoot);
  server.on("/gps", handleGPSData);
  server.on("/set", handleSetLocation);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
#ifdef TEST_MODE
  // Simulate a GPS fix that slowly drifts, so you can watch the marker
  // move on the map without any real GPS hardware connected.
  static unsigned long lastSim = 0;
  if (millis() - lastSim > 3000) {
    latitude = 24.8607 + (random(-50, 50) / 100000.0);   // Karachi area, tweak as you like
    longitude = 67.0011 + (random(-50, 50) / 100000.0);
    speedKmph = random(0, 40);
    satellites = 8;
    gpsFixValid = true;
    lastSim = millis();
  }
#else
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid()) {
        latitude = gps.location.lat();
        longitude = gps.location.lng();
        gpsFixValid = true;
      }
      if (gps.speed.isValid()) {
        speedKmph = gps.speed.kmph();
      }
      if (gps.satellites.isValid()) {
        satellites = gps.satellites.value();
      }
    }
  }

  // Detect whether the GPS module is actually sending any data at all.
  // A correctly wired module streams NMEA sentences constantly, even
  // with no satellite fix yet. If the character count hasn't moved in
  // the last few seconds, nothing is coming in - almost always a wiring,
  // power, or baud-rate problem rather than "just no fix yet".
  if (millis() - lastGpsCheckTime > gpsCheckInterval) {
    unsigned long currentChars = gps.charsProcessed();
    if (currentChars == lastCharsProcessed) {
      gpsModuleDetected = false;
      gpsFixValid = false; // can't trust a fix if data has stopped arriving
    } else {
      gpsModuleDetected = true;
    }
    lastCharsProcessed = currentChars;
    lastGpsCheckTime = millis();
  }
#endif

  server.handleClient();

  if (millis() - lastDisplayUpdate > displayInterval) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
}
