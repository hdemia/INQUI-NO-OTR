# INQUI-NO OTR

Inqui-NO On The Road è una piattaforma mobile per il monitoraggio delle polveri sottili lungo un percorso. Utilizzando l'Arduino Nano ESP32 abbiamo a disposizione molti GPIO, con connettività WiFi. Grazie a questa caratteristica siamo in grado di gestire una connessione wireless che ci consente di servire una pagina web dove è possibile visualizzare informazioni di debug e scaricare i file contenuti nella microSD.

Componenti hardware principali utilizzati per il progetto:
- Arduino Nano ESP32
- Panasonic SN-GCJA5
- ublox NEO-M8N
- MH-CD42 come power manager in grado di ricaricare una pila formato 21700 da 5Ah e attraverso un booster a 5V tutto il resto del progetto in pochi centimetri di spazio

Librerie esterne utilizzate:
- [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus)
- [Panasonic SN-GCJA5](https://github.com/sparkfun/SparkFun_Particle_Sensor_SN-GCJA5_Arduino_Library)
