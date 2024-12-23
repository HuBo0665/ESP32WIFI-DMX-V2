#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <FS.h>
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
#include "file_system.h"

// 初始化常量
#define INITIAL_PIXEL_COUNT 1
#define WDT_TIMEOUT 10
#define WIFI_CONNECT_TIMEOUT 10000


// 在文件顶部添加常量定义
#define DMX_TASK_STACK_SIZE 8192
#define NETWORK_TASK_STACK_SIZE 8192
#define BACKGROUND_TASK_STACK_SIZE 4096

#define DMX_TASK_PRIORITY 2
#define NETWORK_TASK_PRIORITY 1
#define BACKGROUND_TASK_PRIORITY 1


// 全局变量
WebServer* webServer = nullptr;
ArtnetNode* artnetNode = nullptr;
String apSSID = DEFAULT_AP_SSID;
String apPassword = DEFAULT_AP_PASS;
bool noWiFiMode = false;

static const uint32_t WDT_RESET_INTERVAL = 1000;    // 1 秒
static const uint32_t MAIN_LOOP_DELAY = 10;         // 10 毫秒
static const uint32_t SYSTEM_HEAP_CHECK_INTERVAL = 30000; // 30 秒
static const uint32_t SYSTEM_STATUS_CHECK_INTERVAL = 5000; // 5 秒

TaskHandle_t taskHandle = NULL;
unsigned long lastWdtReset = 0;
unsigned long lastHeapCheck = 0;
unsigned long lastStatusCheck = 0;
unsigned long lastHeapReport = 0;  // 添加这个变量声明


// 在文件顶部声明任务句柄和任务函数
void backgroundTask(void * parameter);  // 只声明一次


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
    Serial.println("Starting system initialization...");

    esp_task_wdt_reset();  // 喂狗

    // 配置管理
    Serial.println("Loading configuration...");
    ConfigManager::Config configData;
    if (!ConfigManager::load(configData)) {
        Serial.println("Warning: Failed to load configuration, setting defaults");
        ConfigManager::setDefaults(configData);
        if (!ConfigManager::save(configData)) {
            Serial.println("System initialization failed at: Default Config Save");
            return false;
        }
        Serial.println("Default configuration saved successfully");
    }

    Serial.println("Configuration loaded successfully");
    esp_task_wdt_reset();  // 喂狗

    // AP配置加载和处理
    Serial.println("Loading AP configuration...");
    String apConfig = loadConfig("/ap_config.json");
    if (apConfig.length() > 0) {
        Serial.println("Loaded AP config: " + apConfig);
    } else {
        Serial.println("Creating default AP configuration...");
        const char* defaultConfig = "{\"enabled\":true,\"ssid\":\"MyAP\",\"password\":\"12345678\"}";
        if (!saveConfig("/ap_config.json", defaultConfig)) {
            Serial.println("System initialization failed at: AP Config Save");
            return false;
        }
        Serial.println("Default AP configuration saved successfully");
    }

    Serial.println("AP configuration loaded successfully");
    esp_task_wdt_reset();  // 喂狗

    // 更新像素配置
    Serial.println("Initializing pixel configuration...");
    pixels.updateLength(configData.pixelCount);
    pixels.updateType(static_cast<neoPixelType>(configData.pixelType));
    Serial.println("Pixel configuration updated");

    // 创建Art-Net节点
    Serial.println("Creating ArtNet node...");
    artnetNode = new ArtnetNode();
    if (!artnetNode) {
        Serial.println("System initialization failed at: ArtNet Node Creation");
        return false;
    }
    Serial.println("ArtNet node created successfully");

    // 创建Web服务器
    Serial.println("Creating Web server...");
    webServer = new WebServer(artnetNode);
    if (!webServer) {
        Serial.println("System initialization failed at: Web Server Creation");
        return false;
    }
    Serial.println("Web server created successfully");

    Serial.println("System initialization completed successfully");
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
    if (dmxTask != nullptr) {
        vTaskDelete(dmxTask);
        dmxTask = nullptr;
    }
    if (networkTask != nullptr) {
        vTaskDelete(networkTask);
        networkTask = nullptr;
    }

    BaseType_t dmxTaskCreated = xTaskCreatePinnedToCore(
        dmxTaskFunction,
        "DMX Task",
        DMX_TASK_STACK_SIZE,
        NULL,
        DMX_TASK_PRIORITY,
        &dmxTask,
        0  // 在核心0上运行
    );

    if (dmxTaskCreated != pdPASS) {
        Serial.println("Failed to create DMX task");
        return false;
    }

    esp_task_wdt_reset();  // 在创建任务之间喂狗

    BaseType_t networkTaskCreated = xTaskCreatePinnedToCore(
        networkTaskFunction,
        "Network Task",
        NETWORK_TASK_STACK_SIZE,
        NULL,
        NETWORK_TASK_PRIORITY,
        &networkTask,
        1  // 在核心1上运行
    );

    if (networkTaskCreated != pdPASS) {
        Serial.println("Failed to create Network task");
        vTaskDelete(dmxTask);  // 清理已创建的任务
        dmxTask = nullptr;
        return false;
    }

    return true;
}


