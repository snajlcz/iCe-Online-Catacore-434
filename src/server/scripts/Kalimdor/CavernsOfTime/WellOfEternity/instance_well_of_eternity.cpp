/*
 * Copyright (C) 2006-2013 iCe Online <http://ice-wow.eu>
 *
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

#include "ScriptPCH.h"
#include "well_of_eternity.h"

#define INST_WOE_SCRIPT instance_well_of_eternity::instance_well_of_eternity_InstanceMapScript

#define CRYSTAL_ACTIVE (0)
#define CRYSTAL_INACTIVE (1)

void INST_WOE_SCRIPT::Initialize()
{
    demonPulseDone = false;
    abyssalSlained = false;

    illidanGUID = 0;
    perotharnGUID = 0;
    queenGUID = 0;
    mannorothGUID = 0;
    varothenGUID = 0;
    illidanPreludeEntry = 0;

    legionTimer = 2000;
    waveCounter = WAVE_ONE;

    crystalsRemaining = MAX_CRYSTALS;
    deffendersKilled = 0;

    memset(m_auiEncounter, 0, sizeof(uint32)* MAX_ENCOUNTER);
    memset(crystals, CRYSTAL_ACTIVE, sizeof(uint32)* MAX_CRYSTALS);
    connectors.clear();
    guardians.clear();
    crystalGUIDS.clear();
    doomguards.clear();

    GetCorrUiEncounter();
}

std::string INST_WOE_SCRIPT::GetSaveData()
{
    OUT_SAVE_INST_DATA;

    std::ostringstream saveStream;
    saveStream << m_auiEncounter[0];
    for (uint8 i = 1; i < MAX_ENCOUNTER; i++)
        saveStream << " " << m_auiEncounter[i];

    OUT_SAVE_INST_DATA_COMPLETE;
    return saveStream.str();
}

void INST_WOE_SCRIPT::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    for (uint8 i = 0; i < MAX_ENCOUNTER; i++)
        loadStream >> m_auiEncounter[i];

    for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
    {
        if (m_auiEncounter[i] == IN_PROGRESS)
            m_auiEncounter[i] = NOT_STARTED;
    }

    GetCorrUiEncounter();
    OUT_LOAD_INST_DATA_COMPLETE;
}

void INST_WOE_SCRIPT::OnCreatureCreate(Creature* pCreature, bool add)
{
    if (!add)
        return;

    switch (pCreature->GetEntry())
    {
        case PEROTHARN_ENTRY:
            perotharnGUID = pCreature->GetGUID();
            break;
        case QUEEN_AZSHARA_ENTRY:
            queenGUID = pCreature->GetGUID();
            break;
        case MANNOROTH_ENTRY:
            mannorothGUID = pCreature->GetGUID();
            break;
        case VAROTHEN_ENTRY:
            varothenGUID = pCreature->GetGUID();
            break;
        case PORTAL_CONNECTOR_1_ENTRY:
        case PORTAL_CONNECTOR_2_ENTRY:
        case PORTAL_CONNECTOR_3_ENTRY:
        {
            CONNECTOR_INFO ci (pCreature->GetGUID(), pCreature->GetEntry());
            connectors.push_back(ci);
            break;
        }
        case GUARDIAN_DEMON_ENTRY:
            guardians.push_back(pCreature->GetGUID());
            break;
        case ILLIDAN_STORMRAGE_ENTRY:
            illidanGUID = pCreature->GetGUID();
            break;
        case ENTRY_ILLIDAN_PRELUDE:
            illidanPreludeEntry = pCreature->GetGUID();
            break;
        case ENTRY_DOOMGUARD_ANNIHILATOR:
            doomguards.push_back(pCreature->GetGUID());
            break;
    }
}

void INST_WOE_SCRIPT::OnGameObjectCreate(GameObject* go, bool add)
{
    if (add == false)
        return;

    switch (go->GetEntry())
    {
        case PORTAL_ENERGY_FOCUS_ENTRY:
            crystalGUIDS.push_back(go->GetGUID());
            break;
    }
}

uint32 INST_WOE_SCRIPT::GetData(uint32 DataId)
{
    if (DataId < MAX_ENCOUNTER)
        return m_auiEncounter[DataId];

    return 0;
}

void INST_WOE_SCRIPT::SetData(uint32 type, uint32 data)
{
    if (type < MAX_ENCOUNTER)
        m_auiEncounter[type] = data;

    if (data == DONE)
    {
        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0];
        for (uint8 i = 1; i < MAX_ENCOUNTER; i++)
            saveStream << " " << m_auiEncounter[i];

        GetCorrUiEncounter();
        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint64 INST_WOE_SCRIPT::GetData64(uint32 type)
{
    switch (type)
    {
        case DATA_PEROTHARN:
            return perotharnGUID;
        case DATA_QUEEN_AZSHARA:
            return queenGUID;
        case DATA_CAPTAIN_VAROTHEN:
            return varothenGUID;
        case DATA_MANNOROTH:
            return mannorothGUID;
        case DATA_ILLIDAN:
            return illidanGUID;
        case ENTRY_ILLIDAN_PRELUDE:
            return illidanPreludeEntry;
    }
    return 0;
}

void INST_WOE_SCRIPT::TurnOffConnectors(uint32 connEntry, Creature * source)
{
    crystalsRemaining--;

    for (std::list<CONNECTOR_INFO>::iterator it = connectors.begin(); it != connectors.end(); it++)
    {
        CONNECTOR_INFO conInfo = *it;

        if (conInfo.entry == connEntry)
        if (Creature * pConnector = ObjectAccessor::GetCreature(*source, conInfo.guid))
        {
            pConnector->InterruptNonMeleeSpells(false);
            pConnector->CastSpell(pConnector,SPELL_CONNECTOR_DEAD,true);
        }
    }
}

void INST_WOE_SCRIPT::CrystalDestroyed(uint32 crystalXCoord)
{
    switch (crystalXCoord)
    {
        case FIRST_CRYSTAL_X_COORD:
            crystals[0] = CRYSTAL_INACTIVE;
            break;
        case SECOND_CRYSTAL_X_COORD:
            crystals[1] = CRYSTAL_INACTIVE;
            break;
        case THIRD_CRYSTAL_X_COORD:
            crystals[2] = CRYSTAL_INACTIVE;
            break;
    }
}

bool INST_WOE_SCRIPT::IsPortalClosing(DemonDirection dir)
{
    if (crystals[uint32(dir)] == CRYSTAL_INACTIVE)
        return true;
    return false;
}

void INST_WOE_SCRIPT::OnDeffenderDeath(Creature * deffender)
{
    ++deffendersKilled;

    if (deffendersKilled % 3 == 0)
    {
        if (GameObject * pGoCrystal = deffender->FindNearestGameObject(PORTAL_ENERGY_FOCUS_ENTRY, 200.0f))
            pGoCrystal->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);

        if (Creature* pIllidan = ObjectAccessor::GetObjectInMap(illidanGUID, this->instance, (Creature*)NULL))
            pIllidan->AI()->DoAction(ACTION_ON_ALL_DEFFENDERS_KILLED);
    }
}

bool INST_WOE_SCRIPT::IsIlidanCurrentWPReached(uint64 victimGUID)
{
    bool result = false;
    if (Creature * pIllidanVictim = ObjectAccessor::GetObjectInMap(victimGUID, this->instance, (Creature*)NULL))
    if (Creature * pCrystalStalker = pIllidanVictim->FindNearestCreature(FEL_CRYSTAL_STALKER_ENTRY,100.0f,true))
    {
        uint32 xPos = pCrystalStalker->GetPositionX();

        // Attacker from first group
        if (xPos == FIRST_CRYSTAL_X_COORD && illidanStep == ILLIDAN_FIRST_PACK_STEP)
            result = true;
        else if (xPos == SECOND_CRYSTAL_X_COORD && illidanStep == ILLIDAN_SECOND_PACK_STEP)
            result = true;
        else if (xPos == THIRD_CRYSTAL_X_COORD && illidanStep == ILLIDAN_THIRD_PACK_STEP)
            result = true;

        if (pIllidanVictim->GetEntry() == PEROTHARN_ENTRY) // Should be no problem
            return true;
    }
    return result;
}

void INST_WOE_SCRIPT::RegisterIllidanVictim(uint64 victimGUID)
{
    illidanVictims.push_back(victimGUID);
}

void INST_WOE_SCRIPT::UnRegisterIllidanVictim(uint64 victimGUID)
{
    illidanVictims.remove(victimGUID);

    if (illidanVictims.empty())
    if (Creature * pIllidan = ObjectAccessor::GetObjectInMap(illidanGUID, this->instance, (Creature*)NULL))
        if(IsIlidanCurrentWPReached(victimGUID))
            pIllidan->GetMotionMaster()->MovePoint(ILLIDAN_ATTACK_END_WP, illidanPos[illidanStep],true);
}

Creature * INST_WOE_SCRIPT::GetIllidanVictim()
{
    if (illidanVictims.empty())
        return NULL;

    if (IsIlidanCurrentWPReached(illidanVictims.front()) == false)
        return NULL;

    return ObjectAccessor::GetObjectInMap(illidanVictims.front(), this->instance, (Creature*)NULL);
}

Creature * INST_WOE_SCRIPT::GetGuardianByDirection(uint32 direction)
{
    for (std::list<uint64>::iterator it = guardians.begin(); it != guardians.end(); it++)
    {
        if (Creature * pGuardian = ObjectAccessor::GetObjectInMap(*it, this->instance, (Creature*)NULL))
        {
            if (pGuardian->AI()->GetData(DATA_GET_GUARDIAN_LANE) == direction)
                return pGuardian;
        }
    }
    return NULL;
}

void INST_WOE_SCRIPT::SummonAndInitDemon(Creature * summoner,Position summonPos, DemonDirection dir, DemonWave wave, Position movePos, WayPointStep wpID)
{
    Creature * pDemon = NULL;

    // Dont summon other demons if demon portal is closing -> cause crystals were destroyed
    if (crystals[dir] == CRYSTAL_INACTIVE)
        return;

    pDemon = summoner->SummonCreature(LEGION_PORTAL_DEMON, summonPos);

    if (pDemon)
    {
        pDemon->AI()->SetData(DEMON_DATA_DIRECTION, dir);
        pDemon->AI()->SetData(DEMON_DATA_WAVE, wave);
        pDemon->GetMotionMaster()->MovePoint(wpID, movePos,true);
    }
}

void INST_WOE_SCRIPT::Update(uint32 diff)
{
    if (!instance->HavePlayers())
        return;

    if (legionTimer <= diff)
    {
        if (Creature * pPerotharn = this->instance->GetCreature(perotharnGUID))
        {
            if (pPerotharn->isDead() || crystalsRemaining == 0)
                return; //  be carefull

            switch (waveCounter)
            {
                case WAVE_ONE: // Going to mid position (LEFT/RIGHT)
                {
                    SummonAndInitDemon(pPerotharn, legionDemonSpawnPositions[DIRECTION_LEFT],
                                       DIRECTION_LEFT, waveCounter, midPositions[DIRECTION_LEFT], WP_MID);

                    SummonAndInitDemon(pPerotharn, legionDemonSpawnPositions[DIRECTION_RIGHT],
                                       DIRECTION_RIGHT, waveCounter, midPositions[DIRECTION_RIGHT], WP_MID);

                    waveCounter = WAVE_TWO;
                    break;
                }
                case WAVE_TWO: // Going to farther mid position (LEFT/RIGHT)
                {
                    SummonAndInitDemon(pPerotharn, legionDemonSpawnPositions[DIRECTION_LEFT],
                                       DIRECTION_LEFT, waveCounter, midPositions2[DIRECTION_LEFT], WP_MID);

                    SummonAndInitDemon(pPerotharn, legionDemonSpawnPositions[DIRECTION_RIGHT],
                                       DIRECTION_RIGHT, waveCounter, midPositions2[DIRECTION_RIGHT], WP_MID);

                    waveCounter = WAVE_THREE;
                    break;
                }
                case WAVE_THREE: // Going straight till end (END)
                {
                    SummonAndInitDemon(pPerotharn, legionDemonSpawnPositions[DIRECTION_LEFT],
                                       DIRECTION_STRAIGHT, waveCounter, frontEndPositions[DIRECTION_LEFT], WP_END);

                    SummonAndInitDemon(pPerotharn, legionDemonSpawnPositions[DIRECTION_RIGHT],
                                       DIRECTION_STRAIGHT, waveCounter, frontEndPositions[DIRECTION_RIGHT], WP_END);

                    waveCounter = WAVE_ONE;
                    break;
                }
            }
        }
        legionTimer = 1500;
    }
    else legionTimer -= diff;
}

void INST_WOE_SCRIPT::CallDoomGuardsForHelp(Unit * victim)
{
    for (std::list<uint64>::iterator itr = doomguards.begin(); itr != doomguards.end(); ++itr)
    {
        if (Creature * doomGuard = ObjectAccessor::GetObjectInMap(*itr, this->instance, (Creature*)NULL))
        {
            if (doomGuard->isDead())
                continue;

            doomGuard->AddThreat(victim, 10.0f);
            doomGuard->SetInCombatWith(victim);
            victim->SetInCombatWith(doomGuard);
        }
    }
}

uint32* INST_WOE_SCRIPT::GetCorrUiEncounter()
{
    currEnc[0] = m_auiEncounter[DATA_PEROTHARN];
    currEnc[1] = m_auiEncounter[DATA_QUEEN_AZSHARA];
    currEnc[2] = m_auiEncounter[DATA_MANNOROTH];
    sInstanceSaveMgr->setInstanceSaveData(instance->GetInstanceId(), currEnc, MAX_ENCOUNTER);

    return NULL;
}

void AddSC_instance_well_of_eternity()
{
    new instance_well_of_eternity();
}

// UPDATE `instance_template` SET `script`='instance_well_of_eternity' WHERE  `map`=939 LIMIT 1;