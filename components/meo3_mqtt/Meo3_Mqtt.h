#pragma once

#include <string>
#include <cstring>
#include "esp_log.h"
#include "esp_event.h"
#include <mqtt_client.h>
#include "Meo3_Type.h"   

class MeoMqttClient {
public:
    typedef void (*OnMessageFn)(const char* topic, const uint8_t* payload, unsigned int length, void* ctx);

    MeoMqttClient();
    ~MeoMqttClient(); 

    // Logging
    void setLogger(MeoLogFunction logger);
    void setDebugTags(const char* tagsCsv);

    // Cấu hình Broker
    void configure(const char* host, uint16_t port = 1883);

    // Cấu hình Username/Password
    void setCredentials(const char* deviceId, const char* transmitKey);

    // Cấu hình nâng cao
    void setBufferSize(uint16_t bytes);     // IDF quản lý buffer tự động, nhưng có thể config outbox size
    void setKeepAlive(uint16_t seconds);    // Mặc định 120s trong IDF
    void setSocketTimeout(uint16_t seconds);// Network timeout

    // LWT (Last Will and Testament)
    void setWill(const char* topic, const char* payload, uint8_t qos = 0, bool retain = true);

    // Kết nối (Khởi động MQTT Task)
    bool connect();
    
    // Ngắt kết nối
    void disconnect();

    // Trong IDF, loop() không cần làm gì về mạng, nhưng giữ lại để tương thích logic cũ
    void loop();

    bool isConnected();

    // Publish / Subscribe
    bool publish(const char* topic, const uint8_t* payload, size_t len, bool retained = false);
    bool publish(const char* topic, const char* payload, bool retained = false);
    bool subscribe(const char* topic, uint8_t qos = 0);

    // Set callback xử lý tin nhắn
    void setMessageHandler(OnMessageFn fn, void* ctx);

    // Accessors
    const char* host() const { return _host.c_str(); }
    uint16_t    port() const { return _port; }
    const char* deviceId() const { return _deviceId.c_str(); }

private:
    // Lưu trữ thông tin config để khởi tạo sau
    std::string _host;
    uint16_t    _port = 1883;
    std::string _deviceId; // Username
    std::string _txKey;    // Password
    
    // LWT
    std::string _willTopic;
    std::string _willPayload;
    uint8_t     _willQos = 0;
    bool        _willRetain = false;
    bool        _hasWill = false;

    // Config timeout/buffer
    int         _keepAlive = 120;
    int         _networkTimeout = 10;
    int         _bufferSize = 1024;

    // --- IDF Handles ---
    esp_mqtt_client_handle_t _client = NULL;
    bool _connected = false;

    // Callbacks
    OnMessageFn  _onMessage = nullptr;
    void*        _onMessageCtx = nullptr;

    // Logging
    MeoLogFunction _logger = nullptr;
    char           _debugTags[96] = {0};

    // Static Event Handler (Bắt buộc cho IDF C-style callback)
    static void _mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    
    // Internal Helper
    void _handleEvent(int32_t event_id, void *event_data);
    void _invokeMessageHandler(const char* topic, int topic_len, const char* data, int data_len);

    bool _debugTagEnabled(const char* tag) const;
    void _log(const char* level, const char* tag, const char* msg) const;
    void _logf(const char* level, const char* tag, const char* fmt, ...) const;

    static MeoMqttClient* _self;
};