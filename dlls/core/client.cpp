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
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/saverestore.h"
#include "core/player.h"
#include "gameplay/spectator.h"
#include "core/client.h"
#include "ai/soundent.h"
#include "gameplay/gamerules.h"
#include "core/game.h"
#include "customentity.h"
#include "weapons/weapon_base.h"
#include "weapons/weapon_rpg.h"
#include "weaponinfo.h"
#include "usercmd.h"
#include "netadr.h"
#include "pm_shared.h"

#if !defined( _WIN32 )
#include <ctype.h>
#endif

extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL g_fGameOver;
extern DLL_GLOBAL int g_iSkillLevel;
extern DLL_GLOBAL ULONG g_ulFrameCount;

extern void CopyToBodyQue( entvars_t *pev );
extern int giPrecacheGrunt;
extern int gmsgSayText;

extern cvar_t allow_spectators;

extern int g_teamplay;

void LinkUserMessages( void );

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
void set_suicide_frame( entvars_t *pev )
{
	if ( !FStrEq( STRING( pev->model ), "models/player.mdl" ) )
		return; // allready gibbed

	//	pev->frame		= $deatha11;
	pev->solid     = SOLID_NOT;
	pev->movetype  = MOVETYPE_TOSS;
	pev->deadflag  = DEAD_DEAD;
	pev->nextthink = -1;
}

/*
===========
ClientConnect

called when a player connects to a server
============
*/
BOOL ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128] )
{
	return g_pGameRules->ClientConnected( pEntity, pszName, pszAddress, szRejectReason );

	// a client connecting during an intermission can cause problems
	//	if (intermission_running)
	//		ExitIntermission ();
}

/*
===========
ClientDisconnect

called when a player disconnects from a server

GLOBALS ASSUMED SET:  g_fGameOver
============
*/
void ClientDisconnect( edict_t *pEntity )
{
	if ( g_fGameOver )
		return;

	char text[256] = "";
	if ( pEntity->v.netname )
		snprintf( text, sizeof( text ), "- %s has left the game\n", STRING( pEntity->v.netname ) );
	text[sizeof( text ) - 1] = 0;
	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
	WRITE_BYTE( ENTINDEX( pEntity ) );
	WRITE_STRING( text );
	MESSAGE_END();

	CSound *pSound;
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( pEntity ) );
	{
		// since this client isn't around to think anymore, reset their sound.
		if ( pSound )
		{
			pSound->Reset();
		}
	}

	// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->v.takedamage = DAMAGE_NO; // don't attract autoaim
	pEntity->v.solid      = SOLID_NOT; // nonsolid
	UTIL_SetOrigin( &pEntity->v, pEntity->v.origin );

	g_pGameRules->ClientDisconnected( pEntity );
}

