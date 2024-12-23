#include "RDMHandler.h"
#include <esp_random.h>
#include <WiFi.h>

// 构造函数，初始化成员变量
RDMHandler::RDMHandler() : 
    dmxPort(nullptr),
    discoveryEnabled(true),
    respondToDiscovery(false),
    transactionNumber(0) {
    generateUID();
    memset(&deviceInfo, 0, sizeof(DeviceInfo));
    strcpy(deviceInfo.manufacturer, "ACME");
    strcpy(deviceInfo.model, "ESP32-DMX");
    strcpy(deviceInfo.label, "DMX Node");
    deviceInfo.dmxStartAddress = 1;
    deviceInfo.powerCycles = 0;
    deviceInfo.identifyMode = false;
}

// 析构函数
RDMHandler::~RDMHandler() {
}

// 初始化函数
void RDMHandler::begin() {
    deviceInfo.powerCycles++;
}

void RDMHandler::begin(ESP32DMX* dmx) {
    dmxPort = dmx;
    begin();
}

// 周期性更新函数
void RDMHandler::update() {
    // 周期性更新，如果需要的话
}

// 处理RDM命令
void RDMHandler::handleCommand(uint8_t* data, uint16_t length) {
    if (!data || length < sizeof(RDMHeader)) return;

    RDMHeader* header = (RDMHeader*)data;
    if (!validateHeader(header)) return;

    switch (header->commandClass) {
        case RDM_DISCOVERY_COMMAND:
            if (discoveryEnabled) handleDiscovery(header);
            break;
        case RDM_GET_COMMAND:
            handleGetCommand(header);
            break;
        case RDM_SET_COMMAND:
            handleSetCommand(header);
            break;
    }
}

// 处理发现命令
void RDMHandler::handleDiscovery(RDMHeader* header) {
    if (!isRDMBroadcast(&header->destinationUID)) return;

    // 准备发现响应
    uint8_t response[sizeof(RDMHeader) + 24];
    RDMHeader* respHeader = (RDMHeader*)response;
    prepareResponse(header, RDM_DISCOVERY_RESPONSE);

    // 发送响应
    if (dmxPort) {
        dmxPort->sendRDM(response, sizeof(RDMHeader) + 24);
    }
}

// 处理GET命令
void RDMHandler::handleGetCommand(RDMHeader* header) {
    if (!dmxPort) return;
    
    if (memcmp(&header->destinationUID, &uid, sizeof(RDMUID)) != 0 && 
        !isRDMBroadcast(&header->destinationUID)) {
        return;
    }

    switch (header->parameterID) {
        case PARAM_DEVICE_INFO:
            handleDeviceInfo(header);
            break;
        case PARAM_DMX_START_ADDRESS:
            handleDMXStartAddress(header);
            break;
        // 处理其他GET参数
    }
}

// 处理SET命令
void RDMHandler::handleSetCommand(RDMHeader* header) {
    if (!dmxPort) return;
    
    if (memcmp(&header->destinationUID, &uid, sizeof(RDMUID)) != 0 && 
        !isRDMBroadcast(&header->destinationUID)) {
        return;
    }

    switch (header->parameterID) {
        case PARAM_DMX_START_ADDRESS:
            handleDMXStartAddress(header);
            break;
        case PARAM_IDENTIFY_DEVICE:
            handleIdentifyDevice(header);
            break;
        // 处理其他SET参数
    }
}

// 处理设备信息
void RDMHandler::handleDeviceInfo(RDMHeader* header) {
    struct DeviceInfoResponse {
        uint16_t protocolVersion;
        uint16_t deviceModel;
        uint16_t productCategory;
        uint32_t softwareVersion;
        uint16_t dmxFootprint;
        uint16_t dmxPersonality;
        uint16_t dmxStartAddress;
        uint16_t subDeviceCount;
        uint8_t sensorCount;
    } response;

    response.protocolVersion = 0x0100;  // RDM V1.0
    response.deviceModel = 0x0001;
    response.productCategory = 0x0101;  // DMX512 Receiver
    response.softwareVersion = 0x00000100;
    response.dmxFootprint = 512;
    response.dmxPersonality = 1;
    response.dmxStartAddress = deviceInfo.dmxStartAddress;
    response.subDeviceCount = 0;
    response.sensorCount = 0;

    // 准备并发送响应
    uint8_t respBuffer[sizeof(RDMHeader) + sizeof(DeviceInfoResponse)];
    RDMHeader* respHeader = (RDMHeader*)respBuffer;
    prepareResponse(header, RDM_GET_COMMAND_RESPONSE);
    memcpy(respBuffer + sizeof(RDMHeader), &response, sizeof(DeviceInfoResponse));
    
    if (dmxPort) {
        dmxPort->sendRDM(respBuffer, sizeof(respBuffer));
    }
}

