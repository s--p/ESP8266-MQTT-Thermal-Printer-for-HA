#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "Adafruit_Thermal.h"
#include "SoftwareSerial.h"

#define RX_PIN 3 // RX GPIO-Pin of your Microcontroller (ESP8266 Wemos D1 Mini)
#define TX_PIN 1 // TX GPIO-Pin of your Microcontroller (ESP8266 Wemos D1 Mini)

#define MAX_TOPIC_LENGTH 50
#define MAX_PAYLOAD_LENGTH 10

char mqtt_id[24];
char mqtt_topic[50];

uint32_t lastTimeItHappened = millis() + papercheck_milliseconds; 

WiFiClient client;
PubSubClient mqtt(client);

SoftwareSerial mySerial(RX_PIN, TX_PIN); 
Adafruit_Thermal printer(&mySerial);

bool upsideDownMode = false;  // Variable, um den UpsideDown-Modus zu verfolgen
#define MAX_COLUMN_WIDTH 32  // Beispielwert, falls dein Drucker 32 Zeichen pro Zeile druckt.
// Maximum Anzahl Zeilen die im Buffer gespeichert werden können
#define MAX_LINES 10
// Maximum Zeichen pro Zeile
#define MAX_CHARS_PER_LINE 33  // 32 Zeichen + Nullterminator

void callback(char* topic, byte* payload, unsigned int length) {

  // set textlineheight
if (strcmp(topic,mqtt_listen_topic_textlineheight)==0){
      //this topic expects integer!
      int payload_int = mqtt_row_spacing;
      for (int i=0;i<length;i++) {
        char c = payload[i];
        if (c >= '0' && c <= '9')
          payload_int = payload_int*10 + c - '0';  //encode to interger
        }
      printer.setLineHeight(payload_int);
  } 
  // set text size (S | M | L)
  if (strcmp(topic,mqtt_listen_topic_textsize)==0){
      char c = 'S';  
      for (int i=0;i<length;i++) {
        c = payload[i]; 
      }
      printer.setSize(c);
  } 
  // topic to inverse the text (0 | 1)
  if (strcmp(topic,mqtt_listen_topic_textinverse)==0){
      char c = '0';
      for (int i=0;i<length;i++) { 
       c = payload[i];   
      }
      if (c == '1') {
        printer.inverseOn();
      } else {
        printer.inverseOff();
      } 
  } 
  // topic to justify the text (L | C | R)
  if (strcmp(topic,mqtt_listen_topic_textjustify)==0){
      char c = 'L';
      for (int i=0;i<length;i++) { 
        c = payload[i];   
      }
      printer.justify(c);
  } 
  // topic to bold the text (0 | 1)
  if (strcmp(topic,mqtt_listen_topic_textbold)==0){
      char c = '0';
      for (int i=0;i<length;i++) { 
        c = payload[i];   
      }
      if (c == '1') {
        printer.boldOn();
      } else {
        printer.boldOff();
      } 
  } 
  // topic to underline the text (0 | 1)
  if (strcmp(topic,mqtt_listen_topic_textunderline)==0){
      char c = '0';
      for (int i=0;i<length;i++) { 
        c = payload[i];   
      }
      if (c == '1') {
        printer.underlineOn();
      } else {
        printer.underlineOff();
      } 
  } 
  
 // topic to print barcode 
 if (strcmp(topic,mqtt_listen_topic_barcode)==0){
      uint8_t barcode_type = 0;  
      char barcode_value[255] = ""; // some barcodes allows only 255 digits(!) but not for our 58mm printer ;)
      int y = 0;
      bool barcodeseperatorfound = false; 
      for (int i=0;i<length;i++) {
        if (!barcodeseperatorfound){
           if (payload[i] == '|'){
              barcodeseperatorfound = true;
              continue;
           }
          char c = payload[i];
          if (c >= '0' && c <= '9') {
            barcode_type = barcode_type*10 + c - '0';//encode to interger
          }
        } else {
          barcode_value[y++] = (char)payload[i];
        }     
      }

      printer.printBarcode(barcode_value, (uint8_t) barcode_type);
  } 
  // Topic zum Umschalten des UpsideDown-Drucks
  if (strcmp(topic, mqtt_listen_topic_textupsidedown) == 0) {
    char c = '0';
    for (int i = 0; i < length; i++) { 
      c = payload[i];   
    }
    if (c == '1') {
      printer.upsideDownOn();
      upsideDownMode = true;  // UpsideDown aktivieren
    } else {
      printer.upsideDownOff();
      upsideDownMode = false;  // UpsideDown deaktivieren
    } 
  }

  // Topic zum Drucken von Text
  if (strcmp(topic, mqtt_listen_topic_text2print) == 0) {
    if (upsideDownMode) {
      // Arrays für die Zeilen
      char lines[MAX_LINES][MAX_CHARS_PER_LINE];
      int currentLine = 0;
      int currentChar = 0;
      
      // Text in Zeilen aufteilen
      for (int i = 0; i < length && currentLine < MAX_LINES; i++) {
        char c = (char)payload[i];
        
        // Wenn Zeilenende oder Zeile voll
        if (c == '\n' || currentChar >= MAX_CHARS_PER_LINE - 1) {
          lines[currentLine][currentChar] = '\0';
          currentLine++;
          currentChar = 0;
        } else {
          lines[currentLine][currentChar] = c;
          currentChar++;
        }
      }
      // Letzte Zeile abschließen wenn nötig
      if (currentChar > 0 && currentLine < MAX_LINES) {
        lines[currentLine][currentChar] = '\0';
        currentLine++;
      }
      
      // Zeilen in umgekehrter Reihenfolge drucken
      printer.feed(1);
      for (int i = currentLine - 1; i >= 0; i--) {
        printer.println(lines[i]);
      }
      printer.feed(1);
    } else {
      // Normaler Druck ohne Zeilenumkehr
      for (int i = 0; i < length; i++) {
        printer.write(payload[i]);
      }
      printer.println();
    }
  }
}

