//LIBRARIES INCLUDED
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

//PINS DEFINITION
#define DHTPIN D4
#define DHTTYPE DHT11
#define ALERT_LED D1
#define MOTOR_DIRECTION D2
#define MOTOR_LED LED_BUILTIN

//OBJECT INSTANTIATION
DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;
PubSubClient clientMQTT(client);

//variables
const String host = "192.168.0.12";
const char* hostMQTT = "broker.mqttdashboard.com";
const int httpPort = 8080;
const int mqttPort = 1883;
float lastTemperature = 0.0;
float lastHumidity = 0.0;
String comparacao = "**";
float nominalTemperature = 18.0;
bool automaticMode = true;
bool windowChoise = false;
bool windowOpen = false;
String strMacAddress;
char macAddress[6];

//functions declaration
void configModeCallback(WiFiManager *myWiFiManager);
void motorLogic();
void storeMeasures();
void loadConfig();
void activateMotor(bool orientation);
void connectMQTT();

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

  clientMQTT.setServer(hostMQTT, mqttPort);
}

void loop() {

  if (!clientMQTT.connected()) {
    connectMQTT();
  }
  clientMQTT.publish("pedrohbs.projeto.motor", "Microcontrolador escrevendo no tópico...");
  storeMeasures();
  loadConfig();
  motorLogic();

  delay(5000);
}

void connectMQTT() {
  while (!clientMQTT.connected()) {
    Serial.print("Aguardando conexao MQTT...");
    if (clientMQTT.connect(macAddress)) {
      Serial.println("MQTT conectado.");
      clientMQTT.subscribe("pedrohbs.projeto.motor");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(clientMQTT.state());
      Serial.println(" tentando reconectar em 5 segundos.");
      delay(5000);
    }
  }
}


void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void storeMeasures() {
  Serial.println("Iniciando requisição POST para medidas.");

  float temperature, humidity;
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (temperature != NULL) {
    lastTemperature = temperature;
  }
  if (humidity != NULL) {
    lastHumidity = humidity;
  }

  const int capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;

  doc["temperature"] = lastTemperature;
  doc["humidity"] = lastHumidity;

  String body;
  serializeJson(doc, body);

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
  }

  if (client.connected()) {
    Serial.println("Requisição completa.");
    client.stop();  // DISCONNECT FROM THE SERVER
  }
}

void loadConfig() {
  Serial.println("Iniciando requisição GET para configuração de funcionamento.");

  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  client.println("GET /config?last=true HTTP/1.1");
  client.println("Host: " + host + ":" + httpPort);
  client.println("Accept: */*");
  client.println("Content-Type: application/json");
  client.println();   //HTTP request termination empty line

  while (!client.available()) {}

  while (client.available()) {
    String input = client.readStringUntil('\r');
    if (input.indexOf("data") > 0) {
      if (input != comparacao) {
        DynamicJsonDocument doc_res(1024);
        deserializeJson(doc_res, input);
        automaticMode = doc_res["data"][0]["automatic"];
        windowChoise = doc_res["data"][0]["state"];
        nominalTemperature = (float)doc_res["data"][0]["temperature"];
      }
    }
  }

  if (client.connected()) {
    Serial.println("Requisição completa.");
    client.stop();  // DISCONNECT FROM THE SERVER
  }
}

void motorLogic() {
  if (automaticMode) {
    if (lastTemperature >= nominalTemperature) {
      activateMotor(true);
      Serial.println("Estado: aberto.");
    } else {
      activateMotor(false);
      Serial.println("Estado: fechado.");
    }
  } else {
    if (windowChoise) {
      if (!windowOpen) {
        activateMotor(true);
        Serial.println("Estado: aberto.");
      }
    } else {
      if (windowOpen) {
        activateMotor(false);
        Serial.println("Estado: fechado.");
      }
    }
  }
}

void activateMotor(bool orientation) {
  if (orientation) {
    if (!windowOpen) {
      Serial.println("Abrindo.");
      if (!clientMQTT.connected()) {
        connectMQTT();
      }
      clientMQTT.publish("pedrohbs.projeto.motor", "m1r1");
      digitalWrite(MOTOR_DIRECTION, HIGH);
      digitalWrite(MOTOR_LED, LOW);
      delay(1500);
      clientMQTT.publish("pedrohbs.projeto.motor", "m0r1");
      digitalWrite(MOTOR_LED, HIGH);
      windowOpen = true;
    }
  }
  if (!orientation) {
    if (windowOpen) {
      Serial.println("Fechando.");
      if (!clientMQTT.connected()) {
        connectMQTT();
      }
      clientMQTT.publish("pedrohbs.projeto.motor", "m1r0");
      digitalWrite(MOTOR_DIRECTION, LOW);
      digitalWrite(MOTOR_LED, LOW);
      delay(1500);
      clientMQTT.publish("pedrohbs.projeto.motor", "m0r0");
      digitalWrite(MOTOR_LED, HIGH);
      windowOpen = false;
    }
  }
}
