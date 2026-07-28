#include <Arduino.h>
#include "SX1276Helpers.h"
#include "board-config.h"

#if defined(RADIO_SX127X)
#include <map>

#if defined(ESP8266)
#elif defined(ESP32)
#define CONFIG_DISABLE_HAL_LOCKS true
#include "TickerUsESP32.h"
#include <esp_task_wdt.h>
#include <SPI.h>
#endif

namespace Radio {
    SPISettings SpiSettings(4000000, MSBFIRST, SPI_MODE0);

    std::map<uint8_t, regBandWidth> __bw =
    {
        {25, {0x01, 0x04}},
        {50, {0x01, 0x03}},
        {100, {0x01, 0x02}},
        {125, {0x00, 0x02}},
        {200, {0x01, 0x01}},
        {250, {0x00, 0x01}}
    };

    void IRAM_ATTR SPI_beginTransaction() {
        SPI.beginTransaction(Radio::SpiSettings);
        digitalWrite(RADIO_NSS, LOW);
    }

    void IRAM_ATTR SPI_endTransaction() {
        digitalWrite(RADIO_NSS, HIGH);
        SPI.endTransaction();
    }

    void initHardware() {
        printf("\nSPI Init");
        pinMode(RADIO_MISO, INPUT_PULLUP);
        pinMode(RADIO_RESET, INPUT);

        while (!digitalRead(RADIO_RESET)) {
#if defined(ESP32)
            esp_task_wdt_reset();
#endif
            delayMicroseconds(1);
        }
        delayMicroseconds(BOARD_READY_AFTER_POR);

#if defined(ESP32)
        SPI.begin(RADIO_SCLK, RADIO_MISO, RADIO_MOSI, RADIO_NSS);
#endif

        pinMode(RADIO_NSS, OUTPUT);
        pinMode(RADIO_RESET, OUTPUT);
        digitalWrite(RADIO_RESET, HIGH);
        digitalWrite(RADIO_NSS, HIGH);
        delayMicroseconds(BOARD_READY_AFTER_POR);

        writeByte(REG_OPMODE, RF_OPMODE_STANDBY);

        pinMode(SCAN_LED, OUTPUT);
        digitalWrite(SCAN_LED, 1);
        printf("\nRadio Chip is ready\n");
    }

    void setPreambleLength(uint16_t preambleLen) {
        writeByte(REG_PREAMBLEMSB, (preambleLen >> 8) & 0xFF);
        writeByte(REG_PREAMBLELSB, preambleLen & 0xFF);
    }

    void initRegisters(uint8_t maxPayloadLength) {
        writeByte(REG_OPMODE, (readByte(REG_OPMODE) & RF_OPMODE_MASK) | RF_OPMODE_STANDBY);
        writeByte(REG_OSC, RF_OSC_CLKOUT_OFF);
        writeByte(
            REG_PACKETCONFIG1,
            RF_PACKETCONFIG1_PACKETFORMAT_VARIABLE | RF_PACKETCONFIG1_DCFREE_OFF | RF_PACKETCONFIG1_CRC_ON |
            RF_PACKETCONFIG1_CRCAUTOCLEAR_ON | RF_PACKETCONFIG1_CRCWHITENINGTYPE_CCITT |
            RF_PACKETCONFIG1_ADDRSFILTERING_OFF);
        writeByte(
            REG_PACKETCONFIG2,
            RF_PACKETCONFIG2_DATAMODE_PACKET | RF_PACKETCONFIG2_IOHOME_ON | RF_PACKETCONFIG2_IOHOME_POWERFRAME);
        writeByte(
            REG_SYNCCONFIG,
            RF_SYNCCONFIG_AUTORESTARTRXMODE_WAITPLL_OFF | RF_SYNCCONFIG_PREAMBLEPOLARITY_AA | RF_SYNCCONFIG_SYNC_ON);

        writeByte(REG_SYNCVALUE1, SYNC_BYTE_1);
        writeByte(REG_SYNCVALUE2, SYNC_BYTE_2);

        writeByte(
            REG_DIOMAPPING1,
            RF_DIOMAPPING1_DIO0_00 | RF_DIOMAPPING1_DIO1_01 | RF_DIOMAPPING1_DIO2_11 | RF_DIOMAPPING1_DIO3_01);
        writeByte(REG_DIOMAPPING2, RF_DIOMAPPING2_MAP_PREAMBLEDETECT | RF_DIOMAPPING2_DIO4_11 | RF_DIOMAPPING2_DIO5_10);

        if (MAX_FREQS != 1)
            writeByte(REG_PLLHOP, readByte(REG_PLLHOP) | RF_PLLHOP_FASTHOP_ON);

        writeByte(REG_PARAMP, RF_PARAMP_MODULATIONSHAPING_00 | RF_PARAMP_0012_US);
        writeByte(REG_PREAMBLEMSB, PREAMBLE_MSB);
        writeByte(REG_PREAMBLELSB, PREAMBLE_LSB);
        writeByte(REG_FIFOTHRESH, RF_FIFOTHRESH_TXSTARTCONDITION_FIFONOTEMPTY);
        writeByte(REG_PAYLOADLENGTH, 0xff);
        writeByte(REG_RSSICONFIG, RF_RSSICONFIG_SMOOTHING_8);
        writeByte(REG_RXCONFIG, RF_RXCONFIG_AFCAUTO_ON | RF_RXCONFIG_AGCAUTO_ON | RF_RXCONFIG_RXTRIGER_PREAMBLEDETECT | RF_RXCONFIG_RESTARTRXONCOLLISION_ON);
        writeByte(REG_AFCBW, RF_AFCBW_MANTAFC_16 | RF_AFCBW_EXPAFC_1);
        writeByte(REG_AFCFEI, 0x01);
        writeByte(REG_LNA, RF_LNA_BOOST_ON | RF_LNA_GAIN_G1);
        writeByte(
            REG_PREAMBLEDETECT,
            RF_PREAMBLEDETECT_DETECTOR_ON | RF_PREAMBLEDETECT_DETECTORSIZE_2 | RF_PREAMBLEDETECT_DETECTORTOL_10);
        writeByte(REG_PACONFIG, RF_PACONFIG_PASELECT_MASK | RF_PACONFIG_PASELECT_PABOOST);
        writeByte(REG_OCP, RF_OCP_ON | RF_OCP_TRIM_240_MA);
        writeByte(REG_PADAC, 0x87);
    }

