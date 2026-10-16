/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	In-Game Spatial Grid Real-Time Debug Overlay (SPEC §4.5, GitHub Issue #105)
 *	Client-Side Rendering, State Management & HUD Readout Implementation
 *
 ****/

#include "gladder_grid_debug_client.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

#if defined( CLIENT_DLL )
#include "hud.h"
#include "cl_util.h"
#include "triangleapi.h"
#include "render/triangle_render_registry.h"
#include "r_studioint.h"
#include "com_model.h"

extern engine_studio_api_t IEngineStudio;
static cvar_t *s_pCvarDebugRadius = nullptr;
#endif

static bool s_bClientGridDebugActive = false;
static bool s_bInBatch = false;
static std::vector<GladderClientDebugCell> s_clientDebugCells;
static std::vector<GladderClientDebugCell> s_incomingDebugCells;

bool GladderGridDebug_IsClientActive( void )
{
	return s_bClientGridDebugActive;
}

void GladderGridDebug_SetClientActive( bool bActive )
{
	s_bClientGridDebugActive = bActive;
	if ( !s_bClientGridDebugActive )
	{
		GladderGridDebug_ClientReset();
	}
}

bool GladderGridDebug_ToggleClient( void )
{
	GladderGridDebug_SetClientActive( !s_bClientGridDebugActive );
	return s_bClientGridDebugActive;
}

void GladderGridDebug_ClientReset( void )
{
	s_bInBatch = false;
	s_incomingDebugCells.clear();
	s_incomingDebugCells.shrink_to_fit();
	s_clientDebugCells.clear();
	s_clientDebugCells.shrink_to_fit();
}

size_t GladderGridDebug_GetClientCellCount( void )
{
	return s_clientDebugCells.size();
}

const std::vector<GladderClientDebugCell> &GladderGridDebug_GetClientCells( void )
{
	return s_clientDebugCells;
}

static inline int16_t ReadInt16( const uint8_t *p )
{
	return static_cast<int16_t>( p[0] | ( p[1] << 8 ) );
}

static inline float ReadCoord( const uint8_t *p )
{
	int16_t raw = ReadInt16( p );
	return static_cast<float>( raw ) * 0.125f;
}

bool GladderGridDebug_ParseUserMessage( const void *pbuf, int iSize )
{
	if ( !pbuf || iSize < 1 )
		return false;

	const uint8_t *p = static_cast<const uint8_t *>( pbuf );
	uint8_t opcode = p[0];

	if ( opcode == GLADDER_GRID_DBG_OP_CLEAR )
	{
		s_bInBatch = false;
		s_incomingDebugCells.clear();
		s_incomingDebugCells.shrink_to_fit();
		s_clientDebugCells.clear();
		s_clientDebugCells.shrink_to_fit();
		return true;
	}

	if ( opcode == GLADDER_GRID_DBG_OP_BATCH_BEGIN )
	{
		s_bInBatch = true;
		s_incomingDebugCells.clear();
		return true;
	}

	if ( opcode == GLADDER_GRID_DBG_OP_BATCH_END )
	{
		s_bInBatch = false;
		s_clientDebugCells = std::move( s_incomingDebugCells );
		return true;
	}

	if ( opcode == GLADDER_GRID_DBG_OP_CHUNK )
	{
		if ( iSize < 2 )
			return false;

		uint8_t chunkCount = p[1];
		if ( chunkCount == 0 )
			return true;

		bool bHasNormal = ( iSize >= 2 + static_cast<int>( chunkCount ) * 11 );
		int bytesPerCell = bHasNormal ? 11 : 9;
		int requiredSize = 2 + static_cast<int>( chunkCount ) * bytesPerCell;
		if ( iSize < requiredSize )
			return false;

		int offset = 2;
		std::vector<GladderClientDebugCell> &targetVec = s_bInBatch ? s_incomingDebugCells : s_clientDebugCells;

		for ( uint8_t i = 0; i < chunkCount; ++i )
		{
			float x = ReadCoord( p + offset );
			float y = ReadCoord( p + offset + 2 );
			float z = ReadCoord( p + offset + 4 );
			float nx = 0.0f, ny = 0.0f, nz = 1.0f;

			if ( bHasNormal )
			{
				int8_t rawNx = static_cast<int8_t>( p[offset + 6] );
				int8_t rawNy = static_cast<int8_t>( p[offset + 7] );
				nx = static_cast<float>( rawNx ) / 127.0f;
				ny = static_cast<float>( rawNy ) / 127.0f;
				float nzSq = 1.0f - ( nx * nx + ny * ny );
				nz = ( nzSq > 0.001f ) ? std::sqrt( nzSq ) : 0.0f;
				if ( nz < 0.1f )
					nz = 1.0f;

				int8_t areaId = static_cast<int8_t>( p[offset + 8] );
				uint8_t flags = p[offset + 9];
				bool occ = ( p[offset + 10] != 0 );
				offset += 11;

				targetVec.push_back( GladderClientDebugCell( x, y, z, areaId, flags, occ, nx, ny, nz ) );
			}
			else
			{
				int8_t areaId = static_cast<int8_t>( p[offset + 6] );
				uint8_t flags = p[offset + 7];
				bool occ = ( p[offset + 8] != 0 );
				offset += 9;

				targetVec.push_back( GladderClientDebugCell( x, y, z, areaId, flags, occ, 0.0f, 0.0f, 1.0f ) );
			}
		}
		return true;
	}

	return false;
}

void GladderDrawSpatialGridOverlay( void )
{
#if defined( CLIENT_DLL )
	if ( !s_bClientGridDebugActive || s_clientDebugCells.empty() )
		return;

	if ( !gEngfuncs.pTriAPI )
		return;

	// Bind a solid sprite / texture so the triangle pipeline doesn't inherit leftover alpha/color textures
	static struct model_s *s_pWhiteSprite = nullptr;
	if ( !s_pWhiteSprite && gEngfuncs.pfnSPR_Load && gEngfuncs.GetSpritePointer )
	{
		HSPRITE hSpr = gEngfuncs.pfnSPR_Load( "sprites/laserbeam.spr" );
		if ( !hSpr )
			hSpr = gEngfuncs.pfnSPR_Load( "sprites/white.spr" );
		if ( hSpr )
			s_pWhiteSprite = (struct model_s *)gEngfuncs.GetSpritePointer( hSpr );
	}
	if ( !s_pWhiteSprite && IEngineStudio.GetChromeSprite )
	{
		s_pWhiteSprite = (struct model_s *)IEngineStudio.GetChromeSprite();
	}
	if ( s_pWhiteSprite )
	{
		gEngfuncs.pTriAPI->SpriteTexture( s_pWhiteSprite, 0 );
	}

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );

	const float halfSize = 14.5f;   // 29x29 quad in 32x32 grid leaves a 3-unit gutter so adjacent cells never collide or share edges
	const float flQuadElev = 4.0f; // 4.0 units strictly above floor along surface normal
	const float flLineElev = 4.4f; // 4.4 units strictly above floor along normal (0.4 units above quad to eliminate depth conflict)

	for ( const auto &cell : s_clientDebugCells )
	{
		float r = 0.2f, g = 1.0f, b = 0.2f, a = 0.35f;

		if ( cell.isOccupied )
		{
			// Occupied by monster/entity: Yellow
			r = 1.0f; g = 0.9f; b = 0.1f; a = 0.45f;
		}
		else if ( !( cell.flags & GLADDER_CELL_FLAG_CLEARANCE_OK ) )
		{
			// Clearance rejected / blocked: Red / Orange
			r = 1.0f; g = 0.2f; b = 0.1f; a = 0.35f;
		}
		else if ( cell.flags & GLADDER_CELL_FLAG_LARGE_CLEARANCE )
		{
			// Large hull clearance confirmed: Cyan
			r = 0.1f; g = 0.9f; b = 1.0f; a = 0.35f;
		}

		// Area tint modulation: slightly alter border/color based on areaId
		if ( cell.areaId > 0 )
		{
			float hueShift = static_cast<float>( ( cell.areaId % 5 ) ) * 0.08f;
			b = (std::min)( 1.0f, b + hueShift );
		}

		float nx = cell.nx;
		float ny = cell.ny;
		float nz = cell.nz;
		if ( nz < 0.1f || nz > 1.01f )
		{
			nx = 0.0f;
			ny = 0.0f;
			nz = 1.0f;
		}

		float qx[4], qy[4], qz[4];
		float lx[4], ly[4], lz[4];

		for ( int k = 0; k < 4; ++k )
		{
			float dx = ( k == 0 || k == 3 ) ? -halfSize : halfSize;
			float dy = ( k == 0 || k == 1 ) ? -halfSize : halfSize;
			float dz = -( nx * dx + ny * dy ) / nz;

			float sx = cell.x + dx;
			float sy = cell.y + dy;
			float sz = cell.z + dz;

			qx[k] = sx + nx * flQuadElev;
			qy[k] = sy + ny * flQuadElev;
			qz[k] = sz + nz * flQuadElev;

			lx[k] = sx + nx * flLineElev;
			ly[k] = sy + ny * flLineElev;
			lz[k] = sz + nz * flLineElev;
		}

		// Translucent floor quad
		gEngfuncs.pTriAPI->Color4f( r, g, b, a );
		gEngfuncs.pTriAPI->CullFace( TRI_NONE );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		for ( int k = 0; k < 4; ++k )
		{
			gEngfuncs.pTriAPI->Brightness( 1.0f );
			gEngfuncs.pTriAPI->TexCoord2f( 0.5f, 0.5f );
			gEngfuncs.pTriAPI->Vertex3f( qx[k], qy[k], qz[k] );
		}
		gEngfuncs.pTriAPI->End();

		// Vibrant wire border
		gEngfuncs.pTriAPI->Color4f( (std::min)( 1.0f, r * 1.3f ),
		                            (std::min)( 1.0f, g * 1.3f ),
		                            (std::min)( 1.0f, b * 1.3f ),
		                            0.85f );
		gEngfuncs.pTriAPI->CullFace( TRI_NONE );
		gEngfuncs.pTriAPI->Begin( TRI_LINES );
		for ( int k = 0; k < 4; ++k )
		{
			int nextK = ( k + 1 ) % 4;
			gEngfuncs.pTriAPI->Brightness( 1.0f );
			gEngfuncs.pTriAPI->TexCoord2f( 0.5f, 0.5f );
			gEngfuncs.pTriAPI->Vertex3f( lx[k], ly[k], lz[k] );

			gEngfuncs.pTriAPI->Brightness( 1.0f );
			gEngfuncs.pTriAPI->TexCoord2f( 0.5f, 0.5f );
			gEngfuncs.pTriAPI->Vertex3f( lx[nextK], ly[nextK], lz[nextK] );
		}
		gEngfuncs.pTriAPI->End();
	}

	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
#endif
}

