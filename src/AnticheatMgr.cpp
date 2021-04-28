/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AnticheatMgr.h"
#include "AnticheatData.h"
#include "Config.h"
#include "Log.h"
#include "Player.h"

AnticheatMgr::AnticheatMgr()
{
}

AnticheatMgr::~AnticheatMgr()
{
	m_Players.clear();
}

void AnticheatMgr::SetExcludedMaps()
{
    excludeACMapsId.clear();

    std::stringstream excludeStream(sConfigMgr->GetOption<std::string>("AntiCheats.forceExcludeMapsid", ""));
    std::string temp;
    while (std::getline(excludeStream, temp, ','))
        excludeACMapsId.insert(atoi(temp.c_str()));

    LOG_INFO("anticheat", "AntiCheats disabled for %u maps", (uint32)excludeACMapsId.size());
}

void AnticheatMgr::HandlePlayerLogin(Player* player)
{
    AnticheatData anticheatData(player);
    anticheatData.SetLastMoveClientTimestamp(getMSTime());
    anticheatData.SetLastMoveServerTimestamp(getMSTime());

    m_Players[player->GetGUID()] = std::move(anticheatData);
}

void AnticheatMgr::HandlePlayerLogout(Player* player)
{
    m_Players.erase(player->GetGUID());
}

void AnticheatMgr::DeleteCommand(ObjectGuid guid /*= ObjectGuid::Empty*/)
{
    if (!guid)
        m_Players.clear();
    else
        m_Players.erase(guid);
}

void AnticheatMgr::SetClientTimestamps(Player* player)
{
    auto itr = m_Players.find(player->GetGUID());
    if (itr != m_Players.end())
    {
        itr->second.SetLastMoveClientTimestamp(getMSTime());
        itr->second.SetLastMoveServerTimestamp(getMSTime());
    }
}

void AnticheatMgr::Update(Player* player, uint32 time)
{
    auto itr = m_Players.find(player->GetGUID());
    if (itr != m_Players.end())
        itr->second.Update(time);
}

void AnticheatMgr::SetSkipOnePacketForASH(Player* player, bool apply)
{
    auto itr = m_Players.find(player->GetGUID());
    if (itr != m_Players.end())
        itr->second.SetSkipOnePacketForASH(apply);
}

void AnticheatMgr::SetCanFlybyServer(Player* player, bool apply)
{
    auto itr = m_Players.find(player->GetGUID());
    if (itr != m_Players.end())
        itr->second.SetCanFlybyServer(apply);
}

void AnticheatMgr::SetUnderACKmount(Player* player)
{
    auto itr = m_Players.find(player->GetGUID());
    if (itr != m_Players.end())
        itr->second.SetUnderACKmount();
}

void AnticheatMgr::SetRootACKUpd(Player* player)
{
    auto itr = m_Players.find(player->GetGUID());
    if (itr != m_Players.end())
        itr->second.SetRootACKUpd();
}

void AnticheatMgr::SetJumpingbyOpcode(Player* player, bool jump)
{
    auto itr = m_Players.find(player->GetGUID());
    if (itr != m_Players.end())
        itr->second.SetJumpingbyOpcode(jump);
}

void AnticheatMgr::UpdateMovementInfo(Player* player, MovementInfo const& movementInfo)
{
    auto itr = m_Players.find(player->GetGUID());
    if (itr != m_Players.end())
        itr->second.UpdateMovementInfo(movementInfo);
}

bool AnticheatMgr::HandleDoubleJump(Player* player, Unit* mover)
{
    auto itr = m_Players.find(player->GetGUID());
    if (itr != m_Players.end())
        return itr->second.HandleDoubleJump(mover);

    return true;
}

bool AnticheatMgr::CheckMovementInfo(Player* player, MovementInfo const& movementInfo, Unit* mover, bool jump)
{
    auto itr = m_Players.find(player->GetGUID());
    if (itr != m_Players.end())
        return itr->second.CheckMovementInfo(movementInfo, mover, jump);

    return true;
}

