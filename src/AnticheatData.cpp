#include "AnticheatData.h"
#include "AnticheatMgr.h"
#include "AnticheatLanguage.h"
#include "Config.h"
#include "Player.h"
#include "Vehicle.h"

#define DEFAULT_PLAYER_BOUNDING_RADIUS      0.388999998569489f     // player size, also currently used (correctly?) for any non Unit world objects

AnticheatData::AnticheatData(Player* player /* =nullptr*/) : m_owner(player)
{
    m_flyhackTimer = 0;
    if (sConfigMgr->GetOption<bool>("AntiCheats.FlyHack.Enabled", true))
    {
        m_flyhackTimer = sConfigMgr->GetOption<int32>("AntiCheats.FlyHackTimer", 1000);
    }

    m_mountTimer = 0;
    m_rootUpdTimer = 0;
    m_ACKmounted = false;
    m_rootUpd = false;
    m_skipOnePacketForASH = true;
    m_isjumping = false;
    m_canfly = false;
    lastMoveClientTimestamp = 0;
    lastMoveServerTimestamp = 0;
}

AnticheatData::~AnticheatData()
{
}

void AnticheatData::Update(uint32 time)
{
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
}

void AnticheatData::SetUnderACKmount()
{
    m_mountTimer = 3000;
    m_ACKmounted = true;
}

void AnticheatData::SetRootACKUpd()
{
    uint32 pinginthismoment = fabs(GetLastMoveClientTimestamp() - GetLastMoveServerTimestamp()) / 1000000;
    m_rootUpdTimer = 1500 + pinginthismoment;
    m_rootUpd = true;
}

