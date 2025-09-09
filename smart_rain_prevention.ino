#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>

// ---------------- WiFi ----------------
const char* ssid = "yantrigo";
const char* password = "rx*yx14*rx";

// ---------------- MQTT ----------------
const char* mqtt_server = "test.mosquitto.org";
const char* mqtt_topic = "smart_clothes/cmd";
WiFiClient espClient;
PubSubClient client(espClient);

// ---------------- ThingSpeak ----------------
const char* thingspeak_host = "http://api.thingspeak.com/update";
String apiKey = "SVBO243TZVQPEKP3";
WiFiClient httpClient;

// ---------------- Pins ----------------
#define ENA D8
#define IN1 D2
#define IN2 D3
#define RAIN_SENSOR D5
#define LIMIT_FORWARD D6
#define LIMIT_BACKWARD D7

// ---------------- Config ----------------
const int MOTOR_SPEED = 200;  // ~5% speed
unsigned long lastThingSpeak = 0;
const unsigned long thingSpeakInterval = 20000; // 20s

// ---------------- State ----------------
enum Motion { STOPPED, MOVING_IN, MOVING_OUT };
Motion motionState = STOPPED;
String clothesState = "STOP";

bool manual = false;          // true = obey last MQTT command, false = automatic
String lastCmd = "STOP";      // last command received via MQTT
bool wifiConnected = false;

// ---------------- Motor Control ----------------
void stopMotor() {
  analogWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  motionState = STOPPED;
  clothesState = "STOP";
}

void startMoveIn() {
  if (digitalRead(LIMIT_FORWARD) == LOW) {
    stopMotor();
    return;
  }
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, MOTOR_SPEED);
  motionState = MOVING_IN;
  clothesState = "IN";
}

void startMoveOut() {
  if (digitalRead(LIMIT_BACKWARD) == LOW) {
    stopMotor();
    return;
  }
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(ENA, MOTOR_SPEED);
  motionState = MOVING_OUT;
  clothesState = "OUT";
}

// ---------------- MQTT Callback ----------------
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  msg.toUpperCase();

  lastCmd = msg;
  manual = true;  // whenever a MQTT command comes, enter manual mode
}

// ---------------- WiFi Setup ----------------
void setup_wifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  if (wifiConnected) Serial.println("\nWiFi Connected!");
  else Serial.println("\nWiFi Connection Failed!");
}

// ---------------- MQTT Reconnect ----------------
void reconnect() {
  if (!wifiConnected) return;
  while (!client.connected()) {
    if (client.connect("SmartClothesClient")) {
      client.subscribe(mqtt_topic);
      Serial.println("MQTT Connected & Subscribed");
    } else {
      delay(2000);
    }
  }
}

// ---------------- ThingSpeak ----------------
/*void sendThingSpeak(String state) {
  if (!wifiConnected) return;

  int value = 0;
  if (state == "IN") value = 1;
  else if (state == "OUT") value = 2;

  HTTPClient http;
  String url = String(thingspeak_host) + "?api_key=" + apiKey + "&field1=" + String(value);

  http.begin(httpClient, url);
  int httpResponse = http.GET();
  if (httpResponse > 0) {
    Serial.print("ThingSpeak response code: ");
    Serial.println(httpResponse);
  } else {
    Serial.print("Error sending to ThingSpeak: ");
    Serial.println(httpResponse);
  }
  http.end();
}*/

void sendThingSpeak(String state) {
  if (!wifiConnected) return;

  int value = 0;
  if (state == "IN") value = 1;
  else if (state == "OUT") value = 2;
  else return; // Skip sending STOP state

  HTTPClient http;
  String url = String(thingspeak_host) + "?api_key=" + apiKey + "&field1=" + String(value);

  http.begin(httpClient, url);
  int httpResponse = http.GET();
  if (httpResponse > 0) {
    Serial.print("ThingSpeak response code: ");
    Serial.println(httpResponse);
  } else {
    Serial.print("Error sending to ThingSpeak: ");
    Serial.println(httpResponse);
  }
  http.end();
}


// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  setup_wifi();

  if (wifiConnected) {
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
  }

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(RAIN_SENSOR, INPUT);
  pinMode(LIMIT_FORWARD, INPUT_PULLUP);
  pinMode(LIMIT_BACKWARD, INPUT_PULLUP);

  stopMotor();
  Serial.println("Smart Rain Prevention System Ready");
}

// ---------------- Loop ----------------
void loop() {
  // MQTT handling
  if (wifiConnected) {
    if (!client.connected()) reconnect();
    client.loop();
  }

  // Stop motor if limits reached
  if (motionState == MOVING_IN && digitalRead(LIMIT_FORWARD) == LOW) stopMotor();
  if (motionState == MOVING_OUT && digitalRead(LIMIT_BACKWARD) == LOW) stopMotor();

  // Control logic
  if (manual) {
    // obey last MQTT command
    if (motionState == STOPPED) {
    if (lastCmd == "IN" && digitalRead(LIMIT_FORWARD) == HIGH) startMoveIn();
    else if (lastCmd == "OUT" && digitalRead(LIMIT_BACKWARD) == HIGH) startMoveOut();
    else if(lastCmd == "AUTO"){
      manual=false;
    }}

    if (lastCmd == "STOP") stopMotor();
  } else {
    // automatic mode
    if (motionState == STOPPED) {
      if (digitalRead(RAIN_SENSOR) == LOW && digitalRead(LIMIT_FORWARD) == HIGH) startMoveIn();
      else if (digitalRead(RAIN_SENSOR) == HIGH && digitalRead(LIMIT_BACKWARD) == HIGH) startMoveOut();
    }
  }

  // Reset manual if motor stops
  //if (manual && motionState == STOPPED) manual = false;

  // ThingSpeak update every 20s
  if (wifiConnected && millis() - lastThingSpeak > thingSpeakInterval) {
    lastThingSpeak = millis();
    sendThingSpeak(clothesState);
  }
}
