# iohomecontrol ESPHome Component

ESPHome external component for io-homecontrol (Somfy/Velux) blinds and screens.

## Architecture

```
ESPHome native API (Home Assistant)
        |
 IohcGatewayComponent   <- ESPHome Component (setup/loop)
        |
  IohcCover (x N)       <- ESPHome Cover entities, one per blind
        |
  IohcGateway           <- Transport-agnostic gateway core
        |
  iohcRemote1W          <- io-homecontrol 1W protocol (upstream, unchanged)
        |
  iohcRadio / SX1276    <- SX1276 SPI radio driver (upstream, unchanged)
```

## Key design choices

- **No MQTT.** All HA communication via ESPHome native API.
- **ISR-safe.** DIO0 IRQ only enqueues a lightweight event token;
  all decoding, crypto and JSON writes happen in `loop()` context.
- **Protocol core unchanged.** `iohcRadio`, `iohcRemote1W`, `iohcPacket`,
  `iohcCryptoHelpers` and all AES/CRC code are the upstream originals.
- **Dynamic covers.** Cover entities are created at runtime from `1W.json`
  and registered with ESPHome `App`. No static YAML per device needed.
- **Persistent state.** Sequence counters in NVS + LittleFS (upstream logic
  retained). OTA-safe: highest counter value wins on boot.

## Hardware target

| Board | LilyGO T3-S3 V1.2 |
|-------|--------------------|
| MCU   | ESP32-S3           |
| Radio | SX1276 868 MHz     |
| OLED  | 0.96" I2C          |

## Pinout (T3-S3 V1.2 SX1276)

| Signal  | GPIO |
|---------|------|
| SCK     | 5    |
| MISO    | 3    |
| MOSI    | 6    |
| NSS/CS  | 7    |
| RESET   | 8    |
| DIO0    | 33   |
| DIO1    | 34   |

## Not MQTT

The original `mqtt_handler.cpp` and all `#define MQTT` guards have been
removed. The ESPHome `api:` component handles Home Assistant discovery,
entity registration and command routing automatically.
