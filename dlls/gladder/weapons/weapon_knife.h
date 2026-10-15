/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal: Combat Knife (weapon_knife)
 *	(SPEC §7.1, GitHub Issue #13)
 *
 ****/

#pragma once

#ifndef WEAPON_KNIFE_H
#define WEAPON_KNIFE_H

#include "weapons/weapon_base.h"
#include "gladder_weapons.h"

class CKnife : public CBasePlayerWeapon
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
	int Swing( int fFirst );
	int Stab( void );

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
	TraceResult m_trHit;
	unsigned short m_usKnife = 0;
};

#endif // WEAPON_KNIFE_H
