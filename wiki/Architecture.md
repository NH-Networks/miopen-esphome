# Architectuur

## Overzicht

```
Home Assistant
    ↕ ESPHome native API (api: + encryption)
IohcGatewayComponent        ← ESPHome Component (setup/loop)
    ↕
IohcCover × N               ← ESPHome Cover entiteiten (dynamisch)
    ↕
IohcGateway                 ← Transport-agnostische gateway core
    ↕
iohcRemote1W                ← io-homecontrol 1W protocol (upstream, ongewijzigd)
    ↕
iohcRadio / SX1276          ← SX1276 868 MHz SPI driver (upstream, ongewijzigd)
```

## Ontwerpkeuzes

| Keuze | Reden |
|---|---|
| **Geen MQTT** | Alles via native API; geen extra broker nodig |
| **ISR-veilig** | DIO0 IRQ zet alleen een token in de queue; alle decoding gebeurt in `loop()` |
| **Protocollkern ongewijzigd** | `iohcRadio`, `iohcRemote1W`, `iohcPacket`, `iohcCryptoHelpers` zijn de upstream originelen |
| **Dynamische covers** | Cover entiteiten worden aangemaakt op runtime vanuit `1W.json`; geen statische YAML per scherm |
| **Persistente toestand** | Sequentie-counters in NVS + LittleFS (upstream logica behouden) |

## Bestandsstructuur

```
components/iohomecontrol/
├── __init__.py          ESPHome codegen / config validatie
├── gateway_events.h     Transport-agnostische domein-events
├── iohc_gateway.h/.cpp  Gateway core: ISR-queue, radio init, command dispatch
├── iohc_esphome.h/.cpp  ESPHome adapter: Cover entiteiten, knoppen, sensoren
└── README.md            Beknopte technische referentie

lilygo-t3s3-sx1276-868.yaml   ESPHome configuratie (template)
iohomecontrol.yaml            ESPHome configuratie (project-specifiek)
```
