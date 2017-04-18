#include "MQTTbroker.h"

#ifdef DEBUG_ESP_PORT
void PrintHex8(uint8_t *data, uint8_t length) 
{ 
  DEBUG_ESP_PORT.print("[");
  for (int i=0; i<length; i++) {
         if (data[i]<0x10) { DEBUG_ESP_PORT.print("0"); }
         DEBUG_ESP_PORT.print(data[i],HEX); DEBUG_ESP_PORT.print(" ");
  }
  DEBUG_ESP_PORT.print("]\n\n");
}
#define DEBUG_MQTTBROKER(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#define DEBUG_MQTTBROKER_HEX(...) PrintHex8( __VA_ARGS__ )
#else
#define DEBUG_MQTTBROKER(...)
#define DEBUG_MQTTBROKER_HEX(...)
#endif

/*
 * Public
 */
 
MQTTbroker::MQTTbroker(WebSocketsServer * webSocket){
    WS = webSocket;
}

void MQTTbroker::setCallback(callback_t cb){
    callback = cb;
    runCallback = false;
}

void MQTTbroker::unsetCallback(void){
    callback = NULL;
    runCallback = false;
}

void MQTTbroker::begin(void){
    uint8_t i,j;
    for (i=0;i<MQTTBROKER_CLIENT_MAX+1;i++){
        MQTTclients[i].status = false;
        for(j=0;j<MQTTBROKER_CLIENT_SUBSCRIPTIONS_MAX;j++) MQTTclients[i].subscriptions[j].status = false;        
    }
}

