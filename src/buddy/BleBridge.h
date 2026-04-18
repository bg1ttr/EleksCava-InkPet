#pragma once

// BleBridge — Nordic UART Service over BLE (NimBLE-Arduino)
// Implements the wire transport for the Anthropic Claude desktop buddy protocol.
// Peer = Claude desktop app. Inbound JSON lines arrive via NUS RX, outbound
// JSON lines leave via NUS TX notifications (chunked to MTU).

#include <Arduino.h>
#include <functional>
#include <stdint.h>

class BleBridge {
public:
    using LineCallback     = std::function<void(const char* line, size_t len)>;
    using ConnectCallback  = std::function<void(bool connected, bool secured)>;
    using PasskeyCallback  = std::function<void(uint32_t passkey)>;

    static BleBridge* getInstance();

    // Start the BLE stack, build NUS service, begin advertising.
    // deviceName prefix should be "Claude" per protocol; last 2 BT-MAC bytes appended.
    bool begin();

    // Advertise again after disconnect (auto-invoked).
    void restartAdvertising();

    // Write a payload to the client (TX notify). \n suffix is sender's responsibility.
    // Returns bytes written; 0 on error or if disconnected.
    size_t write(const uint8_t* data, size_t len);
    size_t writeLine(const char* json);   // appends \n for you

    // Drain any buffered inbound data and invoke the line callback for each full line.
    // Call from loop().
    void poll();

    // Erase all bonded devices. Invoked by {"cmd":"unpair"} and factory reset.
    void clearBonds();

    // Status / introspection.
    bool isConnected() const { return _connected; }
    bool isSecured()   const { return _secured; }
    uint32_t lastRxMs() const { return _lastRxMs; }
    uint32_t lastTxMs() const { return _lastTxMs; }
    uint16_t getMtu() const   { return _mtu; }
    const char* getDeviceName() const { return _deviceName; }
    uint32_t getPasskey() const { return _passkey; }
    bool hasPendingPasskey() const { return _passkey != 0 && !_secured; }
    void clearPasskey() { _passkey = 0; }

    // Callbacks (single subscriber each).
    void onLine(LineCallback cb)       { _lineCb = cb; }
    void onConnect(ConnectCallback cb) { _connectCb = cb; }
    void onPasskey(PasskeyCallback cb) { _passkeyCb = cb; }

    // ---- Internal hooks used by the NimBLE callback classes ----
    void _onRxData(const uint8_t* data, size_t len);
    void _onConnect(uint16_t connHandle);
    void _onDisconnect();
    void _onMtuChange(uint16_t mtu);
    void _onAuthComplete(bool success, bool encrypted);
    void _onPasskeyRequest(uint32_t key);

private:
    BleBridge();
    static BleBridge* _instance;

    // RX ring buffer (inbound JSON arrives here, drained by poll()).
    static const size_t RX_CAP = 2048;
    uint8_t  _rx[RX_CAP];
    volatile size_t _rxHead;
    volatile size_t _rxTail;

    // Line assembly (per-transport — only BLE here; USB assembly lives elsewhere if needed).
    static const size_t LINE_CAP = 1024;
    char    _lineBuf[LINE_CAP];
    size_t  _lineLen;

    // Callbacks
    LineCallback     _lineCb;
    ConnectCallback  _connectCb;
    PasskeyCallback  _passkeyCb;

    // State
    bool      _started;
    bool      _connected;
    bool      _secured;
    uint16_t  _mtu;
    uint32_t  _lastRxMs;
    uint32_t  _lastTxMs;
    uint32_t  _passkey;
    char      _deviceName[24];

    // Ring buffer push (called from the BLE task — must be ISR-safe).
    void _rxPush(const uint8_t* data, size_t len);
    int  _rxPop();
    size_t _rxAvailable() const;
};