void setup() {

  mySerial.begin(baud);
  printer.begin();
  printer.setDefault();
  
  printer.setSize(mqtt_text_size);
  printer.setLineHeight(mqtt_row_spacing);
  printer.feed(1);
  printer.println(F("Printer starting"));
  
  wifi_station_set_hostname(my_id);
  printer.print(F("Hostname: "));
  printer.println(my_id);
  WiFi.mode(WIFI_STA);

  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);

}

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    printer.println(F("Connecting to WiFi..."));
    WiFi.begin(ssid, password);

    unsigned long begin_started = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(10);
      if (millis() - begin_started > 60000) {
        ESP.restart();
      }
    }
    printer.println(F("WiFi connected!"));
  }

  if (!mqtt.connected()) {
    if (mqtt.connect(mqtt_id, mqtt_user, mqtt_pass)) {
      printer.println(F("MQTT connected"));
      printer.feed(1);
      mqtt.subscribe(mqtt_listen_topic_text2print);
      mqtt.subscribe(mqtt_listen_topic_textsize);
      mqtt.subscribe(mqtt_listen_topic_textlineheight);
      mqtt.subscribe(mqtt_listen_topic_textinverse);
      mqtt.subscribe(mqtt_listen_topic_textjustify);
      mqtt.subscribe(mqtt_listen_topic_textbold);
      mqtt.subscribe(mqtt_listen_topic_textunderline);
      mqtt.subscribe(mqtt_listen_topic_barcode);
      mqtt.subscribe(mqtt_listen_topic_textupsidedown);
    } else {
      printer.println(F("MQTT connection failed"));
      printer.feed(1);
      delay(2000);
      return;
    }
  }

  //check the paperload
  if ((millis() - lastTimeItHappened >= papercheck_milliseconds)){
    bool bPaperCheck = printer.hasPaper();
    delay(100);
    if (bPaperCheck) {
      mqtt.publish(mqtt_listen_topic_papercheck, "yes");
    } else {
      mqtt.publish(mqtt_listen_topic_papercheck, "no");
    } 
    lastTimeItHappened = millis();
  }

  mqtt.loop();
  
}
