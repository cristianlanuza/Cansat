#include <LoRa.h>
#include <Wire.h>
#include "SSD1306.h"

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>


#define LORASCK     5    // GPIO5  -- SX1278's SCK
#define LORAMISO    19   // GPIO19 -- SX1278's MISO
#define LORAMOSI    27   // GPIO27 -- SX1278's MOSI
#define LORACS      18   // GPIO18 -- SX1278's CS
#define LORARST     23   //  -- SX1278's RESET
#define DI0     26   // GPIO26 -- SX1278's IRQ(Interrupt Request)
#define BAND    868E6
#define LORA_SF 10

#define SDSCK     14    // SD SCK pin
#define SDMISO    2     // SD MISO pin
#define SDMOSI    15    // SD MOSI pin
#define SDCS      13    // SD chip select

#define latitudEntera 41  //41 Zaragoza, 42 Huesca
#define longitudEntera -1

Adafruit_BME280 bme; // I2C

SSD1306 display(0x3c, 21, 22);
String rssi = "RSSI --";
String packSize = "--";

//String packet;
byte destino;
//byte emisor;
//byte mensajeID;
//byte checksum;

String DataWriteSD = "";          // String de escritura SD
bool ErrorSD = false;              // False = no error

// SD CARD
void appendFile(fs::FS &fs, const char * path, const char * message) {
  //Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    //Serial.println("Failed to open file for appending");
    ErrorSD = true;
    return;
  }
  if (file.print(message)) {
    //Serial.print("Message appended,");
    //Serial.println (message);
    ErrorSD = false;

  } else {
    //Serial.println("Append failed");
    ErrorSD = true;
  }
  file.close();
}

void cbk(int packetSize) {
  bool mensajeMio;

  String recepcion = "";

  byte header;
  byte mensajeID;
  byte checksum;
  byte checksumCalculado = 0;
  bool checksumCoincide;

  float temperatura;
  long presion;
  float humedad;

  float latitud;
  float longitud;
  long altitud;
  int satelites;
  long hora;

  int horaHora;
  int horaMinuto;
  int horaSegundo;
  String horaTexto;

  unsigned long radiactividad;

  byte datos[15];

  float temperaturaBase;
  long presionBase;
  float humedadBase;

  header = LoRa.read();

  if (header == B10101010 && packetSize == 18) {
    mensajeMio = true;
  } else {
    mensajeMio = false;
  }

  if (mensajeMio) {
    
    mensajeID = LoRa.read();
    checksum = LoRa.read();

    for (int i = 0; i < 15; i++) {
      datos[i] = LoRa.read();
      checksumCalculado += datos[i];
    }

    checksumCoincide = (checksum == checksumCalculado);

    temperatura = ((datos[0] << 5) | (datos[1] >> 3)) / 100.0 - 10;
    presion = ((((datos[1] & B00000111) << 13) | (datos[2] << 5) | datos[3] >> 3)) + 70000;
    humedad = ((((datos[3] & B00000111) << 11) | (datos[4] << 3) | datos[5] >> 5)) / 100.0;

    latitud = latitudEntera + (((datos[5] & B00011111) << 12) | (datos[6] << 4) | (datos[7] >> 4)) / 100000.0;

    longitud = longitudEntera + (((datos[7] & B00001111) << 13) | (datos[8] << 5) | (datos[9] >> 3)) / 100000.0;

    altitud = ((datos[9] & B00000111) << 8) | datos[10];
    satelites = (datos[11] >> 3);

    hora = ((datos[11] & B00000111) << 14) | (datos[12] << 6) | (datos[13] >> 2);

    horaHora = hora / 3600;
    horaMinuto = (hora - horaHora * 3600) / 60;
    horaSegundo = hora - horaHora * 3600 - horaMinuto * 60;

    horaTexto = ((horaHora < 10) ? "0" : "") + String(horaHora) + ":" + ((horaMinuto < 10) ? "0" : "") + String(horaMinuto) + ":" + ((horaSegundo < 10) ? "0" : "") + String(horaSegundo);

    radiactividad = ((datos[13] & B00000011) << 8) | (datos[14]);

  } else {
    recepcion += (char)header;
    while (LoRa.available()) {
      recepcion += (char)LoRa.read();
    }
  }

  temperaturaBase = bme.readTemperature();
  presionBase = bme.readPressure();
  humedadBase = bme.readHumidity();

  //Display
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  display.drawString(0, 0, "RSSI: " + String(LoRa.packetRssi()) + ", " + (checksumCoincide ? "OK" : "ERROR"));

  //display.drawStringMaxWidth(0 , 24 , 128, packet);

  if (mensajeMio) {
    display.drawString(0, 10, String(packetSize, DEC) + " B, " + "ID: " + String((unsigned int)mensajeID, DEC) + ", Head: " + String((unsigned int)header, DEC));
    display.drawString(0, 20, String(temperatura, 2) + "°C " + String(presion, DEC) + "Pa " + String(humedad, 2) + "% ");
    display.drawString(0, 30, String(latitud, 5) + "°N, " + String(longitud, 5) + "°E");
    display.drawString(0, 40, String(altitud) + "m " + String(satelites) + "sat " + String(radiactividad, DEC) + "CPM " + horaTexto);
  } else {
    display.drawString(0, 10, String(packetSize, DEC) + " B");
    display.drawStringMaxWidth(0, 24, 128, recepcion);
  }

  if (ErrorSD == true) {
    display.drawString(0, 50, "NO SD " + String(temperaturaBase) + "°C " + String(presionBase) + "Pa");
  }
  else {
    display.drawString(0, 50, "SD OK " + String(temperaturaBase) + "°C " + String(presionBase) + "Pa");
  }
  display.display();



  DataWriteSD = "";
  DataWriteSD += String (header);
  DataWriteSD += ",";
  DataWriteSD += String (LoRa.packetRssi());
  DataWriteSD += ",";
  DataWriteSD += String (mensajeID);
  DataWriteSD += ",";
  DataWriteSD += String (checksum);
  DataWriteSD += ",";
  DataWriteSD += String (checksumCalculado);
  DataWriteSD += ",";
  DataWriteSD += String (temperatura, 2);
  DataWriteSD += ",";
  DataWriteSD += String (presion);
  DataWriteSD += ",";
  DataWriteSD += String (humedad, 2);
  DataWriteSD += ",";
  DataWriteSD += String (latitud, 5);
  DataWriteSD += ",";
  DataWriteSD += String (longitud, 5);
  DataWriteSD += ",";
  DataWriteSD += String (altitud);
  DataWriteSD += ",";
  DataWriteSD += String (satelites);
  DataWriteSD += ",";
  DataWriteSD += String (horaHora);
  DataWriteSD += ",";
  DataWriteSD += String (horaMinuto);
  DataWriteSD += ",";
  DataWriteSD += String (horaSegundo);
  DataWriteSD += ",";
  DataWriteSD += String (radiactividad);
  DataWriteSD += ",";
  DataWriteSD += String (temperaturaBase);
  DataWriteSD += ",";
  DataWriteSD += String (presionBase);
  DataWriteSD += ",";
  DataWriteSD += String (humedadBase);
  DataWriteSD += ("\r\n");

  Serial.print(DataWriteSD);

}

