

const uint8_t ModbusMaxDataLen = 12; //Theoretical limit: 252. This should not be set below 4
const uint16_t RxJsonBuffSize = 50 + 8 * ModbusMaxDataLen;

/*
* Accsess .ino Declerations 
*/

void MqttPublishPayload(const char* topic, const char* payload);

void ClaimSerial(void);


/*
* Methods
*/


void modbus_bridgeInit();

void ModbusToMQTT();

void MQTTtoModbus(const char* json);

