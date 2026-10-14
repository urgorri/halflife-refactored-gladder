/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD: Lambda Collectible Center-Screen Notification (SPEC §7.3, GitHub Issue #92)
 *
 ****/

#pragma once

#ifndef HUD_GLADDER_LAMBDA_H
#define HUD_GLADDER_LAMBDA_H

#include "hud/hud_base.h"
#include "hud/hud_registry.h"

class CHudGladderLambda : public CHudBase
{
  public:
	static constexpr float DISPLAY_HOLD_TIME  = 1.2f;
	static constexpr float DISPLAY_FADE_TIME  = 0.5f;
	static constexpr float DISPLAY_TOTAL_TIME = 1.7f;

	CHudGladderLambda()
	    : m_iScoreBonus( 0 ),
	      m_iCollectibleCount( 0 ),
	      m_flStartTime( -100.0f )
	{
		m_szText[0] = '\0';
		m_iFlags    = HUD_ACTIVE;
	}

	int Init( void ) override;
	int VidInit( void ) override;
	int Draw( float flTime ) override;
	void Reset( void ) override
	{
		m_iScoreBonus       = 0;
		m_iCollectibleCount = 0;
		m_flStartTime       = -100.0f;
		m_szText[0]         = '\0';
	}

	// User message dispatcher
	int MsgFunc_GladderLambda( const char *pszName, int iSize, void *pbuf );

	// Trigger notification (used by message dispatcher and test suites)
	void TriggerNotification( int iScoreBonus, int iCollectibleCount, float flTime = 0.0f );

	// Queries
	int GetScoreBonus( void ) const { return m_iScoreBonus; }
	int GetCollectibleCount( void ) const { return m_iCollectibleCount; }
	const char *GetText( void ) const { return m_szText; }
	float GetStartTime( void ) const { return m_flStartTime; }
	bool IsDisplaying( float flTime ) const
	{
		float flElapsed = flTime - m_flStartTime;
		return ( flElapsed >= 0.0f && flElapsed < DISPLAY_TOTAL_TIME );
	}

  private:
	int m_iScoreBonus;
	int m_iCollectibleCount;
	float m_flStartTime;
	char m_szText[128];
};

extern CHudGladderLambda g_HudGladderLambda;

#endif // HUD_GLADDER_LAMBDA_H
