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

#include "iohcOtherDevice2W.h"
#include <ArduinoJson.h>
#include "iohcCryptoHelpers.h"
#include <string>
#include "iohcRadio.h"
#include <vector>
#include "iohcDevice.h"
#include <numeric>
#include <stdio.h>
#include <sys/stat.h>
#include "esp_log.h"

static const char *OTHER_TAG = "iohcOther2W";
static const char *FS_BASE_O = "/littlefs";

namespace IOHC {
    iohcOtherDevice2W *iohcOtherDevice2W::_iohcOtherDevice2W = nullptr;

    iohcOtherDevice2W::iohcOtherDevice2W() = default;

    iohcOtherDevice2W *iohcOtherDevice2W::getInstance() {
        if (!_iohcOtherDevice2W) {
            _iohcOtherDevice2W = new iohcOtherDevice2W();
            _iohcOtherDevice2W->load();
            _iohcOtherDevice2W->initializeValid();
        }
        return _iohcOtherDevice2W;
    }

    address fake_gateway = {0xba, 0x11, 0xad};

    void iohcOtherDevice2W::forgePacket(iohcPacket *packet, const std::vector<uint8_t> &toSend, size_t typn = 0) {
        gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
        IOHC::relStamp.store(esp_timer_get_time());

        packet->payload.packet.header.CtrlByte1.asStruct.MsgLen = sizeof(_header) - 1;
        packet->payload.packet.header.CtrlByte1.asStruct.Protocol = 0;
        packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
        packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 0;
        packet->payload.packet.header.cmd = 0x2A;
        packet->payload.packet.header.CtrlByte2.asByte = 0;
        packet->payload.packet.header.CtrlByte2.asStruct.Prio = 1;
        packet->payload.packet.header.CtrlByte2.asStruct.LPM = 0;

        u_int16_t bcast = typn;
        packet->payload.packet.header.target[0] = 0x00;
        packet->payload.packet.header.target[1] = bcast >> 8;
        packet->payload.packet.header.target[2] = bcast & 0x00ff;

        packet->payload.packet.header.CtrlByte1.asByte += toSend.size();
        memcpy(packet->payload.buffer + 9, toSend.data(), toSend.size());
        packet->buffer_length = toSend.size() + 9;

        packet->frequency = CHANNEL2;
        packet->repeatTime = 50;
        packet->repeat = 0;
        packet->lock = false;
    }

