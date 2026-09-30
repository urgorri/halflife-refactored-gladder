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
/*

===== util.cpp ========================================================

  Utility code.  Really not optional after all.

*/

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include <stdint.h>
#include "core/saverestore.h"
#include <time.h>
#include "shake.h"
#include "core/decals.h"
#include "core/player.h"
#include "weapons/weapon_base.h"
#include "gameplay/gamerules.h"
#include <cstring>

float UTIL_WeaponTimeBase( void )
{
#if defined( CLIENT_WEAPONS )
	return 0.0;
#else
	return gpGlobals->time;
#endif
}

void UTIL_ParametricRocket( entvars_t *pev, Vector vecOrigin, Vector vecAngles, edict_t *owner )
{
	pev->startpos = vecOrigin;
	// Trace out line to end pos
	TraceResult tr;
	UTIL_MakeVectors( vecAngles );
	UTIL_TraceLine( pev->startpos, pev->startpos + gpGlobals->v_forward * 8192, ignore_monsters, owner, &tr );
	pev->endpos = tr.vecEndPos;

	// Now compute how long it will take based on current velocity
	Vector vecTravel = pev->endpos - pev->startpos;
	float travelTime = 0.0;
	if ( pev->velocity.Length() > 0 )
	{
		travelTime = vecTravel.Length() / pev->velocity.Length();
	}
	pev->starttime  = gpGlobals->time;
	pev->impacttime = gpGlobals->time + travelTime;
}

BOOL UTIL_GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon )
{
	return g_pGameRules->GetNextBestWeapon( pPlayer, pCurrentWeapon );
}

//	float UTIL_MoveToOrigin( edict_t *pent, const Vector vecGoal, float flDist, int iMoveType )
void UTIL_MoveToOrigin( edict_t *pent, const Vector &vecGoal, float flDist, int iMoveType )
{
	float rgfl[3];
	vecGoal.CopyToArray( rgfl );
	//		return MOVE_TO_ORIGIN ( pent, rgfl, flDist, iMoveType );
	MOVE_TO_ORIGIN( pent, rgfl, flDist, iMoveType );
}

void UTIL_MakeVectors( const Vector &vecAngles )
{
	MAKE_VECTORS( vecAngles );
}

void UTIL_MakeAimVectors( const Vector &vecAngles )
{
	float rgflVec[3];
	vecAngles.CopyToArray( rgflVec );
	rgflVec[0] = -rgflVec[0];
	MAKE_VECTORS( rgflVec );
}

template < typename T >
inline void SwapValues( T &a, T &b )
{
	T swapVal = a;
	a         = b;
	b         = swapVal;
}

void UTIL_MakeInvVectors( const Vector &vec, globalvars_t *pgv )
{
	MAKE_VECTORS( vec );

	pgv->v_right = pgv->v_right * -1;

	SwapValues( pgv->v_forward.y, pgv->v_right.x );
	SwapValues( pgv->v_forward.z, pgv->v_up.x );
	SwapValues( pgv->v_right.z, pgv->v_up.y );
}

void UTIL_EmitAmbientSound( edict_t *entity, const Vector &vecOrigin, const char *samp, float vol, float attenuation, int fFlags, int pitch )
{
	float rgfl[3];
	vecOrigin.CopyToArray( rgfl );

	if ( samp && *samp == '!' )
	{
		char name[32];
		if ( SENTENCEG_Lookup( samp, name ) >= 0 )
			EMIT_AMBIENT_SOUND( entity, rgfl, name, vol, attenuation, fFlags, pitch );
	}
	else
		EMIT_AMBIENT_SOUND( entity, rgfl, samp, vol, attenuation, fFlags, pitch );
}

static unsigned short FixedUnsigned16( float value, float scale )
{
	int output;

	output = value * scale;
	if ( output < 0 )
		output = 0;
	if ( output > 0xFFFF )
		output = 0xFFFF;

	return (unsigned short)output;
}

static short FixedSigned16( float value, float scale )
{
	int output;

	output = value * scale;

	if ( output > 32767 )
		output = 32767;

	if ( output < -32768 )
		output = -32768;

	return (short)output;
}