// 处理DMX起始地址
void RDMHandler::handleDMXStartAddress(RDMHeader* header) {
    if (header->commandClass == RDM_GET_COMMAND) {
        // 准备并发送GET响应
        uint8_t respBuffer[sizeof(RDMHeader) + 2];
        RDMHeader* respHeader = (RDMHeader*)respBuffer;
        prepareResponse(header, RDM_GET_COMMAND_RESPONSE);
        uint16_t* addr = (uint16_t*)(respBuffer + sizeof(RDMHeader));
        *addr = deviceInfo.dmxStartAddress;
        
        if (dmxPort) {
            dmxPort->sendRDM(respBuffer, sizeof(respBuffer));
        }
    } else if (header->commandClass == RDM_SET_COMMAND) {
        // 处理SET命令
        uint16_t newAddress = *(uint16_t*)(((uint8_t*)header) + sizeof(RDMHeader));
        if (newAddress > 0 && newAddress <= 512) {
            deviceInfo.dmxStartAddress = newAddress;
        }
    }
}

// 处理设备识别
void RDMHandler::handleIdentifyDevice(RDMHeader* header) {
    if (header->commandClass == RDM_GET_COMMAND) {
        // 发送当前识别状态
        uint8_t respBuffer[sizeof(RDMHeader) + 1];
        RDMHeader* respHeader = (RDMHeader*)respBuffer;
        prepareResponse(header, RDM_GET_COMMAND_RESPONSE);
        respBuffer[sizeof(RDMHeader)] = deviceInfo.identifyMode ? 1 : 0;
        
        if (dmxPort) {
            dmxPort->sendRDM(respBuffer, sizeof(respBuffer));
        }
    } else if (header->commandClass == RDM_SET_COMMAND) {
        // 设置识别模式
        deviceInfo.identifyMode = *(((uint8_t*)header) + sizeof(RDMHeader)) != 0;
    }
}

// 设置设备信息
void RDMHandler::setDeviceInfo(const char* manufacturer, const char* model, const char* label) {
    strncpy(deviceInfo.manufacturer, manufacturer, sizeof(deviceInfo.manufacturer) - 1);
    strncpy(deviceInfo.model, model, sizeof(deviceInfo.model) - 1);
    strncpy(deviceInfo.label, label, sizeof(deviceInfo.label) - 1);
}

// 设置DMX起始地址
void RDMHandler::setDMXStartAddress(uint16_t address) {
    if (address > 0 && address <= 512) {
        deviceInfo.dmxStartAddress = address;
    }
}

// 启用或禁用RDM发现
void RDMHandler::enableDiscovery(bool enable) {
    discoveryEnabled = enable;
}

// 验证RDM头
bool RDMHandler::validateHeader(RDMHeader* header) {
    return header->startCode == 0xCC && 
           header->subStartCode == RDM_SUB_START_CODE &&
           header->messageLength >= sizeof(RDMHeader);
}

// 生成唯一标识符UID
void RDMHandler::generateUID() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    memcpy(uid.id, mac, 6);
    uid.id[0] = 0x77;  // 制造商ID
    uid.id[1] = 0x77;  // 可以改为你的实际制造商ID
}

// 检查是否为RDM广播
bool RDMHandler::isRDMBroadcast(RDMUID* testUID) {
    for (int i = 0; i < RDM_UID_LENGTH; i++) {
        if (testUID->id[i] != 0xFF) return false;
    }
    return true;
}

// 准备RDM响应
void RDMHandler::prepareResponse(RDMHeader* header, uint8_t cmdClass) {
    header->startCode = 0xCC;
    header->subStartCode = RDM_SUB_START_CODE;
    // 交换源和目标UID
    RDMUID tempUID = header->destinationUID;
    header->destinationUID = header->sourceUID;
    header->sourceUID = uid;
    header->commandClass = cmdClass;
    header->transactionNumber = transactionNumber++;
}