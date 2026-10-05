/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD: Dynamic Kill Streak Combo Gauge (SPEC §9.1, GitHub Issue #15)
 *
 ****/

#pragma once

#ifndef HUD_GLADDER_COMBO_H
#define HUD_GLADDER_COMBO_H

#include "hud/hud_base.h"
#include "hud/hud_registry.h"

class CHudGladderCombo : public CHudBase
{
  public:
	CHudGladderCombo()
	    : m_iComboMultiplier( 1 ),
	      m_flDecayFraction( 0.0f ),
	      m_flLastKillTime( 0.0f ),
	      m_flAlpha( 0.0f )
	{
		m_iFlags = HUD_ACTIVE;
	}

	int Init( void ) override;
	int VidInit( void ) override;
	int Draw( float flTime ) override;
	void Reset( void ) override
	{
		m_iComboMultiplier = 1;
		m_flDecayFraction  = 0.0f;
		m_flLastKillTime   = 0.0f;
		m_flAlpha          = 0.0f;
	}

	// User message dispatcher
	int MsgFunc_GladderCombo( const char *pszName, int iSize, void *pbuf );

	// Getters
	int GetComboMultiplier( void ) const { return m_iComboMultiplier; }
	float GetDecayFraction( void ) const { return m_flDecayFraction; }
	bool IsActive( void ) const { return m_iComboMultiplier > 1 && m_flDecayFraction > 0.0f; }

	// Test/Simulation setters
	void SetComboState( int multiplier, float decayFraction, float flTime = 0.0f )
	{
		if ( multiplier > m_iComboMultiplier || ( multiplier > 1 && decayFraction > m_flDecayFraction ) )
		{
			m_flLastKillTime = flTime;
		}
		m_iComboMultiplier = multiplier;
		m_flDecayFraction  = decayFraction;
	}

  private:
	int m_iComboMultiplier;
	float m_flDecayFraction;
	float m_flLastKillTime;
	float m_flAlpha;
};

extern CHudGladderCombo g_HudGladderCombo;

#endif // HUD_GLADDER_COMBO_H