void setup() {
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high
  
  bme.begin(0x76);

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  Serial.begin(115200);
  delay (2000);
  Serial.println();
  Serial.println("LoRa Receiver Callback");

  SPI.begin(LORASCK, LORAMISO, LORAMOSI, LORACS);

  LoRa.setPins(LORACS, LORARST, DI0);
  // LoRa.setSpreadingFactor(LORA_SF);
  if (!LoRa.begin(868E6)) {
    Serial.println("Starting LoRa failed!");
    // Muestra en pantalla
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    //display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "------  WARNING ------");
    display.drawString(0, 15, "--- LORA INIT ERROR  ---");

    display.display();

    while (1);    // NO ARRANCA

    LoRa.setSpreadingFactor(LORA_SF);
  }
  //LoRa.onReceive(cbk);
  LoRa.receive();
  Serial.println("LoRa init ok");
  // Muestra en pantalla
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  //display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "LORA INIT OK");
  display.display();

  SPI.end();
  SPI.begin(SDSCK, SDMISO, SDMOSI, SDCS);  // SD
  if (!SD.begin(SDCS)) {
    Serial.println("Card Mount Failed");
    display.drawString(0, 15, "--- ERROR SD CARD ---");
    display.display();
    ErrorSD = true;
    delay (1000);        // asegura que el mensaje se ve 3 sg

  }
  else {
    Serial.println("Card Mount O.K.");
    display.drawString(0, 15, "SD CARD OK");
    display.display();
    ErrorSD = false;

  }

  SPI.end();
  SPI.begin(LORASCK, LORAMISO, LORAMOSI, LORACS); // LORA
  LoRa.setSpreadingFactor(LORA_SF);
  
  delay(1500);
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    cbk(packetSize);


    SPI.end();
    SPI.begin(SDSCK, SDMISO, SDMOSI, SDCS);  // SD


    char buffer[200];
    DataWriteSD.toCharArray(buffer, 200);
    appendFile(SD, "/CanSat.txt", buffer);


    SPI.end();
    SPI.begin(LORASCK, LORAMISO, LORAMOSI, LORACS); // LORA
    LoRa.setSpreadingFactor(LORA_SF);
    //loraData();

  }
  delay(1);
}
