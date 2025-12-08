
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
EthernetServer server(80);        

// Connections:
// rs (LCD pin 4) to Arduino pin 12
// rw (LCD pin 5) to Arduino pin 11
// enable (LCD pin 6) to Arduino pin 10
// LCD pin 15 to Arduino pin 13
// LCD pins d4, d5, d6, d7 to Arduino pins 5, 4, 3, 2
LiquidCrystal lcd(8 ,9, A2, 5, 6, 3, A3);

int backLight = 7;    // pin 13 will control the backlight

//TMP36 Pin Variables
int sensorPin = 4; //the analog pin the TMP36's Vout (sense) pin is connected to
int sensorPin1 = 5; //the analog pin the TMP36's Vout (sense) pin is connected to
                        //the resolution is 10 mV / degree centigrade with a
                        //500 mV offset to allow for negative temperatures
 
/*
 * setup() - this function runs once when you turn your Arduino on
 * We initialize the serial connection with the computer
 */
void setup()
{
  // analoogue to digital pins
  pinMode(A2, INPUT_PULLUP);      
  pinMode(A3, INPUT_PULLUP);      

  pinMode(backLight, OUTPUT);
  digitalWrite(backLight, HIGH); // turn backlight on. Replace 'HIGH' with 'LOW' to turn it off.
  lcd.begin(16,2);              // columns, rows.  use 16,2 for a 16x2 LCD, etc.
  lcd.clear();                  // start with a blank screen
  lcd.setCursor(0,0);           // set cursor to column 0, row 0 (the first row)
 
  // if you have a 4 row LCD, uncomment these lines to write to the bottom rows
  // and change the lcd.begin() statement above.
  //lcd.setCursor(0,2);         // set cursor to column 0, row 2
  //lcd.print("Row 3");
  //lcd.setCursor(0,3);         // set cursor to column 0, row 3
  //lcd.print("Row 4");

  Ethernet.begin(mac, arduinoIP, dnsIP, gatewayIP, subnetIP);
  server.begin();

  Serial.begin(9600);  //Start the serial connection with the computer
                       //to view the result open the serial monitor  
}
 
void loop()                     // run over and over again
{
  
 //getting the voltage reading from the temperature sensor
 int reading = analogRead(sensorPin);  
 int reading1 = analogRead(sensorPin);  
 // get temp average
 int readingAVG = ((reading + reading1) / 2);
 
 // converting that reading to voltage, for 3.3v arduino use 3.3
 float voltage = readingAVG * 5.0;
 voltage /= 1024.0;  

 // print out the voltage
 //Serial.print(voltage); Serial.println(" volts");
 
 // now print out the temperature
 float temperatureC = (voltage - 0.5) * 100 ;  //converting from 10 mv per degree wit 500 mV offset
                                               //to degrees ((voltage - 500mV) times 100) 
 // now convert to Fahrenheit
 float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;

  String messageC = String(temperatureC) + " degrees C";
  lcd.setCursor(0,0);
  lcd.print(messageC);    // change text to whatever you like. keep it clean!
  //Serial.println(messageC);
  lcd.setCursor(0,1);           // set cursor to column 0, row 1
  String messageF = String(temperatureF) + " degrees F";
  lcd.print(messageF);
  //Serial.println(messageF);

  JsonDocument doc;

  doc["temp"]["C"] = temperatureC;
  doc["temp"]["F"] = temperatureF;

  char output[256];
  serializeJson(doc, output);

 //delay(1000);    

 EthernetClient client = server.available();
  // detect if current is the first line
  boolean current_line_is_first = true;

  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet link is off");
  }
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
}