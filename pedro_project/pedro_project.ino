//LIBRARIES INCLUDED
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

//PINS DEFINITION
#define DHTPIN D3
#define DHTTYPE DHT11
#define ALERT_LED D1
#define MOTOR_DIRECTION D2
#define MOTOR_LED LED_BUILTIN

//OBJECT INSTANTIATION
DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;

//variables
const size_t MAX_CONTENT_SIZE = 256;// max size of the HTTP response
const String host = "192.168.0.12";
const int httpPort = 8080;
float lastTemperature = 0.0;
float lastHumidity = 0.0;
String comparacao = "**";
const float nominalTemperature = 18.0;
const int measureInterval = 30000;
bool windowOpen = false;

//functions declaration
void configModeCallback(WiFiManager *myWiFiManager);
void serverAccess();
void motorLogic();

void setup() {
  pinMode(MOTOR_DIRECTION, OUTPUT);
  pinMode(MOTOR_LED, OUTPUT);
  digitalWrite(MOTOR_DIRECTION, LOW);
  digitalWrite(MOTOR_LED, HIGH);
  Serial.begin(115200);
  dht.begin();

  WiFiManager wifiManager;

  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    ESP.reset();
    delay(1000);
  }

}

void loop() {

  //DynamicJsonDocument doc(MAX_CONTENT_SIZE);


  serverAccess();
  motorLogic();

  delay(5000);

}


void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}


void serverAccess() {
  float temperature, humidity;
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
//  if (temperature != NULL) {
//    lastTemperature = temperature;
//  }
//  if (humidity != NULL) {
//    lastHumidity = humidity;
//  }
  lastTemperature = temperature;
  lastHumidity = humidity;

  const int capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;

  doc["temperature"] = lastTemperature;
  doc["humidity"] = lastHumidity;

  String body;
  serializeJson(doc, body);
  Serial.println(body);

  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  client.println("POST /measure HTTP/1.1");
  client.println("Host: " + host + ":" + httpPort);
  client.println("Accept: */*");
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println();
  client.print(body);
  client.println();   //HTTP request termination empty line


  while (!client.available()) {}

  while (client.available()) {
    String input = client.readStringUntil('\r');
    if (input.indexOf("data") > 0) {
      if (input != comparacao) {
        comparacao = input;
        Serial.print(input);
      }
    }

  }

  if (client.connected()) {
    client.stop();  // DISCONNECT FROM THE SERVER
  }
}

void motorLogic() {
  if (lastTemperature >= nominalTemperature) {
    Serial.println("Estado: aberto.");
    if (!windowOpen) {
      Serial.println("Abrindo.");
      digitalWrite(MOTOR_DIRECTION, HIGH);
      digitalWrite(MOTOR_LED, LOW);
      delay(10000);
      digitalWrite(MOTOR_LED, HIGH);
      windowOpen = true;
    }
  } else {
    Serial.println("Estado: fechado.");
    if (windowOpen) {
      Serial.println("Fechando.");
      digitalWrite(MOTOR_DIRECTION, LOW);
      digitalWrite(MOTOR_LED, LOW);
      delay(10000);
      digitalWrite(MOTOR_LED, HIGH);
      windowOpen = false;
    }

  }

}
