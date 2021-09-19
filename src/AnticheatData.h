#ifndef SC_ACDATA_H
#define SC_ACDATA_H

#include "Common.h"

class Player;
class Unit;

struct MovementInfo;

enum CheatTypes
{
    FLY_HACK    = 0,
    SPEED_HACK,
    DOUBLE_JUMP,
    FAKE_JUMP,
    FAKE_FLY,
    IGNORE_CONTROL,
    CLIMB_HACK,
    NO_FALLING,
    WATERWALK,

    MAX_CHEATS
};

enum GMTextType
{
    GM_TEXT_EVERY_REPORT        = -1,
    GM_TEXT_NONE                = 0
};

enum AppyPenaltyType
{
    APPLY_PENALTY_EVERY_REPORT  = -1,
    APPLY_PENALTY_NONE          = 0
};

enum PenaltyType
{
    PENALTY_NONE                = 0,
    PENALTY_KICK                = 1,
    PENALTY_BAN_PLAYER          = 2,
    PENALTY_BAN_ACCOUNT         = 3
};

class AnticheatData
{
    public:
        AnticheatData(Player* player = nullptr, uint32 time = 0);
        ~AnticheatData();

        bool LoadFromDB(Player* player);

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
        bool CheckMovementInfo(MovementInfo const& movementInfo, Unit* mover, uint16 opcode);
        bool CheckMovement(MovementInfo const& movementInfo, Unit* mover, uint16 opcode);
        bool CheckOnFlyHack(); // AFH

        void SetLastMoveClientTimestamp(uint32 timestamp) { lastMoveClientTimestamp = timestamp; }
        void SetLastMoveServerTimestamp(uint32 timestamp) { lastMoveServerTimestamp = timestamp; }
        uint32 GetLastMoveClientTimestamp() const { return lastMoveClientTimestamp; }
        uint32 GetLastMoveServerTimestamp() const { return lastMoveServerTimestamp; }

        bool HandleDoubleJump(Unit* mover, MovementInfo const& movementInfo);

        void ResetFallingData();

        void StartWaitingLandOrSwimOpcode();
        bool IsWaitingLandOrSwimOpcode() const { return m_antiNoFallDmg; }
        bool IsUnderLastChanceForLandOrSwimOpcode() const { return m_antiNoFallDmgLastChance; }
        void SetSuccessfullyLanded() { m_antiNoFallDmgLastChance = false; }
        // END AntiCheat system

        // Walking data from move packets
        void SetWalkingFlag(bool walkstatus) { m_walking = walkstatus; }
        bool HasWalkingFlag() const { return m_walking; }

        bool NoFallingDamage(uint16 opcode);
        void HandleNoFallingDamage(uint16 opcode);

        void RecordAntiCheatLog(CheatTypes cheatType);
        bool ApplyPenalty(CheatTypes cheatType);

        void SendGMText(CheatTypes cheatType, uint32 stringId, ...);

    private:
        Player* m_owner;

        uint32 m_flyhackTimer;
        uint32 m_mountTimer;
        uint32 m_rootUpdTimer;
        uint32 m_antiNoFallDmgTimer;

        // Timestamp on client clock of the moment the most recently processed movement packet was SENT by the client
        uint32 lastMoveClientTimestamp;
        // Timestamp on server clock of the moment the most recently processed movement packet was RECEIVED from the client
        uint32 lastMoveServerTimestamp;

        bool m_ACKmounted;
        bool m_rootUpd;
        bool m_skipOnePacketForASH; // Used for skip 1 movement packet after charge or blink
        bool m_isjumping;           // Used for jump-opcode in movementhandler
        bool m_canfly;              // Used for access at fly flag - handled restricted access
        bool m_antiNoFallDmg;
        bool m_antiNoFallDmgLastChance;
        bool m_walking;             // Player walking

        bool m_loadedFromDB;
        std::array<uint32, MAX_CHEATS> m_reports;
};

#endif
