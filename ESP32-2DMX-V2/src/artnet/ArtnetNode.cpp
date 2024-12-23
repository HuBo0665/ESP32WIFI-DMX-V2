#include "ArtnetNode.h"

// 静态成员初始化
const uint8_t ArtnetNode::ARTNET_ID[8] = {'A', 'r', 't', '-', 'N', 'e', 't', 0};

ArtnetNode::ArtnetNode()
    : dmx(nullptr)
    , pixels(nullptr)
    , dmxCallback(nullptr)
    , rdmCallback(nullptr)
    , pixelCallback(nullptr) {
    initializeDefaults();
}

ArtnetNode::~ArtnetNode() {
    udp.stop();
}

void ArtnetNode::initializeDefaults() {
    // 配置默认值
    strcpy(config.shortName, "ESP32 ArtNode");
    strcpy(config.longName, "ESP32 Art-Net Node");
    config.net = 0;
    config.subnet = 0;
    config.universe = 0;
    config.dmxMode = 0;
    config.dmxStartAddress = 1;
    config.pixelCount = 170;
    config.pixelType = 0;
    config.mergeMode = true;

    // 状态初始化
    memset(&status, 0, sizeof(Status));
    status.firmware = 1;
    status.ports = 1;
    status.portTypes[0] = 0x80;  // 输出端口
    status.version = ARTNET_VERSION;
}

bool ArtnetNode::begin() {
    // 获取网络信息
    IPAddress localIP = WiFi.localIP();
    uint32_t ip = localIP;
    memcpy(status.ip, &ip, 4);

    // 启动UDP
    if (!udp.begin(ARTNET_PORT)) {
        return false;
    }

    // 初始化缓冲区
    memset(dmxBuffer, 0, sizeof(dmxBuffer));
    memset(pixelBuffer, 0, sizeof(pixelBuffer));

    return true;
}

void ArtnetNode::update() {
    int packetSize = udp.parsePacket();
    if (packetSize == 0) return;

    // 读取数据包
    int length = udp.read(artnetBuffer, sizeof(artnetBuffer));
    if (length < 10) return;

    // 验证Art-Net包
    if (!validatePacket(artnetBuffer, length)) return;

    // 解析操作码
    uint16_t opcode = artnetBuffer[8] | (artnetBuffer[9] << 8);

    // 处理不同类型的Art-Net包
    switch (opcode) {
        case OpPoll:
            handleArtPoll();
            break;
        case OpDmx:
            handleArtDmx(artnetBuffer, length);
            break;
        case OpAddress:
            handleArtAddress(artnetBuffer, length);
            break;
        case OpRdm:
            handleArtRdm(artnetBuffer, length);
            break;
        case OpSync:
            handleArtSync();
            break;
    }
}

void ArtnetNode::handleArtDmx(uint8_t* data, uint16_t length) {
    if (length < 18) return;  // DMX数据包最小长度

    uint8_t sequence = data[12];
    uint8_t physical = data[13];
    uint8_t subnet = data[14] >> 4;
    uint8_t universe = data[14] & 0x0F;
    uint16_t dmxLength = (data[16] << 8) | data[17];

    // 检查是否是目标宇宙
    if (subnet != config.subnet || universe != config.universe) {
        return;
    }

    // 限制DMX数据长度
    if (dmxLength > ARTNET_DMX_LENGTH) {
        dmxLength = ARTNET_DMX_LENGTH;
    }

    // 复制DMX数据
    memcpy(dmxBuffer, &data[18], dmxLength);

    // 调用DMX回调
    if (dmxCallback) {
        dmxCallback(universe, dmxBuffer, dmxLength);
    }

    // 更新DMX输出
    if (dmx) {
        dmx->write(dmxBuffer, dmxLength);
    }

    // 处理像素数据
    if (pixels && config.pixelCount > 0) {
        uint16_t pixelLength = config.pixelCount * 3;
        if (pixelLength > dmxLength) {
            pixelLength = dmxLength;
        }
        memcpy(pixelBuffer, dmxBuffer, pixelLength);
        if (pixelCallback) {
            pixelCallback(pixelBuffer, pixelLength);
        }
        pixels->update();
    }
}

void ArtnetNode::handleArtPoll() {
    sendArtPollReply();
}

