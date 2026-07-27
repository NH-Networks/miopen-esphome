#include "iohcRadio.h"
#include <esp32-hal-gpio.h>
#include <map>
#include "esp_log.h"
#include <queue>
#include <utility>
#include "log_buffer.h"

#define LONG_PREAMBLE_MS 1920
#define SHORT_PREAMBLE_MS 40

namespace IOHC {
    iohcRadio *iohcRadio::_iohcRadio = nullptr;
    volatile unsigned long iohcRadio::_g_payload_millis = 0L;
    uint8_t iohcRadio::_flags[2] = {0, 0};
    volatile bool iohcRadio::send_lock = false;
    volatile iohcRadio::RadioState iohcRadio::radioState = iohcRadio::RadioState::IDLE;
    volatile bool iohcRadio::txComplete = false;

    TaskHandle_t handle_interrupt;
    TaskHandle_t callbackTask = NULL;
    QueueHandle_t callbackQueue = NULL;
    struct Callback {
        IohcPacketDelegate *callback;
        iohcPacket *packet;
    };

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

    void IRAM_ATTR handle_interrupt_fromisr() {
        bool preamble = digitalRead(RADIO_PREAMBLE_DETECTED);
        bool payload = digitalRead(RADIO_PACKET_AVAIL);
        iohcRadio::txComplete = true;

        if (payload) {
            iohcRadio::setRadioState(iohcRadio::RadioState::PAYLOAD);
        } else if (preamble) {
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
        Radio::initHardware();
        Radio::calibrate();

        Radio::initRegisters(MAX_FRAME_LEN);
        Radio::setCarrier(Radio::Carrier::Deviation, 19200);
        Radio::setCarrier(Radio::Carrier::Bitrate, 38400);
        Radio::setCarrier(Radio::Carrier::Bandwidth, 250);
        Radio::setCarrier(Radio::Carrier::Modulation, Radio::Modulation::FSK);

#if defined(RADIO_SX127X)
        attachInterrupt(RADIO_DIO0_PIN, handle_interrupt_fromisr, RISING);
        attachInterrupt(RADIO_DIO2_PIN, handle_interrupt_fromisr, RISING);
#endif

        callbackQueue = xQueueCreate(20, sizeof(struct Callback *));
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

    iohcRadio *iohcRadio::getInstance() {
        if (!_iohcRadio)
            _iohcRadio = new iohcRadio();
        return _iohcRadio;
    }

    void iohcRadio::start(uint8_t num_freqs, uint32_t *scan_freqs, uint32_t scanTimeUs,
                          IohcPacketDelegate rxCallback, IohcPacketDelegate txCallback) {
        this->num_freqs = num_freqs;
        this->scan_freqs = scan_freqs;
        this->scanTimeUs = scanTimeUs ? scanTimeUs : DEFAULT_SCAN_INTERVAL_US;
        this->rxCB = std::move(rxCallback);
        this->txCB = std::move(txCallback);

        Radio::clearBuffer();
        Radio::clearFlags();
        Radio::setCarrier(Radio::Carrier::Frequency, scan_freqs[0]);
        Radio::setRx();
    }

    void IRAM_ATTR iohcRadio::tickerCounter(iohcRadio *radio) {
#if defined(RADIO_SX127X)
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
            radio->tickCounter = 0;
            radio->preCounter = 0;
            return;
        }

        if (radioState == iohcRadio::RadioState::PREAMBLE) {
            radio->tickCounter = 0;
            radio->preCounter = radio->preCounter + 1;
            if ((radio->preCounter * SM_GRANULARITY_US) >= SM_PREAMBLE_RECOVERY_TIMEOUT_US) {
                Radio::clearFlags();
                radio->preCounter = 0;
            }
        }

        if (radioState != iohcRadio::RadioState::RX) return;

        radio->tickCounter = radio->tickCounter + 1;
        if (radio->tickCounter * SM_GRANULARITY_US < radio->scanTimeUs) return;

        radio->tickCounter = 0;

        if (radio->num_freqs == 1) return;

        radio->currentFreqIdx += 1;
        if (radio->currentFreqIdx >= radio->num_freqs)
            radio->currentFreqIdx = 0;

        Radio::setCarrier(Radio::Carrier::Frequency, radio->scan_freqs[radio->currentFreqIdx]);
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

    packets2send = std::move(sendQueue.front());
    sendQueue.pop();
    txCounter = 0;
    txComplete = false;
    ets_printf("TX: Preparing %d packet(s)\n", packets2send.size());
    setRadioState(RadioState::TX);

    auto packet = packets2send[txCounter];

    Radio::setPreambleLength(LONG_PREAMBLE_MS);
    ets_printf("TX: Using LONG preamble (%d ms)\n", LONG_PREAMBLE_MS);

    Radio::setStandby();
    Radio::clearFlags();
    Radio::writeBytes(REG_FIFO, packet->payload.buffer, packet->buffer_length);
    Radio::setTx();

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

    if (!radio->txComplete) {
        ets_printf("TX: Waiting for TXDONE... (state=%s)\n", radioStateToString(radio->radioState));
        return;
    }

    ESP_LOGD("RADIO", "TXDONE flag set, ready to send repeat or next packet.\n");

    if (packet->repeat > 0) {
        packet->repeat--;
        ets_printf("TX: Repeating current packet (%d repeats left)\n", packet->repeat);
    } else {
        radio->sent(packet);
        radio->txCounter++;

        if (radio->txCounter == radio->packets2send.size()) {
            ets_printf("TX: All packets sent. Stopping Ticker.\n");
            radio->Sender.detach();
            radio->packets2send.clear();
            Radio::setRx();
            radio->setRadioState(RadioState::RX);
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

    Radio::setPreambleLength(SHORT_PREAMBLE_MS);
    Radio::setStandby();
    Radio::clearFlags();
    Radio::writeBytes(REG_FIFO, packet->payload.buffer, packet->buffer_length);
    Radio::setTx();

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

    if (xQueueSendToBack(callbackQueue, &callbackData, 0) != pdPASS) {
        vPortFree(callbackData);
        return false;
    }
    return true;
}

    bool IRAM_ATTR iohcRadio::sent(iohcPacket *packet) {
        bool ret = false;
        if (packet) {
            packetStamp = esp_timer_get_time();
            packet->decode(true);
            addLogMessage(String(packet->decodeToString(true).c_str()));
        }
        if (txCB && !queueCallback(&txCB, packet)) {
            delete packet;
        }
        return ret;
    }

    bool IRAM_ATTR iohcRadio::receive(bool stats) {
        digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
        auto iohc = new iohcPacket;
        iohc->buffer_length = 0;
        iohc->frequency = scan_freqs[currentFreqIdx];

        _g_payload_millis = esp_timer_get_time();
        packetStamp = _g_payload_millis;
#if defined(RADIO_SX127X)
        if (stats) {
            iohc->rssi = static_cast<float>(Radio::readByte(REG_RSSIVALUE)) / -2.0f;
            int16_t thres = Radio::readByte(REG_RSSITHRESH);
            iohc->snr = iohc->rssi > thres ? 0 : (thres - iohc->rssi);
            int16_t f = (uint16_t) Radio::readByte(REG_AFCMSB);
            f = (f << 8) | (uint16_t) Radio::readByte(REG_AFCLSB);
            iohc->afc = f * 61.0;
        }
#endif

#if defined(RADIO_SX127X)
        while (Radio::dataAvail()) {
            iohc->payload.buffer[iohc->buffer_length++] = Radio::readByte(REG_FIFO);
        }
#endif

        iohc->decode(true);
        addLogMessage(String(iohc->decodeToString(true).c_str()));

        if (rxCB && !queueCallback(&rxCB, iohc)) {
            delete iohc;
        }
        digitalWrite(RX_LED, false);
        return true;
    }

    void IRAM_ATTR iohcRadio::i_preamble() {
#if defined(RADIO_SX127X)
        bool preamble = digitalRead(RADIO_PREAMBLE_DETECTED);
        iohcRadio::setRadioState(preamble ? iohcRadio::RadioState::PREAMBLE : iohcRadio::RadioState::RX);
#endif
    }

    void IRAM_ATTR iohcRadio::i_payload() {
#if defined(RADIO_SX127X)
        bool payload = digitalRead(RADIO_PACKET_AVAIL);
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
        radioState = newState;
        ets_printf("State: %s\n", radioStateToString(newState));
    }
}
