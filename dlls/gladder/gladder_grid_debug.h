/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	In-Game Spatial Grid Real-Time Debug Overlay (SPEC §4.5, GitHub Issue #105)
 *	Server Component & Shared Network Protocol Definitions
 *
 ****/

#pragma once

#ifndef GLADDER_GRID_DEBUG_H
#define GLADDER_GRID_DEBUG_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "gladder_grid_indexer.h"
#include <vector>
#include <cstdint>
#include <cstddef>

// Protocol Opcodes for GladGrid user message
#define GLADDER_GRID_DBG_OP_CLEAR       0
#define GLADDER_GRID_DBG_OP_CHUNK       1
#define GLADDER_GRID_DBG_OP_BATCH_BEGIN 2
#define GLADDER_GRID_DBG_OP_BATCH_END   3

// Maximum cells sent per packet to ensure netchannel safe headroom (< 192 bytes)
#define GLADDER_GRID_DBG_MAX_CELLS_PER_CHUNK 16

// Debug cell representation
struct GladderDebugCell
{
	Vector origin;
	Vector normal;
	int32_t areaId;
	uint32_t flags;
	bool isOccupied;

	GladderDebugCell()
	    : origin( g_vecZero ),
	      normal( Vector( 0.0f, 0.0f, 1.0f ) ),
	      areaId( 0 ),
	      flags( 0 ),
	      isOccupied( false )
	{
	}

	GladderDebugCell( const Vector &pos, int32_t aId, uint32_t f, bool occ = false )
	    : origin( pos ),
	      normal( Vector( 0.0f, 0.0f, 1.0f ) ),
	      areaId( aId ),
	      flags( f ),
	      isOccupied( occ )
	{
	}

	GladderDebugCell( const Vector &pos, const Vector &norm, int32_t aId, uint32_t f, bool occ = false )
	    : origin( pos ),
	      normal( norm ),
	      areaId( aId ),
	      flags( f ),
	      isOccupied( occ )
	{
	}
};

// Pure logic: filter cells within radius around center position
std::vector<GladderDebugCell> GladderGridDebug_FilterCells( const GladderGridIndexer &indexer,
                                                          const Vector &playerPos,
                                                          float flRadius,
                                                          const std::vector<bool> *pOccupied = nullptr );

// Server lifecycle & streaming controls
bool GladderGridDebug_IsServerActive( void );
void GladderGridDebug_SetServerActive( bool bActive );
bool GladderGridDebug_ToggleServer( void );
float GladderGridDebug_GetRadius( void );
void GladderGridDebug_SetRadius( float flRadius );
void GladderGridDebug_ServerReset( void );

// Handles 'gladder_grid_debug [0|1]' command received from client
bool GladderGridDebug_HandleClientCommand( CBasePlayer *pPlayer, const char *pcmd );

// Per-frame / rate-limited streaming tick
void GladderGridDebug_ServerThink( const GladderGridIndexer &indexer, CBaseEntity *pPlayer );

#endif // GLADDER_GRID_DEBUG_H
