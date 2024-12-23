// script.js

// WebSocket 连接管理类
class WebSocketManager {
    constructor() {
        this.ws = null;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 3000;
        this.init();
    }

    init() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;
        this.connect(wsUrl);
    }

    connect(url) {
        this.ws = new WebSocket(url);

        this.ws.onopen = () => {
            console.log('WebSocket 已连接');
            this.reconnectAttempts = 0;
            this.updateConnectionStatus(true);
        };

        this.ws.onclose = () => {
            console.log('WebSocket 已断开');
            this.updateConnectionStatus(false);
            this.attemptReconnect();
        };

        this.ws.onerror = (error) => {
            console.error('WebSocket 错误:', error);
            this.updateConnectionStatus(false);
        };

        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this.handleMessage(data);
            } catch (e) {
                console.error('消息解析错误:', e);
            }
        };
    }

    attemptReconnect() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            console.log(`尝试重新连接... (${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
            setTimeout(() => this.init(), this.reconnectDelay);
        }
    }

    updateConnectionStatus(connected) {
        const indicator = document.getElementById('connection-status');
        if (indicator) {
            indicator.classList.toggle('connected', connected);
            indicator.classList.toggle('disconnected', !connected);
            indicator.title = connected ? '已连接' : '已断开';
        }
    }

    handleMessage(data) {
        switch (data.type) {
            case 'status':
                this.updateStatus(data);
                break;
            case 'config':
                this.updateConfig(data);
                break;
            case 'ap_status':
                this.updateAPStatus(data);
                break;
            case 'pixel_test':
                this.handlePixelTestResponse(data);
                break;
            default:
                console.log('未知消息类型:', data.type);
        }
    }

    updateStatus(data) {
        this.updateElementText('uptime', this.formatUptime(data.uptime));
        this.updateElementText('wifi-strength', `${data.rssi} dBm`);
        this.updateElementText('memory-usage', this.formatBytes(data.freeHeap));
        if (data.ap_enabled !== undefined) {
            this.updateAPStatus(data);
        }
    }

    updateConfig(data) {
        Object.entries(data).forEach(([key, value]) => {
            const element = document.getElementById(this.camelToKebab(key));
            if (element) {
                if (element.type === 'checkbox') {
                    element.checked = value;
                } else if (element.type === 'number') {
                    element.value = parseInt(value);
                } else {
                    element.value = value;
                }
            }
        });

        const dhcpEnabled = document.getElementById('dhcp-enabled');
        if (dhcpEnabled) {
            this.handleDHCPChange(dhcpEnabled.checked);
        }
    }

    updateAPStatus(data) {
        const elements = {
            status: document.getElementById('ap-status'),
            text: document.getElementById('ap-status-text'),
            ip: document.getElementById('ap-ip'),
            stations: document.getElementById('ap-stations')
        };

        if (elements.status) {
            elements.status.style.display = data.ap_enabled ? 'block' : 'none';
            if (elements.text) elements.text.textContent = data.ap_enabled ? '运行中' : '已停止';
            if (data.ap_enabled) {
                this.updateElementText('ap-ip', data.ap_ip || '-');
                this.updateElementText('ap-stations', data.ap_stations || '0');
            }
        }
    }

    handlePixelTestResponse(data) {
        const testStatus = document.getElementById('pixel-test-status');
        if (testStatus) {
            testStatus.textContent = data.success ? '测试运行中...' : '测试失败';
            testStatus.className = data.success ? 'success' : 'error';
        }
    }

    formatUptime(seconds) {
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = seconds % 60;
        return `${days}天 ${hours}小时 ${minutes}分 ${secs}秒`;
    }

    formatBytes(bytes) {
        const sizes = ['B', 'KB', 'MB', 'GB'];
        if (bytes === 0) return '0 B';
        const i = Math.floor(Math.log(bytes) / Math.log(1024));
        return `${(bytes / Math.pow(1024, i)).toFixed(2)} ${sizes[i]}`;
    }

    camelToKebab(str) {
        return str.replace(/([a-z0-9]|(?=[A-Z]))([A-Z])/g, '$1-$2').toLowerCase();
    }

    send(data) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(data));
        } else {
            console.warn('WebSocket未连接，无法发送消息');
        }
    }

    updateElementText(id, text) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = text;
        }
    }
}

// UI 管理类
class UIManager {
    constructor(wsManager) {
        this.wsManager = wsManager;
        this.lastConfig = null;
        this.isInitialized = false;
        
        // 立即加载配置
        this.initializeConfig().then(() => {
            this.initializeTabs();
            this.initializeFormHandlers();
            this.initializeSpecialControls();
            this.initializeSystemControls();
            this.startConfigRefreshTimer();
        });
        // 添加刷新配置的定时器，初始加载配置
        this.refreshCurrentConfig();
        this.startConfigRefreshTimer();
        this.lastConfig = {}; // 添加配置缓存
        this.configUpdateTimeout = null; // 添加更新超时控制

        // 添加页面可见性变化监听
        document.addEventListener('visibilitychange', () => {
            if (document.visibilityState === 'visible') {
                this.refreshCurrentConfig();
            }
        });
    }

    // 初始化配置
    async initializeConfig() {
        try {
            // 首先尝试从localStorage获取缓存的配置
            const cachedConfig = this.loadCachedConfig();
            if (cachedConfig) {
                this.updateConfigDisplay(cachedConfig);
            }

            // 然后立即从服务器获取最新配置
            await this.refreshCurrentConfig();
            this.isInitialized = true;
        } catch (error) {
            console.error('初始化配置失败:', error);
            // 如果失败，至少显示缓存的配置
            const cachedConfig = this.loadCachedConfig();
            if (cachedConfig) {
                this.updateConfigDisplay(cachedConfig);
            }
        }
    }

    // 加载缓存的配置
    loadCachedConfig() {
        try {
            const cached = localStorage.getItem('lastConfig');
            return cached ? JSON.parse(cached) : null;
        } catch (error) {
            console.error('读取缓存配置失败:', error);
            return null;
        }
    }

    // 保存配置到缓存
    saveCachedConfig(config) {
        try {
            localStorage.setItem('lastConfig', JSON.stringify(config));
        } catch (error) {
            console.error('保存配置到缓存失败:', error);
        }
    }

     // 网络配置更新
     updateNetworkConfig(config, focusedElement) {
        // DHCP设置
        const dhcpSwitch = document.getElementById('dhcp-enabled');
        if (dhcpSwitch && dhcpSwitch !== focusedElement) {
            dhcpSwitch.checked = config.dhcpEnabled ?? false;
            this.handleDHCPChange();
        }

        // 静态IP设置
        ['static-ip', 'static-mask', 'static-gateway'].forEach(id => {
            const element = document.getElementById(id);
            if (element && element !== focusedElement) {
                const configKey = id.replace(/-/g, '');
                element.value = config[configKey] || '';
            }
        });
    }

    // Art-Net配置更新
    updateArtNetConfig(config, focusedElement) {
        ['artnet-net', 'artnet-subnet', 'artnet-universe', 'dmx-start-address'].forEach(id => {
            const element = document.getElementById(id);
            if (element && element !== focusedElement) {
                const configKey = id.replace(/-/g, '');
                const value = config[configKey];
                if (value !== undefined) {
                    element.value = value;
                }
            }
        });
    }

    // 像素配置更新
    updatePixelConfig(config, focusedElement) {
        ['pixel-count', 'pixel-type'].forEach(id => {
            const element = document.getElementById(id);
            if (element && element !== focusedElement) {
                const configKey = id.replace(/-/g, '');
                const value = config[configKey];
                if (value !== undefined) {
                    element.value = value;
                }
            }
        });

        const pixelEnabled = document.getElementById('pixel-enabled');
        if (pixelEnabled && pixelEnabled !== focusedElement) {
            pixelEnabled.checked = config.pixelEnabled ?? false;
        }
    }

    // 改进的表单数据处理方法
    processFormData(formData) {
        const data = {};
        formData.forEach((value, key) => {
            const element = document.getElementById(key);
            if (element) {
                if (element.type === 'checkbox') {
                    data[key] = element.checked;
                } else if (element.type === 'number') {
                    data[key] = parseInt(value) || 0;
                } else {
                    data[key] = value;
                }
            }
        });
        return data;
    }

    // 改进的提示框显示方法
    showToast(message, type = 'info') {
        const toast = document.getElementById('toast');
        if (toast) {
            const icon = type === 'success' ? '✓' : type === 'error' ? '✗' : 'ℹ';
            
            // 清除之前的定时器和进度条
            if (this.toastTimeout) {
                clearTimeout(this.toastTimeout);
                const oldProgress = toast.querySelector('.toast-progress');
                if (oldProgress) {
                    oldProgress.remove();
                }
            }

            toast.innerHTML = `
                <span class="toast-icon">${icon}</span>
                <span class="toast-message">${message}</span>
                <div class="toast-progress"></div>
            `;
            
            toast.className = `toast show ${type}`;
            
            this.toastTimeout = setTimeout(() => {
                toast.classList.remove('show');
            }, 3000);
        }
    }


    // 改进的配置刷新定时器
    startConfigRefreshTimer() {
        setInterval(() => {
            if (!document.hidden) { // 只在页面可见时刷新
                this.refreshCurrentConfig();
            }
        }, 15000); // 缩短刷新间隔以保持更及时的更新
    }

    // 改进的配置刷新方法
    async refreshCurrentConfig() {
        try {
            const response = await fetch('/api/config');
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            const config = await response.json();
            
            // 更新显示并保存到缓存
            this.updateConfigDisplay(config);
            this.saveCachedConfig(config);
            this.lastConfig = config;
            
            return true;
        } catch (error) {
            console.error('获取配置失败:', error);
            return false;
        }
    }

    // 更新配置显示
    updateConfigDisplay(config) {
        // 更新网络配置显示
        if (config.dhcpEnabled !== undefined) {
            const dhcpSwitch = document.getElementById('dhcp-enabled');
            if (dhcpSwitch) {
                dhcpSwitch.checked = config.dhcpEnabled;
                this.handleDHCPChange();
            }
        }
        
        // 保存当前焦点元素
        const focusedElement = document.activeElement;

        // 更新网络配置
        this.updateNetworkConfig(config, focusedElement);
        
        // 更新Art-Net配置
        this.updateArtNetConfig(config, focusedElement);
        
        // 更新像素配置
        this.updatePixelConfig(config, focusedElement);
        
        // 更新其他通用配置
        this.updateGeneralConfig(config, focusedElement);


        // 更新静态IP设置
        ['static-ip', 'static-mask', 'static-gateway'].forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                const configKey = id.replace(/-/g, '');
                element.value = config[configKey] || '';
            }
        });

        // 更新Art-Net设置
        ['artnet-net', 'artnet-subnet', 'artnet-universe', 'dmx-start-address'].forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                const configKey = id.replace(/-/g, '');
                element.value = config[configKey] || '';
            }
        });

        // 更新像素设置
        ['pixel-count', 'pixel-type'].forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                const configKey = id.replace(/-/g, '');
                element.value = config[configKey] || '';
            }
        });

        const pixelEnabled = document.getElementById('pixel-enabled');
        if (pixelEnabled) {
            pixelEnabled.checked = config.pixelEnabled || false;
        }

        // 更新设备名称
        const deviceName = document.getElementById('device-name');
        if (deviceName) {
            deviceName.value = config.deviceName || '';
        }
    }

    // 表单提交处理改进
    async handleFormSubmit(e, formType, label) {
        e.preventDefault();
        const form = e.target;
        
        // 禁用表单元素防止重复提交
        const formElements = Array.from(form.elements);
        formElements.forEach(element => element.disabled = true);

        try {
            const formData = new FormData(form);
            const data = this.processFormData(formData);

            const response = await fetch(`/api/${formType}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            // 保存成功后更新缓存和显示
            const updatedConfig = {...this.lastConfig, ...data};
            this.saveCachedConfig(updatedConfig);
            this.lastConfig = updatedConfig;
            
            this.showToast(`${label}设置已保存并已更新显示`, 'success');
            
            // 延迟刷新以确保服务器更新完成
            setTimeout(() => this.refreshCurrentConfig(), 500);

        } catch (error) {
            this.showToast(`${label}设置保存失败: ${error.message}`, 'error');
            // 恢复到最后已知的正确配置
            if (this.lastConfig) {
                this.updateConfigDisplay(this.lastConfig);
            }
        } finally {
            // 重新启用表单元素
            formElements.forEach(element => element.disabled = false);
        }
    }
  
    // 自定义增强的提示框
    showToast(message, type = 'info') {
        const toast = document.getElementById('toast');
        if (toast) {
            // 添加更详细的状态信息
            const icon = type === 'success' ? '✓' : type === 'error' ? '✗' : 'ℹ';
            toast.innerHTML = `<span class="toast-icon">${icon}</span> ${message}`;
            toast.className = `toast show ${type}`;
            
            // 添加进度条动画
            const progress = document.createElement('div');
            progress.className = 'toast-progress';
            toast.appendChild(progress);
            
            setTimeout(() => {
                toast.classList.remove('show');
                // 等动画结束后移除进度条
                setTimeout(() => {
                    if (progress.parentNode === toast) {
                        toast.removeChild(progress);
                    }
                }, 300);
            }, 3000);
        }
    }


    initializeTabs() {
        const tabs = document.querySelectorAll('.tab-nav li');
        const contents = document.querySelectorAll('.tab-content');

        tabs.forEach(tab => {
            tab.addEventListener('click', () => {
                tabs.forEach(t => t.classList.remove('active'));
                contents.forEach(c => c.classList.remove('active'));
                tab.classList.add('active');
                const content = document.getElementById(tab.dataset.tab);
                if (content) content.classList.add('active');
            });
        });
    }

    initializeFormHandlers() {
        const forms = {
            network: '网络',
            artnet: 'Art-Net',
            pixel: '像素',
            ap: 'AP模式'
        };

        Object.entries(forms).forEach(([type, label]) => {
            const form = document.getElementById(`${type}-form`);
            if (form) {
                form.addEventListener('submit', async (e) => {
                    e.preventDefault();
                    await this.handleFormSubmit(e, type, label);
                });
            }
        });
    }

    initializeSpecialControls() {
        this.addChangeListener('dhcp-enabled', this.handleDHCPChange.bind(this));
        this.addChangeListener('pixel-test', this.handlePixelTest.bind(this));
        this.addChangeListener('ap-enabled', this.handleAPEnabledChange.bind(this));
    }

    initializeSystemControls() {
        const commands = {
            reboot: '重启',
            'factory-reset': '恢复出厂设置'
        };

        Object.entries(commands).forEach(([cmd, label]) => {
            const btn = document.getElementById(`${cmd}-btn`);
            if (btn) {
                btn.addEventListener('click', () => {
                    const confirmMsg = cmd === 'reboot'
                        ? '确定要重启设备吗？'
                        : '确定要恢复出厂设置吗？所有配置将被清除！';

                    if (confirm(confirmMsg)) {
                        this.sendSystemCommand(cmd);
                    }
                });
            }
        });
    }

    addChangeListener(id, handler) {
        const element = document.getElementById(id);
        if (element) {
            element.addEventListener('change', handler);
        }
    }

    async handleFormSubmit(e, formType, label) {
        const form = e.target;
        const formData = new FormData(form);
        const data = {};

        formData.forEach((value, key) => {
            const inputElement = form[key];
            if (inputElement) {
                if (inputElement.type === 'checkbox') {
                    data[key] = inputElement.checked;
                } else if (inputElement.type === 'number') {
                    data[key] = parseInt(value);
                } else {
                    data[key] = value;
                }
            }
        });

        try {
            const response = await fetch(`/api/${formType}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });

            
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            this.showToast(`${label}设置已保存`, 'success');

            if (formType === 'ap' && data.enabled) {
                const apStatus = document.getElementById('ap-status');
                if (apStatus) apStatus.style.display = 'block';
            }
        } catch (error) {
            this.showToast(`${label}设置保存失败: ${error.message}`, 'error');
        }
    }

    handleDHCPChange() {
        const dhcpEnabled = document.getElementById('dhcp-enabled');
        const staticSettings = document.getElementById('static-ip-settings');
        if (dhcpEnabled && staticSettings) {
            staticSettings.style.display = dhcpEnabled.checked ? 'none' : 'block';
        }
    }

    handlePixelTest() {
        const testSelect = document.getElementById('pixel-test');
        if (testSelect) {
            this.wsManager.send({
                type: 'pixel-test',
                mode: parseInt(testSelect.value)
            });
        }
    }

    handleAPEnabledChange(e) {
        const apStatus = document.getElementById('ap-status');
        if (apStatus) {
            apStatus.style.display = e.target.checked ? 'block' : 'none';
        }
    }

    async sendSystemCommand(command) {
        try {
            const response = await fetch(`/api/${command}`, { method: 'POST' });
            if (!response.ok) throw new Error('命令执行失败');

            const label = command === 'reboot' ? '重启' : '重置';
            this.showToast(`${label}命令已发送`, 'success');
        } catch (error) {
            this.showToast(`命令执行失败: ${error.message}`, 'error');
        }
    }

    showToast(message, type = 'info') {
        const toast = document.getElementById('toast');
        if (toast) {
            toast.textContent = message;
            toast.className = `toast show ${type}`;
            setTimeout(() => toast.classList.remove('show'), 3000);
        }
    }
}

// 页面加载完成后初始化应用
document.addEventListener('DOMContentLoaded', () => {
    const wsManager = new WebSocketManager();
    const uiManager = new UIManager(wsManager);
});