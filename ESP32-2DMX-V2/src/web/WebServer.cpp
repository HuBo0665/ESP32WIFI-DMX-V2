#include "WebServer.h"
#include "ConfigManager.h"

// 构造函数，初始化成员变量
WebServer::WebServer(ArtnetNode* node)
    : artnetNode(node),
      server(new AsyncWebServer(80)),
      ws(new AsyncWebSocket("/ws")),
      dnsServer(nullptr),
      config(),        // 添加成员初始化
      apConfig()      // 添加成员初始化
{
    // 在构造函数体内进行其他初始化
    memset(&config, 0, sizeof(Config));
    strncpy(config.deviceName, DEVICE_NAME, sizeof(config.deviceName));
    config.dhcpEnabled = true;
    config.pixelCount = MAX_PIXELS;
    config.pixelEnabled = true;

    // 初始化AP配置
    memset(&apConfig, 0, sizeof(APConfig));
    loadAPConfig();
}

// 析构函数，释放资源
WebServer::~WebServer() {
    if (dnsServer) {
        delete dnsServer;
    }
    delete server;
    delete ws;
}

// 启动Web服务器
void WebServer::begin() {
    // 初始化文件系统
    if (!initFS()) {
        Serial.println("文件系统初始化失败!");
        return;
    }
    // 加载index.html文件
    File file = LittleFS.open("/web/index.html", "r");
    if (!file) {
        Serial.println("打开 /web/index.html 文件失败！");
        return;
    }

    Serial.println("成功打开 /web/index.html 文件");
    while (file.available()) {
        Serial.write(file.read());
    }
    file.close();


    // 添加AP配置的API路由
    server->on("/api/ap/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        DynamicJsonDocument doc(256);
        doc["ssid"] = apConfig.ssid;
        doc["enabled"] = apConfig.enabled;
        sendJsonResponse(request, doc);
    });
    

    server->on("/api/ap/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleAPConfig(request);
    });

    // 使用lambda函数处理WebSocket事件
    ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
        handleWebSocket(client, type, arg, data, len);
    });
    server->addHandler(ws);

    // 加载配置
    loadConfig();




    // 设置API路由
    server->on("/api/network", HTTP_POST, [](AsyncWebServerRequest* request) {},
        NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleNetworkConfig(request, data, len);
    });

    server->on("/api/artnet", HTTP_POST, [](AsyncWebServerRequest* request) {},
        NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleArtnetConfig(request, data, len);
    });

    server->on("/api/pixel", HTTP_POST, [](AsyncWebServerRequest* request) {},
        NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handlePixelConfig(request, data, len);
    });

    server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleConfig(request);
    });
    server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            // 处理配置
    request->send(200, "application/json", "{\"success\": true}");
    handleConfigUpdate(request, data, len);
    });
    server->on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleReboot(request);
    });
    server->on("/api/factory-reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleFactoryReset(request);
    });


    // 静态文件服务
    server->serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html");

    // 404处理
    server->onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not Found");
    });

    server->begin();
}


// 添加新的配置处理方法
void WebServer::handleNetworkConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    // 更新网络配置
    if (doc.containsKey("dhcpEnabled")) {
        config.dhcpEnabled = doc["dhcpEnabled"];
    }
    if (!config.dhcpEnabled) {
        if (doc.containsKey("staticIP")) {
            stringToIP(doc["staticIP"], config.staticIP);
        }
        if (doc.containsKey("staticMask")) {
            stringToIP(doc["staticMask"], config.staticMask);
        }
        if (doc.containsKey("staticGateway")) {
            stringToIP(doc["staticGateway"], config.staticGateway);
        }
    }

    if (saveConfigFile()) {
        request->send(200, "application/json", "{\"status\":\"success\"}");
        applyConfig();
        notifyConfigChange();
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
    }
}