void MQTTbroker::parsing(uint8_t num, uint8_t * payload, uint8_t length){
    if (numIsIncorrect(num)) return;
    delay(0);
    
    switch(*payload>>4){
    case CONNECT: //1
      {
        uint8_t variable_header[2];
        uint8_t Protocol_level = payload[8];
        uint8_t Connect_flags = payload[9];

        //if (Connect_flags&CLEAN_SESSION) 
        variable_header[0] = 0x01&SESSION_PRESENT_ZERO; //Anyway create a new Session

        if (Connect_flags&WILL_FLAG){}
        
        if (Connect_flags&WILL_QOS){}
        
        if (Connect_flags&WILL_RETAIN){}
        
        if (Connect_flags&PASSWORD_FLAG){}

        if (Connect_flags&USER_NAME_FLAG){}

        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][RECEIVE] CONNECT [Protocol level = %d, Connect flags = %X]\n",num,Protocol_level,Connect_flags);
        DEBUG_MQTTBROKER_HEX(payload,length);
        
        if (Protocol_level==MQTT_VERSION_3_1_1){
           variable_header[1] = CONNECT_ACCEPTED;
           sendAnswer(num, CONNACK, 0, 2, variable_header, 2);     
           connect(num);
        } else {
           variable_header[1] = CONNECT_REFUSED_UPV; 
           sendAnswer(num, CONNACK, 0, 2, variable_header, 2);
           disconnect(num);
        }
      }
    break;
    case PUBLISH: //3
      {
        uint8_t DUP = (*payload>>3)&0x1;
        uint8_t QoS = (*payload>>1)&0x3;
        uint8_t RETAIN = (*payload)&0x1;
        uint8_t Remaining_length = payload[1];
        uint16_t Length_topic_name = MSB_LSB(&payload[2]); 
        if (Length_topic_name>255) Length_topic_name = MQTTBROKER_TOPIC_MAX_LENGTH; //mistake in IoT manager!!!
        
        uint8_t * Packet_identifier = NULL;
        uint8_t Packet_identifier_length = 0;

        if (QoS>0) { 
          Packet_identifier = &payload[4+Length_topic_name];
          Packet_identifier_length = 2;
        } // else without packet identifier
        
        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][RECEIVE] PUBLISH [DUP = %d, QoS = %d, RETAIN = %d, Rem_len = %d, Topic_len = %d]\n",num,DUP,QoS,RETAIN,Remaining_length,Length_topic_name);
        DEBUG_MQTTBROKER_HEX(payload,length);  
        
        publish(num,&payload[4],Length_topic_name,Packet_identifier,&payload[4+Packet_identifier_length+Length_topic_name],Remaining_length-2-Packet_identifier_length-Length_topic_name,RETAIN);
        
        switch(QoS){
        case 0: break; //Don't need answer
        case 1: //Need PUBACK... will be later
          sendAnswer(num, PUBACK);
        break; 
        case 2: //Need PUBREC, PUBREL, PUBCOMP... will be later
          sendAnswer(num, PUBREC);
          sendAnswer(num, PUBREL);
          sendAnswer(num, PUBCOMP);
        break;
        }
        
        if (runCallback) {
          delay(0);
          callback(data_to_string(&payload[4], Length_topic_name), &payload[4+Packet_identifier_length+Length_topic_name],Remaining_length-2-Packet_identifier_length-Length_topic_name);   
          runCallback = false;
        }    
      }
    break;
    case SUBSCRIBE: //8
      {
        uint16_t Packet_identifier = MSB_LSB(&payload[2]); 
        uint16_t Length_MSB_LSB = MSB_LSB(&payload[4]);
        uint8_t Requesteed_QoS = payload[6+Length_MSB_LSB];
        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][RECEIVE] SUBSCRIBE [Packet identifier = %d, Length = %d, Requested QoS = %d]\n",num,Packet_identifier,Length_MSB_LSB,Requesteed_QoS);
        DEBUG_MQTTBROKER_HEX(payload,length);
        if (!subscribe(num,&payload[6],Length_MSB_LSB)) {
          Requesteed_QoS = 0x80; //Failure
        }
        sendAnswer(num, SUBACK, 0, 3, &payload[2], 2, &Requesteed_QoS, 1);  
      }
    break;
    case UNSUBSCRIBE: //10
      {
        uint16_t Packet_identifier = MSB_LSB(&payload[2]); 
        uint16_t Length_MSB_LSB = MSB_LSB(&payload[4]); 
        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][RECEIVE] UNSUBSCRIBE [Packet identifier = %d, Length = %d]\n",num,Packet_identifier,Length_MSB_LSB);
        DEBUG_MQTTBROKER_HEX(payload,length);
        unsubscribe(num,&payload[6],Length_MSB_LSB);
        sendAnswer(num, UNSUBACK, 0, 2, &payload[2], 2);
      }
    break;
    case PINGREQ: //12
      {
        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][RECEIVE] PINGREQ\n",num);
        DEBUG_MQTTBROKER_HEX(payload,length);
        sendAnswer(num, PINGRESP);
      }  
    break;
    case DISCONNECT: //14
      {
        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][RECEIVE] DISCONNECT\n",num);
        DEBUG_MQTTBROKER_HEX(payload,length);
        disconnect(num);
      } 
    break;
    default: 
      {
        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][RECEIVE] UNKNOWN COMMAND\n",num);
        DEBUG_MQTTBROKER_HEX(payload,length);
      }
    }
}

void MQTTbroker::publish(String topic, uint8_t* payload, uint8_t length, bool retain){
    publish(MQTTBROKER_MY_NUM,(uint8_t *)&topic[0],topic.length(),NULL,payload,length,retain);
}

void MQTTbroker::subscribe(String topic){
    subscribe(MQTTBROKER_MY_NUM, (uint8_t *)&topic[0], topic.length());
}

void MQTTbroker::unsubscribe(String topic){
    unsubscribe(MQTTBROKER_MY_NUM, (uint8_t *)&topic[0], topic.length());
}

void MQTTbroker::disconnect(uint8_t num){
    if (numIsIncorrect(num)) return;
    unsubscribe(num);
    MQTTclients[num].status = false;
    WS->disconnect(num);    
}

bool MQTTbroker::clientIsConnected(uint8_t num){
  if (num==MQTTBROKER_MY_NUM) return true; //Always true
  if (numIsIncorrect(num)) return false;
  return MQTTclients[num].status;
}

/*
 * Private
 */

String MQTTbroker::data_to_string(uint8_t * data, uint8_t length){
    String str;
    str.reserve(length);
    for (uint8_t i = 0; i < length; i++)
      str += (char)data[i];
    return str;
}

bool MQTTbroker::numIsIncorrect(uint8_t num){
    if(num >= MQTTBROKER_CLIENT_MAX) {
        return true;
    } else {
        return false;
    }
}

