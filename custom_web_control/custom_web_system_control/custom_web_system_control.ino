#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h> // Include the ArduinoJson library (install via Library Manager)

// --- CONFIGURATION START ---
// WiFi credentials
const char* ssid = "DESKTOP-6OTM9JL 2484"; // <<<<<<< REPLACE WITH YOUR WIFI SSID
const char* password = "01040212"; // <<<<<<< REPLACE WITH YOUR WIFI PASSWORD

// AWS API Gateway Endpoint details
// Based on your previous input: https://axjd117usj.execute-api.ap-south-1.amazonaws.com/dev/mydevice
const char* host = "axjd117usj.execute-api.ap-south-1.amazonaws.com"; // <<<<<<< REPLACE WITH YOUR API GATEWAY HOST (ONLY THE HOSTNAME)
const char* path = "/dev/mydevice"; // The resource path on your API Gateway, confirmed as /dev/mydevice
const char* deviceId = "esp001"; // This device's unique ID, matches what you send from the web UI

// Amazon Root CA Certificate for secure connection validation
// IMPORTANT: The certificate you previously provided is NOT the Amazon Root CA.
// It appears to be a self-signed certificate for "ApiGateway", which will NOT work for validating
// the connection to your AWS API Gateway endpoint.
//
// YOU MUST REPLACE THE PLACEHOLDER BELOW with the actual Base-64 encoded string of the Amazon Root CA.
// Follow the detailed instructions below the code block to obtain the CORRECT Amazon Root CA certificate.
//
// NOTE: This is currently commented out and setInsecure() is used below to resolve compilation issues.
// For production, you MUST uncomment client.setCACert() and provide the correct Root CA.
const char* root_ca_certificate = \
"-----BEGIN CERTIFICATE-----\n" \
"YOUR_AMAZON_ROOT_CA_CERTIFICATE_CONTENT\n" \
"-----END CERTIFICATE-----\n"; // <<<<<<< REPLACE THIS ENTIRE LINE WITH THE ACTUAL ROOT CA CERTIFICATE CONTENT

// The certificate content you provided previously (COMMENTED OUT as it's not the correct Root CA)
/*
const char* incorrect_api_gateway_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIC6TCCAdGgAwIBAgIJANA5zCOkKbpDMA0GGCSqGSIb3DQEBCwUAMDQxCzAJBgNV\n" \
"BAYTAlVTMRAwDgYDVQQHEwdTZWF0dGxlMRMwEQYDVQQDEwpBcGlHYXRld2F5MB4X\n" \
"DTI1MDcyNTEyMzUxMFoXDTI2MDcyNTEyMzUxMFowNDELMAkGA1UEBhMCVVMxEDAO\n" \
"BgNVBAcTB1NlYXR0bGUxEzARBgNVBAMTCkFwaUdhdGV3YXkwggEiMA0GCSqGSIb3\n" \
"DQEBAQUAA4IBDwAwggEKAoIBAQDDArfG/j1YghMeOZEiyyyNIy1lV2hfc0UpdJTq\n" \
"3u/9navv+Casf4JvEibfR5irTSFcFnOy9l++mrVIMQ3XdLD1UuMNRYcYlquRStc/\n" \
"HdUYQawu/V+iUVqsNIme10iHSEDmgQ/lFuNW87n0hDI/p09ALleAjXNKMR2zguSU\n" \
"VOFMBxPJGJvMRKuD5WqboAAC7QVYTMqbAe3cDaBPou8MZ/fBJdx2nJuNyeS6aImV\n" \
"BFZj/fht9AVikZ+Up2LbXn04yI0HhcSAML9mxb9XXtoDKAJiV8zuOGHYJkhclf6N\n" \
"vh6YPuizuMAfmT29IEKlIsAr9XbComeyQMH+8EQIwPezDXeJAgMBAAEwDQYJKoZI\n" \
"hvcNAQELBQADggEBAHIQwd0Ttb+CrjwyjM0EgoN0BQ083PLw1ufxzLzpKxDG+6Yx\n" \
"BMblqEGrvT1PqGSXwolLmPgIr/BUJYi1msUUOcmqFHope1bJJTnt1jMBwrqsmfjf\n" \
"NbeQFcZRsxPwtdz9lmtd+SI1oUbOSA7T8QyqP0AWnVMUz4/JfI2iDAGrsyfrvS6V\n" \
"0Ydee98Go3E3miiuLZjlW1+/3T+1XzuJO6kdU/V39lN9jT2BTl9DN4+K0HCTHfkJ\n" \
"CjCOQTxa23VWnm/Hkv7AnoaxBa43jfcQ/FX6Jj+PVPZzXOWQk8xUDLu4pzOAYe00\n" \
"ediJbncNo6/IXKLfI54h6lUEjzG+q36BibQDg60=\n" \
"-----END CERTIFICATE-----\n";
*/

