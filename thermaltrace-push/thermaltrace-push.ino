
#include "DHT.h"
#include <ArduinoJson.h>

// ThermalTrace.dev push ingest — dual DHT22, classic JSON schema:
// https://thermaltrace.dev/about/ingest-and-webhooks
//
// ESP32 / ESP8266: HTTPS POST to INGEST_URL (recommended).
// Arduino Uno + Ethernet: HTTP POST to a LAN relay (see secrets.h.example).

#if defined(ESP32) || defined(ESP8266)
#define USE_WIFI_PUSH 1
#else
#define USE_WIFI_PUSH 0
#endif

#if USE_WIFI_PUSH
#include <WiFi.h>
#include <HTTPClient.h>
#else
#include <SPI.h>
#include <Ethernet.h>
#include <LiquidCrystal.h>
#include <avr/wdt.h>
#endif

// ESP32/ESP8266 use GPIO numbers; Uno uses analog pins A4/A5.
#if USE_WIFI_PUSH
#define DHTPIN1 4
#define DHTPIN2 5
#else
#define DHTPIN1 A4
#define DHTPIN2 A5
#endif
#define DHTTYPE DHT22

#define DHT_INTERVAL_MS 2000
#define POST_INTERVAL_MS 60000

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "your-wifi"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "your-password"
#endif
#ifndef INGEST_URL
#define INGEST_URL "https://thermaltrace.dev/api/ingest/YOUR_DEVICE_KEY"
#endif
#ifndef RELAY_HOST
#define RELAY_HOST ""
#endif
#ifndef RELAY_PORT
#define RELAY_PORT 8080
#endif
#ifndef RELAY_PATH
#define RELAY_PATH "/ingest"
#endif

DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

unsigned long lastDhtReadMs = 0;
unsigned long lastPostMs = 0;
float temperatureC0 = 0;
float temperatureC1 = 0;
float temperatureCAVG = 0;
float temperatureF0 = 0;
float temperatureF1 = 0;
float temperatureFAVG = 0;
float humidity0 = 0;
float humidity1 = 0;
float humidityAVG = 0;
bool sensorsOk = false;
char jsonOutput[256];
char lcdLine0[17];
char lcdLine1[17];

#if !USE_WIFI_PUSH
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDA, 0x03 };
IPAddress arduinoIP(192, 168, 12, 112);
IPAddress dnsIP(192, 168, 12, 8);
IPAddress gatewayIP(192, 168, 12, 1);
IPAddress subnetIP(255, 255, 255, 0);
LiquidCrystal lcd(8, 9, A2, 5, 6, 3, A3);
int backLight = 7;
#endif

void readSensors();
void updateDisplay();
bool pushReadings();

#if !USE_WIFI_PUSH
bool pushViaRelay();
#endif

void setup()
{
  pinMode(DHTPIN1, INPUT_PULLUP);
  pinMode(DHTPIN2, INPUT_PULLUP);

#if USE_WIFI_PUSH
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print(F("WiFi"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(F("."));
  }
  Serial.println(F(" ok"));
#else
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);

  pinMode(backLight, OUTPUT);
  digitalWrite(backLight, HIGH);
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);

  Ethernet.begin(mac, arduinoIP, dnsIP, gatewayIP, subnetIP);
  Serial.begin(9600);
  wdt_enable(WDTO_8S);
#endif

  dht1.begin();
  dht2.begin();
  delay(DHT_INTERVAL_MS);
  readSensors();
  updateDisplay();
  lastDhtReadMs = millis();
  pushReadings();
  lastPostMs = millis();
}