bool MQTTbroker::compare(uint8_t * topic_name, uint8_t length_topic_name, MQTTbroker_subscribe_t * sub){
    uint8_t cs=0;
    uint8_t ct=0;
    
    for (ct=0;ct<length_topic_name;ct++){
        if (cs>=sub->subscribe_topic_length) return false;
        if (topic_name[ct]==sub->subscribe_topic_name[cs]) {
            cs++;
        } else {
            if (sub->subscribe_topic_name[cs]==0x2B) {
                if ((topic_name[ct]==0x2F)
                  &&(cs<sub->subscribe_topic_length-1)
                  &&(sub->subscribe_topic_name[cs+1]==0x2F)) { 
                    cs+=2;
                  }
            } else return false;                   
        }        
    }

    if ((sub->subscribe_topic_name[cs]==0x2B)&&(cs==sub->subscribe_topic_length-1)) return true;
    if (cs<sub->subscribe_topic_length) return false; 
    if (cs==sub->subscribe_topic_length) return true;
    
    return false;
}

bool MQTTbroker::publish(uint8_t num, uint8_t * topic_name, uint8_t length_topic_name, uint8_t * packet_identifier, uint8_t * payload, uint8_t length_payload, bool retain){
    if(length_topic_name>MQTTBROKER_TOPIC_MAX_LENGTH) {
        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][PUBLISH] TOO LONG TOPIC NAME\n\n",num);
        return false; 
    }

    for (uint8_t i=0;i<=MQTTBROKER_CLIENT_MAX;i++){
        if(i==num) continue;
        if(clientIsConnected(i)) {
            for (uint8_t j=0;j<MQTTBROKER_CLIENT_SUBSCRIPTIONS_MAX;j++){
                if (MQTTclients[i].subscriptions[j].status) {
                    delay(0);
                    if(compare(topic_name, length_topic_name, &MQTTclients[i].subscriptions[j])) {
                        if(i==MQTTBROKER_MY_NUM){
                            if (callback) runCallback = true;
                        } else {
                            sendMessage(i,topic_name,length_topic_name,payload,length_payload);
                        }
                    }
                }
            }   
        }
        delay(0);
    }

    if (retain) {
    //save to retained array
    }

    return true;
}

bool MQTTbroker::sendMessage(uint8_t num, uint8_t * topic_name, uint8_t length_topic_name, uint8_t * payload, uint8_t length_payload){
    uint8_t i;
    uint8_t remaining_length = length_topic_name + length_payload + 2;
    uint8_t answer_msg[remaining_length];
    
    answer_msg[0] = (PUBLISH<<4)|0x00; //DUP, QoS, RETAIN 
    answer_msg[1] = remaining_length;
    answer_msg[2] = 0;
    answer_msg[3] = length_topic_name;
    for (i=4;i<length_topic_name+4;i++) answer_msg[i] = *(topic_name++);
    for (i=length_topic_name+4;i<remaining_length+2;i++) answer_msg[i] = *(payload++);

    delay(0);
    DEBUG_MQTTBROKER("[MQTT_BROKER][%d][SENDMESSAGE]\n",num); 
    DEBUG_MQTTBROKER_HEX((uint8_t *) &answer_msg,remaining_length+2); 
    WS->sendBIN(num, (const uint8_t *) &answer_msg, remaining_length+2);
}

