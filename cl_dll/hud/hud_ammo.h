/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

#ifndef HUD_AMMO_H
#define HUD_AMMO_H

#include "wrect.h"
#include "cdll_dll.h"
#include "global_consts.h"
#include <string.h>

#ifndef HSPRITE
#define HSPRITE int
#endif

#define MAX_WEAPON_NAME 128
#define WEAPON_FLAGS_SELECTONEMPTY 1
#define WEAPON_IS_ONTARGET 0x40

struct WEAPON
{
	char szName[MAX_WEAPON_NAME];
	int iAmmoType;
	int iAmmo2Type;
	int iMax1;
	int iMax2;
	int iSlot;
	int iSlotPos;
	int iFlags;
	int iId;
	int iClip;

	int iCount; // # of items in plist

	HSPRITE hActive;
	wrect_t rcActive;
	HSPRITE hInactive;
	wrect_t rcInactive;
	HSPRITE hAmmo;
	wrect_t rcAmmo;
	HSPRITE hAmmo2;
	wrect_t rcAmmo2;
	HSPRITE hCrosshair;
	wrect_t rcCrosshair;
	HSPRITE hAutoaim;
	wrect_t rcAutoaim;
	HSPRITE hZoomedCrosshair;
	wrect_t rcZoomedCrosshair;
	HSPRITE hZoomedAutoaim;
	wrect_t rcZoomedAutoaim;
};

typedef int AMMO;

typedef void ( *CrosshairFallbackFn )( WEAPON *pWeapon, int iResolution );

// this is the max number of items in each bucket
#define MAX_WEAPON_POSITIONS MAX_WEAPON_SLOTS

class WeaponsResource
{
  private:
	WEAPON rgWeapons[MAX_WEAPONS];
	WEAPON *rgSlots[MAX_WEAPON_SLOTS + 1][MAX_WEAPON_POSITIONS + 1];
	int riAmmo[MAX_AMMO_TYPES];

  public:
	void Init( void )
	{
		memset( rgWeapons, 0, sizeof rgWeapons );
		Reset();
	}

	void Reset( void )
	{
		iOldWeaponBits = 0;
		memset( rgSlots, 0, sizeof rgSlots );
		memset( riAmmo, 0, sizeof riAmmo );
	}

	int iOldWeaponBits;

	WEAPON *GetWeapon( int iId ) { return &rgWeapons[iId]; }
	void AddWeapon( WEAPON *wp )
	{
		rgWeapons[wp->iId] = *wp;
		LoadWeaponSprites( &rgWeapons[wp->iId] );
	}

	void PickupWeapon( WEAPON *wp )
	{
		rgSlots[wp->iSlot][wp->iSlotPos] = wp;
	}

	void DropWeapon( WEAPON *wp )
	{
		rgSlots[wp->iSlot][wp->iSlotPos] = NULL;
	}

	void DropAllWeapons( void )
	{
		for ( int i = 0; i < MAX_WEAPONS; i++ )
		{
			if ( rgWeapons[i].iId )
				DropWeapon( &rgWeapons[i] );
		}
	}

	WEAPON *GetWeaponSlot( int slot, int pos ) { return rgSlots[slot][pos]; }

	void LoadWeaponSprites( WEAPON *wp );
	void LoadAllWeaponSprites( void );
	WEAPON *GetFirstPos( int iSlot );
	void SelectSlot( int iSlot, int fAdvance, int iDirection );
	WEAPON *GetNextActivePos( int iSlot, int iSlotPos );

	int HasAmmo( WEAPON *p );

	AMMO GetAmmo( int iId ) { return iId; }
	void SetAmmo( int iId, int iCount ) { riAmmo[iId] = iCount; }
	int CountAmmo( int iId );
	HSPRITE *GetAmmoPicFromWeapon( int iAmmoId, wrect_t &rect );

	static void SetCrosshairFallbackHandler( CrosshairFallbackFn pfnHandler );
	static CrosshairFallbackFn GetCrosshairFallbackHandler( void );
};

extern WeaponsResource gWR;

#define MAX_HISTORY 12
enum
{
	HISTSLOT_EMPTY,
	HISTSLOT_AMMO,
	HISTSLOT_WEAP,
	HISTSLOT_ITEM,
};

class HistoryResource
{
  private:
	struct HIST_ITEM
	{
		int type;
		float DisplayTime;
		int iCount;
		int iId;
	};

	HIST_ITEM rgAmmoHistory[MAX_HISTORY];

  public:
	void Init( void )
	{
		Reset();
	}

	void Reset( void )
	{
		memset( rgAmmoHistory, 0, sizeof rgAmmoHistory );
	}

	int iHistoryGap;
	int iCurrentHistorySlot;

	void AddToHistory( int iType, int iId, int iCount = 0 );
	void AddToHistory( int iType, const char *szName, int iCount = 0 );

	void CheckClearHistory( void );
	int DrawAmmoHistory( float flTime );
};

extern HistoryResource gHR;
extern WEAPON *gpActiveSel;
extern WEAPON *gpLastSel;
extern int g_weaponselect;

#endif // HUD_AMMO_H
