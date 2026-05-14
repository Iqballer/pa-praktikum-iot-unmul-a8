#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>

const char* ssid = "ball";
const char* password = "tplu1234";

// MQTT 
const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);

// Telegram
String botToken = "8637281652:AAFmsUcdY_VbknK4TgyQuEfE25vNEKQxwnc";
String chatID = "-1003803563175";

// Pin
#define TRIG 5
#define ECHO 18

#define BUZZER 12

#define DHTPIN 25
#define DHTTYPE DHT22

#define SERVO_PIN 21  

DHT dht(DHTPIN, DHTTYPE);
Servo servo;

long duration;
float distance;
float suhu;

bool notifSent = false;
bool buzzerEnabled = true;
bool distanceNotifSent = false;

// MQTT Callback
void callback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message: ");
  Serial.println(message);

  // Kontrol servo
  if (String(topic) == "vyn/iot/control1") {

    if (message == "ON") {
      servo.write(90);
      Serial.println("Servo Open");
    }

    else if (message == "OFF") {
      servo.write(0);
      Serial.println("Servo Close");
    }
  }

  if (String(topic) == "vyn/iot/buzzer") {

    if (message == "ON") {
      buzzerEnabled = true;
      Serial.println("Buzzer Enabled");
    }

    else if (message == "OFF") {
      buzzerEnabled = false;
      noTone(BUZZER);
      Serial.println("Buzzer Disabled");
    }
  }
}

// Reconnect mqtt
void reconnect() {

  while (!client.connected()) {

    Serial.print("Connecting MQTT...");

    if (client.connect("ESP32Garage")) {

      Serial.println("Connected");

      client.subscribe("vyn/iot/control1");
      client.subscribe("vyn/iot/buzzer");

    } else {

      Serial.print("Failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void setup() {

  Serial.begin(115200);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(BUZZER, OUTPUT);

 servo.setPeriodHertz(50);
 servo.attach(SERVO_PIN, 500, 2400);
 servo.write(0);

  dht.begin();

  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(" Connected!");

  // mqtt
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  // Ultrasonik
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  duration = pulseIn(ECHO, HIGH);

  distance = duration * 0.034 / 2;

  // Buzzer
  if (buzzerEnabled) {

    if (distance > 100) {
      digitalWrite(BUZZER, HIGH);
      delay(400);
      digitalWrite(BUZZER, LOW);
    }

    else if (distance > 30) {
      digitalWrite(BUZZER, HIGH);
      delay(200);
      digitalWrite(BUZZER, LOW);
      distanceNotifSent = false;
    }

    else {
      digitalWrite(BUZZER, HIGH);
      delay(50);
      digitalWrite(BUZZER, LOW);

      if (!distanceNotifSent) {

        String pesan =
          "WARNING!%0AMobil terlalu dekat dengan tembok!%0AJarak: "
          + String(distance) + "%20cm";

        sendTelegram(pesan);

        distanceNotifSent = true;
      }
    }

  } else {

    noTone(BUZZER);

  }

  // suhu
  suhu = dht.readTemperature();

  // mqtt publish
  client.publish("vyn/iot/suhu", String(suhu).c_str());
  client.publish("vyn/iot/jarak", String(distance).c_str());

  // Telegram
  if (suhu >= 25 && !notifSent) {

    sendTelegram("⚠️ Suhu garasi tinggi: " + String(suhu) + "°C");

    notifSent = true;
  }

  if (suhu < 25) {
    notifSent = false;
  }

  Serial.print("Jarak: ");
  Serial.print(distance);

  Serial.print(" cm | Suhu: ");
  Serial.println(suhu);

  delay(1000);
}

void sendTelegram(String message) {

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    message.replace(" ", "%20");
    message.replace("\n", "%0A");

    String url =
      "https://api.telegram.org/bot" + botToken +
      "/sendMessage?chat_id=" + chatID +
      "&text=" + message;

    http.begin(url);

    int httpResponseCode = http.GET();

    http.end();
  }
}