void readSensors()
{
  float h1 = dht1.readHumidity();
  float t1 = dht1.readTemperature();
  float h2 = dht2.readHumidity();
  float t2 = dht2.readTemperature();

  sensorsOk = !(isnan(h1) || isnan(t1) || isnan(h2) || isnan(t2));
  if (!sensorsOk) {
    Serial.println(F("Failed to read from DHT22!"));
    return;
  }

  temperatureC0 = t1;
  temperatureC1 = t2;
  temperatureCAVG = (temperatureC0 + temperatureC1) / 2.0;

  temperatureF0 = (temperatureC0 * 9.0 / 5.0) + 32.0;
  temperatureF1 = (temperatureC1 * 9.0 / 5.0) + 32.0;
  temperatureFAVG = (temperatureF0 + temperatureF1) / 2.0;

  humidity0 = h1;
  humidity1 = h2;
  humidityAVG = (humidity0 + humidity1) / 2.0;

  JsonDocument doc;
  doc["temp"]["avg"]["c"] = temperatureCAVG;
  doc["temp"]["avg"]["f"] = temperatureFAVG;
  doc["temp"]["avg"]["h"] = humidityAVG;
  doc["temp"]["0"]["c"] = temperatureC0;
  doc["temp"]["0"]["f"] = temperatureF0;
  doc["temp"]["0"]["h"] = humidity0;
  doc["temp"]["1"]["c"] = temperatureC1;
  doc["temp"]["1"]["f"] = temperatureF1;
  doc["temp"]["1"]["h"] = humidity1;
  serializeJson(doc, jsonOutput);

  char tempC[8];
  char hum[8];
  char tempF[8];
  dtostrf(temperatureCAVG, 0, 1, tempC);
  dtostrf(humidityAVG, 0, 1, hum);
  dtostrf(temperatureFAVG, 0, 1, tempF);
  snprintf(lcdLine0, sizeof(lcdLine0), "%s C Hum %s", tempC, hum);
  snprintf(lcdLine1, sizeof(lcdLine1), "%s F", tempF);
}

#if USE_WIFI_PUSH
bool pushReadings()
{
  if (!sensorsOk) {
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi down"));
    return false;
  }

  HTTPClient http;
  http.begin(INGEST_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(jsonOutput);
  Serial.print(F("POST "));
  Serial.print(code);
  Serial.print(F(" "));
  Serial.println(jsonOutput);
  http.end();
  return code >= 200 && code < 300;
}
#else
bool pushViaRelay()
{
  if (RELAY_HOST[0] == '\0') {
    Serial.println(F("Set RELAY_HOST in secrets.h for Uno push"));
    return false;
  }
  if (!sensorsOk) {
    return false;
  }

  EthernetClient client;
  if (!client.connect(RELAY_HOST, RELAY_PORT)) {
    Serial.println(F("Relay connect failed"));
    return false;
  }

  client.print(F("POST "));
  client.print(RELAY_PATH);
  client.println(F(" HTTP/1.1"));
  client.print(F("Host: "));
  client.println(RELAY_HOST);
  client.println(F("Content-Type: application/json"));
  client.print(F("Content-Length: "));
  client.println(strlen(jsonOutput));
  client.println(F("Connection: close"));
  client.println();
  client.println(jsonOutput);

  unsigned long deadline = millis() + 5000;
  while (client.connected() && millis() < deadline) {
    if (client.available()) {
      Serial.write(client.read());
    }
  }
  client.stop();
  return true;
}

bool pushReadings()
{
  return pushViaRelay();
}
#endif

void updateDisplay()
{
#if !USE_WIFI_PUSH
  lcd.setCursor(0, 0);
  if (sensorsOk) {
    lcd.print(lcdLine0);
    lcd.setCursor(0, 1);
    lcd.print(lcdLine1);
  } else {
    lcd.print(F("Sensor error"));
    lcd.setCursor(0, 1);
    lcd.print(F("Check DHT22"));
  }
#endif
}

void loop()
{
#if !USE_WIFI_PUSH
  wdt_reset();
#endif

  unsigned long now = millis();

  if (now - lastDhtReadMs >= DHT_INTERVAL_MS) {
    lastDhtReadMs = now;
    readSensors();
    updateDisplay();
  }

  if (now - lastPostMs >= POST_INTERVAL_MS) {
    lastPostMs = now;
    pushReadings();
  }

#if !USE_WIFI_PUSH
  wdt_reset();
#endif
}