// called by ClientKill and DeadThink
void respawn( entvars_t *pev, BOOL fCopyCorpse )
{
	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pev );
	if ( g_pGameRules && pPlayer )
	{
		g_pGameRules->PlayerRespawn( pPlayer, fCopyCorpse );
	}
	else if ( gpGlobals->coop || gpGlobals->deathmatch )
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			CopyToBodyQue( pev );
		}

		// respawn player
		if ( pPlayer )
		{
			pPlayer->Spawn();
		}
	}
	else
	{ // restart the entire server
		SERVER_COMMAND( "reload\n" );
	}
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
void ClientKill( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;

	CBasePlayer *pl = (CBasePlayer *)CBasePlayer::Instance( pev );

	if ( pl->m_fNextSuicideTime > gpGlobals->time )
		return; // prevent suiciding too ofter

	pl->m_fNextSuicideTime = gpGlobals->time + 1; // don't let them suicide for 5 seconds after suiciding

	// have the player kill themself
	pev->health = 0;
	pl->Killed( pev, GIB_NEVER );

	//	pev->modelindex = g_ulModelIndexPlayer;
	//	pev->frags -= 2;		// extra penalty
	//	respawn( pev );
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
void ClientPutInServer( edict_t *pEntity )
{
	CBasePlayer *pPlayer;

	entvars_t *pev = &pEntity->v;

	pPlayer = GetClassPtr( (CBasePlayer *)pev );
	pPlayer->SetCustomDecalFrames( -1 ); // Assume none;

	// Allocate a CBasePlayer for pev, and call spawn
	pPlayer->Spawn();

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;

	pPlayer->pev->iuser1 = 0; // disable any spec modes
	pPlayer->pev->iuser2 = 0;
}

/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
void ClientUserInfoChanged( edict_t *pEntity, char *infobuffer )
{
	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	if ( pEntity->v.netname && STRING( pEntity->v.netname )[0] != 0 && !FStrEq( STRING( pEntity->v.netname ), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) ) )
	{
		char sName[256];
		char *pName = g_engfuncs.pfnInfoKeyValue( infobuffer, "name" );
		strncpy( sName, pName, sizeof( sName ) - 1 );
		sName[sizeof( sName ) - 1] = '\0';

		// First parse the name and remove any %'s
		for ( char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
		{
			// Replace it with a space
			if ( *pApersand == '%' )
				*pApersand = ' ';
		}

		// Set the name
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pEntity ), infobuffer, "name", sName );

		if ( gpGlobals->maxClients > 1 )
		{
			char text[256];
			sprintf( text, "* %s changed name to %s\n", STRING( pEntity->v.netname ), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
			MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
			WRITE_BYTE( ENTINDEX( pEntity ) );
			WRITE_STRING( text );
			MESSAGE_END();
		}

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n",
			                STRING( pEntity->v.netname ),
			                GETPLAYERUSERID( pEntity ),
			                GETPLAYERAUTHID( pEntity ),
			                g_engfuncs.pfnInfoKeyValue( infobuffer, "model" ),
			                g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" changed name to \"%s\"\n",
			                STRING( pEntity->v.netname ),
			                GETPLAYERUSERID( pEntity ),
			                GETPLAYERAUTHID( pEntity ),
			                GETPLAYERUSERID( pEntity ),
			                g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
	}

	g_pGameRules->ClientUserInfoChanged( GetClassPtr( (CBasePlayer *)&pEntity->v ), infobuffer );
}

static int g_serveractive = 0;

void ServerDeactivate( void )
{
	// It's possible that the engine will call this function more times than is necessary
	//  Therefore, only run it one time for each call to ServerActivate
	if ( g_serveractive != 1 )
	{
		return;
	}

	g_serveractive = 0;

	// Peform any shutdown operations here...
	//
}

void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	int i;
	CBaseEntity *pClass;

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;

	// Clients have not been initialized yet
	for ( i = 0; i < edictCount; i++ )
	{
		if ( pEdictList[i].free )
			continue;

		// Clients aren't necessarily initialized until ClientPutInServer()
		if ( i < clientMax || !pEdictList[i].pvPrivateData )
			continue;

		pClass = CBaseEntity::Instance( &pEdictList[i] );
		// Activate this entity if it's got a class & isn't dormant
		if ( pClass && !( pClass->pev->flags & FL_DORMANT ) )
		{
			pClass->Activate();
		}
		else
		{
			ALERT( at_console, "Can't instance %s\n", STRING( pEdictList[i].v.classname ) );
		}
	}

	// Link user messages here to make sure first client can get them...
	LinkUserMessages();
}

/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink( edict_t *pEntity )
{
	entvars_t *pev       = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE( pEntity );

	if ( pPlayer )
		pPlayer->PreThink();
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink( edict_t *pEntity )
{
	entvars_t *pev       = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE( pEntity );

	if ( pPlayer )
		pPlayer->PostThink();
}

void ParmsNewLevel( void )
{
}

void ParmsChangeLevel( void )
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;

	if ( pSaveData )
		pSaveData->connectionCount = BuildChangeList( pSaveData->levelList, MAX_LEVEL_CONNECTIONS );
}

