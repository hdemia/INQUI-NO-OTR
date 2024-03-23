# INQUI-NO OTR

Inqui-NO On The Road è una piattaforma mobile per il monitoraggio delle polveri sottili, temperatura e umidità lungo un percorso. Utilizzando l'Arduino UNO R4 WiFi, sfruttiamo le capacità del modulo ESP32-S3. Grazie a questa componente, siamo in grado di gestire una connessione wireless che ci consente di servire una pagina web dove è possibile visualizzare informazioni di debug e scaricare i file contenuti nella microSD.

Componenti hardware principali utilizzati per il progetto:
- Arduino UNO R4 WiFi
- Nova SDS011
- DHT22
- Adafruit Ultimate GPS v3

Librerie esterne utilizzate:
- [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus)
- [SdsDustSensor](https://github.com/lewapek/sds-dust-sensors-arduino-library)
- [BME280I2C](https://github.com/finitespace/BME280)
- [DHT22](https://github.com/dvarrel/DHT22)
