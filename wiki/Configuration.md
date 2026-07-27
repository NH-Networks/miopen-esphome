# YAML Configuratie

## Bestanden

| Bestand | Doel |
|---|---|
| `lilygo-t3s3-sx1276-868.yaml` | Volledige template, direct bruikbaar |
| `iohomecontrol.yaml` | Project-specifieke versie |
| `secrets.yaml` | Wachtwoorden en sleutels (niet committen!) |

## Verplichte secrets

```yaml
wifi_ssid: "JouwNetwerk"
wifi_password: "JouwWifiWachtwoord"
ap_password: "fallback123"
api_encryption_key: "<genereer: esphome generate-key>"
ota_password: "jouw-ota-wachtwoord"
web_password: "jouw-web-wachtwoord"
```

## Component parameters

### Verplicht

| Parameter | Type | Omschrijving |
|---|---|---|
| `sck_pin` | int | SPI klok GPIO |
| `miso_pin` | int | SPI MISO GPIO |
| `mosi_pin` | int | SPI MOSI GPIO |
| `nss_pin` | int | SPI CS GPIO |
| `reset_pin` | int | Radio reset GPIO |
| `dio0_pin` | int | DIO0 IRQ GPIO |

### Optioneel

| Parameter | Standaard | Omschrijving |
|---|---|---|
| `dio1_pin` | -1 | DIO1 GPIO (ongebruikt) |
| `frequency` | 868950000 | Radio frequentie in Hz |
| `devices_file` | /1W.json | Pad naar apparatenlijst in LittleFS |

### Sensoren (allemaal optioneel)

| Parameter | Type | Omschrijving |
|---|---|---|
| `rssi_sensor` | Sensor | Signaalsterkte in dBm |
| `rx_counter` | Sensor | Ontvangen frames teller |
| `tx_counter` | Sensor | Verzonden frames teller |
| `status_sensor` | TextSensor | Gateway status tekst |
| `last_addr_sensor` | TextSensor | Laatste gehoord radio-adres |
| `pending_sensor` | TextSensor | Adres tijdens scan modus |

### Knoppen (allemaal optioneel)

| Parameter | Omschrijving |
|---|---|
| `scan_button` | Start scan/luistermodus |
| `add_button` | Koppel scherm (Add-frame) |
| `remove_button` | Ontkoppel scherm |
| `new_remote_button` | Maak virtueel remote aan |
| `reload_button` | Herlaad 1W.json |

## ESP32 framework (2026.7.0)

Sinds ESPHome 2026.7.0 is ESP-IDF de standaard toolchain.
Gebruik altijd expliciete instellingen:

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
    version: recommended
  toolchain: esp-idf
```

## Web server (2026.7.0)

```yaml
web_server:
  port: 80
  version: 3                        # version 1 deprecated
  auth:
    username: admin
    password: !secret web_password
    type: digest                    # nieuw in 2026.7: wachtwoord niet over het netwerk
  ota: true
  enable_private_network_access: true  # nu standaard uit in 2026.7
```
