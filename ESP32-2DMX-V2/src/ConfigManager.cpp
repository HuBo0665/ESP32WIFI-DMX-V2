#define DEVICE_NAME "default_device_name"
#define DEFAULT_PIXELS 100 // Define DEFAULT_PIXELS with an appropriate value
#define DEVICE_NAME "default_device_name"
#define DEFAULT_PIXELS 100 // Define DEFAULT_PIXELS with an appropriate value

#include "ConfigManager.h"
#include "LittleFS.h" // 包含 LittleFS 头文件
#include "file_system.h"  // 添加这行来包含 loadConfig 和 saveConfig 函数声明
#include <ESPAsyncWebServer.h>
#include <FS.h>

const char* ConfigManager::CONFIG_FILE = "/config.json";

bool ConfigManager::load(Config& config) {
    
    if (!ConfigManager::load(config)) {
    ConfigManager::setDefaults(config);
    ConfigManager::save(config);
    }

    if (!LittleFS.exists(CONFIG_FILE)) {
        setDefaults(config);
        return save(config);
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        return false;
    }

    // 网络配置
    strlcpy(config.deviceName, doc["deviceName"] | DEVICE_NAME, sizeof(config.deviceName));
    config.dhcpEnabled = doc["dhcpEnabled"] | true;
    
    JsonArray staticIP = doc["staticIP"];
    JsonArray staticMask = doc["staticMask"];
    JsonArray staticGateway = doc["staticGateway"];
    
    for (int i = 0; i < 4; i++) {
        config.staticIP[i] = staticIP[i] | 0;
        config.staticMask[i] = staticMask[i] | 0;
        config.staticGateway[i] = staticGateway[i] | 0;
    }

    // Art-Net配置
    config.artnetNet = doc["artnetNet"] | 0;
    config.artnetSubnet = doc["artnetSubnet"] | 0;
    config.artnetUniverse = doc["artnetUniverse"] | 0;
    config.dmxStartAddress = doc["dmxStartAddress"] | 1;

    // 像素配置
    config.pixelCount = doc["pixelCount"] | DEFAULT_PIXELS;
    config.pixelType = doc["pixelType"] | 0;
    config.pixelEnabled = doc["pixelEnabled"] | true;

    // 系统配置
    config.rdmEnabled = doc["rdmEnabled"] | true;
    config.brightness = doc["brightness"] | 255;

    return true;
}

bool handleAPConfig(AsyncWebServerRequest *request) {
    if (request->method() == HTTP_GET) {
        // 读取配置
        String config = loadConfig("/ap_config.json");
        if (config.length() > 0) {
            request->send(200, "application/json", config);
        } else {
            request->send(500, "text/plain", "Failed to load AP configuration");
        }
    } 
    else if (request->method() == HTTP_POST) {
        String json = request->arg("plain");
        // 保存配置
        if (saveConfig("/ap_config.json", json.c_str())) {
            Serial.println("AP configuration saved");
            request->send(200, "text/plain", "Configuration saved");
        } else {
            request->send(500, "text/plain", "Failed to save configuration");
        }
    }
    return true;
}


bool ConfigManager::handleAPConfig(AsyncWebServerRequest *request) {
    if (request->method() == HTTP_GET) {
        String config = loadConfig("/ap_config.json");
        if (config.length() > 0) {
            request->send(200, "application/json", config);
        } else {
            request->send(500, "text/plain", "Failed to load AP configuration");
        }
    } else if (request->method() == HTTP_POST) {
        String json = request->arg("plain");
        if (saveConfig("/ap_config.json", json.c_str())) {
            request->send(200, "text/plain", "Configuration saved");
        } else {
            request->send(500, "text/plain", "Failed to save configuration");
        }
    }
    return true;
}


bool ConfigManager::save(const Config& config) {
    StaticJsonDocument<1024> doc;

    // 网络配置
    doc["deviceName"] = config.deviceName;
    doc["dhcpEnabled"] = config.dhcpEnabled;
    
    JsonArray staticIP = doc.createNestedArray("staticIP");
    JsonArray staticMask = doc.createNestedArray("staticMask");
    JsonArray staticGateway = doc.createNestedArray("staticGateway");
    
    for (int i = 0; i < 4; i++) {
        staticIP.add(config.staticIP[i]);
        staticMask.add(config.staticMask[i]);
        staticGateway.add(config.staticGateway[i]);
    }

    // Art-Net配置
    doc["artnetNet"] = config.artnetNet;
    doc["artnetSubnet"] = config.artnetSubnet;
    doc["artnetUniverse"] = config.artnetUniverse;
    doc["dmxStartAddress"] = config.dmxStartAddress;

    // 像素配置
    doc["pixelCount"] = config.pixelCount;
    doc["pixelType"] = config.pixelType;
    doc["pixelEnabled"] = config.pixelEnabled;

    // 系统配置
    doc["rdmEnabled"] = config.rdmEnabled;
    doc["brightness"] = config.brightness;

    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) {
        return false;
    }

    serializeJson(doc, file);
    file.close();
    return true;
}

void ConfigManager::setDefaults(Config& config) {
    // 网络配置
    strlcpy(config.deviceName, DEVICE_NAME, sizeof(config.deviceName));
    config.dhcpEnabled = true;
    
    // 默认静态IP配置，确保 IP 地址与网关在同一子网内
    config.staticIP[0] = 192;
    config.staticIP[1] = 168;
    config.staticIP[2] = 4;  // 修改为4，确保与网关在同一网段
    config.staticIP[3] = 10; // 设置为一个合适的 IP 地址

    config.staticMask[0] = 255;
    config.staticMask[1] = 255;
    config.staticMask[2] = 255;
    config.staticMask[3] = 0;
    
    config.staticGateway[0] = 192;
    config.staticGateway[1] = 168;
    config.staticGateway[2] = 4;  // 网关要与设备的 IP 地址在同一网段
    config.staticGateway[3] = 1;

    // Art-Net配置
    config.artnetNet = 0;
    config.artnetSubnet = 0;
    config.artnetUniverse = 0;
    config.dmxStartAddress = 1;

    // 像素配置
    config.pixelCount = DEFAULT_PIXELS;
    config.pixelType = 0;
    config.pixelEnabled = true;

    // 系统配置
    config.rdmEnabled = true;
    config.brightness = 255;
}
