#include "Meo3_Ble.h"

MeoBle::MeoBle() = default;

bool MeoBle::begin(const char* deviceName) {
    BLEDevice::init(deviceName && deviceName[0] ? deviceName : "MEO Device");
    // Optional minimal security; can be extended in future features
    // BLEDevice::setSecurityAuth(true, true, true);
    // BLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    _server = BLEDevice::createServer();
    return (_server != nullptr);
}

void MeoBle::startAdvertising() {
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    if (adv) adv->start();
}

void MeoBle::stopAdvertising() {
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    if (adv) adv->stop();
}

BLEService* MeoBle::createService(const char* serviceUuid) {
    if (!_server) return nullptr;
    BLEService* svc = _server->createService(serviceUuid);
    if (svc) {
        BLEAdvertising* adv = BLEDevice::getAdvertising();
        if (adv) adv->addServiceUUID(svc->getUUID());
    }
    return svc;
}

BLECharacteristic* MeoBle::createCharacteristic(BLEService* svc,
                                                   const char* charUuid,
                                                   uint32_t properties) {
    if (!svc) return nullptr;
    return svc->createCharacteristic(charUuid, properties);
}

void MeoBle::setCharWriteHandler(BLECharacteristic* ch, OnWriteFn fn, void* userCtx) {
    if (!ch || !fn) return;
    ch->setCallbacks(new _Callbacks(fn, userCtx));
}

BLEServer* MeoBle::server() const {
    return _server;
}

// _Callbacks implementation
MeoBle::_Callbacks::_Callbacks(OnWriteFn fn, void* ctx)
: _fn(fn), _ctx(ctx) {}

void MeoBle::_Callbacks::onWrite(BLECharacteristic* ch) {
    if (_fn) _fn(ch, _ctx);
}