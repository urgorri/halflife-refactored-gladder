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
//
// teamplay_gamerules.cpp
//
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "weapons/weapon_base.h"
#include "weapons/projectile_satchel.h"
#include "gameplay/gamerules.h"

#include "core/skill.h"
#include "core/game.h"
#include "items/item_base.h"
#include "voice_gamemgr.h"
#include "hltv.h"
#include "world/trains.h"

#if !defined( _WIN32 )
#include <ctype.h>
#endif

float g_flIntermissionStartTime = 0;
CVoiceGameMgr g_VoiceGameMgr;

class CMultiplayGameMgrHelper : public IVoiceGameMgrHelper
{
  public:
	virtual bool CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker )
	{
		if ( g_teamplay )
		{
			if ( g_pGameRules->PlayerRelationship( pListener, pTalker ) != GR_TEAMMATE )
			{
				return false;
			}
		}

		return true;
	}
};
static CMultiplayGameMgrHelper g_GameMgrHelper;

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

CHalfLifeMultiplay ::CHalfLifeMultiplay()
{
	g_VoiceGameMgr.Init( &g_GameMgrHelper, gpGlobals->maxClients );

	RefreshSkillData();
	m_flIntermissionEndTime   = 0;
	g_flIntermissionStartTime = 0;

	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if ( IS_DEDICATED_SERVER() )
	{
		// this code has been moved into engine, to only run server.cfg once
	}
	else
	{
		// listen server
		char *lservercfgfile = (char *)CVAR_GET_STRING( "lservercfgfile" );

		if ( lservercfgfile && lservercfgfile[0] )
		{
			char szCommand[256];

			ALERT( at_console, "Executing listen server config file\n" );
			snprintf( szCommand, sizeof( szCommand ), "exec %s\n", lservercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}
}

BOOL CHalfLifeMultiplay::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if ( g_VoiceGameMgr.ClientCommand( pPlayer, pcmd ) )
		return TRUE;

	return CGameRules::ClientCommand( pPlayer, pcmd );
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::RefreshSkillData( void )
{
	// load all default values
	CGameRules::RefreshSkillData();

	// override some values for multiplay.

	// suitcharger
	gSkillData.suitchargerCapacity = 30;

	// Crowbar whack
	gSkillData.plrDmgCrowbar = 25;

	// Glock Round
	gSkillData.plrDmg9MM = 12;

	// 357 Round
	gSkillData.plrDmg357 = 50;

	// MP5 Round
	gSkillData.plrDmgMP5 = 12;

	// M203 grenade
	gSkillData.plrDmgM203Grenade = 100;

	// Shotgun buckshot
	gSkillData.plrDmgBuckshot = 20; // fewer pellets in deathmatch

	// Crossbow
	gSkillData.plrDmgCrossbowClient = 20;

	// RPG
	gSkillData.plrDmgRPG = 120;

	// Egon
	gSkillData.plrDmgEgonWide   = 20;
	gSkillData.plrDmgEgonNarrow = 10;

	// Hand Grendade
	gSkillData.plrDmgHandGrenade = 100;

	// Satchel Charge
	gSkillData.plrDmgSatchel = 120;

	// Tripmine
	gSkillData.plrDmgTripmine = 150;

	// hornet
	gSkillData.plrDmgHornet = 10;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay ::Think( void )
{
	g_VoiceGameMgr.Update( gpGlobals->frametime );

	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining  = 0;

	if ( g_fGameOver ) // someone else quit the game already
	{
		// bounds check
		int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
		if ( time < 1 )
			CVAR_SET_STRING( "mp_chattime", "1" );
		else if ( time > MAX_INTERMISSION_TIME )
			CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

		m_flIntermissionEndTime = g_flIntermissionStartTime + mp_chattime.value;

		// check to see if we should change levels now
		if ( m_flIntermissionEndTime < gpGlobals->time )
		{
			if ( m_iEndIntermissionButtonHit // check that someone has pressed a key, or the max intermission time is over
			     || ( ( g_flIntermissionStartTime + MAX_INTERMISSION_TIME ) < gpGlobals->time ) )
				ChangeLevel(); // intermission is over
		}

		return;
	}

	float flTimeLimit = timelimit.value * 60;
	float flFragLimit = fraglimit.value;

	time_remaining = (int)( flTimeLimit ? ( flTimeLimit - gpGlobals->time ) : 0 );

	if ( flTimeLimit != 0 && gpGlobals->time >= flTimeLimit )
	{
		GoToIntermission();
		return;
	}

	if ( flFragLimit )
	{
		int bestfrags = 9999;
		int remain;

		// check if any player is over the frag limit
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

			if ( pPlayer && pPlayer->pev->frags >= flFragLimit )
			{
				GoToIntermission();
				return;
			}

			if ( pPlayer )
			{
				remain = flFragLimit - pPlayer->pev->frags;
				if ( remain < bestfrags )
				{
					bestfrags = remain;
				}
			}
		}
		frags_remaining = bestfrags;
	}

	// Updates when frags change
	if ( frags_remaining != last_frags )
	{
		g_engfuncs.pfnCvar_DirectSet( &fragsleft, UTIL_VarArgs( "%i", frags_remaining ) );
	}

	// Updates once per second
	if ( timeleft.value != last_time )
	{
		g_engfuncs.pfnCvar_DirectSet( &timeleft, UTIL_VarArgs( "%i", time_remaining ) );
	}

	last_frags = frags_remaining;
	last_time  = time_remaining;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsMultiplayer( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsDeathmatch( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsCoOp( void )
{
	return gpGlobals->coop;
}

//=========================================================
float CHalfLifeMultiplay ::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	int iFallDamage = (int)falldamage.value;

	switch ( iFallDamage )
	{
	case 1: // progressive
		pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
		return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
		break;
	default:
	case 0: // fixed
		return 10;
		break;
	}
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	return TRUE;
}

//=========================================================

//=========================================================
//=========================================================
int CHalfLifeMultiplay::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	// half life deathmatch has only enemies
	return GR_NOTTEAMMATE;
}

BOOL CHalfLifeMultiplay ::PlayFootstepSounds( CBasePlayer *pl, float fvol )
{
	if ( g_footsteps && g_footsteps->value == 0 )
		return FALSE;

	if ( pl->IsOnLadder() || pl->pev->velocity.Length2D() > 220 )
		return TRUE; // only make step sounds in multiplayer if the player is moving fast enough

	return FALSE;
}

BOOL CHalfLifeMultiplay ::FAllowFlashlight( void )
{
	return flashlight.value != 0;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay ::FAllowMonsters( void )
{
	return ( allowmonsters.value != 0 );
}

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME 6

void CHalfLifeMultiplay ::GoToIntermission( void )
{
	if ( g_fGameOver )
		return; // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN( MSG_ALL, SVC_INTERMISSION );
	MESSAGE_END();

	// bounds check
	int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
	if ( time < 1 )
		CVAR_SET_STRING( "mp_chattime", "1" );
	else if ( time > MAX_INTERMISSION_TIME )
		CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

	m_flIntermissionEndTime   = gpGlobals->time + ( (int)mp_chattime.value );
	g_flIntermissionStartTime = gpGlobals->time;

	g_fGameOver                 = TRUE;
	m_iEndIntermissionButtonHit = FALSE;
}

#define MAX_RULE_BUFFER 1024

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s *next;

	char mapname[32];
	int minplayers, maxplayers;
	char rulebuffer[MAX_RULE_BUFFER];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	struct mapcycle_item_s *items;
	struct mapcycle_item_s *next_item;
} mapcycle_t;

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle( mapcycle_t *cycle )
{
	mapcycle_item_t *p, *n, *start;
	p = cycle->items;
	if ( p )
	{
		start = p;
		p     = p->next;
		while ( p != start )
		{
			n = p->next;
			delete p;
			p = n;
		}

		delete cycle->items;
	}
	cycle->items     = NULL;
	cycle->next_item = NULL;
}

static char com_token[1500];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse( char *data )
{
	int c;
	int len;

	len          = 0;
	com_token[0] = 0;

	if ( !data )
		return NULL;

// skip whitespace
skipwhite:
	while ( ( c = *data ) <= ' ' )
	{
		if ( c == 0 )
			return NULL; // end of file;
		data++;
	}

	// skip // comments
	if ( c == '/' && data[1] == '/' )
	{
		while ( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if ( c == '\"' )
	{
		data++;
		while ( 1 )
		{
			c = *data++;
			if ( c == '\"' || !c )
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' )
			break;
	} while ( c > 32 );

	com_token[len] = 0;
	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( char *buffer )
{
	char *p;

	p = buffer;
	while ( *p && *p != '\n' )
	{
		if ( !isspace( *p ) || isalnum( *p ) )
			return 1;

		p++;
	}

	return 0;
}

/*
==============
ReloadMapCycleFile


Parses mapcycle.txt file into mapcycle_t structure
==============
*/
int ReloadMapCycleFile( char *filename, mapcycle_t *cycle )
{
	char szBuffer[MAX_RULE_BUFFER];
	char szMap[32];
	int length;
	char *pFileList;
	char *aFileList = pFileList = (char *)LOAD_FILE_FOR_ME( filename, &length );
	int hasbuffer;
	mapcycle_item_s *item, *newlist = NULL, *next;

	if ( pFileList && length )
	{
		// the first map name in the file becomes the default
		while ( 1 )
		{
			hasbuffer = 0;
			memset( szBuffer, 0, MAX_RULE_BUFFER );

			pFileList = COM_Parse( pFileList );
			if ( strlen( com_token ) <= 0 )
				break;

			strncpy( szMap, com_token, sizeof( szMap ) );
			szMap[sizeof( szMap ) - 1] = '\0';

			// Any more tokens on this line?
			if ( COM_TokenWaiting( pFileList ) )
			{
				pFileList = COM_Parse( pFileList );
				if ( strlen( com_token ) > 0 )
				{
					hasbuffer = 1;
					strncpy( szBuffer, com_token, sizeof( szBuffer ) );
					szBuffer[sizeof( szBuffer ) - 1] = '\0';
				}
			}

			// Check map
			if ( IS_MAP_VALID( szMap ) )
			{
				// Create entry
				char *s;

				item = new mapcycle_item_s;

				strncpy( item->mapname, szMap, sizeof( item->mapname ) - 1 );
				item->mapname[sizeof( item->mapname ) - 1] = '\0';

				item->minplayers = 0;
				item->maxplayers = 0;

				memset( item->rulebuffer, 0, MAX_RULE_BUFFER );

				if ( hasbuffer )
				{
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "minplayers" );
					if ( s && s[0] )
					{
						item->minplayers = atoi( s );
						item->minplayers = max( item->minplayers, 0 );
						item->minplayers = min( item->minplayers, gpGlobals->maxClients );
					}
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "maxplayers" );
					if ( s && s[0] )
					{
						item->maxplayers = atoi( s );
						item->maxplayers = max( item->maxplayers, 0 );
						item->maxplayers = min( item->maxplayers, gpGlobals->maxClients );
					}

					// Remove keys
					//
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "minplayers" );
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "maxplayers" );

					strncpy( item->rulebuffer, szBuffer, sizeof( item->rulebuffer ) - 1 );
					item->rulebuffer[sizeof( item->rulebuffer ) - 1] = '\0';
				}

				item->next   = cycle->items;
				cycle->items = item;
			}
			else
			{
				ALERT( at_console, "Skipping %s from mapcycle, not a valid map\n", szMap );
			}
		}

		FREE_FILE( aFileList );
	}

	// Fixup circular list pointer
	item = cycle->items;

	// Reverse it to get original order
	while ( item )
	{
		next       = item->next;
		item->next = newlist;
		newlist    = item;
		item       = next;
	}
	cycle->items = newlist;
	item         = cycle->items;

	// Didn't parse anything
	if ( !item )
	{
		return 0;
	}

	while ( item->next )
	{
		item = item->next;
	}
	item->next = cycle->items;

	cycle->next_item = item->next;

	return 1;
}

/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers( void )
{
	int num = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

		if ( pEnt )
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map command is issued on server
 level transition
==============
*/
void ExtractCommandString( char *s, char *szCommand )
{
	// Now make rules happen
	char pkey[512];
	char value[512]; // use two buffers so compares
	                 // work without stomping on each other
	char *o;

	if ( *s == '\\' )
		s++;

	while ( 1 )
	{
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while ( *s != '\\' && *s )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;

		strncat( szCommand, pkey, 1500 - strlen( szCommand ) - 1 );
		if ( strlen( value ) > 0 )
		{
			strncat( szCommand, " ", 1500 - strlen( szCommand ) - 1 );
			strncat( szCommand, value, 1500 - strlen( szCommand ) - 1 );
		}
		strncat( szCommand, "\n", 1500 - strlen( szCommand ) - 1 );

		if ( !*s )
			return;
		s++;
	}
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeMultiplay ::ChangeLevel( void )
{
	static char szPreviousMapCycleFile[256];
	static mapcycle_t mapcycle;

	char szNextMap[32];
	char szFirstMapInList[32];
	char szCommands[1500];
	char szRules[1500];
	int minplayers = 0, maxplayers = 0;
	strncpy( szFirstMapInList, "hldm1", sizeof( szFirstMapInList ) - 1 ); // the absolute default level is hldm1
	szFirstMapInList[sizeof( szFirstMapInList ) - 1] = '\0';

	int curplayers;
	BOOL do_cycle = TRUE;

	// find the map to change to
	char *mapcfile = (char *)CVAR_GET_STRING( "mapcyclefile" );
	ASSERT( mapcfile != NULL );

	szCommands[0] = '\0';
	szRules[0]    = '\0';

	curplayers = CountPlayers();

	// Has the map cycle filename changed?
	if ( stricmp( mapcfile, szPreviousMapCycleFile ) )
	{
		strncpy( szPreviousMapCycleFile, mapcfile, sizeof( szPreviousMapCycleFile ) - 1 );
		szPreviousMapCycleFile[sizeof( szPreviousMapCycleFile ) - 1] = '\0';

		DestroyMapCycle( &mapcycle );

		if ( !ReloadMapCycleFile( mapcfile, &mapcycle ) || ( !mapcycle.items ) )
		{
			ALERT( at_console, "Unable to load map cycle file %s\n", mapcfile );
			do_cycle = FALSE;
		}
	}

	if ( do_cycle && mapcycle.items )
	{
		BOOL keeplooking = FALSE;
		BOOL found       = FALSE;
		mapcycle_item_s *item;

		// Assume current map
		strncpy( szNextMap, STRING( gpGlobals->mapname ), sizeof( szNextMap ) - 1 );
		szNextMap[sizeof( szNextMap ) - 1] = '\0';
		strncpy( szFirstMapInList, STRING( gpGlobals->mapname ), sizeof( szFirstMapInList ) - 1 );
		szFirstMapInList[sizeof( szFirstMapInList ) - 1] = '\0';

		// Traverse list
		for ( item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next )
		{
			keeplooking = FALSE;

			ASSERT( item != NULL );

			if ( item->minplayers != 0 )
			{
				if ( curplayers >= item->minplayers )
				{
					found      = TRUE;
					minplayers = item->minplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( item->maxplayers != 0 )
			{
				if ( curplayers <= item->maxplayers )
				{
					found      = TRUE;
					maxplayers = item->maxplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( keeplooking )
				continue;

			found = TRUE;
			break;
		}

		if ( !found )
		{
			item = mapcycle.next_item;
		}

		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		strncpy( szNextMap, item->mapname, sizeof( szNextMap ) - 1 );
		szNextMap[sizeof( szNextMap ) - 1] = '\0';

		ExtractCommandString( item->rulebuffer, szCommands );
		strncpy( szRules, item->rulebuffer, sizeof( szRules ) - 1 );
		szRules[sizeof( szRules ) - 1] = '\0';
	}

	if ( !IS_MAP_VALID( szNextMap ) )
	{
		strncpy( szNextMap, szFirstMapInList, sizeof( szNextMap ) - 1 );
		szNextMap[sizeof( szNextMap ) - 1] = '\0';
	}

	g_fGameOver = TRUE;

	ALERT( at_console, "CHANGE LEVEL: %s\n", szNextMap );
	if ( minplayers || maxplayers )
	{
		ALERT( at_console, "PLAYER COUNT:  min %i max %i current %i\n", minplayers, maxplayers, curplayers );
	}
	if ( strlen( szRules ) > 0 )
	{
		ALERT( at_console, "RULES:  %s\n", szRules );
	}

	CHANGE_LEVEL( szNextMap, NULL );
	if ( strlen( szCommands ) > 0 )
	{
		SERVER_COMMAND( szCommands );
	}
}

//=========================================================
//=========================================================
// Busters Gamerules
//=========================================================
//=========================================================

#define EGON_BUSTING_TIME 10

bool IsBustingGame()
{
	return sv_busters.value == 1;
}

bool IsPlayerBusting( CBaseEntity *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsPlayer() || !IsBustingGame() )
		return FALSE;

	return ( (CBasePlayer *)pPlayer )->HasPlayerItemFromID( WEAPON_EGON );
}

BOOL BustingCanHaveItem( CBasePlayer *pPlayer, CBaseEntity *pItem )
{
	BOOL bIsWeaponOrAmmo = FALSE;

	if ( strstr( STRING( pItem->pev->classname ), "weapon_" ) || strstr( STRING( pItem->pev->classname ), "ammo_" ) )
	{
		bIsWeaponOrAmmo = TRUE;
	}

	// Busting players can't have ammo nor weapons
	if ( IsPlayerBusting( pPlayer ) && bIsWeaponOrAmmo )
		return FALSE;

	return TRUE;
}

//=========================================================
CMultiplayBusters::CMultiplayBusters()
{
	m_flEgonBustingCheckTime = -1;
}

//=========================================================
void CMultiplayBusters::Think()
{
	CheckForEgons();

	CHalfLifeMultiplay::Think();
}

//=========================================================
int CMultiplayBusters::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	// If the attacker is busting, they get a point per kill
	if ( IsPlayerBusting( pAttacker ) )
		return 1;

	// If the victim is busting, then the attacker gets a point
	if ( IsPlayerBusting( pKilled ) )
		return 2;

	return 0;
}

//=========================================================
void CMultiplayBusters::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	if ( IsPlayerBusting( pVictim ) )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "The Buster is dead!!" );

		// Reset egon check time
		m_flEgonBustingCheckTime = -1;

		CBasePlayer *peKiller = NULL;
		CBaseEntity *ktmp     = CBaseEntity::Instance( pKiller );

		if ( ktmp && ( ktmp->Classify() == CLASS_PLAYER ) )
		{
			peKiller = (CBasePlayer *)ktmp;
		}
		else if ( ktmp && ( ktmp->Classify() == CLASS_VEHICLE ) )
		{
			CBasePlayer *pDriver = ( (CFuncVehicle *)ktmp )->m_pDriver;

			if ( pDriver != NULL )
			{
				peKiller = pDriver;
				ktmp     = pDriver;
				pKiller  = pDriver->pev;
			}
		}

		if ( peKiller )
		{
			UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "%s has has killed the Buster!\n", STRING( peKiller->pev->netname ) ) );
		}

		pVictim->pev->renderfx    = kRenderFxNone;
		pVictim->pev->rendercolor = g_vecZero;
		// pVictim->pev->effects &= ~EF_BRIGHTFIELD;
	}

	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );
}

//=========================================================
void CMultiplayBusters::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor )
{
	// Only death notices that the Buster was involved in in Busting game mode
	if ( !IsPlayerBusting( pVictim ) && !IsPlayerBusting( CBaseEntity::Instance( pKiller ) ) )
		return;

	CHalfLifeMultiplay::DeathNotice( pVictim, pKiller, pevInflictor );
}

