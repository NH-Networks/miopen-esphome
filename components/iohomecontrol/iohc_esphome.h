#pragma once
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/button/button.h"
#include "iohc_gateway.h"
#include <map>
#include <string>

namespace iohomecontrol {

// ---------------------------------------------------------------------------
// IohcCover — één ESPHome cover entiteit per io-homecontrol apparaat
// ---------------------------------------------------------------------------
class IohcCover : public esphome::cover::Cover {
public:
    explicit IohcCover(const std::string& device_id, IohcGateway* gw)
        : device_id_(device_id), gateway_(gw) {}

    // Virtual destructor required: class is polymorphic (delete via base pointer)
    virtual ~IohcCover() = default;

    esphome::cover::CoverTraits get_traits() override {
        auto t = esphome::cover::CoverTraits();
        t.set_supports_position(true);
        t.set_supports_stop(true);
        t.set_is_assumed_state(true);
        return t;
    }

    void control(const esphome::cover::CoverCall& call) override;
    void update_state(CoverState state, float position, bool estimated);
    const std::string& device_id() const { return device_id_; }

private:
    std::string  device_id_;
    IohcGateway* gateway_;
    float                           prev_position_{-2.0f};
    esphome::cover::CoverOperation  prev_operation_{esphome::cover::COVER_OPERATION_IDLE};
};

// ---------------------------------------------------------------------------
// Koppelknoppen
// ---------------------------------------------------------------------------

class IohcScanButton : public esphome::button::Button {
public:
    void set_gateway(IohcGateway* gw) { gateway_ = gw; }
protected:
    void press_action() override {
        if (gateway_) gateway_->cmd_scan();
    }
private:
    IohcGateway* gateway_{nullptr};
};

class IohcAddButton : public esphome::button::Button {
public:
    void set_gateway(IohcGateway* gw)          { gateway_ = gw; }
    void set_target(const std::string* target)  { target_  = target; }
protected:
    void press_action() override {
        if (!gateway_ || !target_ || target_->empty()) return;
        gateway_->cmd_add(*target_);
    }
private:
    IohcGateway*        gateway_{nullptr};
    const std::string*  target_{nullptr};
};

class IohcRemoveButton : public esphome::button::Button {
public:
    void set_gateway(IohcGateway* gw)          { gateway_ = gw; }
    void set_target(const std::string* target)  { target_  = target; }
protected:
    void press_action() override {
        if (!gateway_ || !target_ || target_->empty()) return;
        gateway_->cmd_remove(*target_);
    }
private:
    IohcGateway*        gateway_{nullptr};
    const std::string*  target_{nullptr};
};

class IohcNewRemoteButton : public esphome::button::Button {
public:
    void set_gateway(IohcGateway* gw)          { gateway_ = gw; }
    void set_name_source(const std::string* n)  { name_    = n; }
protected:
    void press_action() override {
        if (!gateway_ || !name_ || name_->empty()) return;
        gateway_->cmd_new_remote(*name_);
    }
private:
    IohcGateway*        gateway_{nullptr};
    const std::string*  name_{nullptr};
};

class IohcReloadButton : public esphome::button::Button {
public:
    void set_gateway(IohcGateway* gw) { gateway_ = gw; }
protected:
    void press_action() override {
        if (gateway_) gateway_->cmd_reload_devices();
    }
private:
    IohcGateway* gateway_{nullptr};
};

// ---------------------------------------------------------------------------
// IohcGatewayComponent — hoofd ESPHome component
// ---------------------------------------------------------------------------
class IohcGatewayComponent : public esphome::Component {
public:
    void set_sck_pin(int p)    { gateway_.set_sck_pin(p); }
    void set_miso_pin(int p)   { gateway_.set_miso_pin(p); }
    void set_mosi_pin(int p)   { gateway_.set_mosi_pin(p); }
    void set_nss_pin(int p)    { gateway_.set_nss_pin(p); }
    void set_reset_pin(int p)  { gateway_.set_reset_pin(p); }
    void set_dio0_pin(int p)   { gateway_.set_dio0_pin(p); }
    void set_dio1_pin(int p)   { gateway_.set_dio1_pin(p); }
    void set_frequency(uint32_t f)             { gateway_.set_frequency(f); }
    void set_devices_file(const std::string& f){ gateway_.set_devices_file(f); }
    void set_cozy_file(const std::string& f)   { gateway_.set_cozy_file(f); }
    void set_other_file(const std::string& f)  { gateway_.set_other_file(f); }
    void set_radio_platform(const std::string& p) { gateway_.set_radio_platform(p); }
    void add_remote_map(const std::string& name, const std::vector<std::string>& devices) {
        gateway_.add_remote_map(name, devices);
    }

    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return esphome::setup_priority::HARDWARE; }

