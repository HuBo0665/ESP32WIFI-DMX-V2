#pragma once

#include <Arduino.h>
#include <NeoPixelBus.h>
#include "config.h"

// 像素类型定义
enum PixelType {
    TYPE_WS2812 = 0,
    TYPE_SK6812 = 1,
    TYPE_APA102 = 2
};

// 效果类型定义
enum PixelEffect {
    EFFECT_NONE = 0,
    EFFECT_RAINBOW = 1,
    EFFECT_CHASE = 2,
    EFFECT_FADE = 3,
    EFFECT_TWINKLE = 4,
    EFFECT_FIRE = 5
};

class PixelDriver {
public:
    PixelDriver();
    ~PixelDriver();

    // 基本功能
    bool begin(gpio_num_t pin, uint16_t numPixels, PixelType type = TYPE_WS2812);
    void update();
    void show();
    void clear();
    
    // 像素控制
    void setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    void setPixelHSV(uint16_t index, float h, float s, float v);
    void setRange(uint16_t start, uint16_t count, uint8_t r, uint8_t g, uint8_t b);
    void setBrightness(uint8_t brightness);
    
    // 效果控制
    void setEffect(PixelEffect effect);
    void setEffectSpeed(uint8_t speed);
    void setEffectColor(uint8_t r, uint8_t g, uint8_t b);
    void setEffectParams(uint8_t param1, uint8_t param2);
    
    // DMX控制
    void handleDMX(uint8_t* data, uint16_t length);
    void setDMXMode(bool enabled) { dmxMode = enabled; }
    
    // 状态查询
    uint16_t getNumPixels() const { return numPixels; }
    bool isEnabled() const { return enabled; }
    PixelEffect getCurrentEffect() const { return currentEffect; }

private:
    // NeoPixelBus对象
    using PixelBus = NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>;
    PixelBus* strip;
    
    // 配置参数
    uint16_t numPixels;
    gpio_num_t dataPin;
    PixelType pixelType;
    bool enabled;
    bool dmxMode;
    
    // 效果参数
    PixelEffect currentEffect;
    uint8_t effectSpeed;
    uint8_t effectStep;
    uint32_t lastUpdate;
    RgbColor effectColor;
    uint8_t brightness;
    uint8_t param1;
    uint8_t param2;
    
    // 效果处理方法
    void updateEffects();
    void updateRainbow();
    void updateChase();
    void updateFade();
    void updateTwinkle();
    void updateFire();
    
    // 颜色转换
    RgbColor HSVtoRGB(float h, float s, float v);
    RgbColor applyBrightness(const RgbColor& color);
    
    // 帮助方法
    bool validatePixelIndex(uint16_t index) const;
    void initializeStrip();
};