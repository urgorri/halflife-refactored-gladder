/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Modernized Monster Turning Speed (yawspeed) & Champion Agility Modifier
 *	(SPEC §3, GitHub Issue #26)
 *
 ****/

#pragma once

#ifndef GLADDER_MONSTER_MODIFIERS_H
#define GLADDER_MONSTER_MODIFIERS_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/basemonster.h"
#include <string>

struct Schedule_t;

class GladderMonsterModifiers
{
  public:
	// Base arcade turn speed multiplier for ground monsters
	static constexpr float DEFAULT_ARCADE_YAW_MULTIPLIER = 2.5f;

	// Additional multiplier applied to Elite Champions
	static constexpr float CHAMPION_YAW_MULTIPLIER = 1.4f;

	// Calculate modern arcade yaw speed for a monster
	static float GetModernYawSpeed( CBaseMonster *pMonster, float flDefaultYawSpeed );

	// Specific target yawspeed query per monster species classname
	static float GetSpeciesYawSpeed( const std::string &szClassname, float flDefaultYawSpeed );

	// Check whether a monster is an airborne or stationary entity exempt from 2D yaw scaling
	static bool IsExemptSpecies( const std::string &szClassname );

	// Augment monster schedule interrupt masks for active movement and chase schedules (SPEC §3, Issue #64)
	static int GetScheduleInterruptMask( CBaseMonster *pMonster, Schedule_t *pSchedule, int iDefaultMask );

	// Determine if a schedule or monster represents an active movement/chase state
	static bool IsMovementSchedule( Schedule_t *pSchedule, CBaseMonster *pMonster = nullptr );

	// Difficulty-scaled Houndeye sonic blast cast speed (SPEC §3, Issue #121)
	static float GetHoundeyeCastSpeed( int iSkillLevel );

	// Difficulty-scaled Bullsquid spit spread (SPEC §3, Issue #116, #121)
	static float GetBullsquidSpitSpread( int iSkillLevel );

	// Difficulty-scaled Bullsquid toxic cloud duration (SPEC §3, Issue #116, #121)
	static float GetBullsquidCloudDuration( int iSkillLevel );
	static float GetToxicCloudDuration( int iSkillLevel );

	// Difficulty-scaled Headcrab & Baby Headcrab locomotion speed, leap velocity, and attack range (SPEC §3, Issue #121)
	static float GetHeadcrabMoveSpeed( int iSkillLevel );
	static float GetHeadcrabJumpSpeed( int iSkillLevel );
	static float GetHeadcrabAttackRange( int iSkillLevel );

	// Difficulty-scaled framerate multiplier for walking/running zombies & monsters (SPEC §3, Issue #116, #121)
	static float GetZombieMoveSpeed( int iSkillLevel );
	static float GetDifficultyFramerateMultiplier( int iSkillLevel );

	// Difficulty-scaled Vortigaunt / Alien Slave zap powerup cast speed (SPEC §3, Issue #121)
	static float GetVortigauntCastSpeed( int iSkillLevel );

	// Difficulty-scaled Alien Grunt hornet velocity and Skill 3 tracking agility (SPEC §3, Issue #121)
	static float GetAGruntHornetSpeed( int iSkillLevel );
	static float GetHornetTrackAgility( int iSkillLevel, float flDefaultWeight = 0.5f );

	// Difficulty-scaled HECU Grunt aim reaction wait time and grenade check delay (SPEC §3, Issue #121)
	static float GetHGruntWaitTime( int iSkillLevel, float flDefaultWait );
	static float GetHGruntGrenadeCheckDelay( int iSkillLevel, float flDefaultDelay );

	// Difficulty-scaled Alien Controller flight speed and energy ball burst count (SPEC §3, Issue #121)
	static float GetControllerMoveSpeed( int iSkillLevel );
	static int GetControllerBurstCount( int iSkillLevel, int iDefaultCount = 1 );

	// Difficulty-scaled heavy monster flinch recovery resistance (SPEC §3, Issue #116, #121)
	static float GetHeavyMonsterFlinchResistance( int iSkillLevel );
	static bool IsHeavyFlinchResistantMonster( const CBaseMonster *pMonster );

	// Synchronize gSkillData monster behavioral fields for the active skill tier (SPEC §3, Issue #121)
	static void ApplySkillDataOverrides( int iSkillLevel );

	// Pre-placed dead monster environmental prop helpers (SPEC §3, §11.1, Issue #123)
	static bool IsDeadMonsterPropClassname( const char *pszClassname );
	static bool IsDeadMonsterProp( const CBaseEntity *pEntity );
	static void ConfigureDeadMonsterProp( CBaseEntity *pEntity );
};

#endif // GLADDER_MONSTER_MODIFIERS_H
