#include "ESP32DMX.h"

// 构造函数，初始化成员变量
ESP32DMX::ESP32DMX(uart_port_t uartNum)
    : uartNum(uartNum)
    , enabled(false)
    , outputting(false)
    , transmitting(false)
    , frameCount(0)
    , lastFrameTime(0)
    , frameErrors(0) {
    
    // 初始化DMX缓冲区
    memset(dmxBuffer, 0, DMX_BUFFER_SIZE);
    dmxBuffer[0] = 0;  // DMX起始码
    
    // 配置UART参数
    uart_config.baud_rate = DMX_BAUDRATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_2;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_APB;
}

// 发送RDM数据
void ESP32DMX::sendRDM(uint8_t* data, uint16_t length) {
    if (!enabled || !data || length == 0) return;

    // 保存当前状态
    bool wasOutputting = outputting;
    if (wasOutputting) {
        stopOutput();
    }

    // 发送RDM帧
    sendBreak(176);  // RDM break time = 176µs
    sendMAB();       // Mark After Break

    // 设置RDM波特率 (250kbps)
    uart_set_baudrate(uartNum, 250000);

    // 设置DE引脚为发送模式
    if (dirPin != GPIO_NUM_NC) {
        gpio_set_level(dirPin, 1);
    }

    // 发送RDM数据
    uart_write_bytes(uartNum, (const char*)data, length);
    uart_wait_tx_done(uartNum, portMAX_DELAY);

    // 添加额外延迟以确保数据发送完成
    delayMicroseconds(50);

    // 设置DE引脚为接收模式
    if (dirPin != GPIO_NUM_NC) {
        gpio_set_level(dirPin, 0);
    }

    // 恢复DMX波特率
    uart_set_baudrate(uartNum, DMX_BAUDRATE);

    // 恢复之前的状态
    if (wasOutputting) {
        startOutput();
    }
}

// 析构函数
ESP32DMX::~ESP32DMX() {
    end();
}

// 初始化UART和GPIO引脚
bool ESP32DMX::begin(gpio_num_t txPin, gpio_num_t dirPin) {
    this->txPin = txPin;
    this->dirPin = dirPin;

    // 配置GPIO
    configurePins();
    
    // 配置UART
    esp_err_t err = uart_param_config(uartNum, &uart_config);
    if (err != ESP_OK) {
        log_e("UART config failed");
        return false;
    }

    // 安装UART驱动
    err = uart_driver_install(uartNum, DMX_BUFFER_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        log_e("UART driver install failed");
        return false;
    }

    // 设置UART引脚
    err = uart_set_pin(uartNum, txPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        log_e("UART pin config failed");
        return false;
    }

    enabled = true;
    return true;
}

// 配置GPIO引脚
void ESP32DMX::configurePins() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << dirPin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // 默认为接收模式
    gpio_set_level(dirPin, 0);
}

// 启动DMX输出
void ESP32DMX::startOutput() {
    if (!enabled) return;
    
    gpio_set_level(dirPin, 1);  // 设置为输出模式
    outputting = true;
}

// 停止DMX输出
void ESP32DMX::stopOutput() {
    if (!enabled) return;
    
    gpio_set_level(dirPin, 0);  // 设置为接收模式
    outputting = false;
}

// 设置DMX通道数据
void ESP32DMX::setChannel(uint16_t channel, uint8_t value) {
    if (validateChannel(channel)) {
        dmxBuffer[channel] = value;
    }
}

// 获取DMX通道数据
uint8_t ESP32DMX::getChannel(uint16_t channel) const {
    if (validateChannel(channel)) {
        return dmxBuffer[channel];
    }
    return 0;
}

// 清空DMX通道数据
void ESP32DMX::clearChannels() {
    memset(dmxBuffer + 1, 0, DMX_BUFFER_SIZE - 1);
}

// 开始DMX帧
void ESP32DMX::startFrame() {
    if (!enabled || !outputting) return;
    
    sendBreak(DMX_BREAK_US);
    sendMAB();
}

// 结束DMX帧
void ESP32DMX::endFrame() {
    if (!enabled || !outputting) return;
    
    waitForTransmitComplete();
    frameCount++;
    lastFrameTime = millis();
}

// 发送Break信号
void ESP32DMX::sendBreak(uint32_t breakTime) {
    uart_set_baudrate(uartNum, 1000000 / breakTime);
    uart_write_bytes(uartNum, "\0", 1);
    uart_wait_tx_done(uartNum, portMAX_DELAY);
    uart_set_baudrate(uartNum, DMX_BAUDRATE);
}

// 发送MAB信号
void ESP32DMX::sendMAB() {
    delayMicroseconds(DMX_MAB_US);
}

// 更新DMX数据
void ESP32DMX::update() {
    if (!enabled || !outputting) return;

    startFrame();
    write(dmxBuffer, DMX_BUFFER_SIZE);
    endFrame();
}

// 等待传输完成
void ESP32DMX::waitForTransmitComplete() {
    uart_wait_tx_done(uartNum, portMAX_DELAY);
}

// 验证通道号是否合法
bool ESP32DMX::validateChannel(uint16_t channel) const {
    return channel < DMX_BUFFER_SIZE;
}

// 结束DMX
void ESP32DMX::end() {
    if (enabled) {
        stopOutput();
        uart_driver_delete(uartNum);
        enabled = false;
    }
}

// 实现write函数
void ESP32DMX::write(uint8_t* data, uint16_t length) {
    if (enabled) {
        uart_write_bytes(uartNum, (const char*)data, length);
    }
}