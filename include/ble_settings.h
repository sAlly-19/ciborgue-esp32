#pragma once

#include <atomic>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include "nvs_config.h"
#include "power_manager.h"

class BleSettings final : private BLECharacteristicCallbacks,
                          private BLEServerCallbacks,
                          private BLESecurityCallbacks {
    struct Packet {
        uint8_t version;
        uint8_t brightness;
        uint8_t volume;
        uint8_t muted;
        uint8_t idleChirps;
    };
    static_assert(sizeof(Packet) == 5, "BLE settings packet must be five bytes");

    static constexpr uint16_t disconnected = UINT16_MAX;
    NvsConfigManager& nvs;
    PowerManager& power;
    BLESecurity security;
    BLEServer* server = nullptr;
    QueueHandle_t pending = nullptr;
    std::atomic<uint16_t> connection{disconnected};
    std::atomic<bool> advertiseAgain{false};
    std::atomic<bool> acceptWrites{false};
    std::atomic<uint32_t> cached{0};
    uint32_t pin = 0;

    void refresh() {
        cached.store(uint32_t(nvs.getTftBrightness()) |
                     (uint32_t(nvs.getSfxVolume()) << 8) |
                     (uint32_t(nvs.isSfxMuted()) << 16) |
                     (uint32_t(nvs.isSfxIdleEnabled()) << 24));
    }

    void onRead(BLECharacteristic* characteristic, esp_ble_gatts_cb_param_t*) override {
        const uint32_t value = cached.load();
        Packet packet{1, uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24)};
        characteristic->setValue(reinterpret_cast<uint8_t*>(&packet), sizeof(packet));
    }

    void onWrite(BLECharacteristic* characteristic, esp_ble_gatts_cb_param_t*) override {
        if (!acceptWrites.load()) return;
        const auto bytes = characteristic->getValue();
        if (bytes.size() != sizeof(Packet)) return;
        Packet packet;
        memcpy(&packet, bytes.data(), sizeof(packet));
        if (packet.version != 1 || packet.volume > 100 || packet.muted > 1 || packet.idleChirps > 1) return;
        xQueueOverwrite(pending, &packet);
    }

    void onConnect(BLEServer*, esp_ble_gatts_cb_param_t* info) override {
        connection.store(info->connect.conn_id);
    }

    void onDisconnect(BLEServer*, esp_ble_gatts_cb_param_t*) override {
        connection.store(disconnected);
        advertiseAgain.store(true);
    }

    uint32_t onPassKeyRequest() override { return pin; }
    void onPassKeyNotify(uint32_t) override {}
    bool onSecurityRequest() override { return true; }
    bool onConfirmPIN(uint32_t) override { return false; }
    void onAuthenticationComplete(esp_ble_auth_cmpl_t) override {}

public:
    BleSettings(NvsConfigManager& config, PowerManager& powerManager)
        : nvs(config), power(powerManager) {}

    void begin() {
        if (!nvs.isBleEnabled() || server) return;
        if (!pending) pending = xQueueCreate(1, sizeof(Packet));
        if (!pending) {
            Serial.println("ERR:BLE_QUEUE");
            return;
        }

        pin = 100000 + esp_random() % 900000;
        BLEDevice::init("Ciborgue");
        BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
        BLEDevice::setSecurityCallbacks(this);
        security.setStaticPIN(pin);
        security.setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
        security.setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

        server = BLEDevice::createServer();
        server->setCallbacks(this);
        auto* service = server->createService("6c730001-18af-4d4f-9bdc-c034e99e1400");
        auto* characteristic = service->createCharacteristic(
            "6c730002-18af-4d4f-9bdc-c034e99e1400",
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
        characteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
        characteristic->setCallbacks(this);
        refresh();
        service->start();

        auto* advertising = BLEDevice::getAdvertising();
        advertising->addServiceUUID(service->getUUID());
        advertising->setScanResponse(true);
        acceptWrites.store(true);
        advertising->start();
    }

    void setEnabled(bool enabled) {
        const bool alreadyEnabled = nvs.isBleEnabled();
        nvs.saveBleEnabled(enabled);
        acceptWrites.store(enabled);
        if (enabled) {
            if (!server) begin();
            else if (!alreadyEnabled && !isConnected()) BLEDevice::startAdvertising();
        } else if (server) {
            BLEDevice::stopAdvertising();
            xQueueReset(pending);
            const uint16_t id = connection.load();
            if (id != disconnected) server->disconnect(id);
        }
    }

    void update() {
        if (!server) return;
        const bool shouldAdvertise = advertiseAgain.exchange(false);
        if (!nvs.isBleEnabled()) {
            xQueueReset(pending);
            return;
        }
        if (shouldAdvertise && !isConnected()) BLEDevice::startAdvertising();
        Packet packet;
        if (xQueueReceive(pending, &packet, 0) == pdTRUE) {
            power.setDisplayBrightness(packet.brightness);
            if (packet.volume != nvs.getSfxVolume()) nvs.saveSfxVolume(packet.volume);
            if (bool(packet.muted) != nvs.isSfxMuted()) nvs.saveSfxMuted(packet.muted);
            if (bool(packet.idleChirps) != nvs.isSfxIdleEnabled()) nvs.saveSfxIdleEnabled(packet.idleChirps);
        }
        refresh();
    }

    bool isConnected() const { return connection.load() != disconnected; }
    uint32_t getPin() const { return pin; }
};
