#ifndef IOHC_UTILS_H
#define IOHC_UTILS_H

#include <unordered_map>
#include "iohcPacket.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace IOHC {
#define to_hex_str(hex_val) (static_cast<std::stringstream const&>(std::stringstream() << std::hex << static_cast<int>(hex_val))).str()
#define bytesToStr(val, width) (static_cast<std::stringstream const&>(std::stringstream() << std::hex << std::setw(width) << std::setfill('0') << val)).str()

inline std::string bitrow_to_hex_string(const uint8_t* bitrow, unsigned bit_len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (unsigned i = 0; i < (8 * bit_len + 7) / 8; ++i) {
        ss << std::setw(2) << static_cast<unsigned>(bitrow[i]);
    }
    return ss.str();
}

inline std::unordered_map<int, std::string> sCommandId = {
{0xFF,"DO_NOTHING"}, 
{0x00,"EXECUTE FUNCTION 0x00     "}, 
{0x01,"ACTIVATE MODE 0x01        "},
{0x03,"PRIVATE REQ 0x03          "},
{0x04,"PRIVATE ACK 0x04          "},
{0x19,"SET SENSOR VALUE 0x19     "},  
{0x20,"PRIVATE REQ 0x20          "},
{0x21,"PRIVATE ACK 0x21          "},
{0x28,"DISCOVER REQ 0x28         "},
{0x29,"DISCOVER ACK 0x29         "},
{0x2A,"DISCOVER REMOTE REQ 0x2A  "},
{0x2B,"DISCOVER REMOTE ACK 0x2B  "},
{0x2C,"DISCOVER ACTUATOR REQ 0x2C"},
{0x2D,"DISCOVER ACTUATOR ACK 0x2D"},
{0x2E,"UNKNOWN_0x2E              "},
{0x31,"ASK_CHALLENGE_0x31        "},
{0x32,"KEY TRANSFERT REQ 0x32    "},
{0x33,"KEY TRANSFERT ACK 0x33    "},
{0x36,"ADDRESS REQUEST 0x36      "},
{0x38,"LAUNCH KEY TRANSFERT 0x38 "},
{0x3C,"CHALLENGE REQ 0x3C        "},
{0x3D,"CHALLENGE ACK 0x3D        "},
{0x50,"GET NAME REQ 0x50         "},
{0x51,"GET NAME ACK 0x51         "},
{0xfe,"ERROR 0xFE                "}
};

inline std::unordered_map<int, std::string> sDevicesType = {
{0b0000000000, "All"},
{0b0000000001, "Venetian blind"},
{0b0000000010, "Roller shutter"},
{0b0000000011, "Awning (External for windows)"},
{0b0000000100, "Window opener"},
{0b0000000101, "Garage opener"},
{0b0000000110, "Light"},
{0b0000000111, "Gate opener"},
{0b0000001000, "Rolling Door Opener"},
{0b0000001001, "Lock"},
{0b0000001010, "Blind"},
{0b0000001011, "Unk"},
{0b0000001100, "Beacon"},
{0b0000001101, "Dual Shutter"},
{0b0000001110, "Heating Temperature Interface"},
{0b0000001111, "On / Off Switch"},
{0b0000010000, "Horizontal Awning"},
{0b0000010001, "External Venetian Blind"},
{0b0000010010, "Louvre Blind"},
{0b0000010011, "Curtain track"},
{0b0000010100, "Ventilation Point"},
{0b0000010101, "Exterior heating"},
{0b0000010110, "Heat pump (Not currently supported)"},
{0b0000010111, "Intrusion alarm"},
{0b0000011000, "Swinging Shutter"}
};

inline std::unordered_map<int, std::string> sAceiLevel = {
    {0,"Prot Human"},
    {1,"Prot Sensor"},
    {2,"User Controller"},
    {3,"User Remote"},
    {4,"Auto 1"},
    {5,"Auto 2"},
    {6,"Auto SAAC"},
    {7,"Auto 4"},
};

inline std::unordered_map<int, std::string> sOriginator = {};

inline int get_address_class(address address) {
    if (address[0] != 0) { return 13; }

    if (address[1] || (address[2] & 0xC0) != 0) {
        switch (address[2] & 0x3F) {
            case 0x3B: return  7;
            case 0x3C: return  8;
            case 0x3D: return  9;
            case 0x3E: return 10;
            case 0x3F: return 11;
            default: return 12;
        }
    }
    else {
        switch (address[2] & 0x3F) {
            case 0x00: return 0;
            default: return 1;
            case 0x3B: return 2;
            case 0x3C: return 3;
            case 0x3D: return 4;
            case 0x3E: return 5;
            case 0x3F: return 6;
        }
    }
}

}
#endif
