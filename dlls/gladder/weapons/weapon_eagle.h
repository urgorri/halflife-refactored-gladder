/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal: Desert Eagle (weapon_eagle)
 *	(SPEC §7.1, GitHub Issue #13)
 *
 ****/

#pragma once

#ifndef WEAPON_EAGLE_H
#define WEAPON_EAGLE_H

#include "weapons/weapon_base.h"
#include "gladder_weapons.h"

class CEagle : public CBasePlayerWeapon
{
  public:
	void Spawn( void ) override;
	void Precache( void ) override;
	int iItemSlot( void ) override { return 2; }
	int GetItemInfo( ItemInfo *p ) override;

	BOOL Deploy( void ) override;
	void Holster( int skiplocal = 0 ) override;

	void PrimaryAttack( void ) override;
	void SecondaryAttack( void ) override;
	void Reload( void ) override;
	void WeaponIdle( void ) override;

	void UpdateSpot( void );

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
	bool m_bLaserActive = false;
	unsigned short m_usFireEagle = 0;
	CBaseEntity *m_pLaserSpot = nullptr;
};

#endif // WEAPON_EAGLE_H