void WebServer::handleArtnetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    // 更新 Art-Net 配置
    if (doc.containsKey("artnetNet")) {
        config.artnetNet = doc["artnetNet"];
    }
    if (doc.containsKey("artnetSubnet")) {
        config.artnetSubnet = doc["artnetSubnet"];
    }
    if (doc.containsKey("artnetUniverse")) {
        config.artnetUniverse = doc["artnetUniverse"];
    }
    if (doc.containsKey("dmxStartAddress")) {
        config.dmxStartAddress = doc["dmxStartAddress"];
    }

    if (saveConfigFile()) {
        request->send(200, "application/json", "{\"status\":\"success\"}");
        applyConfig();
        notifyConfigChange();
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
    }
}

void WebServer::handlePixelConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    // 更新像素配置
    if (doc.containsKey("pixelCount")) {
        config.pixelCount = doc["pixelCount"];
    }
    if (doc.containsKey("pixelType")) {
        config.pixelType = doc["pixelType"];
    }
    if (doc.containsKey("pixelEnabled")) {
        config.pixelEnabled = doc["pixelEnabled"];
    }

    if (saveConfigFile()) {
        request->send(200, "application/json", "{\"status\":\"success\"}");
        applyConfig();
        notifyConfigChange();
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
    }
}




void WebServer::handleReboot(AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Rebooting...\"}");
    delay(500);  // 给响应一些时间发送
    ESP.restart();
}

void WebServer::handleFactoryReset(AsyncWebServerRequest* request) {
    // 执行出厂设置重置操作
    // 例如：清除配置文件
    if (LittleFS.remove("/config.json")) {
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Factory reset successful. Rebooting...\"}");
        delay(500);  // 给响应一些时间发送
        ESP.restart();
    } else {
        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to reset configuration\"}");
    }
}

void WebServer::handleWsMessage(AsyncWebSocketClient* client, char* data) {
    // 处理 WebSocket 消息
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, data);
    
    if (error) {
        Serial.println("Failed to parse WebSocket message");
        return;
    }

    // 获取消息类型
    const char* type = doc["type"] | "unknown";
    
    if (strcmp(type, "get_status") == 0) {
        // 发送状态信息
        DynamicJsonDocument response(1024);
        response["type"] = "status";
        response["uptime"] = millis() / 1000;
        response["heap"] = ESP.getFreeHeap();
        if (WiFi.getMode() & WIFI_STA) {
            response["wifi_status"] = WiFi.status();
            response["ip"] = WiFi.localIP().toString();
            response["rssi"] = WiFi.RSSI();
        }
        if (WiFi.getMode() & WIFI_AP) {
            response["ap_stations"] = WiFi.softAPgetStationNum();
            response["ap_ip"] = WiFi.softAPIP().toString();
        }

        String responseStr;
        serializeJson(response, responseStr);
        client->text(responseStr);
    }
    else if (strcmp(type, "get_config") == 0) {
        // 发送当前配置
        DynamicJsonDocument response(1024);
        response["type"] = "config";
        createConfigJson(response);  // 使用之前定义的方法
        
        String responseStr;
        serializeJson(response, responseStr);
        client->text(responseStr);
    }
    else if (strcmp(type, "set_config") == 0) {
        // 更新配置
        JsonObject configData = doc["config"];
        if (!configData.isNull()) {
            parseConfig(doc);  // 使用之前定义的方法
            saveConfigFile();  // 保存配置
            
            // 发送确认
            client->text("{\"type\":\"config_update\",\"status\":\"success\"}");
        } else {
            client->text("{\"type\":\"config_update\",\"status\":\"error\",\"message\":\"Invalid config data\"}");
        }
    }
    else {
        // 未知消息类型
        client->text("{\"type\":\"error\",\"message\":\"Unknown message type\"}");
    }
}

