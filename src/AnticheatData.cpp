#include "AnticheatData.h"
#include "AnticheatMgr.h"
#include "AnticheatLanguage.h"
#include "Config.h"
#include "Player.h"
#include "Vehicle.h"
#include "Realm.h"

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

    m_loadedFromDB = false;
    if (player && LoginDatabase.PQuery("SELECT 1 FROM `anticheat_logs` WHERE `guid`=%d", player->GetGUID().GetCounter()))
        m_loadedFromDB = true;
}

AnticheatData::~AnticheatData()
{
}

void AnticheatData::Update(uint32 time)
{
    if (m_owner->movespline->Initialized() && !m_owner->movespline->Finalized())
        return;

    if (m_flyhackTimer > 0)
    {
        if (time >= m_flyhackTimer)
        {
            if (!CheckOnFlyHack() && sConfigMgr->GetOption<bool>("AntiCheats.FlyHack.Kick.Enabled", true))
            {
                m_owner->GetSession()->KickPlayer();
            }

            m_flyhackTimer = sConfigMgr->GetOption<int32>("AntiCheats.FlyHackTimer", 1000);
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

    if (m_owner->ToUnit()->IsFalling() || m_owner->IsFalling())
    {
        return true;
    }

    if (m_owner->m_mover != m_owner)
    {
        return true;
    }

    if (sAnticheatMgr->isMapDisabledForAC(m_owner->GetMapId()) || sAnticheatMgr->isAreaDisabledForAC(m_owner->GetAreaId()))
    {
        return true;
    }

    if (m_owner->IsFlying() && !m_owner->CanFly()) // kick flyhacks
    {
        LOG_INFO("anticheat", "PassiveAnticheat: FlyHack Detected for Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), m_owner->GetMapId(),
            m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
        LOG_INFO("anticheat", "Player::========================================================");
        LOG_INFO("anticheat", "Player IsFlying but CanFly is false");

        sWorld->SendGMText(LANG_GM_ANNOUNCE_AFH_CANFLYWRONG, m_owner->GetName().c_str());
        RecordAntiCheatLog(FLY_HACK);
        return false;
    }

    if (m_owner->IsFlying() || m_owner->IsLevitating() || m_owner->IsInFlight())
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

    Position npos = m_owner->GetPosition();
    float pz = npos.GetPositionZ();
    if (!m_owner->IsInWater() && m_owner->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
    {
        LiquidData liquid_status;
        m_owner->GetMap()->getLiquidStatus(npos.GetPositionX(), npos.GetPositionY(), pz, MAP_ALL_LIQUIDS, &liquid_status, m_owner->GetCollisionHeight());

        float waterlevel = liquid_status.level; // water walking
        bool hovergaura = m_owner->HasAuraType(SPELL_AURA_WATER_WALK) || m_owner->HasAuraType(SPELL_AURA_HOVER);
        if (waterlevel > INVALID_HEIGHT && (pz - waterlevel) <= (hovergaura ? m_owner->GetCollisionHeight() + 1.5f : m_owner->GetCollisionHeight() + 1.5f))
        {
            return true;
        }

        LOG_INFO("anticheat", "PassiveAnticheat: FlyHack Detected for Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), m_owner->GetMapId(),
            m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
        LOG_INFO("anticheat", "Player::========================================================");
        LOG_INFO("anticheat", "Player has a MOVEMENTFLAG_SWIMMING, but not in water");

        sWorld->SendGMText(LANG_GM_ANNOUNCE_AFK_SWIMMING, m_owner->GetName().c_str());
        RecordAntiCheatLog(FLY_HACK);
        return false;
    }
    else if (!m_owner->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
    {
        float z = m_owner->GetMap()->GetHeight(m_owner->GetPhaseMask(), npos.GetPositionX(), npos.GetPositionY(), pz + m_owner->GetCollisionHeight() + 0.5f, true, 50.0f); // smart flyhacks -> SimpleFly
        if (z > INVALID_HEIGHT)
        {
            float diff = pz - z;
            if (diff > 6.8f) // better calculate the second time for false situations, but not call GetHoverOffset everytime (economy resource)
            {
                LiquidData liquid_status;
                m_owner->GetMap()->getLiquidStatus(npos.GetPositionX(), npos.GetPositionY(), pz, MAP_ALL_LIQUIDS, &liquid_status, m_owner->GetCollisionHeight());

                float waterlevel = liquid_status.level; // water walking
                if (waterlevel > INVALID_HEIGHT && waterlevel + m_owner->GetCollisionHeight() > pz)
                {
                    return true;
                }

                float cx, cy, cz;
                m_owner->GetTheClosestPoint(cx, cy, cz, 0.5, pz, 6.8f); // first check
                if (pz - cz > 6.8f)
                {
                    m_owner->GetMap()->GetObjectHitPos(m_owner->GetPhaseMask(), m_owner->GetPositionX(), m_owner->GetPositionY(),
                        m_owner->GetPositionZ() + m_owner->GetCollisionHeight(), cx, cy, cz + m_owner->GetCollisionHeight(), cx, cy, cz, -m_owner->GetCollisionHeight());
                    if (pz - cz > 6.8f)
                    {
                        LOG_INFO("anticheat", "PassiveAnticheat: FlyHack Detected for Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
                            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), m_owner->GetMapId(),
                            m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
                        LOG_INFO("anticheat", "Player::========================================================");
                        LOG_INFO("anticheat", "playerZ = %f", pz);
                        LOG_INFO("anticheat", "normalZ = %f", z);
                        LOG_INFO("anticheat", "checkz = %f", cz);
                        LOG_INFO("anticheat", "========================================================");
                        sWorld->SendGMText(LANG_GM_ANNOUNCE_AFH, m_owner->GetName().c_str());
                        RecordAntiCheatLog(FLY_HACK);
                        return false;
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

bool AnticheatData::CheckMovementInfo(MovementInfo const& movementInfo, Unit* mover, bool jump)
{
    if (CheckMovement(movementInfo, mover, jump))
    {
        UpdateMovementInfo(movementInfo);
        return true;
    }

    return false;
}

bool AnticheatData::CheckMovement(MovementInfo const& movementInfo, Unit* mover, bool jump)
{
    if (m_owner->movespline->Initialized() && !m_owner->movespline->Finalized())
    {
        return true;
    }

    if (sAnticheatMgr->isMapDisabledForAC(m_owner->GetMapId()) || sAnticheatMgr->isAreaDisabledForAC(m_owner->GetAreaId()))
    {
        return true;
    }

    if (sConfigMgr->GetOption<bool>("AntiCheats.FakeJumper.Enabled", true) && mover->IsFalling() && movementInfo.pos.GetPositionZ() > mover->GetPositionZ())
    {
        if (!IsJumpingbyOpcode() && !UnderACKmount() && !m_owner->IsFlying())
        {
            // fake jumper -> for example gagarin air mode with falling flag (like player jumping), but client can't sent a new coords when falling
            LOG_INFO("anticheat", "PassiveAnticheat: Fake jumper by Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
                m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), m_owner->GetMapId(),
                m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
            sWorld->SendGMText(LANG_GM_ANNOUNCE_JUMPER_FAKE, m_owner->GetName().c_str());
            RecordAntiCheatLog(FAKE_JUMP);
            if (sConfigMgr->GetOption<bool>("AntiCheats.FakeJumper.Kick.Enabled", true))
            {
                return false;
            }
        }
    }

    if (sConfigMgr->GetOption<bool>("AntiCheats.FakeFlyingmode.Enabled", true) && !IsCanFlybyServer() && !UnderACKmount() &&
        movementInfo.HasMovementFlag(MOVEMENTFLAG_MASK_MOVING_FLY) && !m_owner->IsInWater())
    {
        LOG_INFO("anticheat", "PassiveAnticheat: Fake flying mode (using MOVEMENTFLAG_FLYING flag doesn't restricted) by Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), m_owner->GetMapId(),
            m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
        sWorld->SendGMText(LANG_GM_ANNOUNCE_JUMPER_FLYING, m_owner->GetName().c_str());
        RecordAntiCheatLog(FAKE_FLY);
        if (sConfigMgr->GetOption<bool>("AntiCheats.FakeFlyingmode.Kick.Enabled", true))
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
        if (m_owner->ToUnit()->IsFalling() || m_owner->IsInFlight())
        {
            return true;
        }

        bool vehicle = false;
        if (m_owner->GetVehicleKit() && m_owner->GetVehicleKit()->GetBase())
            vehicle = true;

        if (m_owner->m_mover != m_owner)
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

        bool transportflag = (movementInfo.GetMovementFlags() & MOVEMENTFLAG_ONTRANSPORT) || m_owner->HasUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT);
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
            if (m_owner->GetTransOffsetX() == 0.f) // if it elevator or fist step - player can have zero this coord
                return true;

            x = m_owner->GetTransOffsetX();
            y = m_owner->GetTransOffsetY();
            z = m_owner->GetTransOffsetZ();
        }
        else
            m_owner->GetPosition(x, y, z);

        if (sConfigMgr->GetOption<bool>("AntiCheats.IgnoreControlMovement.Enabled", true))
        {
            if (m_owner->HasUnitState(UNIT_STATE_ROOT) && !UnderACKRootUpd())
            {
                bool unrestricted = npos.GetPositionX() != x || npos.GetPositionY() != y;
                if (unrestricted)
                {
                    LOG_INFO("anticheat", "CheckMovementInfo :  Ignore control Hack detected for Account id : %u, Player %s (%s), Position: %s, MovementFlags: %d",
                        m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
                        m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
                    sWorld->SendGMText(LANG_GM_ANNOUNCE_MOVE_UNDER_CONTROL, m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str());
                    RecordAntiCheatLog(IGNORE_CONTROL);

                    if (sConfigMgr->GetOption<bool>("AntiCheats.MoveUnderControl.Kick.Enabled", true))
                    {
                        return false;
                    }
                }
            }
        }

        float flyspeed = 0.f;
        float distance, runspeed, difftime, normaldistance, delay, diffPacketdelay;
        uint32 ptime;
        std::string mapname = m_owner->GetMap()->GetMapName();

        // calculate distance - don't use func, because x,z can be offset transport coords
        distance = sqrt((npos.GetPositionY() - y) * (npos.GetPositionY() - y) + (npos.GetPositionX() - x) * (npos.GetPositionX() - x));

        if (!jump && !m_owner->CanFly() && !m_owner->isSwimming() && !transportflag && distance > 0.f)
        {
            float diffz = fabs(movementInfo.pos.GetPositionZ() - z);
            float tanangle = distance / diffz;

            if (movementInfo.pos.GetPositionZ() > z && diffz > 1.87f && tanangle < 0.57735026919f) // 30 degrees
            {
                LOG_INFO("anticheat", "PassiveAnticheat: Climb Hack detected for Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
                    m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), m_owner->GetMapId(),
                    m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());

                sWorld->SendGMText(LANG_GM_ANNOUNCE_WALLCLIMB, m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), diffz, distance, tanangle, mapname.c_str(), m_owner->GetMapId(), x, y, z);
                RecordAntiCheatLog(CLIMB_HACK);

                if (sConfigMgr->GetOption<bool>("AntiCheats.Wallclimb.Kick.Enabled", true))
                {
                    return false;
                }
            }
        }

        uint32 oldstime = GetLastMoveServerTimestamp();
        uint32 stime = World::GetGameTimeMS();
        uint32 ping;
        ptime = movementInfo.time;

        if (!vehicle)
            runspeed = m_owner->GetSpeed(MOVE_RUN);
        else
            runspeed = m_owner->GetVehicleKit()->GetBase()->GetSpeed(MOVE_RUN);

        if (m_owner->isSwimming())
        {
            if (!vehicle)
                runspeed = m_owner->GetSpeed(MOVE_SWIM);
            else
                runspeed = m_owner->GetVehicleKit()->GetBase()->GetSpeed(MOVE_SWIM);
        }

        if (m_owner->IsFlying() || m_owner->CanFly())
        {
            if (!vehicle)
                flyspeed = m_owner->GetSpeed(MOVE_FLIGHT);
            else
                flyspeed = m_owner->GetVehicleKit()->GetBase()->GetSpeed(MOVE_FLIGHT);
        }

        if (flyspeed > runspeed)
            runspeed = flyspeed;

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

        LOG_INFO("anticheat", "PassiveAnticheat: SpeedHack Detected for Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), m_owner->GetMapId(),
            m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
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

        sWorld->SendGMText(LANG_GM_ANNOUNCE_ASH, m_owner->GetName().c_str(), normaldistance, distance);
        RecordAntiCheatLog(SPEED_HACK);
    }
    else
    {
        return true;
    }

    if (sConfigMgr->GetOption<bool>("AntiCheats.SpeedHack.Kick.Enabled", true))
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

bool AnticheatData::HandleDoubleJump(Unit* mover)
{
    SetJumpingbyOpcode(true);
    SetUnderACKmount();

    if (sAnticheatMgr->isMapDisabledForAC(m_owner->GetMapId()) || sAnticheatMgr->isAreaDisabledForAC(m_owner->GetAreaId()))
    {
        return true;
    }

    if (m_owner->movespline->Initialized() && !m_owner->movespline->Finalized())
    {
        return true;
    }

    if (mover->IsFalling())
    {
        LOG_INFO("anticheat", "PassiveAnticheat: Double jump by Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
            m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());

        sWorld->SendGMText(LANG_GM_ANNOUNCE_DOUBLE_JUMP, m_owner->GetName().c_str());

        RecordAntiCheatLog(DOUBLE_JUMP);

        if (sConfigMgr->GetOption<bool>("AntiCheats.DoubleJump.Enabled", true))
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
        if (sAnticheatMgr->isMapDisabledForAC(m_owner->GetMapId()) || sAnticheatMgr->isAreaDisabledForAC(m_owner->GetAreaId()))
        {
            SetSuccessfullyLanded();
            return true;
        }

        if (m_owner->movespline->Initialized() && !m_owner->movespline->Finalized())
        {
            SetSuccessfullyLanded();
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

        if (IsCanFlybyServer())
            checkNorm = true;

        if (!checkNorm)
        {
            LOG_INFO("anticheat", "PassiveAnticheat: NoFallingDamage by Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
                m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
                m_owner->GetMapId(), m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());

            sWorld->SendGMText(LANG_GM_ANNOUNCE_NOFALLINGDMG, m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str());

            RecordAntiCheatLog(NO_FALLING);

            if (sConfigMgr->GetOption<bool>("AntiCheats.NoFallingDmg.Kick.Enabled", false))
            {
                m_owner->GetSession()->KickPlayer("Kicked by anticheat::NoFallingDamage");
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
    }

    if (m_loadedFromDB)
    {
        LoginDatabase.PExecute("UPDATE `anticheat_logs` SET %s=%s + 1 WHERE `guid`=%d", cheatColumn, cheatColumn, m_owner->GetGUID().GetCounter());
    }
    else
    {
        m_loadedFromDB = true;
        LoginDatabase.PExecute("INSERT INTO anticheat_logs (`account`, `guid`, `player`, %s) VALUES (%d, %d, '%s', 1)",
            cheatColumn, m_owner->GetSession()->GetAccountId(), m_owner->GetGUID().GetCounter(), m_owner->GetName().c_str());
    }
}

void AnticheatData::HandleNoFallingDamage(uint16 opcode)
{
    if (!IsCanFlybyServer())
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
