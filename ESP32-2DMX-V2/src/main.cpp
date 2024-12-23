#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include "config.h"
#include <Adafruit_NeoPixel.h>
#include "dmx/ESP32DMX.h"
#include "artnet/ArtnetNode.h"
#include "rdm/RDMHandler.h"
#include "pixels/PixelDriver.h"
#include "web/WebServer.h"
#include "ConfigManager.h"
#include "GlobalConfig.h"

// 初始化常量
#define INITIAL_PIXEL_COUNT 1
#define TASK_STACK_SIZE 16384
#define WDT_TIMEOUT 10
#define WIFI_CONNECT_TIMEOUT 10000
#define STATUS_CHECK_INTERVAL 10000
#define HEAP_REPORT_INTERVAL 30000

// 全局变量
WebServer* webServer = nullptr;
ArtnetNode* artnetNode = nullptr;
String apSSID = DEFAULT_AP_SSID;
String apPassword = DEFAULT_AP_PASS;
bool noWiFiMode = false;

// 全局对象
ESP32DMX dmxA(1);  // UART1
ESP32DMX dmxB(2);  // UART2
RDMHandler rdmHandler;
PixelDriver pixelDriver;
ConfigManager::Config config;
Adafruit_NeoPixel pixels(INITIAL_PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
GlobalConfig gConfig;

// Task handles
TaskHandle_t dmxTask = nullptr;
TaskHandle_t networkTask = nullptr;

// 函数声明
bool initializeSystem();
bool setupNetwork();
bool setupHardware();
bool createTasks();
bool startAPMode();
void loadConfig();
void validatePacket(uint8_t* dmxAData, uint8_t* dmxBData);
void stringToIP(const char* str, uint8_t* ip);

// DMX处理任务
void dmxTaskFunction(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(1);
    
    while (true) {
        esp_task_wdt_reset();
        dmxA.update();
        dmxB.update();
        rdmHandler.update();
        vTaskDelay(xDelay);
    }
}

// 网络处理任务
void networkTaskFunction(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(1);
    
    while (true) {
        esp_task_wdt_reset();
        validatePacket(dmxA.getDMXData(), dmxB.getDMXData());
        if (artnetNode) artnetNode->update();
        if (webServer) webServer->update();
        vTaskDelay(xDelay);
    }
}

void validatePacket(uint8_t* dmxAData, uint8_t* dmxBData) {
    // 数据包验证逻辑
}

// WiFi事件处理
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("[WiFi-event] event: %d\n", event);
    
    switch(event) {
        case SYSTEM_EVENT_AP_START:
            Serial.println("AP Started");
            Serial.printf("AP SSID: %s\n", apSSID.c_str());
            Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            Serial.printf("Station connected to AP - Total: %d\n", 
                        WiFi.softAPgetStationNum());
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            Serial.printf("Station disconnected from AP - Total: %d\n", 
                        WiFi.softAPgetStationNum());
            break;
        case SYSTEM_EVENT_STA_START:
            Serial.println("Station Mode Started");
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("Connected to AP");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("Disconnected from AP");
            break;
    }
}

bool startAPMode() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    WiFi.mode(WIFI_AP);
    delay(100);
    
    if (!WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1),    // AP IP
        IPAddress(192, 168, 4, 1),    // Gateway
        IPAddress(255, 255, 255, 0)   // Subnet mask
    )) {
        Serial.println("AP Config Failed");
        return false;
    }
    
    if (!WiFi.softAP(
        apSSID.c_str(),
        apPassword.c_str(),
        AP_CHANNEL,
        false,              // 不隐藏SSID
        MAX_AP_CONNECTIONS
    )) {
        Serial.println("AP Start Failed");
        return false;
    }
    
    Serial.println("AP Mode Started Successfully");
    Serial.printf("AP SSID: %s\n", apSSID.c_str());
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    return true;
}

bool initializeSystem() {
    // 初始化文件系统
if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed - Formatting");
    LittleFS.format();


    if (!LittleFS.exists("/littlefs")) {
        Serial.println("目录 /littlefs 不存在，正在创建...");
        LittleFS.mkdir("/littlefs");
    }


    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Initialization Failed");
        return false;
    }

    // 输出文件系统的空间使用情况
    uint32_t totalBytes = LittleFS.totalBytes();  // 获取总字节数
    uint32_t usedBytes = LittleFS.usedBytes();    // 获取已用字节数

    Serial.print("Total bytes: ");
    Serial.println(totalBytes);  // 打印总字节数

    Serial.print("Used bytes: ");
    Serial.println(usedBytes);   // 打印已用字节数


}

    // 加载配置
    if (!ConfigManager::load(config)) {
        Serial.println("Using default config");
        ConfigManager::setDefaults(config);
        ConfigManager::save(config);
    }

    // 更新NeoPixel配置
    pixels.updateLength(config.pixelCount);
    pixels.updateType(static_cast<neoPixelType>(config.pixelType));

    // 创建Art-Net节点
    artnetNode = new ArtnetNode();
    if (!artnetNode) {
        Serial.println("Failed to create ArtnetNode");
        return false;
    }

    // 创建Web服务器
    webServer = new WebServer(artnetNode);
    if (!webServer) {
        Serial.println("Failed to create WebServer");
        return false;
    }

    return true;
}

