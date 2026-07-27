#include "iohc_gateway.h"
#include "esphome/core/log.h"
#include <Arduino.h>
#include <LittleFS.h>

// Protocol core (upstream)
#include "iohcRadio.h"
#include "iohcRemote1W.h"
#include "iohcDevice.h"
#include "nvs_helpers.h"
#include "fileSystemHelpers.h"
#include "iohcCryptoHelpers.h"  // bytesToHexString, IOHC::lastFromAddress
#include "tokens.h"
#include "iohcRemoteMap.h"

static const char* TAG = "iohc_gateway";

namespace iohomecontrol {

IohcGateway* IohcGateway::instance_ = nullptr;

IohcGateway::IohcGateway()  { instance_ = this; }
IohcGateway::~IohcGateway() {
    if (radio_)  { delete radio_;  radio_  = nullptr; }
    if (remote_) { delete remote_; remote_ = nullptr; }
    if (instance_ == this) instance_ = nullptr;
}

// ---------------------------------------------------------------------------
// ISR — enqueues alleen een statische token; alle verwerking in loop()
// GEEN millis(), GEEN stack-allocatie, GEEN heap-gebruik in ISR
// ---------------------------------------------------------------------------
void IRAM_ATTR IohcGateway::isr_dio0_handler() {
    if (instance_) instance_->enqueue_rx_irq();
}

void IRAM_ATTR IohcGateway::enqueue_rx_irq() {
    // Gebruik static sentinel — geen stack-allocatie in ISR
    // timestamp_ms wordt ingevuld in loop() na het poppen
    GatewayEvent ev = rx_sentinel_event();
    event_queue_.push(ev);
}

// ---------------------------------------------------------------------------
// begin() — aangeroepen vanuit IohcGatewayComponent::setup()
// ---------------------------------------------------------------------------
bool IohcGateway::begin() {
    ESP_LOGI(TAG, "Initialising IOHC gateway (SX1276, %u Hz)", frequency_);

    if (!init_radio()) {
        last_error_ = "SX1276 init failed";
        ESP_LOGE(TAG, "%s", last_error_.c_str());
        return false;
    }

    if (!load_devices())
        ESP_LOGW(TAG, "No devices in %s — pair new screens via scan+add",
                 devices_file_.c_str());

    if (dio0_pin_ >= 0) {
        pinMode(dio0_pin_, INPUT);
        attachInterrupt(digitalPinToInterrupt(dio0_pin_),
                        IohcGateway::isr_dio0_handler, RISING);
        ESP_LOGI(TAG, "DIO0 IRQ attached on GPIO%d", dio0_pin_);
    }

    radio_ok_ = true;
    return true;
}

bool IohcGateway::init_radio() {
    if (!radio_) radio_ = new iohcRadio();
    return radio_->init(nss_pin_, reset_pin_,
                        sck_pin_, miso_pin_, mosi_pin_, frequency_);
}

bool IohcGateway::load_devices() {
    // Wis de bestaande remote bij reload om duplicaten te voorkomen
    if (remote_) {
        delete remote_;
        remote_ = nullptr;
    }
    remote_ = new iohcRemote1W(radio_);
    bool ok = remote_->load();
    if (ok && remote_) {
        for (const auto& r : remote_->getRemotes()) {
            GatewayEvent ev{};
            ev.type = EventType::DEVICE_ADDED;
            std::string id = bytesToHexString(r.node, sizeof(r.node));
            snprintf(ev.device_discovered.device_id, sizeof(ev.device_discovered.device_id), "%s", id.c_str());
            snprintf(ev.device_discovered.name, sizeof(ev.device_discovered.name), "%s", r.name.c_str());
            dispatch_event(ev);
        }
    }

    // Load remotes map from ESPHome YAML config
    for (const auto& r : remote_maps_) {
        // Convert name to a fake node address or generate one if needed?
        // Wait, the original `iohcRemoteMap` expects a node address to add it.
        // We can just use the first 3 chars of the name, or hash the name to a 3-byte address.
        address node = {0, 0, 0};
        for (size_t i = 0; i < r.name.length() && i < 3; i++) {
            node[i] = r.name[i];
        }
        
        iohcRemoteMap* rmap = iohcRemoteMap::getInstance();
        rmap->add(node, r.name);
        for (const auto& d : r.devices) {
            rmap->linkDevice(node, d);
        }
    }

    return ok;
}

void IohcGateway::add_remote_map(const std::string& name, const std::vector<std::string>& devices) {
    RemoteMapEntry entry;
    entry.name = name;
    entry.devices = devices;
    remote_maps_.push_back(entry);
}

// ---------------------------------------------------------------------------
// loop() — drain event queue, tick protocol core, scan timeout
// ---------------------------------------------------------------------------
void IohcGateway::loop() {
    // Scan mode auto-reset na 60 seconden
    if (scan_mode_ && (millis() - scan_start_ms_ >= 60000)) {
        scan_mode_ = false;
        ESP_LOGI(TAG, "Scan mode timeout — terug naar normaal");
    }

    GatewayEvent ev;
    while (event_queue_.pop(ev)) {
        if (ev.type == EventType::FRAME_RECEIVED && remote_) {
            // RSSI VOOR processRx() lezen — register wordt overschreven na verwerking
            last_rssi_ = radio_ ? radio_->getRSSI() : 0;
            ev.frame.rssi      = last_rssi_;
            ev.frame.timestamp_ms = millis();  // timestamp hier, niet in ISR

            remote_->processRx();
            rx_count_++;

            // Laatste gehoord adres bijwerken vanuit upstream global
            last_addr_ = bytesToHexString(IOHC::lastFromAddress,
                                          sizeof(IOHC::lastFromAddress));
        }
        dispatch_event(ev);
    }

    if (remote_) {
        remote_->tick();
        remote_->updatePositions();

        // Stuur positie- en statusupdates naar ESPHome Covers
        for (const auto& r : remote_->getRemotes()) {
            float pos = r.positionTracker.getPosition() / 100.0f; // 0.0 tot 1.0
            bool moving = r.positionTracker.isMoving();
            std::string id = bytesToHexString(r.node, sizeof(r.node));

            CoverState state = CoverState::UNKNOWN;
            if (moving) {
                if (r.movement == IOHC::iohcRemote1W::remote::Movement::Opening)
                    state = CoverState::OPENING;
                else if (r.movement == IOHC::iohcRemote1W::remote::Movement::Closing)
                    state = CoverState::CLOSING;
            } else {
                if (pos >= 0.995f)
                    state = CoverState::OPEN;
                else if (pos <= 0.005f)
                    state = CoverState::CLOSED;
                else
                    state = CoverState::STOPPED;
            }

            auto pos_it = last_pub_pos_.find(id);
            auto state_it = last_pub_state_.find(id);
            bool pos_changed = (pos_it == last_pub_pos_.end() || fabsf(pos - pos_it->second) >= 0.005f);
            bool state_changed = (state_it == last_pub_state_.end() || state != state_it->second);

            if (pos_changed || state_changed) {
                last_pub_pos_[id] = pos;
                last_pub_state_[id] = state;

                GatewayEvent state_ev{};
                state_ev.type = EventType::DEVICE_STATE;
                snprintf(state_ev.device_state.device_id, sizeof(state_ev.device_state.device_id), "%s", id.c_str());
                state_ev.device_state.state = state;
                state_ev.device_state.position = pos;
                state_ev.device_state.is_estimated = true;
                dispatch_event(state_ev);
            }
        }
    }
}

void IohcGateway::dispatch_event(const GatewayEvent& ev) {
    for (auto& cb : callbacks_) cb(ev);
}

// ---------------------------------------------------------------------------
// Bewegingscommando's
// ---------------------------------------------------------------------------
bool IohcGateway::cmd_open(const std::string& id) {
    if (!remote_) return false;
    Tokens t = {"open", id};
    remote_->cmd(IOHC::RemoteButton::Open, &t);
    tx_count_++;
    // TX event genereren voor consistente tx_count tracking via events
    GatewayEvent ev{};
    ev.type = EventType::FRAME_TRANSMITTED;
    dispatch_event(ev);
    return true;
}
bool IohcGateway::cmd_close(const std::string& id) {
    if (!remote_) return false;
    Tokens t = {"close", id};
    remote_->cmd(IOHC::RemoteButton::Close, &t);
    tx_count_++;
    GatewayEvent ev{}; ev.type = EventType::FRAME_TRANSMITTED;
    dispatch_event(ev);
    return true;
}
bool IohcGateway::cmd_stop(const std::string& id) {
    if (!remote_) return false;
    Tokens t = {"stop", id};
    remote_->cmd(IOHC::RemoteButton::Stop, &t);
    tx_count_++;
    GatewayEvent ev{}; ev.type = EventType::FRAME_TRANSMITTED;
    dispatch_event(ev);
    return true;
}
bool IohcGateway::cmd_set_position(const std::string& id, float pct) {
    if (!remote_) return false;
    Tokens t = {"position", id, std::to_string((int)pct)};
    remote_->cmd(IOHC::RemoteButton::Position, &t);
    tx_count_++;
    GatewayEvent ev{}; ev.type = EventType::FRAME_TRANSMITTED;
    dispatch_event(ev);
    return true;
}

// ---------------------------------------------------------------------------
// Koppelcommando's
// ---------------------------------------------------------------------------
bool IohcGateway::cmd_new_remote(const std::string& name) {
    if (!remote_) {
        ESP_LOGE(TAG, "new1W mislukt: gateway remote engine niet geïnitialiseerd");
        GatewayEvent ev{}; ev.type = EventType::RADIO_ERROR;
        snprintf(ev.radio_error.message, sizeof(ev.radio_error.message), "Remote engine niet gereed");
        ev.radio_error.code = -1;
        dispatch_event(ev);
        return false;
    }
    if (name.empty()) {
        ESP_LOGE(TAG, "new1W mislukt: geen schermnaam ingevuld");
        GatewayEvent ev{}; ev.type = EventType::RADIO_ERROR;
        snprintf(ev.radio_error.message, sizeof(ev.radio_error.message), "Vul eerst een schermnaam in");
        ev.radio_error.code = -2;
        dispatch_event(ev);
        return false;
    }

    ESP_LOGI(TAG, "new1W: aanmaken virtueel remote '%s'", name.c_str());
    if (remote_->addRemote(name)) {
        const auto& remotes = remote_->getRemotes();
        if (!remotes.empty()) {
            const auto& r = remotes.back();
            GatewayEvent ev{};
            ev.type = EventType::DEVICE_ADDED;
            std::string id = bytesToHexString(r.node, sizeof(r.node));
            snprintf(ev.device_discovered.device_id, sizeof(ev.device_discovered.device_id), "%s", id.c_str());
            snprintf(ev.device_discovered.name, sizeof(ev.device_discovered.name), "%s", r.name.c_str());
            dispatch_event(ev);
        }
        return true;
    }

    ESP_LOGE(TAG, "new1W mislukt: toevoegen remote '%s' is mislukt", name.c_str());
    GatewayEvent ev{}; ev.type = EventType::RADIO_ERROR;
    snprintf(ev.radio_error.message, sizeof(ev.radio_error.message), "Aanmaken remote '%s' mislukt", name.c_str());
    ev.radio_error.code = -3;
    dispatch_event(ev);
    return false;
}

bool IohcGateway::cmd_add(const std::string& id) {
    if (!remote_ || id.empty()) {
        ESP_LOGE(TAG, "add mislukt: doel-ID '%s' is leeg of remote engine niet gereed", id.c_str());
        GatewayEvent ev{}; ev.type = EventType::RADIO_ERROR;
        snprintf(ev.radio_error.message, sizeof(ev.radio_error.message), "Vul eerst een doel scherm ID in");
        ev.radio_error.code = -4;
        dispatch_event(ev);
        return false;
    }
    ESP_LOGI(TAG, "add: stuur Add-frame naar '%s'", id.c_str());
    Tokens t = {"add", id};
    remote_->cmd(IOHC::RemoteButton::Add, &t);
    tx_count_++;
    GatewayEvent ev{}; ev.type = EventType::FRAME_TRANSMITTED;
    dispatch_event(ev);
    return true;
}

bool IohcGateway::cmd_remove(const std::string& id) {
    if (!remote_ || id.empty()) {
        ESP_LOGE(TAG, "remove mislukt: doel-ID '%s' is leeg of remote engine niet gereed", id.c_str());
        GatewayEvent ev{}; ev.type = EventType::RADIO_ERROR;
        snprintf(ev.radio_error.message, sizeof(ev.radio_error.message), "Vul eerst een doel scherm ID in");
        ev.radio_error.code = -5;
        dispatch_event(ev);
        return false;
    }
    ESP_LOGI(TAG, "remove: stuur Remove-frame naar '%s'", id.c_str());
    Tokens t = {"remove", id};
    remote_->cmd(IOHC::RemoteButton::Remove, &t);
    tx_count_++;
    GatewayEvent ev{}; ev.type = EventType::FRAME_TRANSMITTED;
    dispatch_event(ev);
    return true;
}

bool IohcGateway::cmd_pair(const std::string& id) {
    if (!remote_ || id.empty()) {
        ESP_LOGE(TAG, "pair mislukt: ID '%s' is leeg of remote engine niet gereed", id.c_str());
        return false;
    }
    Tokens t = {"pair", id};
    remote_->cmd(IOHC::RemoteButton::Pair, &t);
    tx_count_++;
    GatewayEvent ev{}; ev.type = EventType::FRAME_TRANSMITTED;
    dispatch_event(ev);
    return true;
}

bool IohcGateway::cmd_scan() {
    if (!remote_) return false;
    ESP_LOGI(TAG, "scan: luistermodus gestart (60s timeout)");
    scan_mode_     = true;
    scan_start_ms_ = millis();
    // Geen TX — puur luisteren. Geen cmd() aanroep.
    return true;
}

bool IohcGateway::cmd_reload_devices() {
    ESP_LOGI(TAG, "Herladen apparaten uit '%s'", devices_file_.c_str());
    return load_devices();  // load_devices() wist remote_ eerst
}

} // namespace iohomecontrol
