#ifndef RDMHANDLER_H
#define RDMHANDLER_H

#include <stdint.h>
#include "ESP32DMX.h"

#define RDM_SUB_START_CODE 0x01
#define RDM_DISCOVERY_COMMAND 0x10
#define RDM_GET_COMMAND 0x20
#define RDM_SET_COMMAND 0x30
#define RDM_DISCOVERY_RESPONSE 0x11
#define RDM_GET_COMMAND_RESPONSE 0x21
#define RDM_SET_COMMAND_RESPONSE 0x31

#define PARAM_DEVICE_INFO 0x0060
#define PARAM_DMX_START_ADDRESS 0x00F0
#define PARAM_IDENTIFY_DEVICE 0x1000

#define RDM_UID_LENGTH 6

struct RDMUID {
    uint8_t id[RDM_UID_LENGTH];
};

struct RDMHeader {
    uint8_t startCode;
    uint8_t subStartCode;
    uint8_t messageLength;
    RDMUID destinationUID;
    RDMUID sourceUID;
    uint8_t transactionNumber;
    uint8_t portID;
    uint8_t messageCount;
    uint16_t subDevice;
    uint8_t commandClass;
    uint16_t parameterID;
    uint16_t parameterLength;
};

struct DeviceInfo {
    char manufacturer[32];
    char model[32];
    char label[32];
    uint16_t dmxStartAddress;
    uint32_t powerCycles;
    bool identifyMode;
};

class RDMHandler {
public:
    RDMHandler();
    ~RDMHandler();

    void begin();
    void begin(ESP32DMX* dmx);
    void update();
    void handleCommand(uint8_t* data, uint16_t length);
    void setDeviceInfo(const char* manufacturer, const char* model, const char* label);
    void setDMXStartAddress(uint16_t address);
    void enableDiscovery(bool enable);

private:
    ESP32DMX* dmxPort;
    RDMUID uid;
    DeviceInfo deviceInfo;
    bool discoveryEnabled;
    bool respondToDiscovery;
    uint8_t transactionNumber;

    void generateUID();
    bool validateHeader(RDMHeader* header);
    bool isRDMBroadcast(RDMUID* testUID);
    void prepareResponse(RDMHeader* header, uint8_t cmdClass);
    void handleDiscovery(RDMHeader* header);
    void handleGetCommand(RDMHeader* header);
    void handleSetCommand(RDMHeader* header);
    void handleDeviceInfo(RDMHeader* header);
    void handleDMXStartAddress(RDMHeader* header);
    void handleIdentifyDevice(RDMHeader* header);
};

#endif // RDMHANDLER_H