    void set_rssi_sensor(esphome::sensor::Sensor* s)              { rssi_sensor_   = s; }
    void set_rx_counter(esphome::sensor::Sensor* s)               { rx_sensor_     = s; }
    void set_tx_counter(esphome::sensor::Sensor* s)               { tx_sensor_     = s; }
    void set_status_sensor(esphome::text_sensor::TextSensor* s)   { status_sensor_ = s; }
    void set_last_addr_sensor(esphome::text_sensor::TextSensor* s){ last_addr_sensor_ = s; }
    void set_pending_sensor(esphome::text_sensor::TextSensor* s)  { pending_sensor_  = s; }

    void set_scan_button(IohcScanButton* b)          { scan_btn_       = b; }
    void set_add_button(IohcAddButton* b)             { add_btn_        = b; }
    void set_remove_button(IohcRemoveButton* b)       { remove_btn_     = b; }
    void set_new_remote_button(IohcNewRemoteButton* b){ new_remote_btn_ = b; }
    void set_reload_button(IohcReloadButton* b)       { reload_btn_     = b; }

    void set_target_device(const std::string& id)   { target_device_ = id; }
    void set_device_name(const std::string& name)    { device_name_   = name; }
    const std::string& target_device() const         { return target_device_; }
    const std::string& device_name()   const         { return device_name_;   }

    IohcGateway* get_gateway() { return &gateway_; }
    void register_cover(IohcCover* cov) {
        if (cov != nullptr) {
            covers_[cov->device_id()] = cov;
        }
    }

    void cmd_scan()                                  { gateway_.cmd_scan(); }
    void cmd_add(const std::string& target)          { gateway_.cmd_add(target); }
    void cmd_remove(const std::string& target)       { gateway_.cmd_remove(target); }
    void cmd_new_remote(const std::string& name)     { gateway_.cmd_new_remote(name); }
    void cmd_reload()                                { gateway_.cmd_reload_devices(); }

private:
    void on_gateway_event(const GatewayEvent& ev);
    IohcCover* get_or_create_cover(const std::string& device_id,
                                   const std::string& name = "");
    void wire_buttons();

    IohcGateway  gateway_;
    std::map<std::string, IohcCover*> covers_;

    esphome::sensor::Sensor*           rssi_sensor_{nullptr};
    esphome::sensor::Sensor*           rx_sensor_{nullptr};
    esphome::sensor::Sensor*           tx_sensor_{nullptr};
    esphome::text_sensor::TextSensor*  status_sensor_{nullptr};
    esphome::text_sensor::TextSensor*  last_addr_sensor_{nullptr};
    esphome::text_sensor::TextSensor*  pending_sensor_{nullptr};

    IohcScanButton*      scan_btn_{nullptr};
    IohcAddButton*       add_btn_{nullptr};
    IohcRemoveButton*    remove_btn_{nullptr};
    IohcNewRemoteButton* new_remote_btn_{nullptr};
    IohcReloadButton*    reload_btn_{nullptr};

    std::string target_device_;
    std::string device_name_;

    uint32_t error_count_{0};
    uint32_t last_diag_ms_{0};
};

// ---------------------------------------------------------------------------
// Custom ESPHome Actions
// ---------------------------------------------------------------------------
template<typename... Ts> class ScanAction : public esphome::Action<Ts...> {
 public:
  explicit ScanAction(IohcGatewayComponent *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->cmd_scan(); }
 private:
  IohcGatewayComponent *parent_;
};

template<typename... Ts> class AddAction : public esphome::Action<Ts...> {
 public:
  explicit AddAction(IohcGatewayComponent *parent) : parent_(parent) {}
  esphome::TemplatableValue<std::string, Ts...> target_;
  template<typename V> void set_target(V target) { this->target_ = target; }
  void play(Ts... x) override {
    auto target = this->target_.value(x...);
    this->parent_->cmd_add(target);
  }
 private:
  IohcGatewayComponent *parent_;
};

template<typename... Ts> class RemoveAction : public esphome::Action<Ts...> {
 public:
  explicit RemoveAction(IohcGatewayComponent *parent) : parent_(parent) {}
  esphome::TemplatableValue<std::string, Ts...> target_;
  template<typename V> void set_target(V target) { this->target_ = target; }
  void play(Ts... x) override {
    auto target = this->target_.value(x...);
    this->parent_->cmd_remove(target);
  }
 private:
  IohcGatewayComponent *parent_;
};

template<typename... Ts> class NewRemoteAction : public esphome::Action<Ts...> {
 public:
  explicit NewRemoteAction(IohcGatewayComponent *parent) : parent_(parent) {}
  esphome::TemplatableValue<std::string, Ts...> name_;
  template<typename V> void set_name(V name) { this->name_ = name; }
  void play(Ts... x) override {
    auto name = this->name_.value(x...);
    this->parent_->cmd_new_remote(name);
  }
 private:
  IohcGatewayComponent *parent_;
};

template<typename... Ts> class ReloadAction : public esphome::Action<Ts...> {
 public:
  explicit ReloadAction(IohcGatewayComponent *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->cmd_reload(); }
 private:
  IohcGatewayComponent *parent_;
};

} // namespace iohomecontrol
