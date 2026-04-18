#include "BleBridge.h"
#include <NimBLEDevice.h>
#include <esp_system.h>
#include <esp_mac.h>
#include "../Logger.h"

static const char* TAG = "BLE";

// Nordic UART Service (de-facto serial-over-BLE standard)
static const char* NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* NUS_RX_UUID      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";  // host → device (write)
static const char* NUS_TX_UUID      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";  // device → host (notify)

BleBridge* BleBridge::_instance = nullptr;

BleBridge::BleBridge()
    : _rxHead(0), _rxTail(0), _lineLen(0),
      _started(false), _connected(false), _secured(false),
      _mtu(23), _lastRxMs(0), _lastTxMs(0), _passkey(0) {
    _deviceName[0] = 0;
}

BleBridge* BleBridge::getInstance() {
    if (!_instance) _instance = new BleBridge();
    return _instance;
}

// ============================================================
// NimBLE callbacks — thin forwarders to BleBridge
// ============================================================

namespace {

NimBLECharacteristic* g_txChar = nullptr;
NimBLECharacteristic* g_rxChar = nullptr;

class RxCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic* c) override {
        std::string v = c->getValue();
        if (!v.empty()) {
            BleBridge::getInstance()->_onRxData(
                reinterpret_cast<const uint8_t*>(v.data()), v.size());
        }
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer* s, ble_gap_conn_desc* desc) override {
        BleBridge::getInstance()->_onConnect(desc->conn_handle);
    }
    void onDisconnect(NimBLEServer* s) override {
        BleBridge::getInstance()->_onDisconnect();
    }
    void onMTUChange(uint16_t mtu, ble_gap_conn_desc* desc) override {
        BleBridge::getInstance()->_onMtuChange(mtu);
    }
    uint32_t onPassKeyRequest() override {
        // Display-only peripheral — desktop app prompts user for passkey;
        // we supply it from the bridge's random value.
        uint32_t k = BleBridge::getInstance()->getPasskey();
        BleBridge::getInstance()->_onPasskeyRequest(k);
        return k;
    }
    void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
        bool encrypted = desc->sec_state.encrypted;
        BleBridge::getInstance()->_onAuthComplete(encrypted, encrypted);
    }
    bool onConfirmPIN(uint32_t pin) override { return true; }
};

ServerCallbacks g_serverCb;
RxCallbacks     g_rxCb;

}  // namespace

// ============================================================
// Public API
// ============================================================

