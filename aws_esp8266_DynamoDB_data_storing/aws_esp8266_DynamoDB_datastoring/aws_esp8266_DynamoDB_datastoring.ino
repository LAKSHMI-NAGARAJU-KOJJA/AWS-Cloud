#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // For creating JSON payloads
#include <time.h>        // For NTP time synchronization
#include "secrets.h"     // Include your secrets.h file containing certificates and private key

// --- WiFi Credentials ---
// Replace with your actual WiFi SSID and password
const char WIFI_SSID[] = "DESKTOP-6OTM9JL 2484";
const char WIFI_PASSWORD[] = "01040212";

// --- AWS IoT Core Configuration ---
// Your AWS IoT Thing Name (must match the one registered in AWS IoT)
const char THINGNAME[] = "ESP8266";

// MQTT broker host address from AWS
const char MQTT_HOST[] = "a3n99thkggptxt-ats.iot.ap-south-1.amazonaws.com";

// MQTT topics for publishing and subscribing
const char AWS_IOT_PUBLISH_TOPIC[] = "esp8266/pub"; // Topic to publish sensor data
const char AWS_IOT_SUBSCRIBE_TOPIC[] = "esp8266/sub"; // Topic to receive commands (optional)

// --- Sensor and Timing Configuration ---
const long interval = 5000; // Publishing interval in milliseconds (5 seconds)
const int8_t TIME_ZONE = 5; // IST is UTC+5:30, using 5 for simplicity. Adjust as needed.
                            // For precise IST, it's 5.5 hours, which isn't directly an int.
                            // For simplicity, using 5. If exact time is critical,
                            // you might need to adjust the NTP server or offset.

// Last time message was sent
unsigned long lastMillis = 0;

// LDR sensor pin
#define LDR_PIN A0 // Analog input pin for LDR

// --- Global Objects ---
WiFiClientSecure net; // Secure WiFi client for TLS connection
// BearSSL objects for certificates and private key are now defined in secrets.h
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
PubSubClient client(net); // MQTT client using the secure WiFi client

// --- Function Prototypes ---
void NTPConnect();
void messageReceived(char *topic, byte *payload, unsigned int length);
void connectAWS();
void publishMessage();

// --- Function Definitions ---

/**
 * @brief Connects to an NTP server to synchronize the ESP8266's internal clock.
 * This is crucial for validating SSL/TLS certificates with AWS IoT.
 */
void NTPConnect() {
  Serial.print("Setting time using SNTP");
  // configTime(timezone_offset_seconds, daylight_savings_offset_seconds, ntp_server1, ntp_server2, ...)
  // Using pool.ntp.org and time.nist.gov for reliable time sync.
  configTime(TIME_ZONE * 3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr); // Get current time
  // Wait until time is set (time_t will be less than a specific epoch if not set)
  while (now < 1510592825) { // A known epoch time (Nov 13, 2017)
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
}

/**
 * @brief Callback function executed when an MQTT message is received on a subscribed topic.
 * @param topic The MQTT topic the message was received on.
 * @param payload The message payload as a byte array.
 * @param length The length of the payload.
 */
void messageReceived(char *topic, byte *payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  // Print the payload characters
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

/**
 * @brief Handles the complete connection process: WiFi, NTP, and AWS IoT MQTT.
 */
void connectAWS() {
  delay(3000); // Small delay to stabilize
  WiFi.mode(WIFI_STA); // Set WiFi to station mode
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Connect to WiFi network
  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));

  // Wait until WiFi is connected
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  NTPConnect(); // Synchronize time for secure connection

  // Set up BearSSL for secure connection
  net.setTrustAnchors(&cert); // Set the Amazon Root CA certificate
  net.setClientRSACert(&client_crt, &key); // Set the client certificate and private key

  // Configure PubSubClient
  client.setServer(MQTT_HOST, 8883); // Set MQTT broker host and secure port (8883)
  client.setCallback(messageReceived); // Set the callback for incoming messages

  Serial.println("Connecting to AWS IoT...");

  // Attempt to connect to AWS IoT using the THINGNAME as client ID
  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000); // Wait and retry
  }

  // Check if connection failed after retries
  if (!client.connected()) {
    Serial.println("AWS IoT Connection Timeout! Check credentials and network.");
    return; // Exit if connection failed
  }

  // If connected, subscribe to the command topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");
  Serial.print("Subscribed to: ");
  Serial.println(AWS_IOT_SUBSCRIBE_TOPIC);
}

/**
 * @brief Reads LDR sensor data, formats it into a JSON payload, and publishes it to AWS IoT.
 */
void publishMessage() {
  int lightLevel = analogRead(LDR_PIN); // Read LDR value (0-1023 on ESP8266 analog pin)

  Serial.print("LDR Light Level: ");
  Serial.println(lightLevel);

  // Create a JSON document to hold the sensor data
  // StaticJsonDocument is used for fixed-size JSON objects
  StaticJsonDocument<200> doc; // Allocate a JSON document with a capacity of 200 bytes

  // Add data fields to the JSON document
  doc["thingName"] = THINGNAME; // <--- ADDED THIS LINE: Include the device's name
  doc["time"] = millis();       // Current uptime of the ESP8266 in milliseconds
  doc["light_level"] = lightLevel; // The LDR sensor reading

  // Serialize the JSON document into a character array (string)
  char jsonBuffer[200]; // Buffer to store the serialized JSON
  serializeJson(doc, jsonBuffer); // Convert JSON object to string

  // Publish the JSON string to the specified MQTT topic
  Serial.print("Publishing to ");
  Serial.print(AWS_IOT_PUBLISH_TOPIC);
  Serial.print(": ");
  Serial.println(jsonBuffer);
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

// --- Arduino Setup and Loop Functions ---

/**
 * @brief Setup function: Initializes serial communication, sensor pin, and connects to AWS IoT.
 */
void setup() {
  Serial.begin(115200); // Initialize serial communication for debugging
  pinMode(LDR_PIN, INPUT); // Set LDR pin as input
  connectAWS(); // Establish connection to AWS IoT
}

/**
 * @brief Loop function: Continuously checks for MQTT messages and publishes sensor data periodically.
 */
void loop() {
  // Check if it's time to publish a new message
  if (millis() - lastMillis > interval) {
    lastMillis = millis(); // Update last published time
    if (client.connected()) { // Only publish if connected to MQTT broker
      publishMessage(); // Publish the LDR sensor data
    } else {
      Serial.println("MQTT client not connected. Reconnecting...");
      connectAWS(); // Attempt to reconnect if disconnected
    }
  }
  client.loop(); // Required for PubSubClient to process incoming/outgoing messages and maintain connection
}
