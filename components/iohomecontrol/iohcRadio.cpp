/*
   Copyright (c) 2024. CRIDP https://github.com/cridp

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

           http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#include <driver/gpio.h>
#include <rom/ets_sys.h>
#include <map>
#include "esp_log.h"
#include <queue>
#include <string>
#include <cstdlib>  // std::to_string

#include "iohcRadio.h"
#include <utility>
#include "utils.h"

// ---------------------------------------------------------------------------
// setCrashMarker stub — only available on custom firmware builds that define
// it externally. Fall back to a no-op so the ESP-IDF build compiles cleanly.
// ---------------------------------------------------------------------------
#ifndef setCrashMarker
#define setCrashMarker(msg) ((void)0)
#endif

// Convenience wrappers so the rest of the file reads clearly.
static inline uint32_t millis()  { return (uint32_t)(esp_timer_get_time() / 1000ULL); }
static inline uint32_t micros()  { return (uint32_t)(esp_timer_get_time()); }
static inline void delayMicroseconds(uint32_t us) { ets_delay_us(us); }

static const char *TAG = "iohcRadio";

inline void addLogMessage(const std::string& msg) {
    ESP_LOGD(TAG, "%s", msg.c_str());
}

#define LONG_PREAMBLE_MS 1920
#define SHORT_PREAMBLE_MS 40

namespace IOHC {
    static void radioTickerTaskLoop(void *arg) {
        auto *radio = static_cast<iohcRadio *>(arg);
        const TickType_t interval = pdMS_TO_TICKS(1);
        while (true) {
            iohcRadio::tickerCounter(radio);
            vTaskDelay(interval);
        }
    }

    iohcRadio *iohcRadio::_iohcRadio = nullptr;
    volatile unsigned long iohcRadio::_g_payload_millis = 0L;
    uint8_t iohcRadio::_flags[2] = {0, 0};
    volatile bool iohcRadio::send_lock = false;
    volatile iohcRadio::RadioState iohcRadio::radioState = iohcRadio::RadioState::IDLE;
    TaskHandle_t iohcRadio::txTaskHandle = nullptr;
    volatile bool iohcRadio::txComplete = false;
    volatile bool iohcRadio::txBatchActive = false;


    TaskHandle_t handle_interrupt;
    TaskHandle_t callbackTask = NULL;
    QueueHandle_t callbackQueue = NULL;
    struct Callback {
        IohcPacketDelegate *callback;
        iohcPacket *packet;
    };

    static volatile uint32_t twoWPayloadIrqCount = 0;
    static volatile uint32_t twoWSyncIrqCount = 0;
    static uint32_t lastTwoWScanDiagMs = 0;
    static uint32_t lastTwoWScanSummaryMs = 0;
    static uint32_t lastInvalidRxLogMs = 0;
    static uint32_t lastDiagSyncCount = 0;
    static uint32_t lastDiagPayloadCount = 0;
    static uint32_t lastHandledPayloadIrqCount = 0;
    static uint8_t lastDiagFreqIdx = 0xff;

    static std::string twoWRadioDiag(uint32_t frequency, uint8_t irq1, uint8_t irq2) {
#if defined(RADIO_SX127X)
        return " freq=" + std::to_string(frequency) +
               " dio0=" + std::to_string(gpio_get_level((gpio_num_t)RADIO_PACKET_AVAIL)) +
               " dio2=" + std::to_string(gpio_get_level((gpio_num_t)RADIO_PREAMBLE_DETECTED)) +
               " irq_sync=" + std::to_string(twoWSyncIrqCount) +
               " irq_payload=" + std::to_string(twoWPayloadIrqCount) +
               " irq1=0x" + to_hex_str(irq1) +
               " irq2=0x" + to_hex_str(irq2) +
               " rssi=" + std::to_string(Radio::readByte(REG_RSSIVALUE)) +
               " op=0x" + to_hex_str(Radio::readByte(REG_OPMODE)) +
               " sync=0x" + to_hex_str(Radio::readByte(REG_SYNCCONFIG)) +
               " rx=0x" + to_hex_str(Radio::readByte(REG_RXCONFIG)) +
               " dioMap=0x" + to_hex_str(Radio::readByte(REG_DIOMAPPING1)) +
               "/0x" + to_hex_str(Radio::readByte(REG_DIOMAPPING2));
#else
        return "";
#endif
    }

    static bool shouldStartTwoWListenAfterTx(const iohcPacket *packet) {
        if (!packet || packet->payload.packet.header.CtrlByte1.asStruct.Protocol != 0) {
            return false;
        }

        // During pairing, early 2W discovery packets must not switch the radio
        // into fast FHSS scan. Start that window only after key transfer.
        return packet->payload.packet.header.cmd == 0x32;
    }
    /**
     * The function `handle_interrupt_task` waits for a notification and then calls the `tickerCounter`
     * function if certain conditions are met.
     */
    void IRAM_ATTR handle_interrupt_task(void *pvParameters) {
        static uint32_t thread_notification;
        const TickType_t xMaxBlockTime = pdMS_TO_TICKS(655 * 4);
        while (true) {
            thread_notification = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
            if (thread_notification &&
                (iohcRadio::radioState == iohcRadio::RadioState::PAYLOAD ||
                 iohcRadio::radioState == iohcRadio::RadioState::PREAMBLE)) {
                iohcRadio::tickerCounter((iohcRadio *) pvParameters);
            }
        }
    }

    void IRAM_ATTR handle_payload_interrupt_fromisr(void */*arg*/) {
        if (!gpio_get_level((gpio_num_t)RADIO_PACKET_AVAIL)) {
            return;
        }

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (iohcRadio::txBatchActive) {
            iohcRadio::txComplete = true;
            if (iohcRadio::txTaskHandle) {
                vTaskNotifyGiveFromISR(iohcRadio::txTaskHandle, &xHigherPriorityTaskWoken);
            }
        } else {
            twoWPayloadIrqCount++;
            iohcRadio::setRadioState(iohcRadio::RadioState::PAYLOAD);
            vTaskNotifyGiveFromISR(handle_interrupt, &xHigherPriorityTaskWoken);
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    void IRAM_ATTR handle_sync_interrupt_fromisr(void */*arg*/) {
        if (iohcRadio::txBatchActive) {
            return;
        }

        const bool syncActive = gpio_get_level((gpio_num_t)RADIO_PREAMBLE_DETECTED);
        if (syncActive) {
            twoWSyncIrqCount++;
            iohcRadio::setRadioState(iohcRadio::RadioState::PREAMBLE);
        } else {
            iohcRadio::setRadioState(iohcRadio::RadioState::RX);
        }
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(handle_interrupt, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    void callbackTaskLoop(void *parameters) {
        Callback *callback = NULL;
        while (true) {
            if (xQueueReceive(callbackQueue, &callback, portMAX_DELAY) == pdPASS && callback != NULL) {
                (*callback->callback)(callback->packet);
                delete callback->packet;
                vPortFree(callback);
            }
        }
    }

    iohcRadio::iohcRadio() {
        _iohcRadio = this;

        callbackQueue = xQueueCreate(40, sizeof(struct Callback *));
        auto callbackTaskCode = xTaskCreatePinnedToCore(callbackTaskLoop, "CallbackTask", 4096, NULL, 5, &callbackTask, 0);
        if (callbackTaskCode != pdPASS || callbackQueue == NULL) {
            printf("ERROR: Can't create callback-task or corresponding queue %d\n", callbackTaskCode);
            return;
        }

        printf("Starting Interrupt Handler...\n");
        BaseType_t task_code = xTaskCreatePinnedToCore(handle_interrupt_task, "handle_interrupt_task", 8192,
                                                       this, 4,
                                                       &handle_interrupt, xPortGetCoreID());
        if (task_code != pdPASS) {
            printf("ERROR STATEMACHINE Can't create task %d\n", task_code);
            return;
        }
    }

    bool iohcRadio::init(int nss, int rst, int sck, int miso, int mosi, uint32_t freq) {
        Radio::initHardware(nss, rst, sck, miso, mosi);
        Radio::calibrate();
        Radio::initRegisters(MAX_FRAME_LEN);
        Radio::setCarrier(Radio::Carrier::Deviation, 19200);
        Radio::setCarrier(Radio::Carrier::Bitrate, 38400);
        Radio::setCarrier(Radio::Carrier::Bandwidth, 250);
        Radio::setCarrier(Radio::Carrier::Modulation, Radio::Modulation::FSK);

#if defined(RADIO_SX127X)
        // Install GPIO ISR service (safe to call multiple times)
        gpio_install_isr_service(0);

        // DIO0 → payload interrupt (RISING edge only)
        gpio_set_direction((gpio_num_t)RADIO_DIO0_PIN, GPIO_MODE_INPUT);
        gpio_set_intr_type((gpio_num_t)RADIO_DIO0_PIN, GPIO_INTR_POSEDGE);
        gpio_isr_handler_add((gpio_num_t)RADIO_DIO0_PIN, handle_payload_interrupt_fromisr, nullptr);

        // DIO2 → sync/preamble interrupt (both edges)
        gpio_set_direction((gpio_num_t)RADIO_DIO2_PIN, GPIO_MODE_INPUT);
        gpio_set_intr_type((gpio_num_t)RADIO_DIO2_PIN, GPIO_INTR_ANYEDGE);
        gpio_isr_handler_add((gpio_num_t)RADIO_DIO2_PIN, handle_sync_interrupt_fromisr, nullptr);
#elif defined(CC1101)
        gpio_install_isr_service(0);
        gpio_set_direction((gpio_num_t)RADIO_PREAMBLE_DETECTED, GPIO_MODE_INPUT);
        gpio_set_intr_type((gpio_num_t)RADIO_PREAMBLE_DETECTED, GPIO_INTR_POSEDGE);
        gpio_isr_handler_add((gpio_num_t)RADIO_PREAMBLE_DETECTED, [](void*){ IOHC::iohcRadio::i_preamble(); }, nullptr);
#endif
        return true;
    }

    int iohcRadio::getRSSI() {
#if defined(RADIO_SX127X)
        return Radio::readByte(REG_RSSIVALUE);
#else
        return 0;
#endif
    }

    iohcRadio *iohcRadio::getInstance() {
        if (!_iohcRadio)
            _iohcRadio = new iohcRadio();
        return _iohcRadio;
    }

    void iohcRadio::start(uint8_t num_freqs, uint32_t *scan_freqs, uint32_t scanTimeUs,
                          IohcPacketDelegate rxCallback, IohcPacketDelegate txCallback) {
        this->num_freqs = num_freqs;
        this->configuredNumFreqs = num_freqs;
        this->scan_freqs = scan_freqs;
        this->normalScanTimeUs = scanTimeUs ? scanTimeUs : DEFAULT_SCAN_INTERVAL_US;
        this->scanTimeUs = this->normalScanTimeUs;
        this->num_freqs = 1;
        this->currentFreqIdx = 0;
        this->twoWScanActive = false;
        this->twoWScanUntilMs = 0;
        this->rxCB = std::move(rxCallback);
        this->txCB = std::move(txCallback);

        Radio::clearBuffer();
        Radio::clearFlags();
        Radio::setCarrier(Radio::Carrier::Frequency, scan_freqs[0]);
        Radio::setRx();
        setRadioState(RadioState::RX);
        ESP_LOGI(TAG, "Radio RX normal channels=%u dwell_us=%u freq=%u 2w_available=%u",
                 this->num_freqs, this->scanTimeUs, scan_freqs[0], this->configuredNumFreqs);
#if defined(ESP32)
        if (!tickerTaskHandle &&
            xTaskCreatePinnedToCore(radioTickerTaskLoop, "radioTicker", 4096, this,
                                    4, &tickerTaskHandle, 0) != pdPASS) {
            tickerTaskHandle = nullptr;
            addLogMessage("Failed to create radio ticker task");
        }
#else
        TickTimer.attach_us(SM_GRANULARITY_US, tickerCounter, this);
#endif
    }

    void iohcRadio::startTwoWScan(uint32_t windowMs, uint32_t dwellUs) {
#if defined(RADIO_SX127X)
        setCrashMarker("radio: startTwoWScan enter");
        if (!scan_freqs || configuredNumFreqs == 0) {
            addLogMessage("2W scan not started; radio frequencies not configured");
            return;
        }
        twoWScanActive = true;
        twoWScanUntilMs = millis() + windowMs;
        num_freqs = configuredNumFreqs;
        scanTimeUs = dwellUs ? dwellUs : TWOW_SCAN_INTERVAL_US;
        if (num_freqs > 1 && scanTimeUs == normalScanTimeUs) {
            scanTimeUs = TWOW_SCAN_INTERVAL_US;
        }
        currentFreqIdx = 0;
        tickCounter = 0;
        preCounter = 0;
        twoWPayloadIrqCount = 0;
        twoWSyncIrqCount = 0;
        lastHandledPayloadIrqCount = 0;
        lastTwoWScanDiagMs = millis();
        lastTwoWScanSummaryMs = millis();
        lastInvalidRxLogMs = 0;
        lastDiagSyncCount = 0;
        lastDiagPayloadCount = 0;
        lastDiagFreqIdx = 0xff;
        Radio::clearBuffer();
        Radio::clearFlags();
        Radio::setCarrier(Radio::Carrier::Frequency, scan_freqs[currentFreqIdx]);
        Radio::setRx();
        ets_delay_us(500);
        setRadioState(RadioState::RX);
        std::string scanMsg = "2W scan started channels=" + std::to_string(num_freqs) +
                         " dwell_us=" + std::to_string(scanTimeUs) +
                         " window_ms=" + std::to_string(windowMs) +
                         " freqs=";
        for (uint8_t idx = 0; idx < num_freqs; ++idx) {
            if (idx) scanMsg += ",";
            scanMsg += std::to_string(scan_freqs[idx]);
        }
        addLogMessage(scanMsg + twoWRadioDiag(scan_freqs[currentFreqIdx],
                                              Radio::readByte(REG_IRQFLAGS1),
                                              Radio::readByte(REG_IRQFLAGS2)));
        setCrashMarker("radio: startTwoWScan done");
#endif
    }

    void iohcRadio::stopTwoWScan() {
#if defined(RADIO_SX127X)
        if (!scan_freqs) {
            return;
        }
        twoWScanActive = false;
        twoWScanUntilMs = 0;
        num_freqs = 1;
        scanTimeUs = normalScanTimeUs;
        currentFreqIdx = 0;
        tickCounter = 0;
        preCounter = 0;
        Radio::clearBuffer();
        Radio::clearFlags();
        Radio::setCarrier(Radio::Carrier::Frequency, scan_freqs[currentFreqIdx]);
        Radio::setRx();
        setRadioState(RadioState::RX);
        addLogMessage("2W scan stopped; RX back to " + std::to_string(scan_freqs[currentFreqIdx]));
#endif
    }

    void IRAM_ATTR iohcRadio::tickerCounter(iohcRadio *radio) {
#if defined(RADIO_SX127X)
        if (txBatchActive) {
            return;
        }

        Radio::readBytes(REG_IRQFLAGS1, _flags, sizeof(_flags));

        if (radioState == iohcRadio::RadioState::PAYLOAD) {
            if (_flags[0] & RF_IRQFLAGS1_TXREADY) {
                Radio::clearFlags();
                if (radioState != iohcRadio::RadioState::TX) {
                    Radio::setRx();
                    radio->setRadioState(iohcRadio::RadioState::RX);
                }
                return;
            }
            radio->receive(false);
            Radio::clearFlags();
            Radio::setRx();
            radio->setRadioState(iohcRadio::RadioState::RX);
            radio->tickCounter = 0;
            radio->preCounter = 0;
            return;
        }

        if (radioState == iohcRadio::RadioState::PREAMBLE) {
            if (_flags[1] & RF_IRQFLAGS2_PAYLOADREADY) {
                radio->receive(false);
                Radio::clearFlags();
                Radio::setRx();
                radio->setRadioState(iohcRadio::RadioState::RX);
                radio->tickCounter = 0;
                radio->preCounter = 0;
                return;
            }
            radio->tickCounter = 0;
            radio->preCounter = radio->preCounter + 1;

            if ((radio->preCounter * SM_GRANULARITY_US) >= SM_PREAMBLE_RECOVERY_TIMEOUT_US) {
                Radio::clearFlags();
                Radio::setRx();
                radio->setRadioState(iohcRadio::RadioState::RX);
                radio->preCounter = 0;
            }
        }

        if (radioState != iohcRadio::RadioState::RX) return;

        if (radio->twoWScanActive &&
            static_cast<long>(millis() - radio->twoWScanUntilMs) >= 0) {
            radio->stopTwoWScan();
            return;
        }

        if (radio->twoWScanActive &&
            (_flags[1] & RF_IRQFLAGS2_PAYLOADREADY) &&
            twoWPayloadIrqCount != lastHandledPayloadIrqCount) {
            lastHandledPayloadIrqCount = twoWPayloadIrqCount;
            addLogMessage("2W scan payload-ready fallback" +
                          twoWRadioDiag(radio->scan_freqs[radio->currentFreqIdx],
                                        _flags[0], _flags[1]));
            radio->receive(false);
            Radio::clearFlags();
            Radio::setRx();
            radio->setRadioState(iohcRadio::RadioState::RX);
            radio->tickCounter = 0;
            radio->preCounter = 0;
            return;
        }

        if (radio->twoWScanActive) {
            const uint32_t nowMs = millis();
            const uint8_t opMode = Radio::readByte(REG_OPMODE) & ~RF_OPMODE_MASK;
            if (opMode == RF_OPMODE_SYNTHESIZER_RX) {
                Radio::setRx();
            }
            const bool irqChanged = twoWSyncIrqCount != lastDiagSyncCount ||
                                    twoWPayloadIrqCount != lastDiagPayloadCount;
            const bool summaryDue = nowMs - lastTwoWScanSummaryMs >= 10000UL;

            if (irqChanged && nowMs - lastTwoWScanDiagMs >= 1000UL) {
                lastTwoWScanDiagMs = nowMs;
                lastDiagSyncCount = twoWSyncIrqCount;
                lastDiagPayloadCount = twoWPayloadIrqCount;
                lastDiagFreqIdx = radio->currentFreqIdx;
                addLogMessage("2W scan diag" + twoWRadioDiag(radio->scan_freqs[radio->currentFreqIdx],
                                                             _flags[0], _flags[1]));
            } else if (summaryDue) {
                lastTwoWScanSummaryMs = nowMs;
                lastDiagFreqIdx = radio->currentFreqIdx;
                addLogMessage("2W scan listening channels=" + std::to_string(radio->num_freqs) +
                              twoWRadioDiag(radio->scan_freqs[radio->currentFreqIdx],
                                            _flags[0], _flags[1]));
            }
        }

        radio->tickCounter = radio->tickCounter + 1;
        if (radio->tickCounter * SM_GRANULARITY_US < radio->scanTimeUs) return;

        radio->tickCounter = 0;

        if (radio->num_freqs == 1) return;

        radio->currentFreqIdx += 1;
        if (radio->currentFreqIdx >= radio->num_freqs)
            radio->currentFreqIdx = 0;

        Radio::setCarrier(Radio::Carrier::Frequency, radio->scan_freqs[radio->currentFreqIdx]);
        Radio::clearFlags();
        Radio::setRx();
        radio->setRadioState(iohcRadio::RadioState::RX);

#elif defined(CC1101)
        if (__g_preamble){
            radio->receive();
            radio->tickCounter = 0;
            radio->preCounter = 0;
            return;
        }

        if (radioState != iohcRadio::RadioState::RX)
            return;

        if ((++radio->tickCounter * SM_GRANULARITY_US) < radio->scanTimeUs)
            return;
#endif
    }

void iohcRadio::queueSend(std::vector<iohcPacket *> &iohcTx) {
    if (iohcTx.empty()) {
        return;
    }
    sendQueue.push(std::move(iohcTx));
    ets_printf("TX: Queued send batch. Queue depth=%d\n", static_cast<int>(sendQueue.size()));
}

void iohcRadio::startQueuedSend() {
    if (radioState == RadioState::TX || packets2send.size() > 0 || sendQueue.empty()) {
        return;
    }

    setCrashMarker("radio: startQueuedSend");
    packets2send = std::move(sendQueue.front());
    sendQueue.pop();
    txCounter = 0;
    txComplete = false;
    txBatchActive = true;
    resumeTwoWScanAfterTx = twoWScanActive && twoWScanUntilMs > millis();
    resumeTwoWScanUntilMs = resumeTwoWScanAfterTx ? twoWScanUntilMs : 0;
    resumeTwoWScanWindowMs = resumeTwoWScanAfterTx
        ? static_cast<uint32_t>(twoWScanUntilMs - millis())
        : 0;
    resumeTwoWScanDwellUs = scanTimeUs ? scanTimeUs : TWOW_SCAN_INTERVAL_US;
    currentBatchHas2W = false;
    for (auto *packet : packets2send) {
        if (shouldStartTwoWListenAfterTx(packet)) {
            currentBatchHas2W = true;
            break;
        }
    }
    ets_printf("TX: Preparing %d packet(s), start_2w_listen=%d resume_2w_scan=%d\n",
               packets2send.size(),
               currentBatchHas2W ? 1 : 0,
               resumeTwoWScanAfterTx ? 1 : 0);
    setRadioState(RadioState::TX);

    auto packet = packets2send[txCounter];

    Radio::setPreambleLength(LONG_PREAMBLE_MS);
    ets_printf("TX: Using LONG preamble (%d ms)\n", LONG_PREAMBLE_MS);

    Radio::setStandby();
    if (packet->frequency != 0) {
        Radio::setCarrier(Radio::Carrier::Frequency, packet->frequency);
    }
    Radio::clearFlags();
    Radio::writeBytes(REG_FIFO, packet->payload.buffer, packet->buffer_length);
    Radio::setTx();
    txStartedAtUs = esp_timer_get_time();

    ets_printf("TX: Sent first packet (%d repeats) at %llu us\n", packet->repeat, esp_timer_get_time());

    Sender.attach_ms(packet->repeatTime, &iohcRadio::onTxTicker, (void*)this);
}

void iohcRadio::send(iohcPacket *packet) {
    std::vector<iohcPacket *> packets = { packet };
    send(packets);
}

void iohcRadio::send(std::vector<iohcPacket *> &iohcTx) {
    queueSend(iohcTx);
    startQueuedSend();
}

void iohcRadio::onTxTicker(void *arg) {
    iohcRadio *radio = (iohcRadio *)arg;
    auto packet = radio->packets2send[radio->txCounter];

    uint8_t irqFlags2 = Radio::readByte(0x3F);
    if (irqFlags2 & 0x08) {
        ets_printf("FSK: Detected PacketSent (TXDONE) via register (ISR missed?)\n");
        Radio::writeByte(0x3F, 0x08);
        iohcRadio::txComplete = true;
    }
    if (!radio->txComplete &&
        esp_timer_get_time() - radio->txStartedAtUs > 2000000ULL) {
        ets_printf("TX: PacketSent timeout; forcing completion\n");
        iohcRadio::txComplete = true;
    }

    if (!radio->txComplete) {
        ets_printf("TX: Waiting for TXDONE... (state=%s)\n", radioStateToString(radio->radioState));
        return;
    }

    ESP_LOGD(TAG, "TXDONE flag set, ready to send repeat or next packet.");

    if (packet->repeat > 0) {
        packet->repeat--;
        ets_printf("TX: Repeating current packet (%d repeats left)\n", packet->repeat);
    } else {
        radio->sent(packet);
        radio->txCounter++;

        if (radio->txCounter == radio->packets2send.size()) {
            ets_printf("TX: All packets sent. Stopping Ticker.\n");
            const bool start2WListen = radio->currentBatchHas2W;
            radio->Sender.detach();
            radio->packets2send.clear();
            radio->currentBatchHas2W = false;
            radio->txBatchActive = false;
            const unsigned long nowMs = millis();
            const bool resumePairingScan = radio->resumeTwoWScanAfterTx &&
                                           radio->resumeTwoWScanUntilMs > nowMs;
            if (resumePairingScan) {
                const uint32_t remainingMs =
                    static_cast<uint32_t>(radio->resumeTwoWScanUntilMs - nowMs);
                radio->startTwoWScan(remainingMs, radio->resumeTwoWScanDwellUs);
            } else if (start2WListen) {
                radio->startTwoWScan();
            } else if (radio->twoWScanActive) {
                radio->stopTwoWScan();
            } else {
                Radio::setRx();
                radio->setRadioState(RadioState::RX);
            }
            radio->resumeTwoWScanAfterTx = false;
            radio->resumeTwoWScanUntilMs = 0;
            radio->resumeTwoWScanWindowMs = 0;
            radio->startQueuedSend();
            return;
        }

        packet = radio->packets2send[radio->txCounter];
        ets_printf("TX: Moving to next packet %d/%d (repeat=%d)\n",
                    radio->txCounter + 1,
                    radio->packets2send.size(),
                    packet->repeat);
    }

    radio->txComplete = false;
    radio->setRadioState(RadioState::TX);
    Radio::setStandby();
    if (packet->frequency != 0) {
        Radio::setCarrier(Radio::Carrier::Frequency, packet->frequency);
    }
    Radio::clearFlags();
    Radio::writeBytes(REG_FIFO, packet->payload.buffer, packet->buffer_length);
    Radio::setTx();
    radio->txStartedAtUs = esp_timer_get_time();

    ets_printf("TX: Sent packet %d/%d at %llu us\n",
               radio->txCounter + 1,
               radio->packets2send.size(),
               esp_timer_get_time());
}

bool queueCallback(IohcPacketDelegate* callback, iohcPacket* packet) {
    Callback *callbackData = (Callback*) pvPortMalloc(sizeof(Callback));
    if (callbackData == NULL) {
        return false;
    }

    callbackData->callback = callback;
    callbackData->packet = packet;

    if (xQueueSendToBack(callbackQueue, &callbackData, pdMS_TO_TICKS(25)) != pdPASS) {
        addLogMessage("Radio callback queue full; dropping packet");
        vPortFree(callbackData);
        return false;
    }
    return true;
}

    bool IRAM_ATTR iohcRadio::sent(iohcPacket *packet) {
        bool ret = false;
        if (packet) {
            packetStamp.store(esp_timer_get_time());
            packet->decode(true);
            addLogMessage(std::string(packet->decodeToString(true).c_str()));
#if defined(MQTT)
            publishRadioLogEvent(packet, "TX");
#endif
        }
        if (txCB && !queueCallback(&txCB, packet)) {
            delete packet;
        }
        return ret;
    }

    bool IRAM_ATTR iohcRadio::receive(bool stats) {
        gpio_set_level((gpio_num_t)RX_LED, gpio_get_level((gpio_num_t)RX_LED) ^ 1);
        auto iohc = new iohcPacket;
        iohc->buffer_length = 0;
        iohc->frequency = scan_freqs[currentFreqIdx];

        _g_payload_millis = esp_timer_get_time();
        packetStamp.store(_g_payload_millis);
#if defined(RADIO_SX127X)
        if (stats) {
            iohc->rssi = static_cast<float>(Radio::readByte(REG_RSSIVALUE)) / -2.0f;
            int16_t thres = Radio::readByte(REG_RSSITHRESH);
            iohc->snr = iohc->rssi > thres ? 0 : (thres - iohc->rssi);
            int16_t f = (uint16_t) Radio::readByte(REG_AFCMSB);
            f = (f << 8) | (uint16_t) Radio::readByte(REG_AFCLSB);
            iohc->afc = f * 61.0;
        }
#elif defined(CC1101)
        __g_preamble = false;

        uint8_t tmprssi=Radio::SPIgetRegValue(REG_RSSI);
        if (tmprssi>=128)
            iohc->rssi = (float)((tmprssi-256)/2)-74;
        else
            iohc->rssi = (float)(tmprssi/2)-74;

        uint8_t bytesInFIFO = Radio::SPIgetRegValue(REG_RXBYTES, 6, 0);
        size_t readBytes = 0;
        uint32_t lastPop = millis();
#endif

#if defined(RADIO_SX127X)

        bool rxOverflow = false;
        uint16_t fifoCount = 0;
        uint32_t emptySinceUs = 0;
        const uint32_t readStartedUs = micros();
        const uint32_t fifoStableEmptyUs = twoWScanActive ? 3500 : 1200;
        const uint32_t fifoReadTimeoutUs = twoWScanActive ? 30000 : 12000;
        const uint8_t irqFlags1Before = Radio::readByte(REG_IRQFLAGS1);
        const uint8_t irqFlags2Before = Radio::readByte(REG_IRQFLAGS2);

        while ((micros() - readStartedUs) < fifoReadTimeoutUs) {
            if (Radio::dataAvail()) {
                const uint8_t value = Radio::readByte(REG_FIFO);
                fifoCount++;
                emptySinceUs = 0;
                if (iohc->buffer_length < MAX_FRAME_LEN) {
                    iohc->payload.buffer[iohc->buffer_length++] = value;
                } else {
                    rxOverflow = true;
                }
                if (iohc->buffer_length >= 1) {
                    const uint8_t expectedLength =
                        iohc->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1;
                    if (expectedLength >= sizeof(_header) &&
                        expectedLength <= MAX_FRAME_LEN &&
                        iohc->buffer_length >= expectedLength) {
                        break;
                    }
                }
                continue;
            }

            if (emptySinceUs == 0) {
                emptySinceUs = micros();
            } else if ((micros() - emptySinceUs) >= fifoStableEmptyUs) {
                break;
            }

            ets_delay_us(100);
        }

        const uint8_t irqFlags1After = Radio::readByte(REG_IRQFLAGS1);
        const uint8_t irqFlags2After = Radio::readByte(REG_IRQFLAGS2);

#elif defined(CC1101)
        uint8_t lenghtFrameCoded = 0xFF;
        uint8_t tmpBuffer[64]={0x00};
        while (readBytes < lenghtFrameCoded) {
            if ( (readBytes>=1) && (lenghtFrameCoded==0xFF) ){
                lenghtFrame = (Radio::reverseByte( ((uint8_t)(tmpBuffer[0]<<4) | (uint8_t)(tmpBuffer[1]>>4)))) & 0b00011111;
                lenghtFrameCoded = ((lenghtFrame + 2 + 1)*8) + ((lenghtFrame + 2 + 1)*2);
                lenghtFrameCoded = ceil((float)lenghtFrameCoded/8);
                Radio::setPktLenght(lenghtFrameCoded);
            }

            if (bytesInFIFO == 0) {
                if (millis() - lastPop > 5) {
                    break;
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1));
                    bytesInFIFO = Radio::SPIgetRegValue(REG_RXBYTES, 6, 0);
                    continue;
                }
            }

            uint8_t bytesToRead = (((uint8_t)(lenghtFrameCoded - readBytes))<(bytesInFIFO)?((uint8_t)(lenghtFrameCoded - readBytes)):(bytesInFIFO));
            Radio::SPIreadRegisterBurst(REG_FIFO, bytesToRead, &(tmpBuffer[readBytes]));
            readBytes += bytesToRead;
            lastPop = millis();
            bytesInFIFO = Radio::SPIgetRegValue(REG_RXBYTES, 6, 0);
        }

        frmErr=true;
        if (lenghtFrameCoded<255){
            int8_t lenFuncDecodeFrame = Radio::decodeFrame(tmpBuffer, lenghtFrameCoded);
            if (lenFuncDecodeFrame>0 && lenFuncDecodeFrame<=MAX_FRAME_LEN){
                if (iohcUtils::radioPacketComputeCrc(tmpBuffer, lenFuncDecodeFrame) == 0 ){
                    iohc->buffer_length = lenFuncDecodeFrame;
                    memcpy(iohc->payload.buffer, tmpBuffer, lenFuncDecodeFrame);
                    frmErr=false;
                }
            }
        }

        if (Radio::SPIgetRegValue(REG_MCSM1, 3, 2) == RF_RXOFF_IDLE) {
            Radio::SPIsendCommand(CMD_IDLE);
            Radio::SPIsendCommand(CMD_FLUSH_RX | CMD_READ);
        }

        Radio::SPIsendCommand(CMD_RX);
        setRadioState(iohcRadio::RadioState::RX);

#endif

        if (iohc->buffer_length == 0) {
            lastHandledPayloadIrqCount = twoWPayloadIrqCount;
            Radio::clearBuffer();
            Radio::clearFlags();
            Radio::setRx();
            setRadioState(iohcRadio::RadioState::RX);
            delete iohc;
            iohc = nullptr;
            gpio_set_level((gpio_num_t)RX_LED, 0);
            return false;
        }

        const uint8_t msgLen = (iohc->buffer_length > 0)
            ? iohc->payload.packet.header.CtrlByte1.asStruct.MsgLen
            : 0;
        const uint8_t expectedLength = msgLen + 1;
        const bool isTwoW = iohc->payload.packet.header.CtrlByte1.asStruct.Protocol == 0;

        if (!rxOverflow &&
            expectedLength >= sizeof(_header) &&
            expectedLength <= MAX_FRAME_LEN &&
            iohc->buffer_length > expectedLength) {
            ets_printf("RX: Trimming trailing FIFO bytes from %u to %u\n",
                       iohc->buffer_length, expectedLength);
            iohc->buffer_length = expectedLength;
        }

        if (rxOverflow || iohc->buffer_length < sizeof(_header) ||
            expectedLength < sizeof(_header) || expectedLength > MAX_FRAME_LEN ||
            iohc->buffer_length != expectedLength) {
            lastHandledPayloadIrqCount = twoWPayloadIrqCount;
            const uint8_t safeRawLen = iohc->buffer_length <= MAX_FRAME_LEN ? iohc->buffer_length : MAX_FRAME_LEN;
            const std::string raw = bytesToHexString(iohc->payload.buffer, safeRawLen).c_str();
            const std::string details = "len=" + std::to_string(iohc->buffer_length) +
                                   " expected=" + std::to_string(expectedLength) +
                                   " overflow=" + std::string(rxOverflow ? "yes" : "no") +
                                   " raw=" + raw;
            const bool logInvalidRx = !twoWScanActive || millis() - lastInvalidRxLogMs >= 5000UL;
            if (logInvalidRx) {
                lastInvalidRxLogMs = millis();
                addLogMessage("Radio RX invalid frame " + details);
            }
            Radio::clearBuffer();
            Radio::clearFlags();
            Radio::setRx();
            setRadioState(iohcRadio::RadioState::RX);
            delete iohc;
            iohc = nullptr;
            gpio_set_level((gpio_num_t)RX_LED, 0);
            return false;
        }
#if defined(RADIO_SX127X)
        if (isTwoW) {
            ets_printf("2W RX FIFO read=%u stored=%u overflow=%d irq1=%02x/%02x irq2=%02x/%02x expected=%d\n",
                       fifoCount, iohc->buffer_length, rxOverflow ? 1 : 0,
                       irqFlags1Before, irqFlags1After, irqFlags2Before, irqFlags2After,
                       expectedLength);
            addLogMessage("2W RX FIFO read=" + std::to_string(fifoCount) +
                          " stored=" + std::to_string(iohc->buffer_length) +
                          " overflow=" + std::string(rxOverflow ? "yes" : "no") +
                          " irq1=0x" + to_hex_str(irqFlags1Before) + "/0x" + to_hex_str(irqFlags1After) +
                          " irq2=0x" + to_hex_str(irqFlags2Before) + "/0x" + to_hex_str(irqFlags2After));
        }
#endif
        std::string rawMessage = "Radio RX len=" + std::to_string(iohc->buffer_length) +
                            " freq=" + std::to_string(iohc->frequency) +
                            " proto=" + std::string(iohc->payload.packet.header.CtrlByte1.asStruct.Protocol ? "1W" : "2W") +
                            " cmd=" + to_hex_str(iohc->payload.packet.header.cmd) +
                            " raw=" + bytesToHexString(iohc->payload.buffer, iohc->buffer_length).c_str();
        if (twoWScanActive && !isTwoW) {
            addLogMessage("1W RX during 2W scan " + rawMessage);
        } else {
            addLogMessage(rawMessage);
        }
        lastHandledPayloadIrqCount = twoWPayloadIrqCount;
        if (iohc->buffer_length > MAX_FRAME_LEN ||
            iohc->buffer_length != iohc->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1) {
            addLogMessage("Radio RX rejected before decode len=" + std::to_string(iohc->buffer_length) +
                          " expected=" + std::to_string(iohc->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1));
            Radio::clearBuffer();
            Radio::clearFlags();
            Radio::setRx();
            setRadioState(iohcRadio::RadioState::RX);
            delete iohc;
            iohc = nullptr;
            gpio_set_level((gpio_num_t)RX_LED, 0);
            return false;
        }
        iohc->decode(true);
        addLogMessage(std::string(iohc->decodeToString(true).c_str()));
        iohcPacket *receivedPacket = iohc;
        iohc = nullptr;
        if (rxCB) {
            setCrashMarker("radio: rx callback queued");
            if (!queueCallback(&rxCB, receivedPacket)) {
                delete receivedPacket;
            }
        } else {
            delete receivedPacket;
        }
        gpio_set_level((gpio_num_t)RX_LED, 0);
        return true;
    }

    void IRAM_ATTR iohcRadio::i_preamble() {
#if defined(RADIO_SX127X)
        bool preamble = gpio_get_level((gpio_num_t)RADIO_PREAMBLE_DETECTED);
#elif defined(CC1101)
        __g_preamble = true;
        bool preamble = __g_preamble;
#endif
        iohcRadio::setRadioState(preamble ? iohcRadio::RadioState::PREAMBLE : iohcRadio::RadioState::RX);
    }

    void IRAM_ATTR iohcRadio::i_payload() {
#if defined(RADIO_SX127X)
        bool payload = gpio_get_level((gpio_num_t)RADIO_PACKET_AVAIL);
        iohcRadio::setRadioState(payload ? iohcRadio::RadioState::PAYLOAD : iohcRadio::RadioState::RX);
#endif
    }

    const char* iohcRadio::radioStateToString(RadioState state) {
    switch (state) {
        case RadioState::IDLE:     return "IDLE";
        case RadioState::RX:       return "RX";
        case RadioState::TX:       return "TX";
        case RadioState::PREAMBLE: return "PREAMBLE";
        case RadioState::PAYLOAD:  return "PAYLOAD";
        case RadioState::LOCKED:   return "LOCKED";
        case RadioState::ERROR:    return "ERROR";
        default:                   return "UNKNOWN";
    }
    }

    void IRAM_ATTR iohcRadio::setRadioState(RadioState newState) {
        if (radioState == newState) {
            return;
        }
        radioState = newState;
        ets_printf("State: %s\n", radioStateToString(newState));
    }
}
