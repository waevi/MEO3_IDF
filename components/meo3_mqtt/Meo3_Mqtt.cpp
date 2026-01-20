#include "Meo3_Mqtt.h" 
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "esp_log.h"
#include "esp_random.h" 

MeoMqttClient* MeoMqttClient::_self = nullptr;

MeoMqttClient::MeoMqttClient() {
    _self = this;
    //config
    _bufferSize = 1024;
    _keepAlive = 15;
    _networkTimeout = 15000; 
}

MeoMqttClient::~MeoMqttClient() {
    disconnect();
}

void MeoMqttClient::setLogger(MeoLogFunction logger) {
    _logger = logger;
}

void MeoMqttClient::setDebugTags(const char* tagsCsv) {
    if (!tagsCsv) { _debugTags[0] = '\0'; return; }
    strncpy(_debugTags, tagsCsv, sizeof(_debugTags) - 1);
    _debugTags[sizeof(_debugTags) - 1] = '\0';
}

void MeoMqttClient::configure(const char* host, uint16_t port) {
    // Lưu vào std::string để an toàn bộ nhớ
    _host = host ? host : "";
    _port = port;
    
    if (_logger && _debugTagEnabled("MQTT")) {
        _logf("DEBUG", "MQTT", "Configured broker %s:%u", _host.c_str(), _port);
    }
}

void MeoMqttClient::setCredentials(const char* deviceId, const char* transmitKey) {
    _deviceId = deviceId ? deviceId : "";
    _txKey = transmitKey ? transmitKey : "";
    
    if (_logger && _debugTagEnabled("MQTT")) {
        _logf("DEBUG", "MQTT", "Credentials set: deviceId=%s", _deviceId.c_str());
    }
}

void MeoMqttClient::setBufferSize(uint16_t bytes) {
    _bufferSize = bytes;
}
void MeoMqttClient::setKeepAlive(uint16_t seconds) {
    _keepAlive = seconds;
}
void MeoMqttClient::setSocketTimeout(uint16_t seconds) {
    _networkTimeout = seconds * 1000; // Đổi sang ms cho IDF
}

void MeoMqttClient::setWill(const char* topic, const char* payload, uint8_t qos, bool retain) {
    _willTopic = topic ? topic : "";
    _willPayload = payload ? payload : "";
    _willQos = qos;
    _willRetain = retain;
    _hasWill = true;
}

bool MeoMqttClient::connect() {
    if (_client != NULL) {
        // Nếu client đã tồn tại, kiểm tra xem có đang nối không
        if (_connected) return true;
        // Nếu không, có thể cần destroy đi tạo lại hoặc reconnect, ở đây ta chọn destroy cho sạch
        esp_mqtt_client_stop(_client);
        esp_mqtt_client_destroy(_client);
        _client = NULL;
    }

    // 1. Tạo Client ID nếu chưa có (Thay cho millis())
    std::string finalClientId;
    if (!_deviceId.empty()) {
        finalClientId = "meo-" + _deviceId;
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "meo-device-%lu", (unsigned long)esp_random());
        finalClientId = buf;
    }

    // 2. Tạo URI string (Bắt buộc cho IDF Client)
    char uri[256];
    if (_host.empty()) return false;
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", _host.c_str(), _port);

    // 3. Cấu hình Config Struct (ESP-IDF v5.x)
    esp_mqtt_client_config_t mqtt_cfg = {};
    
    // Broker info
    mqtt_cfg.broker.address.uri = uri;
    
    // Credentials
    if (!_deviceId.empty()) {
        mqtt_cfg.credentials.client_id = finalClientId.c_str();
        mqtt_cfg.credentials.username = _deviceId.c_str();
        mqtt_cfg.credentials.authentication.password = _txKey.c_str();
    } else {
        mqtt_cfg.credentials.client_id = finalClientId.c_str();
    }

    // Timing & Buffer
    mqtt_cfg.session.keepalive = _keepAlive;
    mqtt_cfg.network.timeout_ms = _networkTimeout;
    mqtt_cfg.buffer.size = _bufferSize;
    
    // Last Will
    if (_hasWill) {
        mqtt_cfg.session.last_will.topic = _willTopic.c_str();
        mqtt_cfg.session.last_will.msg = _willPayload.c_str();
        mqtt_cfg.session.last_will.qos = _willQos;
        mqtt_cfg.session.last_will.retain = _willRetain;
        mqtt_cfg.session.last_will.msg_len = _willPayload.length();
    }

    // 4. Khởi tạo Client
    _client = esp_mqtt_client_init(&mqtt_cfg);
    if (_client == NULL) {
        _log("ERROR", "MQTT", "Failed to init client memory");
        return false;
    }

    // 5. Đăng ký Event Callback (Thay cho setCallback cũ)
    // Truyền 'this' vào arg cuối cùng để dùng trong static function
    // Thêm (esp_mqtt_event_id_t) vào trước
    esp_mqtt_client_register_event(_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, _mqtt_event_handler, this);

    // 6. Start Client
    esp_err_t err = esp_mqtt_client_start(_client);
    
    bool started = (err == ESP_OK);
    _log(started ? "INFO" : "ERROR", "MQTT", started ? "Client task started" : "Start failed");
    
    // Lưu ý: IDF connect là Async. Hàm này trả về true nghĩa là Task đã chạy, 
    // chưa chắc đã Connect thành công ngay lập tức. Trạng thái _connected sẽ update trong event handler.
    return started;
}

