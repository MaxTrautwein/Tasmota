

const uint8_t ModbusMaxDataLen = 60; //Theoretical limit: 252. If the value is set to low it might result in a buffer overflow
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

