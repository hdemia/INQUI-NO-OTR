
#include <SPI.h>
#include <TinyGPSPlus.h>
#include <SD.h>
#include "SdsDustSensor.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include "arduino_secrets.h"
#include <BME280I2C.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include "SparkFun_Particle_Sensor_SN-GCJA5_Arduino_Library.h" 


#define STATION_ID "_A"


void buzzer(int times, int t_delay=500);

String DATE, TIME, DATE_S, TIME_S, TIMESTAMP_ISO;

void getGPSDateTime();

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
int status = WL_IDLE_STATUS;

uint32_t timer = millis();
uint32_t timer_led = millis();

String FILE_NAME = "/logfile.CSV";
boolean filename_date_fixed = false;
int errorCycles = 0; // Quante volte contiguamente il ciclo di scrittura non funziona

WiFiServer server(80);
void handleHTTPRequest();


#define LED_PIN D8
#define ALERT_PIN D9
#define PSUPPLY_OFF_PIN D6
#define OFF_REED_PIN D5


/*
 LEGGE I VALORI DI PM2.5 E PM10 DAL SENSORE SN-GCJA5 E LI VISUALIZZA
*/ 
float PM25;                                               // PM2.5 RILEVATA
float PM10;                                               // PM10 RILEVATA
SFE_PARTICLE_SENSOR SENSORE_PM;                           // SENSORE SN-GCJA5 ASSEGNATO A "SENSORE_PM" 

TinyGPSPlus gps;
#define gpsPort Serial1


int off_reed_counter = 0;
uint32_t off_reed_timer = 0;

void TaskSecondaryLogic( void *pvParameters );


void buzzer(int times, int t_delay){
  for(int i = 0; i<times; i++){
    esp_task_wdt_reset();
    digitalWrite(ALERT_PIN, HIGH);
    delay(t_delay);
    esp_task_wdt_reset();
    digitalWrite(ALERT_PIN, LOW);
    delay(t_delay);
    esp_task_wdt_reset();
  }
}

void power_supply(boolean status){
  digitalWrite(PSUPPLY_OFF_PIN, !status);
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
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(ALERT_PIN, OUTPUT);
  pinMode(OFF_REED_PIN, INPUT_PULLUP);
  pinMode(PSUPPLY_OFF_PIN, OUTPUT);

  power_supply(true);

  buzzer(3);
  
  delay(5000);

  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL); 

  esp_task_wdt_reset();
  Serial.println("PIN OK");
  delay(1000);
  esp_task_wdt_reset();
  SENSORE_PM.begin(); 
  SPI.begin();
  Serial.println("CONFIGURING SD");
  while (!SD.begin()){
    Serial.println("SD:: Trying...");
    delay(100);
  }; // stop until ok from microSD
  Serial.println("SD CONFIGURED");
  esp_task_wdt_reset();
  gpsPort.begin(9600, SERIAL_8N1, RX, TX);
  WiFi.softAPConfig(IPAddress(192,48,56,1), IPAddress(192,48,56,1), IPAddress(255,255,255,0));

  status = WiFi.softAP(strcat(ssid, STATION_ID), pass);
  if (!status) {
    Serial.println("Creating access point failed");
    // don't continue
    while (true){
      delay(100);
    }
  }
  esp_task_wdt_reset();

  // wait 100ms for connection:
  delay(100);

  // start the web server on port 80
  server.begin();

  esp_task_wdt_reset();

  Wire.begin();

  if (SENSORE_PM.begin() == false)
  {
    Serial.println("The particle sensor did not respond. Please check wiring. Freezing...");
    while (1)
      ;
  }
  esp_task_wdt_reset();

  uint32_t secondaryLogic_delay = 100;
  xTaskCreate(
    TaskSecondaryLogic
    ,  "Task secondaryLogic" // A name just for humans
    ,  2048        // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);`
    ,  (void*) &secondaryLogic_delay // Task parameter which can modify the task behavior. This must be passed as pointer to void.
    ,  2  // Priority
    ,  NULL // Task handle is not used here - simply pass NULL
  );

  Serial.println("END OF SETUP");
}