//=========================================================
int CMultiplayBusters::WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon->m_iId == WEAPON_EGON )
		return GR_WEAPON_RESPAWN_NO;

	return CHalfLifeMultiplay::WeaponShouldRespawn( pWeapon );
}

//=========================================================
// CheckForEgons:
// Check to see if any player has an egon
// If they don't then get the lowest player on the scoreboard and give them one
// Then check to see if any weapon boxes out there has an egon, and delete it
//=========================================================
void CMultiplayBusters::CheckForEgons()
{
	if ( m_flEgonBustingCheckTime <= 0.0f )
	{
		m_flEgonBustingCheckTime = gpGlobals->time + EGON_BUSTING_TIME;
		return;
	}

	if ( m_flEgonBustingCheckTime <= gpGlobals->time )
	{
		m_flEgonBustingCheckTime = -1.0f;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

			// Someone is busting, no need to continue
			if ( IsPlayerBusting( pPlayer ) )
				return;
		}

		int bBestFrags           = 9999;
		CBasePlayer *pBestPlayer = NULL;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( pPlayer && pPlayer->pev->frags <= bBestFrags )
			{
				bBestFrags  = pPlayer->pev->frags;
				pBestPlayer = pPlayer;
			}
		}

		if ( pBestPlayer )
		{
			pBestPlayer->GiveNamedItem( "weapon_egon" );

			CBaseEntity *pEntity = NULL;

			// Find a weaponbox that includes an Egon, then destroy it
			while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "weaponbox" ) ) != NULL )
			{
				CWeaponBox *pWeaponBox = (CWeaponBox *)pEntity;

				if ( pWeaponBox )
				{
					CBasePlayerItem *pWeapon;

					for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
					{
						pWeapon = pWeaponBox->m_rgpPlayerItems[i];

						while ( pWeapon )
						{
							// There you are, bye box
							if ( pWeapon->m_iId == WEAPON_EGON )
							{
								pWeaponBox->Kill();
								break;
							}

							pWeapon = pWeapon->m_pNext;
						}
					}
				}
			}
		}
	}
}