// AP配置处理
void WebServer::handleAPConfig(AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, request->_tempObject);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (doc.containsKey("ssid") && doc.containsKey("password")) {
        // 保存新的AP配置
        strlcpy(apConfig.ssid, doc["ssid"] | "", sizeof(apConfig.ssid));
        strlcpy(apConfig.password, doc["password"] | "", sizeof(apConfig.password));
        apConfig.enabled = doc["enabled"] | false;
        
        // 保存配置
        saveAPConfig();
        
        // 应用新配置
        if (apConfig.enabled) {
            startAP(apConfig.ssid, apConfig.password);
        } else {
            stopAP();
        }
        
        request->send(200, "application/json", "{\"status\":\"success\"}");
    } else {
        request->send(400, "application/json", "{\"error\":\"Missing parameters\"}");
    }
}

//处理DNS请求
void WebServer::processDNS() {
    if (dnsServer != nullptr) {
        dnsServer->processNextRequest();
    }
}

// 启动AP模式
void WebServer::startAP(const char* ssid, const char* password) {
    WiFi.disconnect(true);
    delay(100);
    
    WiFi.mode(WIFI_AP);
    
    // 配置AP
    WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1),    // AP IP
        IPAddress(192, 168, 4, 1),    // Gateway
        IPAddress(255, 255, 255, 0)   // Subnet
    );
    
    // 确保DNS服务器被正确初始化和启动
    if (dnsServer == nullptr) {
        dnsServer = new DNSServer();
    }
    dnsServer->start(53, "*", IPAddress(192, 168, 4, 1));
    
    if (WiFi.softAP(ssid, password)) {
        Serial.println("AP Mode Started");
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
        
        // 广播AP状态变更
        DynamicJsonDocument doc(256);
        doc["type"] = "ap_status";
        doc["enabled"] = true;
        doc["ip"] = WiFi.softAPIP().toString();
        
        String message;
        serializeJson(doc, message);
        ws->textAll(message);
    } else {
        Serial.println("AP Mode Failed to Start");
    }
}

// 停止AP模式
void WebServer::stopAP() {
    if (WiFi.getMode() & WIFI_AP) {
        if (dnsServer) {
            dnsServer->stop();
            delete dnsServer;
            dnsServer = nullptr;
        }
        WiFi.softAPdisconnect(true);
        Serial.println("AP Mode Stopped");
        
        // 广播AP状态变更
        DynamicJsonDocument doc(256);
        doc["type"] = "ap_status";
        doc["enabled"] = false;
        
        String message;
        serializeJson(doc, message);
        ws->textAll(message);
    }
}

// 检查AP是否运行
bool WebServer::isAPRunning() const {
    return (WiFi.getMode() & WIFI_AP) != 0;
}

// 保存AP配置
void WebServer::saveAPConfig() {
    File file = LittleFS.open("/ap_config.json", "w");
    if (!file) {
        Serial.println("Failed to save AP config");
        return;
    }
    
    DynamicJsonDocument doc(256);
    doc["ssid"] = apConfig.ssid;
    doc["password"] = apConfig.password;
    doc["enabled"] = apConfig.enabled;
    
    serializeJson(doc, file);
    file.close();
}

// 加载AP配置
void WebServer::loadAPConfig() {
    File file = LittleFS.open("/ap_config.json", "r");
    if (!file) {
        // 使用默认配置
        strlcpy(apConfig.ssid, "HuBo-DMX-AP", sizeof(apConfig.ssid));
        strlcpy(apConfig.password, "542628277", sizeof(apConfig.password));
        apConfig.enabled = false;
        return;
    }

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (!error) {
        strlcpy(apConfig.ssid, doc["ssid"] | "EHuBo-DMX-AP", sizeof(apConfig.ssid));
        strlcpy(apConfig.password, doc["password"] | "542628277", sizeof(apConfig.password));
        apConfig.enabled = doc["enabled"] | false;
    }
}


// 处理WebSocket事件
void WebServer::handleWebSocket(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected\n", client->id());
            sendStatus(client);
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            if (len) {
                data[len] = 0;
                handleWsMessage(client, (char*)data);
            }
            break;
        case WS_EVT_ERROR:
            Serial.println("WebSocket error");
            break;
    }
}

