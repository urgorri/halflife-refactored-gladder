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
};

#endif // GLADDER_MONSTER_MODIFIERS_H
