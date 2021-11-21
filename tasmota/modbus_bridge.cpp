#include <TasmotaModbus.h>
#include <modbus_bridge.h>
#include <jsmn.h>

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
            free(tOutData);
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
                free(OutData);
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

  //Using 
  //16 RX
  //17 TX
  //--> TX works fine but no RX at all
  //The new config with 
  //3 RX
  //1 TX
  // Rx and Tx work but sometimes there are some itermittend issues
  //Are those ports already used for somthing else?
  modBusBridgeInstance = new TasmotaModbus(3 , 1);
  uint8_t result = modBusBridgeInstance->Begin(9600,1);
  
  //Other Uses of TasmotaModbus seems to all do that.
  //TODO Check what its purpose is.
  if (modBusBridgeInstance->hardwareSerial()) {
      ClaimSerial();
  }

  //modBusBridgeInstance->m_hardserial
  status = 1;
}

// JSON:
// id:
// function:
// data:



//Relay data of the modbus to MQTT
// "MQTTbridge/<Device ID>"


/**
 * @brief This function provides access to recived messages.
 * 
 * @param address 
 * @param function 
 * @param data
 * @param ec Holds the Return / Error Code of TasmotaModbus->ReceiveBuffer
 * @param include_crc if this is set to true then the CRC will be included in data, otherwise it will be omitted
 * @return byte Number of bytes recived
 */
byte ModbusRx(uint8_t& address,uint8_t& function,uint8_t data[],uint8_t& ec ,bool include_crc = false ){
  byte status = 0;

  if(modBusBridgeInstance->ReceiveReady())
  {
    byte cnt= modBusBridgeInstance->ReceiveCount();
    byte InputBuff[cnt];
    ec = modBusBridgeInstance->ReceiveBuffer(InputBuff,cnt);
    memcpy(&address,InputBuff,sizeof(byte));
    memcpy(&function,InputBuff + 1,sizeof(byte));
    byte crcSub = include_crc ? 0 : 2;
    memcpy(data,InputBuff +2 ,sizeof(byte)* (cnt-2 -crcSub));
    status = cnt - crcSub;
  }

  //Status Holds the number of bytes read
  return status;
}

/**
 * @brief Recives data from Modbus and Transmits it via MQTT
 * 
 */
void ModbusToMQTT(){
   modbus_bridgeInit();
  byte address = 0;
  byte function = 0;
  byte data[252];
  uint8_t errorCode;
  byte rxcnt =  ModbusRx(address,function,data,errorCode);
  if (rxcnt > 0)
  {
    char jdata[RxJsonBuffSize] = "";
    char jdat[RxJsonBuffSize - 50] = "";
    for (int i = 0; i < rxcnt - 2; i++) {
        sprintf(jdat, "%s\"0x%02X\"", jdat, data[i]);
        if (i + 1 < rxcnt - 2) strcat(jdat, ",");
    }
    sprintf(jdata, "{\"Address\": \"0x%02X\",\"Function\": \"0x%02X\",\"Status\":\"%u\",\"Data\": [%s]}", address, function, errorCode,jdat);
    
    MqttPublishPayload("tasmota/modbusbridge/rx",jdata);
  }
}


/**
 * @brief Relays messages from MQTT to Modbus
 * 
 * @param json 
 */
void MQTTtoModbus(const char* json){
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
    modBusBridgeInstance->Send(address,function,datar);
  }
}
