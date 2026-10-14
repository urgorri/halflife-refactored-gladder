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

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "pm_shared.h"

#include <string.h>
#include <stdio.h>

#include "hud_ammo.h"
#include "vgui/vgui_TeamFortressViewport.h"

extern WEAPON *gpActiveSel;
extern WEAPON *gpLastSel;
extern WeaponsResource gWR;
extern int g_weaponselect;
extern int giBucketHeight, giBucketWidth, giABHeight, giABWidth;

void CHudAmmo::Think( void )
{
	if ( gHUD.m_fPlayerDead )
		return;

	if ( gHUD.m_iWeaponBits != gWR.iOldWeaponBits )
	{
		gWR.iOldWeaponBits = gHUD.m_iWeaponBits;

		for ( int i = MAX_WEAPONS - 1; i > 0; i-- )
		{
			WEAPON *p = gWR.GetWeapon( i );

			if ( p )
			{
				if ( gHUD.m_iWeaponBits & ( 1 << p->iId ) )
					gWR.PickupWeapon( p );
				else
					gWR.DropWeapon( p );
			}
		}
	}

	if ( !gpActiveSel )
		return;

	// has the player selected one?
	if ( gHUD.m_iKeyBits & IN_ATTACK )
	{
		if ( gpActiveSel != (WEAPON *)1 )
		{
			ServerCmd( gpActiveSel->szName );
			g_weaponselect = gpActiveSel->iId;
		}

		gpLastSel   = gpActiveSel;
		gpActiveSel = NULL;
		gHUD.m_iKeyBits &= ~IN_ATTACK;

		PlaySound( "common/wpn_select.wav", 1 );
	}
}

//
// Helper function to return a Ammo pointer from id
//

HSPRITE *WeaponsResource ::GetAmmoPicFromWeapon( int iAmmoId, wrect_t &rect )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( rgWeapons[i].iAmmoType == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo;
			return &rgWeapons[i].hAmmo;
		}
		else if ( rgWeapons[i].iAmmo2Type == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo2;
			return &rgWeapons[i].hAmmo2;
		}
	}

	return NULL;
}

// Menu Selection Code

void WeaponsResource ::SelectSlot( int iSlot, int fAdvance, int iDirection )
{
	if ( gHUD.m_Menu.m_fMenuDisplayed && ( fAdvance == FALSE ) && ( iDirection == 1 ) )
	{
		gHUD.m_Menu.SelectMenuItem( iSlot + 1 );
		return;
	}

	if ( iSlot >= MAX_WEAPON_SLOTS )
		return;

	if ( gHUD.m_fPlayerDead || gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) )
		return;

	if ( !( gHUD.m_iWeaponBits & ( 1 << ( WEAPON_SUIT ) ) ) )
		return;

	if ( !( gHUD.m_iWeaponBits & ~( 1 << ( WEAPON_SUIT ) ) ) )
		return;

	WEAPON *p       = NULL;
	bool fastSwitch = CVAR_GET_FLOAT( "hud_fastswitch" ) != 0;

	if ( ( gpActiveSel == NULL ) || ( gpActiveSel == (WEAPON *)1 ) || ( iSlot != gpActiveSel->iSlot ) )
	{
		PlaySound( "common/wpn_hudon.wav", 1 );
		p = GetFirstPos( iSlot );

		if ( p && fastSwitch )
		{
			WEAPON *p2 = GetNextActivePos( p->iSlot, p->iSlotPos );
			if ( !p2 )
			{
				ServerCmd( p->szName );
				g_weaponselect = p->iId;
				return;
			}
		}
	}
	else
	{
		PlaySound( "common/wpn_moveselect.wav", 1 );
		if ( gpActiveSel )
			p = GetNextActivePos( gpActiveSel->iSlot, gpActiveSel->iSlotPos );
		if ( !p )
			p = GetFirstPos( iSlot );
	}

	if ( !p )
	{
		if ( !fastSwitch )
			gpActiveSel = (WEAPON *)1;
		else
			gpActiveSel = NULL;
	}
	else
		gpActiveSel = p;
}

void CHudAmmo::SlotInput( int iSlot )
{
	if ( gViewPort && gViewPort->SlotInput( iSlot ) )
		return;

	gWR.SelectSlot( iSlot, FALSE, 1 );
}

void CHudAmmo::UserCmd_Slot1( void )
{
	SlotInput( 0 );
}

void CHudAmmo::UserCmd_Slot2( void )
{
	SlotInput( 1 );
}

void CHudAmmo::UserCmd_Slot3( void )
{
	SlotInput( 2 );
}

