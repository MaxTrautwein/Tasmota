#include <modbus_bridge.h>
#include <language/en_GB.h>
#include <tasmota.h>
#include <tasmota_globals.h>
#include <tasmota_template.h>


/**
 * @brief Check if a Token matches a Key
 * 
 * @param json Json String
 * @param tok Single Token
 * @param s Key
 * @return int 0 if tok matches s. Otherwise -1
 */
int ModbusMqttBridge::jsoneq(const char* json, jsmntok_t* tok, const char* s) {
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->len &&
        strncmp(json + tok->start, s, tok->len) == 0) {
        return 0;
    }
    return -1;
}

/**
 * @brief Get the Key Value Byte
 * 
 * @param tokenCnt Total number of Tokens
 * @param Tokens Token Array
 * @param key Key to get
 * @param json Json String
 * @param Outdata Value
 * @return int Length
 */
int ModbusMqttBridge::GetKeyValue(int tokenCnt, jsmntok_t Tokens[],const char* key, const char* json, byte& Outdata) {
    int len = -1;
    for (int i = 1; i < tokenCnt; i++) {
        if (jsoneq(json, &Tokens[i], key) == 0) {
            len = Tokens[i + 1].len;
            char* tOutData = (char*)malloc((len + 1) * sizeof(char));
            strncpy(tOutData, json + Tokens[i + 1].start, len);
            strncpy(tOutData + len, "\0", 1);
            Outdata =(byte) strtoul(tOutData,NULL, 16) ;
            free(tOutData);
        }
    }
    return len;
}

/**
 * @brief Get the Key Value Array of Byte
 * 
 * @param tokenCnt Total number of Tokens
 * @param Tokens Token Array
 * @param key Key to get
 * @param json Json String
 * @param Return Array of Value
 * @return int Length
 */
int ModbusMqttBridge::GetKeyValue(int tokenCnt, jsmntok_t Tokens[], const char* key, const char* json , byte Return[]  ) {
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
                Return[j] = strtoul(OutData, NULL, 16);
                free(OutData);
            }
        }
    }
    return j;
}


/**
 * @brief Init the Modbus Interface if possible
 * 
 * @return true If init was successful otherwise false
 */
bool ModbusMqttBridge::modbus_bridgeInit(){
  if (ModbusActive) return true;
  //Only run if Configured
  if (!PinUsed(GPIO_MODBUSBRIDGE_RX,0) || !PinUsed(GPIO_MODBUSBRIDGE_TX,0)) return false;

  modBusBridgeInstance = new TasmotaModbus(Pin(GPIO_MODBUSBRIDGE_RX,0) , Pin(GPIO_MODBUSBRIDGE_TX,0));
  uint8_t result = modBusBridgeInstance->Begin(Baudrate,ModbusConf);
  
  //Other Uses of TasmotaModbus seems to all do that.
  //TODO Check what its purpose is.
  //hardwareSerial is not used for the ESP32
  if (modBusBridgeInstance->hardwareSerial()) {
      ClaimSerial();
  }

  ModbusActive = true;
  return true;
}

/**
 * @brief This function provides access to recived messages.
 * 
 * @param address Modbus Address
 * @param function Modbus Function
 * @param data Upto 252 Bytes of data + 2 Bytes if include_crc == true
 * @param ec Holds the Return / Error Code of TasmotaModbus->ReceiveBuffer
 * @param include_crc if this is set to true then the CRC will be included in data, otherwise it will be omitted
 * @return byte Number of bytes recived
 */
byte ModbusMqttBridge::ModbusRx(uint8_t& address,uint8_t& function,uint8_t data[],uint8_t& ec ,bool include_crc = false ){
  byte status = 0;
  byte crcSub = include_crc ? 0 : 2;
  if(modBusBridgeInstance->ReceiveReady())
  {
    //While the theoretical max size is 256 Byte the maximum supported by TasmotaModbus is 255 Byte
    byte InputBuff[255];
    ec = modBusBridgeInstance->ReceiveBuffer(InputBuff,255);
    byte cnt= modBusBridgeInstance->ReceiveCount();   
    if (ec == 0 && cnt >= 3){
      memcpy(&address,InputBuff,sizeof(byte));
      memcpy(&function,InputBuff + 1,sizeof(byte));
      memcpy(data,InputBuff +2 ,sizeof(byte)* (cnt-2 -crcSub));
      status = cnt - crcSub;
    }
  }
  //Status Holds the number of bytes read
  return status;
}

/**
 * @brief Recives data from Modbus and Transmits it via MQTT
 * 
 */
