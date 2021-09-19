#include "AnticheatData.h"
#include "AnticheatMgr.h"
#include "AnticheatLanguage.h"
#include "BanMgr.h"
#include "Config.h"
#include "Player.h"
#include "Vehicle.h"
#include "Realm.h"
#include "MoveSplineInit.h"

#define DEFAULT_PLAYER_BOUNDING_RADIUS      0.388999998569489f     // player size, also currently used (correctly?) for any non Unit world objects

AnticheatData::AnticheatData(Player* player, uint32 time) : m_owner(player)
{
    m_flyhackTimer = 0;
    if (sConfigMgr->GetOption<bool>("AntiCheats.FlyHack.Enabled", true))
    {
        m_flyhackTimer = sConfigMgr->GetOption<int32>("AntiCheats.FlyHackTimer", 1000);
    }

    m_mountTimer = 0;
    m_rootUpdTimer = 0;
    m_antiNoFallDmgTimer = 0;
    m_ACKmounted = false;
    m_rootUpd = false;
    m_skipOnePacketForASH = true;
    m_isjumping = false;
    m_canfly = false;
    m_antiNoFallDmg = false;
    m_antiNoFallDmgLastChance = false;
    m_walking = false;
    lastMoveClientTimestamp = time;
    lastMoveServerTimestamp = time;

    m_reports.fill(0);
    m_loadedFromDB = LoadFromDB(player);
}

AnticheatData::~AnticheatData()
{
}

bool AnticheatData::LoadFromDB(Player* player)
{
    if (!player)
    {
        return false;
    }

    QueryResult result = LoginDatabase.PQuery("SELECT * FROM `anticheat_logs` WHERE `guid`=%d", player->GetGUID().GetCounter());
    if (!result)
    {
        return false;
    }

    Field* field = result->Fetch();
    for (uint8 i = FLY_HACK; i < MAX_CHEATS; ++i)
    {
        m_reports[i] = field[i].GetUInt32();
    }

    return true;
}

void AnticheatData::Update(uint32 time)
{
    Unit* mover = m_owner->m_mover;
    if (mover->movespline->Initialized() && !mover->movespline->Finalized())
        return;

    if (m_flyhackTimer > 0)
    {
        if (time >= m_flyhackTimer)
        {
            if (CheckOnFlyHack())
            {
                m_flyhackTimer = sConfigMgr->GetOption<int32>("AntiCheats.FlyHackTimer", 1000);
            }
        }
        else
        {
            m_flyhackTimer -= time;
        }
    }

    if (m_ACKmounted && m_mountTimer > 0)
    {
        if (time >= m_mountTimer)
        {
            m_mountTimer = 0;
            m_ACKmounted = false;
        }
        else
        {
            m_mountTimer -= time;
        }
    }

    if (m_rootUpd && m_rootUpdTimer > 0)
    {
        if (time >= m_rootUpdTimer)
        {
            m_rootUpdTimer = 0;
            m_rootUpd = false;
        }
        else
        {
            m_rootUpdTimer -= time;
        }
    }

    if (m_antiNoFallDmg && m_antiNoFallDmgTimer > 0)
    {
        if (time >= m_antiNoFallDmgTimer)
        {
            m_antiNoFallDmgTimer = 0;
            m_antiNoFallDmg = false;
            m_antiNoFallDmgLastChance = true;
        }
        else
            m_antiNoFallDmgTimer -= time;
    }
}

void AnticheatData::SetUnderACKmount()
{
    m_mountTimer = 3000;
    m_ACKmounted = true;
}

void AnticheatData::SetRootACKUpd()
{
    float fabscount = fabs(float(GetLastMoveClientTimestamp()) - float(GetLastMoveServerTimestamp()));
    uint32 pinginthismoment = uint32(fabscount) / 1000000;
    m_rootUpdTimer = 1500 + pinginthismoment;
    m_rootUpd = true;
}

