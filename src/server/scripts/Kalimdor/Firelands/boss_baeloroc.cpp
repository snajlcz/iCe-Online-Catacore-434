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
#include "firelands.h"

enum Spells
{
    BERSERK                     = 26662,
    BLAZE_OF_GLORY              = 99252,
    INCENDIARY_SOUL             = 99369,

    DECIMATION_BLADE            = 99352,
    DECIMATION_BLADE_TRIGGERED  = 99353,
    INFERNO_BLADE               = 99350,
    INFERNO_BLADE_TRIGGERED     = 99351,

    VITAL_SPARK                 = 99262,
    VITAL_FLAME                 = 99263,

    SUMMON_SHARD_DUMMY          = 99259,
    SUMMON_SHARD                = 99260,

    // HEROIC SPELLS
    COUNTDOWN                   = 99516,
    COUNTDOWN_TRIGGERED         = 99517,

};

struct Yells
{
    uint32 sound;
    const char * text;
};

static const Yells onAggro = {24441,"You are forbidden from my master's domain, mortals."};

static const Yells onShard = {24446,"Fool mortals. Hurl yourselves into your own demise!"};

static const Yells onInfernoBlade = {24459,"Burn beneath my molten fury!"};

static const Yells onDecimationBlade = {24447,"By the Firelord's command, you, too, shall perish!"};

static const Yells onBerserk = {24450,"Your flesh is forfeit to the fires of this realm."};

static const Yells onDeath = {24444,"Mortal filth... the master's keep is forbidden..."};


static const Yells onKill[3]=
{
    {24449, "You have been judged."},
    {24451, "Behold your weakness."},
    {24452, "None shall pass!"},
};

enum actions
{
    DO_EQUIP_INFERNO_BLADE              = 0,
    DO_EQUIP_DECIMATION_BLADE           = 1,
    DO_EQUIP_NORMAL_BLADE               = 2,
};

# define MINUTE (60000)
# define NEVER  (4294967295) // used as "delayed" timer ( max uint32 value )


enum ItemEntries // Thanks Gregory for help with sniffing :)
{
    NORMAL_BLADE_ENTRY        = 71055,
    INFERNO_BLADE_ENTRY       = 94155,
    DECIMATION_BLADE_ENTRY    = 94157,
};

class boss_baleroc : public CreatureScript
{
public:
    boss_baleroc() : CreatureScript("boss_baleroc") { }
  
    CreatureAI* GetAI(Creature* pCreature) const
    {
        return new boss_balerocAI(pCreature);
    }

    struct boss_balerocAI : public ScriptedAI
    {
        boss_balerocAI(Creature *c) : ScriptedAI(c),Summons(c)
        {
            me->SetFlag(UNIT_FIELD_FLAGS,UNIT_FLAG_NON_ATTACKABLE);
            instance = me->GetInstanceScript();
            SetEquipmentSlots(false, NORMAL_BLADE_ENTRY, NORMAL_BLADE_ENTRY, EQUIP_NO_CHANGE); // Set blades to both hands
            me->SummonGameObject(208906,126.92f,-63.55f,55.27f,2.5823f,0,0,0,0,0); // Fire wall
        }

        uint32 blazeOfGloryTimer;
        uint32 decimationBladeTimer;
        uint32 infernoBladeTimer;
        uint32 berserkTimer;
        uint32 castShardTimer;
        uint32 summonShardTimer;
        bool meleePhase;

        InstanceScript * instance;
        SummonList Summons;

        void Reset()
        {
            if(instance)
            {
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
                instance->SetData(TYPE_BALEROC, NOT_STARTED);
            }

            castShardTimer          = 4000;
            summonShardTimer        = NEVER;
            blazeOfGloryTimer       = 8000;
            decimationBladeTimer    = 28000;
            infernoBladeTimer       = NEVER;
            berserkTimer            = 6 * MINUTE;
            meleePhase = true;
        }

        void JustSummoned(Creature* summon)
        {
            Summons.push_back(summon->GetGUID());
        }

       void KilledUnit(Unit * /*victim*/)
       {
           uint8 _rand = urand(0,2);
           PlayAndYell(onKill[_rand].sound,onKill[_rand].text);
       }

        void JustDied(Unit* /*killer*/)
        {
            if (GameObject * door1 = me->FindNearestGameObject(208906,200.0f))
                door1->Delete();

            Summons.DespawnAll();
            RemoveBlazeOfGloryFromPlayers();

            if (instance)
            {
                instance->SetData(TYPE_BALEROC,DONE);
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE,me);
            }

