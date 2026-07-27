# ESPHome 2026.7.0 — Relevante wijzigingen

Dit project vereist minimaal ESPHome **2026.7.0**.
Hier staan de voor dit project relevante wijzigingen en breaking changes.

## Breaking changes die dit project betreffen

### 1. ESP32 toolchain standaard naar ESP-IDF

Configs zonder expliciete `toolchain:` compileren nu met native ESP-IDF in plaats van PlatformIO.

**Actie:** Altijd expliciet instellen:
```yaml
esp32:
  toolchain: esp-idf
  framework:
    type: esp-idf
```

Om het oude gedrag te bewaren:
```yaml
esp32:
  toolchain: platformio
  framework:
    type: arduino
```

### 2. Web server cross-origin verzoeken geblokkeerd

Verzoeken met een `Origin` header van een ander domein worden nu geblokkeerd.

**Actie:**
```yaml
web_server:
  version: 3
  enable_private_network_access: true
  # allowed_origins:
  #   - "http://homeassistant.local:8123"
```

### 3. `enable_private_network_access` standaard uit

LAN-toegang vanuit een browser vereist nu expliciete instelling.

**Actie:** Voeg toe aan `web_server:`:
```yaml
  enable_private_network_access: true
```

### 4. Web server version 1 deprecated

Version 1 wordt verwijderd in 2027.1.0. Gebruik version 3.

### 5. Packages include syntax gewijzigd

Oud (werkt niet meer):
```yaml
packages: !include mypackage.yaml
```

Nieuw:
```yaml
packages: [!include mypackage.yaml]
```

### 6. Web server digest authenticatie

Nieuw in 2026.7.0: wachtwoord wordt niet meer over het netwerk gestuurd.

```yaml
web_server:
  auth:
    type: digest    # nieuw
```

## Nieuwe features gebruikt in dit project

| Feature | Gebruik |
|---|---|
| OTA downgrade protection | Gesupport via `project: version:` |
| NVS encryption | Optioneel - zie ESP32-S3 documentatie |
| ccache standaard aan | Snellere hercompilaties |
| `min_version:` in YAML | Geblokkeerd op <2026.7.0 |
