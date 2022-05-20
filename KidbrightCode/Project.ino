#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "PubSubClient.h"
#include <SimpleTimer.h>

#define SDA 4
#define SCL 5
#define ADDR 0x4D

#define pump_pin 26
#define moisture_pin 35

// ----- WiFi configuration --------------
char ssid[] = "--WIFI NAME--";           //wifi name
char pass[] = "--WIFI PASSWORD--";     //wifi password   

// ^^^^^^^^^^^^^^ NETPIE 2020 configuration ^^^^^^^^^^^^^^^^^^
const char* mqtt_server = "broker.netpie.io";
const int mqtt_port = 1883;
const char* mqtt_Client = "--DEVICE CLIENT ID--";
const char* mqtt_username = "--DEVICE TOKEN--";
const char* mqtt_password = "--DEVICE SECRET--";
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

const int analogInPin = 36;
int sensVal;
int output;
char msg[100];

WiFiClient espClient;
PubSubClient client(espClient);

SimpleTimer timer;
int t2;


StaticJsonDocument<200> data;

float readTemp(){
  byte temp_data[2];
  word  temp;
    Wire.beginTransmission(ADDR);
    Wire.write(0);
    Wire.endTransmission();
    Wire.requestFrom(0x4D, 2);
    temp_data[1] = Wire.read();
    temp_data[0] = Wire.read();
    temp = (temp_data[1]<<8)+temp_data[0];
    return to_celcius(temp);
}
float to_celcius(int tmp){
      float sum = 0; // temperature that can read
      for(int i=14; i>1;i--){ //check all of temperature-data bit
          if (bitRead(tmp, i)== 1){
              sum += pow(2, i-7);
          }
      }
      if(bitRead(tmp, 15)==1){ // check sign bit
          sum *= -1;  
      }
      return sum;
}
void send_status(String userId){
  String datastr;
  int pump_status;
  sensVal = analogRead(analogInPin);
  output = map(sensVal, 0, 1023, 100, 0);
  pump_status = digitalRead(pump_pin);
  datastr = "{\"Temperature\": "+String(readTemp())+", \"Brightness\": "+String(output)+", \"PumpStatus\": "+ String(pump_status)+", \"userId\": \""+userId+"\"}";
  datastr.toCharArray(msg, (datastr.length()+1));
  client.publish("@msg/project/status",msg);
}
void send_pump_status(){
  int pump_status;
  pump_status = digitalRead(pump_pin);
  String datastr = "{\"PumpStatus\": "+ String(pump_status)+"}";
  datastr.toCharArray(msg, (datastr.length()+1));
  client.publish("@msg/project/pump",msg);
}
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("\nMessage arrived [");
  Serial.print(topic);
  Serial.print("]\n");

  if(String(topic)=="@msg/project/command"){
    String message;
    for (int i = 0; i < length; i++) {
      message = message + (char)payload[i];
    }
    deserializeJson(data, message);
    String command = data["command"];
    String userId = data["userId"];

    if(command == "status"){
      send_status(userId);
    }
    else if(command == "pumpon"){
      digitalWrite(pump_pin, HIGH);
      send_pump_status();
      t2 = timer.setInterval(1000, autoStopPump);
    }
    else if(command == "pumpoff"){
      digitalWrite(pump_pin, LOW);
    }
  }
}
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection…");
    if (client.connect(mqtt_Client, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("@msg/project/#");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}
void autoStopPump(){
  if(digitalRead(moisture_pin)==0){
    digitalWrite(pump_pin, LOW);
  }
  if(digitalRead(pump_pin)==0){
    timer.disable(t2);
    send_pump_status();
  }
}
void sendFeedData(){
    String datastr;
    sensVal = analogRead(analogInPin);
    output = map(sensVal, 0, 1023, 100, 0);
    datastr = "{\"data\":{\"Temperature\":"+String(readTemp())+",\"Brightness\":"+String(output)+",\"PumpStatus\":"+String(digitalRead(pump_pin))+"}}";
    datastr.toCharArray(msg, (datastr.length()+1));
    client.publish("@shadow/data/update",msg);
}
void setup() {
    Wire.begin(SDA, SCL);
    Serial.begin(9600);
    pinMode(moisture_pin,INPUT);
    pinMode(pump_pin,OUTPUT);
    if (WiFi.begin(ssid, pass)) {
        while (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            Serial.print(".");
        }
    }
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    
// ^^^^^^^^^^^^^ initialization for NETPIE 2020 ^^^^^^^^^^^^^^^^^
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

  timer.setInterval(5000, sendFeedData);
}

void loop() {
  if (client.connected()) {
    client.loop();
  }
  else {
    reconnect();
  }
  timer.run();
}
