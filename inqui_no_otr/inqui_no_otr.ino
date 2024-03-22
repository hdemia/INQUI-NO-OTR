#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <SD.h>
#include "SdsDustSensor.h"
#include "WiFiS3.h"
#include "arduino_secrets.h"
#include <BME280I2C.h>
#include <Wire.h>
#include <WDT.h>
#include <DHT22.h>


#define STATION_ID "_A"


void buzzer(int times, int t_delay=500);

String DATE, TIME, DATE_S, TIME_S, TIMESTAMP_ISO;

void getGPSDateTime();

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
int status = WL_IDLE_STATUS;

uint32_t timer = millis();
uint32_t timer_led = millis();

#define SD_PIN 10
String FILE_NAME = "logfile.CSV";
boolean filename_date_fixed = false;
File file;

WiFiServer server(80);
int handleHTTPRequest(WiFiClient client);


#define LED_PIN 9
#define ALERT_PIN 8
#define AUTOSUPPLY_OFF_PIN 6
#define OFF_REED_PIN 7
#define DHT22_PIN 5

#define DHT_CONNECTED true

/*
 LEGGE I VALORI DI PM2.5 E PM10 DAL SENSORE NOVA-SDS011 E LI VISUALIZZA
*/ 
SoftwareSerial SDSSerial(2,3);
float PM25;                                               // PM2.5 RILEVATA
float PM10;                                               // PM10 RILEVATA
SdsDustSensor SENSORE_PM(SDSSerial);                      // SENSORE NOVA-SDS011 ASSEGNATO A "SENSORE_PM" 

float TEMP(0), HUM(0);

TinyGPSPlus gps;
#define gpsPort Serial1


int off_reed_counter = 0;
uint32_t off_reed_timer = 0;


DHT22 dht22(DHT22_PIN);


void buzzer(int times, int t_delay){
  for(int i = 0; i<times; i++){
    WDT.refresh();
    digitalWrite(ALERT_PIN, HIGH);
    delay(t_delay);
    WDT.refresh();
    digitalWrite(ALERT_PIN, LOW);
    delay(t_delay);
    WDT.refresh();
  }
}

void autosupply(boolean status){
  digitalWrite(AUTOSUPPLY_OFF_PIN, !status);
}

void getGPSDateTime(){
  String zeroPrefixDay = gps.date.day() < 10 ? "0" : "";
  String zeroPrefixMonth = gps.date.month() < 10 ? "0" : "";
  String zeroPrefixHour = gps.time.hour() < 10 ? "0" : "";
  String zeroPrefixMinute = gps.time.minute() < 10 ? "0" : "";
  String zeroPrefixSecond = gps.time.second() < 10 ? "0" : "";
  DATE_S = zeroPrefixDay + String(gps.date.day()) + "/" + zeroPrefixMonth + String(gps.date.month()) + "/" + String(gps.date.year());
  TIME_S = zeroPrefixHour + String(gps.time.hour()) + ":" + zeroPrefixMinute + String(gps.time.minute()) + ":" + zeroPrefixSecond + String(gps.time.second());
  DATE = String(gps.date.year()).substring(2) + zeroPrefixMonth + String(gps.date.month()) + zeroPrefixDay + String(gps.date.day());
  TIME = zeroPrefixHour + String(gps.time.hour()) + zeroPrefixMinute + String(gps.time.minute()) + zeroPrefixSecond + String(gps.time.second());
  TIMESTAMP_ISO = String(gps.date.year()) + "-" + zeroPrefixMonth + String(gps.date.month()) + "-" + zeroPrefixDay + String(gps.date.day()) + "T" + zeroPrefixHour + String(gps.time.hour()) + ":" + zeroPrefixMinute + String(gps.time.minute()) + ":" + zeroPrefixSecond + String(gps.time.second()) + ".000";
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(ALERT_PIN, OUTPUT);
  pinMode(OFF_REED_PIN, INPUT);
  pinMode(AUTOSUPPLY_OFF_PIN, OUTPUT);

  WDT.begin(5000);

  buzzer(3);
  WDT.refresh();
  Serial.println("PIN OK");
  Serial.begin(115200);
  delay(1000);
  WDT.refresh();
  SENSORE_PM.begin(); 
  while (!SD.begin(SD_PIN)); // stop until ok from microSD
  WDT.refresh();
  gpsPort.begin(9600);
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true){
      delay(100);
    };
  }
  WDT.refresh();
  WiFi.config(IPAddress(192,48,56,2));

  status = WiFi.beginAP(strcat(ssid, STATION_ID), pass);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // don't continue
    while (true){
      delay(100);
    }
  }
  WDT.refresh();

  // wait 100ms for connection:
  delay(100);

  // start the web server on port 80
  server.begin();

  autosupply(true);
  WDT.refresh();

  Serial.println("END OF SETUP");
}

