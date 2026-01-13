#pragma once

#include <string>
#include <vector>

#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"

class MeoStorage {
public:
    MeoStorage();
    ~MeoStorage();

    // Khởi tạo NVS và mở namespace mặc định là "meo"
    bool begin(const char* ns = "meo");

    // Thay thế cho putBytes/getBytes
    bool loadBytes(const char* key, uint8_t* buffer, size_t length);
    bool saveBytes(const char* key, const uint8_t* data, size_t length);

    // Sử dụng std::string thay cho Arduino String
    bool loadString(const char* key, std::string& valueOut);
    bool saveString(const char* key, const std::string& value);

    // C-String hỗ trợ
    bool saveCString(const char* key, const char* value);
    bool loadCString(const char* key, char* buffer, size_t bufferLen);

    // Kiểu Short (int16_t)
    bool loadShort(const char* key, int16_t& valueOut);
    bool saveShort(const char* key, int16_t value);

    // Xóa một key
    bool clearKey(const char* key);
    // Xóa toàn bộ namespace
    bool clearAll();

private:
    bool _initialized;
    nvs_handle_t _handle;
    const char* _namespace;
};