// 处理获取配置的请求
void WebServer::handleConfig(AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(1024);
    createConfigJson(doc);
    sendJsonResponse(request, doc);
}

// 处理更新配置的请求
void WebServer::handleConfigUpdate(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "text/plain", String("Invalid JSON: ") + error.f_str());
        return;
    }

    Config newConfig = config;

    // 更新所有可能的配置项
    if (doc.containsKey("deviceName")) {
        strlcpy(newConfig.deviceName, doc["deviceName"], sizeof(newConfig.deviceName));
    }
    if (doc.containsKey("dhcpEnabled")) {
        newConfig.dhcpEnabled = doc["dhcpEnabled"];
    }
    if (doc.containsKey("staticIP")) {
        JsonArray ip = doc["staticIP"];
        for (size_t i = 0; i < 4; i++) {
            newConfig.staticIP[i] = ip[i];
        }
    }
    if (doc.containsKey("staticMask")) {
        JsonArray mask = doc["staticMask"];
        for (size_t i = 0; i < 4; i++) {
            newConfig.staticMask[i] = mask[i];
        }
    }
    if (doc.containsKey("staticGateway")) {
        JsonArray gateway = doc["staticGateway"];
        for (size_t i = 0; i < 4; i++) {
            newConfig.staticGateway[i] = gateway[i];
        }
    }
    if (doc.containsKey("artnetNet")) {
        newConfig.artnetNet = doc["artnetNet"];
    }
    if (doc.containsKey("artnetSubnet")) {
        newConfig.artnetSubnet = doc["artnetSubnet"];
    }
    if (doc.containsKey("artnetUniverse")) {
        newConfig.artnetUniverse = doc["artnetUniverse"];
    }
    if (doc.containsKey("dmxStartAddress")) {
        newConfig.dmxStartAddress = doc["dmxStartAddress"];
    }
    if (doc.containsKey("pixelCount")) {
        newConfig.pixelCount = doc["pixelCount"];
    }
    if (doc.containsKey("pixelType")) {
        newConfig.pixelType = doc["pixelType"];
    }
    if (doc.containsKey("pixelEnabled")) {
        newConfig.pixelEnabled = doc["pixelEnabled"];
    }

    // 保存并应用新配置
    if (ConfigManager::save((const ConfigManager::Config&)newConfig)) {
        config = newConfig;  // 更新当前配置
        applyConfig();       // 应用新配置

        // 发送成功响应，并包含更新后的配置
        DynamicJsonDocument response(1024);
        response["status"] = "success";
        response["message"] = "Configuration updated successfully";

        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr);

        // 通知所有连接的WebSocket客户端配置已更新
        notifyConfigChange();
    } else {
        request->send(500, "text/plain", "Failed to save configuration");
    }
}

// 创建配置的JSON表示
void WebServer::createConfigJson(JsonDocument& doc) {
    doc["deviceName"] = config.deviceName;
    doc["dhcpEnabled"] = config.dhcpEnabled;
    doc["staticIP"] = ipToString(config.staticIP);
    doc["staticMask"] = ipToString(config.staticMask);
    doc["staticGateway"] = ipToString(config.staticGateway);
    doc["artnetNet"] = config.artnetNet;
    doc["artnetSubnet"] = config.artnetSubnet;
    doc["artnetUniverse"] = config.artnetUniverse;
    doc["dmxStartAddress"] = config.dmxStartAddress;
    doc["pixelCount"] = config.pixelCount;
    doc["pixelType"] = config.pixelType;
    doc["pixelEnabled"] = config.pixelEnabled;
}

