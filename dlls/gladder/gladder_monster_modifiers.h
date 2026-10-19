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

	// Difficulty-scaled framerate multiplier for walking/running zombies & monsters (SPEC §3, Issue #116)
	static float GetDifficultyFramerateMultiplier( int iSkillLevel );

	// Difficulty-scaled Bullsquid spit spread (SPEC §3, Issue #116)
	static float GetBullsquidSpitSpread( int iSkillLevel );

	// Difficulty-scaled toxic cloud duration (SPEC §3, Issue #116)
	static float GetToxicCloudDuration( int iSkillLevel );

	// Difficulty-scaled heavy monster flinch recovery resistance (SPEC §3, Issue #116)
	static float GetHeavyMonsterFlinchResistance( int iSkillLevel );
};

#endif // GLADDER_MONSTER_MODIFIERS_H
