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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <string>

#include "core/extdll.h"
#include "core/util.h"
#include "core/skill.h"
#include "core/skill_manager.h"

std::unordered_map<std::string, float> &SkillManager::GetCacheStorage( void )
{
	static std::unordered_map<std::string, float> s_cache;
	return s_cache;
}

std::unordered_map<std::string, float> &SkillManager::GetOverridesStorage( void )
{
	static std::unordered_map<std::string, float> s_overrides;
	return s_overrides;
}

std::unordered_map<std::string, SkillTierDefaults> &SkillManager::GetDefaultsStorage( void )
{
	static std::unordered_map<std::string, SkillTierDefaults> s_defaults;
	return s_defaults;
}

int SkillManager::GetSkillLevel( void )
{
	if ( gSkillData.iSkillLevel >= SKILL_EASY && gSkillData.iSkillLevel <= SKILL_HARD )
	{
		return gSkillData.iSkillLevel;
	}

	if ( g_iSkillLevel >= SKILL_EASY && g_iSkillLevel <= SKILL_HARD )
	{
		return g_iSkillLevel;
	}

	return SKILL_EASY;
}

void SkillManager::SetSkillLevel( int iLevel )
{
	int clamped = iLevel;
	if ( clamped < SKILL_EASY )
	{
		clamped = SKILL_EASY;
	}
	else if ( clamped > SKILL_HARD )
	{
		clamped = SKILL_HARD;
	}

	gSkillData.iSkillLevel = clamped;
	g_iSkillLevel          = clamped;
	InvalidateCache();
}

float SkillManager::Query( const char *pszBaseCvarName, float flDefault )
{
	if ( !pszBaseCvarName || !*pszBaseCvarName )
	{
		return flDefault;
	}

	// 1. Check direct runtime overrides
	auto &overrides = GetOverridesStorage();
	auto itOverride = overrides.find( pszBaseCvarName );
	if ( itOverride != overrides.end() )
	{
		return itOverride->second;
	}

	// 2. Check query cache
	auto &cache  = GetCacheStorage();
	auto itCache = cache.find( pszBaseCvarName );
	if ( itCache != cache.end() )
	{
		return itCache->second;
	}

	float flValue = 0.0f;
	bool bFound   = false;
	int iSkill    = GetSkillLevel();

	char szBuffer[64];
	snprintf( szBuffer, sizeof( szBuffer ), "%s%d", pszBaseCvarName, iSkill );

	// 3. Query engine CVAR with skill level digit suffix (e.g. sk_plr_crowbar1)
	if ( g_engfuncs.pfnCVarGetPointer )
	{
		cvar_t *pCvar = CVAR_GET_POINTER( szBuffer );
		if ( pCvar )
		{
			flValue = pCvar->value;
			bFound  = true;
		}
		else
		{
			// Try base name without digit suffix
			pCvar = CVAR_GET_POINTER( pszBaseCvarName );
			if ( pCvar )
			{
				flValue = pCvar->value;
				bFound  = true;
			}
		}
	}
	else if ( g_engfuncs.pfnCVarGetFloat )
	{
		flValue = CVAR_GET_FLOAT( szBuffer );
		if ( flValue != 0.0f )
		{
			bFound = true;
		}
		else
		{
			flValue = CVAR_GET_FLOAT( pszBaseCvarName );
			if ( flValue != 0.0f )
			{
				bFound = true;
			}
		}
	}

	// 4. Fallback to registered tier defaults if engine cvar is absent
	if ( !bFound )
	{
		auto &defaults = GetDefaultsStorage();
		auto itDefault = defaults.find( pszBaseCvarName );
		if ( itDefault != defaults.end() )
		{
			switch ( iSkill )
			{
			case SKILL_EASY:
				flValue = itDefault->second.flEasy;
				break;
			case SKILL_MEDIUM:
				flValue = itDefault->second.flMedium;
				break;
			case SKILL_HARD:
				flValue = itDefault->second.flHard;
				break;
			default:
				flValue = itDefault->second.flEasy;
				break;
			}
			bFound = true;
		}
	}

	// 5. Final fallback to flDefault
	if ( !bFound )
	{
		flValue = flDefault;
	}

	cache[pszBaseCvarName] = flValue;
	return flValue;
}

void SkillManager::RegisterDefault( const char *pszBaseCvarName, float flEasy, float flMedium, float flHard )
{
	if ( !pszBaseCvarName || !*pszBaseCvarName )
		return;

	SkillTierDefaults defs;
	defs.flEasy   = flEasy;
	defs.flMedium = flMedium;
	defs.flHard   = flHard;

	GetDefaultsStorage()[pszBaseCvarName] = defs;
	InvalidateCache();
}

void SkillManager::RegisterBuiltinDefaults( void )
{
	RegisterDefault( "sk_houndeye_cast_speed", 1.0f, 1.0f, 1.0f );
	RegisterDefault( "sk_bullsquid_spit_spread", 0.05f, 0.05f, 0.05f );
	RegisterDefault( "sk_headcrab_jump_speed", 650.0f, 650.0f, 650.0f );
	RegisterDefault( "sk_headcrab_attack_range", 256.0f, 256.0f, 256.0f );
	RegisterDefault( "sk_headcrab_move_speed", 1.0f, 1.0f, 1.0f );
	RegisterDefault( "sk_zombie_move_speed", 1.0f, 1.0f, 1.0f );
	RegisterDefault( "sk_islave_cast_speed", 1.0f, 1.0f, 1.5f );
	RegisterDefault( "sk_agrunt_hornet_speed", 600.0f, 600.0f, 600.0f );
}

void SkillManager::SetOverride( const char *pszBaseCvarName, float flValue )
{
	if ( !pszBaseCvarName || !*pszBaseCvarName )
		return;

	GetOverridesStorage()[pszBaseCvarName] = flValue;
	InvalidateCache();
}

void SkillManager::ClearOverrides( void )
{
	GetOverridesStorage().clear();
	InvalidateCache();
}

void SkillManager::InvalidateCache( void )
{
	GetCacheStorage().clear();
}

size_t SkillManager::GetCacheSize( void )
{
	return GetCacheStorage().size();
}

void SkillManager::Reset( void )
{
	GetCacheStorage().clear();
	GetOverridesStorage().clear();
	GetDefaultsStorage().clear();
}
