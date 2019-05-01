#include <LoRa.h>

#include <SPIFFS.h>
#include "FS.h"
#include "SPI.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <TinyGPS++.h>

//Datos LoRa

#define LORA_CS 18
#define LORA_RST 23
#define LORA_IRQ 26

#define LORA_SF 10

byte header = 0xAA;     // address of this device
byte mensajeID = 0x00;  //ID del mensaje (consecutivos)

unsigned long tiempoUltimoEnvio = 0;    // last send time
int intervalo = 1000;          // interval between sends

//Datos BME
#define utilizarBME true

Adafruit_BME280 bme;

//Datos GPS
TinyGPSPlus gps;

//Datos radiactividad
#define PIN_RADIACTIVIDAD 4
volatile unsigned long cuentasPorSegundo = 0;
unsigned int lecturas[60];

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR deteccionParticula() {
  portENTER_CRITICAL_ISR(&mux);
  cuentasPorSegundo++;
  portEXIT_CRITICAL_ISR(&mux);
}

//Datos SPIFFS
#define FORMATEAR_SPIFFS_SI_NECESARIO true
bool grabarSPIFFS = true;
bool mostrarDatos = true;
bool borrarAntesSPIFFS = false;
bool formatearAntesSPIFFS = false;


void setup() {
  Serial.begin(115200);                   // initialize serial

  Serial.println("CanSat Lanuza");

  //CONFIGURACIÓN LORA
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);// set CS, reset, IRQ pin
  // LoRa.setSpreadingFactor(LORA_SF);  // LORA_SF

  if (!LoRa.begin(868E6)) {
    Serial.println("Inicialización LoRa fallida");
  }
  else {
    Serial.println("Inicialización LoRa satisfactoria");
  }

  LoRa.setSpreadingFactor(LORA_SF);

  //CONFIGURACIÓN BME280

  //Wire.begin(21, 22);

  if (utilizarBME) {
    if (!bme.begin(0x76/*, &Wire*/)) {
      Serial.println("Inicialización BME280 fallida");
    } else {
      Serial.println("Inicialización BME280 satisfactoria");
    }
  }
  else {
    Serial.println("BME280 no utilizado");
  }

  //CONFIGURACIÓN GPS
  Serial1.begin(9600, SERIAL_8N1, 12, 15);   //17-TX 18-RX

  //LoRa.onReceive(onReceive);
  //LoRa.receive();

  //CONFIGURACIÓN SPIFFS
  if (!SPIFFS.begin(FORMATEAR_SPIFFS_SI_NECESARIO)) {
    Serial.println("Inicialización SPIFFS fallida");
  } else {
    Serial.println("Inicialización SPIFFS satisfactoria");
  }

  delay(2000);

  //CONFIGURACIÓN RADIACTIVIDAD
  pinMode(PIN_RADIACTIVIDAD, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_RADIACTIVIDAD), deteccionParticula, FALLING);
}

