/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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
//=========================================================
// skill.cpp - code for skill level concerns
//=========================================================
#include "core/extdll.h"
#include "core/util.h"
#include "core/skill.h"

skilldata_t gSkillData;

//=========================================================
// take the name of a cvar, tack a digit for the skill level
// on, and return the value.of that Cvar
//=========================================================
float GetSkillCvar( char *pName )
{
	float flValue = SkillManager::Query( pName, 0.0f );

	if ( flValue <= 0 )
	{
		char szBuffer[64];
		snprintf( szBuffer, sizeof( szBuffer ), "%s%d", pName, gSkillData.iSkillLevel );
		ALERT( at_console, "\n\n** GetSkillCVar Got a zero for %s **\n\n", szBuffer );
	}

	return flValue;
}
