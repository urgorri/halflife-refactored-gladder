/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal: M249 SAW (weapon_m249)
 *	(SPEC §7.1, GitHub Issue #13)
 *
 ****/

#pragma once

#ifndef WEAPON_M249_H
#define WEAPON_M249_H

#include "weapons/weapon_base.h"

enum M249Anim
{
	M249_SLOWIDLE = 0,
	M249_IDLE2,
	M249_LAUNCH,
	M249_RELOAD1,
	M249_HOLSTER,
	M249_DRAW,
	M249_SHOOT1,
	M249_SHOOT2
};

class CM249 : public CBasePlayerWeapon
{
  public:
	void Spawn( void ) override;
	void Precache( void ) override;
	int iItemSlot( void ) override { return 4; }
	int GetItemInfo( ItemInfo *p ) override;

	BOOL Deploy( void ) override;
	void Holster( int skiplocal = 0 ) override;

	void PrimaryAttack( void ) override;
	void Reload( void ) override;
	void WeaponIdle( void ) override;

	BOOL UseDecrement( void ) override
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

  private:
	int m_iShell = 0;
	int m_iLink = 0;
	unsigned short m_usFireM249 = 0;
};

class CAmmo556 : public CBasePlayerAmmo
{
  public:
	void Spawn( void ) override;
	void Precache( void ) override;
	BOOL AddAmmo( CBaseEntity *pOther ) override;
};

#endif // WEAPON_M249_H