void ArtnetNode::sendArtPollReply() {
    uint8_t reply[239];
    memset(reply, 0, sizeof(reply));

    // ID
    memcpy(reply, ARTNET_ID, 8);
    
    // OpCode
    reply[8] = OpPollReply & 0xFF;
    reply[9] = (OpPollReply >> 8) & 0xFF;
    
    // IP地址
    memcpy(reply + 10, status.ip, 4);
    
    // 端口
    reply[14] = (ARTNET_PORT >> 8) & 0xFF;
    reply[15] = ARTNET_PORT & 0xFF;
    
    // 版本信息
    reply[16] = status.version >> 8;
    reply[17] = status.version & 0xFF;
    
    // 网络设置
    reply[18] = config.subnet;
    reply[19] = status.firmware;
    
    // 状态
    reply[21] = status.status1;
    reply[22] = status.status2;
    
    // 名称
    memcpy(reply + 26, config.shortName, 18);
    memcpy(reply + 44, config.longName, 64);
    
    // 发送回复
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(reply, sizeof(reply));
    udp.endPacket();
}

bool ArtnetNode::validatePacket(uint8_t* data, uint16_t length) {
    // 检查Art-Net ID
    return (length >= 10) && (memcmp(data, ARTNET_ID, 8) == 0);
}

void ArtnetNode::setConfig(const Config& config) {
    this->config = config;
    updateStatus();
}

void ArtnetNode::updateStatus() {
    status.goodInput = 0x80;  // 数据是好的
    status.goodOutput = 0x80; // 输出是好的
    status.status1 = 0x80;    // 显示正常运行
}

// 回调设置方法
void ArtnetNode::setDMXCallback(void (*callback)(uint16_t, uint8_t*, uint16_t)) {
    dmxCallback = callback;
}

void ArtnetNode::setRDMCallback(void (*callback)(uint8_t*, uint16_t)) {
    rdmCallback = callback;
}

void ArtnetNode::setPixelCallback(void (*callback)(uint8_t*, uint16_t)) {
    pixelCallback = callback;
}

void ArtnetNode::handleArtRdm(uint8_t* data, uint16_t size) {
    // 处理 Art-Net RDM 包
    if (!data || size < ART_RDM_MIN_SIZE) {
        return;
    }

    // 解析 RDM 数据
    uint8_t sequenceNumber = data[12];
    uint8_t portAddress = data[13];
    uint8_t* rdmData = &data[14];
    uint16_t rdmDataLength = size - 14;

    // TODO: 实现 RDM 处理逻辑
    // 这里添加您的 RDM 处理代码
}

void ArtnetNode::handleArtAddress(uint8_t* data, uint16_t size) {
    // 处理 Art-Net Address 包
    if (!data || size < ART_ADDRESS_MIN_SIZE) {
        return;
    }

    // 解析地址配置
    uint8_t netSwitch = data[12];
    uint8_t subSwitch = data[13];
    uint8_t universe = data[14];
    uint8_t commandResponse = data[15];

    // 更新配置
    if (netSwitch != 0x7f) {
        config.net = netSwitch;
    }
    if (subSwitch != 0x7f) {
        config.subnet = subSwitch;
    }
    if (universe != 0x7f) {
        config.universe = universe;
    }

    // TODO: 处理其他地址配置
    // 这里添加额外的地址处理代码
}

void ArtnetNode::handleArtSync() {
    // 处理 Art-Net Sync 包
    // 通常用于同步多个节点的输出

    // 设置同步标志
    syncReceived = true;

    // 如果启用了同步模式，则在收到同步包时更新输出
    if (syncMode) {
        // 更新 DMX 输出
        updateDmxOutput();
    }
}

void ArtnetNode::updateDmxOutput() {
    // 实际的 DMX 输出更新逻辑
    // 这个方法应该在您的代码中已经实现
}

// 如果需要，添加其他辅助方法
bool ArtnetNode::isValidArtNet(uint8_t* data, uint16_t size) {
    // 验证 Art-Net 包的有效性
    if (!data || size < ART_NET_MIN_SIZE) {
        return false;
    }

    // 检查 Art-Net ID
    if (data[0] != 'A' || data[1] != 'r' || data[2] != 't' || data[3] != '-' ||
        data[4] != 'N' || data[5] != 'e' || data[6] != 't' || data[7] != 0x00) {
        return false;
    }

    return true;
}