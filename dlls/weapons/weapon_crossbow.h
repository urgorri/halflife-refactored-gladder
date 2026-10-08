/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/
#ifndef WEAPON_CROSSBOW_H
#define WEAPON_CROSSBOW_H

#include "weapons/weapon_base.h"

enum crossbow_e
{
	CROSSBOW_IDLE1 = 0, // full
	CROSSBOW_IDLE2,     // empty
	CROSSBOW_FIDGET1,   // full
	CROSSBOW_FIDGET2,   // empty
	CROSSBOW_FIRE1,     // full
	CROSSBOW_FIRE2,     // reload
	CROSSBOW_FIRE3,     // empty
	CROSSBOW_RELOAD,    // from empty
	CROSSBOW_DRAW1,     // full
	CROSSBOW_DRAW2,     // empty
	CROSSBOW_HOLSTER1,  // full
	CROSSBOW_HOLSTER2,  // empty
};

class CCrossbow : public CBasePlayerWeapon
{
  public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot() { return 3; }
	int GetItemInfo( ItemInfo *p );

	void FireBolt( void );
	void FireSniperBolt( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL Deploy();
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );

	int m_fInZoom; // don't save this

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

  private:
	unsigned short m_usCrossbow;
	unsigned short m_usCrossbow2;
};

#endif // WEAPON_CROSSBOW_H