bool setupNetwork() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    WiFi.setHostname(config.deviceName);

    if (!config.dhcpEnabled) {
        IPAddress ip(config.staticIP);
        IPAddress gateway(config.staticGateway);
        IPAddress subnet(config.staticMask);
        if (!WiFi.config(ip, gateway, subnet)) {
            Serial.println("Static IP Config Failed");
        }
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && 
           millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
        delay(500);
        Serial.print(".");
        esp_task_wdt_reset();
    }

    if (WiFi.status() != WL_CONNECTED && !noWiFiMode) {
        Serial.println("\nWiFi Connection Failed - Starting AP Mode");
        return startAPMode();
    }

    Serial.println("\nWiFi Connected");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

bool setupHardware() {
    // 初始化DMX
    dmxA.begin(DMX_TX_A_PIN, DMX_DIR_A_PIN);
    dmxB.begin(DMX_TX_B_PIN, DMX_DIR_B_PIN);

    // 配置Art-Net
    ArtnetNode::Config artnetConfig = {
        .net = config.artnetNet,
        .subnet = config.artnetSubnet,
        .universe = config.artnetUniverse,
        .dmxStartAddress = config.dmxStartAddress
    };
    artnetNode->setConfig(artnetConfig);
    
    if (!artnetNode->begin()) {
        Serial.println("Art-Net Init Failed");
        return false;
    }

    // 初始化其他硬件
    if (config.pixelEnabled) {
        pixels.begin();
        pixels.setBrightness(config.brightness);
        if (!pixelDriver.begin(PIXEL_PIN, PIXEL_COUNT)) {
            Serial.println("Pixel Driver Init Failed");
            return false;
        }
    }

    if (config.rdmEnabled) {
        rdmHandler.begin(&dmxA);
    }

    return true;
}

bool createTasks() {
    BaseType_t dmxTaskCreated = xTaskCreatePinnedToCore(
        dmxTaskFunction,
        "DMX Task",
        TASK_STACK_SIZE,
        NULL,
        1,
        &dmxTask,
        0
    );

    BaseType_t networkTaskCreated = xTaskCreatePinnedToCore(
        networkTaskFunction,
        "Network Task",
        TASK_STACK_SIZE,
        NULL,
        1,
        &networkTask,
        1
    );

    return (dmxTaskCreated == pdPASS && networkTaskCreated == pdPASS);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nStarting...");

    // 配置看门狗
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);

    // 初始化系统
    if (!initializeSystem()) {
        Serial.println("System Init Failed!");
        ESP.restart();
        return;
    }

    // 初始化网络
    WiFi.onEvent(onWiFiEvent);
    if (!setupNetwork()) {
        Serial.println("Network Setup Failed!");
        ESP.restart();
        return;
    }

    // 初始化硬件
    if (!setupHardware()) {
        Serial.println("Hardware Setup Failed!");
        ESP.restart();
        return;
    }

    // 创建任务
    if (!createTasks()) {
        Serial.println("Task Creation Failed!");
        ESP.restart();
        return;
    }

    if (webServer) {
        webServer->begin();
    }

    Serial.println("Setup Completed Successfully");
}

void loop() {
    static unsigned long lastCheck = 0;
    static unsigned long lastHeapReport = 0;
    
    esp_task_wdt_reset();

    // 系统状态报告
    if (millis() - lastHeapReport >= HEAP_REPORT_INTERVAL) {
        Serial.printf("Free Heap: %d, Max Block: %d\n", 
            ESP.getFreeHeap(), ESP.getMaxAllocHeap());
        Serial.printf("WiFi Status: %d, RSSI: %d\n", 
            WiFi.status(), WiFi.RSSI());
        lastHeapReport = millis();
    }

    // 网络状态检查
    if (millis() - lastCheck >= STATUS_CHECK_INTERVAL) {
        if (WiFi.getMode() & WIFI_AP) {
            Serial.printf("AP Status - Connected: %d\n", 
                WiFi.softAPgetStationNum());
        }
        if (WiFi.getMode() & WIFI_STA) {
            Serial.printf("WiFi Status: %d\n", WiFi.status());
        }
        lastCheck = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(1));
}