void loop() {
  if (millis() > tiempoUltimoEnvio + intervalo) {
    tiempoUltimoEnvio = millis();            // timestamp the message

    enviarDatos();

    //LoRa.receive();                     // go back into receive mode
  }

  while (Serial1.available()) {
    gps.encode(Serial1.read());
  }

  if (Serial.available()) {
    String comando;

    while (Serial.available()) {
      comando += (char)Serial.read();
    }

    comando.replace("\n", "");
    comando.replace("\r", "");
    comando.replace(" ", "_");
    comando.toUpperCase();

    if (comando != "SPIFFS_BORRAR" && borrarAntesSPIFFS == true) {
      Serial.println("Borrar SPIFFS cancelado");
      borrarAntesSPIFFS = false;
      return;
    }

    if (comando != "SPIFFS_FORMATEAR" && formatearAntesSPIFFS == true) {
      Serial.println("Formatear SPIFFS cancelado");
      formatearAntesSPIFFS = false;
      return;
    }

    if (comando == "AYUDA") {
      Serial.println("Comando AYUDA: Los comandos existentes son SPIFFS_ON, SPIFFS_OFF, SPIFFS?, SPIFFS_BORRAR, SPIFFS_FORMATEAR, SPIFFS_TAMANO, SERIE_ON, SERIE_OFF y SERIE?");
    }
    else if (comando == "SPIFFS_ON") {
      grabarSPIFFS = true;
      Serial.println("Comando SPIFFS_ON: Información guardada en SPIFFS");
    } else if (comando == "SPIFFS_OFF") {
      grabarSPIFFS = false;
      Serial.println("Comando SPIFFS_OFF: Información no guardada en SPIFFS");
    } else if (comando == "SPIFFS?") {
      if (grabarSPIFFS) {
        Serial.println("Comando SPIFFS?: SPIFFS activado");
      } else {
        Serial.println("Comando SPIFFS?: SPIFFS desactivado");
      }
    } else if (comando == "SPIFFS_BORRAR") {
      if (borrarAntesSPIFFS) {
        writeFile(SPIFFS, "/CanSat.txt", "");
        Serial.println("Comando SPIFFS_BORRAR: Archivo borrado de SPIFFS");
        borrarAntesSPIFFS = false;
      } else {
        Serial.println("Comando SPIFFS_BORRAR: Escribe el comando de nuevo para borrar el archivo");
        borrarAntesSPIFFS = true;
      }
    } else if (comando == "SPIFFS_FORMATEAR") {
      if (formatearAntesSPIFFS) {
        SPIFFS.format();
        Serial.println("Comando SPIFFS_FORMATEAR: memoria SPIFFS formateada");
        formatearAntesSPIFFS = false;
      } else {
        Serial.println("Comando SPIFFS_FORMATEAR: Escribe el comando de nuevo para formatear la memoria SPIFFS");
        formatearAntesSPIFFS = true;
      }
    } else if (comando == "SPIFFS_LEER") {
      Serial.println("Comando SPIFFS_LEER: Información guardada enviada por serie");
      Serial.println("[INICIO]");
      //Serial.print(readFile(SPIFFS, "/CanSat.txt"));
      readFile(SPIFFS, "/CanSat.txt");
      Serial.println("\n[FIN]");
    } else if (comando == "SPIFFS_TAMANO") {
      unsigned long tamano = fileSize(SPIFFS, "/CanSat.txt");
      Serial.println("Comando SPIFFS_TAMANO: " + String(tamano / 1024.0, 1) + " KB (" + String(tamano / 1507328.0 * 100.0, 1) + "%)");
    } else if (comando == "SERIE_ON") {
      mostrarDatos = true;
      Serial.println("Comando SERIE_ON: Datos mostrados por serie");
    } else if (comando == "SERIE_OFF") {
      mostrarDatos = false;
      Serial.println("Comando SERIE_OFF: Datos no mostrados por serie");
    } else if (comando == "SERIE?") {
      if (mostrarDatos) {
        Serial.println("Comando SERIE?: Serie activado");
      } else {
        Serial.println("Comando SERIE?: Serie desactivado");
      }
    } else {
      Serial.println("No entiendo el comando " + comando + ", escribe AYUDA para obtener la lista de comandos");
    }
  }
}

