#pragma once

#include <Arduino.h>

struct GlobalConfig {
    // 网络配置
    char deviceName[32];
    bool dhcpEnabled;
    uint8_t staticIP[4];
    uint8_t staticMask[4];
    uint8_t staticGateway[4];
    
    // Art-Net配置
    uint8_t artnetNet;
    uint8_t artnetSubnet;
    uint8_t artnetUniverse;
    uint16_t dmxStartAddress;
    
    // DMX配置
    bool dmxEnabled;
    uint8_t dmxMode;  // 0: 单端口, 1: 双端口
    
    // 像素配置
    bool pixelEnabled;
    uint16_t pixelCount;
    uint8_t pixelType;
    uint8_t brightness;
    
    // RDM配置
    bool rdmEnabled;
    
    // 构造函数，设置默认值
    GlobalConfig() {
        strcpy(deviceName, "ESP32-ArtNode");
        dhcpEnabled = true;
        memset(staticIP, 0, sizeof(staticIP));
        memset(staticMask, 0, sizeof(staticMask));
        memset(staticGateway, 0, sizeof(staticGateway));
        
        artnetNet = 0;
        artnetSubnet = 0;
        artnetUniverse = 0;
        dmxStartAddress = 1;
        
        dmxEnabled = true;
        dmxMode = 0;
        
        pixelEnabled = true;
        pixelCount = 170;
        pixelType = 0;
        brightness = 255;
        
        rdmEnabled = true;
    }
};

// 创建全局配置实例
extern GlobalConfig gConfig;