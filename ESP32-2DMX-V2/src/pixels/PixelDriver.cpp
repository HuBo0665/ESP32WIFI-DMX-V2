#include "PixelDriver.h"

PixelDriver::PixelDriver()
    : strip(nullptr)
    , numPixels(0)
    , enabled(false)
    , dmxMode(false)
    , currentEffect(EFFECT_NONE)
    , effectSpeed(128)
    , effectStep(0)
    , lastUpdate(0)
    , brightness(255)
    , param1(0)
    , param2(0) {
}

PixelDriver::~PixelDriver() {
    if (strip) {
        delete strip;
        strip = nullptr;
    }
}

bool PixelDriver::begin(gpio_num_t pin, uint16_t count, PixelType type) {
    dataPin = pin;
    numPixels = (count > MAX_PIXELS) ? MAX_PIXELS : count;
    pixelType = type;
    
    // 创建并初始化LED控制对象
    initializeStrip();
    
    if (!strip) {
        return false;
    }
    
    strip->Begin();
    clear();
    show();
    
    enabled = true;
    return true;
}

void PixelDriver::initializeStrip() {
    if (strip) {
        delete strip;
    }
    
    strip = new PixelBus(numPixels, dataPin);
}

void PixelDriver::setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (!enabled || !validatePixelIndex(index)) return;
    
    RgbColor color(r, g, b);
    strip->SetPixelColor(index, applyBrightness(color));
}

void PixelDriver::setPixelHSV(uint16_t index, float h, float s, float v) {
    if (!enabled || !validatePixelIndex(index)) return;
    
    RgbColor color = HSVtoRGB(h, s, v);
    strip->SetPixelColor(index, applyBrightness(color));
}

void PixelDriver::setRange(uint16_t start, uint16_t count, uint8_t r, uint8_t g, uint8_t b) {
    if (!enabled) return;
    
    RgbColor color(r, g, b);
    color = applyBrightness(color);
    
    uint16_t end = start + count;
    if (end > numPixels) end = numPixels;
    
    for (uint16_t i = start; i < end; i++) {
        strip->SetPixelColor(i, color);
    }
}

void PixelDriver::setBrightness(uint8_t value) {
    brightness = value;
    if (enabled && !dmxMode) {
        show();  // 立即更新显示
    }
}

void PixelDriver::clear() {
    if (!enabled) return;
    
    for (uint16_t i = 0; i < numPixels; i++) {
        strip->SetPixelColor(i, RgbColor(0));
    }
}

void PixelDriver::show() {
    if (!enabled) return;
    strip->Show();
}

void PixelDriver::handleDMX(uint8_t* data, uint16_t length) {
    if (!enabled || !dmxMode || !data) return;
    
    uint16_t pixelCount = length / 3;
    if (pixelCount > numPixels) {
        pixelCount = numPixels;
    }
    
    for (uint16_t i = 0; i < pixelCount; i++) {
        uint16_t base = i * 3;
        setPixel(i, data[base], data[base + 1], data[base + 2]);
    }
    
    show();
}

void PixelDriver::update() {
    if (!enabled || dmxMode) return;
    
    uint32_t now = millis();
    if (now - lastUpdate < (256 - effectSpeed)) {
        return;
    }
    
    lastUpdate = now;
    
    if (currentEffect != EFFECT_NONE) {
        updateEffects();
        show();
    }
}

void PixelDriver::updateEffects() {
    switch (currentEffect) {
        case EFFECT_RAINBOW:
            updateRainbow();
            break;
        case EFFECT_CHASE:
            updateChase();
            break;
        case EFFECT_FADE:
            updateFade();
            break;
        case EFFECT_TWINKLE:
            updateTwinkle();
            break;
        case EFFECT_FIRE:
            updateFire();
            break;
        default:
            break;
    }
}

void PixelDriver::updateRainbow() {
    for (uint16_t i = 0; i < numPixels; i++) {
        float hue = (float)(effectStep + i * 256 / numPixels) / 256.0f;
        setPixelHSV(i, hue, 1.0f, 1.0f);
    }
    effectStep = (effectStep + 1) & 0xFF;
}

void PixelDriver::updateChase() {
    clear();
    uint16_t pos = effectStep % numPixels;
    setPixel(pos, effectColor.R, effectColor.G, effectColor.B);
    effectStep = (effectStep + 1) % numPixels;
}

void PixelDriver::updateFade() {
    float intensity = (float)(sin(effectStep * PI / 128) + 1) / 2;
    RgbColor color = applyBrightness(RgbColor(
        effectColor.R * intensity,
        effectColor.G * intensity,
        effectColor.B * intensity
    ));
    
    for (uint16_t i = 0; i < numPixels; i++) {
        strip->SetPixelColor(i, color);
    }
    
    effectStep = (effectStep + 1) & 0xFF;
}

void PixelDriver::updateTwinkle() {
    for (uint16_t i = 0; i < numPixels; i++) {
        if (random(100) < param1) {  // param1 控制闪烁概率
            setPixel(i, effectColor.R, effectColor.G, effectColor.B);
        } else {
            setPixel(i, 0, 0, 0);
        }
    }
}

void PixelDriver::updateFire() {
    // 火焰效果实现
    for (uint16_t i = 0; i < numPixels; i++) {
        int r = 255;
        int g = param1;  // 控制火焰黄色程度
        int b = 0;
        float intensity = (float)(random(80, 100)) / 100.0f;
        
        setPixel(i, r * intensity, g * intensity, b);
    }
}

RgbColor PixelDriver::HSVtoRGB(float h, float s, float v) {
    if (s <= 0.0f) return RgbColor(v * 255);
    
    h = fmod(h, 1.0f) * 6.0f;
    int i = (int)h;
    float f = h - (float)i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    
    switch (i) {
        default:
        case 0: return RgbColor(v * 255, t * 255, p * 255);
        case 1: return RgbColor(q * 255, v * 255, p * 255);
        case 2: return RgbColor(p * 255, v * 255, t * 255);
        case 3: return RgbColor(p * 255, q * 255, v * 255);
        case 4: return RgbColor(t * 255, p * 255, v * 255);
        case 5: return RgbColor(v * 255, p * 255, q * 255);
    }
}

RgbColor PixelDriver::applyBrightness(const RgbColor& color) {
    if (brightness == 255) return color;
    return RgbColor(
        (color.R * brightness) >> 8,
        (color.G * brightness) >> 8,
        (color.B * brightness) >> 8
    );
}

bool PixelDriver::validatePixelIndex(uint16_t index) const {
    return index < numPixels;
}

void PixelDriver::setEffect(PixelEffect effect) {
    currentEffect = effect;
    effectStep = 0;
    if (effect == EFFECT_NONE) {
        clear();
        show();
    }
}

void PixelDriver::setEffectSpeed(uint8_t speed) {
    effectSpeed = speed;
}

void PixelDriver::setEffectColor(uint8_t r, uint8_t g, uint8_t b) {
    effectColor = RgbColor(r, g, b);
}

void PixelDriver::setEffectParams(uint8_t p1, uint8_t p2) {
    param1 = p1;
    param2 = p2;
}