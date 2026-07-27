# Troubleshooting

## Build mislukt: `toolchain` fout

**Symptoom:** `Unknown toolchain` of onverwachte PlatformIO errors.

**Oplossing:** Voeg toe aan `esp32:` blok:
```yaml
esp32:
  toolchain: esp-idf
  framework:
    type: esp-idf
    version: recommended
```

## Build mislukt: `Cmd::scanMode` undefined

**Symptoom:** `error: 'scanMode' is not a member of 'Cmd'`

**Oorzaak:** Oude versie van `iohc_gateway.cpp` refereerde aan een global uit `interact.cpp`.
**Oplossing:** Update naar de laatste versie van de `esphome-native-api` branch.
De gateway gebruikt nu een lokale `scan_mode_` bool.

## Build mislukt: `bytesToHexString` undefined

**Symptoom:** `error: 'bytesToHexString' was not declared`

**Oplossing:** Zorg dat `iohcCryptoHelpers.h` geïnclude is in `iohc_gateway.cpp`. De huidige versie doet dit correct.

## Radio init mislukt (status: "Radio init failed")

**Controleer:**
1. SPI pinnen correct in YAML (zie [Hardware](Hardware))
2. Spanning op het board: T3-S3 vereist 3.3V op SX1276
3. Bekabeling: loshangende jumperwires geven onbetrouwbare SPI
4. Zet logger op DEBUG en zoek naar `iohc_gateway` log regels

```yaml
logger:
  level: DEBUG
```

## Scherm reageert niet op koppelen

**Controleer:**
1. Is het scherm in pair mode? (kort knopje achterop remote, scherm beweegt op/neer)
2. Pair mode duurt maar ~30 seconden — herhaal indien verlopen
3. Is het juiste adres ingevuld bij **Doel scherm ID**?
4. Gebruik **Scan** en **Gebruik laatste adres als doel** om het adres automatisch te detecteren
5. Controleer **Gateway RSSI** — waarden onder -100 dBm zijn te zwak

## Cover verschijnt niet in Home Assistant

**Oorzaak:** Covers worden dynamisch aangemaakt na ontvangst van een event.

**Oplossing:**
1. Voer **Koppel scherm** uit
2. Stuur daarna een commando naar het scherm (open/close)
3. Het event triggert de cover-registratie
4. Herstart HA om entiteiten te verversen als ze niet verschijnen

## `id(iohc_gw_last_addr)` compile error

**Symptoom:** `error: 'iohc_gw_last_addr' was not declared`

**Oorzaak:** Verkeerde ID in de YAML. Oud bug, gefixed in huidige versie.

**Oplossing:** Gebruik `id(last_addr_sensor)` — dit is de correcte ID zoals gedefinieerd in de sensor.

## Web UI niet bereikbaar

**Controleer:**
1. `enable_private_network_access: true` aanwezig in `web_server:` blok?
2. Browser blocked cross-origin? Voeg toe:
   ```yaml
   web_server:
     allowed_origins:
       - "http://homeassistant.local:8123"
   ```
3. Navigeer naar `http://iohomecontrol.local` (niet HTTPS)

## Logs bekijken

```bash
# Via ESPHome CLI:
esphome logs lilygo-t3s3-sx1276-868.yaml

# Via web UI:
http://iohomecontrol.local  # → Logs tab

# Via Home Assistant:
Instellingen → Logboek → filter op "iohc"
```
