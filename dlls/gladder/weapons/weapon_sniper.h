/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal: M40A1 Sniper Rifle (weapon_sniper)
 *	(SPEC §7.1, GitHub Issue #13)
 *
 ****/

#pragma once

#ifndef WEAPON_SNIPER_H
#define WEAPON_SNIPER_H

#include "weapons/weapon_base.h"
#include "gladder_weapons.h"

class CSniperRifle : public CBasePlayerWeapon
{
  public:
	void Spawn( void ) override;
	void Precache( void ) override;
	int iItemSlot( void ) override { return 3; }
	int GetItemInfo( ItemInfo *p ) override;

	BOOL Deploy( void ) override;
	void Holster( int skiplocal = 0 ) override;

	void PrimaryAttack( void ) override;
	void SecondaryAttack( void ) override;
	void Reload( void ) override;
	void WeaponIdle( void ) override;
	void ToggleZoom( void );

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
	bool m_bInZoom = false;
	unsigned short m_usSniper = 0;
};

class CSniperAmmo : public CBasePlayerAmmo
{
  public:
	void Spawn( void ) override;
	void Precache( void ) override;
	BOOL AddAmmo( CBaseEntity *pOther ) override;
};

#endif // WEAPON_SNIPER_H
