/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	In-Game Spatial Grid Real-Time Debug Overlay (SPEC §4.5, GitHub Issue #105)
 *	Client-Side Rendering, State Management & HUD Readout
 *
 ****/

#pragma once

#ifndef GLADDER_GRID_DEBUG_CLIENT_H
#define GLADDER_GRID_DEBUG_CLIENT_H

#include <vector>
#include <cstdint>
#include <cstddef>
#include "hud/hud_base.h"
#include "hud/hud_registry.h"

#define GLADDER_GRID_DBG_OP_CLEAR       0
#define GLADDER_GRID_DBG_OP_CHUNK       1
#define GLADDER_GRID_DBG_OP_BATCH_BEGIN 2
#define GLADDER_GRID_DBG_OP_BATCH_END   3

#ifndef GLADDER_CELL_FLAG_VALID
#define GLADDER_CELL_FLAG_VALID          0x01
#endif
#ifndef GLADDER_CELL_FLAG_CLEARANCE_OK
#define GLADDER_CELL_FLAG_CLEARANCE_OK   0x02
#endif
#ifndef GLADDER_CELL_FLAG_CEILING_VALID
#define GLADDER_CELL_FLAG_CEILING_VALID  0x04
#endif
#ifndef GLADDER_CELL_FLAG_NO_MONSTERS
#define GLADDER_CELL_FLAG_NO_MONSTERS    0x08
#endif
#ifndef GLADDER_CELL_FLAG_PICKUP_ONLY
#define GLADDER_CELL_FLAG_PICKUP_ONLY    0x10
#endif
#ifndef GLADDER_CELL_FLAG_LARGE_CLEARANCE
#define GLADDER_CELL_FLAG_LARGE_CLEARANCE 0x20
#endif
#ifndef GLADDER_CELL_FLAG_FLOATING_BASE
#define GLADDER_CELL_FLAG_FLOATING_BASE  0x40
#endif

// Client-side cached cell structure (C++11 compliant, explicit constructors)
struct GladderClientDebugCell
{
	float x, y, z;
	float nx, ny, nz;
	int32_t areaId;
	uint32_t flags;
	bool isOccupied;

	GladderClientDebugCell()
	    : x( 0.0f ), y( 0.0f ), z( 0.0f ),
	      nx( 0.0f ), ny( 0.0f ), nz( 1.0f ),
	      areaId( 0 ),
	      flags( 0 ),
	      isOccupied( false )
	{
	}

	GladderClientDebugCell( float _x, float _y, float _z, int32_t aId, uint32_t fl, bool occ,
	                        float _nx = 0.0f, float _ny = 0.0f, float _nz = 1.0f )
	    : x( _x ), y( _y ), z( _z ),
	      nx( _nx ), ny( _ny ), nz( _nz ),
	      areaId( aId ),
	      flags( fl ),
	      isOccupied( occ )
	{
	}
};

// Client state controls & accessors
bool GladderGridDebug_IsClientActive( void );
void GladderGridDebug_SetClientActive( bool bActive );
bool GladderGridDebug_ToggleClient( void );
void GladderGridDebug_ClientReset( void );
size_t GladderGridDebug_GetClientCellCount( void );
const std::vector<GladderClientDebugCell> &GladderGridDebug_GetClientCells( void );

// Safe message parser for GladGrid user messages
bool GladderGridDebug_ParseUserMessage( const void *pbuf, int iSize );

// Triangle API drawing callback
void GladderDrawSpatialGridOverlay( void );

// HUD overlay readout element
class CHudGladderGridDebug : public CHudBase
{
  public:
	CHudGladderGridDebug();
	int Init( void ) override;
	int VidInit( void ) override;
	int Draw( float flTime ) override;
	void Reset( void ) override;
	void InitHUDData( void ) override;

	// User message dispatcher
	int MsgFunc_GladderGridDebug( const char *pszName, int iSize, void *pbuf );

	// Command dispatcher
	static void Cmd_ToggleGridDebug( void );
};

extern CHudGladderGridDebug g_HudGladderGridDebug;

#endif // GLADDER_GRID_DEBUG_CLIENT_H
