#ifndef IOHC_PACKET_H
#define IOHC_PACKET_H

#include <vector>
#include <string>
#include "board-config.h"

#if defined(RADIO_SX127X)
#include "SX1276Helpers.h"
#endif

#define RESET_AFTER_LAST_MSG_US         15000
#define MAX_FRAME_LEN                   32
#define IOHC_INBOUND_MAX_PACKETS        255
#define IOHC_OUTBOUND_MAX_PACKETS       20

namespace IOHC {
    typedef uint8_t address[3];

    struct CB1 {
        uint8_t MsgLen: 5;
        uint8_t Protocol: 1;
        uint8_t StartFrame: 1;
        uint8_t EndFrame: 1;
    };

    struct CB2 {
        uint8_t Version: 2;
        uint8_t Prio: 1;
        uint8_t Unk2: 1;
        uint8_t Unk3: 1;
        uint8_t LPM: 1;
        uint8_t Routed: 1;
        uint8_t Beacon: 1;
    };

    union CtrlByte1Union {
        uint8_t asByte;
        CB1 asStruct;
    };

    union CtrlByte2Union {
        uint8_t asByte;
        CB2 asStruct;
    };

    struct _header {
        CtrlByte1Union CtrlByte1;
        CtrlByte2Union CtrlByte2;
        address target;
        address source;
        uint8_t cmd;
    };

    struct Acei {
        uint8_t isvalid: 1;
        uint8_t extended: 2;
        uint8_t service: 2;
        uint8_t level: 3;
    };

    union AceiUnion {
        uint8_t asByte;
        Acei asStruct;
    };

    inline void setAcei(AceiUnion&acei, uint8_t value) {
        acei.asByte = value;
    }

    struct _p0x00_16 {
        uint8_t origin;
        AceiUnion acei;
        uint8_t main[2];
        uint8_t fp1;
        uint8_t fp2;
        uint8_t data[2];
        uint8_t sequence[2];
        uint8_t hmac[6];
    };

    struct _p0x00_14 {
        uint8_t origin;
        AceiUnion acei;
        uint8_t main[2];
        uint8_t fp1;
        uint8_t fp2;
        uint8_t sequence[2];
        uint8_t hmac[6];
    };

    struct _p0x01_13 {
        uint8_t origin;
        AceiUnion acei;
        uint8_t main;
        uint8_t fp1;
        uint8_t fp2;
        uint8_t sequence[2];
        uint8_t hmac[6];
    };

    struct _p0x20_15 {
        uint8_t origin;
        AceiUnion acei;
        uint8_t main[2];
        uint8_t fp1;
        uint8_t fp2;
        uint8_t fp3;
        uint8_t sequence[2];
        uint8_t hmac[6];
    };

    struct _p0x20_13 {
        uint8_t origin;
        AceiUnion acei;
        uint8_t main[2];
        uint8_t fp1;
        uint8_t sequence[2];
        uint8_t hmac[6];
    };

    struct _p0x20_16 {
        uint8_t origin;
        AceiUnion acei;
        uint8_t main[2];
        uint8_t fp1;
        uint8_t fp2;
        uint8_t data[2];
        uint8_t sequence[2];
        uint8_t hmac[6];
    };

    struct _p0x2b {
        uint8_t actuator[2];
        address backbone;
        uint8_t manufacturer;
        uint8_t info;
        uint8_t tstamp[2];
    };

    struct _p0x2e {
        uint8_t data;
        uint8_t sequence[2];
        uint8_t hmac[6];
    };

    struct _p0x30 {
        uint8_t enc_key[16];
        uint8_t man_id;
        uint8_t data;
        uint8_t sequence[2];
    };

    union _msg {
        _p0x01_13 p0x01_13;
        _p0x00_14 p0x00_14;
        _p0x20_15 p0x20_15;
        _p0x20_13 p0x20_13;
        _p0x00_16 p0x00_16;
        _p0x20_16 p0x20_16;
        _p0x2b p0x29;
        _p0x2b p0x2b;
        _p0x2e p0x2e;
        _p0x30 p0x30;
        _p0x2e p0x39;
    };

    struct _packet {
        _header header;
        _msg msg;
    };

    typedef union {
        uint8_t buffer[MAX_FRAME_LEN];
        _packet packet;
    } Payload;

    typedef struct {
        uint8_t memorizedCmd;
        std::vector<uint8_t> memorizedData;
    } Memorize;

    inline unsigned long packetStamp = 0L;
    inline unsigned long relStamp = 0L;
    inline size_t lastSendCmd = 0xFF;
    inline address lastFromAddress = {0};

    class iohcPacket {
    public:
        iohcPacket() = default;
        ~iohcPacket() = default;

        Payload payload{};
        uint8_t buffer_length = 0;
        uint32_t frequency = CHANNEL2;
        unsigned long repeatTime = 0L;
        uint8_t repeat = 0;
        bool lock = false;
        unsigned long delayed = 0;

        double afc{};
        uint8_t snr{};
        float rssi{};
        uint8_t lna{};

        void decode(bool verbosity = false);
        std::string decodeToString(bool verbosity = false);

    protected:
        uint8_t source_originator[3] = {0};
    };
}
#endif
