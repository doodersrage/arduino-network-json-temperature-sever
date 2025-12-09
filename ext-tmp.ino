
#include <SPI.h>
#include <Ethernet.h>
#include <LiquidCrystal.h>
#include <ArduinoJson.h>

// initial
char c = 0;           // received data
char command[2] = "\0";  // command

// ethernet configuration
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDA, 0x02 };
IPAddress arduinoIP(192, 168, 12, 111);
IPAddress dnsIP(192, 168, 12, 10);
IPAddress gatewayIP(192, 168, 12, 1);
IPAddress subnetIP(255, 255, 255, 0);
// start server on port 80, change to whatever port you would like
EthernetServer server(80);

// LCD connections:
LiquidCrystal lcd(8 ,9, A2, 5, 6, 3, A3);
int backLight = 7;    // pin 13 will control the backlight

//TMP36 Pin Variables
int sensorPin = 4; //the analog pin the TMP36's Vout (sense) pin is connected to
int sensorPin1 = 5; //the analog pin the TMP36's Vout (sense) pin is connected to
                        //the resolution is 10 mV / degree centigrade with a
                        //500 mV offset to allow for negative temperatures
 
/*
 * setup() - this function runs once when you turn your Arduino on
 */
void setup()
{
  // analoogue to digital pins
  pinMode(A2, INPUT_PULLUP);      
  pinMode(A3, INPUT_PULLUP);      

  // LCD panel 
  pinMode(backLight, OUTPUT);
  digitalWrite(backLight, HIGH); // turn backlight on. Replace 'HIGH' with 'LOW' to turn it off.
  lcd.begin(16,2);              // columns, rows.  use 16,2 for a 16x2 LCD, etc.
  lcd.clear();                  // start with a blank screen
  lcd.setCursor(0,0);           // set cursor to column 0, row 0 (the first row)
  
  // ethernet shield
  Ethernet.begin(mac, arduinoIP, dnsIP, gatewayIP, subnetIP);
  server.begin();

  // serial output
  Serial.begin(9600);  //Start the serial connection with the computer
                       //to view the result open the serial monitor  
}
 
void loop()
{
  
  //getting the voltage reading from the temperature sensor
  int reading0 = analogRead(sensorPin);  
  int reading1 = analogRead(sensorPin);  
  // get temp average
  int readingAVG = ((reading0 + reading1) / 2);

  // converting that reading to voltage, for 3.3v arduino use 3.3
  float voltage0 = reading0 * 5.0;
  voltage0 /= 1024.0;  
  float voltage1 = reading1 * 5.0;
  voltage1 /= 1024.0;  

  // now print out the temperature
  float temperatureC0 = (voltage0 - 0.5) * 100 ;  //converting from 10 mv per degree wit 500 mV offset
  float temperatureC1 = (voltage1 - 0.5) * 100 ;  
  float temperatureCAVG = ((temperatureC0 + temperatureC1) / 2);
                                                //to degrees ((voltage - 500mV) times 100) 
  // now convert to Fahrenheit
  float temperatureF0 = (temperatureC0 * 9.0 / 5.0) + 32.0;
  float temperatureF1 = (temperatureC1 * 9.0 / 5.0) + 32.0;
  float temperatureFAVG = ((temperatureF0 + temperatureF1) / 2);

  // convert temp data to JSON
  JsonDocument doc;
  doc["temp"]["avg"]["c"] = temperatureCAVG;
  doc["temp"]["avg"]["f"] = temperatureFAVG;
  doc["temp"]["0"]["c"] = temperatureC0;
  doc["temp"]["0"]["f"] = temperatureF0;
  doc["temp"]["1"]["c"] = temperatureC1;
  doc["temp"]["1"]["f"] = temperatureF1;

  // serialize json data
  char output[256];
  serializeJson(doc, output);

  // make ethernet sheild available
  EthernetClient client = server.available();
  // detect if current is the first line
  boolean current_line_is_first = true;

  // check for client, print found temp data
 if (client) {
    // an http request ends with a blank line
    boolean current_line_is_blank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // if we've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so we can send a reply
        if (c == '\n' && current_line_is_blank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: application/json");
          client.println();
          
          // webpage title
          client.println(output);
                    
          break;
        }
        if (c == '\n') {
          // we're starting a new line
          current_line_is_first = false;
          current_line_is_blank = true;
        } 
        else if (c != '\r') {
          // we've gotten a character on the current line
          current_line_is_blank = false;
        }
        // get the first http request
        if (current_line_is_first && c == '=') {
          for (int i = 0; i < 1; i++) {
            c = client.read();
            command[i] = c;
          }
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    client.stop();
  }                                 //waiting a second

  // print temp data to LCD
  String messageC = String(temperatureCAVG) + " degrees C";
  lcd.setCursor(0,0);
  lcd.print(messageC);    // change text to whatever you like. keep it clean!
  lcd.setCursor(0,1);           // set cursor to column 0, row 1
  String messageF = String(temperatureFAVG) + " degrees F";
  lcd.print(messageF);

  // delay for a seond then loop
  delay(1000);  

}