void ModbusMqttBridge::ModbusToMQTT(){
  if (!modbus_bridgeInit()) return;
  byte address = 0;
  byte function = 0;
  byte data[ModbusMaxDataLen];
  uint8_t errorCode;
  byte rxcnt = ModbusRx(address,function,data,errorCode);
  char jdata[RxJsonBuffSize] = "";
  char jdat[RxJsonBuffSize - 50] = "";
  if (rxcnt > 0)
  {
    for (int i = 0; i < rxcnt - 2; i++) {
        sprintf(jdat, "%s\"0x%02X\"", jdat, data[i]);
        if (i + 1 < rxcnt - 2) strcat(jdat, ",");
    }
  }
  GotResponse = true; 
  sprintf(jdata, "{\"Address\": \"0x%02X\",\"Function\": \"0x%02X\",\"Status\":\"%u\",\"Data\": [%s],\"LastRequest\":\"0x%04X\"}", address, function, errorCode,jdat,lastCommand);

  MqttPublishPayload("tasmota/modbusbridge/rx",jdata);
}

/**
 * @brief Update The Modbus Configuration
 * 
 * JSON Keys with Values
 * 
 * "Baud":
 *  (0)  1200
 *  (1)  2400
 *  (2)  4800
 *  (3)  9600   (default)
 *  (4)  14400
 *  (5)  19200
 * 
 * "Config":
 *  (0)  SERIAL_8N1 (default)
 *  (1)  SERIAL_8N2
 *  (2)  SERIAL_8E1
 *  (3)  SERIAL_8E2
 *  (4)  SERIAL_8O1
 *  (5)  SERIAL_8O2
 * 
 * "Strict":
 * (0) Disable Strict Mode
 * (1) Enable Strict Mode
 * 
 * @param json JSON msg
 * @return byte
 * 0 = Pass
 * 1 = No valid Data
 * 2 = invalid Baud Index
 * 3 = invalid Config Index
 * 4 = invalid Strict Value
 */
byte ModbusMqttBridge::ModbusConfigCmd(const char* json){
  byte BaudIndex = 0;
  byte ConfigIndex = 0;
  byte StrictModeValue = 0;
  jsmn_parser p;
  jsmntok_t t[20]; 
  byte matches = 0;
  byte Updates = 0;

  jsmn_init(&p);
  int r = 0;
  r = jsmn_parse(&p,json , strlen(json), t, 20);
  if(GetKeyValue(r, t, "Baud",json,BaudIndex) > 0) {
    matches = 1;
    if (BaudIndex < (sizeof(SerialBauds)/sizeof(*SerialBauds))){
      if (Baudrate != SerialBauds[BaudIndex]) Updates++;
      Baudrate = SerialBauds[BaudIndex];
    }else{
      return 2;
    }

  }
  if(GetKeyValue(r, t, "Config",json,ConfigIndex)> 0) {
    matches += 2;
    if (ConfigIndex < (sizeof(SerialModes)/sizeof(*SerialModes))){
      if (ModbusConf != SerialModes[ConfigIndex]) Updates++;
      ModbusConf = SerialModes[ConfigIndex];
    }else{
      return 3;
    }
  }
  if(GetKeyValue(r, t, "Strict",json,StrictModeValue)> 0) {
    matches += 4;
    if (StrictModeValue == 0){
      StrictResponseMode = false;
      Updates++;
    }else if (StrictModeValue == 1){
      StrictResponseMode = true;
      GotResponse = true;
      Updates++;
    }else{
      return 4;
    }
  }

  if (Updates){
    //Close the Modbus Connection
    delete( modBusBridgeInstance);
    ModbusActive = false;
  }
  if (matches){
    return 0;
  }else{
    return 1;
  }
}


/**
 * @brief Relays messages from MQTT to Modbus
 * 
 * JSON Keys:
 * 
 * "Address"  -> 1 Byte Target Address
 * "Function" -> 1 Byte Function Code
 * "Data"     -> 1-252 Byte of Data (Array)
 * 
 * @param json Json String
 * @return byte 
 * 0 = Pass
 * 1 = Init Error
 * 2 = Invalid Request
 * 3 = StrictMode Blocked
 */
byte ModbusMqttBridge::MQTTtoModbus(const char* json){
  jsmn_parser p;
  jsmntok_t t[257]; 
  byte data[252];
  byte datalen;
  byte rc_Status = 0;

  jsmn_init(&p);
  int r = 0;
  r = jsmn_parse(&p,json , strlen(json), t, 128);
  byte address = 0;
  byte function = 0;
  if(GetKeyValue(r, t, "Address",json,address) > 0) rc_Status++;
  if(GetKeyValue(r, t, "Function",json,function)> 0) rc_Status++;
  datalen = GetKeyValue(r, t, "Data", json,data);

  if (rc_Status == 2 && datalen > 0)
  {
    byte datar[datalen];
    uint16_t crc = 0;
    memcpy(datar,data,sizeof(byte)*datalen);
    
    if (!modbus_bridgeInit()){
      return 1;
    }
    if (StrictResponseMode && GotResponse == false) return 3;
    modBusBridgeInstance->Send(address,function,datar,crc);
    //Save the Last Command
    lastCommand = crc;
    GotResponse = false;
    return 0;
  }
  return 2;
}

/**
 * @brief Get the Modbus Mqtt Bridge object
 * 
 * @return ModbusMqttBridge* Always the same instance
 */
ModbusMqttBridge* GetModbusMqttBridge(){
  static ModbusMqttBridge *classref = new ModbusMqttBridge();
  return classref;
}
