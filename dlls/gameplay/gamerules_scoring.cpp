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

// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeMultiplay ::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 1;
}

//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
void CHalfLifeMultiplay ::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp     = CBaseEntity::Instance( pKiller );
	if ( ktmp && ( ktmp->Classify() == CLASS_PLAYER ) )
		peKiller = (CBasePlayer *)ktmp;
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

	DeathNotice( pVictim, pKiller, pInflictor );

	pVictim->m_iDeaths += 1;

	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );

	if ( pVictim->pev == pKiller )
	{ // killed self
		pKiller->frags -= 1;
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
		pKiller->frags += IPointsForKill( peKiller, pVictim );

		FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );
	}
	else
	{ // killed by the world
		pKiller->frags -= 1;
	}

	// update the scores
	// killed scores
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
	WRITE_BYTE( ENTINDEX( pVictim->edict() ) );
	WRITE_SHORT( pVictim->pev->frags );
	WRITE_SHORT( pVictim->m_iDeaths );
	WRITE_SHORT( 0 );
	WRITE_SHORT( GetTeamIndex( pVictim->m_szTeamName ) + 1 );
	MESSAGE_END();

	// killers score, if it's a player
	CBaseEntity *ep = CBaseEntity::Instance( pKiller );
	if ( ep && ep->Classify() == CLASS_PLAYER )
	{
		CBasePlayer *PK = (CBasePlayer *)ep;

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX( PK->edict() ) );
		WRITE_SHORT( PK->pev->frags );
		WRITE_SHORT( PK->m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( GetTeamIndex( PK->m_szTeamName ) + 1 );
		MESSAGE_END();

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}
#ifndef HLDEMO_BUILD
	if ( pVictim->HasNamedPlayerItem( "weapon_satchel" ) )
	{
		DeactivateSatchels( pVictim );
	}
#endif
}

