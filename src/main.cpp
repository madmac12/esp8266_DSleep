/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-mqtt-publish-dht11-dht22-arduino/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
#include <Arduino.h>

#include "DHT.h"
#include <WiFi.h>
#include <Adafruit_Sensor.h>
#include <AsyncMqttClient.h>
#include <WiFiUdp.h> //time
#include <NTPClient.h>

#define WIFI_SSID "MM331"
#define WIFI_PASSWORD "macM&M0712"
// Raspberry Pi Mosquitto MQTT Broker
//#define MQTT_HOST IPAddress(192, 168, 1, XXX)
// For a cloud MQTT broker, type the domain name
#define MQTT_HOST "test.mosquitto.org"
#define MQTT_PORT 1883
#define MQTT_CLIENT macmqtt1

//  MQTT Topics
#define MQTT_PUB_TEMP "esp/mac/tp1"
#define MQTT_PUB_HUM "esp/mac/hum1"
#define MQTT_PUB_TIME "esp/mac/tz1"


//deepsleep modified
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 60       /* Time ESP32 will go to sleep (in seconds)  15*60*/

// Digital pin connected to the DHT sensor
#define DHTPIN 4  

// Uncomment whatever DHT sensor type you're using
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)   

// timestamp
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "north-america.pool.ntp.org");

RTC_DATA_ATTR int bootCount = 0;  //pour garder le compte en deep sleep
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
}

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Variables to hold sensor readings
float temp=10;
float hum=5;

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval = 100000;        // Interval at which to publish sensor readings

// functions declarations
void connectToWifi();
void connectToMqtt();
void WiFiEvent(WiFiEvent_t event);
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttPublish(uint16_t packetId);
void read_DHT22();
void print_wakeup_reason();
void readLocalTime();

void setup() {
  Serial.begin(115200);
  delay(2000);  //Take some time to open up the Serial Monitor
  dht.begin();
  delay(500);
  //Increment boot number and print it every reboot from deepsleep
  bootCount+=1;
  Serial.println("Boot number: " + String(bootCount));
  delay(1000);

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  //timeClient.begin(); pas besoin!!
  // timeClient.update();

  //esp_sleep_enable_timer_wakeup((uint64_t)1hr *60*TIME_TO_SLEEP*uS_TO_S_FACTOR 1 hr);// 4 h(4*uS_TO_S_FACTOR);  //3 hr 1.08e+10
  esp_sleep_enable_timer_wakeup((uint64_t)1*60*TIME_TO_SLEEP*uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every x hr");
  delay(2000);
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  // If your broker requires authentication (username and password), set them below
  //mqttClient.setCredentials("REPlACE_WITH_YOUR_USER", "REPLACE_WITH_YOUR_PASSWORD");
  connectToWifi();
  delay(3000);
  configTime(-5 * 3600, 3600, "north-america.pool.ntp.org");
  delay(100);
  readLocalTime();  //read time
  delay(2000);
  read_DHT22();

  Serial.println("Going to sleep now");
  delay(1000);
  Serial.flush();
  delay(500);
  esp_deep_sleep_start();
}

void loop() {
  //never call
}


void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}
void read_DHT22() {
  delay(2000);
  //temp = dht.readTemperature(); a changer
  // Read temperature as Fahrenheit (isFahrenheit = true)
  temp = dht.readTemperature(true);
  //temp+=bootCount;
  Serial.print("tp: ");
  Serial.println(temp);

  delay(1000);
  hum = dht.readHumidity(); 
  // Read temperature as Fahrenheit (isFahrenheit = true)
  //temp = dht.readTemperature(true);
  //hum+=bootCount;
  Serial.print("hum: ");
  Serial.println(hum);

  delay(1000);

  // Publish an MQTT message on topic MQTT_PUB_TEMP
  uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_TEMP, 1, true, String(temp).c_str());
  Serial.printf("Publishing on topic %s at QoS 1, packetId: %i ", MQTT_PUB_TEMP, packetIdPub1);
  Serial.printf("Message: %.2f \n", temp);
  delay(2000);
   // Publish an MQTT message on topic MQTT_PUB_TEMP
  uint16_t packetIdPub2 = mqttClient.publish(MQTT_PUB_HUM, 1, true, String(hum).c_str());
  Serial.printf("Publishing on topic %s at QoS 1, packetId: %i ", MQTT_PUB_HUM, packetIdPub2);
  Serial.printf("Message: %.2f \n", hum);
  delay(2000);
}
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
    default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void readLocalTime() {
  struct tm timeinfo;
  delay(1000);

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  //send mqtt
  char timestamp[16];
  strftime(timestamp, sizeof(timestamp), "%m/%d %H:%M", &timeinfo);
   // Publish an MQTT message on topic MQTT_PUB_TIME

  Serial.print("time :");
  Serial.println(timestamp);
  uint16_t packetIdPub = mqttClient.publish(MQTT_PUB_TIME, 1, true, timestamp);
  Serial.printf("Publishing timestamp on topic %s at QoS 1, packetId %i: %s\n", MQTT_PUB_TIME, packetIdPub, timestamp);
}
