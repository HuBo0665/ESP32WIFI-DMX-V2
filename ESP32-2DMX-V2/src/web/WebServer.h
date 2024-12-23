#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include "LittleFS.h"
#include <FS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "artnet/ArtnetNode.h"
#include "ConfigManager.h"
#include <DNSServer.h>


class WebServer {
public:
    WebServer(ArtnetNode* node);
    virtual ~WebServer();

    // 基本功能
    void begin(); 
    void update();
    void processDNS(); // 处理DNS请求

    void handleNetworkConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleArtnetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handlePixelConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);


    // AP模式相关
    void handleAPConfig(AsyncWebServerRequest* request);
    void startAP(const char* ssid, const char* password);
    void stopAP();
    bool isAPRunning() const;


    // 配置更新处理方法
    void handleConfigUpdate(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleConfig(AsyncWebServerRequest* request);
    void applyConfig();

    // 配置管理
    struct Config {
        char deviceName[32];
        char wifiSSID[32];
        char wifiPassword[32];
        bool dhcpEnabled;
        uint8_t staticIP[4];
        uint8_t staticMask[4];
        uint8_t staticGateway[4];
        uint8_t artnetNet;
        uint8_t artnetSubnet;
        uint8_t artnetUniverse;
        uint16_t dmxStartAddress;
        uint16_t pixelCount;
        uint8_t pixelType;
        bool pixelEnabled;
    };

    // AP模式配置
    struct APConfig {
        char ssid[32];
        char password[64];
        bool enabled;
    }; 

    void loadConfig();
    const Config& getConfig() const { return config; }

private:
    // 主要组件
    ArtnetNode* artnetNode;      // ArtNet节点指针
    AsyncWebServer* server;       // Web服务器指针
    AsyncWebSocket* ws;          // WebSocket指针
    DNSServer* dnsServer;        // DNS服务器指针
    Config config;               // 配置结构体
    APConfig apConfig;          // AP配置结构体

    void saveAPConfig();
    void loadAPConfig();
    void saveConfig() { saveConfigFile(); }


    // Web事件处理
    void handleWebSocket(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
    void handleUpdate(AsyncWebServerRequest* request);
    void handleReboot(AsyncWebServerRequest* request);
    void handleFactoryReset(AsyncWebServerRequest* request);

    // JSON处理
    void sendJsonResponse(AsyncWebServerRequest* request, const JsonDocument& doc);
    void parseConfig(const JsonDocument& doc);
    void createConfigJson(JsonDocument& doc);

    // WebSocket通信
    void sendStatus(AsyncWebSocketClient* client);
    void handleWsMessage(AsyncWebSocketClient* client, const char* message);
    void handleWsMessage(AsyncWebSocketClient* client, char* data);

    // 文件系统
    bool initFS();
    bool loadConfigFile();
    bool saveConfigFile();

    // 实用函数
    void notifyConfigChange();
    String ipToString(const uint8_t* ip);
    void stringToIP(const char* str, uint8_t* ip);
};