#pragma once

#include <string>
#include <cstdint>

#include <BLEDevice.h>

class MeoBle {
public:
    // Định nghĩa kiểu function pointer cho callback khi có dữ liệu ghi vào
    typedef void (*OnWriteFn)(BLECharacteristic* ch, void* userCtx);

    MeoBle();

    // Khởi tạo BLE stack và đặt tên thiết bị
    // Trả về false nếu khởi tạo server thất bại
    bool begin(const char* deviceName);

    // Bắt đầu/Dừng quảng cáo (Advertising)
    void startAdvertising();
    void stopAdvertising();

    // Tạo Service bằng UUID (chuỗi)
    BLEService* createService(const char* serviceUuid);

    // Tạo Characteristic trên Service với các thuộc tính (NIMBLE_PROPERTY::READ | ...)
    BLECharacteristic* createCharacteristic(BLEService* svc,
                                               const char* charUuid,
                                               uint32_t properties);

    // Gắn hàm xử lý sự kiện Write (nhẹ, không dùng std::function)
    void setCharWriteHandler(BLECharacteristic* ch, OnWriteFn fn, void* userCtx);

    // Truy xuất đối tượng Server gốc nếu cần tính năng nâng cao
    BLEServer* server() const;

private:
    BLEServer* _server = nullptr;

    // Class nội bộ để chuyển đổi từ BLE C++ Callback sang Function Pointer
    class _Callbacks : public BLECharacteristicCallbacks {
    public:
        _Callbacks(OnWriteFn fn, void* ctx);
        
        // Override hàm onWrite của thư viện
        void onWrite(BLECharacteristic* ch) override;
    private:
        OnWriteFn _fn;
        void*     _ctx;
    };
};