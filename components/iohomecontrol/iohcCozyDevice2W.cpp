/*
   Copyright (c) 2024. CRIDP https://github.com/cridp

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

           http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#include "iohcCozyDevice2W.h"
#include "iohcCryptoHelpers.h"
#include <ArduinoJson.h>
#include <numeric>
#include <stdio.h>
#include <sys/stat.h>
#include "esp_log.h"

static const char *COZY_TAG = "iohcCozy2W";
static const char *FS_BASE  = "/littlefs";

namespace IOHC {
    iohcCozyDevice2W *iohcCozyDevice2W::_iohcCozyDevice2W = nullptr;

    iohcCozyDevice2W::iohcCozyDevice2W() = default;

    iohcCozyDevice2W *iohcCozyDevice2W::getInstance() {
        if (!_iohcCozyDevice2W) {
            _iohcCozyDevice2W = new iohcCozyDevice2W();
            _iohcCozyDevice2W->load();
            _iohcCozyDevice2W->initializeValid();
        }
        return _iohcCozyDevice2W;
    }

    /**
    * @brief Forge a Cozy packet into iohcPacket. This is the function that is called when calling a command
    * @param packet * The IOHC packet to forge
    * @param toSend
    */
    void iohcCozyDevice2W::forgePacket(iohcPacket *packet, const std::vector<unsigned char> &toSend) {
        gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
        IOHC::relStamp.store(esp_timer_get_time());

        // Common Flags
        // 8 if protocol version is 0 else 10
        packet->payload.packet.header.CtrlByte1.asStruct.MsgLen = sizeof(_header) - 1;
        packet->payload.packet.header.CtrlByte1.asStruct.Protocol = 0;
        packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
        packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 0;
        packet->payload.packet.header.CtrlByte2.asByte = 0;

        packet->payload.packet.header.CtrlByte1.asByte += toSend.size();
        memcpy(packet->payload.buffer + 9, toSend.data(), toSend.size());
        packet->buffer_length = toSend.size() + 9;

        packet->frequency = CHANNEL2;
        packet->repeatTime = 25;
        packet->repeat = 0;
        packet->lock = false;
    }

    /**
    * @brief Checks if this cozy our fake gateway.
    */
    bool iohcCozyDevice2W::isFake(address nodeSrc, address nodeDst) {
        this->Fake = false;
        if (!memcmp(this->gateway, nodeSrc, 3) || !memcmp(this->gateway, nodeDst, 3)) { this->Fake = true; }
        return this->Fake;
    }

    /// Emulates device button press
    void iohcCozyDevice2W::cmd(DeviceButton cmd, Tokens *data) {
        if (!_radioInstance) {
            ESP_LOGE(COZY_TAG, "NO RADIO INSTANCE");
            _radioInstance = IOHC::iohcRadio::getInstance();
        }

        switch (cmd) {
            case DeviceButton::associate: {
                std::vector<uint8_t> toSend = {};

                auto* packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_ASK_CHALLENGE_0x31;
                memorizeSend.memorizedData = toSend;
                memorizeSend.memorizedCmd = iohcDevice::SEND_ASK_CHALLENGE_0x31;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
                packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, master_to, 3);

                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packet);
                break;
            }
            case DeviceButton::powerOn: {
                std::vector<uint8_t> toSend = {0x0C, 0x60, 0x01, 0x2C};

                auto* packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedData = toSend;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, master_to, 3);

                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packet);

                break;
            }
            case DeviceButton::setTemp: {
                std::vector<uint8_t> toSend = {0x0C, 0x61, 0x01, 0x03, 0xFF, 0x00};

                int temp = 10 * std::stof(data->at(1));
                toSend[4] = temp;

                int addr = 0;
                if (data->size() == 2) addr = 0;
                else addr = std::stoi(data->at(2));

                auto* packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedData = toSend;
                memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, addresses.at(addr).data(), 3);

                packet->delayed = 50;

                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packet);
                break;
            }
            case DeviceButton::setMode: {
                std::vector<uint8_t> toSend = {0x0C, 0x61, 0x01, 0x00, 0xFF};

                const char *dat = data->at(1).c_str();
                if (strcasecmp(dat, "auto") == 0) toSend[4] = 0x00;
                if (strcasecmp(dat, "manual") == 0) toSend[4] = 0x01;
                if (strcasecmp(dat, "prog") == 0) toSend[4] = 0x02;
                if (strcasecmp(dat, "off") == 0) toSend[4] = 0x04;

                size_t dest = 0;

                std::vector<iohcPacket *> packets2send;
                for (const auto &addr: addresses) {
                    auto* packet = new iohcPacket;
                    packets2send.push_back(packet);
                    forgePacket(packet, toSend);

                    packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                    memorizeSend.memorizedData = toSend;
                    memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                    packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
                    memcpy(packet->payload.packet.header.source, gateway, 3);
                    memcpy(packet->payload.packet.header.target, addresses.at(dest).data(), 3);

                    dest++;
                }
                packets2send[1]->delayed = 250;
                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);

                _radioInstance->send(packets2send);

                break;
            }
            case DeviceButton::setPresence: {
                std::vector<uint8_t> toSend = {0x0C, 0x61, 0x01, 0x10, 0xFF};

                const char *dat = data->at(1).c_str();
                if (strcasecmp(dat, "on") == 0) toSend[4] = 0x01;
                if (strcasecmp(dat, "off") == 0) toSend[4] = 0x00;

                auto* packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedData = toSend;
                memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, master_to, 3);

                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packet);
                break;
            }
            case DeviceButton::setWindow: {
                std::vector<uint8_t> toSend = {0x0C, 0x61, 0x01, 0x0E, 0xFF};

                const char *dat = data->at(1).c_str();
                if (strcasecmp(dat, "open") == 0) toSend[4] = 0x01;
                if (strcasecmp(dat, "close") == 0) toSend[4] = 0x00;

                int addr = 0;
                if (data->size() == 2) addr = 0;
                else addr = std::stoi(data->at(2));

                auto* packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedData = toSend;
                memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, addresses.at(addr).data(), 3);

                packet->delayed = 50;

                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packet);
                break;
            }
            case DeviceButton::midnight: {
                std::vector<uint8_t> toSend = {0x0c, 0x60, 0x01, 0x30};

                auto *packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedData = toSend;
                memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, master_to, 3);

                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packet);

                break;
            }

            default: break;
        } // switch (cmd)
        IOHC::packetStamp.store(esp_timer_get_time());
    }

    /**
    * @brief Load Cozy 2W settings from file.
    */
    bool iohcCozyDevice2W::load() {
        _radioInstance = iohcRadio::getInstance();

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s%s", FS_BASE, COZY_2W_FILE);

        struct stat st{};
        if (stat(full_path, &st) != 0) {
            ESP_LOGW(COZY_TAG, "*2W Cozy devices not available");
            return false;
        }
        ESP_LOGI(COZY_TAG, "Loading Cozy 2W devices settings from %s", full_path);

        FILE *f = fopen(full_path, "r");
        if (!f) {
            ESP_LOGE(COZY_TAG, "Failed to open %s", full_path);
            return false;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, f);
        fclose(f);

        if (error) {
            ESP_LOGE(COZY_TAG, "Failed to parse JSON: %s", error.c_str());
            return false;
        }

        for (JsonPair kv: doc.as<JsonObject>()) {
            device d;
            hexStringToBytes(kv.key().c_str(), d._node);
            auto jobj = kv.value().as<JsonObject>();
            d._type = jobj["type"].as<std::string>();
            d._description = jobj["description"].as<std::string>();
            hexStringToBytes(jobj["dst"].as<const char *>(), d._dst);
            devices.push_back(d);
        }
        ESP_LOGI(COZY_TAG, "Loaded %d x 2W devices", devices.size());

        return true;
    }

    bool iohcCozyDevice2W::save() {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s%s", FS_BASE, COZY_2W_FILE);

        FILE *f = fopen(full_path, "w");
        if (!f) {
            ESP_LOGE(COZY_TAG, "Failed to open %s for writing", full_path);
            return false;
        }

        JsonDocument doc;

        for (const auto &d: devices) {
            std::string key = bytesToHexString(d._node, sizeof(d._node));
            JsonObject jobj = doc[key.c_str()].to<JsonObject>();
            jobj["dst"] = bytesToHexString(d._dst, sizeof d._dst);
            jobj["type"] = d._type;
            jobj["description"] = d._description;
        }
        serializeJsonPretty(doc, f);
        fclose(f);

        return true;
    }

    void iohcCozyDevice2W::initializeValid() {
        size_t validKey = 0;
        auto valid = std::vector<uint8_t>(255);
        std::iota(valid.begin(), valid.end(), 0);

        valid = {
            0x00, 0x01, 0x03, 0x0a, 0x0c, 0x19, 0x1e, 0x20, 0x23, 0x28, 0x2a, 0x2c, 0x2e, 0x31, 0x32, 0x36, 0x38, 0x39,
            0x3c, 0x46, 0x48, 0x4a, 0x4b,
            0x50, 0x52, 0x54, 0x56, 0x60, 0x64, 0x6e, 0x6f, 0x71, 0x73, 0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8b, 0x8e,
            0x90, 0x92, 0x94, 0x96, 0x98,
            0x02, 0x0b, 0x0e, 0x14, 0x16, 0x25, 0x30, 0x34, 0x3a, 0x3d, 0x58
        };

        for (int key: valid) {
            mapValid[key] = 0;
        }
    }
}
