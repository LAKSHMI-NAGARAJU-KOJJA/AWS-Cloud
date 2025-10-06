#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "secrets.h"

// WiFi credentials
const char WIFI_SSID[] = "DESKTOP-6OTM9JL 2484";
const char WIFI_PASSWORD[] = "01040212";

// Device name from AWS
const char THINGNAME[] = "temp_humidity";
const char MQTT_HOST[] = "a3n99thkggptxt-ats.iot.ap-south-1.amazonaws.com";

// MQTT topics
const char AWS_IOT_PUBLISH_TOPIC[] = "esp8266/pub";
const char AWS_IOT_SUBSCRIBE_TOPIC[] = "esp8266/sub";

// Publishing interval
const long interval = 5000;
unsigned long lastMillis = 0;

// Sensor pins
#define IR_PIN D1
#define TRIG_PIN D5
#define ECHO_PIN D6
#define DHTPIN D2
#define DHTTYPE DHT22

// DHT22 Setup
DHT dht(DHTPIN, DHTTYPE);

// Secure connection and MQTT setup
WiFiClientSecure net;
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
PubSubClient client(net);

// Time sync function
void NTPConnect() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 1510592825) {
    delay(500);
    now = time(nullptr);
  }
  Serial.println("done!");
}

// MQTT message receive callback
void messageReceived(char *topic, byte *payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// AWS connection setup
void connectAWS() {
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  NTPConnect();

  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);

  client.setServer(MQTT_HOST, 8883);
  client.setCallback(messageReceived);

  Serial.println("Connecting to AWS IoT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }

  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");
}

// Read sensors and publish to AWS IoT
void publishMessage() {
  // Read DHT22
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read DHT22");
    humidity = -1;
    temperature = -1;
  }

  // Read Ultrasonic
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout after 30ms
  float distance = duration * 0.034 / 2; // Speed of sound: 340 m/s → 0.034 cm/microsec

  // Read IR
  int irState = digitalRead(IR_PIN);

  // Create JSON payload
  StaticJsonDocument<256> doc;
  doc["thingName"] = THINGNAME;
  doc["timestamp"] = millis();
  doc["temp"] = temperature;
  doc["humidity"] = humidity;
  doc["distance"] = distance;
  doc["ir_state"] = irState;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
  Serial.print("Published: ");
  Serial.println(jsonBuffer);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize sensor pins
  pinMode(IR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  dht.begin();

  connectAWS();
}

void loop() {
  if (!client.connected()) {
    connectAWS();
  }

  if (millis() - lastMillis > interval) {
    lastMillis = millis();
    if (client.connected()) {
      publishMessage();
    }
  }

  client.loop();
}