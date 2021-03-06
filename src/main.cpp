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

#define NUM_LEDS 150
CRGB leds[NUM_LEDS];

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

struct Config {
  char wifi_ssid[64];
  char wifi_password[64];
  char aws_iot_endpoint[64];
  char thing_name[64];
};

Config config;

void loadConfiguration(Config &config) {
  File config_file = SPIFFS.open("/config.json", "r");
  if (!config_file) {
    Serial.println("Failed to open config file");
  }
  else {
    Serial.println("Config file opened");
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, config_file);
  if (error) {
    Serial.println("Failed to read config file");
  }

  strlcpy(config.wifi_ssid, doc["wifi_ssid"], sizeof(config.wifi_ssid));
  strlcpy(config.wifi_password, doc["wifi_password"], sizeof(config.wifi_password));
  strlcpy(config.aws_iot_endpoint, doc["aws_iot_endpoint"], sizeof(config.aws_iot_endpoint));
  strlcpy(config.thing_name, doc["thing_name"], sizeof(config.thing_name));

  config_file.close();
}

void setLEDs(const char rgb[], int n) {
  unsigned long int hexColor = strtoul(rgb, NULL, 16);

  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < n) {
      leds[i] = hexColor;
    }
    else {
      leds[i] = 0x000000;
    }
  }

  FastLED.show();
}

void handleShadowGetAccepted(char* topic, StaticJsonDocument<512> doc);
void handleShadowUpdateAccepted(char* topic, StaticJsonDocument<512> doc);

void callback(char* topic, byte* payload, int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  StaticJsonDocument<512> msg;
  deserializeJson(msg, payload, length);

  if (regex_match(topic, regex(".*/shadow/update/accepted"))) {
    handleShadowUpdateAccepted(topic, msg);
  }

  if (regex_match(topic, regex(".*/shadow/get/accepted"))) {
    handleShadowGetAccepted(topic, msg);
  }

  Serial.println("");
}

WiFiClientSecure espClient;
PubSubClient client(config.aws_iot_endpoint, 8883, callback, espClient);

void updateStateColor(const char* color, int n) {
  setLEDs(color, n);

  const size_t capacity = 3*JSON_OBJECT_SIZE(2);
  DynamicJsonDocument updateDoc(capacity);

  JsonObject state = updateDoc.createNestedObject("state");
  JsonObject state_reported = state.createNestedObject("reported");
  state_reported["color"] = color;
  state_reported["number"] = n;

  char updateBuffer[512];
  serializeJson(updateDoc, updateBuffer);

  client.publish("$aws/things/led-lightstrip-1/shadow/update", updateBuffer);
}

void handleShadowGetAccepted(char* topic, StaticJsonDocument<512> msg)  {
  if (msg["state"]["desired"]) {
    const char* color = msg["state"]["desired"]["color"];
    int n = msg["state"]["desired"]["number"];
    setLEDs(color, n);
  }
}

void handleShadowUpdateAccepted(char* topic, StaticJsonDocument<512> msg) {
  if (msg["state"]["desired"]) {
    const char* color = msg["state"]["desired"]["color"];
    int n = msg["state"]["desired"]["number"];
    updateStateColor(color, n);
  }
}

void setup_wifi() {
  delay(10);

  espClient.setBufferSizes(512, 512);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(config.wifi_ssid);

  WiFi.begin(config.wifi_ssid, config.wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  setLEDs("00FF00", 1);

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

  // Load certificate file
  File cert = SPIFFS.open("/cert.der", "r");
  if (!cert) {
    Serial.println("Failed to open Cert file");
    setLEDs("FF0000", 2);
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
    setLEDs("FF0000", 2);
  }

  // Load private key file
  File private_key = SPIFFS.open("/private.der", "r");
  if (!private_key) {
    Serial.println("Failed to open private key file");
    setLEDs("FF0000", 2);
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
    setLEDs("FF0000", 2);
  }

  // Load CA file
  File ca = SPIFFS.open("/ca.der", "r");
  if (!ca) {
    Serial.println("Failed to open CA file");
    setLEDs("FF0000", 2);
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
    setLEDs("FF0000", 2);
  }

  setLEDs("00FF00", 2);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESPthing")) {
      Serial.println("connected");
      setLEDs("00FF00", 3);

      // Once connected, subscribe to shadow updates
      client.subscribe("$aws/things/led-lightstrip-1/shadow/get/accepted");
      client.subscribe("$aws/things/led-lightstrip-1/shadow/update/accepted");
      // Request the device shadow state
      client.publish("$aws/things/led-lightstrip-1/shadow/get", "");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      char buf[256];
      espClient.getLastSSLError(buf,256);
      Serial.print("WiFiClientSecure SSL error: ");
      Serial.println(buf);

      setLEDs("FF0000", 3);
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  FastLED.addLeds<WS2812B, 5, GRB>(leds, NUM_LEDS);

  setLEDs("000000", 0);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    setLEDs("FF0000", 2);
    return;
  }

  loadConfiguration(config);
  setup_wifi();
  reconnect();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();
}