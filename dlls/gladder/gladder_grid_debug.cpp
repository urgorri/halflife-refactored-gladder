/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	In-Game Spatial Grid Real-Time Debug Overlay (SPEC §4.5, GitHub Issue #105)
 *	Server Implementation & Rate-Limited Network Streaming
 *
 ****/

#include "gladder_grid_debug.h"
#include "gladder_usermsg.h"
#include "core/player.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>

static bool s_bServerGridDebugActive = false;
static float s_flDebugRadius         = 512.0f;
static Vector s_vecLastUpdatePos     = Vector( 99999.0f, 99999.0f, 99999.0f );
static float s_flLastUpdateTime      = 0.0f;

std::vector<GladderDebugCell> GladderGridDebug_FilterCells( const GladderGridIndexer &indexer,
                                                          const Vector &playerPos,
                                                          float flRadius,
                                                          const std::vector<bool> *pOccupied )
{
	std::vector<GladderDebugCell> result;
	if ( indexer.GetCellCount() == 0 || flRadius <= 0.0f )
		return result;

	float radiusSq = flRadius * flRadius;
	result.reserve( 128 );

	for ( size_t i = 0; i < indexer.GetCellCount(); ++i )
	{
		const auto &c = indexer.GetCell( i );
		float dx = c.origin.x - playerPos.x;
		float dy = c.origin.y - playerPos.y;
		float dz = c.origin.z - playerPos.z;
		float distSq = dx * dx + dy * dy + dz * dz;

		if ( distSq <= radiusSq )
		{
			bool occ = ( pOccupied && i < pOccupied->size() ) ? ( *pOccupied )[i] : false;
			result.emplace_back( c.origin, c.areaId, c.flags, occ );
		}
	}

	return result;
}

bool GladderGridDebug_IsServerActive( void )
{
	return s_bServerGridDebugActive;
}

void GladderGridDebug_SetServerActive( bool bActive )
{
	s_bServerGridDebugActive = bActive;
	if ( !s_bServerGridDebugActive )
	{
		s_vecLastUpdatePos = Vector( 99999.0f, 99999.0f, 99999.0f );
		s_flLastUpdateTime = 0.0f;
	}
}

bool GladderGridDebug_ToggleServer( void )
{
	GladderGridDebug_SetServerActive( !s_bServerGridDebugActive );
	return s_bServerGridDebugActive;
}

float GladderGridDebug_GetRadius( void )
{
	return s_flDebugRadius;
}

void GladderGridDebug_SetRadius( float flRadius )
{
	s_flDebugRadius = (std::max)( 128.0f, (std::min)( 1024.0f, flRadius ) );
}

void GladderGridDebug_ServerReset( void )
{
	s_bServerGridDebugActive = false;
	s_flDebugRadius         = 512.0f;
	s_vecLastUpdatePos     = Vector( 99999.0f, 99999.0f, 99999.0f );
	s_flLastUpdateTime      = 0.0f;
}

bool GladderGridDebug_HandleClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if ( !pcmd )
		return false;

	if ( strncmp( pcmd, "gladder_grid_debug", 18 ) != 0 )
		return false;

	const char *pArg = pcmd + 18;
	while ( *pArg == ' ' || *pArg == '\t' )
	{
		pArg++;
	}

	if ( *pArg == '1' )
	{
		GladderGridDebug_SetServerActive( true );
	}
	else if ( *pArg == '0' )
	{
		GladderGridDebug_SetServerActive( false );
		if ( pPlayer && pPlayer->pev && gmsgGladderGridDebug )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgGladderGridDebug, nullptr, pPlayer->pev );
			WRITE_BYTE( GLADDER_GRID_DBG_OP_CLEAR );
			MESSAGE_END();
		}
	}
	else
	{
		bool bNow = GladderGridDebug_ToggleServer();
		if ( !bNow && pPlayer && pPlayer->pev && gmsgGladderGridDebug )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgGladderGridDebug, nullptr, pPlayer->pev );
			WRITE_BYTE( GLADDER_GRID_DBG_OP_CLEAR );
			MESSAGE_END();
		}
	}

	ALERT( at_console, "[Gladder] Spatial grid debug overlay: %s\n",
	       s_bServerGridDebugActive ? "ON" : "OFF" );
	return true;
}

void GladderGridDebug_ServerThink( const GladderGridIndexer &indexer, CBaseEntity *pPlayer )
{
	if ( !s_bServerGridDebugActive || !pPlayer || !pPlayer->pev )
		return;

	if ( !gmsgGladderGridDebug )
		return;

	if ( indexer.GetCellCount() == 0 )
		return;

	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	Vector playerPos = pPlayer->pev->origin;

	// Rate-limited updates: resend only when moving > 32 units or after 0.25 seconds
	float distMoved = ( playerPos - s_vecLastUpdatePos ).Length();
	if ( distMoved < 32.0f && ( flTime - s_flLastUpdateTime ) < 0.25f )
	{
		return;
	}

	s_vecLastUpdatePos = playerPos;
	s_flLastUpdateTime = flTime;

	std::vector<GladderDebugCell> cells = GladderGridDebug_FilterCells( indexer, playerPos, s_flDebugRadius );

	// Send clear opcode first to discard previous window
	MESSAGE_BEGIN( MSG_ONE, gmsgGladderGridDebug, nullptr, pPlayer->pev );
	WRITE_BYTE( GLADDER_GRID_DBG_OP_CLEAR );
	MESSAGE_END();

	if ( cells.empty() )
		return;

	// Stream cells in chunks of up to GLADDER_GRID_DBG_MAX_CELLS_PER_CHUNK
	size_t total = cells.size();
	size_t offset = 0;

	while ( offset < total )
	{
		size_t chunkCount = (std::min)( static_cast<size_t>( GLADDER_GRID_DBG_MAX_CELLS_PER_CHUNK ), total - offset );

		MESSAGE_BEGIN( MSG_ONE, gmsgGladderGridDebug, nullptr, pPlayer->pev );
		WRITE_BYTE( GLADDER_GRID_DBG_OP_CHUNK );
		WRITE_BYTE( static_cast<int>( chunkCount ) );

		for ( size_t i = 0; i < chunkCount; ++i )
		{
			const auto &cell = cells[offset + i];
			WRITE_COORD( cell.origin.x );
			WRITE_COORD( cell.origin.y );
			WRITE_COORD( cell.origin.z );
			WRITE_BYTE( static_cast<int>( cell.areaId ) );
			WRITE_BYTE( static_cast<int>( cell.flags ) );
			WRITE_BYTE( cell.isOccupied ? 1 : 0 );
		}

		MESSAGE_END();
		offset += chunkCount;
	}
}
