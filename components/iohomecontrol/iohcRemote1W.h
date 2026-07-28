#ifndef IOHC_1W_DEVICE_H
#define IOHC_1W_DEVICE_H

#include "iohcDevice.h"
#include <vector>
#include <string>
#include "tokens.h"
#include "blind_position.h"

#define IOHC_1W_REMOTE  "/1W.json"

namespace IOHC {
    enum class RemoteButton {
        Pair,
        Add,
        Remove,
        Open,
        Close,
        Stop,
        Vent,
        ForceOpen,
        Position,
        Absolute,
        Mode1, Mode2, Mode3, Mode4
    };

    class iohcRemote1W : public iohcDevice {
    public:
        struct remote {
            address node{};
            uint16_t sequence{};
            uint8_t key[16]{};
            std::vector<uint8_t> type{};
            uint8_t manufacturer{};
            bool paired{false};
            std::string description;
            std::string name;
            uint32_t travelTime{};
            bool repeatOnNoResponse{false};
            BlindPosition positionTracker{};
            enum class Movement { Idle, Opening, Closing } movement{Movement::Idle};
            float lastPublishedPosition{0.0f};
            float lastSavedPosition{0.0f};
            std::string lastPublishedState{};
            float targetPosition{-1.0f};
        };

        static iohcRemote1W* getInstance();
        ~iohcRemote1W() override = default;

        void cmd(RemoteButton cmd, Tokens* data);
        void handleRemoteAction(RemoteButton cmd, const std::string &description);
        bool load() override;
        bool save() override;

        static void forgePacket(iohcPacket* packet, uint16_t typn);

        const std::vector<remote>& getRemotes() const;
        bool addRemote(const std::string &name);
        bool removeRemote(const std::string &description);
        bool renameRemote(const std::string &description, const std::string &name);
        bool setTravelTime(const std::string &description, uint32_t travelTime);
        bool setRepeatOnNoResponse(const std::string &description, bool repeatOnNoResponse);
        void updatePositions();
        void tick();
        void processRx(iohcPacket* packet);

    private:
        iohcRemote1W();
        static iohcRemote1W* _iohcRemote1W;

    protected:
        int8_t target[3];
        std::vector<remote> remotes;
    };
}
#endif
