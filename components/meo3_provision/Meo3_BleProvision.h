#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include "Meo3_Storage.h"
#include "Meo3_Ble.h" // Giả sử class này wrap việc init NimBLE
#include "Meo3_Type.h"

// Arduino BLE includes
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// UUID Macros (Giữ nguyên chuỗi để tiện log, nhưng code sẽ cần convert)
#define MEO_BLE_PROV_SERV_UUID      "9f27f7f0-0000-1000-8000-00805f9b34fb"
#define CH_UUID_WIFI_SSID           "9f27f7f1-0000-1000-8000-00805f9b34fb"
#define CH_UUID_WIFI_PASS           "9f27f7f2-0000-1000-8000-00805f9b34fb"
#define CH_UUID_DEV_MODEL           "9f27f7f3-0000-1000-8000-00805f9b34fb"
#define CH_UUID_DEV_MANUF           "9f27f7f4-0000-1000-8000-00805f9b34fb"
#define CH_UUID_PROV_PROG           "9f27f7f5-0000-1000-8000-00805f9b34fb"
#define CH_UUID_DEV_ID              "9f27f7f6-0000-1000-8000-00805f9b34fb"
#define CH_UUID_TX_KEY              "9f27f7f7-0000-1000-8000-00805f9b34fb"

class MeoBleProvision {
public:
    MeoBleProvision();

    void setLogger(MeoLogFunction logger);
    void setDebugTags(const char* tagsCsv);

    bool begin(MeoBle* ble, MeoStorage* storage,
               const char* devModel, const char* devManufacturer);
    
    bool begin(MeoBle* ble, MeoStorage* storage);

    void startAdvertising();
    void stopAdvertising();
    void loop();

    void setRuntimeStatus(const char* wifi, const char* mqtt);
    void setAutoRebootOnProvision(bool enable, uint32_t delayMs = 300);

    // Callback xử lý khi có thiết bị ghi dữ liệu vào (Static để tương thích C API)
    static int onAccess(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);

private:
    MeoBle*            _ble      = nullptr;
    MeoStorage*        _storage  = nullptr;

    std::string        _devModel;
    std::string        _devManuf;

    BLEService*        _svc      = nullptr;
    BLECharacteristic* _chSsid   = nullptr;
    BLECharacteristic* _chPass   = nullptr;
    BLECharacteristic* _chModel  = nullptr;
    BLECharacteristic* _chManuf  = nullptr;
    BLECharacteristic* _chDevId  = nullptr;
    BLECharacteristic* _chTxKey  = nullptr;
    BLECharacteristic* _chProg   = nullptr;

    // Trạng thái
    std::string         _wifiStatus = "unknown";
    std::string         _mqttStatus = "unknown";
    char                _statusBuf[128];

    bool                _autoReboot = true;
    uint32_t            _rebootDelayMs = 300;
    bool                _ssidWritten = false;
    bool                _passWritten = false;
    bool                _rebootScheduled = false;
    uint32_t            _rebootAtMs = 0;

    // Logging
    MeoLogFunction _logger = nullptr;
    char           _debugTags[96] = {0};

    // Internal methods
    bool _createServiceAndCharacteristics();
    void _bindWriteHandlers();
    void _loadInitialValues();
    static void _onWriteStatic(BLECharacteristic* ch, void* ctx);
    void _onWrite(BLECharacteristic* ch);
    void _updateStatus();
    void _scheduleRebootIfReady();
    bool _debugTagEnabled(const char* tag) const;
    void _log(const char* level, const char* tag, const char* msg) const;
};