//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
void StartFrame( void )
{
	if ( g_pGameRules )
		g_pGameRules->Think();

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = teamplay.value;
	g_ulFrameCount++;
}

void ClientPrecache( void )
{
	// setup precaches always needed
	PRECACHE_SOUND( "player/sprayer.wav" ); // spray paint sound for PreAlpha

	// PRECACHE_SOUND("player/pl_jumpland2.wav");		// Landing step audio

	PRECACHE_SOUND( "player/pl_fallpain2.wav" );
	PRECACHE_SOUND( "player/pl_fallpain3.wav" );

	PRECACHE_SOUND( "player/pl_step1.wav" ); // walk on concrete
	PRECACHE_SOUND( "player/pl_step2.wav" );
	PRECACHE_SOUND( "player/pl_step3.wav" );
	PRECACHE_SOUND( "player/pl_step4.wav" );

	PRECACHE_SOUND( "common/npc_step1.wav" ); // NPC walk on concrete
	PRECACHE_SOUND( "common/npc_step2.wav" );
	PRECACHE_SOUND( "common/npc_step3.wav" );
	PRECACHE_SOUND( "common/npc_step4.wav" );

	PRECACHE_SOUND( "player/pl_metal1.wav" ); // walk on metal
	PRECACHE_SOUND( "player/pl_metal2.wav" );
	PRECACHE_SOUND( "player/pl_metal3.wav" );
	PRECACHE_SOUND( "player/pl_metal4.wav" );

	PRECACHE_SOUND( "player/pl_dirt1.wav" ); // walk on dirt
	PRECACHE_SOUND( "player/pl_dirt2.wav" );
	PRECACHE_SOUND( "player/pl_dirt3.wav" );
	PRECACHE_SOUND( "player/pl_dirt4.wav" );

	PRECACHE_SOUND( "player/pl_duct1.wav" ); // walk in duct
	PRECACHE_SOUND( "player/pl_duct2.wav" );
	PRECACHE_SOUND( "player/pl_duct3.wav" );
	PRECACHE_SOUND( "player/pl_duct4.wav" );

	PRECACHE_SOUND( "player/pl_grate1.wav" ); // walk on grate
	PRECACHE_SOUND( "player/pl_grate2.wav" );
	PRECACHE_SOUND( "player/pl_grate3.wav" );
	PRECACHE_SOUND( "player/pl_grate4.wav" );

	PRECACHE_SOUND( "player/pl_slosh1.wav" ); // walk in shallow water
	PRECACHE_SOUND( "player/pl_slosh2.wav" );
	PRECACHE_SOUND( "player/pl_slosh3.wav" );
	PRECACHE_SOUND( "player/pl_slosh4.wav" );

	PRECACHE_SOUND( "player/pl_tile1.wav" ); // walk on tile
	PRECACHE_SOUND( "player/pl_tile2.wav" );
	PRECACHE_SOUND( "player/pl_tile3.wav" );
	PRECACHE_SOUND( "player/pl_tile4.wav" );
	PRECACHE_SOUND( "player/pl_tile5.wav" );

	PRECACHE_SOUND( "player/pl_swim1.wav" ); // breathe bubbles
	PRECACHE_SOUND( "player/pl_swim2.wav" );
	PRECACHE_SOUND( "player/pl_swim3.wav" );
	PRECACHE_SOUND( "player/pl_swim4.wav" );

	PRECACHE_SOUND( "player/pl_ladder1.wav" ); // climb ladder rung
	PRECACHE_SOUND( "player/pl_ladder2.wav" );
	PRECACHE_SOUND( "player/pl_ladder3.wav" );
	PRECACHE_SOUND( "player/pl_ladder4.wav" );

	PRECACHE_SOUND( "player/pl_wade1.wav" ); // wade in water
	PRECACHE_SOUND( "player/pl_wade2.wav" );
	PRECACHE_SOUND( "player/pl_wade3.wav" );
	PRECACHE_SOUND( "player/pl_wade4.wav" );

	PRECACHE_SOUND( "debris/wood1.wav" ); // hit wood texture
	PRECACHE_SOUND( "debris/wood2.wav" );
	PRECACHE_SOUND( "debris/wood3.wav" );

	PRECACHE_SOUND( "plats/train_use1.wav" ); // use a train
	PRECACHE_SOUND( "plats/vehicle_ignition.wav" );

	PRECACHE_SOUND( "buttons/spark5.wav" ); // hit computer texture
	PRECACHE_SOUND( "buttons/spark6.wav" );
	PRECACHE_SOUND( "debris/glass1.wav" );
	PRECACHE_SOUND( "debris/glass2.wav" );
	PRECACHE_SOUND( "debris/glass3.wav" );

	PRECACHE_SOUND( SOUND_FLASHLIGHT_ON );
	PRECACHE_SOUND( SOUND_FLASHLIGHT_OFF );

	// player gib sounds
	PRECACHE_SOUND( "common/bodysplat.wav" );

	// player pain sounds
	PRECACHE_SOUND( "player/pl_pain2.wav" );
	PRECACHE_SOUND( "player/pl_pain4.wav" );
	PRECACHE_SOUND( "player/pl_pain5.wav" );
	PRECACHE_SOUND( "player/pl_pain6.wav" );
	PRECACHE_SOUND( "player/pl_pain7.wav" );

	PRECACHE_MODEL( "models/player.mdl" );

	// hud sounds

	PRECACHE_SOUND( "common/wpn_hudoff.wav" );
	PRECACHE_SOUND( "common/wpn_hudon.wav" );
	PRECACHE_SOUND( "common/wpn_moveselect.wav" );
	PRECACHE_SOUND( "common/wpn_select.wav" );
	PRECACHE_SOUND( "common/wpn_denyselect.wav" );

	// geiger sounds

	PRECACHE_SOUND( "player/geiger6.wav" );
	PRECACHE_SOUND( "player/geiger5.wav" );
	PRECACHE_SOUND( "player/geiger4.wav" );
	PRECACHE_SOUND( "player/geiger3.wav" );
	PRECACHE_SOUND( "player/geiger2.wav" );
	PRECACHE_SOUND( "player/geiger1.wav" );

	if ( giPrecacheGrunt )
		UTIL_PrecacheOther( "monster_human_grunt" );
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Half-Life";
}