bool MQTTbroker::subscribe(uint8_t num, uint8_t * topic, uint8_t length){
    uint8_t i, clear=MQTTBROKER_CLIENT_SUBSCRIPTIONS_MAX;
   
    if(length>=MQTTBROKER_TOPIC_MAX_LENGTH) {
        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][SUBSCRIBE] TOO LONG TOPIC NAME\n\n");
        return false; 
    }
    
    for(i=0;i<MQTTBROKER_CLIENT_SUBSCRIPTIONS_MAX;i++){
        if(!MQTTclients[num].subscriptions[i].status) {
            if (clear==MQTTBROKER_CLIENT_SUBSCRIPTIONS_MAX) clear = i;
        } else {
            if(memcmp(MQTTclients[num].subscriptions[i].subscribe_topic_name,topic,length)==0) {          
                DEBUG_MQTTBROKER("[MQTT_BROKER][%d][SUBSCRIBE] ALREADY SUBSCRIBED\n\n");
                return true;          
            }
        }
    }

    if(clear==MQTTBROKER_CLIENT_SUBSCRIPTIONS_MAX){
        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][SUBSCRIBE] MASSIVE IS FULL\n\n");
        return false; 
    }
    
    MQTTclients[num].subscriptions[clear].status = true;

    for(i=0;i<length;i++){
        MQTTclients[num].subscriptions[clear].subscribe_topic_name[i] = topic[i];
    }

    MQTTclients[num].subscriptions[clear].subscribe_topic_length = length;

    DEBUG_MQTTBROKER("[MQTT_BROKER][%d][SUBSCRIBE] SUCCESSFUL [Index=%d]\n",num,clear); 
    DEBUG_MQTTBROKER_HEX((uint8_t *) &MQTTclients[num].subscriptions[clear].subscribe_topic_name,length);
}

void MQTTbroker::unsubscribe(uint8_t num, uint8_t * topic, uint8_t length){
    uint8_t i;
    if ((topic == NULL)&&(length==0)) {
        for(i=0;i<MQTTBROKER_CLIENT_SUBSCRIPTIONS_MAX;i++) MQTTclients[num].subscriptions[i].status = false;
        DEBUG_MQTTBROKER("[MQTT_BROKER][%d][UNSUBSCRIBE] ALL\n\n",num);
    } else {
        for(i=0;i<MQTTBROKER_CLIENT_SUBSCRIPTIONS_MAX;i++){
            if(memcmp(MQTTclients[num].subscriptions[i].subscribe_topic_name,topic,length)==0) {          
                  MQTTclients[num].subscriptions[i].status = false;
                  DEBUG_MQTTBROKER("[MQTT_BROKER][%d][UNSUBSCRIBE] SUCCESSFUL [Index=%d]\n\n",num,i); 
            }
        }
    }
}

void MQTTbroker::connect(uint8_t num){
    MQTTclients[num].status = true;
}

uint16_t MQTTbroker::MSB_LSB(uint8_t * msb_byte){
    return (uint16_t)(*msb_byte<<8)+*(msb_byte+1);
}

void MQTTbroker::sendAnswer(uint8_t num, uint8_t fixed_header_comm, uint8_t fixed_header_lsb, uint8_t fixed_header_remaining_length, uint8_t * variable_header, uint8_t variable_header_length, uint8_t * payload, uint8_t payload_length){
    uint8_t i;
    uint8_t answer_msg[fixed_header_remaining_length+2];
    
    delay(0);
    
    answer_msg[0] = (fixed_header_comm<<4)|fixed_header_lsb; 
    answer_msg[1] = fixed_header_remaining_length;
    for (i=2;i<variable_header_length+2;i++) answer_msg[i] = *(variable_header++);
    
    switch(fixed_header_comm){
    case CONNACK: //2   
      DEBUG_MQTTBROKER("[MQTT_BROKER][%d][SEND] CONNACK\n",num);
    break;
    case PUBACK: //4 QoS level 1
    break;
    case PUBREC: //5 QoS level 2, part 1 
    break;
    case PUBREL: //6 QoS level 2, part 2
    break;
    case PUBCOMP: //7 QoS level 2, part 3
    break;
    case SUBACK: //9      
      answer_msg[i] = *payload;
      DEBUG_MQTTBROKER("[MQTT_BROKER][%d][SEND] SUBACK\n",num);
    break;
    case UNSUBACK: //11      
      DEBUG_MQTTBROKER("[MQTT_BROKER][%d][SEND] UNSUBACK\n",num);
    break;
    case PINGRESP: //13      
      DEBUG_MQTTBROKER("[MQTT_BROKER][%d][SEND] PINGRESP\n",num);
    break;
    default: return; 
    }    
    DEBUG_MQTTBROKER_HEX((uint8_t *) &answer_msg,fixed_header_remaining_length+2);
    WS->sendBIN(num, (const uint8_t *) &answer_msg, fixed_header_remaining_length+2);
}


