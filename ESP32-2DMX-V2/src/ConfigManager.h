#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>  // 确保包含这个头文件
#include "LittleFS.h"
#include <FS.h>

class ConfigManager {
public:
    struct Config {
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
        
        // 像素配置
        uint16_t pixelCount;
        uint8_t pixelType;
        bool pixelEnabled;
        
        // 系统配置
        bool rdmEnabled;
        uint8_t brightness;
    };

    static bool load(Config& config);
    static bool save(const Config& config);
    static void setDefaults(Config& config);
    static bool handleAPConfig(AsyncWebServerRequest *request);  // 添加声明
private:
    static const char* CONFIG_FILE;
};