void CHudAmmo::UserCmd_Slot4( void )
{
	SlotInput( 3 );
}

void CHudAmmo::UserCmd_Slot5( void )
{
	SlotInput( 4 );
}

void CHudAmmo::UserCmd_Slot6( void )
{
	SlotInput( 5 );
}

void CHudAmmo::UserCmd_Slot7( void )
{
	SlotInput( 6 );
}

void CHudAmmo::UserCmd_Slot8( void )
{
	SlotInput( 7 );
}

void CHudAmmo::UserCmd_Slot9( void )
{
	SlotInput( 8 );
}

void CHudAmmo::UserCmd_Slot10( void )
{
	SlotInput( 9 );
}

void CHudAmmo::UserCmd_Close( void )
{
	if ( gpActiveSel )
	{
		gpLastSel   = gpActiveSel;
		gpActiveSel = NULL;
		PlaySound( "common/wpn_hudoff.wav", 1 );
	}
	else
		EngineClientCmd( "escape" );
}

// Selects the next item in the weapon menu
void CHudAmmo::UserCmd_NextWeapon( void )
{
	if ( gHUD.m_fPlayerDead || ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) ) )
		return;

	if ( !gpActiveSel || gpActiveSel == (WEAPON *)1 )
		gpActiveSel = m_pWeapon;

	int pos  = 0;
	int slot = 0;
	if ( gpActiveSel )
	{
		pos  = gpActiveSel->iSlotPos + 1;
		slot = gpActiveSel->iSlot;
	}

	for ( int loop = 0; loop <= 1; loop++ )
	{
		for ( ; slot < MAX_WEAPON_SLOTS; slot++ )
		{
			for ( ; pos < MAX_WEAPON_POSITIONS; pos++ )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if ( wsp && gWR.HasAmmo( wsp ) )
				{
					gpActiveSel = wsp;
					return;
				}
			}

			pos = 0;
		}

		slot = 0;
	}

	gpActiveSel = NULL;
}

// Selects the previous item in the menu
void CHudAmmo::UserCmd_PrevWeapon( void )
{
	if ( gHUD.m_fPlayerDead || ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) ) )
		return;

	if ( !gpActiveSel || gpActiveSel == (WEAPON *)1 )
		gpActiveSel = m_pWeapon;

	int pos  = MAX_WEAPON_POSITIONS - 1;
	int slot = MAX_WEAPON_SLOTS - 1;
	if ( gpActiveSel )
	{
		pos  = gpActiveSel->iSlotPos - 1;
		slot = gpActiveSel->iSlot;
	}

	for ( int loop = 0; loop <= 1; loop++ )
	{
		for ( ; slot >= 0; slot-- )
		{
			for ( ; pos >= 0; pos-- )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if ( wsp && gWR.HasAmmo( wsp ) )
				{
					gpActiveSel = wsp;
					return;
				}
			}

			pos = MAX_WEAPON_POSITIONS - 1;
		}

		slot = MAX_WEAPON_SLOTS - 1;
	}

	gpActiveSel = NULL;
}

//
// Draws the ammo bar on the hud
//
static int DrawBar( int x, int y, int width, int height, float f )
{
	int r, g, b;

	if ( f < 0 )
		f = 0;
	if ( f > 1 )
		f = 1;

	if ( f )
	{
		int w = f * width;

		// Always show at least one pixel if we have ammo.
		if ( w <= 0 )
			w = 1;
		UnpackRGB( r, g, b, RGB_GREENISH );
		FillRGBA( x, y, w, height, r, g, b, 255 );
		x += w;
		width -= w;
	}

	UnpackRGB( r, g, b, RGB_YELLOWISH );

	FillRGBA( x, y, width, height, r, g, b, 128 );

	return ( x + width );
}

static void DrawAmmoBar( WEAPON *p, int x, int y, int width, int height )
{
	if ( !p )
		return;

	if ( p->iAmmoType != -1 )
	{
		if ( !gWR.CountAmmo( p->iAmmoType ) )
			return;

		float f = (float)gWR.CountAmmo( p->iAmmoType ) / (float)p->iMax1;

		x = DrawBar( x, y, width, height, f );

		// Do we have secondary ammo too?

		if ( p->iAmmo2Type != -1 )
		{
			f = (float)gWR.CountAmmo( p->iAmmo2Type ) / (float)p->iMax2;

			x += 5; //!!!

			DrawBar( x, y, width, height, f );
		}
	}
}

