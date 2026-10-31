/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD: Sky-Drop Tactical Overview & Deployment Overlay Implementation (SPEC §2, §12.1, Issue #141)
 *
 ****/

#include "hud_gladder_skydrop.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef CLIENT_DLL
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "com_model.h"

static int MsgFunc_GladDeploy_Dispatcher( const char *pszName, int iSize, void *pbuf )
{
	return g_HudGladderSkyDrop.MsgFunc_GladderDeploy( pszName, iSize, pbuf );
}
#endif

CHudGladderSkyDrop g_HudGladderSkyDrop;

REGISTER_HUD_ELEMENT_NAMED( g_HudGladderSkyDrop, 2800, "GladderSkyDrop" );

bool GladderOverviewProjector::ParseFromBuffer( const char *pBuffer )
{
	if ( !pBuffer || !*pBuffer )
		return false;

	Reset();
	const char *p = pBuffer;
	char token[256];

	auto getNextToken = []( const char *&src, char *dst, size_t dstSize ) -> bool {
		while ( *src && ( *src == ' ' || *src == '\t' || *src == '\r' || *src == '\n' ) )
			src++;
		if ( !*src )
			return false;
		if ( *src == '{' || *src == '}' )
		{
			dst[0] = *src++;
			dst[1] = '\0';
			return true;
		}
		size_t len = 0;
		while ( *src && *src != ' ' && *src != '\t' && *src != '\r' && *src != '\n' && *src != '{' && *src != '}' && len + 1 < dstSize )
		{
			dst[len++] = *src++;
		}
		dst[len] = '\0';
		return ( len > 0 );
	};

	while ( getNextToken( p, token, sizeof( token ) ) )
	{
		if ( !strcmp( token, "global" ) )
		{
			if ( !getNextToken( p, token, sizeof( token ) ) || strcmp( token, "{" ) )
				continue;

			while ( getNextToken( p, token, sizeof( token ) ) && strcmp( token, "}" ) )
			{
				if ( !strcmp( token, "zoom" ) )
				{
					if ( getNextToken( p, token, sizeof( token ) ) )
						m_zoom = static_cast<float>( atof( token ) );
				}
				else if ( !strcmp( token, "origin" ) )
				{
					if ( getNextToken( p, token, sizeof( token ) ) )
						m_originX = static_cast<float>( atof( token ) );
					if ( getNextToken( p, token, sizeof( token ) ) )
						m_originY = static_cast<float>( atof( token ) );
					if ( getNextToken( p, token, sizeof( token ) ) )
						m_originZ = static_cast<float>( atof( token ) );
				}
				else if ( !strcmp( token, "rotated" ) )
				{
					if ( getNextToken( p, token, sizeof( token ) ) )
						m_rotated = atoi( token );
				}
			}
			m_bLoaded = true;
			m_bLoadedFromFile = true;
		}
		else if ( !strcmp( token, "layer" ) )
		{
			if ( !getNextToken( p, token, sizeof( token ) ) || strcmp( token, "{" ) )
				continue;

			while ( getNextToken( p, token, sizeof( token ) ) && strcmp( token, "}" ) )
			{
				if ( !strcmp( token, "image" ) )
				{
					if ( getNextToken( p, token, sizeof( token ) ) )
					{
						char *clean = token;
						if ( clean[0] == '"' )
						{
							clean++;
							size_t clen = strlen( clean );
							if ( clen > 0 && clean[clen - 1] == '"' )
								clean[clen - 1] = '\0';
						}
						m_layerImage = clean;
					}
				}
			}
		}
	}

	if ( m_zoom <= 0.001f )
		m_zoom = 1.0f;

	return m_bLoaded;
}

void GladderOverviewProjector::AutoFitBounds( float dropX, float dropY, const std::vector<GladderGateCoord> &gates )
{
	float minX = dropX;
	float maxX = dropX;
	float minY = dropY;
	float maxY = dropY;

	for ( size_t i = 0; i < gates.size(); ++i )
	{
		if ( gates[i].x < minX ) minX = gates[i].x;
		if ( gates[i].x > maxX ) maxX = gates[i].x;
		if ( gates[i].y < minY ) minY = gates[i].y;
		if ( gates[i].y > maxY ) maxY = gates[i].y;
	}

	float spanX = maxX - minX;
	float spanY = maxY - minY;
	float maxSpan = ( spanX > spanY ) ? spanX : spanY;
	if ( maxSpan < 800.0f )
		maxSpan = 800.0f;

	m_originX = ( minX + maxX ) * 0.5f;
	m_originY = ( minY + maxY ) * 0.5f;
	m_zoom    = 4096.0f / ( maxSpan * 1.4f );
	if ( m_zoom <= 0.05f )
		m_zoom = 0.05f;
	if ( m_zoom > 4.0f )
		m_zoom = 4.0f;
	m_rotated = 0;
	m_bLoaded = true;
}

bool GladderOverviewProjector::LoadOverview( const char *pszMapName )
{
	Reset();
	if ( !pszMapName || !*pszMapName )
		return false;

	char mapClean[64];
	strncpy( mapClean, pszMapName, sizeof( mapClean ) - 1 );
	mapClean[sizeof( mapClean ) - 1] = '\0';

	// Strip directory prefixes if present
	char *pBase = strrchr( mapClean, '/' );
	if ( !pBase )
		pBase = strrchr( mapClean, '\\' );
	if ( pBase )
		pBase++;
	else
		pBase = mapClean;

	// Strip .bsp
	char *pDot = strrchr( pBase, '.' );
	if ( pDot )
		*pDot = '\0';

#ifdef CLIENT_DLL
	char filename[128];
	snprintf( filename, sizeof( filename ), "overviews/%s.txt", pBase );
	char *pfile = (char *)gEngfuncs.COM_LoadFile( filename, 5, NULL );
	if ( pfile )
	{
		bool bOk = ParseFromBuffer( pfile );
		gEngfuncs.COM_FreeFile( pfile );
		if ( bOk && !m_layerImage.empty() )
		{
			m_pMapSprite = gEngfuncs.LoadMapSprite( m_layerImage.c_str() );
		}
		return bOk;
	}
#endif

	// Fallback default: origin 0,0, zoom 1.0
	m_originX = 0.0f;
	m_originY = 0.0f;
	m_zoom    = 1.0f;
	m_rotated = 0;
	m_bLoaded = true;
	m_bLoadedFromFile = false;
	return true;
}

bool GladderOverviewProjector::WorldToScreen( float worldX, float worldY, int boxCenterX, int boxCenterY, int boxSide, int &outX, int &outY ) const
{
	float mapWidthWorld = 4096.0f / ( m_zoom > 0.001f ? m_zoom : 1.0f );
	float mapHeightWorld = mapWidthWorld;
	float dx = worldX - m_originX;
	float dy = worldY - m_originY;

	float nx = 0.5f;
	float ny = 0.5f;

	if ( m_rotated != 0 )
	{
		nx = 0.5f + ( dy / mapWidthWorld );
		ny = 0.5f - ( dx / mapHeightWorld );
	}
	else
	{
		nx = 0.5f + ( dx / mapWidthWorld );
		ny = 0.5f - ( dy / mapHeightWorld );
	}

	nx = (std::max)( 0.0f, (std::min)( 1.0f, nx ) );
	ny = (std::max)( 0.0f, (std::min)( 1.0f, ny ) );

	int halfSide = boxSide / 2;
	outX = ( boxCenterX - halfSide ) + static_cast<int>( nx * boxSide );
	outY = ( boxCenterY - halfSide ) + static_cast<int>( ny * boxSide );
	return true;
}

CHudGladderSkyDrop::CHudGladderSkyDrop()
    : m_bActive( false ),
      m_flStartTime( -100.0f )
{
	m_iFlags = HUD_ACTIVE;
}

int CHudGladderSkyDrop::Init( void )
{
#ifdef CLIENT_DLL
	gEngfuncs.pfnHookUserMsg( "GladDeploy", MsgFunc_GladDeploy_Dispatcher );
#endif
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderSkyDrop::VidInit( void )
{
	float curTime = 0.0f;
#ifdef CLIENT_DLL
	curTime = gHUD.m_flTime;
#endif
	float flDuration = ( m_deployMsg.flBriefingDuration > 0.5f ) ? m_deployMsg.flBriefingDuration : 2.8f;
	if ( !m_bActive || ( curTime - m_flStartTime >= flDuration ) )
	{
		Reset();
	}
	else
	{
#ifdef CLIENT_DLL
		const char *pszLevel = gEngfuncs.pfnGetLevelName();
		if ( pszLevel && *pszLevel && m_lastLoadedMap != pszLevel )
		{
			m_lastLoadedMap = pszLevel;
			m_projector.LoadOverview( pszLevel );
			if ( !m_projector.IsLoadedFromFile() )
			{
				m_projector.AutoFitBounds( m_deployMsg.dropX, m_deployMsg.dropY, m_deployMsg.extractionGates );
			}
		}
#endif
	}
	return 1;
}

void CHudGladderSkyDrop::Reset( void )
{
	m_bActive = false;
	m_flStartTime = -100.0f;
	m_deployMsg = GladderDeployMsg();
	m_projector.Reset();
	m_lastLoadedMap.clear();
}

void CHudGladderSkyDrop::TriggerDeployment( const GladderDeployMsg &msg, float flTime )
{
	m_deployMsg = msg;
	m_flStartTime = flTime;
	m_bActive = true;
	m_iFlags |= HUD_ACTIVE;
}

int CHudGladderSkyDrop::MsgFunc_GladderDeploy( const char *pszName, int iSize, void *pbuf )
{
	GladderDeployMsg dmsg;
	if ( Gladder_ParseDeployMsg( pbuf, iSize, dmsg ) )
	{
		float curTime = 0.0f;
#ifdef CLIENT_DLL
		curTime = gHUD.m_flTime;
		const char *pszLevel = gEngfuncs.pfnGetLevelName();
		if ( pszLevel && *pszLevel && m_lastLoadedMap != pszLevel )
		{
			m_lastLoadedMap = pszLevel;
			m_projector.LoadOverview( pszLevel );
		}
		if ( !m_projector.IsLoadedFromFile() )
		{
			m_projector.AutoFitBounds( dmsg.dropX, dmsg.dropY, dmsg.extractionGates );
		}
#endif
		TriggerDeployment( dmsg, curTime );
	}
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderSkyDrop::Draw( float flTime )
{
#ifdef CLIENT_DLL
	if ( !m_bActive )
		return 1;

	if ( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL )
		return 1;

	float flDuration = ( m_deployMsg.flBriefingDuration > 0.5f ) ? m_deployMsg.flBriefingDuration : 2.8f;
	float flElapsed = flTime - m_flStartTime;
	if ( flElapsed < 0.0f || flElapsed >= flDuration )
	{
		m_bActive = false;
		return 1;
	}

	// 1. Radar Window Box calculation
	int minDim = (std::min)( ScreenWidth, ScreenHeight );
	int boxSide = static_cast<int>( minDim * 0.60f );
	if ( boxSide < 240 )
		boxSide = 240;
	if ( boxSide > 520 )
		boxSide = 520;

	int cx = ScreenWidth / 2;
	int cy = ScreenHeight / 2 + 15;
	int halfSide = boxSide / 2;

	// Dark semi-transparent backdrop behind entire tactical window
	FillRGBA( cx - halfSide, cy - halfSide, boxSide, boxSide, 10, 18, 24, 225 );

	// Draw overview map sprite if loaded
	struct model_s *pMapSprite = m_projector.GetMapSprite();
	if ( pMapSprite && gEngfuncs.pTriAPI )
	{
		int numFrames = pMapSprite->numframes;
		int iTiles = static_cast<int>( sqrt( static_cast<double>( numFrames / 12 ) ) );
		if ( iTiles > 0 )
		{
			int xTiles = iTiles * 4;
			int yTiles = iTiles * 3;
			float tileW = static_cast<float>( boxSide ) / static_cast<float>( xTiles );
			float tileH = static_cast<float>( boxSide ) / static_cast<float>( yTiles );

			gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
			gEngfuncs.pTriAPI->CullFace( TRI_NONE );
			gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 0.85f );

			for ( int iy = 0; iy < yTiles; ++iy )
			{
				for ( int ix = 0; ix < xTiles; ++ix )
				{
					int frame = iy * xTiles + ix;
					if ( frame >= numFrames )
						continue;

					float tx = ( cx - halfSide ) + ix * tileW;
					float ty = ( cy - halfSide ) + iy * tileH;

					gEngfuncs.pTriAPI->SpriteTexture( pMapSprite, frame );
					gEngfuncs.pTriAPI->Begin( TRI_QUADS );
					gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f );
					gEngfuncs.pTriAPI->Vertex3f( tx, ty, 0.0f );
					gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f );
					gEngfuncs.pTriAPI->Vertex3f( tx + tileW, ty, 0.0f );
					gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f );
					gEngfuncs.pTriAPI->Vertex3f( tx + tileW, ty + tileH, 0.0f );
					gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f );
					gEngfuncs.pTriAPI->Vertex3f( tx, ty + tileH, 0.0f );
					gEngfuncs.pTriAPI->End();
				}
			}
		}
	}

	// Border around tactical box (Cyan/Teal)
	FillRGBA( cx - halfSide, cy - halfSide, boxSide, 2, 0, 180, 220, 255 );             // top
	FillRGBA( cx - halfSide, cy + halfSide - 2, boxSide, 2, 0, 180, 220, 255 );         // bottom
	FillRGBA( cx - halfSide, cy - halfSide, 2, boxSide, 0, 180, 220, 255 );             // left
	FillRGBA( cx + halfSide - 2, cy - halfSide, 2, boxSide, 0, 180, 220, 255 );         // right

	// Subtle internal grid crosshairs
	FillRGBA( cx - halfSide + 4, cy, boxSide - 8, 1, 0, 100, 140, 80 );
	FillRGBA( cx, cy - halfSide + 4, 1, boxSide - 8, 0, 100, 140, 80 );

	// 2. Extraction Gates Markers (SPEC §2, §12.1: green extraction icons)
	for ( size_t i = 0; i < m_deployMsg.extractionGates.size(); ++i )
	{
		const auto &gate = m_deployMsg.extractionGates[i];
		int gx = 0, gy = 0;
		m_projector.WorldToScreen( gate.x, gate.y, cx, cy, boxSide, gx, gy );

		// Green diamond icon with shadow backing
		FillRGBA( gx - 6, gy - 6, 13, 13, 0, 0, 0, 255 );
		FillRGBA( gx - 5, gy - 5, 11, 11, 32, 255, 32, 255 );
		FillRGBA( gx - 3, gy - 3, 7, 7, 10, 24, 18, 255 );
		FillRGBA( gx - 1, gy - 1, 3, 3, 32, 255, 32, 255 );

		gEngfuncs.pfnDrawSetTextColor( 0.2f, 1.0f, 0.2f );
		DrawConsoleString( gx - 28, gy + 8, "[EXTRACTION]" );
	}

	// 3. Drop Zone Reticle (SPEC §2, §12.1: animated pulsing reticle)
	int dx = 0, dy = 0;
	m_projector.WorldToScreen( m_deployMsg.dropX, m_deployMsg.dropY, cx, cy, boxSide, dx, dy );

	float pulse = 8.0f + 3.0f * sinf( flTime * 9.0f );
	int pOffset = static_cast<int>( pulse );

	// Black drop-shadow backing
	FillRGBA( dx - 2, dy - 2, 5, 5, 0, 0, 0, 255 );

	// Amber crosshair ticks around drop point
	FillRGBA( dx - 1, dy - 1, 3, 3, 255, 210, 30, 255 ); // center dot
	FillRGBA( dx - 1, dy - pOffset - 7, 3, 7, 255, 210, 30, 255 ); // top
	FillRGBA( dx - 1, dy + pOffset, 3, 7, 255, 210, 30, 255 );     // bottom
	FillRGBA( dx - pOffset - 7, dy - 1, 7, 3, 255, 210, 30, 255 ); // left
	FillRGBA( dx + pOffset, dy - 1, 7, 3, 255, 210, 30, 255 );     // right

	// Corner brackets around reticle
	FillRGBA( dx - pOffset - 4, dy - pOffset - 4, 3, 1, 255, 210, 30, 200 );
	FillRGBA( dx - pOffset - 4, dy - pOffset - 4, 1, 3, 255, 210, 30, 200 );
	FillRGBA( dx + pOffset + 2, dy - pOffset - 4, 3, 1, 255, 210, 30, 200 );
	FillRGBA( dx + pOffset + 4, dy - pOffset - 4, 1, 3, 255, 210, 30, 200 );
	FillRGBA( dx - pOffset - 4, dy + pOffset + 4, 3, 1, 255, 210, 30, 200 );
	FillRGBA( dx - pOffset - 4, dy + pOffset + 2, 1, 3, 255, 210, 30, 200 );
	FillRGBA( dx + pOffset + 2, dy + pOffset + 4, 3, 1, 255, 210, 30, 200 );
	FillRGBA( dx + pOffset + 4, dy + pOffset + 2, 1, 3, 255, 210, 30, 200 );

	gEngfuncs.pfnDrawSetTextColor( 1.0f, 0.85f, 0.1f );
	DrawConsoleString( dx - 24, dy + pOffset + 8, "[DROP ZONE]" );

	// 4. Centered Tactical Telemetry
	// Title: DEPLOYMENT POINT DESIGNATED - INFILTRATING ARENA (Cyan)
	gEngfuncs.pfnDrawSetTextColor( 0.0f, 0.85f, 1.0f );
	const char *szTitle = "DEPLOYMENT POINT DESIGNATED - INFILTRATING ARENA";
	int titleW = static_cast<int>( strlen( szTitle ) ) * 9;
	DrawConsoleString( cx - ( titleW / 2 ), cy - halfSide - 44, szTitle );

	// Hostile Quota: ELIMINATE <N> HOSTILES (Orange / Red)
	char szHostiles[64];
	if ( m_deployMsg.iHostileQuota == 1 )
		snprintf( szHostiles, sizeof( szHostiles ), "ELIMINATE 1 HOSTILE" );
	else
		snprintf( szHostiles, sizeof( szHostiles ), "ELIMINATE %d HOSTILES", m_deployMsg.iHostileQuota );

	gEngfuncs.pfnDrawSetTextColor( 1.0f, 0.5f, 0.1f );
	int hostW = static_cast<int>( strlen( szHostiles ) ) * 9;
	DrawConsoleString( cx - ( hostW / 2 ), cy - halfSide - 24, szHostiles );

	// Mutator warning line if active
	if ( m_deployMsg.iMutatorType > 0 )
	{
		const char *szMutator = "SPECIAL MUTATOR ACTIVE";
		switch ( m_deployMsg.iMutatorType )
		{
			case 1: szMutator = "BLACKOUT MUTATOR ACTIVE"; break;
			case 2: szMutator = "LOW GRAVITY MUTATOR ACTIVE"; break;
			case 3: szMutator = "SWARM MUTATOR ACTIVE"; break;
			case 4: szMutator = "ELITE SURGE MUTATOR ACTIVE"; break;
			case 5: szMutator = "BIG MOMMA MUTATOR ACTIVE"; break;
			default: break;
		}
		gEngfuncs.pfnDrawSetTextColor( 1.0f, 0.2f, 0.2f );
		int mutW = static_cast<int>( strlen( szMutator ) ) * 9;
		DrawConsoleString( cx - ( mutW / 2 ), cy - halfSide - 6, szMutator );
	}

	// Footer: Coordinates and Touchdown countdown
	float flRemaining = ( flDuration > flElapsed ) ? ( flDuration - flElapsed ) : 0.0f;
	char szFooter[128];
	snprintf( szFooter, sizeof( szFooter ), "GRID: [%.0f, %.0f, %.0f] | TOUCHDOWN IN %.1fS",
	          m_deployMsg.dropX, m_deployMsg.dropY, m_deployMsg.dropZ, flRemaining );

	gEngfuncs.pfnDrawSetTextColor( 0.7f, 0.7f, 0.7f );
	int footW = static_cast<int>( strlen( szFooter ) ) * 9;
	DrawConsoleString( cx - ( footW / 2 ), cy + halfSide + 10, szFooter );

	// 5. Ellipsis screen transition: in the final 0.4s, fade fullscreen smoothly to black (SPEC §2, §12.1)
	if ( flElapsed >= ( flDuration - 0.4f ) )
	{
		float fadeAlpha = ( flElapsed - ( flDuration - 0.4f ) ) / 0.4f;
		fadeAlpha = (std::max)( 0.0f, (std::min)( 1.0f, fadeAlpha ) );
		FillRGBA( 0, 0, ScreenWidth, ScreenHeight, 0, 0, 0, static_cast<int>( fadeAlpha * 255.0f ) );
	}

#endif
	return 1;
}