bool BleBridge::begin() {
    if (_started) return true;

    // Build device name: "Claude-XXXX" where XXXX = last 2 BT-MAC bytes
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(_deviceName, sizeof(_deviceName), "Claude-%02X%02X", mac[4], mac[5]);

    // Generate a random 6-digit passkey for LE Secure Connections.
    // Re-roll on every boot; the desktop app prompts the user for it on pairing.
    _passkey = 100000 + (esp_random() % 900000);

    LOG_INFO(TAG, "Starting NimBLE as %s (passkey=%06lu)",
             _deviceName, (unsigned long)_passkey);

    NimBLEDevice::init(_deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(517);

    // Match the official firmware's security posture:
    //   LE Secure Connections + MITM + Bonding, DisplayOnly IO cap.
    // The central (Claude desktop) is KeyboardOnly, so it prompts the user
    // for the 6-digit passkey we show on the e-paper.
    NimBLEDevice::setSecurityAuth(true, true, true);        // bond, mitm, sc
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    NimBLEDevice::setSecurityPasskey(_passkey);

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(&g_serverCb);

    NimBLEService* nus = server->createService(NUS_SERVICE_UUID);

    // TX notify + read are encryption-gated (so the peer must pair before
    // it can subscribe and see transcript snippets).
    g_txChar = nus->createCharacteristic(
        NUS_TX_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY |
        NIMBLE_PROPERTY::READ_ENC);

    // RX is encryption-gated too.
    g_rxChar = nus->createCharacteristic(
        NUS_RX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR |
        NIMBLE_PROPERTY::WRITE_ENC);
    g_rxChar->setCallbacks(&g_rxCb);

    nus->start();

    // Mirror the upstream BlueDroid advertising exactly — let NimBLE manage
    // the split between advertising data and scan response automatically.
    // Do NOT call setAdvertisementData/setScanResponseData manually; that
    // overrides NimBLE's built-in flag/name handling and breaks macOS
    // CoreBluetooth's scan filter.
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(NUS_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);      // 7.5 ms — iOS-friendly connection interval
    adv->setMaxPreferred(0x12);      // 22.5 ms
    NimBLEDevice::startAdvertising();

    _started = true;
    LOG_INFO(TAG, "NimBLE advertising started");
    return true;
}

void BleBridge::restartAdvertising() {
    if (!_started) return;
    NimBLEDevice::startAdvertising();
}

size_t BleBridge::write(const uint8_t* data, size_t len) {
    if (!_connected || !g_txChar || !len) return 0;

    // ATT notify payload cap = negotiated MTU - 3 (ATT header).
    size_t chunk = (_mtu > 3) ? (size_t)(_mtu - 3) : 20;
    if (chunk > 180) chunk = 180;     // macOS Central is happier below 185

    size_t sent = 0;
    while (sent < len) {
        size_t n = len - sent;
        if (n > chunk) n = chunk;
        g_txChar->setValue(const_cast<uint8_t*>(data + sent), n);
        g_txChar->notify();
        sent += n;
        delay(4);   // let the BLE stack drain; skipping this drops packets on macOS
    }
    _lastTxMs = millis();
    LOG_INFO(TAG, "TX notify sent %zu bytes (mtu=%u)", sent, _mtu);
    return sent;
}

size_t BleBridge::writeLine(const char* json) {
    if (!json) return 0;
    size_t n = write(reinterpret_cast<const uint8_t*>(json), strlen(json));
    if (n) write(reinterpret_cast<const uint8_t*>("\n"), 1);
    return n;
}

void BleBridge::poll() {
    // Drain the ring buffer into the line assembler; fire lineCb for each \n-terminated JSON.
    while (_rxAvailable() > 0) {
        int c = _rxPop();
        if (c < 0) break;
        if (c == '\n' || c == '\r') {
            if (_lineLen > 0) {
                _lineBuf[_lineLen] = 0;
                if (_lineBuf[0] == '{' && _lineCb) {
                    _lineCb(_lineBuf, _lineLen);
                }
                _lineLen = 0;
            }
        } else if (_lineLen < LINE_CAP - 1) {
            _lineBuf[_lineLen++] = (char)c;
        } else {
            // Line overflow — discard until next \n to resync.
            _lineLen = 0;
        }
    }
}

void BleBridge::clearBonds() {
    NimBLEDevice::deleteAllBonds();
    LOG_INFO(TAG, "All BLE bonds cleared");
}

// ============================================================
// Internal hooks (invoked from BLE task)
// ============================================================

void BleBridge::_onRxData(const uint8_t* data, size_t len) {
    _lastRxMs = millis();
    _rxPush(data, len);
}

void BleBridge::_onConnect(uint16_t connHandle) {
    _connected = true;
    _secured   = false;
    LOG_INFO(TAG, "Peer connected (handle=%u) — showing passkey %06lu",
             connHandle, (unsigned long)_passkey);
    if (_connectCb) _connectCb(true, false);
    // DisplayOnly IO cap means the stack won't call onPassKeyRequest on us —
    // the central (Claude Desktop) asks the user to type the passkey we
    // pre-configured via setSecurityPasskey. So we must proactively tell
    // the UI to render the passkey right now, while the user is looking at
    // the macOS pairing prompt.
    if (_passkey && _passkeyCb) _passkeyCb(_passkey);
}

void BleBridge::_onDisconnect() {
    _connected = false;
    _secured   = false;
    _mtu       = 23;
    _lineLen   = 0;
    _rxHead = _rxTail = 0;
    LOG_INFO(TAG, "Peer disconnected, re-advertising");
    if (_connectCb) _connectCb(false, false);
    NimBLEDevice::startAdvertising();
}

void BleBridge::_onMtuChange(uint16_t mtu) {
    _mtu = mtu;
    LOG_INFO(TAG, "MTU negotiated: %u", mtu);
}

void BleBridge::_onAuthComplete(bool success, bool encrypted) {
    _secured = encrypted;
    LOG_INFO(TAG, "Auth complete: %s, encrypted=%d",
             success ? "OK" : "FAIL", encrypted);
    // Passkey has served its purpose — clear so UI can drop the display.
    _passkey = 0;
    if (_connectCb) _connectCb(true, encrypted);
}

void BleBridge::_onPasskeyRequest(uint32_t key) {
    LOG_INFO(TAG, "Passkey requested: %06lu", (unsigned long)key);
    if (_passkeyCb) _passkeyCb(key);
}

// ============================================================
// Ring buffer — single producer (BLE task) / single consumer (loop)
// ============================================================

void BleBridge::_rxPush(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        size_t next = (_rxHead + 1) % RX_CAP;
        if (next == _rxTail) {
            // Overflow — drop byte. Upstream must keep up.
            return;
        }
        _rx[_rxHead] = data[i];
        _rxHead = next;
    }
}

int BleBridge::_rxPop() {
    if (_rxTail == _rxHead) return -1;
    uint8_t c = _rx[_rxTail];
    _rxTail = (_rxTail + 1) % RX_CAP;
    return c;
}

size_t BleBridge::_rxAvailable() const {
    return (_rxHead + RX_CAP - _rxTail) % RX_CAP;
}