bool AnticheatData::CheckOnFlyHack()
{
    if (IsCanFlybyServer())
    {
        return true;
    }

    if ((m_owner->movespline->Initialized() && !m_owner->movespline->Finalized()) || m_owner->ToUnit()->IsFalling() || m_owner->IsFalling())
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
        return false;
    }

    if (m_owner->IsFlying() || m_owner->IsLevitating() || m_owner->IsInFlight())
    {
        return true;
    }

    if (m_owner->GetVehicle() || m_owner->GetVehicleKit())
    {
        return true;
    }

    if (m_owner->HasAuraType(SPELL_AURA_CONTROL_VEHICLE))
    {
        return true;
    }

    if (m_owner->GetTransport() || m_owner->HasUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT))
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

    Position npos;
    m_owner->GetPosition(&npos);
    float pz = npos.GetPositionZ();
    if (!m_owner->IsInWater() && m_owner->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
    {
        LiquidData liquid_status;
        m_owner->GetMap()->getLiquidStatus(npos.GetPositionX(), npos.GetPositionY(), pz, MAP_ALL_LIQUIDS, &liquid_status, m_owner->GetCollisionHeight());

        float waterlevel = liquid_status.level; // water walking
        bool hovergaura = m_owner->HasAuraType(SPELL_AURA_WATER_WALK) || m_owner->HasAuraType(SPELL_AURA_HOVER);
        if (waterlevel && (pz - waterlevel) <= (hovergaura ? m_owner->GetCollisionHeight() + 2.5f : m_owner->GetCollisionHeight() + 1.5f))
        {
            return true;
        }

        LOG_INFO("anticheat", "PassiveAnticheat: FlyHack Detected for Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), m_owner->GetMapId(),
            m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
        LOG_INFO("anticheat", "Player::========================================================");
        LOG_INFO("anticheat", "Player has a MOVEMENTFLAG_SWIMMING, but not in water");

        sWorld->SendGMText(LANG_GM_ANNOUNCE_AFK_SWIMMING, m_owner->GetName().c_str());
        return false;
    }
    else if (!m_owner->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
    {
        float z = m_owner->GetMap()->GetHeight(m_owner->GetPhaseMask(), npos.GetPositionX(), npos.GetPositionY(), pz + m_owner->GetCollisionHeight() + 0.5f, true, 50.0f); // smart flyhacks -> SimpleFly
        float diff = pz - z;
        if (diff > 6.8f) // better calculate the second time for false situations, but not call GetHoverOffset everytime (economy resource)
        {
            LiquidData liquid_status;
            m_owner->GetMap()->getLiquidStatus(npos.GetPositionX(), npos.GetPositionY(), pz, MAP_ALL_LIQUIDS, &liquid_status, m_owner->GetCollisionHeight());

            float waterlevel = liquid_status.level; // water walking
            if (waterlevel && waterlevel + m_owner->GetCollisionHeight() > pz)
            {
                return true;
            }

            float cx, cy, cz;
            m_owner->GetVoidClosePoint(cx, cy, cz, DEFAULT_PLAYER_BOUNDING_RADIUS, 2.0f, 0, 6.8f); // first check
            if (pz - cz > 6.8f)
            {
                m_owner->GetMap()->getObjectHitPos(m_owner->GetPhaseMask(), m_owner->GetPositionX(), m_owner->GetPositionY(),
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
                    return false;
                }
            }
        }
    }

    return true;
}

void AnticheatData::UpdateMovementInfo(MovementInfo const& movementInfo)
{
    SetLastMoveClientTimestamp(movementInfo.time);
    SetLastMoveServerTimestamp(getMSTime());
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

inline float tangent(float x)
{
    x = std::tan(x);
    //if (x < std::numeric_limits<float>::max() && x > -std::numeric_limits<float>::max()) return x;
    //if (x >= std::numeric_limits<float>::max()) return std::numeric_limits<float>::max();
    //if (x <= -std::numeric_limits<float>::max()) return -std::numeric_limits<float>::max();
    if (x < 100000.0f && x > -100000.0f)
    {
        return x;
    }

    if (x >= 100000.0f)
    {
        return 100000.0f;
    }

    if (x <= 100000.0f)
    {
        return -100000.0f;
    }

    return 0.0f;
}

bool AnticheatData::CheckMovement(MovementInfo const& movementInfo, Unit* mover, bool jump)
{
    if (!sAnticheatMgr->isMapDisabledForAC(m_owner->GetMapId()) && !sAnticheatMgr->isAreaDisabledForAC(m_owner->GetAreaId()))
    {
        if (sConfigMgr->GetOption<bool>("AntiCheats.FakeJumper.Enabled", true) && mover->IsFalling() && movementInfo.pos.GetPositionZ() > mover->GetPositionZ())
        {
            if (!IsJumpingbyOpcode() && !UnderACKmount() && !m_owner->IsFlying())
            {
                // fake jumper -> for example gagarin air mode with falling flag (like player jumping), but client can't sent a new coords when falling
                LOG_INFO("anticheat", "PassiveAnticheat: Fake jumper by Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
                    m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), m_owner->GetMapId(),
                    m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
                sWorld->SendGMText(LANG_GM_ANNOUNCE_JUMPER_FAKE, m_owner->GetName().c_str());
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
            if (sConfigMgr->GetOption<bool>("AntiCheats.FakeFlyingmode.Kick.Enabled", true))
            {
                return false;
            }
        }
    }

    if (!sConfigMgr->GetOption<bool>("AntiCheats.SpeedHack.Enabled", true))
    {
        return true;
    }

    if (sAnticheatMgr->isMapDisabledForAC(m_owner->GetMapId()) || sAnticheatMgr->isAreaDisabledForAC(m_owner->GetAreaId()))
    {
        return true;
    }

    uint32 oldctime = GetLastMoveClientTimestamp();
    if (oldctime)
    {
        if (m_owner->IsFalling() || m_owner->IsInFlight())
        {
            return true;
        }

        bool vehicle = false;
        if (m_owner->GetVehicleKit() && m_owner->GetVehicleKit()->GetBase())
            vehicle = true;

        if (m_owner->GetVehicle())
        {
            return true;
        }

        bool transportflag = m_owner->GetTransport() || (movementInfo.GetMovementFlags() & MOVEMENTFLAG_ONTRANSPORT) || m_owner->HasUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT);

        if (sConfigMgr->GetOption<bool>("AntiCheats.SafeMode.Enabled", true))
        {
            if (UnderACKmount() || transportflag)
            {
                return true;
            }
        }

        if (IsSkipOnePacketForASH())
        {
            SetSkipOnePacketForASH(false);
            return true;
        }

        Position npos = movementInfo.pos;
        if (sConfigMgr->GetOption<bool>("AntiCheats.IgnoreControlMovement.Enabled", true))
        {
            if (m_owner->HasUnitState(UNIT_STATE_ROOT) && !UnderACKRootUpd())
            {
                bool unrestricted = npos.GetPositionX() != m_owner->GetPositionX() || npos.GetPositionY() != m_owner->GetPositionY();
                if (unrestricted)
                {
                    LOG_INFO("anticheat", "CheckMovementInfo :  Ignore control Hack detected for Account id : %u, Player %s (%s), Position: %s, MovementFlags: %d",
                        m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
                        m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
                    sWorld->SendGMText(LANG_GM_ANNOUNCE_MOVE_UNDER_CONTROL, m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str());

                    if (sConfigMgr->GetOption<bool>("AntiCheats.SpeedHack.Kick.Enabled", true))
                    {
                        return false;
                    }
                    else
                    {
                        return true;
                    }
                }
            }
        }

        if (m_owner->HasUnitState(UNIT_STATE_IGNORE_ANTISPEEDHACK))
        {
            return true;
        }

        float distance = npos.GetExactDist2d(m_owner);
        if (!jump && !m_owner->CanFly() && !m_owner->isSwimming() && !transportflag)
        {
            float deltaZ = fabs(m_owner->GetPositionZ() - movementInfo.pos.GetPositionZ());
            float deltaXY = distance;

            // Prevent divide by 0
            if (!G3D::fuzzyEq(deltaXY, 0.f))
            {
                float angle = Position::NormalizeOrientation(tangent(deltaZ / deltaXY));
                if (angle > 1.9f)
                {
                    std::string mapname = m_owner->GetMap()->GetMapName();
                    LOG_INFO("anticheat", "PassiveAnticheat: Climb Hack detected for Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
                        m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), m_owner->GetMapId(),
                        m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());

                    if (sConfigMgr->GetOption<bool>("AntiCheats.SpeedHack.Kick.Enabled", true))
                    {
                        return false;
                    }
                    else
                    {
                        return true;
                    }
                }
            }
        }

        float speed = 0.f;
        if (!vehicle)
        {
            speed = m_owner->GetSpeed(MOVE_RUN);
        }
        else
        {
            speed = m_owner->GetVehicleKit()->GetBase()->GetSpeed(MOVE_RUN);
        }

        if (m_owner->isSwimming())
        {
            if (!vehicle)
            {
                speed = m_owner->GetSpeed(MOVE_SWIM);
            }
            else
            {
                speed = m_owner->GetVehicleKit()->GetBase()->GetSpeed(MOVE_SWIM);
            }
        }

        if (m_owner->IsFlying() || m_owner->CanFly())
        {
            if (!vehicle)
            {
                speed = m_owner->GetSpeed(MOVE_FLIGHT);
            }
            else
            {
                speed = m_owner->GetVehicleKit()->GetBase()->GetSpeed(MOVE_FLIGHT);
            }
        }

        uint32 oldstime = GetLastMoveServerTimestamp();
        uint32 stime = getMSTime();
        uint32 ptime = movementInfo.time;

        float delay = static_cast<float>(ptime - oldctime);
        float diffPacketdelay = 10000000.f - delay;

        if (oldctime > ptime)
        {
            delay = 0.f;
        }

        diffPacketdelay = diffPacketdelay * 0.0000000001f;

        float difftime = delay * 0.001f + diffPacketdelay;
        float normaldistance = (speed * difftime) + 0.002f; // 0.002f a little safe temporary hack
        if (UnderACKmount() || transportflag)
        {
            normaldistance += 20.0f;
        }

        if (distance < normaldistance)
        {
            return true;
        }

        uint32 ping = uint32(diffPacketdelay * 10000.f);
        uint32 latency = m_owner->GetSession()->GetLatency();

        float x, y;
        m_owner->GetPosition(x, y);

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
        LOG_INFO("anticheat", "latency = %u", latency);
        LOG_INFO("anticheat", "========================================================");

        sWorld->SendGMText(LANG_GM_ANNOUNCE_ASH, m_owner->GetName().c_str(), normaldistance, distance);
    }
    else
        {
        return true;
    }

    if (sConfigMgr->GetOption<bool>("AntiCheats.SpeedHack.Kick.Enabled", true))
    {
        return false;
    }

    return true;
}

bool AnticheatData::HandleDoubleJump(Unit* mover)
{
    SetJumpingbyOpcode(true);
    SetUnderACKmount();

    if (mover->IsFalling())
    {
        LOG_INFO("anticheat", "PassiveAnticheat: Double jump by Account id : %u, Player %s (%s), Map: %d, Position: %s, MovementFlags: %d",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(),
            m_owner->GetPosition().ToString().c_str(), m_owner->GetUnitMovementFlags());
        sWorld->SendGMText(LANG_GM_ANNOUNCE_DOUBLE_JUMP, m_owner->GetName().c_str());
        if (sConfigMgr->GetOption<bool>("AntiCheats.DoubleJump.Enabled", true))
        {
            return false;
        }
    }

    return true;
}