// Built-in LED pin (varies by ESP8266 board)
// For NodeMCU/Wemos D1 Mini, the built-in LED is usually on GPIO 2 (D4) and is active LOW.
// For ESP-01, it's also GPIO 2.
const int LED_PIN = 2; // GPIO2, often D4 on NodeMCU/Wemos D1 Mini

// Interval for fetching settings (in milliseconds)
const long fetchInterval = 10000; // Fetch every 10 seconds (10000 ms)
// --- CONFIGURATION END ---

unsigned long lastFetchTime = 0;

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // Turn off LED initially (active LOW, so HIGH means OFF)

    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) { // Try connecting for up to 10 seconds
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFailed to connect to WiFi. Please check credentials and try again.");
        Serial.println("Restarting in 5 seconds...");
        delay(5000);
        ESP.restart(); // Restart the ESP if it can't connect to WiFi
    }
}

void loop() {
    // Check if it's time to fetch new settings and if WiFi is connected
    if (WiFi.status() == WL_CONNECTED && (millis() - lastFetchTime > fetchInterval || lastFetchTime == 0)) {
        fetchSettings();
        lastFetchTime = millis();
    }

    // Add other reflector control logic here
    // e.g., reading sensors, controlling motors based on received settings
    // This loop should run quickly and not block for long operations.
}

void fetchSettings() {
    Serial.println("\nAttempting to fetch settings...");

    WiFiClientSecure client;

    // IMPORTANT: This line is commented out because your Arduino IDE's ESP8266 core
    // is reporting that it does not have the setCACert() method.
    // client.setCACert(root_ca_certificate);

    // For compilation, we are temporarily using setInsecure().
    // This bypasses SSL certificate validation and is INSECURE for production.
    // You MUST update your ESP8266 board package in Arduino IDE to use setCACert() for security.
    client.setInsecure(); // ONLY FOR DEBUGGING/TESTING, REMOVE IN PRODUCTION!

    if (!client.connect(host, 443)) {
        Serial.println("Connection to API Gateway failed!");
        // If this error persists even with setInsecure(), check network connectivity or host/path.
        return;
    }

    // Construct the GET request URL with deviceId as a query parameter
    String url = String(path) + "?deviceId=" + deviceId;
    Serial.print("Requesting URL: ");
    Serial.println(url);

    // Send the HTTP GET request
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "User-Agent: ESP8266\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 10000) { // 10 second timeout for response
            Serial.println(">>> Client Timeout ! No response from API Gateway.");
            client.stop();
            return;
        }
    }

    // Read HTTP response headers and body
    String responseBody = "";
    bool inBody = false;
    while (client.connected() || client.available()) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") { // End of headers
                inBody = true;
                continue;
            }
            if (inBody) {
                responseBody += line;
            } else {
                Serial.print("Header: ");
                Serial.println(line);
            }
        }
    }
    client.stop(); // Close the connection

    Serial.print("Received JSON: ");
    Serial.println(responseBody);

    // Parse JSON response using ArduinoJson
    // Use a StaticJsonDocument or DynamicJsonDocument based on your memory needs.
    // For larger responses, DynamicJsonDocument is better. 512 bytes is a good start.
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, responseBody);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    // Extract values from the JSON document
    bool ledState = doc["ledState"];
    String operationMode = doc["operationMode"];
    int targetAngle = doc["targetAngle"];
    int lightThreshold = doc["lightThreshold"];

    Serial.print("Parsed LED State: ");
    Serial.println(ledState ? "ON" : "OFF");
    Serial.print("Parsed Operation Mode: ");
    Serial.println(operationMode);
    Serial.print("Parsed Target Angle: ");
    Serial.println(targetAngle);
    Serial.print("Parsed Light Threshold: ");
    Serial.println(lightThreshold);

    // Control the built-in LED based on ledState
    // ESP8266 built-in LED is often active LOW (HIGH means OFF, LOW means ON)
    if (ledState) {
        digitalWrite(LED_PIN, LOW); // Turn LED ON
    } else {
        digitalWrite(LED_PIN, HIGH); // Turn LED OFF
    }

    // --- Add your solar reflector control logic here ---
    // Example:
    // if (operationMode == "auto") {
    //     // Implement automatic tracking logic using lightThreshold
    //     // e.g., read LDR sensor, adjust servo motor
    // } else if (operationMode == "manual") {
    //     // Move reflector to targetAngle using a servo motor
    // } else if (operationMode == "sleep") {
    //     // Put reflector in a safe, power-saving position
    // }
    // ---------------------------------------------------
}
