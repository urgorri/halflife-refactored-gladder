/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD: Sky-Drop Tactical Overview & Deployment Overlay (SPEC §2, §12.1, Issue #141)
 *
 ****/

#pragma once

#ifndef HUD_GLADDER_SKYDROP_H
#define HUD_GLADDER_SKYDROP_H

#include "hud/hud_base.h"
#include "hud/hud_registry.h"
#include "gladder_cl_usermsg.h"
#include <string>
#include <vector>

// Overview projection helper to map 3D world coordinates to 2D overview coordinates
class GladderOverviewProjector
{
  public:
	GladderOverviewProjector()
	    : m_originX( 0.0f ),
	      m_originY( 0.0f ),
	      m_originZ( 0.0f ),
	      m_zoom( 1.0f ),
	      m_rotated( 0 ),
	      m_bLoaded( false )
	{
	}

	void Reset( void )
	{
		m_originX = 0.0f;
		m_originY = 0.0f;
		m_originZ = 0.0f;
		m_zoom    = 1.0f;
		m_rotated = 0;
		m_bLoaded = false;
	}

	bool ParseFromBuffer( const char *pBuffer );
	bool LoadOverview( const char *pszMapName );

	bool WorldToScreen( float worldX, float worldY, int boxCenterX, int boxCenterY, int boxSide, int &outX, int &outY ) const;

	float GetOriginX( void ) const { return m_originX; }
	float GetOriginY( void ) const { return m_originY; }
	float GetZoom( void ) const { return m_zoom; }
	int GetRotated( void ) const { return m_rotated; }
	bool IsLoaded( void ) const { return m_bLoaded; }

	void SetData( float originX, float originY, float zoom, int rotated )
	{
		m_originX = originX;
		m_originY = originY;
		m_zoom    = ( zoom > 0.001f ) ? zoom : 1.0f;
		m_rotated = rotated;
		m_bLoaded = true;
	}

  private:
	float m_originX;
	float m_originY;
	float m_originZ;
	float m_zoom;
	int m_rotated;
	bool m_bLoaded;
};

class CHudGladderSkyDrop : public CHudBase
{
  public:
	CHudGladderSkyDrop();

	int Init( void ) override;
	int VidInit( void ) override;
	int Draw( float flTime ) override;
	void Reset( void ) override;

	// User message dispatcher
	int MsgFunc_GladderDeploy( const char *pszName, int iSize, void *pbuf );

	// Simulation hook for unit tests
	void TriggerDeployment( const GladderDeployMsg &msg, float flTime = 0.0f );

	// Queries
	bool IsActive( void ) const { return m_bActive; }
	float GetStartTime( void ) const { return m_flStartTime; }
	const GladderDeployMsg &GetDeployMsg( void ) const { return m_deployMsg; }
	GladderOverviewProjector &GetProjector( void ) { return m_projector; }

  private:
	bool m_bActive;
	float m_flStartTime;
	GladderDeployMsg m_deployMsg;
	GladderOverviewProjector m_projector;
	std::string m_lastLoadedMap;
};

extern CHudGladderSkyDrop g_HudGladderSkyDrop;

#endif // HUD_GLADDER_SKYDROP_H
