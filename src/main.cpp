#include "secrets.h"
#include <string>
#include <regex>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Fs.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "FastLED.h"
using namespace std;

#define NUM_LEDS 100
CRGB leds[NUM_LEDS];

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* AWS_endpoint = AWS_ENDPOINT;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void setLEDs(char rgb[]) {

  unsigned long int hexColor = strtoul(rgb, NULL, 16);

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = hexColor;
  }

  FastLED.show();
}

void handleShadowUpdateDelta(char* topic, StaticJsonDocument<512> doc);
void handleShadowGetAccepted(char* topic, StaticJsonDocument<512> doc);

void callback(char* topic, byte* payload, int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  StaticJsonDocument<512> doc;
  deserializeJson(doc, payload, length);

  if (regex_match(topic, regex(".*/shadow/update/delta"))) {
    Serial.println("");
    Serial.print("match");
    
    handleShadowUpdateDelta(topic, doc);
  }

  if (regex_match(topic, regex(".*/shadow/get/accepted"))) {
    Serial.println("");
    Serial.print("match");
    
    handleShadowGetAccepted(topic, doc);
  }

  Serial.println("");
}

WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient);
long lastMsg = 0;

void handleShadowUpdateDelta(char* topic, StaticJsonDocument<512> doc) {

  const char* state_desired_color = doc["state"]["color"];

  char * desired_color = strdup(state_desired_color);
  setLEDs(desired_color);

  const size_t capacity = 3*JSON_OBJECT_SIZE(1);
  DynamicJsonDocument updateDoc(capacity);

  JsonObject state = updateDoc.createNestedObject("state");
  JsonObject state_reported = state.createNestedObject("reported");
  state_reported["color"] = state_desired_color;

  char updateBuffer[512];
  serializeJson(updateDoc, updateBuffer);

  client.publish("$aws/things/led-lightstrip-1/shadow/update", updateBuffer);
}

void handleShadowGetAccepted(char* topic, StaticJsonDocument<512> doc)  {

  const char* state_desired_color = doc["state"]["desired"]["color"];

  char * desired_color = strdup(state_desired_color);
  setLEDs(desired_color);

  const size_t capacity = 3*JSON_OBJECT_SIZE(1);
  DynamicJsonDocument updateDoc(capacity);

  JsonObject state = updateDoc.createNestedObject("state");
  JsonObject state_reported = state.createNestedObject("reported");
  state_reported["color"] = state_desired_color;

  char updateBuffer[512];
  serializeJson(updateDoc, updateBuffer);

  client.publish("$aws/things/led-lightstrip-1/shadow/update", updateBuffer);
}

void setup_wifi() {
  delay(10);

  espClient.setBufferSizes(512, 512);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  while(!timeClient.update()){
    timeClient.forceUpdate();
  }

  espClient.setX509Time(timeClient.getEpochTime());

  delay(200);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  // Load certificate file
  File cert = SPIFFS.open("/cert.der", "r");
  if (!cert) {
    Serial.println("Failed to open Cert file");
  }
  else {
    Serial.println("Cert file opened");
  }

  delay(200);

  if (espClient.loadCertificate(cert)) {
    Serial.println("Cert loaded");
  }
  else {
    Serial.println("Cert failed to load");
  }

  // Load private key file
  File private_key = SPIFFS.open("/private.der", "r");
  if (!private_key) {
    Serial.println("Failed to open private key file");
  }
  else {
    Serial.println("Private key file opened");
  }

  delay(200);

  if (espClient.loadPrivateKey(private_key)) {
    Serial.println("Private key loaded");
  }
  else {
    Serial.println("Private key failed to load");
  }

  // Load CA file
  File ca = SPIFFS.open("/ca.der", "r");
  if (!ca) {
    Serial.println("Failed to open CA file");
  }
  else {
    Serial.println("CA file opened");
  }

  delay(200);

  if (espClient.loadCACert(ca)) {
    Serial.println("CA loaded");
  }
  else {
    Serial.println("CA failed to load");
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESPthing")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("$aws/things/led-lightstrip-1/shadow/update/delta");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      char buf[256];
      espClient.getLastSSLError(buf,256);
      Serial.print("WiFiClientSecure SSL error: ");
      Serial.println(buf);

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  FastLED.addLeds<WS2812B, 5>(leds, NUM_LEDS);

  setup_wifi();
  reconnect();
  client.subscribe("$aws/things/led-lightstrip-1/shadow/get/accepted");
  client.publish("$aws/things/led-lightstrip-1/shadow/get", "");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 30000) {
    // Serial.println("publishing message");
    // client.publish("$aws/things/led-lightstrip-1/shadow/get", "");
    lastMsg = now;
  }
}