int handleHTTPRequest(WiFiClient client){
  if(client){
    Serial.println("Connected");
    String in_data = "";
    while(client.connected()){
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (in_data.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print(String("<p>Last data " + DATE_S + " " + TIME_S + ":<br>Coords [lat, lng, alt (m)]:  " + String(gps.location.lat()).substring(0,7) + "    " + String(gps.location.lng()).substring(0,7) + "    " + String(gps.altitude.meters()).substring(0,5) + "<br>Sensors [PM2.5 (ug/m3), PM10 (ug/m3), TEMP (*C), HUM (%)]:  " + String(PM25).substring(0,5) + "    " + String(PM10).substring(0,5) + "    " + String(TEMP).substring(0,5) + "    " + String(HUM).substring(0,5) + "</p><br><br>"));
            File root = SD.open("/");

            while (true) {
              File entry =  root.openNextFile();
              if (! entry) {
                // no more files
                break;
              }
              
              if (!entry.isDirectory()) {
                client.print(String("<p style=\"font-size:12px;\"><a href=\"/dow/") + entry.name() + String("\">") + entry.name() + String("</a><!--<a style=\"margin-left:40px\" href=\"/del/") + entry.name() + String("\">DELETE</a></p>-->"));
              }
              entry.close();
            }
            WDT.refresh();
            
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          }else {
            //Serial.println(in_data);
            in_data = "";
          }
        }else if (c != '\r') {
          in_data += c;
        }
        if (in_data.startsWith("GET /del/") && in_data.endsWith("HTTP/1.1")) {
          String filename_remove = "";
          filename_remove = in_data.substring(in_data.indexOf("GET /del/")+9, in_data.indexOf(" HTTP/1.1"));
          if(filename_remove){
            SD.remove("/" + filename_remove);
          }
        }
        if (in_data.startsWith("GET /dow/") && in_data.endsWith("HTTP/1.1")) {
          String filename_dow = "";
          filename_dow = in_data.substring(in_data.indexOf("GET /dow/")+9, in_data.indexOf(" HTTP/1.1"));
          File webFile = SD.open(filename_dow); // open file

          if (webFile){
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/csv;");
            client.println(String("Content-Disposition: attachment; filename=") + filename_dow);
            client.println(String("Content-Length: ")  + String(webFile.size()) + String(";"));
            client.println("Connection: close");
            client.println();

            while(webFile.available() && client.available() && client.connected()){
              WDT.refresh();
              client.print(webFile.readStringUntil('\n')); // send file to client
              delayMicroseconds(10);
            }
            webFile.close();
            client.println();
          }
          delay(100); // give the web browser time to receive the data
          break;
        }
      }else{
        break;
      }
    }
    client.stop();
    Serial.println("Disconnected");
  }
}

void CheckAndTurnOffTheStation(){
  if(digitalRead(OFF_REED_PIN) == LOW){
    if(millis() - off_reed_timer > 300){
      off_reed_counter++;
      if(off_reed_counter >= 4){
        WDT.refresh();
        digitalWrite(ALERT_PIN, HIGH);
        delay(3000);
        autosupply(false);
        while(true){
          delay(100);
        }
      }
      off_reed_timer = millis();
    }
  }else{
    off_reed_counter = 0;
  }
}

void setFilename(){
  if(filename_date_fixed == false){
    if(gps.date.isValid()){
      getGPSDateTime();
      Serial.println("DATETIME fixed");
      FILE_NAME = DATE + STATION_ID ".CSV";
      filename_date_fixed = true;
      Serial.println("Filename: " + FILE_NAME);
    }
  }
}