    void calibrate() {
        uint8_t regPaConfigInitVal = readByte(REG_PACONFIG);
        writeByte(REG_PACONFIG, RF_PACONFIG_PASELECT_RFO);
        writeByte(REG_OSC, RF_OSC_RCCALSTART);
        writeByte(
            REG_IMAGECAL, (RF_IMAGECAL_AUTOIMAGECAL_MASK & RF_IMAGECAL_IMAGECAL_MASK) | RF_IMAGECAL_IMAGECAL_START);
        while ((readByte(REG_IMAGECAL) & RF_IMAGECAL_IMAGECAL_RUNNING) == RF_IMAGECAL_IMAGECAL_RUNNING) {}
        Radio::setCarrier(Radio::Carrier::Frequency, 868000000);
        writeByte(
            REG_IMAGECAL, (RF_IMAGECAL_AUTOIMAGECAL_MASK & RF_IMAGECAL_IMAGECAL_MASK) | RF_IMAGECAL_IMAGECAL_START);
        while ((readByte(REG_IMAGECAL) & RF_IMAGECAL_IMAGECAL_RUNNING) == RF_IMAGECAL_IMAGECAL_RUNNING) {}
        writeByte(REG_PACONFIG, regPaConfigInitVal);
    }

    void IRAM_ATTR setStandby() {
        writeByte(REG_OPMODE, (readByte(REG_OPMODE) & RF_OPMODE_MASK) | RF_OPMODE_STANDBY);
    }

    void IRAM_ATTR setTx() {
        writeByte(REG_SYNCCONFIG, (readByte(REG_SYNCCONFIG) & RF_SYNCCONFIG_SYNCSIZE_MASK) | RF_SYNCCONFIG_SYNCSIZE_2);
        writeByte(REG_OPMODE, (readByte(REG_OPMODE) & RF_OPMODE_MASK) | RF_OPMODE_TRANSMITTER);
        TxReady;
    }

    void IRAM_ATTR setRx() {
        writeByte(REG_SYNCCONFIG, (readByte(REG_SYNCCONFIG) & RF_SYNCCONFIG_SYNCSIZE_MASK) | RF_SYNCCONFIG_SYNCSIZE_3);
        writeByte(REG_OPMODE, (readByte(REG_OPMODE) & RF_OPMODE_MASK) | RF_OPMODE_RECEIVER);
        RxReady;
    }

    void readBurst(uint8_t regAddr, uint8_t *buffer, uint8_t size) {
        for (uint8_t i = 0; i < size; ++i) {
            buffer[i] = readByte(regAddr + i);
        }
    }

    void clearBuffer() {
        const uint8_t bufferSize = 64;
        for (uint8_t i = 0; i < bufferSize; i += 32) {
            uint8_t buffer[32];
            readBytes(REG_FIFO, buffer, sizeof(buffer));
        }
    }

    void IRAM_ATTR clearFlags() {
        uint16_t flags = readWord(REG_IRQFLAGS1);
        flags &= ~0xFFFF;
        writeWord(REG_IRQFLAGS1, flags);
    }

    bool IRAM_ATTR preambleDetected() {
        return readByte(REG_IRQFLAGS1) & RF_IRQFLAGS1_PREAMBLEDETECT;
    }

    bool IRAM_ATTR syncedAddress() {
        return readByte(REG_IRQFLAGS1) & RF_IRQFLAGS1_SYNCADDRESSMATCH;
    }

    bool IRAM_ATTR dataAvail() {
        return (readByte(REG_IRQFLAGS2) & RF_IRQFLAGS2_FIFOEMPTY) == 0;
    }

