
#include "DHT.h"
#include <SPI.h>
#include <Ethernet.h>
#include <LiquidCrystal.h>
#include <ArduinoJson.h>
#include <avr/wdt.h>

// dht pins assignment
#define DHTPIN1 A4     // Digital pin connected to the DHT sensor
#define DHTPIN2 A5     // Digital pin connected to the DHT sensor
// dht type dht22 designation
#define DHTTYPE DHT22
// init dht AM2302 probes
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

char command[2] = "\0";  // command

static const unsigned long CLIENT_TIMEOUT_MS = 3000;
static const unsigned long DHT_INTERVAL_MS = 2000;

unsigned long lastDhtReadMs = 0;
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

// ethernet configuration -- this setting works for me, change to whatever works best for you
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDA, 0x02 };
IPAddress arduinoIP(192, 168, 12, 111);
IPAddress dnsIP(192, 168, 12, 8);
IPAddress gatewayIP(192, 168, 12, 1);
IPAddress subnetIP(255, 255, 255, 0);
// start server on port 80, change to whatever port you would like
EthernetServer server(80);

// LCD connections:
LiquidCrystal lcd(8 ,9, A2, 5, 6, 3, A3);
int backLight = 7;    // pin 7 will control the backlight

void readSensors();
void handleClient();
void updateDisplay();
 
/*
 * setup() - this function runs once when you turn your Arduino on
 */
void setup()
{
  // analoogue to digital pin designations
  pinMode(A2, INPUT_PULLUP);      
  pinMode(A3, INPUT_PULLUP);      
  pinMode(A4, INPUT_PULLUP);      
  pinMode(A5, INPUT_PULLUP);      

  // enable LCD panel 
  pinMode(backLight, OUTPUT);
  digitalWrite(backLight, HIGH); // turn backlight on. Replace 'HIGH' with 'LOW' to turn it off.
  lcd.begin(16,2);              // columns, rows.  use 16,2 for a 16x2 LCD, etc.
  lcd.clear();                  // start with a blank screen
  lcd.setCursor(0,0);           // set cursor to column 0, row 0 (the first row)
  
  // enable ethernet shield
  Ethernet.begin(mac, arduinoIP, dnsIP, gatewayIP, subnetIP);
  server.begin();

  //Start the serial connection with the computer
  Serial.begin(9600);  

  // Enable watch dog timer with 8 seconds timeout
  wdt_enable(WDTO_8S); 

  // start AM2302 sensor readings
  dht1.begin();
  dht2.begin();
  delay(DHT_INTERVAL_MS);
  readSensors();
  updateDisplay();
  lastDhtReadMs = millis();
}

void readSensors()
{
  float h1 = dht1.readHumidity();
  float t1 = dht1.readTemperature();
  float h2 = dht2.readHumidity();
  float t2 = dht2.readTemperature();

  sensorsOk = !(isnan(h1) || isnan(t1) || isnan(h2) || isnan(t2));
  if (!sensorsOk) {
    Serial.println(F("Failed to read from DHT sensor!"));
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

void handleClient()
{
  EthernetClient client = server.available();
  if (!client) {
    return;
  }

  client.setTimeout(500);
  unsigned long deadline = millis() + CLIENT_TIMEOUT_MS;
  boolean current_line_is_first = true;
  boolean current_line_is_blank = true;

  while (client.connected() && millis() < deadline) {
    wdt_reset();

    if (!client.available()) {
      delay(1);
      continue;
    }

    char c = client.read();
    if (c == '\n' && current_line_is_blank) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println();
      if (sensorsOk) {
        client.println(jsonOutput);
      } else {
        client.println("{\"error\":\"sensor read failed\"}");
      }
      break;
    }

    if (c == '\n') {
      current_line_is_first = false;
      current_line_is_blank = true;
    } else if (c != '\r') {
      current_line_is_blank = false;
    }

    if (current_line_is_first && c == '=' && client.available()) {
      command[0] = client.read();
      command[1] = '\0';
    }
  }

  client.flush();
  client.stop();
}

void updateDisplay()
{
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
}

void loop()
{
  wdt_reset();

  unsigned long now = millis();
  if (now - lastDhtReadMs >= DHT_INTERVAL_MS) {
    lastDhtReadMs = now;
    readSensors();
    updateDisplay();
  }

  handleClient();
  wdt_reset();
}
