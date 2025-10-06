//AWS IOT CORE
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// WiFi credentials
const char WIFI_SSID[] = "DESKTOP-6OTM9JL 2484";
const char WIFI_PASSWORD[] = "01040212";

// Device name from AWS

const char THINGNAME[] = "ESP8266";

// MQTT broker host address from AWS
const char MQTT_HOST[] = "a3n99thkggptxt-ats.iot.ap-south-1.amazonaws.com";

// MQTT topics
const char AWS_IOT_PUBLISH_TOPIC[] = "esp8266/pub";
const char AWS_IOT_SUBSCRIBE_TOPIC[] = "esp8266/sub";

// Publishing interval
const long interval = 5000;
const int8_t TIME_ZONE = -5;

// Last time message was sent
unsigned long lastMillis = 0;

// LDR pin
#define LDR_PIN A0

// Secure connection and MQTT setup
WiFiClientSecure net;
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
PubSubClient client(net);

// Time sync function
void NTPConnect() {
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 1510592825) {
    delay(500);
    Serial.print(".");
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

// LDR message publisher
void publishMessage() {
  int lightLevel = analogRead(LDR_PIN);  // Read LDR value (0–1023)

  Serial.print("LDR Light Level: ");
  Serial.println(lightLevel);

  StaticJsonDocument<200> doc;
  doc["time"] = millis();
  doc["light_level"] = lightLevel;

  char jsonBuffer[200];
  serializeJson(doc, jsonBuffer);

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void setup() {
  Serial.begin(115200);
  pinMode(LDR_PIN, INPUT);
  connectAWS();
}

void loop() {
  if (millis() - lastMillis > interval) {
    lastMillis = millis();
    if (client.connected()) {
      publishMessage();
    }
  }
  client.loop();
}