// 解析配置的JSON表示
void WebServer::parseConfig(const JsonDocument& doc) {
    if (doc.containsKey("deviceName")) {
        String deviceName = doc["deviceName"].as<String>();
        if (deviceName.length() > 0) {
            strlcpy(config.deviceName, deviceName.c_str(), sizeof(config.deviceName));
        } else {
            Serial.println("警告：deviceName字段为空，使用默认值！");
            strlcpy(config.deviceName, "DefaultDevice", sizeof(config.deviceName));  // 默认设备名
        }
    }

    if (doc.containsKey("dhcpEnabled")) {
        config.dhcpEnabled = doc["dhcpEnabled"];
    }

    if (doc.containsKey("staticIP")) {
        String staticIP = doc["staticIP"].as<String>();
        if (staticIP.length() > 0) {
            stringToIP(staticIP.c_str(), config.staticIP);
        } else {
            Serial.println("警告：staticIP字段为空，使用默认值！");
            stringToIP("192.168.4.1", config.staticIP);  // 默认IP
        }
    } else {
        // 默认staticIP
        stringToIP("192.168.4.1", config.staticIP);
    }

    if (doc.containsKey("staticMask")) {
        String staticMask = doc["staticMask"].as<String>();
        if (staticMask.length() > 0) {
            stringToIP(staticMask.c_str(), config.staticMask);
        } else {
            Serial.println("警告：staticMask字段为空，使用默认值！");
            stringToIP("255.255.255.0", config.staticMask);  // 默认子网掩码
        }
    } else {
        // 默认staticMask
        stringToIP("255.255.255.0", config.staticMask);
    }

    if (doc.containsKey("staticGateway")) {
        String staticGateway = doc["staticGateway"].as<String>();
        if (staticGateway.length() > 0) {
            stringToIP(staticGateway.c_str(), config.staticGateway);
        } else {
            Serial.println("警告：staticGateway字段为空，使用默认值！");
            stringToIP("192.168.4.1", config.staticGateway);  // 默认网关
        }
    } else {
        // 默认staticGateway
        stringToIP("192.168.4.1", config.staticGateway);
    }

    // 其他字段的处理同样可以做类似的检查和默认值设置
    if (doc.containsKey("artnetNet")) {
        config.artnetNet = doc["artnetNet"];
    }
    if (doc.containsKey("artnetSubnet")) {
        config.artnetSubnet = doc["artnetSubnet"];
    }
    if (doc.containsKey("artnetUniverse")) {
        config.artnetUniverse = doc["artnetUniverse"];
    }
    if (doc.containsKey("dmxStartAddress")) {
        config.dmxStartAddress = doc["dmxStartAddress"];
    }
    if (doc.containsKey("pixelCount")) {
        config.pixelCount = doc["pixelCount"];
    }
    if (doc.containsKey("pixelType")) {
        config.pixelType = doc["pixelType"];
    }
    if (doc.containsKey("pixelEnabled")) {
        config.pixelEnabled = doc["pixelEnabled"];
    }
}


// 发送JSON响应
void WebServer::sendJsonResponse(AsyncWebServerRequest* request, const JsonDocument& doc) {
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// 发送状态信息给WebSocket客户端
void WebServer::sendStatus(AsyncWebSocketClient* client) {
    DynamicJsonDocument doc(1024);
    doc["type"] = "status";
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["rssi"] = WiFi.RSSI();

    String status;
    serializeJson(doc, status);
    client->text(status);
}

// 初始化文件系统
bool WebServer::initFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        return false;
    }
    return true;
}

// 加载配置
void WebServer::loadConfig() {
    if (!loadConfigFile()) {
        Serial.println("使用默认配置");
        saveConfig();
    }
}

