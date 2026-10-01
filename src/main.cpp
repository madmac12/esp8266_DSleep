// Example testing sketch for various DHT humidity/temperature sensors
// Written by ladyada, public domain

// REQUIRES the following Arduino libraries:
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
// - Adafruit Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor

#include <Arduino.h>
/*
 *  ESP32 WiFi Connect To an Existing Network
 *  Full Tutorial @ https://deepbluembedded.com/esp32-wifi-library-examples-tutorial-arduino/
 */

#include <WiFi.h>

// Replace with your own network credentials
const char* ssid = "MM331";
const char* password = "macM&M0712";

void setup(){

    Serial.begin(115200);
    delay(2000);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("\nConnecting to WiFi Network ..");

    while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(1000);
    }

    Serial.println("\nConnected to the WiFi network");
    Serial.print("Local ESP32 IP: ");
    Serial.println(WiFi.localIP());
}

void loop(){
     Serial.println("Ds loop");
      delay(2000);
}