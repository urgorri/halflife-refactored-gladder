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
#include "hud/hud_base.h"
#include "hud/hud_registry.h"

#define GLADDER_GRID_DBG_OP_CLEAR 0
#define GLADDER_GRID_DBG_OP_CHUNK 1

#ifndef GLADDER_CELL_FLAG_CEILING
#define GLADDER_CELL_FLAG_CEILING 0x01
#endif
#ifndef GLADDER_CELL_FLAG_CLEARANCE_OK
#define GLADDER_CELL_FLAG_CLEARANCE_OK 0x02
#endif

// Client-side cached cell structure (C++11 compliant, explicit constructors)
struct GladderClientDebugCell
{
	float x, y, z;
	int32_t areaId;
	uint32_t flags;
	bool isOccupied;

	GladderClientDebugCell()
	    : x( 0.0f ), y( 0.0f ), z( 0.0f ),
	      areaId( 0 ),
	      flags( 0 ),
	      isOccupied( false )
	{
	}

	GladderClientDebugCell( float _x, float _y, float _z, int32_t aId, uint32_t fl, bool occ )
	    : x( _x ), y( _y ), z( _z ),
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
