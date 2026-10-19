/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Toxic Spit Smoke Cloud Entity (SPEC §3, GitHub Issue #116)
 *
 ****/

#pragma once

#ifndef GLADDER_SQUIDSPIT_CLOUD_H
#define GLADDER_SQUIDSPIT_CLOUD_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"

class CSquidSpitCloud : public CBaseEntity
{
  public:
	void Spawn( void ) override;
	void Precache( void ) override;
	void EXPORT CloudThink( void );

	static CSquidSpitCloud *Create( const Vector &vecOrigin, float flDuration );

	float GetDieTime( void ) const { return m_flDieTime; }
	float GetNextDmgTime( void ) const { return m_flNextDmgTime; }

  private:
	float m_flDieTime;
	float m_flNextDmgTime;
	int m_iSprite;
};

#endif // GLADDER_SQUIDSPIT_CLOUD_H