void handleHTTPRequest() {
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("New Client.");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    unsigned long currentTime = millis();
    unsigned long previousTime = currentTime;
    const unsigned long timeoutTime = 2000; // timeout in milliseconds

    while (client.connected() && currentTime - previousTime <= timeoutTime) {   // loop while the client's connected with timeout
      currentTime = millis();
      
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // For normal HTML page request
            if (!currentLine.startsWith("GET /dow/") && !currentLine.startsWith("GET /del/")) {
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();
              
              // Send HTML content
              client.print("<html><head><title>ESP32 Web Server</title></head><body>");
              client.print(String("<p>Last data " + DATE_S + " " + TIME_S + ":<br>Coords [lat, lng, alt (m)]:  " + 
                          String(gps.location.lat()).substring(0,7) + "    " + 
                          String(gps.location.lng()).substring(0,7) + "    " + 
                          String(gps.altitude.meters()).substring(0,5) + 
                          "<br>Sensors [PM2.5 (ug/m3), PM10 (ug/m3)]:  " + 
                          String(PM25).substring(0,5) + "    " + 
                          String(PM10).substring(0,5) + "</p><br><br>"));
              
              File root = SD.open("/");
              if (!root) {
                Serial.println("Failed to open directory");
                client.print("<p>Failed to open directory</p>");
              } else {
                while (true) {
                  File entry = root.openNextFile();
                  if (!entry) {
                    break;
                  }
                  
                  if (!entry.isDirectory()) {
                    client.print(String("<p style=\"font-size:12px;\"><a href=\"/dow/") + 
                                entry.name() + String("\">") + entry.name() + 
                                String("</a><a style=\"margin-left:40px\" href=\"/del/") + 
                                entry.name() + String("\">DELETE</a></p>"));
                  }
                  entry.close();
                  esp_task_wdt_reset();
                }
                root.close();
              }
              
              client.print("</body></html>");
              
              // The HTTP response ends with another blank line:
              client.println();
            }
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
        
        // Check for complete request line
        if (currentLine.endsWith(" HTTP/1.1") || currentLine.endsWith(" HTTP/1.0")) {
          if (currentLine.startsWith("GET /del/")) {
            String filename_remove = currentLine.substring(9, currentLine.indexOf(" HTTP/1."));
            if (filename_remove.length() > 0) {
              if (SD.remove("/" + filename_remove)) {
                // Send success response
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println("Connection: close");
                client.println();
                client.print("<html><body><h3>File deleted: " + filename_remove + "</h3>");
                client.print("<a href='/'>Back to home</a></body></html>");
                client.println();
              } else {
                // Send error response
                client.println("HTTP/1.1 404 Not Found");
                client.println("Content-type:text/html");
                client.println("Connection: close");
                client.println();
                client.print("<html><body><h3>Error deleting file: " + filename_remove + "</h3>");
                client.print("<a href='/'>Back to home</a></body></html>");
                client.println();
              }
              break;
            }
          } else if (currentLine.startsWith("GET /dow/")) {
            String filename_dow = currentLine.substring(9, currentLine.indexOf(" HTTP/1."));
            if (filename_dow.length() > 0) {
              File webFile = SD.open("/" + filename_dow); // open file
              if (webFile) {
                Serial.println("Downloading file: " + filename_dow);
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/csv");
                client.println(String("Content-Disposition: attachment; filename=") + filename_dow);
                client.println(String("Content-Length: ") + String(webFile.size()));
                client.println("Connection: close");
                client.println();
                
                // Send file in chunks to avoid buffer issues
                const int chunkSize = 1024;
                uint8_t buffer[chunkSize];
                
                while (webFile.available()) {
                  esp_task_wdt_reset();
                  int bytesRead = webFile.read(buffer, chunkSize);
                  if (bytesRead > 0) {
                    client.write(buffer, bytesRead);
                  }
                  yield(); // Allow ESP32 to perform background tasks
                }
                
                webFile.close();
              } else {
                // Send 404 if file not found
                client.println("HTTP/1.1 404 Not Found");
                client.println("Content-type:text/html");
                client.println("Connection: close");
                client.println();
                client.print("<html><body><h3>File not found: " + filename_dow + "</h3>");
                client.print("<a href='/'>Back to home</a></body></html>");
                client.println();
              }
              break;
            }
          }
        }
      } else {
        // If no data available but client still connected, wait a bit
        delay(1);
      }
    }
    
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}