            PlayAndYell(onDeath.sound,onDeath.text);
        }

        void EnterCombat(Unit * /*who*/)
        {
            me->SetFloatValue(UNIT_FIELD_COMBATREACH,20.0f);
            me->SetInCombatWithZone();
            RemoveBlazeOfGloryFromPlayers(); // For sure

            if(instance)
            {
                 instance->SetData(TYPE_BALEROC,IN_PROGRESS);
                 instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me);
            }

            PlayAndYell(onAggro.sound,onAggro.text);
        }

        void EnterEvadeMode()
        {
            me->SetFloatValue(UNIT_FIELD_COMBATREACH,5.0f);
            me->SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS,20.0f);
            Summons.DespawnAll();
            RemoveBlazeOfGloryFromPlayers();
            SetEquipmentSlots(false, NORMAL_BLADE_ENTRY, NORMAL_BLADE_ENTRY, EQUIP_NO_CHANGE);

            if(instance)
            {
                instance->SetData(TYPE_BALEROC,NOT_STARTED);
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
            }

            ScriptedAI::EnterEvadeMode();
        }

        void DoAction(const int32 action) // Called in aurascript
        {
            switch(action)
            {
                case DO_EQUIP_INFERNO_BLADE:
                    SetEquipmentSlots(false, INFERNO_BLADE_ENTRY,EQUIP_UNEQUIP, EQUIP_NO_CHANGE);
                    meleePhase = false;
                break;

                case DO_EQUIP_DECIMATION_BLADE:
                    SetEquipmentSlots(false, DECIMATION_BLADE_ENTRY,EQUIP_UNEQUIP, EQUIP_NO_CHANGE);
                    meleePhase = false;
                break;

                case DO_EQUIP_NORMAL_BLADE:
                    SetEquipmentSlots(false, NORMAL_BLADE_ENTRY, NORMAL_BLADE_ENTRY, EQUIP_NO_CHANGE);
                    meleePhase = true;
                break;

                default:
                    break;
            }
        }

        void PlayAndYell(uint32 soundId, const char * text)
        {
            DoPlaySoundToSet(me,soundId);
            me->MonsterYell(text, LANG_UNIVERSAL, 0);
        }

        void RemoveBlazeOfGloryFromPlayers(void)
        {
            if (!instance)
                return;

            Map::PlayerList const& plList = instance->instance->GetPlayers();

            if (plList.isEmpty())
                return;

            for(Map::PlayerList::const_iterator itr = plList.begin(); itr != plList.end(); ++itr)
            {
                if ( Player * p = itr->getSource())
                    p->RemoveAurasDueToSpell(BLAZE_OF_GLORY);
            }
        }

        void SpawnShardOfTorment(bool _10man)
        {
            if(_10man)
            {
                if (Unit * player = SelectTarget(SELECT_TARGET_RANDOM, 2, 100.0f, true) ) // Not on tanks if possible
                    me->CastSpell(player,SUMMON_SHARD,true);
                else if (Unit * player = SelectTarget(SELECT_TARGET_RANDOM, 0, 100.0f, true) )
                        me->CastSpell(player,SUMMON_SHARD,true);
                return;
            }

            //25 man -> spawn 2 shards

            if (!instance)
                return;

            Map::PlayerList const& plList = instance->instance->GetPlayers();

            if (plList.isEmpty())
                return;

            std::list<Player*> shard_list;

            for(Map::PlayerList::const_iterator itr = plList.begin(); itr != plList.end(); ++itr)
            {
                if (Player * p = itr->getSource())
                {
                    if (!p->HasTankSpec() && !p->HasAura(5487)) // Bear form
                        shard_list.push_back(p);
                }
            }

            if(!shard_list.empty() && shard_list.size() >= 2 )
            {
                std::list<Player*>::iterator j = shard_list.begin();
                advance(j, rand()%shard_list.size());

                if (*j && (*j)->IsInWorld() == true )
                    me->CastSpell((*j),SUMMON_SHARD,true);

                shard_list.erase(j);
                j = shard_list.begin();

                advance(j, rand()%shard_list.size());
                if (*j && (*j)->IsInWorld() == true )
                    me->CastSpell((*j),SUMMON_SHARD,true);
            }
            else // If only tanks are alive, select random
            {
                if (Unit * player = SelectTarget(SELECT_TARGET_RANDOM, 0, 100.0f, true) )
                    me->CastSpell(player,SUMMON_SHARD,true);
                if (Unit * player = SelectTarget(SELECT_TARGET_RANDOM, 0, 100.0f, true) )
                        me->CastSpell(player,SUMMON_SHARD,true);
            }
        }

       void UpdateAI(const uint32 diff)
       {
            if (!UpdateVictim())
                return;

            if (!me->IsWithinLOS(me->getVictim()->GetPositionX(),me->getVictim()->GetPositionY(),me->getVictim()->GetPositionZ()))
                me->Kill(me->getVictim(),true);

            if (blazeOfGloryTimer <= diff)
            {
                if(me->getVictim()->ToPlayer())
                    me->CastSpell(me->getVictim(),BLAZE_OF_GLORY,true);
                me->CastSpell(me, INCENDIARY_SOUL, true);
                blazeOfGloryTimer = urand(8000,13000);
            }
            else blazeOfGloryTimer -= diff;

            if (berserkTimer <= diff)
            {
                PlayAndYell(onBerserk.sound,onBerserk.text);
                me->CastSpell(me,BERSERK,true);
                berserkTimer = NEVER;
            }
            else berserkTimer -= diff;

/*****************************************************************************************/

            if (me->hasUnitState(UNIT_STAT_CASTING))
                return;

            if (castShardTimer <= diff)
            {
                PlayAndYell(onShard.sound,onShard.text);
                me->CastSpell(me,SUMMON_SHARD_DUMMY,false); // 1.5 s cast time ( just dummy )
                castShardTimer = urand(31000,33000);
                summonShardTimer = 1500;
                return;
            }
            else castShardTimer -= diff;

            if (summonShardTimer <= diff)
            {
                if(instance)
                {
                    if(getDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL || getDifficulty() == RAID_DIFFICULTY_25MAN_HEROIC)
                        SpawnShardOfTorment(false);
                    else
                        SpawnShardOfTorment(true);
                }

                summonShardTimer = NEVER;
                return;
            }
            else summonShardTimer -= diff;

            if (infernoBladeTimer <= diff)
            {
                PlayAndYell(onInfernoBlade.sound,onInfernoBlade.text);
                SetEquipmentSlots(false, INFERNO_BLADE_ENTRY,EQUIP_UNEQUIP, EQUIP_NO_CHANGE);
                me->CastSpell(me,INFERNO_BLADE,false);
                decimationBladeTimer = 45000;
                infernoBladeTimer = NEVER;
                return;
            }
            else infernoBladeTimer -= diff;

            if (decimationBladeTimer <= diff)
            {
                PlayAndYell(onDecimationBlade.sound,onDecimationBlade.text);
                SetEquipmentSlots(false, DECIMATION_BLADE_ENTRY,EQUIP_UNEQUIP, EQUIP_NO_CHANGE);
                me->CastSpell(me,DECIMATION_BLADE,false);
                infernoBladeTimer = 45000;
                decimationBladeTimer = NEVER;
                return;
            }
            else decimationBladeTimer -= diff;

/**********************************AUTO ATTACK STUFF***********************************************/

            if(meleePhase) // Melee attack phase
                DoMeleeAttackIfReady();
            else // Spell attack phase
            {
                if (me->hasUnitState(UNIT_STAT_CASTING))
                    return;

                if (me->isAttackReady())
                {
                    if(me->HasAura(99352) || me->HasAura(99352) ) // Decimation Blade
                    {
                        if (me->IsWithinCombatRange(me->getVictim(), GetSpellMaxRange(99353, false)))
                        {
                            bool avoided = false; // Decimation blade can be only dodged or parried

                            if (roll_chance_f(me->getVictim()->GetUnitDodgeChance()))
                                avoided = true;

                            if (roll_chance_f(me->getVictim()->GetUnitParryChance()))
                                avoided = true;

                            int32 bp0 = me->getVictim()->CountPctFromMaxHealth(90);

                            if (me->getVictim()->CountPctFromMaxHealth(90) < 250000 )
                                bp0 = 250000;

                            if(avoided)
                                bp0 = 0;

                            me->CastCustomSpell(me->getVictim(),99353,&bp0,0,0,true); // Decimation Strike
                            me->resetAttackTimer();
                        }
                        return;
                    }

                    if (me->HasAura(99350)) // Inferno Blade
                    {
                        if (me->IsWithinCombatRange(me->getVictim(), GetSpellMaxRange(99351, false)))
                        {
                            bool avoided = false; // Inferno blade can be dodged,parried or blocked

                            if (roll_chance_f(me->getVictim()->GetUnitDodgeChance()))
                                avoided = true;

                            if (roll_chance_f(me->getVictim()->GetUnitParryChance()))
                                avoided = true;

                            if (roll_chance_f(me->getVictim()->GetUnitBlockChance()))
                                avoided = true;

                            if(avoided)
                                me->CastCustomSpell(me->getVictim(),99353,0,0,0,true);
                            else
                                me->CastSpell(me->getVictim(),99351,true); // Inferno strike

                            me->resetAttackTimer();
                        }
                    }
                }
            }

       }
    };
};