    uint8_t IRAM_ATTR readByte(uint8_t regAddr) {
        uint8_t getByte;
        readBytes(regAddr, &getByte, 1);
        return (getByte);
    }

    void IRAM_ATTR readBytes(uint8_t regAddr, uint8_t *out, uint8_t len) {
        SPI_beginTransaction();
        SPI.transfer(regAddr);
        for (uint8_t idx = 0; idx < len; ++idx) {
            out[idx] = SPI.transfer(0x00);
        }
        SPI_endTransaction();
    }

    bool IRAM_ATTR writeByte(uint8_t regAddr, uint8_t data, bool check) {
        return writeBytes(regAddr, &data, 1, check);
    }

    auto IRAM_ATTR writeBytes(uint8_t regAddr, uint8_t *in, uint8_t len, bool check) -> bool {
        SPI_beginTransaction();
        SPI.write(regAddr | SPI_Write);
        for (uint8_t idx = 0; idx < len; ++idx) {
            SPI.write(in[idx]);
        }
        SPI_endTransaction();

        if (check) {
            SPI_beginTransaction();
            SPI.transfer(regAddr);
            for (uint8_t idx = 0; idx < len; ++idx) {
                uint8_t getByte = SPI.transfer(0x00);
                if (in[idx] != getByte) {
                    SPI_endTransaction();
                    return false;
                }
            }
            SPI_endTransaction();
        }

        return true;
    }

    uint16_t IRAM_ATTR readWord(uint8_t regAddr) {
        uint8_t lowByte = readByte(regAddr);
        uint8_t highByte = readByte(regAddr + 1);
        return (highByte << 8) | lowByte;
    }

    void IRAM_ATTR writeWord(uint8_t regAddr, uint16_t value) {
        writeByte(regAddr, value >> 8);
        writeByte(regAddr + 1, value & 0xFF);
    }

    bool IRAM_ATTR inStdbyOrSleep() {
        uint8_t data = readByte(REG_OPMODE);
        data &= ~RF_OPMODE_MASK;
        if ((data == RF_OPMODE_SLEEP) || (data == RF_OPMODE_STANDBY))
            return true;
        return false;
    }

    bool IRAM_ATTR setCarrier(Carrier param, uint32_t value) {
        uint32_t tmpVal;
        uint8_t out[4];
        regBandWidth bw{};

        if (!inStdbyOrSleep())
            if (param != Carrier::Frequency)
                return false;

        switch (param) {
            case Carrier::Frequency:
                tmpVal = static_cast<uint32_t>((static_cast<float_t>(value) / FXOSC) * (1 << 19));
                out[0] = (tmpVal & 0x00ff0000) >> 16;
                out[1] = (tmpVal & 0x0000ff00) >> 8;
                out[2] = (tmpVal & 0x000000ff);
                writeBytes(REG_FRFMSB, out, 3);
                break;
            case Carrier::Bandwidth:
                bw = bwRegs(value);
                writeByte(REG_RXBW, bw.Mant | bw.Exp);
                writeByte(REG_AFCBW, bw.Mant | bw.Exp);
                break;
            case Carrier::Deviation:
                tmpVal = static_cast<uint32_t>((static_cast<float_t>(value) / FXOSC) * (1 << 19));
                out[0] = (tmpVal & 0x0000ff00) >> 8;
                out[1] = (tmpVal & 0x000000ff);
                writeBytes(REG_FDEVMSB, out, 2);
                break;
            case Carrier::Modulation:
                switch (value) {
                    case Modulation::FSK: {
                        uint8_t rfOpMode = readByte(REG_OPMODE);
                        rfOpMode &= RF_OPMODE_LONGRANGEMODE_MASK;
                        rfOpMode |= RF_OPMODE_LONGRANGEMODE_OFF;
                        rfOpMode &= RF_OPMODE_MODULATIONTYPE_MASK;
                        rfOpMode |= RF_OPMODE_MODULATIONTYPE_FSK;
                        rfOpMode &= RF_OPMODE_MASK;
                        rfOpMode |= RF_OPMODE_STANDBY;
                        rfOpMode &= ~0x08;
                        writeByte(REG_OPMODE, rfOpMode);
                        break;
                    }
                    case Modulation::LoRa:
                    case Modulation::OOK:
                    default: break;
                }
                break;
            case Carrier::Bitrate:
                tmpVal = FXOSC / value;
                out[0] = (tmpVal & 0x0000ff00) >> 8;
                out[1] = (tmpVal & 0x000000ff);
                writeBytes(REG_BITRATEMSB, out, 2);
                break;
        }

        return true;
    }

    regBandWidth bwRegs(uint8_t bandwidth) {
        for (auto &it: __bw)
            if (it.first == bandwidth)
                return it.second;
        return __bw.rbegin()->second;
    }

    void dump() {}
    void dumpReal() {}
    int dump_fsk_registers(const uint8_t *regs) { return 0; }
}
#endif
