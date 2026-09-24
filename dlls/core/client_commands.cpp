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
#include "core/client.h"
#include "gameplay/gamerules.h"
#include "core/game.h"
#include "core/player_network.h"
#include "systems/crash_handler.h"

#if !defined( _WIN32 )
#include <ctype.h>
#endif

extern cvar_t allow_spectators;
extern int g_teamplay;

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;

#if defined( _MSC_VER ) || defined( WIN32 )
typedef wchar_t uchar16;
typedef unsigned int uchar32;
#else
typedef unsigned short uchar16;
typedef wchar_t uchar32;
#endif

//-----------------------------------------------------------------------------
// Purpose: determine if a uchar32 represents a valid Unicode code point
//-----------------------------------------------------------------------------
bool Q_IsValidUChar32( uchar32 uVal )
{
	// Values > 0x10FFFF are explicitly invalid; ditto for UTF-16 surrogate halves,
	// values ending in FFFE or FFFF, or values in the 0x00FDD0-0x00FDEF reserved range
	return ( uVal < 0x110000u ) && ( ( uVal - 0x00D800u ) > 0x7FFu ) && ( ( uVal & 0xFFFFu ) < 0xFFFEu ) && ( ( uVal - 0x00FDD0u ) > 0x1Fu );
}

// Decode one character from a UTF-8 encoded string. Treats 6-byte CESU-8 sequences
// as a single character, as if they were a correctly-encoded 4-byte UTF-8 sequence.
int Q_UTF8ToUChar32( const char *pUTF8_, uchar32 &uValueOut, bool &bErrorOut )
{
	const uint8 *pUTF8 = (const uint8 *)pUTF8_;

	int nBytes       = 1;
	uint32 uValue    = pUTF8[0];
	uint32 uMinValue = 0;

	// 0....... single byte
	if ( uValue < 0x80 )
		goto decodeFinishedNoCheck;

	// Expecting at least a two-byte sequence with 0xC0 <= first <= 0xF7 (110...... and 11110...)
	if ( ( uValue - 0xC0u ) > 0x37u || ( pUTF8[1] & 0xC0 ) != 0x80 )
		goto decodeError;

	uValue    = ( uValue << 6 ) - ( 0xC0 << 6 ) + pUTF8[1] - 0x80;
	nBytes    = 2;
	uMinValue = 0x80;

	// 110..... two-byte lead byte
	if ( !( uValue & ( 0x20 << 6 ) ) )
		goto decodeFinished;

	// Expecting at least a three-byte sequence
	if ( ( pUTF8[2] & 0xC0 ) != 0x80 )
		goto decodeError;

	uValue    = ( uValue << 6 ) - ( 0x20 << 12 ) + pUTF8[2] - 0x80;
	nBytes    = 3;
	uMinValue = 0x800;

	// 1110.... three-byte lead byte
	if ( !( uValue & ( 0x10 << 12 ) ) )
		goto decodeFinishedMaybeCESU8;

	// Expecting a four-byte sequence, longest permissible in UTF-8
	if ( ( pUTF8[3] & 0xC0 ) != 0x80 )
		goto decodeError;

	uValue    = ( uValue << 6 ) - ( 0x10 << 18 ) + pUTF8[3] - 0x80;
	nBytes    = 4;
	uMinValue = 0x10000;

	// 11110... four-byte lead byte. fall through to finished.

decodeFinished:
	if ( uValue >= uMinValue && Q_IsValidUChar32( uValue ) )
	{
	decodeFinishedNoCheck:
		uValueOut = uValue;
		bErrorOut = false;
		return nBytes;
	}
decodeError:
	uValueOut = '?';
	bErrorOut = true;
	return nBytes;

decodeFinishedMaybeCESU8:
	// Do we have a full UTF-16 surrogate pair that's been UTF-8 encoded afterwards?
	// That is, do we have 0xD800-0xDBFF followed by 0xDC00-0xDFFF? If so, decode it all.
	if ( ( uValue - 0xD800u ) < 0x400u && pUTF8[3] == 0xED && (uint8)( pUTF8[4] - 0xB0 ) < 0x10 && ( pUTF8[5] & 0xC0 ) == 0x80 )
	{
		uValue    = 0x10000 + ( ( uValue - 0xD800u ) << 10 ) + ( (uint8)( pUTF8[4] - 0xB0 ) << 6 ) + pUTF8[5] - 0x80;
		nBytes    = 6;
		uMinValue = 0x10000;
	}
	goto decodeFinished;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if UTF-8 string contains invalid sequences.
//-----------------------------------------------------------------------------
bool Q_UnicodeValidate( const char *pUTF8 )
{
	bool bError = false;
	while ( *pUTF8 )
	{
		uchar32 uVal;
		// Our UTF-8 decoder silently fixes up 6-byte CESU-8 (improperly re-encoded UTF-16) sequences.
		// However, these are technically not valid UTF-8. So if we eat 6 bytes at once, it's an error.
		int nCharSize = Q_UTF8ToUChar32( pUTF8, uVal, bError );
		if ( bError || nCharSize == 6 )
			return false;
		pUTF8 += nCharSize;
	}
	return true;
}

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
void Host_Say( edict_t *pEntity, int teamonly )
{
	CBasePlayer *client;
	int j;
	char *p;
	char text[128];
	char szTemp[256];
	const char *cpSay     = "say";
	const char *cpSayTeam = "say_team";
	const char *pcmd      = CMD_ARGV( 0 );

	// We can get a raw string now, without the "say " prepended
	if ( CMD_ARGC() == 0 )
		return;

	entvars_t *pev      = &pEntity->v;
	CBasePlayer *player = GetClassPtr( (CBasePlayer *)pev );

	// Not yet.
	if ( player->m_flNextChatTime > gpGlobals->time )
		return;

	if ( !stricmp( pcmd, cpSay ) || !stricmp( pcmd, cpSayTeam ) )
	{
		if ( CMD_ARGC() >= 2 )
		{
			p = (char *)CMD_ARGS();
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else // Raw text, need to prepend argv[0]
	{
		if ( CMD_ARGC() >= 2 )
		{
			sprintf( szTemp, "%s %s", (char *)pcmd, (char *)CMD_ARGS() );
		}
		else
		{
			// Just a one word command, use the first word...sigh
			sprintf( szTemp, "%s", (char *)pcmd );
		}
		p = szTemp;
	}

	// remove quotes if present
	if ( *p == '"' )
	{
		p++;
		p[strlen( p ) - 1] = 0;
	}

	// make sure the text has content

	if ( !p || !p[0] || !Q_UnicodeValidate( p ) )
		return; // no character found, so say nothing

	// turn on color set 2  (color on,  no sound)
	// turn on color set 2  (color on,  no sound)
	if ( player->IsObserver() && ( teamonly ) )
		sprintf( text, "%c(SPEC) %s: ", 2, STRING( pEntity->v.netname ) );
	else if ( teamonly )
		sprintf( text, "%c(TEAM) %s: ", 2, STRING( pEntity->v.netname ) );
	else
		sprintf( text, "%c%s: ", 2, STRING( pEntity->v.netname ) );

	j = sizeof( text ) - 2 - strlen( text ); // -2 for /n and null terminator
	if ( (int)strlen( p ) > j )
		p[j] = 0;

	strcat( text, p );
	strcat( text, "\n" );

	player->m_flNextChatTime = gpGlobals->time + CHAT_INTERVAL;

	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	client = NULL;
	while ( ( ( client = (CBasePlayer *)UTIL_FindEntityByClassname( client, "player" ) ) != NULL ) && ( !FNullEnt( client->edict() ) ) )
	{
		if ( !client->pev )
			continue;

		if ( client->edict() == pEntity )
			continue;

		if ( !( client->IsNetClient() ) ) // Not a client ? (should never be true)
			continue;

		// can the receiver hear the sender? or has he muted him?
		if ( g_VoiceGameMgr.PlayerHasBlockedPlayer( client, player ) )
			continue;

		if ( !player->IsObserver() && teamonly && g_pGameRules->PlayerRelationship( client, CBaseEntity::Instance( pEntity ) ) != GR_TEAMMATE )
			continue;

		// Spectators can only talk to other specs
		if ( player->IsObserver() && teamonly )
			if ( !client->IsObserver() )
				continue;

		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, client->pev );
		WRITE_BYTE( ENTINDEX( pEntity ) );
		WRITE_STRING( text );
		MESSAGE_END();
	}

	// print to the sending client
	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, &pEntity->v );
	WRITE_BYTE( ENTINDEX( pEntity ) );
	WRITE_STRING( text );
	MESSAGE_END();

	// echo to server console
	g_engfuncs.pfnServerPrint( text );

	const char *pszSayCmd = teamonly ? "say_team" : "say";

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" %s \"%s\"\n",
		                STRING( pEntity->v.netname ),
		                GETPLAYERUSERID( pEntity ),
		                GETPLAYERAUTHID( pEntity ),
		                g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pEntity ), "model" ),
		                pszSayCmd,
		                p );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" %s \"%s\"\n",
		                STRING( pEntity->v.netname ),
		                GETPLAYERUSERID( pEntity ),
		                GETPLAYERAUTHID( pEntity ),
		                GETPLAYERUSERID( pEntity ),
		                pszSayCmd,
		                p );
	}
}