enum shardSpells
{
    TORMENT_BEAM                    = 99255,
    SHARD_VISUAL                    = 99254,
    WAVE_OF_TORMENT                 = 99261,
    TORMENTED_DEBUFF                = 99257,
    TORMENT_VISUAL_BEAM             = 99258
};

enum shardActions
{
    SOMEONE_IN_RANGE    = 0,
    NO_ONE_IN_RANGE     = 1,
};

// SHARD OF TORMENT AI
class npc_shard_of_torment: public CreatureScript
{
public:
    npc_shard_of_torment(): CreatureScript("npc_shard_of_torment") {}

    struct npc_shard_of_tormentAI: public ScriptedAI
    {
        npc_shard_of_tormentAI(Creature* c): ScriptedAI(c)
        {
            Reset();
            me->SetFlag(UNIT_FIELD_FLAGS,UNIT_FLAG_NOT_SELECTABLE|UNIT_FLAG_DISABLE_MOVE|UNIT_FLAG_NON_ATTACKABLE);
            pInstance = me->GetInstanceScript();
        }

        bool canWave;
        uint32 waveTimer;
        uint32 beamTimer;
        uint32 checkTimer;
        InstanceScript * pInstance;


        void Reset()
        {
            canWave = false;
            waveTimer = 8000;
            checkTimer = 7000;
            beamTimer = 4000;
            me->CastSpell(me,TORMENT_VISUAL_BEAM,true);
            me->SetInCombatWithZone();
        }

