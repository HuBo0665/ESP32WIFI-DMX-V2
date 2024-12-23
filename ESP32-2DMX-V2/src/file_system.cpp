#include <Arduino.h>
#include "LittleFS.h"
#include <FS.h>
#include "file_system.h"

#define FORMAT_LittleFS_IF_FAILED true

bool g_fs_initialized = false;

bool initLittleFS() {
    if (g_fs_initialized) {
        Serial.println("LittleFS already initialized");
        return true;
    }
    
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        return false;
    }
    
    g_fs_initialized = true;
    
    // 打印文件系统信息
    uint32_t totalBytes = LittleFS.totalBytes();
    uint32_t usedBytes = LittleFS.usedBytes();
    uint32_t freeBytes = totalBytes - usedBytes;
    
    Serial.println("File System Initialized");
    Serial.printf("FS Info - Total: %u bytes, Used: %u bytes, Free: %u bytes\n",
                 totalBytes, usedBytes, freeBytes);
    
    return true;
}

bool saveConfig(const char* filename, const char* config) {
    File file = LittleFS.open(filename, "w");
    if (!file) {
        return false;
    }
    
    size_t written = file.print(config);
    file.close();
    return written > 0;
}

String loadConfig(const char* filename) {
    if (!LittleFS.exists(filename)) {
        return String();
    }
    
    File file = LittleFS.open(filename, "r");
    if (!file) {
        return String();
    }
    
    String content = file.readString();
    file.close();
    return content;
}