void MeoMqttClient::disconnect() {
    if (_client) {
        esp_mqtt_client_stop(_client);
        esp_mqtt_client_destroy(_client);
        _client = NULL;
        _connected = false;
    }
}

// Trong IDF, network chạy ngầm trong task riêng, hàm này không cần làm gì
void MeoMqttClient::loop() {
    // Empty
}

bool MeoMqttClient::isConnected() {
    return _connected;
}

bool MeoMqttClient::publish(const char* topic, const uint8_t* payload, size_t len, bool retained) {
    if (!_client || !_connected) return false;
    
    if (_logger && _debugTagEnabled("MQTT")) {
        _logf("DEBUG", "MQTT", "Publish %s len=%u retained=%d", topic ? topic : "", (unsigned)len, retained);
    }
    
    // esp_mqtt_client_publish trả về message_id (-1 là lỗi, khác -1 là đã đưa vào hàng đợi)
    // qos=0 (có thể đổi nếu muốn), retain flag chuyển thành int (0 hoặc 1)
    int msg_id = esp_mqtt_client_publish(_client, topic, (const char*)payload, len, 0, retained ? 1 : 0);
    return (msg_id != -1);
}

bool MeoMqttClient::publish(const char* topic, const char* payload, bool retained) {
    // Gọi hàm overload trên
    return publish(topic, (const uint8_t*)payload, payload ? strlen(payload) : 0, retained);
}

bool MeoMqttClient::subscribe(const char* topic, uint8_t qos) {
    if (!_client || !_connected) return false;
    
    int msg_id = esp_mqtt_client_subscribe(_client, topic, qos);
    bool ok = (msg_id != -1);

    if (_logger && _debugTagEnabled("MQTT")) {
        _logf(ok ? "DEBUG" : "ERROR", "MQTT", "%s subscribe %s",
              ok ? "OK" : "FAIL", topic ? topic : "");
    }
    return ok;
}

void MeoMqttClient::setMessageHandler(OnMessageFn fn, void* ctx) {
    _onMessage = fn;
    _onMessageCtx = ctx;
}

// --- STATIC EVENT HANDLER ---
// Đây là hàm thay thế cho _pubsubThunk và cơ chế callback cũ
void MeoMqttClient::_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    MeoMqttClient* self = (MeoMqttClient*)handler_args;
    if (!self) return;
    self->_handleEvent(event_id, event_data);
}

// --- INTERNAL EVENT PROCESSOR ---
void MeoMqttClient::_handleEvent(int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            _connected = true;
            _log("INFO", "MQTT", "Event: Connected");
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            _connected = false;
            _log("WARN", "MQTT", "Event: Disconnected");
            break;

        case MQTT_EVENT_DATA:
            // Xử lý dữ liệu nhận được
            // Lưu ý: event->topic KHÔNG có null-terminated, phải xử lý thủ công
            _invokeMessageHandler(event->topic, event->topic_len, event->data, event->data_len);
            break;

        case MQTT_EVENT_ERROR:
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                 _log("ERROR", "MQTT", "Transport Error");
            }
            break;
        default:
            break;
    }
}

void MeoMqttClient::_invokeMessageHandler(const char* topic, int topic_len, const char* data, int data_len) {
    if (_onMessage) {
        // Cần copy topic ra buffer tạm để thêm ký tự '\0' kết thúc chuỗi
        // vì thư viện IDF trả về pointer + độ dài chứ không phải C-string chuẩn.
        char t_buf[128]; 
        int t_len_safe = (topic_len < (int)sizeof(t_buf) - 1) ? topic_len : ((int)sizeof(t_buf) - 1);
        
        memcpy(t_buf, topic, t_len_safe);
        t_buf[t_len_safe] = '\0'; // Quan trọng!

        if (_logger && _debugTagEnabled("MQTT")) {
            _logf("DEBUG", "MQTT", "Incoming %s len=%d", t_buf, data_len);
        }
        
        // Gọi callback của user
        _onMessage(t_buf, (const uint8_t*)data, (unsigned int)data_len, _onMessageCtx);
    }
}

// --- LOGGING & UTILS (Giữ nguyên logic của bạn) ---

bool MeoMqttClient::_debugTagEnabled(const char* tag) const {
    if (!_debugTags[0]) return false;
    const char* p = strstr(_debugTags, tag);
    if (!p) return false;
    bool leftOk  = (p == _debugTags) || (*(p - 1) == ',');
    const char* end = p + strlen(tag);
    bool rightOk = (*end == '\0') || (*end == ',');
    return leftOk && rightOk;
}

void MeoMqttClient::_log(const char* level, const char* tag, const char* msg) const {
    if (!_logger) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "[%s] %s", tag ? tag : "MQTT", msg ? msg : "");
    _logger(level, buf);
}

void MeoMqttClient::_logf(const char* level, const char* tag, const char* fmt, ...) const {
    if (!_logger) return;
    char msg[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    _log(level, tag, msg);
}