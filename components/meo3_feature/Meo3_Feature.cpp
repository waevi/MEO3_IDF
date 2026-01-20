#include "Meo3_Feature.h"
#include "esp_log.h"
#include <cstdio>
#include <cstdlib>

static const char* TAG = "MeoFeature";

// Kích thước buffer tạm thời nếu cần, hoặc giới hạn tham số
#define MAX_PARAMS 10

MeoFeature::MeoFeature() {
    // Constructor
}

MeoFeature::~MeoFeature() {
    // Destructor: Không delete _mqtt vì nó được truyền từ bên ngoài vào
}

void MeoFeature::attach(MeoMqttClient* transport, const char* deviceId) {
    _mqtt = transport;
    if (deviceId) {
        _deviceId = deviceId; // std::string tự copy dữ liệu
    }

    if (_mqtt) {
        // Đăng ký hàm static làm callback, truyền 'this' làm context
        _mqtt->setMessageHandler(&MeoFeature::onRawMessage, this);
    }
}

bool MeoFeature::beginFeatureSubscribe(FeatureCallback cb, void* ctx) {
    if (!_mqtt || !_mqtt->isConnected() || _deviceId.empty()) {
        ESP_LOGW(TAG, "Cannot subscribe: MQTT not connected or DeviceID missing");
        return false;
    }

    _cb = cb;
    _cbCtx = ctx;

    // Topic: meo/{device_id}/feature/+/invoke
    std::string topic = "meo/" + _deviceId + "/feature/+/invoke";
    
    ESP_LOGI(TAG, "Subscribing to feature invoke: %s", topic.c_str());
    return _mqtt->subscribe(topic.c_str());
}

bool MeoFeature::publishEvent(const char* eventName,
                              const char* const* keys,
                              const char* const* values,
                              uint8_t count) {
    if (!_mqtt || !_mqtt->isConnected() || _deviceId.empty()) return false;

    std::string topic = "meo/" + _deviceId + "/event/" + eventName;

    // Tạo đối tượng JSON
    cJSON* root = cJSON_CreateObject();
    if (root == NULL) return false;

    for (uint8_t i = 0; i < count; ++i) {
        // cJSON_AddStringToObject tự động copy chuỗi
        cJSON_AddStringToObject(root, keys[i], values[i]);
    }

    // Serialize ra chuỗi (Unformatted để tiết kiệm dung lượng)
    char* jsonStr = cJSON_PrintUnformatted(root);
    if (jsonStr == NULL) {
        cJSON_Delete(root);
        return false;
    }

    // Gửi MQTT
    // Giả định hàm publish của MeoMqttClient nhận (topic, payload, len, retained)
    bool res = _mqtt->publish(topic.c_str(), (const uint8_t*)jsonStr, strlen(jsonStr), false);

    // Giải phóng bộ nhớ
    free(jsonStr);     // cJSON_PrintUnformatted cấp phát bằng malloc
    cJSON_Delete(root); // Xóa object JSON

    return res;
}

bool MeoFeature::sendFeatureResponse(const char* featureName,
                                     bool success,
                                     const char* message) {
    if (!_mqtt || !_mqtt->isConnected() || _deviceId.empty()) return false;

    std::string topic = "meo/" + _deviceId + "/event/feature_response";

    cJSON* root = cJSON_CreateObject();
    if (root == NULL) return false;

    cJSON_AddStringToObject(root, "feature_name", featureName);
    cJSON_AddStringToObject(root, "device_id", _deviceId.c_str());
    cJSON_AddBoolToObject(root, "success", success);
    if (message) {
        cJSON_AddStringToObject(root, "message", message);
    }

    char* jsonStr = cJSON_PrintUnformatted(root);
    if (jsonStr == NULL) {
        cJSON_Delete(root);
        return false;
    }

    bool res = _mqtt->publish(topic.c_str(), (const uint8_t*)jsonStr, strlen(jsonStr), false);

    free(jsonStr);
    cJSON_Delete(root);

    return res;
}

bool MeoFeature::publishStatus(const char* status) {
    if (!_mqtt || !_mqtt->isConnected() || _deviceId.empty()) return false;

    std::string topic = "meo/" + _deviceId + "/status";
    
    // Status thường dùng retain = true
    return _mqtt->publish(topic.c_str(), (const uint8_t*)status, strlen(status), true);
}

// Hàm tĩnh (Static)
void MeoFeature::onRawMessage(const char* topic, const uint8_t* payload, unsigned int length, void* ctx) {
    MeoFeature* self = reinterpret_cast<MeoFeature*>(ctx);
    if (!self) return;

    // Chuyển payload sang const char* để xử lý nội bộ
    self->_dispatchFeatureInvoke(topic, (const char*)payload, (int)length);
}

void MeoFeature::_dispatchFeatureInvoke(const char* topic, const char* payload, int length) {
    if (!_cb || _deviceId.empty()) return;

    // Phân tích Topic: "meo/{device_id}/feature/{featureName}/invoke"
    const char* featureMarker = strstr(topic, "/feature/");
    const char* invokeMarker  = strstr(topic, "/invoke");

    // Kiểm tra tính hợp lệ cơ bản của topic
    if (!featureMarker || !invokeMarker || invokeMarker <= featureMarker) return;

    featureMarker += 9; // độ dài của "/feature/"
    size_t nameLen = (size_t)(invokeMarker - featureMarker);
    
    if (nameLen == 0 || nameLen >= 64) return; // Giới hạn độ dài tên feature

    char featureName[64];
    memcpy(featureName, featureMarker, nameLen);
    featureName[nameLen] = '\0';

    // Chuẩn bị buffer cho JSON (cần null-terminated để parse an toàn)
    // Nếu payload không chắc chắn có null ở cuối, ta cần copy ra buffer tạm.
    char* jsonBuf = (char*)malloc(length + 1);
    if (!jsonBuf) {
        ESP_LOGE(TAG, "OOM parsing JSON");
        return;
    }
    memcpy(jsonBuf, payload, length);
    jsonBuf[length] = '\0';

    // Parse JSON
    cJSON* root = cJSON_Parse(jsonBuf);
    free(jsonBuf); // Xong việc với buffer raw string

    if (root == NULL) {
        ESP_LOGW(TAG, "Invalid JSON format");
        return;
    }

    // Chuẩn bị mảng params
    const char* keys[MAX_PARAMS];
    const char* values[MAX_PARAMS];
    uint8_t count = 0;

    cJSON* params = cJSON_GetObjectItem(root, "params");
    if (params && cJSON_IsObject(params)) {
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, params) {
            if (count >= MAX_PARAMS) break;

            // Chỉ chấp nhận value là string để đơn giản hóa (giống logic cũ)
            if (cJSON_IsString(item)) {
                keys[count]   = item->string;       // Tên key
                values[count] = item->valuestring;  // Giá trị
                count++;
            }
            // Nếu cần hỗ trợ số/bool, cần convert sang string ở đây
        }
    }

    // Gọi callback người dùng
    _cb(featureName, _deviceId.c_str(), keys, values, count, _cbCtx);

    // Dọn dẹp cJSON
    cJSON_Delete(root);
}