void GPSFixedLed(bool gpsFixed){
  if(gpsFixed == true && millis() - timer_led > 10 * 1000){
    timer_led = millis();
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW);
    WDT.refresh();
  }
}

void GPSNotFixedLed(bool gpsFixed){
  if(gpsFixed == false && millis() - timer_led > 10 * 1000){
    timer_led = millis();
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    WDT.refresh();
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  WiFiClient client;
  bool gpsFixed = false;
  int PmRetries = 0;
  int DHTRetries = 0;

  CheckAndTurnOffTheStation();
  WDT.refresh();

  while (gpsPort.available() > 0){
    if (gps.encode(gpsPort.read())){
      if (gps.location.isValid() && gps.date.isValid() && gps.time.isValid()) {
        gpsFixed = true;
      }else{
        gpsFixed = false;
      }
    }
  }
  
  client = server.available();
  handleHTTPRequest(client);

  setFilename();

  GPSFixedLed(gpsFixed);
  GPSNotFixedLed(gpsFixed);

  if (millis() - timer > 60 * 1000) {
    timer = millis(); // reset timer
    gpsFixed = false;
    
    Serial.println("Checking PM");
    PmResult PM=SENSORE_PM.readPm();                          // LETTURA SENSORE PM
    while(!PM.isOk()) {
      Serial.println("PM not ok");
      PmRetries++;
      PM=SENSORE_PM.readPm(); 
      delay(10);
      if(PmRetries == 5){
        break;
      }
    }
    if(PmRetries == 5){
      Serial.println("IMPOSSIBILE LEGGERE VALORI DAL SENSORE, CAUSA DELL'ERRORE: ");
      Serial.println(PM.statusToString());
    } else {
      Serial.println("PM measured");
      PM25=PM.pm25;
      PM10=PM.pm10;
    }
    
    Serial.println("Creating/Opening file");
    file = SD.open(FILE_NAME, FILE_WRITE);
    if (!file) {
      if (SD.begin(SD_PIN)) {
        file=SD.open(FILE_NAME, FILE_WRITE);              
      }
    }
    WDT.refresh();
    Serial.println("File instance created");
    if (DHT_CONNECTED){
      Serial.println("Checking DHT");
      TEMP = dht22.getTemperature();
      HUM = dht22.getHumidity();

      while(dht22.getLastError() != dht22.OK){
        Serial.println("Retrying DHT");
        DHTRetries++;
        TEMP = dht22.getTemperature();
        HUM = dht22.getHumidity();
        delay(10);
        if(DHTRetries == 5){
          break;
        }
      }

      if(DHTRetries == 5){
        Serial.print("DHT22 last error :");
        Serial.println(dht22.getLastError());
        TEMP = -1.0;
        HUM = -1.0;
      }
    }
    WDT.refresh();
    getGPSDateTime();

    if (file) {
      Serial.println("Writing on file");
      // FILE: Latitude, Longitude, Altitude, YYYY-MM-DDTHH:mm:ss.000, PM2.5, PM10, TEMP, HUM
      file.print(gps.location.lat(), 6); file.print(", "); file.print(gps.location.lng(), 6); file.print(", "); file.print(gps.altitude.meters(),3); file.print(", ");
      file.print(TIMESTAMP_ISO); file.print(", ");
      file.print(PM25); file.print(", "); file.print(PM10);
      file.print(", "); file.print(TEMP); file.print(", "); file.print(HUM);
      file.println();
      file.close();
      Serial.println("File closed");
    }

    // SERIAL: Latitude, Longitude, Altitude, DD/MM/YYYY, HH:MM:SS, PM2.5, PM10, TEMP, HUM
    Serial.print(gps.location.lat(), 6); Serial.print(", "); Serial.print(gps.location.lng(), 6); Serial.print(", "); Serial.print(gps.altitude.meters(),3); Serial.print(", ");
    Serial.print(DATE_S); Serial.print(", ");
    Serial.print(TIME_S); Serial.print(", ");
    Serial.print(PM25); Serial.print(", "); Serial.print(PM10);
    Serial.print(", "); Serial.print(TEMP); Serial.print(", "); Serial.print(HUM);
    Serial.println();
  }
}
