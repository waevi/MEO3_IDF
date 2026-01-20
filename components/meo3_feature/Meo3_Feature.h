#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include "cJSON.h"      // Thư viện JSON chuẩn của ESP-IDF
#include "Meo3_Mqtt.h"  // Class MQTT đã sửa ở bước trước

/**
 * MeoFeature: Lớp xử lý logic Feature/Event trên nền tảng ESP-IDF
 * - Sử dụng cJSON thay cho ArduinoJson.
 * - Sử dụng std::string để quản lý bộ nhớ chuỗi an toàn.
 */
class MeoFeature {
public:
    // Callback giữ nguyên logic cũ
    typedef void (*FeatureCallback)(
        const char* featureName,
        const char* deviceId,
        const char* const* keys,   // param keys
        const char* const* values, // param values
        uint8_t count,
        void* ctx
    );

    MeoFeature();
    ~MeoFeature();

    // Gắn transport và định danh thiết bị
    void attach(MeoMqttClient* transport, const char* deviceId);

    // Đăng ký nhận lệnh invoke
    bool beginFeatureSubscribe(FeatureCallback cb, void* ctx);

    // Gửi Event (Dùng mảng keys/values)
    bool publishEvent(const char* eventName,
                      const char* const* keys,
                      const char* const* values,
                      uint8_t count);

    // Gửi phản hồi Feature
    bool sendFeatureResponse(const char* featureName,
                             bool success,
                             const char* message);

    // Gửi trạng thái (online/offline)
    bool publishStatus(const char* status);

    // Hàm tĩnh để nhận dữ liệu từ MeoMqttClient
    static void onRawMessage(const char* topic, const uint8_t* payload, unsigned int length, void* ctx);

private:
    MeoMqttClient* _mqtt = nullptr;
    std::string    _deviceId; // Dùng std::string an toàn hơn char*

    // Callback và Context
    FeatureCallback _cb = nullptr;
    void*           _cbCtx = nullptr;

    // Hàm nội bộ xử lý logic
    void _dispatchFeatureInvoke(const char* topic, const char* payload, int length);
};