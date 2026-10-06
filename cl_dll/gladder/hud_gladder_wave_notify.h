/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD: Wave Transition Centered Overlay (SPEC §12, GitHub Issue #25)
 *
 ****/

#pragma once

#ifndef HUD_GLADDER_WAVE_NOTIFY_H
#define HUD_GLADDER_WAVE_NOTIFY_H

#include "hud/hud_base.h"
#include "hud/hud_registry.h"

class CHudGladderWaveNotify : public CHudBase
{
  public:
	static constexpr float DISPLAY_HOLD_TIME  = 3.0f;
	static constexpr float DISPLAY_FADE_TIME  = 1.0f;
	static constexpr float DISPLAY_TOTAL_TIME = 4.0f;

	CHudGladderWaveNotify()
	    : m_iType( 0 ),
	      m_iWaveNumber( 1 ),
	      m_iMutatorType( 0 ),
	      m_flStartTime( -100.0f )
	{
		m_szTitle[0]    = '\0';
		m_szSubtitle[0] = '\0';
		m_iFlags        = HUD_ACTIVE;
	}

	int Init( void ) override;
	int VidInit( void ) override;
	int Draw( float flTime ) override;
	void Reset( void ) override
	{
		m_iType         = 0;
		m_iWaveNumber   = 1;
		m_iMutatorType  = 0;
		m_flStartTime   = -100.0f;
		m_szTitle[0]    = '\0';
		m_szSubtitle[0] = '\0';
	}

	// User message dispatcher
	int MsgFunc_GladderWaveNotify( const char *pszName, int iSize, void *pbuf );

	// Trigger simulation (for tests)
	void TriggerNotification( int iType, int iWaveNumber, int iMutatorType, float flTime = 0.0f );

	// Queries
	int GetType( void ) const { return m_iType; }
	int GetWaveNumber( void ) const { return m_iWaveNumber; }
	int GetMutatorType( void ) const { return m_iMutatorType; }
	const char *GetTitle( void ) const { return m_szTitle; }
	const char *GetSubtitle( void ) const { return m_szSubtitle; }
	bool IsDisplaying( float flTime ) const
	{
		return ( ( flTime - m_flStartTime ) >= 0.0f && ( flTime - m_flStartTime ) < DISPLAY_TOTAL_TIME );
	}

  private:
	int m_iType;
	int m_iWaveNumber;
	int m_iMutatorType;
	float m_flStartTime;
	char m_szTitle[128];
	char m_szSubtitle[128];
};

extern CHudGladderWaveNotify g_HudGladderWaveNotify;

#endif // HUD_GLADDER_WAVE_NOTIFY_H
