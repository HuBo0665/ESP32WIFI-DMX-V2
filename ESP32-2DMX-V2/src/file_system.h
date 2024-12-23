#pragma once
#include <Arduino.h>
#include <FS.h>
#include "LittleFS.h"

// 文件系统初始化状态
extern bool g_fs_initialized;

bool initLittleFS();
String loadConfig(const char* filename);
bool saveConfig(const char* filename, const char* config);