//=========================================================
BOOL CMultiplayBusters::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	// Buster cannot have more weapons nor ammo
	if ( BustingCanHaveItem( pPlayer, pItem ) == FALSE )
	{
		return FALSE;
	}

	return CHalfLifeMultiplay::CanHavePlayerItem( pPlayer, pItem );
}

//=========================================================
BOOL CMultiplayBusters::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	// Buster cannot have more weapons nor ammo
	if ( BustingCanHaveItem( pPlayer, pItem ) == FALSE )
	{
		return FALSE;
	}

	return CHalfLifeMultiplay::CanHaveItem( pPlayer, pItem );
}

//=========================================================
void CMultiplayBusters::PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	if ( pWeapon->m_iId == WEAPON_EGON )
	{
		pPlayer->RemoveAllItems( false );

		UTIL_ClientPrintAll( HUD_PRINTCENTER, "Long live the new Buster!" );
		UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "%s is busting!\n", STRING( pPlayer->pev->netname ) ) );

		SetPlayerModel( pPlayer );

		pPlayer->pev->health     = pPlayer->pev->max_health;
		pPlayer->pev->armorvalue = 100;

		pPlayer->pev->renderfx    = kRenderFxGlowShell;
		pPlayer->pev->renderamt   = 25;
		pPlayer->pev->rendercolor = Vector( 0, 75, 250 );

		CBasePlayerWeapon *pEgon = (CBasePlayerWeapon *)pWeapon;

		pEgon->m_iDefaultAmmo                        = 100;
		pPlayer->m_rgAmmo[pEgon->m_iPrimaryAmmoType] = pEgon->m_iDefaultAmmo;

		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "ivan" );
	}
}

void CMultiplayBusters::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	SetPlayerModel( pPlayer );

	// Set preferences
	pPlayer->SetPrefsFromUserinfo( infobuffer );
}

void CMultiplayBusters::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );
	SetPlayerModel( pPlayer );
}

void CMultiplayBusters::SetPlayerModel( CBasePlayer *pPlayer )
{
	if ( IsPlayerBusting( pPlayer ) )
	{
		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "ivan" );
	}
	else
	{
		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "skeleton" );
	}
}
