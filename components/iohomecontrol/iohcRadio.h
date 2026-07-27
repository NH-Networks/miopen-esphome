#ifndef IOHC_RADIO_H
#define IOHC_RADIO_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Delegate.h"
#include <cstdint>
#include <queue>

#include "board-config.h"
#include "iohcCryptoHelpers.h"
#include "iohcPacket.h"

#if defined(RADIO_SX127X)
        #include "SX1276Helpers.h"
#endif
#if defined(ESP32)
    #include "TickerUsESP32.h"
#endif

#define SM_GRANULARITY_US               130ULL
#define SM_GRANULARITY_MS               1
#define SM_PREAMBLE_RECOVERY_TIMEOUT_US 1378
#define DEFAULT_SCAN_INTERVAL_US        13520

namespace IOHC {
    using IohcPacketDelegate = Delegate<bool(iohcPacket *iohc)>;

    class iohcRadio  {
        public:
            static iohcRadio *getInstance();
            virtual ~iohcRadio() = default;
            enum class RadioState : uint8_t {
                IDLE,
                RX,
                TX,
                PREAMBLE,
                PAYLOAD,
                LOCKED,
                ERROR
            };
            void start(uint8_t num_freqs, uint32_t *scan_freqs, uint32_t scanTimeUs, IohcPacketDelegate rxCallback, IohcPacketDelegate txCallback);
            void send(iohcPacket *packet);
            void send(std::vector<iohcPacket*>&iohcTx);
            static void setRadioState(RadioState newState);
            static const char* radioStateToString(RadioState state);
            volatile static RadioState radioState;
            static void tickerCounter(iohcRadio *radio);
            static volatile bool txComplete;

        private:
            iohcRadio();
            bool receive(bool stats);
            bool sent(iohcPacket *packet);
            void queueSend(std::vector<iohcPacket*> &iohcTx);
            void startQueuedSend();

            static iohcRadio *_iohcRadio;
            static uint8_t _flags[2];
            volatile static unsigned long _g_payload_millis;
            
            volatile static bool send_lock;

            volatile uint32_t tickCounter = 0;
            volatile uint32_t preCounter = 0;
            volatile uint8_t txCounter = 0;
            static void IRAM_ATTR onTxTicker(void *arg);

            uint8_t num_freqs = 0;
            uint32_t *scan_freqs{};
            uint32_t scanTimeUs{};
            uint8_t currentFreqIdx = 0;

#if defined(ESP32)
            TimersUS::TickerUsESP32 Sender;
#endif
            iohcPacket *iohc{};
            
            IohcPacketDelegate rxCB = nullptr;
            IohcPacketDelegate txCB = nullptr;
            std::vector<iohcPacket*> packets2send{};
            std::queue<std::vector<iohcPacket*>> sendQueue{};
        protected:
            static void i_preamble();
            static void i_payload();
    };
}
#endif