/*
================
Sys_Error

Engine is going to shut down, allows setting a breakpoint in game .dll to catch that occasion
================
*/
void Sys_Error( const char *error_string )
{
	// Default case, do nothing.  MOD AUTHORS:  Add code ( e.g., _asm { int 3 }; here to cause a breakpoint for debugging your game .dlls
}

/*
================
PlayerCustomization

A new player customization has been registered on the server
Frame count definition for spray logo decal
animation right now.
================
*/
void PlayerCustomization( edict_t *pEntity, customization_t *pCust )
{
	entvars_t *pev       = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE( pEntity );

	if ( !pPlayer )
	{
		ALERT( at_console, "PlayerCustomization:  Couldn't get player!\n" );
		return;
	}

	if ( !pCust )
	{
		ALERT( at_console, "PlayerCustomization:  NULL customization!\n" );
		return;
	}

	switch ( pCust->resource.type )
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames( pCust->nUserData2 ); // Second int is max # of frames.
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT( at_console, "PlayerCustomization:  Unknown customization type!\n" );
		break;
	}
}

/*
================================
InconsistentFile

One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
================================
*/
int InconsistentFile( const edict_t *player, const char *filename, char *disconnect_message )
{
	// Server doesn't care?
	if ( CVAR_GET_FLOAT( "mp_consistency" ) != 1 )
		return 0;

	// Default behavior is to kick the player
	sprintf( disconnect_message, "Server is enforcing file consistency for %s\n", filename );

	// Kick now with specified disconnect message.
	return 1;
}