// Shake the screen of all clients within radius
// radius == 0, shake all clients
// Screen shake falloff and client grounding evaluation
void UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius )
{
	int i;
	float localAmplitude;
	ScreenShake shake;

	shake.duration  = FixedUnsigned16( duration, 1 << 12 ); // 4.12 fixed
	shake.frequency = FixedUnsigned16( frequency, 1 << 8 ); // 8.8 fixed

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer || !( pPlayer->pev->flags & FL_ONGROUND ) ) // Don't shake if not onground
			continue;

		localAmplitude = 0;

		if ( radius <= 0 )
			localAmplitude = amplitude;
		else
		{
			Vector delta   = center - pPlayer->pev->origin;
			float distance = delta.Length();

			// Had to get rid of this falloff - it didn't work well
			if ( distance < radius )
				localAmplitude = amplitude; // radius - distance;
		}
		if ( localAmplitude )
		{
			shake.amplitude = FixedUnsigned16( localAmplitude, 1 << 12 ); // 4.12 fixed

			MESSAGE_BEGIN( MSG_ONE, gmsgShake, NULL, pPlayer->edict() ); // use the magic #1 for "one client"

			WRITE_SHORT( shake.amplitude ); // shake amount
			WRITE_SHORT( shake.duration );  // shake lasts this long
			WRITE_SHORT( shake.frequency ); // shake noise frequency

			MESSAGE_END();
		}
	}
}

void UTIL_ScreenShakeAll( const Vector &center, float amplitude, float frequency, float duration )
{
	UTIL_ScreenShake( center, amplitude, frequency, duration, 0 );
}

void UTIL_ScreenFadeBuild( ScreenFade &fade, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	fade.duration  = FixedUnsigned16( fadeTime, 1 << 12 ); // 4.12 fixed
	fade.holdTime  = FixedUnsigned16( fadeHold, 1 << 12 ); // 4.12 fixed
	fade.r         = (int)color.x;
	fade.g         = (int)color.y;
	fade.b         = (int)color.z;
	fade.a         = alpha;
	fade.fadeFlags = flags;
}

void UTIL_ScreenFadeWrite( const ScreenFade &fade, CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgFade, NULL, pEntity->edict() ); // use the magic #1 for "one client"

	WRITE_SHORT( fade.duration );  // fade lasts this long
	WRITE_SHORT( fade.holdTime );  // fade lasts this long
	WRITE_SHORT( fade.fadeFlags ); // fade type (in / out)
	WRITE_BYTE( fade.r );          // fade red
	WRITE_BYTE( fade.g );          // fade green
	WRITE_BYTE( fade.b );          // fade blue
	WRITE_BYTE( fade.a );          // fade blue

	MESSAGE_END();
}

void UTIL_ScreenFadeAll( const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	int i;
	ScreenFade fade;

	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, alpha, flags );

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		UTIL_ScreenFadeWrite( fade, pPlayer );
	}
}

void UTIL_ScreenFade( CBaseEntity *pEntity, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	ScreenFade fade;

	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, alpha, flags );
	UTIL_ScreenFadeWrite( fade, pEntity );
}

void UTIL_HudMessage( CBaseEntity *pEntity, const hudtextparms_t &textparms, const char *pMessage )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, pEntity->edict() );
	WRITE_BYTE( TE_TEXTMESSAGE );
	WRITE_BYTE( textparms.channel & 0xFF );

	WRITE_SHORT( FixedSigned16( textparms.x, 1 << 13 ) );
	WRITE_SHORT( FixedSigned16( textparms.y, 1 << 13 ) );
	WRITE_BYTE( textparms.effect );

	WRITE_BYTE( textparms.r1 );
	WRITE_BYTE( textparms.g1 );
	WRITE_BYTE( textparms.b1 );
	WRITE_BYTE( textparms.a1 );

	WRITE_BYTE( textparms.r2 );
	WRITE_BYTE( textparms.g2 );
	WRITE_BYTE( textparms.b2 );
	WRITE_BYTE( textparms.a2 );

	WRITE_SHORT( FixedUnsigned16( textparms.fadeinTime, 1 << 8 ) );
	WRITE_SHORT( FixedUnsigned16( textparms.fadeoutTime, 1 << 8 ) );
	WRITE_SHORT( FixedUnsigned16( textparms.holdTime, 1 << 8 ) );

	if ( textparms.effect == 2 )
		WRITE_SHORT( FixedUnsigned16( textparms.fxTime, 1 << 8 ) );

	if ( strlen( pMessage ) < 512 )
	{
		WRITE_STRING( pMessage );
	}
	else
	{
		char tmp[512];
		strncpy( tmp, pMessage, 511 );
		tmp[511] = 0;
		WRITE_STRING( tmp );
	}
	MESSAGE_END();
}

void UTIL_HudMessageAll( const hudtextparms_t &textparms, const char *pMessage )
{
	int i;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
			UTIL_HudMessage( pPlayer, textparms, pMessage );
	}
}

extern int gmsgTextMsg, gmsgSayText;
void UTIL_ClientPrintAll( int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgTextMsg );
	WRITE_BYTE( msg_dest );
	WRITE_STRING( msg_name );

	if ( param1 )
		WRITE_STRING( param1 );
	if ( param2 )
		WRITE_STRING( param2 );
	if ( param3 )
		WRITE_STRING( param3 );
	if ( param4 )
		WRITE_STRING( param4 );

	MESSAGE_END();
}

