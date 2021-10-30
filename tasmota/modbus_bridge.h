
/*
* Accsess .ino Declerations 
*/

void MqttPublishPayload(const char* topic, const char* payload);


/*
* Methods
*/


void modbus_bridgeInit();

void MQTTtoModbus();

void ModbusToMQTT();

void MQTT_TestBridge(const char* json);