void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\nStarting initialization...");

    // 配置看门狗
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);

    // 初始化文件系统
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
        delay(1000);
        ESP.restart();
        return;
    }
    Serial.println("LittleFS mounted successfully");

    // 初始化步骤计数
    int initStep = 1;
    unsigned long startTime = millis();

    // 步骤 1: 系统初始化
    Serial.printf("[%d/5] System initialization...\n", initStep++);
    if (!initializeSystem()) {
        Serial.println("System initialization failed!");
        delay(1000);
        ESP.restart();
        return;
    }
    esp_task_wdt_reset();

    // 步骤 2: 网络初始化
    Serial.printf("[%d/5] Network initialization...\n", initStep++);
    WiFi.onEvent(onWiFiEvent);
    if (!setupNetwork()) {
        Serial.println("Network initialization failed!");
        delay(1000);
        ESP.restart();
        return;
    }
    esp_task_wdt_reset();

    // 步骤 3: 硬件初始化
    Serial.printf("[%d/5] Hardware initialization...\n", initStep++);
    if (!setupHardware()) {
        Serial.println("Hardware initialization failed!");
        delay(1000);
        ESP.restart();
        return;
    }
    esp_task_wdt_reset();

    // 步骤 4: 任务创建
    Serial.printf("[%d/5] Creating system tasks...\n", initStep++);
    if (!createTasks()) {
        Serial.println("Task creation failed!");
        delay(1000);
        ESP.restart();
        return;
    }
    esp_task_wdt_reset();

    // 步骤 5: Web服务器和后台任务
    Serial.printf("[%d/5] Starting services...\n", initStep++);
    if (webServer) {
        webServer->begin();
        Serial.println("Web server started");
    }


    // 创建后台任务
    BaseType_t taskCreated = xTaskCreate(
        backgroundTask,
        "Background",
        BACKGROUND_TASK_STACK_SIZE,  // 使用新定义的堆栈大小
        NULL,
        BACKGROUND_TASK_PRIORITY,    // 使用新定义的优先级
        &taskHandle
    );


    if (taskCreated != pdPASS) {
        Serial.println("Background task creation failed!");
        delay(1000);
        ESP.restart();
        return;
    }

    // 初始化完成
    Serial.println("\nInitialization completed successfully!");
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("WiFi Status: %s\n", WiFi.status() == WL_CONNECTED ? 
                 "Connected" : "Not Connected");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    }
    
    esp_task_wdt_reset();
}

bool setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);  // 使用 WIFI_PASS 而不是 WIFI_PASSWORD
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > 10000) { // 10秒超时
            Serial.println("WiFi Connection Timeout");
            return false;
        }
        delay(500);
        Serial.print(".");
        esp_task_wdt_reset(); // 在等待时喂狗
    }
    
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
}

void backgroundTask(void * parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 100ms 周期

    for(;;) {
        esp_task_wdt_reset();
        
        // 系统状态监控
        uint32_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 10000) { // 如果可用堆内存低于10KB
            Serial.printf("Warning: Low heap memory: %u bytes\n", freeHeap);
        }

        // 使用 vTaskDelayUntil 确保固定周期
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void loop() {
    static unsigned long lastWdtReset = 0;
    static unsigned long lastStatusCheck = 0;
    static unsigned long lastHeapCheck = 0;
    const unsigned long currentMillis = millis();

    // 更频繁地喂狗（每100ms）
    if (currentMillis - lastWdtReset >= 100) {
        esp_task_wdt_reset();
        lastWdtReset = currentMillis;
    }

    // 堆内存和系统状态检查（合并两个检查）
    if (currentMillis - lastHeapCheck >= SYSTEM_HEAP_CHECK_INTERVAL) {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t maxBlock = ESP.getMaxAllocHeap();
        int8_t rssi = WiFi.RSSI();
        
        Serial.printf("\nSystem Status Report:\n");
        Serial.printf("- Free Heap: %u bytes\n", freeHeap);
        Serial.printf("- Largest Block: %u bytes\n", maxBlock);
        Serial.printf("- WiFi Status: %s\n", 
            WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("- RSSI: %d dBm\n", rssi);
        }
        
        lastHeapCheck = currentMillis;
    }

    // 网络状态检查
    if (currentMillis - lastStatusCheck >= SYSTEM_STATUS_CHECK_INTERVAL) {
        if (WiFi.getMode() & WIFI_AP) {
            Serial.printf("AP Clients: %d\n", WiFi.softAPgetStationNum());
        }
        lastStatusCheck = currentMillis;
    }

    // 给其他任务运行的机会
    yield();
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY));
}