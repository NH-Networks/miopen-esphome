# miopen-esphome

Native ESPHome component for **io-homecontrol** (Somfy, Velux, Roto) roller blinds and shutters.

Control your io-homecontrol screens directly from Home Assistant — no cloud, no MQTT broker, no custom firmware. Just a cheap SX1276 radio module wired to an ESP32 and a few lines of YAML.

---

## Features

- Full open/close/stop/position control from Home Assistant
- Native ESPHome API — real-time, no MQTT needed
- Pair new screens wirelessly from HA
- Persistent device storage on LittleFS (survives reboots)
- Live RSSI, RX/TX counters, status sensors
- OTA updates via ESPHome dashboard
- Automations via standard ESPHome `on_` triggers
- 1W protocol support (Somfy io / Velux KLF)

---

## Hardware

Any ESP32 board with an SX1276/SX1278 868 MHz LoRa module works. Tested boards:

| Board | Notes |
|---|---|
| LilyGo TTGO LoRa32 | Recommended — integrated SX1276 |
| Heltec WiFi LoRa 32 | Works, set pins accordingly |
| Generic ESP32 + RA-02 | Wire SPI manually |

### SPI wiring (LilyGo TTGO LoRa32 example)

| SX1276 pin | ESP32 GPIO |
|---|---|
| SCK | 5 |
| MISO | 19 |
| MOSI | 27 |
| NSS / CS | 18 |
| RESET | 14 |
| DIO0 | 26 |
| DIO2 | 34 |

---

## Installation

Add this repository as an ESPHome external component:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/NH-Networks/miopen-esphome
      ref: main
    components: [iohomecontrol]
```

---

## Minimal YAML example

```yaml
esphome:
  name: iohc-gateway
  platform: ESP32
  board: ttgo-lora32-v1

external_components:
  - source:
      type: git
      url: https://github.com/NH-Networks/miopen-esphome
      ref: main
    components: [iohomecontrol]

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
ota:
  platform: esphome

iohomecontrol:
  # SPI pins for your SX1276 module
  sck_pin: 5
  miso_pin: 19
  mosi_pin: 27
  nss_pin: 18
  reset_pin: 14
  dio0_pin: 26
  dio1_pin: 34

  # EU frequency — change to 433950000 for non-EU io-homecontrol
  frequency: 868950000

  # Target device ID for Add/Remove buttons (6-char hex node address)
  # Read it from the 'last_addr_sensor' after pressing a button on the screen.
  target_device: ""

  # Name for new virtual remote (used by new_remote_button)
  device_name: ""

  # Diagnostic sensors
  rssi_sensor:
    name: "IOHC RSSI"
  rx_counter:
    name: "IOHC RX frames"
  tx_counter:
    name: "IOHC TX frames"
  status_sensor:
    name: "IOHC Status"
  last_addr_sensor:
    name: "IOHC Last address"
  pending_sensor:
    name: "IOHC Pending device"

  # Pairing control buttons
  scan_button:
    name: "Scan for screens"
  new_remote_button:
    name: "Create virtual remote"
  add_button:
    name: "Add screen"
  remove_button:
    name: "Remove screen"
  reload_button:
    name: "Reload devices"

  # Optional: statically declare known covers so they appear in HA
  # even before the first state update. device_id is the hex node address.
  # covers:
  #   - device_id: a1b2c3
  #     name: Living Room Blind
  #   - device_id: d4e5f6
  #     name: Bedroom Blind
```

---

## Pairing a new screen

1. In Home Assistant, go to **Developer Tools → Services** (or use the button entities).
2. Set **IOHC device_name** text input to a friendly name for the screen (e.g. `Living Room`).
3. Press **Create virtual remote** — this allocates a new 3-byte node address and stores it.
4. Put the screen into pairing mode (hold the screen's programming button until it jogs).
5. Set **IOHC target_device** to the screen’s node address (visible in **IOHC Pending device** sensor after step 4).
6. Press **Add screen** — the gateway sends the pairing frame.
7. The screen jogs to confirm. A new cover entity appears automatically in Home Assistant.
8. Press **Reload devices** to persist the pairing across reboots.

---

## HA Automation example

Close all blinds at sunset:

```yaml
automation:
  - alias: Close blinds at sunset
    trigger:
      - platform: sun
        event: sunset
    action:
      - service: cover.close_cover
        target:
          entity_id: cover.living_room_blind
```

---

## FAQ

**Which protocol versions are supported?**  
1W (standard Somfy io / Velux KLF) is fully supported. 2W (Somfy TaHoma bi-directional) is partially supported — position feedback works but full key exchange is experimental.

**My screens don’t appear in HA after pairing.**  
Make sure you pressed **Reload devices** after pairing. If they still don’t appear, check the ESPHome logs for `Cover registered:` messages and verify the `status_sensor` value.

**Can I use CC1101 instead of SX1276?**  
Yes — set `radio_platform: cc1101` in your YAML. Pin assignments follow the same SPI keys.

**The gateway compiles but the radio does not initialise.**  
Check your SPI wiring, especially NSS and RESET. The RESET pin must be pulled high before the SPI bus is active — verify with a multimeter.

**Covers show as `unknown` after boot.**  
This is normal. The component uses estimated position tracking — position is only updated after the first open/close command or a received frame. Declare covers statically under `covers:` to pre-register them.

---

## Credits

Protocol implementation based on [cridp/miopen](https://github.com/cridp/miopen) — the original io-homecontrol research and reverse engineering.  
ESPHome component wrapper by [NH-Networks](https://github.com/NH-Networks).

## License

Apache License 2.0 — see [LICENSE](LICENSE).