int CHudAmmo::DrawWList( float flTime )
{
	int r, g, b, x, y, a, i;

	if ( !gpActiveSel )
		return 0;

	int iActiveSlot;

	if ( gpActiveSel == (WEAPON *)1 )
		iActiveSlot = -1;
	else
		iActiveSlot = gpActiveSel->iSlot;

	x = 10;
	y = 10;

	// Ensure that there are available choices in the active slot
	if ( iActiveSlot > 0 )
	{
		if ( !gWR.GetFirstPos( iActiveSlot ) )
		{
			gpActiveSel = (WEAPON *)1;
			iActiveSlot = -1;
		}
	}

	// Determine the maximum slot to draw.
	// Always draw at least the 5 standard Half-Life slots; expand dynamically if higher slots contain weapons.
	int iMaxSlot = 5;
	for ( int s = 5; s < MAX_WEAPON_SLOTS; s++ )
	{
		if ( gWR.GetFirstPos( s ) != NULL )
			iMaxSlot = s + 1;
	}
	if ( iActiveSlot >= iMaxSlot )
		iMaxSlot = iActiveSlot + 1;
	if ( iMaxSlot > MAX_WEAPON_SLOTS )
		iMaxSlot = MAX_WEAPON_SLOTS;

	// Draw top line
	for ( i = 0; i < iMaxSlot; i++ )
	{
		int iWidth;

		UnpackRGB( r, g, b, RGB_YELLOWISH );

		if ( iActiveSlot == i )
			a = 255;
		else
			a = 192;

		ScaleColors( r, g, b, 255 );

		int iBucketSpr  = ( i < MAX_WEAPON_SLOTS && m_HUD_buckets[i] >= 0 ) ? m_HUD_buckets[i] : ( m_HUD_bucket0 + i );
		HSPRITE hBucket = gHUD.GetSprite( iBucketSpr );
		if ( hBucket )
		{
			SPR_Set( hBucket, r, g, b );
		}

		// make active slot wide enough to accomodate gun pictures
		if ( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos( iActiveSlot );
			if ( p )
				iWidth = p->rcActive.right - p->rcActive.left;
			else
				iWidth = giBucketWidth;
		}
		else
			iWidth = giBucketWidth;

		// Prevent drawing outside screen boundaries on low resolutions
		if ( x + iWidth > ScreenWidth )
			break;

		if ( hBucket )
		{
			SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( iBucketSpr ) );
		}

		x += iWidth + 5;
	}

	a = 128;
	x = 10;

	// Draw all of the buckets
	for ( i = 0; i < iMaxSlot; i++ )
	{
		y = giBucketHeight + 10;

		if ( i == iActiveSlot )
		{
			WEAPON *p  = gWR.GetFirstPos( i );
			int iWidth = giBucketWidth;
			if ( p )
				iWidth = p->rcActive.right - p->rcActive.left;

			if ( x + iWidth > ScreenWidth )
				break;

			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				p = gWR.GetWeaponSlot( i, iPos );

				if ( !p || !p->iId )
					continue;

				UnpackRGB( r, g, b, RGB_YELLOWISH );

				if ( gpActiveSel == p )
				{
					if ( p->hActive )
					{
						SPR_Set( p->hActive, r, g, b );
						SPR_DrawAdditive( 0, x, y, &p->rcActive );
					}

					SPR_Set( gHUD.GetSprite( m_HUD_selection ), r, g, b );
					SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( m_HUD_selection ) );
				}
				else
				{
					if ( gWR.HasAmmo( p ) )
						ScaleColors( r, g, b, 192 );
					else
					{
						UnpackRGB( r, g, b, RGB_REDISH );
						ScaleColors( r, g, b, 128 );
					}

					if ( p->hInactive )
					{
						SPR_Set( p->hInactive, r, g, b );
						SPR_DrawAdditive( 0, x, y, &p->rcInactive );
					}
				}

				DrawAmmoBar( p, x + giABWidth / 2, y, giABWidth, giABHeight );

				y += p->rcActive.bottom - p->rcActive.top + 5;
			}

			x += iWidth + 5;
		}
		else
		{
			if ( x + giBucketWidth > ScreenWidth )
				break;

			UnpackRGB( r, g, b, RGB_YELLOWISH );

			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				WEAPON *p = gWR.GetWeaponSlot( i, iPos );

				if ( !p || !p->iId )
					continue;

				if ( gWR.HasAmmo( p ) )
				{
					UnpackRGB( r, g, b, RGB_YELLOWISH );
					a = 128;
				}
				else
				{
					UnpackRGB( r, g, b, RGB_REDISH );
					a = 96;
				}

				FillRGBA( x, y, giBucketWidth, giBucketHeight, r, g, b, a );

				y += giBucketHeight + 5;
			}

			x += giBucketWidth + 5;
		}
	}

	return 1;
}