        void DoAction(const int32 action)
        {
            if (action == NO_ONE_IN_RANGE)
            {
                canWave = true;
            }
            else if ( action == SOMEONE_IN_RANGE)
            {
                waveTimer = 1000;
                canWave = false;
            }
        }

        Player* SelectClosestPlayer()
        {
            if (!pInstance)
                return NULL;

            Map::PlayerList const& plList = pInstance->instance->GetPlayers();

            if (plList.isEmpty())
                return NULL;

            float min_range = FLT_MAX;
            Player* minrangeplayer = NULL;
            for(Map::PlayerList::const_iterator itr = plList.begin(); itr != plList.end(); ++itr)
            {
                if (me->GetDistance2d(itr->getSource()) < min_range)
                {
                    min_range = me->GetExactDist2d(itr->getSource());
                    minrangeplayer = itr->getSource();
                }
            }

            return minrangeplayer;
        }

        bool SomeoneInRange(void)
        {
            if (!pInstance)
                return NULL;

            Map::PlayerList const& plList = pInstance->instance->GetPlayers();

            if (plList.isEmpty())
                return NULL;

            for(Map::PlayerList::const_iterator itr = plList.begin(); itr != plList.end(); ++itr)
            {
                if (me->GetDistance2d(itr->getSource()) <= 15.0f) // 15 yards
                    return true;
            }

            return false;
        }

        void UpdateAI(const uint32 diff)
        {
            if (checkTimer <= diff) // Check if someone is in 15 yards range from Crystal
            {
                if (SomeoneInRange())
                    DoAction(SOMEONE_IN_RANGE);
                else
                    DoAction(NO_ONE_IN_RANGE);

                checkTimer = 1500;
            }
            else checkTimer -= diff;

            if (waveTimer <= diff)
            {
                if(canWave)
                    me->CastSpell(me,WAVE_OF_TORMENT,true);

                waveTimer = 1000;
            }
            else waveTimer -= diff;

            if (beamTimer <= diff)
            {
                if (me->HasAura(TORMENT_VISUAL_BEAM))
                {
                    me->RemoveAurasDueToSpell(TORMENT_VISUAL_BEAM);
                    me->CastSpell(me,SHARD_VISUAL,true);
                }

                Player * shardTaregt = SelectClosestPlayer();

                if (shardTaregt && !shardTaregt->HasAura(TORMENT_BEAM))
                    me->CastSpell(shardTaregt,TORMENT_BEAM,false);

                beamTimer = 1000;
            }
            else beamTimer -= diff;

        }
    };

