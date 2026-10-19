/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

struct SkillTierDefaults
{
	float flEasy;
	float flMedium;
	float flHard;
};

class SkillManager
{
  public:
	// Dynamic on-demand query for any difficulty-scaled parameter (e.g. "sk_agrunt_health").
	// Evaluates the active skill level (1, 2, or 3), queries the engine cvar ("<base><level>"),
	// falls back to base name, registered defaults, and flDefault. Caches resolved results.
	static float Query( const char *pszBaseCvarName, float flDefault = 0.0f );

	// Alias for Query()
	static float GetValue( const char *pszBaseCvarName, float flDefault = 0.0f )
	{
		return Query( pszBaseCvarName, flDefault );
	}

	// Register default tier values [easy, medium, hard] for custom parameters
	static void RegisterDefault( const char *pszBaseCvarName, float flEasy, float flMedium, float flHard );
	static void RegisterBuiltinDefaults( void );

	// Direct runtime override for a base parameter name
	static void SetOverride( const char *pszBaseCvarName, float flValue );

	// Clear all runtime overrides
	static void ClearOverrides( void );

	// Invalidate query cache when skill level or map changes
	static void InvalidateCache( void );

	// Returns number of cached parameters
	static size_t GetCacheSize( void );

	// Active difficulty level (clamped 1..3)
	static int GetSkillLevel( void );
	static void SetSkillLevel( int iLevel );

	// Reset all cache, overrides, and registered defaults (for test isolation)
	static void Reset( void );

  private:
	static std::unordered_map<std::string, float> &GetCacheStorage( void );
	static std::unordered_map<std::string, float> &GetOverridesStorage( void );
	static std::unordered_map<std::string, SkillTierDefaults> &GetDefaultsStorage( void );
};

#define REGISTER_SKILL_DEFAULT( baseCvar, easy, medium, hard )                 \
	static struct SkillParamAutoReg_##baseCvar                                 \
	{                                                                          \
		SkillParamAutoReg_##baseCvar()                                         \
		{                                                                      \
			SkillManager::RegisterDefault( #baseCvar, easy, medium, hard );    \
		}                                                                      \
	} s_autoRegSkill_##baseCvar;
