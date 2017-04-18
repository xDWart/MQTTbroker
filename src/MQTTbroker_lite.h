#ifndef MQTTBROKER_H_
#define MQTTBROKER_H_

#include <Arduino.h>
#include <WebSocketsServer.h>

#define MQTT_VERSION_3_1_1                        4

#define MQTTBROKER_CLIENT_MAX                     WEBSOCKETS_SERVER_CLIENT_MAX
#define MQTTBROKER_MY_NUM                         MQTTBROKER_CLIENT_MAX

enum { //PACKETS
    CONNECT = 1, 
    CONNACK = 2, 
    PUBLISH = 3,
    PUBACK = 4,
    PUBREC = 5, 
    PUBREL = 6,
    PUBCOMP = 7,
    SUBSCRIBE = 8,
    SUBACK = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK = 11,
    PINGREQ = 12,
    PINGRESP = 13,
    DISCONNECT = 14,
};

enum { //Connect Flags
    CLEAN_SESSION = 0x02,
    WILL_FLAG = 0x04,
    WILL_QOS = 0x18,
    WILL_RETAIN = 0x20,
    PASSWORD_FLAG = 0x40,
    USER_NAME_FLAG = 0x80,
};

enum { //CONNACK_SP
    SESSION_PRESENT_ZERO = 0, //If the Server accepts a connection with CleanSession set to 1, the Server MUST set Session Present to 0 in the CONNACK packet
    SESSION_PRESENT_ONE = 1,  //look 3.2.2.2 Session Present [mqtt-v.3.1.1-os.pdf]
};

enum { //CONNACK_RC
    CONNECT_ACCEPTED = 0,    //"Connection Accepted"
    CONNECT_REFUSED_UPV = 1, //"Connection Refused: unacceptable protocol version"
    CONNECT_REFUSED_IR = 2,  //"Connection Refused: identifier rejected"
    CONNECT_REFUSED_SU = 3,  //"Connection Refused: server unavailable"
    CONNECT_REFUSED_BUP = 4, //"Connection Refused: bad user name or password"
    CONNECT_REFUSED_NA = 5,  //"Connection Refused: not authorized"
};

typedef struct {
  bool status;
} MQTTbroker_client_t;

typedef enum {
  EVENT_CONNECT,
  EVENT_SUBSCRIBE,
  EVENT_PUBLISH,
  EVENT_DISCONNECT,
} Events_t;

typedef void(*callback_t)(uint8_t num, Events_t event , String topic_name, uint8_t * payload, uint8_t length_payload);

class MQTTbroker_lite {
    public:
        MQTTbroker_lite(WebSocketsServer * webSocket);
        void setCallback(callback_t cb);
        void unsetCallback(void);
        void begin(void);
        void parsing(uint8_t num, uint8_t * payload, uint8_t length);
        void publish(uint8_t num, String topic, uint8_t* payload, uint8_t length);
        void disconnect(uint8_t num);            
        bool clientIsConnected(uint8_t num); 
    private:
        WebSocketsServer * WS;
        callback_t callback;
        MQTTbroker_client_t MQTTclients[MQTTBROKER_CLIENT_MAX+1]; //Last is MQTTBROKER_MY_NUM
        void runCallback(uint8_t num, Events_t event , uint8_t * topic_name = NULL, uint8_t length_topic_name = 0,uint8_t * payload = NULL, uint8_t length_payload = 0);
        void sendAnswer(uint8_t num, uint8_t fixed_header_comm, uint8_t fixed_header_lsb = 0, uint8_t fixed_header_remaining_length = 0, uint8_t * variable_header = NULL, uint8_t variable_header_length = 0, uint8_t * payload = NULL, uint8_t payload_length = 0);
        void sendMessage(uint8_t num, uint8_t * topic_name, uint8_t length_topic_name, uint8_t * payload, uint8_t length_payload);
        void connect(uint8_t num);
        bool numIsIncorrect(uint8_t num);
        String data_to_string(uint8_t * data, uint8_t length);
        uint16_t MSB_LSB(uint8_t * msb_byte);
};

#endif /* MQTTBROKER_H_ */