    CreatureAI* GetAI(Creature* c) const
    {
        return new npc_shard_of_tormentAI(c);
    }
};

class spell_gen_tormented : public SpellScriptLoader
{
public:
    spell_gen_tormented() : SpellScriptLoader("spell_gen_tormented") { }

    class spell_gen_tormented_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_gen_tormented_AuraScript);

        void OnExpire(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
        {
            Unit * target = aurEff->GetBase()->GetUnitOwner();

            if(!target)
                return;

            target->CastSpell(target,TORMENTED_DEBUFF,true); // If torment fades from player, cast tormented debuff on him

        }

        void Register()
        {
            OnEffectRemove += AuraEffectRemoveFn(spell_gen_tormented_AuraScript::OnExpire, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript *GetAuraScript() const
    {
        return new spell_gen_tormented_AuraScript();
    }
};

class spell_gen_baleroc_blades : public SpellScriptLoader
{
public:
    spell_gen_baleroc_blades() : SpellScriptLoader("spell_gen_baleroc_blades") { }

    class spell_gen_baleroc_blades_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_gen_baleroc_blades_AuraScript);

        void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
        {
            Unit * caster= aurEff->GetCaster();

            if(!caster || !caster->ToCreature())
                return;

            if (GetSpellProto()->Id == 99350)
                caster->ToCreature()->AI()->DoAction(DO_EQUIP_INFERNO_BLADE);
            else
                caster->ToCreature()->AI()->DoAction(DO_EQUIP_DECIMATION_BLADE);
        }

        void OnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
        {
            Unit * caster= aurEff->GetCaster();

            if(!caster || !caster->ToCreature())
                return;

            caster->ToCreature()->AI()->DoAction(DO_EQUIP_NORMAL_BLADE);
        }

        void Register()
        {
            OnEffectApply += AuraEffectApplyFn(spell_gen_baleroc_blades_AuraScript::OnApply, EFFECT_FIRST_FOUND, SPELL_AURA_361, AURA_EFFECT_HANDLE_REAL);
            OnEffectRemove += AuraEffectRemoveFn(spell_gen_baleroc_blades_AuraScript::OnRemove, EFFECT_FIRST_FOUND, SPELL_AURA_361, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript *GetAuraScript() const
    {
        return new spell_gen_baleroc_blades_AuraScript();
    }
};

class spell_gen_torment : public SpellScriptLoader
{
    public:
        spell_gen_torment() : SpellScriptLoader("spell_gen_torment") { }

        class spell_gen_torment_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_gen_torment_SpellScript);

            bool stored;

            bool Load()
            {
                stored = false;
                return true;
            }

            uint32 GetSpellId(Unit * caster)
            {
                InstanceScript * pInstance= caster->GetInstanceScript();

                if (pInstance)
                {
                    switch (pInstance->instance->GetDifficulty())
                    {
                        case RAID_DIFFICULTY_10MAN_NORMAL:
                            return 99256;
                        case RAID_DIFFICULTY_25MAN_NORMAL:
                            return 100230;
                        case RAID_DIFFICULTY_10MAN_HEROIC:
                            return 100231;
                        case RAID_DIFFICULTY_25MAN_HEROIC:
                            return 100232;

                        default:
                            return 0;
                    }
                }
                return 0;
            }

            void HandleDamage(SpellEffIndex /*effIndex*/)
            {
                Unit * caster  = GetCaster();
                Unit * target  = GetHitUnit();

                if (!caster || !target)
                    return;

                uint32 spellId = GetSpellId(caster);

                if(spellId)
                {
                    uint32 stacks = target->GetAuraCount(spellId);
                    if(stacks)
                        SetHitDamage(GetHitDamage() * stacks);
                }
            }

            void Register()
            {
                OnEffect += SpellEffectFn(spell_gen_torment_SpellScript::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_gen_torment_SpellScript();
        }
};

class spell_gen_vital_flame : public SpellScriptLoader
{
public:
    spell_gen_vital_flame() : SpellScriptLoader("spell_gen_vital_flame") { }

    class spell_gen_vital_flame_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_gen_vital_flame_AuraScript);

        void OnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
        {
            if(GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_STACK)
                return;

            Unit * caster= aurEff->GetCaster();

            if(!caster)
                return;

            if(aurEff->GetAmount() > 0)
            {
                uint32 returnedSparks = uint32(aurEff->GetAmount() / 5);

                for(uint32 i = 0; i < returnedSparks; i++)
                    caster->CastSpell(caster,99262,true); // Vital Spark
            }
        }

        void Register()
        {
            OnEffectRemove += AuraEffectRemoveFn(spell_gen_vital_flame_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_359, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript *GetAuraScript() const
    {
        return new spell_gen_vital_flame_AuraScript();
    }
};

class go_firelands_bridge_opener : public GameObjectScript
{
public:
    go_firelands_bridge_opener() : GameObjectScript("go_firelands_bridge_opener") { }

    bool OnGossipHello(Player * pPlayer, GameObject* pGo)
    {
        InstanceScript *pInstance = pGo->GetInstanceScript();

        if (pInstance)
        {
            if(GameObject * go_bridge = pPlayer->FindNearestGameObject(5010734,200.0f))
            {
                if(go_bridge->HasFlag(GAMEOBJECT_FLAGS,GO_FLAG_DESTROYED))
                    go_bridge->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_DESTROYED);

                if(go_bridge->HasFlag(GAMEOBJECT_FLAGS,GO_FLAG_DAMAGED))
                    go_bridge->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_DAMAGED);

                    go_bridge->SetGoAnimProgress(0);

                pPlayer->SendCinematicStart (197);
            }
            return true;
        }
        return false;
    }

};


void AddSC_boss_baeloroc()
{
    new boss_baleroc();
    new npc_shard_of_torment();
    new go_firelands_bridge_opener();

    new spell_gen_tormented(); // 99255
    new spell_gen_baleroc_blades(); // 99352,99405,99350
    new spell_gen_torment(); // 99256,100230,100231,100232
    new spell_gen_vital_flame(); // 99263
}

/*
    UPDATE `creature_equip_template` SET `equipentry1`=0, `equipentry2`=0 WHERE  `entry`=53494 LIMIT 1;

    UPDATE `creature_template` SET `ScriptName`='boss_baleroc' WHERE  `entry`=53494 LIMIT 1;
    UPDATE `creature_template` SET `ScriptName`='boss_baleroc' WHERE  `entry`=53587 LIMIT 1;

    UPDATE `creature_template` SET `minlevel`=88, `maxlevel`=88, `exp`=3, `faction_A`=14, `faction_H`=14, `ScriptName`='npc_shard_of_torment' WHERE  `entry`=53495 LIMIT 1;
    UPDATE `creature_template` SET `modelid1`=11686, `modelid2`=11686, `modelid3`=11686 WHERE  `entry`=53495 LIMIT 1;
    UPDATE `creature_template` SET `scale`=1 WHERE  `entry`=53495 LIMIT 1;

    UPDATE `creature_template` SET `mindmg`=80000, `maxdmg`=90000 WHERE  `entry`=53494 LIMIT 1;
    UPDATE `creature_template` SET `mindmg`=80000, `maxdmg`=90000 WHERE  `entry`=53587 LIMIT 1;
    UPDATE `creature_template` SET `Health_mod`=311.75 WHERE  `entry`=53494 LIMIT 1;

    delete from spell_script_names where spell_id in (99255,99352,99405,99350);

     INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
     VALUES (99255, 'spell_gen_tormented'),
      (99352, 'spell_gen_baleroc_blades'),
      (99405, 'spell_gen_baleroc_blades'),
      (99350, 'spell_gen_baleroc_blades');

    delete from spell_script_names where spell_id in (99256,100230,100231,100232);

     INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
     VALUES (99256, 'spell_gen_torment'),
      (100230, 'spell_gen_torment'),
      (100231, 'spell_gen_torment'),
      (100232, 'spell_gen_torment');

    delete from spell_script_names where spell_id = 99263;

    INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
     VALUES (99263, 'spell_gen_vital_flame');

     UPDATE `gameobject_template` SET `ScriptName`='go_firelands_bridge_opener' WHERE  `entry`=209277 LIMIT 1;
*/