/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/
// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
void ClientCommand( edict_t *pEntity )
{
	const char *pcmd = CMD_ARGV( 0 );
	const char *pstr;

	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	entvars_t *pev       = &pEntity->v;
	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pev );
	if ( !pPlayer )
		return;

	if ( FStrEq( pcmd, "say" ) )
	{
		Host_Say( pEntity, 0 );
	}
	else if ( FStrEq( pcmd, "say_team" ) )
	{
		Host_Say( pEntity, 1 );
	}
	else if ( FStrEq( pcmd, "trace_log" ) )
	{
		char szResponse[512];
		g_CrashHandler.ExecuteCommand( CMD_ARGC(), NULL, szResponse, sizeof( szResponse ) );
		ClientPrint( pev, HUD_PRINTCONSOLE, szResponse );
	}
	else if ( FStrEq( pcmd, "fullupdate" ) )
	{
		pPlayer->ForceClientDllUpdate();
	}
	else if ( FStrEq( pcmd, "give" ) )
	{
		if ( CVAR_GET_FLOAT( "sv_cheats" ) != 0.0 )
		{
			int iszItem = ALLOC_STRING( CMD_ARGV( 1 ) ); // Make a copy of the classname
			pPlayer->GiveNamedItem( STRING( iszItem ) );
		}
	}
	else if ( FStrEq( pcmd, "drop" ) )
	{
		// player is dropping an item.
		pPlayer->DropPlayerItem( (char *)CMD_ARGV( 1 ) );
	}
	else if ( FStrEq( pcmd, "fov" ) )
	{
		if ( CVAR_GET_FLOAT( "sv_cheats" ) && CMD_ARGC() > 1 )
		{
			pPlayer->m_iFOV = atoi( CMD_ARGV( 1 ) );
		}
		else
		{
			CLIENT_PRINTF( pEntity, print_console, UTIL_VarArgs( "\"fov\" is \"%d\"\n", (int)pPlayer->m_iFOV ) );
		}
	}
	else if ( FStrEq( pcmd, "use" ) )
	{
		pPlayer->SelectItem( (char *)CMD_ARGV( 1 ) );
	}
	else if ( ( ( pstr = strstr( pcmd, "weapon_" ) ) != NULL ) && ( pstr == pcmd ) )
	{
		pPlayer->SelectItem( pcmd );
	}
	else if ( FStrEq( pcmd, "lastinv" ) )
	{
		pPlayer->SelectLastItem();
	}
	else if ( FStrEq( pcmd, "spectate" ) ) // clients wants to become a spectator
	{
		// always allow proxies to become a spectator
		if ( ( pev->flags & FL_PROXY ) || allow_spectators.value )
		{
			edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot( pPlayer );
			pPlayer->StartObserver( pev->origin, VARS( pentSpawnSpot )->angles );

			// notify other clients of player switching to spectator mode
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s switched to spectator mode\n", ( pev->netname && STRING( pev->netname )[0] != 0 ) ? STRING( pev->netname ) : "unconnected" ) );
		}
		else
		{
			ClientPrint( pev, HUD_PRINTCONSOLE, "Spectator mode is disabled.\n" );
		}
	}
	else if ( FStrEq( pcmd, "specmode" ) ) // new spectator mode
	{
		if ( pPlayer->IsObserver() )
			pPlayer->Observer_SetMode( atoi( CMD_ARGV( 1 ) ) );
	}
	else if ( FStrEq( pcmd, "closemenus" ) )
	{
		// just ignore it
	}
	else if ( FStrEq( pcmd, "follownext" ) ) // follow next player
	{
		if ( pPlayer->IsObserver() )
			pPlayer->Observer_FindNextPlayer( atoi( CMD_ARGV( 1 ) ) ? true : false );
	}
	else if ( g_pGameRules->ClientCommand( pPlayer, pcmd ) )
	{
		// MenuSelect returns true only if the command is properly handled,  so don't print a warning
	}
	else
	{
		// tell the user they entered an unknown command
		char command[128];

		// check the length of the command (prevents crash)
		// max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
		strncpy( command, pcmd, 127 );
		command[127] = '\0';
		// First parse the name and remove any %'s
		for ( char *pAmpersand = command; pAmpersand != NULL && *pAmpersand != 0; pAmpersand++ )
		{
			// Replace it with a space
			if ( *pAmpersand == '%' )
				*pAmpersand = ' ';
		}

		// tell the user they entered an unknown command
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs( "Unknown command: %s\n", command ) );
	}
}