// 从文件加载配置
bool WebServer::loadConfigFile() {
    if (!LittleFS.exists("/config.json")) {
        return false;
    }
    File file = LittleFS.open("/config.json", "r");
    if (!file) {
        return false;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (!LittleFS.exists("/LittleFS")) {
        Serial.println("目录 /LittleFS 不存在，正在创建...");
        LittleFS.mkdir("/LittleFS");
    }


    if (error) {
        return false;
    }

    parseConfig(doc);
    return true;
}

// 保存配置到文件
bool WebServer::saveConfigFile() {
    File file = LittleFS.open("/config.json", "w");
    if (!file) {
        return false;
    }

    DynamicJsonDocument doc(1024);
    createConfigJson(doc);
    serializeJson(doc, file);
    file.close();
    return true;
}

// 应用当前配置
void WebServer::applyConfig() {
    if (artnetNode) {
        ArtnetNode::Config artnetConfig;
        artnetConfig.net = config.artnetNet;
        artnetConfig.subnet = config.artnetSubnet;
        artnetConfig.universe = config.artnetUniverse;
        artnetConfig.dmxStartAddress = config.dmxStartAddress;
        artnetNode->setConfig(artnetConfig);
    }

    // 应用网络配置
    if (!config.dhcpEnabled) {
        IPAddress ip(config.staticIP[0], config.staticIP[1], config.staticIP[2], config.staticIP[3]);
        IPAddress gateway(config.staticGateway[0], config.staticGateway[1], config.staticGateway[2], config.staticGateway[3]);
        IPAddress subnet(config.staticMask[0], config.staticMask[1], config.staticMask[2], config.staticMask[3]);
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.config(ip, gateway, subnet);
        } else {
            Serial.println("Wi-Fi not connected!");
        }
    }
}

// 通知所有WebSocket客户端配置已更改
void WebServer::notifyConfigChange() {
    DynamicJsonDocument doc(1024);
    doc["type"] = "config";
    createConfigJson(doc);

    String message;
    serializeJson(doc, message);
    ws->textAll(message);
}

// 将IP地址转换为字符串
String WebServer::ipToString(const uint8_t* ip) {
    return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}


// 将字符串转换为IP地址
void WebServer::stringToIP(const char* str, uint8_t* ip) {
    // 检查字符串是否为空
    if (str == nullptr || strlen(str) == 0) {
        Serial.println("错误：IP字符串为空！");
        ip[0] = ip[1] = ip[2] = ip[3] = 0;  // 默认IP为0.0.0.0
        return;
    }

    Serial.print("尝试解析IP地址：");
    Serial.println(str);  // 输出尝试解析的字符串

    // 使用sscanf解析IP地址
    int result = sscanf(str, "%hhu.%hhu.%hhu.%hhu", &ip[0], &ip[1], &ip[2], &ip[3]);
    
    // 打印解析结果
    Serial.print("sscanf 结果：");
    Serial.println(result);  // 如果结果不是4，说明解析失败

    // 检查解析结果
    if (result != 4) {
        Serial.println("错误：IP地址格式不正确！");
        ip[0] = ip[1] = ip[2] = ip[3] = 0;  // 默认IP为0.0.0.0
        return;
    }

    // 增加IP地址段范围检查，确保每段在0-255之间
    for (int i = 0; i < 4; i++) {
        if (ip[i] > 255) {
            Serial.print("错误：IP地址段超出范围：");
            Serial.print(i);
            Serial.print(" -> ");
            Serial.println(ip[i]);
            ip[0] = ip[1] = ip[2] = ip[3] = 0;  // 默认IP为0.0.0.0
            return;
        }
    }

    Serial.println("IP地址解析成功！");
}

// 更新状态
void WebServer::update() {
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();
    
    if (now - lastUpdate >= 1000) {
        lastUpdate = now;
        ws->cleanupClients();
        
        // 更新状态时包含AP信息
        DynamicJsonDocument doc(1024);
        doc["type"] = "status";
        doc["uptime"] = millis() / 1000;
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["rssi"] = WiFi.RSSI();
        doc["ap_enabled"] = isAPRunning();
        if (isAPRunning()) {
            doc["ap_stations"] = WiFi.softAPgetStationNum();
            doc["ap_ip"] = WiFi.softAPIP().toString();
        }
        
        String status;
        serializeJson(doc, status);
        ws->textAll(status);
    }
}