bool AnticheatData::CheckOnFlyHack()
{
    if (IsCanFlybyServer())
    {
        return true;
    }

    Unit* mover = m_owner->m_mover;
    if (mover->IsFalling() || (mover->IsPlayer() && mover->ToPlayer()->IsFalling()))
    {
        return true;
    }

    if (sAnticheatMgr->isMapDisabledForAC(mover->GetMapId()) || sAnticheatMgr->isAreaDisabledForAC(mover->GetAreaId()))
    {
        return true;
    }

    if (mover->GetVehicle())
    {
        return true;
    }

    if (mover->IsFlying() && !mover->CanFly() && !mover->HasUnitMovementFlag(MOVEMENTFLAG_CAN_FLY)) // kick flyhacks
    {
        LOG_INFO("anticheat", "PassiveAnticheat: FlyHack Detected for Account id : %u, Player %s (%s), Mover: (%s, %s) Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
            mover->GetName().c_str(), mover->GetGUID().ToString().c_str(), m_owner->GetMapId(),
            mover->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", mover->GetUnitMovementFlags());
        LOG_INFO("anticheat", "Player::========================================================");
        LOG_INFO("anticheat", "Player IsFlying but CanFly is false");

        RecordAntiCheatLog(FLY_HACK);
        SendGMText(FLY_HACK, LANG_GM_ANNOUNCE_AFH_CANFLYWRONG, m_owner->GetName().c_str());
        if (ApplyPenalty(FLY_HACK))
        {
            return false;
        }
    }

    if (mover->IsFlying() || mover->IsLevitating() || mover->IsInFlight())
    {
        return true;
    }

    if (m_owner->HasUnitState(UNIT_STATE_IGNORE_ANTISPEEDHACK))
    {
        return true;
    }

    if (UnderACKmount())
    {
        return true;
    }

    if (IsSkipOnePacketForASH())
    {
        return true;
    }

    Position npos = mover->GetPosition();
    float pz = npos.GetPositionZ();
    if (!mover->IsInWater() && mover->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
    {
        LiquidData const& liquidData = mover->GetMap()->GetLiquidData(mover->GetPhaseMask(), npos.GetPositionX(), npos.GetPositionY(), pz, mover->GetCollisionHeight(), MAP_ALL_LIQUIDS);

        float waterlevel = liquidData.Level; // water walking
        bool  hovergaura = mover->HasAuraType(SPELL_AURA_WATER_WALK) || mover->HasAuraType(SPELL_AURA_HOVER);
        if (waterlevel > INVALID_HEIGHT && (pz - waterlevel) <= (hovergaura ? mover->GetCollisionHeight() + 1.5f : mover->GetCollisionHeight() + 1.5f))
        {
            return true;
        }

        LOG_INFO("anticheat", "PassiveAnticheat: FlyHack Detected for Account id : %u, Player %s (%s), Mover: (%s, %s), Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
            mover->GetName().c_str(), mover->GetGUID().ToString().c_str(), mover->GetMapId(),
            mover->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", mover->GetUnitMovementFlags());
        LOG_INFO("anticheat", "Player::========================================================");
        LOG_INFO("anticheat", "Player has a MOVEMENTFLAG_SWIMMING, but not in water");

        RecordAntiCheatLog(FLY_HACK);
        SendGMText(FLY_HACK, LANG_GM_ANNOUNCE_AFK_SWIMMING, m_owner->GetName().c_str());
        if (ApplyPenalty(FLY_HACK))
        {
            return false;
        }
    }
    else if (!mover->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
    {
        float z = mover->GetMap()->GetHeight(mover->GetPhaseMask(), npos.GetPositionX(), npos.GetPositionY(), pz + mover->GetCollisionHeight() + 0.5f, true, 50.0f); // smart flyhacks -> SimpleFly
        if (z > INVALID_HEIGHT)
        {
            float diff = pz - z;
            if (diff > 6.8f) // better calculate the second time for false situations, but not call GetHoverOffset everytime (economy resource)
            {
                LiquidData const& liquidData = mover->GetMap()->GetLiquidData(mover->GetPhaseMask(), npos.GetPositionX(), npos.GetPositionY(), pz, mover->GetCollisionHeight(), MAP_ALL_LIQUIDS);

                float waterlevel = liquidData.Level; // water walking
                if (waterlevel > INVALID_HEIGHT && waterlevel + mover->GetCollisionHeight() > pz)
                {
                    return true;
                }

                float cx, cy, cz;
                mover->GetTheClosestPoint(cx, cy, cz, 0.5, pz, 6.8f); // first check
                if (pz - cz > 6.8f)
                {
                    mover->GetMap()->GetObjectHitPos(mover->GetPhaseMask(), mover->GetPositionX(), mover->GetPositionY(),
                        mover->GetPositionZ() + mover->GetCollisionHeight(), cx, cy, cz + mover->GetCollisionHeight(), cx, cy, cz, -mover->GetCollisionHeight());
                    if (pz - cz > 6.8f)
                    {
                        LOG_INFO("anticheat", "PassiveAnticheat: FlyHack Detected for Account id : %u, Player %s (%s), Mover: (%s, %s), Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
                            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
                            mover->GetName().c_str(), mover->GetGUID().ToString().c_str(), mover->GetMapId(),
                            mover->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", mover->GetUnitMovementFlags());
                        LOG_INFO("anticheat", "Player::========================================================");
                        LOG_INFO("anticheat", "playerZ = %f", pz);
                        LOG_INFO("anticheat", "normalZ = %f", z);
                        LOG_INFO("anticheat", "checkz = %f", cz);
                        LOG_INFO("anticheat", "========================================================");

                        RecordAntiCheatLog(FLY_HACK);
                        SendGMText(FLY_HACK, LANG_GM_ANNOUNCE_AFH, m_owner->GetName().c_str());
                        if (ApplyPenalty(FLY_HACK))
                        {
                            return false;
                        }
                    }
                }
            }
        }
    }

    return true;
}

void AnticheatData::UpdateMovementInfo(MovementInfo const& movementInfo)
{
    SetLastMoveClientTimestamp(movementInfo.time);
    SetLastMoveServerTimestamp(World::GetGameTimeMS());
}

void AnticheatData::StartWaitingLandOrSwimOpcode()
{
    m_antiNoFallDmgTimer = 3000;
    m_antiNoFallDmg = true;
}

bool AnticheatData::CheckMovementInfo(MovementInfo const& movementInfo, Unit* mover, uint16 opcode)
{
    if (CheckMovement(movementInfo, mover, opcode))
    {
        UpdateMovementInfo(movementInfo);
        return true;
    }

    return false;
}

bool AnticheatData::CheckMovement(MovementInfo const& movementInfo, Unit* mover, uint16 opcode)
{
    if (mover->movespline->Initialized() && !mover->movespline->Finalized())
    {
        return true;
    }

    if (sAnticheatMgr->isMapDisabledForAC(mover->GetMapId()) || sAnticheatMgr->isAreaDisabledForAC(mover->GetAreaId()))
    {
        return true;
    }

    if (mover->GetVehicle())
    {
        return true;
    }

    if (sConfigMgr->GetOption<bool>("AntiCheats.FakeJumpHack.Enabled", true) && mover->IsFalling() && movementInfo.pos.GetPositionZ() > mover->GetPositionZ())
    {
        if (!IsJumpingbyOpcode() && !UnderACKmount() && !mover->IsFlying())
        {
            // fake jumper -> for example gagarin air mode with falling flag (like player jumping), but client can't sent a new coords when falling
            LOG_INFO("anticheat", "PassiveAnticheat: Fake jumper by Account id : %u, Player %s (%s), Mover: (%s, %s), Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
                m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
                mover->GetName().c_str(), mover->GetGUID().ToString().c_str(), m_owner->GetMapId(),
                mover->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", movementInfo.GetMovementFlags());

            RecordAntiCheatLog(FAKE_JUMP);
            SendGMText(FAKE_JUMP, LANG_GM_ANNOUNCE_JUMPER_FAKE, m_owner->GetName().c_str());
            if (ApplyPenalty(FAKE_JUMP))
            {
                return false;
            }
        }
    }

    if (sConfigMgr->GetOption<bool>("AntiCheats.FakeFlyHack.Enabled", true) && !IsCanFlybyServer() && !UnderACKmount() && !mover->HasAuraType(SPELL_AURA_FLY) &&
        movementInfo.HasMovementFlag(MOVEMENTFLAG_MASK_MOVING_FLY) && !mover->IsInWater())
    {
        LOG_INFO("anticheat", "PassiveAnticheat: Fake flying mode (using MOVEMENTFLAG_FLYING flag doesn't restricted) by Account id : %u, Player %s (%s), Mover: (%s, %s), Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
            mover->GetName().c_str(), mover->GetGUID().ToString().c_str(), m_owner->GetMapId(),
            mover->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", movementInfo.GetMovementFlags());

        RecordAntiCheatLog(FAKE_FLY);
        SendGMText(FAKE_FLY, LANG_GM_ANNOUNCE_JUMPER_FLYING, m_owner->GetName().c_str());
        if (ApplyPenalty(FAKE_FLY))
        {
            return false;
        }
    }

    if (sConfigMgr->GetOption<bool>("AntiCheats.WaterwalkHack.Enabled", true) && !UnderACKmount() && movementInfo.HasMovementFlag(MOVEMENTFLAG_WATERWALKING) &&
        !mover->HasAuraType(SPELL_AURA_WATER_WALK) && !mover->HasAuraType(SPELL_AURA_GHOST))
    {
        LOG_INFO("anticheat", "PassiveAnticheat: Waterwalking mode (using MOVEMENTFLAG_WATERWALK flag without aura) by Account id : %u, Player %s (%s), Mover: (%s, %s), Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), mover->GetName().c_str(), mover->GetGUID().ToString().c_str(),
            mover->GetMapId(), mover->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", movementInfo.GetMovementFlags());

        RecordAntiCheatLog(WATERWALK);
        SendGMText(WATERWALK, LANG_GM_ANNOUNCE_WATERWALK, m_owner->GetName().c_str());
        if (ApplyPenalty(WATERWALK))
        {
            return false;
        }
    }

    if (!sConfigMgr->GetOption<bool>("AntiCheats.SpeedHack.Enabled", true))
    {
        return true;
    }

    uint32 oldctime = GetLastMoveClientTimestamp();
    if (oldctime)
    {
        if (mover->IsFalling() || mover->IsInFlight())
        {
            return true;
        }

        if (!m_owner->IsControlledByPlayer())
        {
            return true;
        }

        if (m_owner->HasUnitState(UNIT_STATE_IGNORE_ANTISPEEDHACK))
        {
            return true;
        }

        if (IsSkipOnePacketForASH())
        {
            SetSkipOnePacketForASH(false);
            return true;
        }

        bool transportflag = ((movementInfo.GetMovementFlags() & MOVEMENTFLAG_ONTRANSPORT) || mover->HasUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT)) && !m_owner->GetVehicle();
        float x, y, z;
        Position npos;

        // Position coords for new point
        if (!transportflag)
            npos = movementInfo.pos;
        else
            npos = movementInfo.transport.pos;

        // Position coords for previous point (old)
        // Just CheckMovementInfo are calling before player change UnitMovementFlag MOVEMENTFLAG_ONTRANSPORT
        if (transportflag)
        {
            if (mover->GetTransOffsetX() == 0.f) // if it elevator or fist step - player can have zero this coord
                return true;

            x = mover->GetTransOffsetX();
            y = mover->GetTransOffsetY();
            z = mover->GetTransOffsetZ();
        }
        else
            mover->GetPosition(x, y, z);

        if (sConfigMgr->GetOption<bool>("AntiCheats.IgnoreControlHack.Enabled", true))
        {
            if (mover->HasUnitState(UNIT_STATE_ROOT) && !mover->GetVehicle() && !UnderACKRootUpd())
            {
                bool unrestricted = npos.GetPositionX() != x || npos.GetPositionY() != y;
                if (unrestricted)
                {
                    LOG_INFO("anticheat", "CheckMovementInfo :  Ignore control Hack detected for Account id : %u, Player %s (%s), Mover: (%s, %s), Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
                        m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
                        mover->GetName().c_str(), mover->GetGUID().ToString().c_str(), mover->GetMapId(),
                        mover->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", movementInfo.GetMovementFlags());

                    RecordAntiCheatLog(IGNORE_CONTROL);
                    SendGMText(IGNORE_CONTROL, LANG_GM_ANNOUNCE_MOVE_UNDER_CONTROL, m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str());
                    if (ApplyPenalty(IGNORE_CONTROL))
                    {
                        return false;
                    }
                }
            }
        }

        float distance, runspeed, difftime, normaldistance, delay, diffPacketdelay;
        uint32 ptime;
        std::string mapname = mover->GetMap()->GetMapName();

        // calculate distance - don't use func, because x,z can be offset transport coords
        distance = sqrt(((npos.GetPositionY() - y) * (npos.GetPositionY() - y)) + ((npos.GetPositionX() - x) * (npos.GetPositionX() - x)));

        if (sConfigMgr->GetOption<bool>("AntiCheats.ClimbHack.Enabled", true) && opcode != MSG_MOVE_JUMP && !mover->CanFly() &&
            !mover->HasUnitMovementFlag(MOVEMENTFLAG_CAN_FLY) && !mover->isSwimming() && !transportflag && distance > 0.f)
        {
            float diffz = fabs(movementInfo.pos.GetPositionZ() - z);
            float tanangle = distance / diffz;

            if (movementInfo.pos.GetPositionZ() > z && diffz > 1.87f && tanangle < 0.57735026919f) // 30 degrees
            {
                LOG_INFO("anticheat", "PassiveAnticheat: Climb Hack detected for Account id : %u, Player %s (%s), Mover: (%s, %s), Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
                    m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
                    mover->GetName().c_str(), mover->GetGUID().ToString().c_str(), mover->GetMapId(),
                    mover->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", movementInfo.GetMovementFlags());

                RecordAntiCheatLog(CLIMB_HACK);
                SendGMText(CLIMB_HACK, LANG_GM_ANNOUNCE_WALLCLIMB, m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), diffz, distance, tanangle, mapname.c_str(), m_owner->GetMapId(), x, y, z);
                if (ApplyPenalty(CLIMB_HACK))
                {
                    return false;
                }
            }
        }

        uint32 oldstime = GetLastMoveServerTimestamp();
        uint32 stime = World::GetGameTimeMS();
        uint32 ping;
        ptime = movementInfo.time;

        UnitMoveType moveType = Movement::SelectSpeedType(mover->GetUnitMovementFlags());
        if ((opcode == CMSG_MOVE_SET_FLY || opcode == MSG_MOVE_FALL_LAND) && movementInfo.HasMovementFlag(MOVEMENTFLAG_CAN_FLY))
        {
            moveType = MOVE_FLIGHT;
        }

        runspeed = mover->GetSpeed(moveType);

        delay = ptime - oldctime;
        diffPacketdelay = 10000000 - delay;

        if (oldctime > ptime)
        {
            LOG_INFO("anticheat", "oldctime > ptime");
            delay = 0;
        }
        diffPacketdelay = diffPacketdelay * 0.0000000001f;

        difftime = delay * 0.001f + diffPacketdelay;

        // if movetime faked and lower, difftime should be with "-"
        normaldistance = (runspeed * difftime) + 0.002f; // 0.002f a little safe temporary hack
        if (UnderACKmount())
            normaldistance += 20.0f;

        if (normaldistance - distance >= -1.5f)
            return true;

        ping = uint32(diffPacketdelay * 10000.f);

        LOG_INFO("anticheat", "PassiveAnticheat: SpeedHack Detected for Account id : %u, Player %s (%s), Mover: (%s, %s), Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
            mover->GetName().c_str(), mover->GetGUID().ToString().c_str(), mover->GetMapId(),
            mover->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", movementInfo.GetMovementFlags());
        LOG_INFO("anticheat", "========================================================");
        LOG_INFO("anticheat", "oldX = %f", x);
        LOG_INFO("anticheat", "oldY = %f", y);
        LOG_INFO("anticheat", "newX = %f", npos.GetPositionX());
        LOG_INFO("anticheat", "newY = %f", npos.GetPositionY());
        LOG_INFO("anticheat", "packetdistance = %f", distance);
        LOG_INFO("anticheat", "available distance = %f", normaldistance);
        LOG_INFO("anticheat", "oldStime = %u", oldstime);
        LOG_INFO("anticheat", "oldCtime = %u", oldctime);
        LOG_INFO("anticheat", "serverTime = %u", stime);
        LOG_INFO("anticheat", "packetTime = %u", ptime);
        LOG_INFO("anticheat", "diff delay between old ptk and current pkt = %f", diffPacketdelay);
        LOG_INFO("anticheat", "FullDelay = %f", delay / 1000.f);
        LOG_INFO("anticheat", "difftime = %f", difftime);
        LOG_INFO("anticheat", "ping = %u", ping);
        LOG_INFO("anticheat", "========================================================");

        RecordAntiCheatLog(SPEED_HACK);
        SendGMText(SPEED_HACK, LANG_GM_ANNOUNCE_ASH, m_owner->GetName().c_str(), normaldistance, distance);
    }
    else
    {
        return true;
    }

    if (ApplyPenalty(SPEED_HACK))
    {
        return false;
    }

    if (!HasWalkingFlag() && movementInfo.HasMovementFlag(MOVEMENTFLAG_WALKING))
    {
        SetWalkingFlag(true);
    }

    if (HasWalkingFlag() && !movementInfo.HasMovementFlag(MOVEMENTFLAG_WALKING))
    {
        SetWalkingFlag(false);
    }

    return true;
}

bool AnticheatData::HandleDoubleJump(Unit* mover, MovementInfo const& movementInfo)
{
    SetJumpingbyOpcode(true);
    SetUnderACKmount();

    if (!sConfigMgr->GetOption<bool>("AntiCheats.DoubleJumpHack.Enabled", true))
    {
        return true;
    }

    if (sAnticheatMgr->isMapDisabledForAC(mover->GetMapId()) || sAnticheatMgr->isAreaDisabledForAC(mover->GetAreaId()))
    {
        return true;
    }

    if (mover->movespline->Initialized() && !mover->movespline->Finalized())
    {
        return true;
    }

    if (mover->GetVehicle())
    {
        return true;
    }

    if (mover->IsFalling())
    {
        LOG_INFO("anticheat", "PassiveAnticheat: Double jump by Account id : %u, Player %s (%s), Mover: (%s, %s), Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
            mover->GetName().c_str(), mover->GetGUID().ToString().c_str(), mover->GetMapId(),
            mover->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", movementInfo.GetMovementFlags());

        RecordAntiCheatLog(DOUBLE_JUMP);
        SendGMText(DOUBLE_JUMP, LANG_GM_ANNOUNCE_DOUBLE_JUMP, m_owner->GetName().c_str());
        if (ApplyPenalty(DOUBLE_JUMP))
        {
            return false;
        }
    }

    return true;
}

void AnticheatData::ResetFallingData()
{
    if (IsWaitingLandOrSwimOpcode())
        m_antiNoFallDmg = false;

    if (IsUnderLastChanceForLandOrSwimOpcode())
        m_antiNoFallDmgLastChance = false;
}

bool AnticheatData::NoFallingDamage(uint16 opcode)
{
    if (IsUnderLastChanceForLandOrSwimOpcode())
    {
        if (!sConfigMgr->GetOption<bool>("AntiCheats.NoFallingHack.Enabled", true))
        {
            SetSuccessfullyLanded();
            return true;
        }

        Unit* mover = m_owner->m_mover;
        if (sAnticheatMgr->isMapDisabledForAC(mover->GetMapId()) || sAnticheatMgr->isAreaDisabledForAC(mover->GetAreaId()))
        {
            SetSuccessfullyLanded();
            return true;
        }

        if (mover->movespline->Initialized() && !mover->movespline->Finalized())
        {
            SetSuccessfullyLanded();
            return true;
        }

        if (mover->GetVehicle())
        {
            return true;
        }

        bool checkNorm = false;
        switch (opcode)
        {
            case MSG_MOVE_FALL_LAND:
            case MSG_MOVE_START_SWIM:
                checkNorm = true;
                break;
        }

        if (IsCanFlybyServer() || mover->HasAuraType(SPELL_AURA_FLY))
            checkNorm = true;

        if (!checkNorm)
        {
            LOG_INFO("anticheat", "PassiveAnticheat: NoFallingDamage by Account id : %u, Player %s (%s), Mover: (%s, %s), Map: %d, Position: %s (TransportOffsets: %s), MovementFlags: %d",
                m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
                mover->GetName().c_str(), mover->GetGUID().ToString().c_str(), mover->GetMapId(),
                m_owner->GetPosition().ToString().c_str(), mover->GetTransport() ?  mover->m_movementInfo.transport.pos.ToString().c_str() : "None", mover->GetUnitMovementFlags());

            RecordAntiCheatLog(NO_FALLING);
            SendGMText(NO_FALLING, LANG_GM_ANNOUNCE_NOFALLINGDMG, m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str());
            if (ApplyPenalty(NO_FALLING))
            {
                return false;
            }
        }
        else
            SetSuccessfullyLanded();
    }

    return true;
}

void AnticheatData::RecordAntiCheatLog(CheatTypes cheatType)
{
    std::string cheatColumn;
    switch (cheatType)
    {
        case FLY_HACK:
            cheatColumn = "flyHacks";
            break;
        case SPEED_HACK:
            cheatColumn = "speedHacks";
            break;
        case DOUBLE_JUMP:
            cheatColumn = "doubleJumps";
            break;
        case FAKE_JUMP:
            cheatColumn = "fakeJumps";
            break;
        case FAKE_FLY:
            cheatColumn = "fakeFly";
            break;
        case IGNORE_CONTROL:
            cheatColumn = "ignoreControls";
            break;
        case CLIMB_HACK:
            cheatColumn = "climbHacks";
            break;
        case NO_FALLING:
            cheatColumn = "noFalling";
            break;
        case WATERWALK:
            cheatColumn = "waterwalk";
            break;
    }

    ++m_reports[cheatType];

    if (m_loadedFromDB)
    {
        LoginDatabase.PExecute("UPDATE `anticheat_logs` SET %s=%d WHERE `guid`=%d", cheatColumn, m_reports[cheatType], m_owner->GetGUID().GetCounter());
    }
    else
    {
        m_loadedFromDB = true;
        LoginDatabase.PExecute("INSERT INTO anticheat_logs (`account`, `guid`, `player`, %s) VALUES (%d, %d, '%s', %d)",
            cheatColumn, m_owner->GetSession()->GetAccountId(), m_owner->GetGUID().GetCounter(), m_owner->GetName().c_str(), m_reports[cheatType]);
    }
}

void AnticheatData::HandleNoFallingDamage(uint16 opcode)
{
    Unit* mover = m_owner->m_mover;
    if (!IsCanFlybyServer() && !mover->HasAuraType(SPELL_AURA_FLY))
    {
        bool checkNorm = false;
        switch (opcode)
        {
            case MSG_MOVE_FALL_LAND:
            case MSG_MOVE_START_SWIM:
                checkNorm = true;
                break;
        }

        if (!checkNorm && !IsWaitingLandOrSwimOpcode())
            StartWaitingLandOrSwimOpcode();
    }
}

bool AnticheatData::ApplyPenalty(CheatTypes cheatType)
{
    std::string cheat;
    switch (cheatType)
    {
        case FLY_HACK:
            cheat = "FlyHack";
            break;
        case SPEED_HACK:
            cheat = "SpeedHack";
            break;
        case DOUBLE_JUMP:
            cheat = "DoubleJumpHack";
            break;
        case FAKE_JUMP:
            cheat = "FakeJumpHack";
            break;
        case FAKE_FLY:
            cheat = "FakeFlyHack";
            break;
        case IGNORE_CONTROL:
            cheat = "IgnoreControlHack";
            break;
        case CLIMB_HACK:
            cheat = "ClimbHack";
            break;
        case NO_FALLING:
            cheat = "NoFallingHack";
            break;
        case WATERWALK:
            cheat = "WaterwalkHack";
            break;
    }

    std::stringstream applyPenaltyStr;
    applyPenaltyStr << "AntiCheats." << cheat << ".ApplyPenalty";
    int32 applyPenalty = sConfigMgr->GetOption<int32>(applyPenaltyStr.str(), 0);

    switch (applyPenalty)
    {
        case APPLY_PENALTY_NONE:
            return false;
        case APPLY_PENALTY_EVERY_REPORT:
            break;
        default:
        {
            uint32 reportsCount = m_reports[cheatType];
            if (reportsCount < applyPenalty)
            {
                return false;
            }
            break;
        }
    }

    std::stringstream penaltyTypeStr;
    penaltyTypeStr << "AntiCheats." << cheat << ".ApplyPenalty";
    uint32 penaltyType = sConfigMgr->GetOption<uint32>(penaltyTypeStr.str(), 0);

    BanReturn result = BAN_SUCCESS;
    switch (penaltyType)
    {
        case PENALTY_NONE:
            return false;
        case PENALTY_KICK:
            m_owner->GetSession()->KickPlayer(cheat);
            break;
        case PENALTY_BAN_PLAYER:
            result = sBan->BanCharacter(m_owner->GetName(), "-1", cheat, "AnticheatSystem");
            break;
        case PENALTY_BAN_ACCOUNT:
            result = sBan->BanAccount(m_owner->GetName(), "-1", cheat, "AnticheatSystem");
            break;
    }

    return result == BAN_SUCCESS;
}

void AnticheatData::SendGMText(CheatTypes cheatType, uint32 stringId, ...)
{
    std::string cheat;
    switch (cheatType)
    {
        case FLY_HACK:
            cheat = "FlyHack";
            break;
        case SPEED_HACK:
            cheat = "SpeedHack";
            break;
        case DOUBLE_JUMP:
            cheat = "DoubleJumpHack";
            break;
        case FAKE_JUMP:
            cheat = "FakeJumpHack";
            break;
        case FAKE_FLY:
            cheat = "FakeFlyHack";
            break;
        case IGNORE_CONTROL:
            cheat = "IgnoreControlHack";
            break;
        case CLIMB_HACK:
            cheat = "ClimbHack";
            break;
        case NO_FALLING:
            cheat = "NoFallingHack";
            break;
        case WATERWALK:
            cheat = "WaterwalkHack";
            break;
    }

    std::stringstream gmTextStr;
    gmTextStr << "AntiCheats." << cheat << ".GMText";
    int32 gmText = sConfigMgr->GetOption<int32>(gmTextStr.str(), -1);

    switch (gmText)
    {
        case GM_TEXT_NONE:
            return;
        case GM_TEXT_EVERY_REPORT:
            break;
        default:
        {
            uint32 reportsCount = m_reports[cheatType];
            if (reportsCount < gmText)
            {
                return;
            }
            break;
        }
    }

    va_list ap;
    va_start(ap, stringId);

    sWorld->SendGMText(stringId, &ap);

    va_end(ap);
}
