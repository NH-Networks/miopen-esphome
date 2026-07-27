#include "iohcRemote1W.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "iohcCryptoHelpers.h"
#include <esp_system.h>
#include "oled_display.h"
#include "TickerUsESP32.h"
#include "nvs_helpers.h"
#include <cmath>
#include <algorithm>

namespace IOHC {
    iohcRemote1W* iohcRemote1W::_iohcRemote1W = nullptr;
    static constexpr uint32_t DEFAULT_TRAVEL_TIME_SEC = 10;

    static void positionTaskLoop(void *arg) {
        auto *inst = static_cast<iohcRemote1W *>(arg);
        while (true) {
            inst->updatePositions();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    static const char *remoteButtonToString(RemoteButton cmd) {
        switch (cmd) {
            case RemoteButton::Open: return "OPEN";
            case RemoteButton::Close: return "CLOSE";
            case RemoteButton::Stop: return "STOP";
            case RemoteButton::Vent: return "VENT";
            case RemoteButton::ForceOpen: return "FORCE";
            case RemoteButton::Position: return "POSITION";
            case RemoteButton::Absolute: return "ABSOLUTE";
            case RemoteButton::Pair: return "PAIR";
            case RemoteButton::Add: return "ADD";
            case RemoteButton::Remove: return "REMOVE";
            case RemoteButton::Mode1: return "MODE1";
            case RemoteButton::Mode2: return "MODE2";
            case RemoteButton::Mode3: return "MODE3";
            case RemoteButton::Mode4: return "MODE4";
            default: return "UNKNOWN";
        }
    }

    iohcRemote1W::iohcRemote1W() = default;

    iohcRemote1W* iohcRemote1W::getInstance() {
        if (!_iohcRemote1W) {
            _iohcRemote1W = new iohcRemote1W();
            _iohcRemote1W->load();
            xTaskCreatePinnedToCore(positionTaskLoop, "positionTracker", 4096,
                                    _iohcRemote1W, 1, nullptr, 1);
        }
        return _iohcRemote1W;
    }

    void iohcRemote1W::forgePacket(iohcPacket* packet, uint16_t typn) {
        IOHC::relStamp = esp_timer_get_time();
        digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);

        packet->payload.packet.header.CtrlByte1.asStruct.MsgLen = sizeof(_header) - 1;
        packet->payload.packet.header.CtrlByte1.asStruct.Protocol = 1;
        packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
        packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 1;
        packet->payload.packet.header.CtrlByte2.asByte = 0;
        packet->payload.packet.header.CtrlByte2.asStruct.LPM = 1;

        uint16_t bcast = (typn << 6) + 0b111111; 
        packet->payload.packet.header.target[0] = 0x00;
        packet->payload.packet.header.target[1] = bcast >> 8;
        packet->payload.packet.header.target[2] = bcast & 0x00ff;

        packet->frequency = CHANNEL2;
        packet->repeatTime = 40;
        packet->repeat = 4;
        packet->lock = false;
    }

    std::vector<uint8_t> frame;

    void iohcRemote1W::cmd(RemoteButton cmd, Tokens* data) {
        if (data->size() == 1) {return; }
        std::string description = data->at(1).c_str();

        auto it = std::find_if( remotes.begin(), remotes.end(),  [&] ( const remote &r  ) {
                 return description == r.description;
              } );

        if (it == remotes.end()) {
            printf("ERROR %s NOT IN JSON", description.c_str());
            return;
        }

        remote& r = *it;
        r.positionTracker.update();

        switch (cmd) {
            case RemoteButton::Pair: {
                std::vector<iohcPacket *> packets2send;

                auto* packet = new iohcPacket;
                packets2send.push_back(packet);
                IOHC::iohcRemote1W::forgePacket(packet, r.type[0]);
                packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x2e);

                for (size_t i = 0; i < sizeof(address); i++)
                    packet->payload.packet.header.source[i] = r.node[i];

                packet->payload.packet.header.cmd = 0x2e;
                packet->payload.packet.msg.p0x2e.data = 0x00;
                packet->payload.packet.msg.p0x2e.sequence[0] = r.sequence >> 8;
                packet->payload.packet.msg.p0x2e.sequence[1] = r.sequence & 0x00ff;
                r.sequence += 1;
                nvs_write_sequence(r.node, r.sequence);

                frame = std::vector(&packet->payload.packet.header.cmd, &packet->payload.packet.header.cmd + 2);
                uint8_t hmac[16];
                iohcCrypto::create_1W_hmac(hmac, packet->payload.packet.msg.p0x2e.sequence, r.key, frame);

                for (uint8_t i = 0; i < 6; i++)
                    packet->payload.packet.msg.p0x2e.hmac[i] = hmac[i];

                packet->buffer_length = packet->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1;

                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
                _radioInstance->send(packets2send);
                display1WAction(r.node, remoteButtonToString(cmd), "TX", r.name.c_str());

                display1WPosition(r.node, r.positionTracker.getPosition(), r.name.c_str());

                r.paired = true;
                break;
            }

            case RemoteButton::Remove: {
                std::vector<iohcPacket *> packets2send;

                auto* packet = new iohcPacket;
                packets2send.push_back(packet);
                IOHC::iohcRemote1W::forgePacket(packet, r.type[0]);
                packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x2e);

                for (size_t i = 0; i < sizeof(address); i++)
                    packet->payload.packet.header.source[i] = r.node[i];

                packet->payload.packet.header.cmd = 0x39;
                packet->payload.packet.msg.p0x2e.data = 0x00;
                packet->payload.packet.msg.p0x2e.sequence[0] = r.sequence >> 8;
                packet->payload.packet.msg.p0x2e.sequence[1] = r.sequence & 0x00ff;
                r.sequence += 1;
                nvs_write_sequence(r.node, r.sequence);

                uint8_t hmac[16];
                frame = std::vector(&packet->payload.packet.header.cmd, &packet->payload.packet.header.cmd + 2);
                iohcCrypto::create_1W_hmac(hmac, packet->payload.packet.msg.p0x2e.sequence, r.key, frame);
                for (uint8_t i = 0; i < 6; i++)
                    packet->payload.packet.msg.p0x2e.hmac[i] = hmac[i];

                packet->buffer_length = packet->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1;

                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
                _radioInstance->send(packets2send);
                display1WAction(r.node, remoteButtonToString(cmd), "TX", r.name.c_str());

                display1WPosition(r.node, r.positionTracker.getPosition(), r.name.c_str());

                r.paired = false;
                break;
            }

            case RemoteButton::Add: {
                std::vector<iohcPacket *> packets2send;

                auto* packet = new iohcPacket;
                packets2send.push_back(packet);
                IOHC::iohcRemote1W::forgePacket(packet, r.type[0]);
                packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x30);

                for (size_t i = 0; i < sizeof(address); i++)
                    packet->payload.packet.header.source[i] = r.node[i];
                packet->payload.packet.header.cmd = 0x30;

                uint8_t encKey[16];
                memcpy(encKey, r.key, 16);
                iohcCrypto::encrypt_1W_key(r.node, encKey);
                memcpy(packet->payload.packet.msg.p0x30.enc_key, encKey, 16);

                packet->payload.packet.msg.p0x30.man_id = r.manufacturer;
                packet->payload.packet.msg.p0x30.data = 0x01;
                packet->payload.packet.msg.p0x30.sequence[0] = r.sequence >> 8;
                packet->payload.packet.msg.p0x30.sequence[1] = r.sequence & 0x00ff;
                r.sequence += 1;
                nvs_write_sequence(r.node, r.sequence);

                packet->buffer_length = packet->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1;

                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
                _radioInstance->send(packets2send);
                display1WAction(r.node, remoteButtonToString(cmd), "TX", r.name.c_str());
                display1WPosition(r.node, r.positionTracker.getPosition(), r.name.c_str());
                r.paired = true;
                break;
            }
            default: {
                std::vector<iohcPacket *> packets2send;

                auto* packet = new iohcPacket;
                packets2send.push_back(packet);

                IOHC::iohcRemote1W::forgePacket(packet, r.type[0]);
                for (size_t i = 0; i < sizeof(address); i++)
                    packet->payload.packet.header.source[i] = r.node[i];
                packet->payload.packet.header.cmd = 0x00;
                packet->payload.packet.msg.p0x00_14.origin = 0x01;
                setAcei(packet->payload.packet.msg.p0x00_14.acei, 0x43);
                switch (cmd) {
                    case RemoteButton::Open:
                        packet->payload.packet.msg.p0x00_14.main[0] = 0x00;
                        packet->payload.packet.msg.p0x00_14.main[1] = 0x00;
                        r.positionTracker.startOpening();
                        r.movement = remote::Movement::Opening;
                        r.targetPosition = 100.0f;
                        break;
                    case RemoteButton::Close:
                        packet->payload.packet.msg.p0x00_14.main[0] = 0xc8;
                        packet->payload.packet.msg.p0x00_14.main[1] = 0x00;
                        r.positionTracker.startClosing();
                        r.movement = remote::Movement::Closing;
                        r.targetPosition = 0.0f;
                        break;
                    case RemoteButton::Stop:
                        packet->payload.packet.msg.p0x00_14.main[0] = 0xd2;
                        packet->payload.packet.msg.p0x00_14.main[1] = 0x00;
                        r.positionTracker.stop();
                        r.movement = remote::Movement::Idle;
                        r.targetPosition = r.positionTracker.getPosition();
                        break;
                    case RemoteButton::Vent:
                        packet->payload.packet.msg.p0x00_14.main[0] = 0xd8;
                        packet->payload.packet.msg.p0x00_14.main[1] = 0x03;
                        break;
                    case RemoteButton::ForceOpen:
                        packet->payload.packet.msg.p0x00_14.main[0] = 0x64;
                        packet->payload.packet.msg.p0x00_14.main[1] = 0x00;
                        break;
                    case RemoteButton::Position: {
                        int index = (data->size() > 2) ? 2 : 0;
                        int percent = atoi(data->at(index).c_str());
                        percent = std::clamp(percent, 0, 100);
                        uint8_t val = static_cast<uint8_t>((100 - percent) * 2);
                        packet->payload.packet.msg.p0x00_14.main[0] = val;
                        packet->payload.packet.msg.p0x00_14.main[1] = 0x00;
                        float current = r.positionTracker.getPosition();
                        if (percent > current + 0.5f) {
                            r.positionTracker.startOpening();
                            r.movement = remote::Movement::Opening;
                        } else if (percent < current - 0.5f) {
                            r.positionTracker.startClosing();
                            r.movement = remote::Movement::Closing;
                        } else {
                            r.positionTracker.stop();
                            r.movement = remote::Movement::Idle;
                        }
                        r.targetPosition = percent;
                        break;
                    }
                    case RemoteButton::Absolute: {
                        int index = (data->size() > 2) ? 2 : 0;
                        int percent = atoi(data->at(index).c_str());
                        percent = std::clamp(percent, 0, 100);
                        uint16_t val = static_cast<uint16_t>(percent * 0x0200);
                        packet->payload.packet.msg.p0x00_14.main[0] = val >> 8;
                        packet->payload.packet.msg.p0x00_14.main[1] = val & 0xFF;
                        float targetPos = 100.0f - percent;
                        float current = r.positionTracker.getPosition();
                        if (targetPos > current + 0.5f) {
                            r.positionTracker.startOpening();
                            r.movement = remote::Movement::Opening;
                        } else if (targetPos < current - 0.5f) {
                            r.positionTracker.startClosing();
                            r.movement = remote::Movement::Closing;
                        } else {
                            r.positionTracker.stop();
                            r.movement = remote::Movement::Idle;
                        }
                        r.targetPosition = targetPos;
                        break;
                    }
                    case RemoteButton::Mode1: {
                        packet->payload.packet.header.cmd = 0x01;
                        packet->payload.packet.msg.p0x01_13.main = 0x00;
                        packet->payload.packet.msg.p0x01_13.fp1 = 0x01;
                        packet->payload.packet.msg.p0x01_13.fp2 = r.sequence & 0xFF;
                        break;
                    }
                    case RemoteButton::Mode2: {
                        packet->payload.packet.header.cmd = 0x01;
                        packet->payload.packet.msg.p0x01_13.main = 0x00;
                        packet->payload.packet.msg.p0x01_13.fp1 = 0x02;
                        packet->payload.packet.msg.p0x01_13.fp2 = r.sequence & 0xFF;
                        break;
                    }
                    case RemoteButton::Mode3: {
                        break;
                    }
                    case RemoteButton::Mode4: {
                        packet->payload.packet.header.cmd = 0x00;
                        packet->payload.packet.msg.p0x00_16.main[0] = 0xd2;
                        packet->payload.packet.msg.p0x00_16.main[1] = 0x00;
                        packet->payload.packet.msg.p0x00_16.fp1 = 0x20;
                        packet->payload.packet.msg.p0x00_16.fp2 = 0xCD;
                        packet->payload.packet.msg.p0x00_16.data[0] = 0x2E;
                        packet->payload.packet.msg.p0x00_16.data[1] = 0x00;
                        if (packet->payload.packet.header.source[2] == 0x1B) {
                            packet->payload.packet.msg.p0x00_16.fp2 = 0xCC;
                            packet->payload.packet.msg.p0x00_16.data[0] = 0xA2;
                        }
                        break;
                    }
                    default:
                        return;
                }

                uint8_t hmac[16];
                if (r.type[0] == 0 && (cmd == RemoteButton::Mode1 || cmd == RemoteButton::Mode2)) {
                    packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x01_13);
                    packet->payload.packet.msg.p0x01_13.sequence[0] = r.sequence >> 8;
                    packet->payload.packet.msg.p0x01_13.sequence[1] = r.sequence & 0x00ff;
                    uint8_t toAdd = 5 + 1;
                    frame = std::vector(&packet->payload.packet.header.cmd, &packet->payload.packet.header.cmd + toAdd);
                    iohcCrypto::create_1W_hmac(hmac, packet->payload.packet.msg.p0x01_13.sequence, r.key, frame);
                    for (uint8_t i = 0; i < 6; i++) {
                        packet->payload.packet.msg.p0x01_13.hmac[i] = hmac[i];
                    }
                } else if (r.type[0] == 0 && (cmd == RemoteButton::Mode4)) {
                    packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x00_16);
                    packet->payload.packet.msg.p0x00_16.sequence[0] = r.sequence >> 8;
                    packet->payload.packet.msg.p0x00_16.sequence[1] = r.sequence & 0x00ff;
                    uint8_t toAdd = 8 + 1;
                    frame = std::vector(&packet->payload.packet.header.cmd, &packet->payload.packet.header.cmd + toAdd);
                    iohcCrypto::create_1W_hmac(hmac, packet->payload.packet.msg.p0x00_16.sequence, r.key, frame);
                    for (uint8_t i = 0; i < 6; i++) {
                        packet->payload.packet.msg.p0x00_16.hmac[i] = hmac[i];
                    }
                } else {
                    packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x00_14);
                    packet->payload.packet.msg.p0x00_14.sequence[0] = r.sequence >> 8;
                    packet->payload.packet.msg.p0x00_14.sequence[1] = r.sequence & 0x00ff;
                    uint8_t toAdd = 6 + 1;
                    frame = std::vector(&packet->payload.packet.header.cmd, &packet->payload.packet.header.cmd + toAdd);
                    iohcCrypto::create_1W_hmac(hmac, packet->payload.packet.msg.p0x00_14.sequence, r.key, frame);
                    for (uint8_t i = 0; i < 6; i++) {
                        packet->payload.packet.msg.p0x00_14.hmac[i] = hmac[i];
                    }
                }

                r.sequence += 1;
                nvs_write_sequence(r.node, r.sequence);
                packet->buffer_length = packet->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1;

                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
                _radioInstance->send(packets2send);

                display1WAction(r.node, remoteButtonToString(cmd), "TX", r.name.c_str());
                display1WPosition(r.node, r.positionTracker.getPosition(), r.name.c_str());
                break;
            }
        }
        this->save();
    }

    bool iohcRemote1W::load() {
        _radioInstance = iohcRadio::getInstance();

        if (!LittleFS.exists(IOHC_1W_REMOTE)) return false;

        fs::File f = LittleFS.open(IOHC_1W_REMOTE, "r");
        JsonDocument doc; 

        DeserializationError error = deserializeJson(doc, f); 
        if (error) {
            f.close();
            return false;
        }
        f.close();

        bool updateFile = false;
        std::vector<remote> loadedRemotes;
        for (JsonPair kv: doc.as<JsonObject>()) {
            remote r;
            hexStringToBytes(kv.key().c_str(), r.node);

            auto jobj = kv.value().as<JsonObject>();
            hexStringToBytes(jobj["key"].as<const char *>(), r.key);

            uint8_t btmp[2];
            hexStringToBytes(jobj["sequence"].as<const char *>(), btmp);
            uint16_t file_seq = (btmp[0] << 8) + btmp[1];
            r.sequence = file_seq;

            uint16_t nvs_seq;
            if (nvs_read_sequence(r.node, &nvs_seq)) {
                if (nvs_seq > r.sequence) {
                    r.sequence = nvs_seq;
                    updateFile = true;
                }
            }
            nvs_write_sequence(r.node, r.sequence);
            JsonArray jarr = jobj["type"];
            r.type.reserve(jarr.size());

            for (auto && i : jarr) {
                r.type.push_back(i.as<uint8_t>());
            }

            r.manufacturer = jobj["manufacturer_id"].as<uint8_t>();
            r.description = jobj["description"].as<std::string>();

            if (jobj["name"].is<std::string>()) {
                r.name = jobj["name"].as<std::string>();
            } else {
                r.name = r.description;
                updateFile = true;
            }

            if (jobj["travel_time"].is<uint32_t>()) {
                r.travelTime = jobj["travel_time"].as<uint32_t>();
            } else {
                r.travelTime = DEFAULT_TRAVEL_TIME_SEC;
                updateFile = true;
            }
            if (jobj["paired"].is<bool>()) {
                r.paired = jobj["paired"].as<bool>();
            } else {
                r.paired = false;
                updateFile = true;
            }
            if (jobj["repeatOnNoResponse"].is<bool>()) {
                r.repeatOnNoResponse = jobj["repeatOnNoResponse"].as<bool>();
            } else {
                r.repeatOnNoResponse = false;
            }
            r.positionTracker.setTravelTime(r.travelTime);
            if (jobj["position"].is<float>() || jobj["position"].is<int>()) {
                r.positionTracker.setPosition(
                    std::clamp(jobj["position"].as<float>(), 0.0f, 100.0f));
            } else {
                r.positionTracker.setPosition(0.0f);
                updateFile = true;
            }
            r.lastPublishedPosition = r.positionTracker.getPosition();
            r.lastSavedPosition = r.positionTracker.getPosition();

            loadedRemotes.push_back(r);
        }

        remotes = loadedRemotes;
        if (updateFile) {
            this->save();
        }
        return true;
    }

    bool iohcRemote1W::save() {
        if (remotes.empty()) {
            return false;
        }

        constexpr const char *tempFile = "/1W.json.tmp";
        constexpr const char *backupFile = "/1W.json.bak";
        LittleFS.remove(tempFile);
        fs::File f = LittleFS.open(tempFile, "w");
        if (!f) {
            return false;
        }
        JsonDocument doc;
        for (const auto&r: remotes) {
            auto jobj = doc[bytesToHexString(r.node, sizeof(r.node))].to<JsonObject>();
            jobj["key"] = bytesToHexString(r.key, sizeof(r.key));

            uint8_t btmp[2];
            btmp[1] = r.sequence & 0x00ff;
            btmp[0] = r.sequence >> 8;

            jobj["sequence"] = bytesToHexString(btmp, sizeof(btmp));
            
            auto jarr = jobj["type"].to<JsonArray>();
            for (uint8_t i : r.type) {
                jarr.add(i);
            }

            jobj["manufacturer_id"] = r.manufacturer;
            jobj["description"] = r.description;
            jobj["name"] = r.name;

            jobj["travel_time"] = r.travelTime;
            jobj["position"] = static_cast<int>(std::round(r.positionTracker.getPosition()));
            jobj["paired"] = r.paired;
            jobj["repeatOnNoResponse"] = r.repeatOnNoResponse;
        }
        const size_t written = serializeJson(doc, f);
        f.flush();
        f.close();
        if (written == 0) {
            LittleFS.remove(tempFile);
            return false;
        }

        LittleFS.remove(backupFile);
        if (LittleFS.exists(IOHC_1W_REMOTE) &&
            !LittleFS.rename(IOHC_1W_REMOTE, backupFile)) {
            LittleFS.remove(tempFile);
            return false;
        }
        if (!LittleFS.rename(tempFile, IOHC_1W_REMOTE)) {
            LittleFS.remove(tempFile);
            if (LittleFS.exists(backupFile)) {
                LittleFS.rename(backupFile, IOHC_1W_REMOTE);
            }
            return false;
        }
        LittleFS.remove(backupFile);

        return true;
    }

    const std::vector<iohcRemote1W::remote>& iohcRemote1W::getRemotes() const {
        return remotes;
    }

    bool iohcRemote1W::addRemote(const std::string &name) {
        remote r{};

        bool unique = false;
        while (!unique) {
            for (uint8_t i = 0; i < sizeof(r.node); i++)
                r.node[i] = esp_random() & 0xff;
            unique = std::none_of(remotes.begin(), remotes.end(), [&](const remote &e){
                return memcmp(e.node, r.node, sizeof(r.node)) == 0;
            });
        }

        for (uint8_t &b : r.key)
            b = esp_random() & 0xff;

        r.sequence = 1;
        r.type = {0, 0};
        r.manufacturer = 2;
        r.name = name;
        r.travelTime = DEFAULT_TRAVEL_TIME_SEC;
        r.paired = false;
        r.repeatOnNoResponse = false;

        const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::string desc;
        do {
            desc.clear();
            for (int i = 0; i < 4; ++i)
                desc.push_back(letters[esp_random() % 26]);
        } while (std::any_of(remotes.begin(), remotes.end(), [&](const remote &e){
            return e.description == desc;
        }));
        r.description = desc;

        r.positionTracker.setTravelTime(r.travelTime);
        remotes.push_back(r);
        nvs_write_sequence(r.node, r.sequence);
        save();
        return true;
    }

    bool iohcRemote1W::removeRemote(const std::string &description) {
        auto it = std::find_if(remotes.begin(), remotes.end(), [&](const remote &e) {
            return e.description == description;
        });
        if (it == remotes.end()) {
            return false;
        }
        if (it->paired) {
            return false;
        }
        remotes.erase(it);
        save();
        return true;
    }

    bool iohcRemote1W::renameRemote(const std::string &description, const std::string &name) {
        auto it = std::find_if(remotes.begin(), remotes.end(), [&](const remote &e) {
            return e.description == description;
        });
        if (it == remotes.end()) {
            return false;
        }
        it->name = name;
        save();
        return true;
    }

    void iohcRemote1W::handleRemoteAction(RemoteButton cmd, const std::string &description) {
        auto it = std::find_if(remotes.begin(), remotes.end(), [&](const remote &e) {
            return e.description == description;
        });
        if (it == remotes.end()) {
            return;
        }
        remote &r = *it;
        r.positionTracker.update();

        switch (cmd) {
            case RemoteButton::Open:
                r.positionTracker.startOpening();
                r.movement = remote::Movement::Opening;
                r.targetPosition = 100.0f;
                break;
            case RemoteButton::Close:
                r.positionTracker.startClosing();
                r.movement = remote::Movement::Closing;
                r.targetPosition = 0.0f;
                break;
            case RemoteButton::Stop:
                r.positionTracker.stop();
                r.movement = remote::Movement::Idle;
                r.targetPosition = r.positionTracker.getPosition();
                break;
            default:
                break;
        }
    }

    bool iohcRemote1W::setTravelTime(const std::string &description, uint32_t travelTime) {
        auto it = std::find_if(remotes.begin(), remotes.end(), [&](const remote &e) {
            return e.description == description;
        });
        if (it == remotes.end()) {
            return false;
        }
        it->travelTime = travelTime;
        it->positionTracker.setTravelTime(travelTime);
        save();
        return true;
    }

    bool iohcRemote1W::setRepeatOnNoResponse(const std::string &description, bool repeatOnNoResponse) {
        auto it = std::find_if(remotes.begin(), remotes.end(), [&](const remote &e) {
            return e.description == description;
        });
        if (it == remotes.end()) {
            return false;
        }
        it->repeatOnNoResponse = repeatOnNoResponse;
        save();
        return true;
    }

    void iohcRemote1W::updatePositions() {
        for (auto &r : remotes) {
            r.positionTracker.update();

            float pos = r.positionTracker.getPosition();
            bool moving = r.positionTracker.isMoving();

            if (r.targetPosition >= 0.0f) {
                if (r.movement == remote::Movement::Opening && pos >= r.targetPosition) {
                    pos = r.targetPosition;
                    r.positionTracker.setPosition(pos);
                    r.positionTracker.stop();
                    moving = false;
                } else if (r.movement == remote::Movement::Closing && pos <= r.targetPosition) {
                    pos = r.targetPosition;
                    r.positionTracker.setPosition(pos);
                    r.positionTracker.stop();
                    moving = false;
                }
                if (!moving) {
                    r.targetPosition = -1.0f;
                }
            }

            if (moving) {
                display1WPosition(r.node, pos, r.name.c_str());
                r.lastPublishedPosition = pos;
            } else {
                if (r.lastPublishedPosition != pos) {
                    display1WPosition(r.node, pos, r.name.c_str());
                    r.lastPublishedPosition = pos;
                }
                r.movement = remote::Movement::Idle;
                if (fabs(pos - r.lastSavedPosition) >= 1.0f) {
                    save();
                    r.lastSavedPosition = pos;
                }
            }
        }
    }
}
