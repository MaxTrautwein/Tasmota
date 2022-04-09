#include <jsmn.h>
#include <TasmotaModbus.h>


class ModbusMqttBridge{
    public:
        void ModbusToMQTT();

        byte MQTTtoModbus(const char* json);
        byte ModbusConfigCmd(const char* json);

    private:
        int jsoneq(const char* json, jsmntok_t* tok, const char* s);
        int GetKeyValue(int tokenCnt, jsmntok_t Tokens[],const char* key, const char* json, byte& Outdata);
        int GetKeyValue(int tokenCnt, jsmntok_t Tokens[],const char* key, const char* json , byte Return[]);
        bool modbus_bridgeInit();
        byte ModbusRx(uint8_t& address,uint8_t& function,uint8_t data[],uint8_t& ec ,bool include_crc);


        TasmotaModbus *modBusBridgeInstance;

        const uint8_t ModbusMaxDataLen = 254; //252 Data (+2 CRC)
        const uint16_t RxJsonBuffSize = 50 + 8 * ModbusMaxDataLen;
        long Baudrate = 9600;
        int ModbusConf = SERIAL_8N1;
        bool ModbusActive = false;
        const int SerialModes[6] = {SERIAL_8N1,SERIAL_8N2,SERIAL_8E1,SERIAL_8E2,SERIAL_8O1,SERIAL_8O2};
        const long SerialBauds[6] = {1200,2400,4800,9600,14400,19200};

        //If True a Request may only be sent after a response has been recived
        bool StrictResponseMode = true;

        //Init to true to allow initial Request
        bool GotResponse = true;
        uint16_t lastCommand = 0;
};

ModbusMqttBridge* GetModbusMqttBridge();


/*
* Accsess .ino Declerations 
*/

void MqttPublishPayload(const char* topic, const char* payload);
void ClaimSerial(void);
int IRAM_ATTR Pin(uint32_t gpio , uint32_t index);
bool PinUsed(uint32_t gpio, uint32_t index);
