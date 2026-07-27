#pragma once
#include <stdint.h>
#include <string>

namespace iohomecontrol {

enum class EventType : uint8_t {
    FRAME_RECEIVED      = 0,
    FRAME_TRANSMITTED   = 1,   // was FRAME_TRANSMITTED, consistent met tx_count
    DEVICE_STATE        = 2,
    POSITION_UPDATE     = 3,
    PAIRING_CHANGED     = 4,   // toekomstig gebruik: koppelstatus veranderd
    RADIO_ERROR         = 5,
    DEVICE_ADDED        = 6,
    DEVICE_REMOVED      = 7,
};

struct FrameEvent {
    uint8_t  data[32];
    uint8_t  len;
    int16_t  rssi;
    uint32_t timestamp_ms;
};

enum class CoverState : uint8_t {
    UNKNOWN  = 0,
    OPEN     = 1,
    CLOSED   = 2,
    OPENING  = 3,
    CLOSING  = 4,
    STOPPED  = 5,
};

// device_id: 6 hex chars + null = 7 bytes; char[8] geeft 1 byte marge
// Altijd null-termineren bij aanmaken: device_id[7] = 0
static_assert(sizeof("FFFFFF") <= 8, "device_id buffer te klein");

struct DeviceStateEvent {
    char       device_id[8];   // hex adres, altijd null-terminated
    CoverState state;
    float      position;       // 0.0 (gesloten) .. 1.0 (open), -1 = onbekend
    bool       is_estimated;
};

struct PositionEvent {
    char  device_id[8];
    float position;            // 0.0 .. 1.0
    bool  is_estimated;
};

struct PairingEvent {
    char device_id[8];
    bool paired;
};

struct DeviceDiscoveredEvent {
    char device_id[8];
    char name[32];
};

struct RadioErrorEvent {
    char message[64];
    int  code;
};

struct GatewayEvent {
    EventType type;
    union {
        FrameEvent            frame;
        DeviceStateEvent      device_state;
        PositionEvent         position;
        PairingEvent          pairing;
        DeviceDiscoveredEvent device_discovered;
        RadioErrorEvent       radio_error;
    };
};

// Sentinel event voor gebruik als static in ISR (geen stack-allocatie)
inline const GatewayEvent& rx_sentinel_event() {
    static GatewayEvent ev = []() {
        GatewayEvent e{};
        e.type            = EventType::FRAME_RECEIVED;
        e.frame.len       = 0;
        e.frame.rssi      = 0;
        e.frame.timestamp_ms = 0;
        return e;
    }();
    return ev;
}

} // namespace iohomecontrol
