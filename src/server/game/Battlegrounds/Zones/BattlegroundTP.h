/*
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __BATTLEGROUNDTP_H
#define __BATTLEGROUNDTP_H

class Battleground;

enum BG_TP_WorldStates
{
    TP_FLAGS_TOTAL         = 1601,
    TP_ALLIANCE_FLAGS      = 1581,
    TP_ALLIANCE_FLAGS_SHOW = 2339,
    TP_HORDE_FLAGS         = 1582,
    TP_HORDE_FLAGS_SHOW    = 2338,
    TP_TIME_DISPLAY        = 4247,
    TP_TIME_VALUE          = 4248 //minutes
};

enum BG_TP_Objectives
{
    TP_OBJECTIVE_FLAG_CAPTURE   = 290,
    TP_OBJECTIVE_FLAG_RETURN    = 291
};

class BattlegroundTPScore : public BattlegroundScore
{
    public:
        BattlegroundTPScore() {};
        virtual ~BattlegroundTPScore() {};
        uint32 FlagCaptures;
        uint32 FlagReturns;
};

class BattlegroundTP : public Battleground
{
    friend class BattlegroundMgr;

    public:
        BattlegroundTP();
        ~BattlegroundTP();
        void Update(uint32 diff);

        /* inherited from BattlegroundClass */
        virtual void AddPlayer(Player *plr);
        virtual void StartingEventCloseDoors();
        virtual void StartingEventOpenDoors();

        void RemovePlayer(Player *plr,uint64 guid);
        void HandleAreaTrigger(Player *Source, uint32 Trigger);
        bool SetupBattleground();
        void HandleKillUnit(Creature *unit, Player *killer);
        void EndBattleground(uint32 winner);
        void EventPlayerClickedOnFlag(Player *source, GameObject* /*target_obj*/);

        /* Scorekeeping */
        void UpdatePlayerScore(Player *Source, uint32 type, uint32 value, bool doAddHonor = true);

    private:
};
#endif
