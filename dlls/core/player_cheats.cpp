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
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "world/trains.h"
#include "ai/basemonster.h"
#include "gameplay/gamerules.h"
#include "core/client_customization.h"

#define TRAIN_ACTIVE 0x80
#define TRAIN_NEW 0xc0
#define TRAIN_OFF 0x00
#define TRAIN_NEUTRAL 0x01
#define TRAIN_SLOW 0x02
#define TRAIN_MEDIUM 0x03
#define TRAIN_FAST 0x04
#define TRAIN_BACK 0x05

int TrainSpeed( int iSpeed, int iMax )
{
	float fSpeed, fMax;
	int iRet = 0;

	fMax   = (float)iMax;
	fSpeed = iSpeed;

	fSpeed = fSpeed / fMax;

	if ( iSpeed < 0 )
		iRet = TRAIN_BACK;
	else if ( iSpeed == 0 )
		iRet = TRAIN_NEUTRAL;
	else if ( fSpeed < 0.33 )
		iRet = TRAIN_SLOW;
	else if ( fSpeed < 0.66 )
		iRet = TRAIN_MEDIUM;
	else
		iRet = TRAIN_FAST;

	return iRet;
}

//=========================================================
BOOL CBasePlayer ::FBecomeProne( void )
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return TRUE;
}

//=========================================================
// BarnacleVictimBitten - bad name for a function that is called
// by Barnacle victims when the barnacle pulls their head
// into its mouth. For the player, just die.
//=========================================================
void CBasePlayer ::BarnacleVictimBitten( entvars_t *pevBarnacle )
{
	TakeDamage( pevBarnacle, pevBarnacle, pev->health + pev->armorvalue, DMG_SLASH | DMG_ALWAYSGIB );
}

//=========================================================
// BarnacleVictimReleased - overridden for player who has
// physics flags concerns.
//=========================================================
void CBasePlayer ::BarnacleVictimReleased( void )
{
	m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
}

//=========================================================
// Illumination
// return player light level plus virtual muzzle flash
//=========================================================
int CBasePlayer ::Illumination( void )
{
	int iIllum = CBaseEntity::Illumination();

	iIllum += m_iWeaponFlash;
	if ( iIllum > 255 )
		return 255;
	return iIllum;
}

void CBasePlayer ::EnableControl( BOOL fControl )
{
	if ( !fControl )
		pev->flags |= FL_FROZEN;
	else
		pev->flags &= ~FL_FROZEN;
}

#define DOT_1DEGREE 0.9998476951564
#define DOT_2DEGREE 0.9993908270191
#define DOT_3DEGREE 0.9986295347546
#define DOT_4DEGREE 0.9975640502598
#define DOT_5DEGREE 0.9961946980917
#define DOT_6DEGREE 0.9945218953683
#define DOT_7DEGREE 0.9925461516413
#define DOT_8DEGREE 0.9902680687416
#define DOT_9DEGREE 0.9876883405951
#define DOT_10DEGREE 0.9848077530122
#define DOT_15DEGREE 0.9659258262891
#define DOT_20DEGREE 0.9396926207859
#define DOT_25DEGREE 0.9063077870367

//=========================================================
// Autoaim
// set crosshair position to point to enemey