//=========================================================
// Deathnotice.
//=========================================================
void CHalfLifeMultiplay::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor )
{
	// Work out what killed the player, and send a message to all clients about it
	CBaseEntity *Killer = CBaseEntity::Instance( pKiller );

	const char *killer_weapon_name = "world"; // by default, the player is killed by the world
	int killer_index               = 0;

	// Synchronize player name change
	char *tau   = "tau_cannon";
	char *gluon = "gluon gun";

	if ( pKiller->flags & FL_CLIENT )
	{
		killer_index = ENTINDEX( ENT( pKiller ) );

		if ( pevInflictor )
		{
			if ( pevInflictor == pKiller )
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pKiller );

				if ( pPlayer->m_pActiveItem )
				{
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
				}
			}
			else
			{
				killer_weapon_name = STRING( pevInflictor->classname ); // it's just that easy
			}
		}
	}
	else
	{
		killer_weapon_name = STRING( pevInflictor->classname );
	}

	// strip the monster_* or weapon_* from the inflictor's classname
	if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		killer_weapon_name += 7;
	else if ( strncmp( killer_weapon_name, "monster_", 8 ) == 0 )
		killer_weapon_name += 8;
	else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		killer_weapon_name += 5;

	MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
	WRITE_BYTE( killer_index );                 // the killer
	WRITE_BYTE( ENTINDEX( pVictim->edict() ) ); // the victim
	WRITE_STRING( killer_weapon_name );         // what they were killed by (should this be a string?)
	MESSAGE_END();

	// replace the code names with the 'real' names
	if ( !strcmp( killer_weapon_name, "egon" ) )
		killer_weapon_name = gluon;
	else if ( !strcmp( killer_weapon_name, "gauss" ) )
		killer_weapon_name = tau;

	if ( pVictim->pev == pKiller )
	{
		// killed self

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",
			                STRING( pVictim->pev->netname ),
			                GETPLAYERUSERID( pVictim->edict() ),
			                GETPLAYERAUTHID( pVictim->edict() ),
			                g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
			                killer_weapon_name );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\"\n",
			                STRING( pVictim->pev->netname ),
			                GETPLAYERUSERID( pVictim->edict() ),
			                GETPLAYERAUTHID( pVictim->edict() ),
			                GETPLAYERUSERID( pVictim->edict() ),
			                killer_weapon_name );
		}
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",
			                STRING( pKiller->netname ),
			                GETPLAYERUSERID( ENT( pKiller ) ),
			                GETPLAYERAUTHID( ENT( pKiller ) ),
			                g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( ENT( pKiller ) ), "model" ),
			                STRING( pVictim->pev->netname ),
			                GETPLAYERUSERID( pVictim->edict() ),
			                GETPLAYERAUTHID( pVictim->edict() ),
			                g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
			                killer_weapon_name );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" killed \"%s<%i><%s><%i>\" with \"%s\"\n",
			                STRING( pKiller->netname ),
			                GETPLAYERUSERID( ENT( pKiller ) ),
			                GETPLAYERAUTHID( ENT( pKiller ) ),
			                GETPLAYERUSERID( ENT( pKiller ) ),
			                STRING( pVictim->pev->netname ),
			                GETPLAYERUSERID( pVictim->edict() ),
			                GETPLAYERAUTHID( pVictim->edict() ),
			                GETPLAYERUSERID( pVictim->edict() ),
			                killer_weapon_name );
		}
	}
	else
	{
		// killed by the world

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n",
			                STRING( pVictim->pev->netname ),
			                GETPLAYERUSERID( pVictim->edict() ),
			                GETPLAYERAUTHID( pVictim->edict() ),
			                g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
			                killer_weapon_name );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\" (world)\n",
			                STRING( pVictim->pev->netname ),
			                GETPLAYERUSERID( pVictim->edict() ),
			                GETPLAYERAUTHID( pVictim->edict() ),
			                GETPLAYERUSERID( pVictim->edict() ),
			                killer_weapon_name );
		}
	}

	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
	WRITE_BYTE( 9 );                             // command length in bytes
	WRITE_BYTE( DRC_CMD_EVENT );                 // player killed
	WRITE_SHORT( ENTINDEX( pVictim->edict() ) ); // index number of primary entity
	if ( pevInflictor )
		WRITE_SHORT( ENTINDEX( ENT( pevInflictor ) ) ); // index number of secondary entity
	else
		WRITE_SHORT( ENTINDEX( ENT( pKiller ) ) ); // index number of secondary entity
	WRITE_LONG( 7 | DRC_FLAG_DRAMATIC );           // eventflags (priority and flags)
	MESSAGE_END();

	//  Print a standard message
	// Send message directly to console
	return; // just remove for now
	        /*
	            char	szText[ 128 ];
	    
	            if ( pKiller->flags & FL_MONSTER )
	            {
	                // killed by a monster
	                snprintf( szText, sizeof( szText ), "%s was killed by a monster.\n", STRING( pVictim->pev->netname ) );
	                return;
	            }
	    
	            if ( pKiller == pVictim->pev )
	            {
	                snprintf( szText, sizeof( szText ), "%s commited suicide.\n", STRING( pVictim->pev->netname ) );
	            }
	            else if ( pKiller->flags & FL_CLIENT )
	            {
	                snprintf( szText, sizeof( szText ), "%s : %s : %s\n", STRING( pKiller->netname ), killer_weapon_name, STRING( pVictim->pev->netname ) );
	            }
	            else if ( FClassnameIs ( pKiller, "worldspawn" ) )
	            {
	                snprintf( szText, sizeof( szText ), "%s fell or drowned or something.\n", STRING( pVictim->pev->netname ) );
	            }
	            else if ( pKiller->solid == SOLID_BSP )
	            {
	                snprintf( szText, sizeof( szText ), "%s was mooshed.\n", STRING( pVictim->pev->netname ) );
	            }
	            else
	            {
	                snprintf( szText, sizeof( szText ), "%s died mysteriously.\n", STRING( pVictim->pev->netname ) );
	            }
	    
	            UTIL_ClientPrintAll( szText );
	        */
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world

#define MAX_MOTD_CHUNK 60
#define MAX_MOTD_LENGTH 1536 // (MAX_MOTD_CHUNK * 4)

void CHalfLifeMultiplay ::SendMOTDToClient( edict_t *client )
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char *)LOAD_FILE_FOR_ME( (char *)CVAR_GET_STRING( "motdfile" ), &length );

	// send the server name
	MESSAGE_BEGIN( MSG_ONE, gmsgServerName, NULL, client );
	WRITE_STRING( CVAR_GET_STRING( "hostname" ) );
	MESSAGE_END();

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while ( pFileList && *pFileList && char_count < MAX_MOTD_LENGTH )
	{
		char chunk[MAX_MOTD_CHUNK + 1];

		if ( strlen( pFileList ) < MAX_MOTD_CHUNK )
		{
			strncpy( chunk, pFileList, sizeof( chunk ) - 1 );
			chunk[sizeof( chunk ) - 1] = '\0';
		}
		else
		{
			strncpy( chunk, pFileList, MAX_MOTD_CHUNK );
			chunk[MAX_MOTD_CHUNK] = 0; // strncpy doesn't always append the null terminator
		}

		char_count += strlen( chunk );
		if ( char_count < MAX_MOTD_LENGTH )
			pFileList = aFileList + char_count;
		else
			*pFileList = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, client );
		WRITE_BYTE( *pFileList ? FALSE : TRUE ); // FALSE means there is still more message to come
		WRITE_STRING( chunk );
		MESSAGE_END();
	}

	FREE_FILE( aFileList );
}

void CHalfLifeMultiplay ::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	// Set preferences
	pPlayer->SetPrefsFromUserinfo( infobuffer );
}
