#pragma once
#include <stdint.h>
#include <string>
#include <functional>
#include <vector>
#include <map>
#include "gateway_events.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

class iohcRadio;
class iohcRemote1W;

namespace iohomecontrol {

static constexpr size_t EVENT_QUEUE_SIZE = 16;

// ---------------------------------------------------------------------------
// ISR-safe single-producer / single-consumer ring buffer
// Uses a FreeRTOS spinlock (portMUX) for dual-core safety on ESP32-S3.
// head is written only by the ISR (core 0), tail only by loop() (core 1).
// ---------------------------------------------------------------------------
struct EventQueue {
    GatewayEvent    buf[EVENT_QUEUE_SIZE];
    volatile size_t head{0};
    volatile size_t tail{0};
    portMUX_TYPE    mux = portMUX_INITIALIZER_UNLOCKED;

    bool IRAM_ATTR push(const GatewayEvent& ev) {
        portENTER_CRITICAL_ISR(&mux);
        size_t next = (head + 1) & (EVENT_QUEUE_SIZE - 1);
        bool ok = (next != tail);
        if (ok) {
            buf[head] = ev;
            head = next;
        }
        portEXIT_CRITICAL_ISR(&mux);
        return ok;
    }

    bool pop(GatewayEvent& ev) {
        portENTER_CRITICAL(&mux);
        bool ok = (head != tail);
        if (ok) {
            ev   = buf[tail];
            tail = (tail + 1) & (EVENT_QUEUE_SIZE - 1);
        }
        portEXIT_CRITICAL(&mux);
        return ok;
    }
};

using EventCallback = std::function<void(const GatewayEvent&)>;

class IohcGateway {
public:
    IohcGateway();
    ~IohcGateway();

    // Pin setters
    void set_sck_pin(int p)    { sck_pin_   = p; }
    void set_miso_pin(int p)   { miso_pin_  = p; }
    void set_mosi_pin(int p)   { mosi_pin_  = p; }
    void set_nss_pin(int p)    { nss_pin_   = p; }
    void set_reset_pin(int p)  { reset_pin_ = p; }
    void set_dio0_pin(int p)   { dio0_pin_  = p; }
    void set_dio1_pin(int p)   { dio1_pin_  = p; }
    void set_frequency(uint32_t f)             { frequency_    = f; }
    void set_devices_file(const std::string& f){ devices_file_ = f; }

    bool begin();
    void loop();

    // Motion commands
    bool cmd_open(const std::string& device_id);
    bool cmd_close(const std::string& device_id);
    bool cmd_stop(const std::string& device_id);
    bool cmd_set_position(const std::string& device_id, float position_pct);

    // Pairing / management
    bool cmd_pair(const std::string& device_id);
    bool cmd_add(const std::string& device_id);
    bool cmd_remove(const std::string& device_id);
    bool cmd_new_remote(const std::string& name);
    bool cmd_scan();                 // enter RX-only listen mode (geen TX)
    bool cmd_reload_devices();       // herlaad 1W.json, wist bestaande remote

    void on_event(EventCallback cb) { callbacks_.push_back(std::move(cb)); }

    // Diagnostics
    uint32_t    rx_count()    const { return rx_count_;    }
    uint32_t    tx_count()    const { return tx_count_;    }
    uint32_t    error_count() const { return error_count_; }
    bool        radio_ok()    const { return radio_ok_;    }
    bool        scan_mode()   const { return scan_mode_;   }
    int16_t     last_rssi()   const { return last_rssi_;   }
    const std::string& last_error() const { return last_error_; }
    const std::string& last_addr()  const { return last_addr_;  }

    static void IRAM_ATTR isr_dio0_handler();
    static IohcGateway* instance_;

private:
    void dispatch_event(const GatewayEvent& ev);
    bool init_radio();
    bool load_devices();
    void IRAM_ATTR enqueue_rx_irq();

    int      sck_pin_{-1}, miso_pin_{-1}, mosi_pin_{-1};
    int      nss_pin_{-1}, reset_pin_{-1}, dio0_pin_{-1}, dio1_pin_{-1};
    uint32_t frequency_{868950000};
    std::string devices_file_{"/1W.json"};

    iohcRadio*    radio_{nullptr};
    iohcRemote1W* remote_{nullptr};

    EventQueue  event_queue_;
    std::vector<EventCallback> callbacks_;

    uint32_t    rx_count_{0};
    uint32_t    tx_count_{0};
    uint32_t    error_count_{0};
    bool        radio_ok_{false};
    bool        scan_mode_{false};
    uint32_t    scan_start_ms_{0};       // voor 60s scan timeout
    int16_t     last_rssi_{0};
    std::string last_error_;
    std::string last_addr_;
    std::map<std::string, float>      last_pub_pos_;
    std::map<std::string, CoverState> last_pub_state_;
};

} // namespace iohomecontrol
