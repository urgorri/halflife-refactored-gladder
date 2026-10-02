/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal: Pipe Wrench (weapon_pipewrench)
 *	(SPEC §7.1, GitHub Issue #13)
 *
 ****/

#pragma once

#ifndef WEAPON_PIPEWRENCH_H
#define WEAPON_PIPEWRENCH_H

#include "weapons/weapon_base.h"

enum PipewrenchAnim
{
	PIPEWRENCH_IDLE1 = 0,
	PIPEWRENCH_IDLE2,
	PIPEWRENCH_IDLE3,
	PIPEWRENCH_DRAW,
	PIPEWRENCH_HOLSTER,
	PIPEWRENCH_ATTACK1HIT,
	PIPEWRENCH_ATTACK1MISS,
	PIPEWRENCH_ATTACK2HIT,
	PIPEWRENCH_ATTACK2MISS,
	PIPEWRENCH_ATTACK3HIT,
	PIPEWRENCH_ATTACK3MISS,
	PIPEWRENCH_BIG_SWING_START,
	PIPEWRENCH_BIG_SWING_HIT,
	PIPEWRENCH_BIG_SWING_MISS,
	PIPEWRENCH_BIG_SWING_IDLE
};

enum PipewrenchSwingMode
{
	WRENCH_SWING_NONE = 0,
	WRENCH_SWING_START,
	WRENCH_SWING_HOLD,
	WRENCH_SWING_RELEASE
};

class CPipewrench : public CBasePlayerWeapon
{
  public:
	void Spawn( void ) override;
	void Precache( void ) override;
	int iItemSlot( void ) override { return 1; }
	int GetItemInfo( ItemInfo *p ) override;

	BOOL Deploy( void ) override;
	void Holster( int skiplocal = 0 ) override;

	void PrimaryAttack( void ) override;
	void SecondaryAttack( void ) override;
	void WeaponIdle( void ) override;

	int Swing( int fFirst );
	void BigSwing( void );

	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );

	BOOL UseDecrement( void ) override
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

  private:
	int m_iSwing = 0;
	int m_iSwingMode = WRENCH_SWING_NONE;
	float m_flBigSwingStart = 0.0f;
	TraceResult m_trHit;
	unsigned short m_usPipewrench = 0;
};

#endif // WEAPON_PIPEWRENCH_H
