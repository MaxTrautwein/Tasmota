const uint8_t ModbusMaxDataLen = 254; //252 Data (+2 CRC)
const uint16_t RxJsonBuffSize = 50 + 8 * ModbusMaxDataLen;

/*
* Accsess .ino Declerations 
*/

void MqttPublishPayload(const char* topic, const char* payload);

void ClaimSerial(void);

int IRAM_ATTR Pin(uint32_t gpio , uint32_t index);
//int Pin(uint32_t gpio, uint32_t index);

bool PinUsed(uint32_t gpio, uint32_t index);



/*
* Methods
*/

bool modbus_bridgeInit();

void ModbusToMQTT();

byte MQTTtoModbus(const char* json);