#if defined( CLIENT_DLL )
REGISTER_TRANSPARENT_TRIANGLE_RENDERER( GladderDrawSpatialGridOverlay );

static int MsgFunc_GladGrid_Dispatcher( const char *pszName, int iSize, void *pbuf )
{
	return g_HudGladderGridDebug.MsgFunc_GladderGridDebug( pszName, iSize, pbuf );
}
#endif

CHudGladderGridDebug g_HudGladderGridDebug;

CHudGladderGridDebug::CHudGladderGridDebug()
{
	m_iFlags = HUD_ACTIVE;
}

int CHudGladderGridDebug::Init( void )
{
#if defined( CLIENT_DLL )
	gEngfuncs.pfnHookUserMsg( "GladGrid", MsgFunc_GladGrid_Dispatcher );
	gEngfuncs.pfnAddCommand( "gladder_grid_debug", Cmd_ToggleGridDebug );
	s_pCvarDebugRadius = CVAR_CREATE( "gladder_grid_debug_radius", "512", FCVAR_ARCHIVE );
#endif
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderGridDebug::VidInit( void )
{
	Reset();
	return 1;
}

void CHudGladderGridDebug::Reset( void )
{
	GladderGridDebug_SetClientActive( false );
}

void CHudGladderGridDebug::InitHUDData( void )
{
	GladderGridDebug_SetClientActive( false );
}

int CHudGladderGridDebug::MsgFunc_GladderGridDebug( const char *pszName, int iSize, void *pbuf )
{
	if ( GladderGridDebug_ParseUserMessage( pbuf, iSize ) )
	{
		return 1;
	}
	return 0;
}

void CHudGladderGridDebug::Cmd_ToggleGridDebug( void )
{
#if defined( CLIENT_DLL )
	int argc = gEngfuncs.Cmd_Argc();
	bool bTarget = !s_bClientGridDebugActive;

	if ( argc >= 2 )
	{
		const char *pArg = gEngfuncs.Cmd_Argv( 1 );
		if ( pArg && pArg[0] == '1' )
		{
			bTarget = true;
		}
		else if ( pArg && pArg[0] == '0' )
		{
			bTarget = false;
		}
	}

	GladderGridDebug_SetClientActive( bTarget );

	if ( bTarget )
	{
		gEngfuncs.pfnServerCmd( "gladder_grid_debug 1\n" );
		gEngfuncs.Con_Printf( "[Gladder] Spatial grid debug overlay: ON\n" );
	}
	else
	{
		gEngfuncs.pfnServerCmd( "gladder_grid_debug 0\n" );
		gEngfuncs.Con_Printf( "[Gladder] Spatial grid debug overlay: OFF\n" );
	}
#else
	GladderGridDebug_ToggleClient();
#endif
}

int CHudGladderGridDebug::Draw( float flTime )
{
#if defined( CLIENT_DLL )
	if ( !s_bClientGridDebugActive )
		return 1;

	cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();
	float px = 0.0f, py = 0.0f, pz = 0.0f;
	if ( pPlayer )
	{
		px = pPlayer->origin[0];
		py = pPlayer->origin[1];
		pz = pPlayer->origin[2];
	}

	const GladderClientDebugCell *pUnderCell = nullptr;
	float bestDistSq = 999999.0f;

	for ( const auto &cell : s_clientDebugCells )
	{
		float dx = cell.x - px;
		float dy = cell.y - py;
		float dz = cell.z - pz;
		float distSq = dx * dx + dy * dy;

		if ( fabsf( dz ) <= 64.0f && distSq < bestDistSq && distSq <= ( 48.0f * 48.0f ) )
		{
			bestDistSq = distSq;
			pUnderCell = &cell;
		}
	}

	int x = 20;
	int y = 90;
	char szBuf[160];

	snprintf( szBuf, sizeof( szBuf ), "SPATIAL GRID DEBUG: Active (%u cells cached)",
	          static_cast<unsigned int>( s_clientDebugCells.size() ) );
	gEngfuncs.pfnDrawSetTextColor( 0.2f, 1.0f, 0.4f );
	DrawConsoleString( x, y, szBuf );
	y += 14;

	if ( pUnderCell )
	{
		const char *pszStatus = "OK";
		if ( pUnderCell->isOccupied )
		{
			pszStatus = "OCCUPIED";
		}
		else if ( pUnderCell->flags & GLADDER_CELL_FLAG_CEILING_VALID )
		{
			pszStatus = "CEILING";
		}
		else if ( !( pUnderCell->flags & GLADDER_CELL_FLAG_CLEARANCE_OK ) )
		{
			pszStatus = "REJECTED";
		}

		snprintf( szBuf, sizeof( szBuf ), "Cell (%.0f, %.0f, %.0f) | Area: %d | Flags: 0x%02X [%s]",
		          pUnderCell->x, pUnderCell->y, pUnderCell->z,
		          pUnderCell->areaId, pUnderCell->flags, pszStatus );
		gEngfuncs.pfnDrawSetTextColor( 1.0f, 1.0f, 0.2f );
		DrawConsoleString( x, y, szBuf );
	}
	else
	{
		snprintf( szBuf, sizeof( szBuf ), "Player (%.0f, %.0f, %.0f) | No cell beneath",
		          px, py, pz );
		gEngfuncs.pfnDrawSetTextColor( 0.75f, 0.75f, 0.75f );
		DrawConsoleString( x, y, szBuf );
	}
#endif
	return 1;
}

#if defined( CLIENT_DLL )
REGISTER_HUD_ELEMENT_NAMED( g_HudGladderGridDebug, 2700, "GladderGridDebug" );
#endif