void enviarDatos() {

  byte checksum = 0;

  //Datos en bruto
  float temperaturaRaw = utilizarBME ? bme.readTemperature() : 20.00;
  unsigned long presionRaw = utilizarBME ? bme.readPressure() : 101325;
  float humedadRaw = utilizarBME ? bme.readHumidity() : 50.00;

  float latitudRaw = gps.location.lat();
  float longitudRaw = gps.location.lng();
  unsigned long altitudRaw = gps.altitude.meters();
  unsigned int satelitesRaw = gps.satellites.value();
  int horaHora = gps.time.hour();
  int horaMinuto = gps.time.minute();
  int horaSegundo = gps.time.second();
  unsigned long horaRaw = horaHora * 3600 + horaMinuto * 60 + horaSegundo;

  portENTER_CRITICAL(&mux);
  unsigned long lecturasNuevas = cuentasPorSegundo;
  cuentasPorSegundo = 0;
  portEXIT_CRITICAL(&mux);

  unsigned long radiactividadRaw = 0;
  for (int i = 59; i > 0 ; i--) {
    lecturas[i] = lecturas[i - 1];
  }
  lecturas[0] = lecturasNuevas;
  for (int i = 0; i < 60; i++) {
    radiactividadRaw += lecturas[i];
    //Serial.print(lecturas[i]);
  }
  if ((floor(millis() / 1000) + 1) < 60) {
    radiactividadRaw *= 60.0 / (floor(millis() / 1000) + 1);
  }

  //Serial.println("Temperatura: " + String(temperaturaRaw));
  //Serial.println("Presión: " + String(presionRaw));
  //Serial.println("Humedad: " + String(humedadRaw));
  //Serial.println("Latitud: " + String(latitudRaw, 5));
  //Serial.println("Longitud: " + String(longitudRaw, 5));
  //Serial.println("Altitud: " + String(altitudRaw));
  //Serial.println("Satélites: " + String(satelitesRaw));
  //Serial.println("Hora: " + String(horaRaw));
  //Serial.println("Radiactividad: " + String(radiactividadRaw));

  //Datos procesador
  unsigned long temperatura = (temperaturaRaw + 10) * 100;
  unsigned long presion = presionRaw - 70000;
  unsigned long humedad = humedadRaw * 100;

  unsigned long latitud = latitudRaw * 100000 - (int)latitudRaw * 100000 + ((latitudRaw < 0) ? 100000 : 0);       //17 bits;
  unsigned long longitud = longitudRaw * 100000 - (int)longitudRaw * 100000 + ((longitudRaw < 0) ? 100000 : 0);      //17 bits;
  unsigned long altitud = altitudRaw;                      //11 bits
  unsigned long satelites = satelitesRaw; //5 bits
  unsigned long hora = horaRaw; //17 bits

  unsigned long radiactividad = radiactividadRaw; //10 bits

  //Serial.println("Temperatura enviada: " + String(temperatura));
  //Serial.println("Presión enviada: " + String(presion));
  //Serial.println("Humedad enviada: " + String(humedad));
  //Serial.println("Latitud enviada: " + String(latitud));
  //Serial.println("Longitud enviada: " + String(longitud));
  //Serial.println("Altitud enviada: " + String(altitud));
  //Serial.println("Satélites enviada: " + String(satelites));
  //Serial.println("Hora enviada: " + String(hora));
  //Serial.println("Radiactividad enviada: " + String(radiactividad));


  //Serial.println("Datos recogidos");

  byte datos[15];
  datos[0] = recortar(temperatura, 5, 12, 0);
  datos[1] = recortar(temperatura, 0, 4, 3) | recortar(presion, 13, 15, 0);
  datos[2] = recortar(presion, 5, 12, 0);
  datos[3] = recortar(presion, 0, 4, 3) | recortar(humedad, 11, 13, 0);
  datos[4] = recortar(humedad, 3, 10, 0);
  datos[5] = recortar(humedad, 0, 2, 5) | recortar(latitud, 12, 16, 0);
  datos[6] = recortar(latitud, 4, 11, 0);
  datos[7] = recortar(latitud, 0, 3, 4) | recortar(longitud, 13, 16, 0);
  datos[8] = recortar(longitud, 5, 12, 0);
  datos[9] = recortar(longitud, 0, 4, 3) | recortar(altitud, 8, 10, 0);
  datos[10] = recortar(altitud, 0, 7, 0);
  datos[11] = recortar(satelites, 0, 4, 3) | recortar(hora, 14, 16, 0);
  datos[12] = recortar(hora, 6, 13, 0);
  datos[13] = recortar(hora, 0, 5, 2) | recortar(radiactividad, 8, 9, 0);
  datos[14] = recortar(radiactividad, 0, 7, 0);

  //Serial.println("Datos reordenados");

  for (int i = 0; i < sizeof(datos) / sizeof(datos[0]); i++) {
    checksum += datos[i];
    //Serial.println(datos[i], BIN);
  }

  //Serial.println("Checksum calculado");

  //Serial.print("Datos enviados: ");

  LoRa.beginPacket();                   // start packet
  LoRa.write(header);                   // add header
  //Serial.print((char)header);
  LoRa.write(mensajeID);                // add message ID
  //Serial.print((char)mensajeID);
  LoRa.write(checksum);        // add payload length
  //Serial.print((char)checksum);

  for (int i = 0; i < sizeof(datos) / sizeof(datos[0]); i++) {
    LoRa.write(datos[i]);
    //Serial.print((char)datos[i]);
  }

  LoRa.endPacket();                     // finish packet and send it


  String datosString = "";
  datosString += String((unsigned int)header);
  datosString += ",";
  datosString += String((unsigned int)mensajeID);
  datosString += ",";
  datosString += String((unsigned int)checksum);
  datosString += ",";
  datosString += String(temperaturaRaw, 2);
  datosString += ",";
  datosString += String(presionRaw);
  datosString += ",";
  datosString += String(humedadRaw, 2);
  datosString += ",";
  datosString += String(latitudRaw, 5);
  datosString += ",";
  datosString += String(longitudRaw, 5);
  datosString += ",";
  datosString += String(altitudRaw);
  datosString += ",";
  datosString += String(horaHora);
  datosString += ",";
  datosString += String(horaMinuto);
  datosString += ",";
  datosString += String(horaSegundo);
  datosString += ",";
  datosString += String(radiactividadRaw);
  datosString += "\r\n";

  if (mostrarDatos) {
    Serial.print(datosString);
  }

  if (grabarSPIFFS) {
    char datosChar[200];
    datosString.toCharArray(datosChar, 200);
    appendFile(SPIFFS, "/CanSat.txt", datosChar);
  }

  mensajeID++;                           // increment message ID
}

byte recortar(unsigned long numero, unsigned int inicio, unsigned int fin, unsigned int offset) { //inicio y fin desde el final, 0..n
  return ((numero & (unsigned long)round(pow(2, max(inicio, fin) + 1) - 1)) >> min(inicio, fin)) << offset;
}

void readFile(fs::FS &fs, const char * path) {
  String fileData;
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    //return "ERROR";
    return;
  }
  while (file.available()) {
    Serial.print((char)file.read());
    //fileData += (char)file.read();
  }
  file.close();
  //return fileData;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    return;
  }
  file.print(message);
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    return;
  }
  file.print(message);
  file.close();
}

unsigned long fileSize(fs::FS &fs, const char * path) {
  File file = fs.open(path);
  if (!file) {
    return 0;
  }
  unsigned long tamano = file.size();
  file.close();
  return tamano;
}
