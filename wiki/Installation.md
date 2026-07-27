# Installatie

## Vereisten

- ESPHome **2026.7.0** of nieuwer
- Home Assistant met ESPHome integratie
- LilyGO T3-S3 V1.2 board
- USB-C kabel (eerste flash)

## Stap 1 — Repository klonen

```bash
git clone https://github.com/NH-Networks/iohomecontrol.git
cd iohomecontrol
git checkout esphome-native-api
```

## Stap 2 — Secrets aanmaken

Maak een `secrets.yaml` aan naast de YAML configuratie:

```yaml
wifi_ssid: "JouwNetwerk"
wifi_password: "JouwWifiWachtwoord"
ap_password: "fallback123"
api_encryption_key: "<genereer met: esphome generate-key>"
ota_password: "jouw-ota-wachtwoord"
web_password: "jouw-web-wachtwoord"
```

> **Let op:** voeg `secrets.yaml` toe aan `.gitignore`. Commit dit bestand nooit.

## Stap 3 — Eerste flash (USB)

```bash
esphome run lilygo-t3s3-sx1276-868.yaml
```

ESPHome detecteert automatisch de USB-CDC poort van de T3-S3.

## Stap 4 — Home Assistant koppelen

1. Ga naar **Instellingen → Apparaten & diensten → ESPHome**
2. Het apparaat `iohomecontrol` verschijnt automatisch via mDNS
3. Voer de API encryption key in
4. Klaar — alle entiteiten zijn zichtbaar

## Stap 5 — OTA updates (na eerste flash)

```bash
esphome run lilygo-t3s3-sx1276-868.yaml
# of via de web UI: http://iohomecontrol.local
```

## LittleFS filesystem uploaden

De `1W.json` met gekoppelde schermen wordt opgeslagen in LittleFS.
Na het eerste pairen wordt dit automatisch beheerd.
Handmatig uploaden:

```bash
esphome upload lilygo-t3s3-sx1276-868.yaml --upload-port OTA
```