void ClientPrint( entvars_t *client, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgTextMsg, NULL, client );
	WRITE_BYTE( msg_dest );
	WRITE_STRING( msg_name );

	if ( param1 )
		WRITE_STRING( param1 );
	if ( param2 )
		WRITE_STRING( param2 );
	if ( param3 )
		WRITE_STRING( param3 );
	if ( param4 )
		WRITE_STRING( param4 );

	MESSAGE_END();
}

void UTIL_SayText( const char *pText, CBaseEntity *pEntity )
{
	if ( !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, pEntity->edict() );
	WRITE_BYTE( pEntity->entindex() );
	WRITE_STRING( pText );
	MESSAGE_END();
}

void UTIL_SayTextAll( const char *pText, CBaseEntity *pEntity )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
	WRITE_BYTE( pEntity->entindex() );
	WRITE_STRING( pText );
	MESSAGE_END();
}

char *UTIL_dtos1( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos2( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos3( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos4( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

void UTIL_ShowMessage( const char *pString, CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgHudText, NULL, pEntity->edict() );
	WRITE_STRING( pString );
	MESSAGE_END();
}

void UTIL_ShowMessageAll( const char *pString )
{
	int i;

	// loop through all players

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
			UTIL_ShowMessage( pString, pPlayer );
	}
}

void UTIL_SetSize( entvars_t *pev, const Vector &vecMin, const Vector &vecMax )
{
	SET_SIZE( ENT( pev ), vecMin, vecMax );
}

void UTIL_SetOrigin( entvars_t *pev, const Vector &vecOrigin )
{
	edict_t *ent = ENT( pev );
	if ( ent )
		SET_ORIGIN( ent, vecOrigin );
}

void UTIL_ParticleEffect( const Vector &vecOrigin, const Vector &vecDirection, ULONG ulColor, ULONG ulCount )
{
	PARTICLE_EFFECT( vecOrigin, vecDirection, (float)ulColor, (float)ulCount );
}

char *UTIL_VarArgs( char *format, ... )
{
	va_list argptr;
	static char string[1024];

	va_start( argptr, format );
	vsprintf( string, format, argptr );
	va_end( argptr );

	return string;
}

Vector UTIL_GetAimVector( edict_t *pent, float flSpeed )
{
	Vector tmp;
	GET_AIM_VECTOR( pent, flSpeed, tmp );
	return tmp;
}

int UTIL_IsMasterTriggered( string_t sMaster, CBaseEntity *pActivator )
{
	if ( sMaster )
	{
		edict_t *pentTarget = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( sMaster ) );

		if ( !FNullEnt( pentTarget ) )
		{
			CBaseEntity *pMaster = CBaseEntity::Instance( pentTarget );
			if ( pMaster && ( pMaster->ObjectCaps() & FCAP_MASTER ) )
				return pMaster->IsTriggered( pActivator );
		}

		ALERT( at_console, "Master was null or not a master!\n" );
	}

	// if this isn't a master entity, just say yes.
	return 1;
}

BOOL UTIL_TeamsMatch( const char *pTeamName1, const char *pTeamName2 )
{
	// Everyone matches unless it's teamplay
	if ( !g_pGameRules->IsTeamplay() )
		return TRUE;

	// Both on a team?
	if ( *pTeamName1 != 0 && *pTeamName2 != 0 )
	{
		if ( !stricmp( pTeamName1, pTeamName2 ) ) // Same Team?
			return TRUE;
	}

	return FALSE;
}

void UTIL_StringToVector( float *pVector, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int j;

	strncpy( tempString, pString, sizeof( tempString ) );
	tempString[sizeof( tempString ) - 1] = '\0';
	pstr = pfront = tempString;

	for ( j = 0; j < 3; j++ ) // lifted from pr_edict.c
	{
		pVector[j] = atof( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if ( !*pstr )
			break;
		pstr++;
		pfront = pstr;
	}
	if ( j < 2 )
	{
		/*
		ALERT( at_error, "Bad field in entity!! %s:%s == \"%s\"\n",
		    pkvd->szClassName, pkvd->szKeyName, pkvd->szValue );
		*/
		for ( j = j + 1; j < 3; j++ )
			pVector[j] = 0;
	}
}

void UTIL_StringToIntArray( int *pVector, int count, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int j;

	strncpy( tempString, pString, sizeof( tempString ) );
	tempString[sizeof( tempString ) - 1] = '\0';
	pstr = pfront = tempString;

	for ( j = 0; j < count; j++ ) // lifted from pr_edict.c
	{
		pVector[j] = atoi( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if ( !*pstr )
			break;
		pstr++;
		pfront = pstr;
	}

	for ( j++; j < count; j++ )
	{
		pVector[j] = 0;
	}
}

float UTIL_WaterLevel( const Vector &position, float minz, float maxz )
{
	Vector midUp = position;
	midUp.z      = minz;

	if ( UTIL_PointContents( midUp ) != CONTENTS_WATER )
		return minz;

	midUp.z = maxz;
	if ( UTIL_PointContents( midUp ) == CONTENTS_WATER )
		return maxz;

	float diff = maxz - minz;
	while ( diff > 1.0 )
	{
		midUp.z = minz + diff / 2.0;
		if ( UTIL_PointContents( midUp ) == CONTENTS_WATER )
		{
			minz = midUp.z;
		}
		else
		{
			maxz = midUp.z;
		}
		diff = maxz - minz;
	}

	return midUp.z;
}

extern DLL_GLOBAL short g_sModelIndexBubbles; // holds the index for the bubbles model

void UTIL_Bubbles( Vector mins, Vector maxs, int count )
{
	Vector mid = ( mins + maxs ) * 0.5;

	float flHeight = UTIL_WaterLevel( mid, mid.z, mid.z + 1024 );
	flHeight       = flHeight - mins.z;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, mid );
	WRITE_BYTE( TE_BUBBLES );
	WRITE_COORD( mins.x ); // mins
	WRITE_COORD( mins.y );
	WRITE_COORD( mins.z );
	WRITE_COORD( maxs.x ); // maxz
	WRITE_COORD( maxs.y );
	WRITE_COORD( maxs.z );
	WRITE_COORD( flHeight ); // height
	WRITE_SHORT( g_sModelIndexBubbles );
	WRITE_BYTE( count ); // count
	WRITE_COORD( 8 );    // speed
	MESSAGE_END();
}

void UTIL_BubbleTrail( Vector from, Vector to, int count )
{
	float flHeight = UTIL_WaterLevel( from, from.z, from.z + 256 );
	flHeight       = flHeight - from.z;

	if ( flHeight < 8 )
	{
		flHeight = UTIL_WaterLevel( to, to.z, to.z + 256 );
		flHeight = flHeight - to.z;
		if ( flHeight < 8 )
			return;

		// Ricochet audio playback
		flHeight = flHeight + to.z - from.z;
	}

	if ( count > 255 )
		count = 255;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
	WRITE_BYTE( TE_BUBBLETRAIL );
	WRITE_COORD( from.x ); // mins
	WRITE_COORD( from.y );
	WRITE_COORD( from.z );
	WRITE_COORD( to.x ); // maxz
	WRITE_COORD( to.y );
	WRITE_COORD( to.z );
	WRITE_COORD( flHeight ); // height
	WRITE_SHORT( g_sModelIndexBubbles );
	WRITE_BYTE( count ); // count
	WRITE_COORD( 8 );    // speed
	MESSAGE_END();
}

void UTIL_Remove( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	pEntity->UpdateOnRemove();
	pEntity->pev->flags |= FL_KILLME;
	pEntity->pev->targetname = 0;
}

BOOL UTIL_IsValidEntity( edict_t *pent )
{
	if ( !pent || pent->free || ( pent->v.flags & FL_KILLME ) )
		return FALSE;
	return TRUE;
}

void UTIL_PrecacheOther( const char *szClassname )
{
	edict_t *pent;

	pent = CREATE_NAMED_ENTITY( MAKE_STRING( szClassname ) );
	if ( FNullEnt( pent ) )
	{
		ALERT( at_console, "NULL Ent in UTIL_PrecacheOther\n" );
		return;
	}

	CBaseEntity *pEntity = CBaseEntity::Instance( VARS( pent ) );
	if ( pEntity )
		pEntity->Precache();
	REMOVE_ENTITY( pent );
}

//=========================================================
// UTIL_StripToken - for redundant keynames
//=========================================================
void UTIL_StripToken( const char *pKey, char *pDest, int nLen )
{
	int i = 0;

	while ( i < nLen - 1 && pKey[i] && pKey[i] != '#' )
	{
		pDest[i] = pKey[i];
		i++;
	}
	pDest[i] = 0;
}

// --------------------------------------------------------------
//
// CSave
//
// --------------------------------------------------------------

void UTIL_ForEachEntity( void ( *pfnCallback )( CBaseEntity *pEntity, void *pUserData ), void *pUserData )
{
	if ( !pfnCallback || !gpGlobals )
		return;

	for ( int i = 1; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pEdict = INDEXENT( i );
		if ( !pEdict || pEdict->free )
			continue;
		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
		if ( !pEntity )
			continue;
		pfnCallback( pEntity, pUserData );
	}
}


