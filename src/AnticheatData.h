#ifndef SC_ACDATA_H
#define SC_ACDATA_H

#include "Common.h"

class Player;
class Unit;

struct MovementInfo;

class AnticheatData
{
    public:
        AnticheatData(Player* player = nullptr);
        ~AnticheatData();

        void Update(uint32 time);

        void SetSkipOnePacketForASH(bool apply) { m_skipOnePacketForASH = apply; }
        bool IsSkipOnePacketForASH() const { return m_skipOnePacketForASH; }
        void SetJumpingbyOpcode(bool jump) { m_isjumping = jump; }
        bool IsJumpingbyOpcode() const { return m_isjumping; }
        void SetCanFlybyServer(bool apply) { m_canfly = apply; }
        bool IsCanFlybyServer() const { return m_canfly; }

        bool UnderACKmount() const { return m_ACKmounted; }
        bool UnderACKRootUpd() const { return m_rootUpd; }
        void SetUnderACKmount();
        void SetRootACKUpd();

        // should only be used by packet handlers to validate and apply incoming MovementInfos from clients. Do not use internally to modify m_movementInfo
        void UpdateMovementInfo(MovementInfo const& movementInfo);
        bool CheckMovementInfo(MovementInfo const& movementInfo, Unit* mover, bool jump);
        bool CheckMovement(MovementInfo const& movementInfo, Unit* mover, bool jump);
        bool CheckOnFlyHack(); // AFH

        void SetLastMoveClientTimestamp(uint32 timestamp) { lastMoveClientTimestamp = timestamp; }
        void SetLastMoveServerTimestamp(uint32 timestamp) { lastMoveServerTimestamp = timestamp; }
        uint32 GetLastMoveClientTimestamp() const { return lastMoveClientTimestamp; }
        uint32 GetLastMoveServerTimestamp() const { return lastMoveServerTimestamp; }

        bool HandleDoubleJump(Unit* mover);

    private:
        Player* m_owner;
        uint32 m_flyhackTimer;
        uint32 m_mountTimer;
        uint32 m_rootUpdTimer;
        bool   m_ACKmounted;
        bool   m_rootUpd;
        // Timestamp on client clock of the moment the most recently processed movement packet was SENT by the client
        uint32 lastMoveClientTimestamp;
        // Timestamp on server clock of the moment the most recently processed movement packet was RECEIVED from the client
        uint32 lastMoveServerTimestamp;

        bool m_skipOnePacketForASH; // Used for skip 1 movement packet after charge or blink
        bool m_isjumping;           // Used for jump-opcode in movementhandler
        bool m_canfly;              // Used for access at fly flag - handled restricted access
};

#endif
