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

extern void CopyToBodyQue( entvars_t *pev );

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay ::ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128] )
{
	g_VoiceGameMgr.ClientConnected( pEntity );
	return TRUE;
}

extern int gmsgSayText;
extern int gmsgGameMode;

void CHalfLifeMultiplay ::UpdateGameMode( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
	WRITE_BYTE( 0 ); // game mode none
	MESSAGE_END();
}

void CHalfLifeMultiplay ::InitHUD( CBasePlayer *pl )
{
	// notify other clients of player joining the game
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s has joined the game\n", ( pl->pev->netname && STRING( pl->pev->netname )[0] != 0 ) ? STRING( pl->pev->netname ) : "unconnected" ) );

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" entered the game\n",
		                STRING( pl->pev->netname ),
		                GETPLAYERUSERID( pl->edict() ),
		                GETPLAYERAUTHID( pl->edict() ),
		                g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "model" ) );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" entered the game\n",
		                STRING( pl->pev->netname ),
		                GETPLAYERUSERID( pl->edict() ),
		                GETPLAYERAUTHID( pl->edict() ),
		                GETPLAYERUSERID( pl->edict() ) );
	}

	UpdateGameMode( pl );

	// sending just one score makes the hud scoreboard active;  otherwise
	// it is just disabled for single play
	MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
	WRITE_BYTE( ENTINDEX( pl->edict() ) );
	WRITE_SHORT( 0 );
	WRITE_SHORT( 0 );
	WRITE_SHORT( 0 );
	WRITE_SHORT( 0 );
	MESSAGE_END();

	SendMOTDToClient( pl->edict() );

	// loop through all active players and send their score info to the new client
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		// Access player death counter directly
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
			WRITE_BYTE( i ); // client number
			WRITE_SHORT( plr->pev->frags );
			WRITE_SHORT( plr->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
			MESSAGE_END();
		}
	}

	if ( g_fGameOver )
	{
		MESSAGE_BEGIN( MSG_ONE, SVC_INTERMISSION, NULL, pl->edict() );
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay ::ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		if ( pPlayer )
		{
			FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );

			// team match?
			if ( g_teamplay )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected\n",
				                STRING( pPlayer->pev->netname ),
				                GETPLAYERUSERID( pPlayer->edict() ),
				                GETPLAYERAUTHID( pPlayer->edict() ),
				                g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" ) );
			}
			else
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%i>\" disconnected\n",
				                STRING( pPlayer->pev->netname ),
				                GETPLAYERUSERID( pPlayer->edict() ),
				                GETPLAYERAUTHID( pPlayer->edict() ),
				                GETPLAYERUSERID( pPlayer->edict() ) );
			}

			pPlayer->RemoveAllItems( TRUE ); // destroy all of the players weapons and items
		}
	}
}

//=========================================================

//=========================================================
void CHalfLifeMultiplay ::PlayerThink( CBasePlayer *pPlayer )
{
	if ( g_fGameOver )
	{
		// check for button presses
		if ( pPlayer->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) )
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed  = 0;
		pPlayer->pev->button        = 0;
		pPlayer->m_afButtonReleased = 0;
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay ::PlayerSpawn( CBasePlayer *pPlayer )
{
	BOOL addDefault;
	CBaseEntity *pWeaponEntity = NULL;

	int iAutoWepSwitch        = pPlayer->m_iAutoWepSwitch;
	pPlayer->m_iAutoWepSwitch = 1;

	pPlayer->pev->weapons |= ( 1 << WEAPON_SUIT );

	addDefault = TRUE;

	while ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" ) )
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if ( addDefault )
	{
		pPlayer->GiveNamedItem( "weapon_crowbar" );
		pPlayer->GiveNamedItem( "weapon_9mmhandgun" );
		pPlayer->GiveAmmo( 68, "9mm", _9MM_MAX_CARRY ); // 4 full reloads
	}

	pPlayer->m_iAutoWepSwitch = iAutoWepSwitch;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay ::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay ::FlPlayerSpawnTime( CBasePlayer *pPlayer )
{
	return gpGlobals->time; // now!
}

BOOL CHalfLifeMultiplay ::AllowAutoTargetCrosshair( void )
{
	return ( aimcrosshair.value != 0 );
}

//=========================================================

edict_t *CHalfLifeMultiplay::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = CGameRules::GetPlayerSpawnSpot( pPlayer );
	if ( IsMultiplayer() && pentSpawnSpot->v.target )
	{
		FireTargets( STRING( pentSpawnSpot->v.target ), pPlayer, pPlayer, USE_TOGGLE, 0 );
	}

	return pentSpawnSpot;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay ::PlayerRespawn( CBasePlayer *pPlayer, BOOL fCopyCorpse )
{
	if ( fCopyCorpse )
	{
		CopyToBodyQue( pPlayer->pev );
	}

	pPlayer->Spawn();
}
