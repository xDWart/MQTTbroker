#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <MQTTbroker.h>

//===================define===============
#define BLUE_ESP_LED 2
#define BLUE_NODE_LED 16
#define LED_ON LOW
#define LED_OFF HIGH

//===================const================
const char *ssid = "ESP8266";
const char *password = "12345678";
const char nWidgets = 4;

//===================vars=================
typedef struct 
{
  String sTopic;
  String stat;
} vars_type;

vars_type Var [nWidgets];
String prefix   = "/IoTmanager";    
String deviceID = "Dom"; 
String thing_config[nWidgets];

WebSocketsServer webSocket = WebSocketsServer(80,"","mqtt");
MQTTbroker Broker = MQTTbroker(&webSocket);

void pubConfig(void) {
     for (char i = 0; i < nWidgets; i++) {
        Serial.printf("Publish config "); Serial.println(thing_config[i]);
        Broker.publish((prefix + "/" + deviceID + "/config").c_str(), (uint8_t*)thing_config[i].c_str(), thing_config[i].length());
        delay(50);
     }
     
     for (char i = 0; i < nWidgets; i++) {
        Serial.printf("Publish new status for "); Serial.println(Var[i].sTopic + " value: " + Var[i].stat);
        Broker.publish((Var[i].sTopic + "/status").c_str(), (uint8_t*)Var[i].stat.c_str(), Var[i].stat.length());
        delay(50);
     }  
}

void MQTTCallback(String topic_name, uint8_t * payload, uint8_t length_payload){
        Serial.printf("Receive publish to "); Serial.print(topic_name + " ");
        if (topic_name == Var[0].sTopic + "/control") {
          Serial.print((char)*payload);
          if (*payload == '0') digitalWrite(BLUE_NODE_LED, LED_OFF);
                          else digitalWrite(BLUE_NODE_LED, LED_ON);
        } 
        Serial.println();
        
        if (topic_name == prefix) pubConfig();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
    switch(type) {
        case WStype_DISCONNECTED:
            if (Broker.clientIsConnected(num)) Broker.disconnect(num);                
            break;            
        case WStype_BIN:
            Broker.parsing(num, payload, lenght);
            break;
    }
}

String setStatus ( int s ) {
  String stat = "{\"status\":\"" + String(s) + "\"}";
  return stat;
}

void initVar() {
  DynamicJsonBuffer jsonBuffer;

  JsonObject& root1 = jsonBuffer.createObject();  
  Var[0].sTopic = prefix + "/" + deviceID + "/toggle1";
  Var[0].stat = setStatus(0);                                      
  root1["id"] = 1;
  root1["widget"] = "toggle";
  root1["topic"]  = Var[0].sTopic;
  root1["descr"]  = "Toggle 1";
  root1.printTo(thing_config[0]);

  Var[1].sTopic = prefix + "/" + deviceID + "/toggle2";
  Var[1].stat = setStatus(1);                                      
  root1["id"] = 2;
  root1["widget"] = "toggle";
  root1["topic"]  = Var[1].sTopic;
  root1["descr"]  = "Toggle 2";        
  root1.printTo(thing_config[1]);
  
  Var[2].sTopic = prefix + "/" + deviceID + "/toggle3";
  Var[2].stat = setStatus(0);                                      
  root1["id"] = 3;
  root1["widget"] = "toggle";
  root1["topic"]  = Var[2].sTopic;
  root1["descr"]  = "Toggle 3";             
  root1.printTo(thing_config[2]);

  Var[3].sTopic = prefix + "/" + deviceID + "/toggle4";
  Var[3].stat = setStatus(1);                                      
  root1["id"] = 4;
  root1["widget"] = "toggle";
  root1["topic"]  = Var[3].sTopic;
  root1["descr"]  = "Toggle 4";             
  root1.printTo(thing_config[3]);
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println();
  
  pinMode(BLUE_ESP_LED, OUTPUT);
  digitalWrite(BLUE_ESP_LED, LED_OFF);
  pinMode(BLUE_NODE_LED, OUTPUT);
  digitalWrite(BLUE_NODE_LED, LED_OFF);

  initVar();

  Serial.println("Configuring access point...");  

  WiFi.disconnect();
  WiFi.softAP(ssid, password);
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Broker.begin();
  Broker.setCallback(MQTTCallback);
  Broker.subscribe(prefix.c_str());
  Broker.subscribe((prefix + "/ids").c_str()); 
}

void loop() {
  webSocket.loop();
}

