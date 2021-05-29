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

    if (m_owner->IsFalling() || m_owner->IsFalling())
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
        LOG_INFO("anticheat", "PassiveAnticheat: FlyHack Detected for Account id : %u, Player %s (%s)",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str());
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
        float waterlevel = m_owner->GetBaseMap()->GetWaterLevel(npos.GetPositionX(), npos.GetPositionY()); // water walking
        bool hovergaura = m_owner->HasAuraType(SPELL_AURA_WATER_WALK) || m_owner->HasAuraType(SPELL_AURA_HOVER);
        if (waterlevel && (pz - waterlevel) <= (hovergaura ? m_owner->GetCollisionHeight() + 2.5f : m_owner->GetCollisionHeight() + 1.5f))
        {
            return true;
        }

        LOG_INFO("anticheat", "PassiveAnticheat: FlyHack Detected for Account id : %u, Player %s (%s)",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str());
        LOG_INFO("anticheat", "Player::========================================================");
        LOG_INFO("anticheat", "Player has a MOVEMENTFLAG_SWIMMING, but not in water");

        sWorld->SendGMText(LANG_GM_ANNOUNCE_AFK_SWIMMING, m_owner->GetName().c_str());
        return false;
    }
    else
    {
        float z = m_owner->GetMap()->GetHeight(m_owner->GetPhaseMask(), npos.GetPositionX(), npos.GetPositionY(), pz + m_owner->GetCollisionHeight() + 0.5f, true, 50.0f); // smart flyhacks -> SimpleFly
        float diff = pz - z;
        if (diff > 6.8f) // better calculate the second time for false situations, but not call GetHoverOffset everytime (economy resource)
        {
            float waterlevel = m_owner->GetBaseMap()->GetWaterLevel(npos.GetPositionX(), npos.GetPositionY()); // water walking
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
                    LOG_INFO("anticheat", "PassiveAnticheat: FlyHack Detected for Account id : %u, Player %s (%s)",
                        m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str());
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

bool AnticheatData::CheckMovement(MovementInfo const& movementInfo, Unit* mover, bool jump)
{
    if (!sAnticheatMgr->isMapDisabledForAC(m_owner->GetMapId()) && !sAnticheatMgr->isAreaDisabledForAC(m_owner->GetAreaId()))
    {
        if (sConfigMgr->GetOption<bool>("AntiCheats.FakeJumper.Enabled", true) && mover->IsFalling() && movementInfo.pos.GetPositionZ() > mover->GetPositionZ())
        {
            if (!IsJumpingbyOpcode())
            {
                SetJumpingbyOpcode(true);
                SetUnderACKmount();
            }
            else if (!UnderACKmount() && !m_owner->IsFlying())
            {
                // fake jumper -> for example gagarin air mode with falling flag (like player jumping), but client can't sent a new coords when falling
                LOG_INFO("anticheat", "PassiveAnticheat: Fake jumper by Account id : %u, Player %s (%s)",
                    m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str());
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
            LOG_INFO("anticheat", "PassiveAnticheat: Fake flying mode (using MOVEMENTFLAG_FLYING flag doesn't restricted) by Account id : %u, Player %s (%s)",
                m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str());
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

    uint32 ctime = GetLastMoveClientTimestamp();
    if (ctime)
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
                    LOG_INFO("anticheat", "CheckMovementInfo :  Ignore control Hack detected for Account id : %u, Player %s (%s)",
                        m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str());
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

        float distance, movetime, speed, difftime, normaldistance, delay, delaysentrecieve, x, y;
        distance = npos.GetExactDist2d(m_owner);

        if (!jump && !m_owner->CanFly() && !m_owner->isSwimming() && !transportflag)
        {
            float diffz = fabs(movementInfo.pos.GetPositionZ() - m_owner->GetPositionZ());
            float tanangle = distance / diffz;

            if (movementInfo.pos.GetPositionZ() > m_owner->GetPositionZ() && diffz > 1.87f &&
                tanangle < 0.57735026919f) // 30 degrees
            {
                std::string mapname = m_owner->GetMap()->GetMapName();
                LOG_INFO("anticheat", "PassiveAnticheat: Climb Hack detected for Account id : %u, Player %s (%s), diffZ = %f, distance = %f, angle = %f, Map = %s, mapId = %u, X = %f, Y = %f, Z = %f",
                    m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str(), diffz, distance, tanangle, mapname.c_str(),
                    m_owner->GetMapId(), m_owner->GetPositionX(), m_owner->GetPositionY(), m_owner->GetPositionZ());
                sWorld->SendGMText(LANG_GM_ANNOUNCE_WALLCLIMB, m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), diffz, distance, tanangle, mapname.c_str(),
                    m_owner->GetMapId(), m_owner->GetPositionX(), m_owner->GetPositionY(), m_owner->GetPositionZ());

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

        uint32 oldstime = GetLastMoveServerTimestamp();
        uint32 stime = getMSTime();
        uint32 ping, latency;
        movetime = movementInfo.time;
        latency = m_owner->GetSession()->GetLatency();
        ping = std::max(uint32(60), latency);

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

        delaysentrecieve = (ctime - oldstime) / 10000000000;
        delay = fabsf(movetime - stime) / 10000000000 + delaysentrecieve;
        difftime = (movetime - ctime + ping) * 0.001f + delay;
        normaldistance = speed * difftime; // if movetime faked and lower, difftime should be with "-"
        if (UnderACKmount() || transportflag)
        {
            normaldistance *= 3.0f;
        }

        if (distance < normaldistance)
        {
            return true;
        }

        m_owner->GetPosition(x, y);

        LOG_INFO("anticheat", "PassiveAnticheat: SpeedHack Detected for Account id : %u, Player %s (%s)",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str());
        LOG_INFO("anticheat", "========================================================");
        LOG_INFO("anticheat", "oldX = %f", x);
        LOG_INFO("anticheat", "oldY = %f", y);
        LOG_INFO("anticheat", "newX = %f", npos.GetPositionX());
        LOG_INFO("anticheat", "newY = %f", npos.GetPositionY());
        LOG_INFO("anticheat", "packetdistance = %f", distance);
        LOG_INFO("anticheat", "available distance = %f", normaldistance);
        LOG_INFO("anticheat", "movetime = %f", movetime);
        LOG_INFO("anticheat", "delay sent ptk - recieve pkt (previous) = %f", delaysentrecieve);
        LOG_INFO("anticheat", "FullDelay = %f", delay);
        LOG_INFO("anticheat", "difftime = %f", difftime);
        LOG_INFO("anticheat", "latency = %u", latency);
        LOG_INFO("anticheat", "ping = %u", ping);
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
    SetUnderACKmount();

    if (mover->IsFalling())
    {
        LOG_INFO("anticheat", "PassiveAnticheat: Double jump by Account id : %u, Player %s (%s)",
            m_owner->GetSession()->GetAccountId(), m_owner->GetName().c_str(), m_owner->GetGUID().ToString().c_str());
        sWorld->SendGMText(LANG_GM_ANNOUNCE_DOUBLE_JUMP, m_owner->GetName().c_str());
        if (sConfigMgr->GetOption<bool>("AntiCheats.DoubleJump.Enabled", true))
        {
            return false;
        }
    }

    return true;
}
