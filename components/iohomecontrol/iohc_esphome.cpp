#include "iohc_esphome.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cmath>  // fabsf()

static const char* TAG = "iohc_esphome";

namespace iohomecontrol {

// ---------------------------------------------------------------------------
// IohcCover
// ---------------------------------------------------------------------------
void IohcCover::control(const esphome::cover::CoverCall& call) {
    if (call.get_stop()) {
        gateway_->cmd_stop(device_id_);
        return;
    }
    if (call.get_position().has_value()) {
        float pos = *call.get_position();
        if (pos >= esphome::cover::COVER_OPEN - 0.01f)
            gateway_->cmd_open(device_id_);
        else if (pos <= esphome::cover::COVER_CLOSED + 0.01f)
            gateway_->cmd_close(device_id_);
        else
            gateway_->cmd_set_position(device_id_, pos * 100.0f);
    }
}

void IohcCover::update_state(CoverState state, float position, bool estimated) {
    using namespace esphome::cover;

    // CoverState::OPEN / CLOSED forceren de positie ongeacht de position parameter
    if (state == CoverState::OPEN) {
        position = COVER_OPEN;    // 1.0f
    } else if (state == CoverState::CLOSED) {
        position = COVER_CLOSED;  // 0.0f
    }

    CoverOperation new_op;
    switch (state) {
        case CoverState::OPENING: new_op = COVER_OPERATION_OPENING; break;
        case CoverState::CLOSING: new_op = COVER_OPERATION_CLOSING; break;
        default:                  new_op = COVER_OPERATION_IDLE;    break;
    }

    // Positie bijwerken als geldig en veranderd
    bool pos_changed = false;
    if (position >= 0.0f && position <= 1.0f &&
        fabsf(position - prev_position_) > 0.005f) {
        this->position = position;
        prev_position_ = position;
        pos_changed    = true;
    }

    // Operatie bijwerken als veranderd
    bool op_changed = (new_op != prev_operation_);
    if (op_changed) {
        this->current_operation = new_op;
        prev_operation_         = new_op;
    }

    // Alleen publiceren als iets veranderd is
    if (pos_changed || op_changed) {
        this->publish_state(false);
    }
}

// ---------------------------------------------------------------------------
// IohcGatewayComponent
// ---------------------------------------------------------------------------
void IohcGatewayComponent::setup() {
    ESP_LOGI(TAG, "IOHC gateway component setup");

    gateway_.on_event([this](const GatewayEvent& ev) {
        this->on_gateway_event(ev);
    });

    if (!gateway_.begin()) {
        ESP_LOGE(TAG, "Gateway init mislukt \u2014 component als failed gemarkeerd");
        this->mark_failed();
        if (status_sensor_) status_sensor_->publish_state("Radio init failed");
        return;
    }

    wire_buttons();

    if (status_sensor_)    status_sensor_->publish_state("Ready");
    if (last_addr_sensor_) last_addr_sensor_->publish_state("");
    if (pending_sensor_)   pending_sensor_->publish_state("");
    ESP_LOGI(TAG, "IOHC gateway klaar");
}

void IohcGatewayComponent::wire_buttons() {
    if (scan_btn_) {
        scan_btn_->set_gateway(&gateway_);
    }
    if (add_btn_) {
        add_btn_->set_gateway(&gateway_);
        add_btn_->set_target(&target_device_);
    }
    if (remove_btn_) {
        remove_btn_->set_gateway(&gateway_);
        remove_btn_->set_target(&target_device_);
    }
    if (new_remote_btn_) {
        new_remote_btn_->set_gateway(&gateway_);
        new_remote_btn_->set_name_source(&device_name_);
    }
    if (reload_btn_) {
        reload_btn_->set_gateway(&gateway_);
    }
}

void IohcGatewayComponent::loop() {
    gateway_.loop();

    // Diagnostics rx/tx elke 10 seconden publiceren
    const uint32_t now = millis();
    if (now - last_diag_ms_ >= 10000) {
        last_diag_ms_ = now;
        if (rx_sensor_) rx_sensor_->publish_state((float)gateway_.rx_count());
        if (tx_sensor_) tx_sensor_->publish_state((float)gateway_.tx_count());
    }
}

void IohcGatewayComponent::on_gateway_event(const GatewayEvent& ev) {
    switch (ev.type) {

        case EventType::DEVICE_STATE: {
            std::string id(ev.device_state.device_id);
            auto* cov = get_or_create_cover(id);
            if (cov) cov->update_state(ev.device_state.state,
                                       ev.device_state.position,
                                       ev.device_state.is_estimated);
            break;
        }

        case EventType::POSITION_UPDATE: {
            std::string id(ev.position.device_id);
            auto it = covers_.find(id);
            if (it != covers_.end())
                it->second->update_state(CoverState::UNKNOWN,
                                         ev.position.position,
                                         ev.position.is_estimated);
            break;
        }

        case EventType::FRAME_RECEIVED: {
            if (rssi_sensor_)
                rssi_sensor_->publish_state((float)ev.frame.rssi);

            const std::string& addr = gateway_.last_addr();
            if (!addr.empty()) {
                if (last_addr_sensor_) last_addr_sensor_->publish_state(addr);
                if (pending_sensor_ && gateway_.scan_mode())
                    pending_sensor_->publish_state(addr);
            }
            break;
        }

        case EventType::FRAME_TRANSMITTED: {
            break;
        }

        case EventType::DEVICE_ADDED: {
            std::string id(ev.device_discovered.device_id);
            std::string name(ev.device_discovered.name);
            get_or_create_cover(id, name);
            if (status_sensor_)
                status_sensor_->publish_state("Paired: " + id);
            ESP_LOGI(TAG, "Apparaat gekoppeld: %s (%s)", id.c_str(), name.c_str());
            break;
        }

        case EventType::DEVICE_REMOVED: {
            const std::string id(ev.device_discovered.device_id);
            auto it = covers_.find(id);
            if (it != covers_.end()) {
                delete it->second;
                covers_.erase(it);
                ESP_LOGI(TAG, "Apparaat ontkoppeld: %s", id.c_str());
            }
            break;
        }

        case EventType::RADIO_ERROR: {
            error_count_++;
            ESP_LOGE(TAG, "Radio fout [%d]: %s",
                     ev.radio_error.code, ev.radio_error.message);
            if (status_sensor_)
                status_sensor_->publish_state(
                    std::string("Fout: ") + ev.radio_error.message);
            break;
        }

        case EventType::PAIRING_CHANGED:
        default:
            break;
    }
}

IohcCover* IohcGatewayComponent::get_or_create_cover(
        const std::string& device_id, const std::string& name) {
    auto it = covers_.find(device_id);
    if (it != covers_.end()) return it->second;

    const std::string entity_name   = name.empty() ? ("iohc_" + device_id) : name;
    const std::string object_id_str = "iohc_" + device_id;

    auto* cov = new IohcCover(device_id, &gateway_);
    cov->set_name(entity_name);
    cov->set_object_id(object_id_str);
    esphome::App.register_component(cov);
    covers_[device_id] = cov;

    ESP_LOGI(TAG, "Cover geregistreerd: '%s' (id: %s)",
             entity_name.c_str(), object_id_str.c_str());
    return cov;
}

} // namespace iohomecontrol
