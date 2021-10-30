#include <TasmotaModbus.h>
#include <modbus_bridge.h>
#include <jsmn.h>
//What is that for?
#define XNRG_18             18

// can be user defined in my_user_config.h
#ifndef SDM72_SPEED
  #define SDM72_SPEED       9600    // default SDM72 Modbus address
#endif
// can be user defined in my_user_config.h
#ifndef SDM72_ADDR
  #define SDM72_ADDR        1       // default SDM72 Modbus address Cant this be set via a call?
#endif

TasmotaModbus *modBusBridgeInstance;


static int jsoneq(const char* json, jsmntok_t* tok, const char* s) {
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->len &&
        strncmp(json + tok->start, s, tok->len) == 0) {
        return 0;
    }
    return -1;
}

static int GetKeyValue(int tokenCnt, jsmntok_t Tokens[],const char* key, const char* json, byte& Outdata) {
    int len = -1;
    for (int i = 1; i < tokenCnt; i++) {
        if (jsoneq(json, &Tokens[i], key) == 0) {
            len = Tokens[i + 1].len;
            char* tOutData = (char*)malloc((len + 1) * sizeof(char));
            strncpy(tOutData, json + Tokens[i + 1].start, len);
            strncpy(tOutData + len, "\0", 1);
            Outdata =(byte) strtoul(tOutData, &tOutData, 16) ;
        }
    }
    return len;
}

static int GetKeyValue(int tokenCnt, jsmntok_t Tokens[], const char* key, const char* json , byte Return[]  ) {
    int j = -1;
    for (int i = 1; i < tokenCnt; i++) {
        if (jsoneq(json, &Tokens[i], key) == 0) {
            
            if (Tokens[i + 1].type != JSMN_ARRAY) {
                continue; /* We expect groups to be an array of strings */
            }
            for (j = 0; j < Tokens[i + 1].size; j++) {
                jsmntok_t* g = &Tokens[i + j + 2];
                char* OutData = (char*)malloc((4 + 1) * sizeof(char));
                strncpy(OutData, json + g->start, g->len);
                strncpy(OutData + g->len, "\0", 1);
                Return[j] = strtoul(OutData, &OutData, 16);
            }
        }
    }
    return j;
}





/**
 * @brief (TODO) Init Method. Establish Modbus connection? other stuff? 
 * 
 */
void modbus_bridgeInit(){
  static byte status = 0;
  if (status != 0) return;

  //TODO Get proper pins
  modBusBridgeInstance = new TasmotaModbus(16 , 17);
  uint8_t result = modBusBridgeInstance->Begin(9600,1);
  status = 1;
}

// JSON:
// id:
// function:
// data:


//Grab data from MQTT and send it via the modbus
// "MQTTbridge/<Device ID>"
void MQTTtoModbus(uint8_t address,uint8_t function,uint8_t data[]){
modBusBridgeInstance->Send(address,function,data);
}



//Relay data of the modbus to MQTT
// "MQTTbridge/<Device ID>"
byte ModbusRx(uint8_t& address,uint8_t& function,uint8_t data[]){
  byte status = 0;

  if(modBusBridgeInstance->ReceiveReady())
  {
    byte cnt= modBusBridgeInstance->ReceiveCount();
    byte InputBuff[cnt];
    modBusBridgeInstance->ReceiveBuffer(InputBuff,cnt);
    memcpy(&address,InputBuff,sizeof(byte));
    memcpy(&function,InputBuff + 1,sizeof(byte));
    memcpy(data,InputBuff +2 ,sizeof(byte)* (cnt-2));
    status = cnt;
  }

  //Status Holds the number of bytes read
  return status;
}

void ModbusToMQTT(){
   modbus_bridgeInit();
  byte address = 0;
  byte function = 0;
  byte data[252];
  if (ModbusRx(address,function,data) > 0)
  {

  }



}


/**
 * @brief Debug Method to test out things
 * 
 * @param data 
 * @param topic 
 */
void MQTT_TestBridge(const char* json){
  jsmn_parser p;
  jsmntok_t t[257]; 
  byte data[252];
  byte datalen;
  byte rc_Status = 0;

  jsmn_init(&p);
  int r = 0;
  r = jsmn_parse(&p,json , strlen(json), t, 128); // "s" is the char array holding the json content
  byte address = 0;
  byte function = 0;
  rc_Status = GetKeyValue(r, t, "Address",json,address);
  rc_Status += GetKeyValue(r, t, "Function",json,function);
  datalen = GetKeyValue(r, t, "Data", json,data);
  
  if (rc_Status >= 2 && datalen > 0)
  {
    byte datar[datalen];
    memcpy(datar,data,sizeof(byte)*datalen);
    modbus_bridgeInit();
    MQTTtoModbus(address,function,datar);
  }
}
