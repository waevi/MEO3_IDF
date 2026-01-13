#include "Meo3_Storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <cstring>
#include <string>

static const char* TAG = "MeoStorage";

// Constructor
MeoStorage::MeoStorage()
: _initialized(false), _handle(0) {}

// Destructor (Optional: đóng handle nếu cần)
MeoStorage::~MeoStorage() {
    if (_initialized) {
        nvs_close(_handle);
    }
}

bool MeoStorage::begin(const char* ns) {
    if (_initialized) return true;

    // Khởi tạo NVS Partition (thường làm ở main, nhưng làm ở đây cho chắc chắn)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition bị lỗi, cần erase và init lại
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS Init failed");
        return false;
    }

    // Mở namespace
    err = nvs_open(ns, NVS_READWRITE, &_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS Open failed: %s", esp_err_to_name(err));
        return false;
    }

    _initialized = true;
    return true;
}

bool MeoStorage::loadBytes(const char* key, uint8_t* buffer, size_t length) {
    if (!_initialized || !key || !buffer || length == 0) return false;

    size_t required_size = 0;
    // Lấy kích thước trước để kiểm tra
    esp_err_t err = nvs_get_blob(_handle, key, NULL, &required_size);
    
    if (err != ESP_OK) return false;        // Key không tồn tại hoặc lỗi khác
    if (required_size > length) return false; // Buffer đầu vào quá nhỏ

    // Lấy dữ liệu thật
    err = nvs_get_blob(_handle, key, buffer, &required_size);
    return (err == ESP_OK);
}

bool MeoStorage::saveBytes(const char* key, const uint8_t* data, size_t length) {
    if (!_initialized || !key || !data || length == 0) return false;

    esp_err_t err = nvs_set_blob(_handle, key, data, length);
    if (err == ESP_OK) {
        err = nvs_commit(_handle); // Bắt buộc commit trong ESP-IDF
    }
    return (err == ESP_OK);
}

// Lưu ý: Sử dụng std::string thay vì String của Arduino
bool MeoStorage::loadString(const char* key, std::string& valueOut) {
    if (!_initialized || !key) return false;

    size_t required_size = 0;
    esp_err_t err = nvs_get_str(_handle, key, NULL, &required_size);
    
    if (err != ESP_OK) {
        return false; // Key không tồn tại
    }

    if (required_size == 0) {
        valueOut = "";
        return true;
    }

    char* tempBuf = new char[required_size];
    err = nvs_get_str(_handle, key, tempBuf, &required_size);
    if (err == ESP_OK) {
        valueOut.assign(tempBuf);
    }
    delete[] tempBuf;
    
    return (err == ESP_OK);
}

bool MeoStorage::saveString(const char* key, const std::string& value) {
    if (!_initialized || !key) return false;
    
    // NVS tự động kiểm tra nếu giá trị giống nhau thì không ghi vào flash
    // nhưng ta vẫn có thể kiểm tra thủ công nếu muốn tiết kiệm gọi API
    size_t required_size = 0;
    if (nvs_get_str(_handle, key, NULL, &required_size) == ESP_OK) {
        // Đọc giá trị cũ để so sánh (optional optimization)
        char* tempBuf = new char[required_size];
        nvs_get_str(_handle, key, tempBuf, &required_size);
        if (value == std::string(tempBuf)) {
            delete[] tempBuf;
            return true; // Giống nhau, không cần ghi
        }
        delete[] tempBuf;
    }

    esp_err_t err = nvs_set_str(_handle, key, value.c_str());
    if (err == ESP_OK) {
        err = nvs_commit(_handle);
    }
    return (err == ESP_OK);
}

// C-string helpers
bool MeoStorage::saveCString(const char* key, const char* value) {
    if (!_initialized || !key || !value) return false;

    // Logic so sánh để tránh ghi thừa
    size_t required_size = 0;
    if (nvs_get_str(_handle, key, NULL, &required_size) == ESP_OK) {
        char* tempBuf = new char[required_size];
        nvs_get_str(_handle, key, tempBuf, &required_size);
        if (strcmp(tempBuf, value) == 0) {
            delete[] tempBuf;
            return true; // Giống nhau
        }
        delete[] tempBuf;
    }

    esp_err_t err = nvs_set_str(_handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(_handle);
    }
    return (err == ESP_OK);
}

bool MeoStorage::loadCString(const char* key, char* buffer, size_t bufferLen) {
    if (!_initialized || !key || !buffer || bufferLen == 0) return false;

    size_t required_size = 0;
    esp_err_t err = nvs_get_str(_handle, key, NULL, &required_size);
    
    if (err != ESP_OK) return false;
    if (required_size > bufferLen) return false; // Buffer quá nhỏ

    err = nvs_get_str(_handle, key, buffer, &required_size);
    return (err == ESP_OK);
}

bool MeoStorage::loadShort(const char* key, int16_t& valueOut) {
    if (!_initialized || !key) return false;
    
    esp_err_t err = nvs_get_i16(_handle, key, &valueOut);
    return (err == ESP_OK);
}

bool MeoStorage::saveShort(const char* key, int16_t value) {
    if (!_initialized || !key) return false;

    int16_t current;
    // Kiểm tra giá trị cũ
    if (nvs_get_i16(_handle, key, &current) == ESP_OK) {
        if (current == value) return true;
    }

    esp_err_t err = nvs_set_i16(_handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(_handle);
    }
    return (err == ESP_OK);
}

bool MeoStorage::clearKey(const char* key) {
    if (!_initialized || !key) return false;
    
    esp_err_t err = nvs_erase_key(_handle, key);
    if (err == ESP_OK) {
        nvs_commit(_handle);
    }
    return (err == ESP_OK);
}

bool MeoStorage::clearAll() {
    if (!_initialized) return false;
    
    esp_err_t err = nvs_erase_all(_handle);
    if (err == ESP_OK) {
        nvs_commit(_handle);
    }
    return (err == ESP_OK);
}