    void iohcOtherDevice2W::cmd(Other2WButton cmd, Tokens *data) {
        if (!_radioInstance) {
            ESP_LOGE(OTHER_TAG, "NO RADIO INSTANCE");
            _radioInstance = iohcRadio::getInstance();
        }

        switch (cmd) {
            case Other2WButton::discovery: {
                int bec = 0;

                std::vector<iohcPacket *> packets2send;
                for (int j = 0; j < 255; j++) {
                    auto *packet = new iohcPacket;
                    packets2send.push_back(packet);

                    std::string discovery = "d430477706ba11ad31";
                    std::vector<uint8_t> toSend = {};
                    packet->buffer_length = hexStringToBytes(
                        discovery, packet->payload.buffer);
                    forgePacket(packet, toSend, bec);
                    bec += 0x01;
                    packet->repeatTime = 250;
                }

                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packets2send);
                break;
            }
            case Other2WButton::getName: {
                std::vector<uint8_t> toSend = {};

                int value = std::stol(data->at(1).c_str(), nullptr, 16);
                address target;
                target[0] = static_cast<uint8_t>(value >> 16);
                target[1] = static_cast<uint8_t>(value >> 8);
                target[2] = static_cast<uint8_t>(value);

                auto *packet = new iohcPacket;
                forgePacket(packet, toSend, 0);
                packet->payload.packet.header.cmd = SEND_GET_NAME_0x50;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, fake_gateway, 3);
                memcpy(packet->payload.packet.header.target, target, 3);

                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packet);
                break;
            }
            case Other2WButton::custom: {
                std::vector<uint8_t> toSend = {0x01, 0x47, 0xc8, 0x00, 0x00, 0x00};
                std::vector<iohcPacket *> packets2send;
                for (int acei = 0; acei < 256; acei++) {
                    AceiUnion ACEI{};
                    ACEI.asByte = acei;
                    if (!ACEI.asStruct.isvalid || ACEI.asStruct.service != 0) continue;

                    toSend[1] = acei;

                    address from = {0x08, 0x42, 0xe3};
                    address to_1 = {0x05, 0x4e, 0x17};

                    auto *packet = new iohcPacket;
                    packets2send.push_back(packet);
                    forgePacket(packet, toSend);

                    packet->payload.packet.header.cmd = 0x00;
                    memorizeOther2W.memorizedData = toSend;
                    memorizeOther2W.memorizedCmd = 0x00;

                    packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                    packet->payload.packet.header.CtrlByte2.asStruct.LPM = 1;
                    packet->payload.packet.header.CtrlByte2.asStruct.Prio = 1;

                    memcpy(packet->payload.packet.header.source, from, 3);
                    memcpy(packet->payload.packet.header.target, to_1, 3);

                    packet->delayed = 250;
                }
                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packets2send);
                break;
            }
            case Other2WButton::custom60: {
                std::vector<uint8_t> toSend = {0x0C, 0x60, 0x01, 0xFF};
                const char *dat = data->at(1).c_str();

                int custom = std::stoi(data->at(1));
                toSend[3] = custom;

                auto *packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeOther2W.memorizedData = toSend;
                memorizeOther2W.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, master_to, 3);

                packet->delayed = 250;
                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packet);
                break;
            }
            case Other2WButton::discover28: {
                std::vector<uint8_t> toSend = {};

                address broadcast = {0x00, 0x00, 0x3B};
                std::vector<iohcPacket *> packets2send;
                for (size_t i = 0; i < 1; i++) {
                    auto *packet = new iohcPacket;
                    packets2send.push_back(packet);
                    forgePacket(packet, toSend);

                    packet->payload.packet.header.cmd = iohcDevice::SEND_DISCOVER_0x28;
                    memorizeOther2W.memorizedData = toSend;
                    memorizeOther2W.memorizedCmd = iohcDevice::SEND_DISCOVER_0x28;

                    packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
                    packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 1;
                    packet->payload.packet.header.CtrlByte2.asByte = 0;

                    memcpy(packet->payload.packet.header.source, gateway, 3);
                    memcpy(packet->payload.packet.header.target, broadcast, 3);

                    packet->repeat = 3;
                    packet->repeatTime = 75;
                }
                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packets2send);
                break;
            }
            case Other2WButton::discover2A: {
                address broadcast_3b = {0x00, 0x00, 0x3b};
                address broadcast_3f = {0x00, 0x00, 0x3f};

                std::vector<std::vector<uint8_t>> payloads = {
                    {0xa0, 0xb4, 0x38, 0xd2, 0x5f, 0x27, 0x28, 0x6f, 0xed, 0xd2, 0xad, 0x1f},
                    {0x93, 0x32, 0xd6, 0x18, 0xde, 0x2a, 0x0f, 0xa6, 0x25, 0x0e, 0x2c, 0x7e}
                };
                if (data && data->size() > 1) {
                    uint8_t customPayload[12] = {};
                    if (hexStringToBytes(data->at(1), customPayload) == sizeof(customPayload)) {
                        payloads.clear();
                        payloads.emplace_back(customPayload, customPayload + sizeof(customPayload));
                    } else {
                        ESP_LOGE(OTHER_TAG, "discover2A expects a 12-byte hex payload");
                        break;
                    }
                }
                const uint8_t *targets[] = {broadcast_3b, broadcast_3f};

                std::vector<iohcPacket *> packets2send;
                for (size_t i = 0; i < payloads.size() * 2; i++) {
                    auto *packet = new iohcPacket;
                    packets2send.push_back(packet);

                    const auto &toSend = payloads[i / 2];
                    forgePacket(packet, toSend);
                    packet->payload.packet.header.cmd = iohcDevice::SEND_DISCOVER_REMOTE_0x2A;
                    memcpy(packet->payload.packet.header.target, targets[i % 2], 3);

                    packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
                    packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 1;
                    packet->payload.packet.header.CtrlByte2.asStruct.LPM = 1;
                    packet->payload.packet.header.CtrlByte2.asStruct.Prio = 1;

                    memcpy(packet->payload.packet.header.source, gateway, 3);

                    packet->delayed = 250;
                    packet->repeatTime = 250;
                }
                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);

                _radioInstance->send(packets2send);

                break;
            }
            case Other2WButton::fake0: {
                std::vector<uint8_t> toSend = {0x03, 0xe7, 0x32, 0x00, 0x00, 0x00};

                address from = {0x08, 0x42, 0xe3};

                address guessed[15] = {
                    {0x2D, 0xBE, 0x8D}, {0xDA, 0x2E, 0xE6}, {0x31, 0x58, 0x24}, {0x20, 0xE5, 0x2E}, {0x14, 0xe0, 0x0e},
                    {0x05, 0x4E, 0x17}, {0x1C, 0x68, 0x58}, {0x90, 0x4c, 0x09}, {0xfe, 0x90, 0xee}, {0x41, 0x56, 0x84},
                    {0x08, 0x42, 0xe3},
                    {0x47, 0x77, 0x06}, {0x48, 0x79, 0x02}, {0x8C, 0xCB, 0x30}, {0x8C, 0xCB, 0x31}
                };

                size_t i = 0;
                std::vector<iohcPacket *> packets2send;
                for (i = 0; i < 15; i++) {
                    auto *packet = new iohcPacket;
                    packets2send.push_back(packet);
                    forgePacket(packet, toSend);

                    packet->payload.packet.header.cmd = 0x00;
                    memorizeOther2W.memorizedData = toSend;
                    memorizeOther2W.memorizedCmd = 0x00;
                    IOHC::lastSendCmd.store(0x00);

                    packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
                    packet->payload.packet.header.CtrlByte2.asStruct.LPM = 1;

                    memcpy(packet->payload.packet.header.source, from, 3);
                    memcpy(packet->payload.packet.header.target, guessed[i], 3);

                    packet->delayed = 250;
                    packet->repeatTime = 250;
                }
                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packets2send);

                break;
            }
            case Other2WButton::ack: {
                std::vector<uint8_t> toSend = {};

                auto *packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_KEY_TRANSFERT_ACK_0x33;
                memorizeOther2W.memorizedCmd = iohcDevice::SEND_KEY_TRANSFERT_ACK_0x33;
                memorizeOther2W.memorizedData = toSend;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, master_from, 3);

                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);
                _radioInstance->send(packet);
                break;
            }
            case Other2WButton::checkCmd: {
                std::vector<uint8_t> toSend;
                uint8_t special12[] = {
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
                    0x17, 0x18, 0x19, 0x20, 0x21
                };
                uint8_t specacei[] = {0x01, 0xe7, 0x00, 0x00, 0x00, 0x00};

                address from = {0x08, 0x42, 0xe3};

                uint8_t counter = 0;
                std::vector<iohcPacket *> packets2send;
                for (const auto &command: mapValid) {
                    if (command.second == 0 || (command.second == 5 && command.first != 0x19)) {
                        counter++;

                        if (command.first == 0x00 || command.first == 0x01 || command.first == 0x0B || command.first ==
                            0x0E || command.first == 0x23 || command.first == 0x2A || command.first == 0x1E)
                            toSend.assign(specacei, specacei + 6);
                        if (command.first == 0x8B || command.first == 0x19)
                            toSend.assign(special12, special12 + 1);
                        if (command.first == 0x04) toSend.assign(special12, special12 + 14);
                        uint8_t special03[] = {0x03, 0x00, 0x00};
                        if (command.first == 0x03 || command.first == 0x73)
                            toSend.assign(special03, special03 + 3);
                        uint8_t special0C[] = {0xD8, 0x00, 0x00, 0x00};
                        if (command.first == 0x0C)
                            toSend.assign(special0C, special0C + 4);
                        uint8_t special0D[] = {0x05, 0xaa, 0x0d, 0x00, 0x00};
                        if (command.first == 0x0D)
                            toSend.assign(special0D, special0D + 5);
                        if (command.first == 0x64 || command.first == 0x14)
                            toSend.assign(special12, special12 + 2);
                        if (command.first == 0x2A || command.first == 0x96)
                            toSend.assign(special12, special12 + 12);
                        if (command.first == 0x38 || command.first == 0x3C || command.first == 0x3D)
                            toSend.assign(special12, special12 + 6);
                        if (command.first == 0x32 || command.first == 0x52 || command.first == 0x92)
                            toSend.assign(special12, special12 + 16);
                        if (command.first == 0x46 || command.first == 0x48 || command.first == 0x6E || command.first == 0x6F)
                            toSend.assign(special12, special12 + 9);
                        if (command.first == 0x4A || command.first == 0x8A)
                            toSend.assign(special12, special12 + 18);
                        if (command.first == 0x60 || command.first == 0x82)
                            toSend.assign(special12, special12 + 21);

                        auto *packet = new iohcPacket;
                        packets2send.push_back(packet);
                        forgePacket(packet, toSend);
                        packet->payload.packet.header.cmd = command.first;
                        memorizeOther2W.memorizedCmd = packet->payload.packet.header.cmd;

                        packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
                        packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 0;
                        packet->payload.packet.header.CtrlByte2.asStruct.LPM = 1;
                        packet->payload.packet.header.CtrlByte2.asStruct.Prio = 1;

                        memcpy(packet->payload.packet.header.source, gateway, 3);
                        memcpy(packet->payload.packet.header.target, master_to, 3);

                        packet->repeatTime = 250;
                        packet->delayed = 250;
                    }
                    toSend.clear();
                }
                printf("valid %u\n", counter);
                gpio_set_level((gpio_num_t) RX_LED, gpio_get_level((gpio_num_t) RX_LED) ^ 1);

                _radioInstance->send(packets2send);

                break;
            }
            default: break;
        }
    }

    void iohcOtherDevice2W::initializeValid() {
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

    void iohcOtherDevice2W::scanDump() {
        printf("*********************** Scan result ***********************\n");

        uint8_t count = 0;

        for (auto &it: mapValid) {
            if (it.second != 0x08) {
                if (it.second == 0x3C)
                    printf("%2.2x=AUTH ", it.first, it.second);
                else if (it.second == 0x80)
                    printf("%2.2x=NRDY ", it.first, it.second);
                else
                    printf("%2.2x=%2.2x\t", it.first, it.second);
                count++;
                if (count % 16 == 0) printf("\n");
            }
        }

        if (count % 16 != 0) printf("\n");

        printf("%u toCheck \n", count);
    }

    bool iohcOtherDevice2W::load() {
        _radioInstance = iohcRadio::getInstance();

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s%s", FS_BASE_O, OTHER_2W_FILE);

        struct stat st{};
        if (stat(full_path, &st) != 0) {
            ESP_LOGW(OTHER_TAG, "*2W Other devices not available");
            return false;
        }
        ESP_LOGI(OTHER_TAG, "Loading Other 2W devices settings from %s", full_path);

        FILE *f = fopen(full_path, "r");
        if (!f) {
            ESP_LOGE(OTHER_TAG, "Failed to open %s", full_path);
            return false;
        }

        JsonDocument doc;
        deserializeJson(doc, f);
        fclose(f);

        for (JsonPair kv: doc.as<JsonObject>()) {
            hexStringToBytes(kv.key().c_str(), _node);
            auto jobj = kv.value().as<JsonObject>();
            hexStringToBytes(jobj["dst"].as<const char *>(), _dst);
        }

        return true;
    }

    bool iohcOtherDevice2W::save() {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s%s", FS_BASE_O, OTHER_2W_FILE);

        FILE *f = fopen(full_path, "w");
        if (!f) {
            ESP_LOGE(OTHER_TAG, "Failed to open %s for writing", full_path);
            return false;
        }

        JsonDocument doc;

        std::string key = bytesToHexString(_node, sizeof(_node));
        JsonObject jobj = doc[key.c_str()].to<JsonObject>();
        jobj["dst"] = _dst;

        serializeJsonPretty(doc, f);
        fclose(f);

        return true;
    }
}