void CheckAndTurnOffTheStation(){
  if(digitalRead(OFF_REED_PIN) == LOW){
    if(millis() - off_reed_timer > 300){
      off_reed_counter++;
      if(off_reed_counter >= 4){
        Serial.println("TURNING OFF");
        digitalWrite(ALERT_PIN, HIGH);
        delay(300);
        digitalWrite(ALERT_PIN, LOW);
        esp_task_wdt_reset();
        power_supply(false);
        delay(200);
        power_supply(true);
        delay(200);
        power_supply(false);
        delay(200);
        power_supply(true);
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
      FILE_NAME = "/" + DATE + STATION_ID ".CSV";
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
    esp_task_wdt_reset();
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
    esp_task_wdt_reset();
  }
}

int PMSensor_Status(){

  int okSensors = 0;

  byte sensorStatus = SENSORE_PM.getStatusSensors();
  Serial.print(F(" Sensors: "));
  if (sensorStatus == 0){
    Serial.print(F("All sensors good"));
  }else if (sensorStatus == 1)
    Serial.print(F("One sensor or fan abnormal"));
  else if (sensorStatus == 2)
    Serial.print(F("Two sensors or fan abnormal"));
  else if (sensorStatus == 3)
    Serial.print(F("Both sensors and fan abnormal"));

  byte statusPD = SENSORE_PM.getStatusPD();
  Serial.print(F(" Photo diode: "));
  if (statusPD == 0){
    okSensors++;
    Serial.print(F("Normal"));
  }else if (statusPD == 1){
    okSensors++;
    Serial.print(F("Normal w/ software correction"));
  }else if (statusPD == 2)
    Serial.print(F("Abnormal, loss of function"));
  else if (statusPD == 3)
    Serial.print(F("Abnormal, with software correction"));

  byte statusLD = SENSORE_PM.getStatusLD();
  Serial.print(F(" Laser diode: "));
  if (statusLD == 0){
    okSensors++;
    Serial.print(F("Normal"));
  }else if (statusLD == 1){
    okSensors++;
    Serial.print(F("Normal w/ software correction"));
  }else if (statusLD == 2)
    Serial.print(F("Abnormal, loss of function"));
  else if (statusLD == 3)
    Serial.print(F("Abnormal, with software correction"));

  byte statusFan = SENSORE_PM.getStatusFan();
  Serial.print(F(" Fan: "));
  if (statusFan == 0){
    okSensors++;
    Serial.print(F("Normal"));
  }else if (statusFan == 1){
    okSensors++;
    Serial.print(F("Normal w/ software correction"));
  }else if (statusFan == 2)
    Serial.print(F("In calibration"));
  else if (statusFan == 3)
    Serial.print(F("Abnormal, out of control"));

  Serial.println();

  return okSensors == 3;
}

void TaskSecondaryLogic(void *pvParameters){  // This is a task.
  uint32_t secondaryLogic_delay = *((uint32_t*)pvParameters);

  for (;;){ // A Task shall never return or exit.
    const bool gpsFixed = gps.location.isValid() && gps.date.isValid() && gps.time.isValid();
    setFilename();
    CheckAndTurnOffTheStation();
    GPSFixedLed(gpsFixed);
    GPSNotFixedLed(gpsFixed);
    delay(secondaryLogic_delay);
  }
}

File OpenLogFile(){

  File file;

  if(!SD.exists(FILE_NAME)){
    file = SD.open(FILE_NAME, FILE_WRITE);
  }else{
    file = SD.open(FILE_NAME, FILE_APPEND);
  }

  return file;
}

void loop() {
  // put your main code here, to run repeatedly:
  int PmRetries = 0;
  esp_task_wdt_reset();

  while (gpsPort.available() > 0){
    gps.encode(gpsPort.read());
  }

  handleHTTPRequest();

  if (millis() - timer > 60 * 1000) {
    timer = millis(); // reset timer

    Serial.println("Checking PM");
    int PM_status=PMSensor_Status();                          // LETTURA SENSORE PM
    while(!PM_status) {
      Serial.println("PM not ok");
      PmRetries++;
      PM_status=PMSensor_Status();  
      delay(10);
      if(PmRetries == 5){
        break;
      }
    }
    if(PmRetries == 5){
      Serial.println("IMPOSSIBILE LEGGERE VALORI DAL SENSORE, CAUSA DEGLI ERRORI APPENA ELENCATI");
    } else {
      Serial.println("PM measured");
      PM25=SENSORE_PM.getPM2_5();
      PM10=SENSORE_PM.getPM10();
    }
    
    Serial.println("Creating/Opening file");
    File file = OpenLogFile();
    esp_task_wdt_reset();
    getGPSDateTime();

    if (file) {
      errorCycles = 0;
      Serial.println("Writing on file");
      // FILE: Latitude, Longitude, Altitude, YYYY-MM-DDTHH:mm:ss.000, PM2.5, PM10
      file.print(gps.location.lat(), 6); file.print(", "); file.print(gps.location.lng(), 6); file.print(", "); file.print(gps.altitude.meters(),3); file.print(", ");
      file.print(TIMESTAMP_ISO); file.print(", ");
      file.print(PM25); file.print(", "); file.print(PM10);
      file.println();
      file.close();
      Serial.println("File closed");
    }else{
      errorCycles += 1;
    }

    if(errorCycles >= 3){
      ESP.restart();
    }

    // SERIAL: Latitude, Longitude, Altitude, DD/MM/YYYY, HH:MM:SS, PM2.5, PM10
    Serial.print(gps.location.lat(), 6); Serial.print(", "); Serial.print(gps.location.lng(), 6); Serial.print(", "); Serial.print(gps.altitude.meters(),3); Serial.print(", ");
    Serial.print(DATE_S); Serial.print(", ");
    Serial.print(TIME_S); Serial.print(", ");
    Serial.print(PM25); Serial.print(", "); Serial.print(PM10);
    Serial.println();
  }
}
