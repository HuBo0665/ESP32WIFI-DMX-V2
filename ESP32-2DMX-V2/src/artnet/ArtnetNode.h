#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "config.h"
#include "dmx/ESP32DMX.h"
#include "pixels/PixelDriver.h"
#include "rdm/RDMHandler.h"

// Art-Net 包大小常量定义
#define ART_NET_MIN_SIZE 12
#define ART_RDM_MIN_SIZE 14
#define ART_ADDRESS_MIN_SIZE 16

// Art-Net 协议常量
#define ARTNET_PORT 6454
#define ARTNET_DMX_LENGTH 512
#define ARTNET_VERSION 14

// Art-Net包类型
enum ArtNetOpCodes {
    OpPoll = 0x2000,
    OpPollReply = 0x2100,
    OpDmx = 0x5000,
    OpNzs = 0x5100,
    OpSync = 0x5200,
    OpAddress = 0x6000,
    OpInput = 0x7000,
    OpRdm = 0x8400,
    OpIpProg = 0xF800,
    OpIpProgReply = 0xF900
};

class ArtnetNode {
public:
    // 节点配置结构体
    struct Config {
        char shortName[18];
        char longName[64];
        uint8_t net;
        uint8_t subnet;
        uint8_t universe;
        uint8_t dmxMode;
        uint16_t dmxStartAddress;
        uint16_t pixelCount;
        uint8_t pixelType;
        bool mergeMode;  // HTP = true, LTP = false
    };

    // 节点状态结构体
    struct Status {
        uint8_t ip[4];
        uint8_t mac[6];
        bool dhcp;
        uint8_t firmware;
        uint8_t ports;
        uint8_t portTypes[4];
        uint8_t goodInput;
        uint8_t goodOutput;
        uint8_t status1;
        uint8_t status2;
        uint16_t estab;
        uint8_t version;
    };

    // 构造函数和析构函数
    ArtnetNode();
    ~ArtnetNode();

    // 初始化和更新
    bool begin();
    void update();

    // 配置方法
    void setConfig(const Config& config);
    const Config& getConfig() const { return config; }
    const Status& getStatus() const { return status; }

    // DMX输出控制
    void setDMXOutput(uint8_t* data, uint16_t length);
    void setPixelOutput(uint8_t* data, uint16_t length);

    // 回调函数设置
    void setDMXCallback(void (*callback)(uint16_t universe, uint8_t* data, uint16_t length));
    void setRDMCallback(void (*callback)(uint8_t* data, uint16_t length));
    void setPixelCallback(void (*callback)(uint8_t* data, uint16_t length));

protected:
    bool validatePacket(uint8_t* data, uint16_t length);

private:
    // 成员变量
    Config config;
    Status status;
    WiFiUDP udp;
    ESP32DMX* dmx;
    PixelDriver* pixels;
    bool syncMode;
    bool syncReceived;

    // 数据缓冲区
    uint8_t artnetBuffer[1024];
    uint8_t dmxBuffer[DMX_UNIVERSE_SIZE];
    uint8_t pixelBuffer[MAX_PIXELS * 3];

    // 回调函数指针
    void (*dmxCallback)(uint16_t universe, uint8_t* data, uint16_t length);
    void (*rdmCallback)(uint8_t* data, uint16_t length);
    void (*pixelCallback)(uint8_t* data, uint16_t length);

    // Art-Net包处理方法
    void handleArtDmx(uint8_t* data, uint16_t length);
    void handleArtPoll();
    void handleArtAddress(uint8_t* data, uint16_t length);
    void handleArtRdm(uint8_t* data, uint16_t length);
    void handleArtSync();

    // 辅助方法
    void sendArtPollReply();
    void updateStatus();
    void initializeDefaults();
    void updateDmxOutput();
    bool isValidArtNet(uint8_t* data, uint16_t size);

    // Art-Net ID
    static const uint8_t ARTNET_ID[8];
};