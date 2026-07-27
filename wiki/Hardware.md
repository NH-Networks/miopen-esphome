# Hardware — LilyGO T3-S3 V1.2

## Board

| Eigenschap | Waarde |
|---|---|
| Board | LilyGO T3-S3 V1.2 |
| MCU | ESP32-S3 |
| Radio | SX1276 868 MHz |
| Display | 0.96″ SSD1306 I2C (optioneel) |
| USB | USB-CDC (USB_MODE=1) |

## SX1276 Pinout

| Signaal | GPIO | Opmerking |
|---|---|---|
| SCK | 5 | SPI klok |
| MISO | 3 | SPI data in |
| MOSI | 6 | SPI data uit |
| NSS/CS | 7 | SPI chip select |
| RESET | 8 | Radio reset |
| DIO0 | 33 | RX/TX done IRQ |
| DIO1 | 34 | Optioneel |

## I2C (OLED)

| Signaal | GPIO |
|---|---|
| SDA | 17 |
| SCL | 18 |

Het OLED display is uitgecommentarieerd in de YAML. Verwijder de commentaar-tekens om het in te schakelen.
