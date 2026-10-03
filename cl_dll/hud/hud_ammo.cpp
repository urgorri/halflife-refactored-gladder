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

#if !defined( HL_TESTS )
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "pm_shared.h"

#include <string.h>
#include <stdio.h>

#include "hud_ammo.h"
#include "vgui/vgui_TeamFortressViewport.h"

WEAPON *gpActiveSel; // NULL means off, 1 means just the menu bar, otherwise
                     // this points to the active weapon menu item
WEAPON *gpLastSel;   // Last weapon menu selection

int g_weaponselect = 0;
#else
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include "tests/mock_client_engine.h"
#include "hud_ammo.h"

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define SPR_GetList (*g_mockClientEngineFuncs.pfnSPR_GetList)
#define SPR_Load (*g_mockClientEngineFuncs.pfnSPR_Load)

int ScreenWidth = 640;
int ScreenHeight = 480;
HistoryResource gHR;
#endif

client_sprite_t *GetSpriteList( client_sprite_t *pList, const char *psz, int iRes, int iCount );

WeaponsResource gWR;

static CrosshairFallbackFn s_pfnCrosshairFallback = nullptr;

void WeaponsResource::SetCrosshairFallbackHandler( CrosshairFallbackFn pfnHandler )
{
	s_pfnCrosshairFallback = pfnHandler;
}

CrosshairFallbackFn WeaponsResource::GetCrosshairFallbackHandler( void )
{
	return s_pfnCrosshairFallback;
}

void WeaponsResource ::LoadAllWeaponSprites( void )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( rgWeapons[i].iId )
			LoadWeaponSprites( &rgWeapons[i] );
	}
}

int WeaponsResource ::CountAmmo( int iId )
{
	if ( iId < 0 )
		return 0;

	return riAmmo[iId];
}

int WeaponsResource ::HasAmmo( WEAPON *p )
{
	if ( !p )
		return FALSE;

	// weapons with no max ammo can always be selected
	if ( p->iMax1 == -1 )
		return TRUE;

	return ( p->iAmmoType == -1 ) || p->iClip > 0 || CountAmmo( p->iAmmoType ) || CountAmmo( p->iAmmo2Type ) || ( p->iFlags & WEAPON_FLAGS_SELECTONEMPTY );
}

void WeaponsResource ::LoadWeaponSprites( WEAPON *pWeapon )
{
	int i, iRes;

#if !defined( _TFC )
	if ( ScreenWidth > 2560 && ScreenHeight > 1600 )
		iRes = 2560;
	else if ( ScreenWidth >= 1280 && ScreenHeight > 720 )
		iRes = 1280;
	else
#endif
	    if ( ScreenWidth >= 640 )
		iRes = 640;
	else
		iRes = 320;

	char sz[128];

	if ( !pWeapon )
		return;

	memset( &pWeapon->rcActive, 0, sizeof( wrect_t ) );
	memset( &pWeapon->rcInactive, 0, sizeof( wrect_t ) );
	memset( &pWeapon->rcAmmo, 0, sizeof( wrect_t ) );
	memset( &pWeapon->rcAmmo2, 0, sizeof( wrect_t ) );
	pWeapon->hInactive = 0;
	pWeapon->hActive   = 0;
	pWeapon->hAmmo     = 0;
	pWeapon->hAmmo2    = 0;

	sprintf( sz, "sprites/%s.txt", pWeapon->szName );
	client_sprite_t *pList = SPR_GetList( sz, &i );

	if ( !pList )
		return;

	client_sprite_t *p;

	p = GetSpriteList( pList, "crosshair", iRes, i );
	if ( p )
	{
		sprintf( sz, "sprites/%s.spr", p->szSprite );
		pWeapon->hCrosshair  = SPR_Load( sz );
		pWeapon->rcCrosshair = p->rc;
	}
	else
	{
		pWeapon->hCrosshair = NULL;
		if ( s_pfnCrosshairFallback )
		{
			s_pfnCrosshairFallback( pWeapon, iRes );
		}
	}

	p = GetSpriteList( pList, "autoaim", iRes, i );
	if ( p )
	{
		sprintf( sz, "sprites/%s.spr", p->szSprite );
		pWeapon->hAutoaim  = SPR_Load( sz );
		pWeapon->rcAutoaim = p->rc;
	}
	else
		pWeapon->hAutoaim = 0;

	p = GetSpriteList( pList, "zoom", iRes, i );
	if ( p )
	{
		sprintf( sz, "sprites/%s.spr", p->szSprite );
		pWeapon->hZoomedCrosshair  = SPR_Load( sz );
		pWeapon->rcZoomedCrosshair = p->rc;
	}
	else
	{
		pWeapon->hZoomedCrosshair  = pWeapon->hCrosshair; // default to non-zoomed crosshair
		pWeapon->rcZoomedCrosshair = pWeapon->rcCrosshair;
	}

	p = GetSpriteList( pList, "zoom_autoaim", iRes, i );
	if ( p )
	{
		sprintf( sz, "sprites/%s.spr", p->szSprite );
		pWeapon->hZoomedAutoaim  = SPR_Load( sz );
		pWeapon->rcZoomedAutoaim = p->rc;
	}
	else
	{
		pWeapon->hZoomedAutoaim  = pWeapon->hZoomedCrosshair; // default to zoomed crosshair
		pWeapon->rcZoomedAutoaim = pWeapon->rcZoomedCrosshair;
	}

	p = GetSpriteList( pList, "weapon", iRes, i );
	if ( p )
	{
		sprintf( sz, "sprites/%s.spr", p->szSprite );
		pWeapon->hInactive  = SPR_Load( sz );
		pWeapon->rcInactive = p->rc;

		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
		pWeapon->hInactive = 0;

	p = GetSpriteList( pList, "weapon_s", iRes, i );
	if ( p )
	{
		sprintf( sz, "sprites/%s.spr", p->szSprite );
		pWeapon->hActive  = SPR_Load( sz );
		pWeapon->rcActive = p->rc;
	}
	else
		pWeapon->hActive = 0;

	p = GetSpriteList( pList, "ammo", iRes, i );
	if ( p )
	{
		sprintf( sz, "sprites/%s.spr", p->szSprite );
		pWeapon->hAmmo  = SPR_Load( sz );
		pWeapon->rcAmmo = p->rc;

		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
		pWeapon->hAmmo = 0;

	p = GetSpriteList( pList, "ammo2", iRes, i );
	if ( p )
	{
		sprintf( sz, "sprites/%s.spr", p->szSprite );
		pWeapon->hAmmo2  = SPR_Load( sz );
		pWeapon->rcAmmo2 = p->rc;

		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
		pWeapon->hAmmo2 = 0;
}

// Returns the first weapon for a given slot.
WEAPON *WeaponsResource ::GetFirstPos( int iSlot )
{
	WEAPON *pret = NULL;

	for ( int i = 0; i < MAX_WEAPON_POSITIONS; i++ )
	{
		if ( rgSlots[iSlot][i] && HasAmmo( rgSlots[iSlot][i] ) )
		{
			pret = rgSlots[iSlot][i];
			break;
		}
	}

	return pret;
}

WEAPON *WeaponsResource ::GetNextActivePos( int iSlot, int iSlotPos )
{
	if ( iSlotPos >= MAX_WEAPON_POSITIONS || iSlot >= MAX_WEAPON_SLOTS )
		return NULL;

	WEAPON *p = gWR.rgSlots[iSlot][iSlotPos + 1];

	if ( !p || !gWR.HasAmmo( p ) )
		return GetNextActivePos( iSlot, iSlotPos + 1 );

	return p;
}

#if !defined( HL_TESTS )
int giBucketHeight, giBucketWidth, giABHeight, giABWidth; // Ammo Bar width and height

HSPRITE ghsprBuckets; // Sprite for top row of weapons menu

DECLARE_MESSAGE( m_Ammo, CurWeapon );  // Current weapon and clip
DECLARE_MESSAGE( m_Ammo, WeaponList ); // new weapon type
DECLARE_MESSAGE( m_Ammo, AmmoX );      // update known ammo type's count
DECLARE_MESSAGE( m_Ammo, AmmoPickup ); // flashes an ammo pickup record
DECLARE_MESSAGE( m_Ammo, WeapPickup ); // flashes a weapon pickup record
DECLARE_MESSAGE( m_Ammo, HideWeapon ); // hides the weapon, ammo, and crosshair displays temporarily
DECLARE_MESSAGE( m_Ammo, ItemPickup );

DECLARE_COMMAND( m_Ammo, Slot1 );
DECLARE_COMMAND( m_Ammo, Slot2 );
DECLARE_COMMAND( m_Ammo, Slot3 );
DECLARE_COMMAND( m_Ammo, Slot4 );
DECLARE_COMMAND( m_Ammo, Slot5 );
DECLARE_COMMAND( m_Ammo, Slot6 );
DECLARE_COMMAND( m_Ammo, Slot7 );
DECLARE_COMMAND( m_Ammo, Slot8 );
DECLARE_COMMAND( m_Ammo, Slot9 );
DECLARE_COMMAND( m_Ammo, Slot10 );
DECLARE_COMMAND( m_Ammo, Close );
DECLARE_COMMAND( m_Ammo, NextWeapon );
DECLARE_COMMAND( m_Ammo, PrevWeapon );

// width of ammo fonts
#define AMMO_SMALL_WIDTH 10
#define AMMO_LARGE_WIDTH 20

int CHudAmmo::Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( CurWeapon );
	HOOK_MESSAGE( WeaponList );
	HOOK_MESSAGE( AmmoPickup );
	HOOK_MESSAGE( WeapPickup );
	HOOK_MESSAGE( ItemPickup );
	HOOK_MESSAGE( HideWeapon );
	HOOK_MESSAGE( AmmoX );

	HOOK_COMMAND( "slot1", Slot1 );
	HOOK_COMMAND( "slot2", Slot2 );
	HOOK_COMMAND( "slot3", Slot3 );
	HOOK_COMMAND( "slot4", Slot4 );
	HOOK_COMMAND( "slot5", Slot5 );
	HOOK_COMMAND( "slot6", Slot6 );
	HOOK_COMMAND( "slot7", Slot7 );
	HOOK_COMMAND( "slot8", Slot8 );
	HOOK_COMMAND( "slot9", Slot9 );
	HOOK_COMMAND( "slot10", Slot10 );
	HOOK_COMMAND( "cancelselect", Close );
	HOOK_COMMAND( "invnext", NextWeapon );
	HOOK_COMMAND( "invprev", PrevWeapon );

	Reset();

	CVAR_CREATE( "hud_drawhistory_time", "5", 0 );
	CVAR_CREATE( "hud_fastswitch", "0", FCVAR_ARCHIVE ); // controls whether or not weapons can be selected in one keypress

	m_iFlags |= HUD_ACTIVE; //!!!

	gWR.Init();
	gHR.Init();

	return 1;
};

void CHudAmmo::Reset( void )
{
	m_fFade = 0;
	m_iFlags |= HUD_ACTIVE; //!!!

	gpActiveSel            = NULL;
	gHUD.m_iHideHUDDisplay = 0;

	gWR.Reset();
	gHR.Reset();
}

int CHudAmmo::VidInit( void )
{
	// Load sprites for buckets (top row of weapon menu)
	m_HUD_bucket0   = gHUD.GetSpriteIndex( "bucket1" );
	m_HUD_selection = gHUD.GetSpriteIndex( "selection" );

	for ( int i = 0; i < MAX_WEAPON_SLOTS; i++ )
	{
		char szBucket[16];
		sprintf( szBucket, "bucket%d", i + 1 );
		m_HUD_buckets[i] = gHUD.GetSpriteIndex( szBucket );
		if ( i < 5 && m_HUD_buckets[i] < 0 && m_HUD_bucket0 >= 0 )
		{
			m_HUD_buckets[i] = m_HUD_bucket0 + i;
		}
	}

	ghsprBuckets   = ( m_HUD_bucket0 >= 0 ) ? gHUD.GetSprite( m_HUD_bucket0 ) : 0;
	giBucketWidth  = ( m_HUD_bucket0 >= 0 ) ? ( gHUD.GetSpriteRect( m_HUD_bucket0 ).right - gHUD.GetSpriteRect( m_HUD_bucket0 ).left ) : 0;
	giBucketHeight = ( m_HUD_bucket0 >= 0 ) ? ( gHUD.GetSpriteRect( m_HUD_bucket0 ).bottom - gHUD.GetSpriteRect( m_HUD_bucket0 ).top ) : 0;

	if ( m_HUD_bucket0 >= 0 )
		gHR.iHistoryGap = max( gHR.iHistoryGap, gHUD.GetSpriteRect( m_HUD_bucket0 ).bottom - gHUD.GetSpriteRect( m_HUD_bucket0 ).top );

	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	int nScale = 1;

#if !defined( _TFC )
	if ( ScreenWidth > 2560 && ScreenHeight > 1600 )
		nScale = 4;
	else if ( ScreenWidth >= 1280 && ScreenHeight > 720 )
		nScale = 3;
	else
#endif
	    if ( ScreenWidth >= 640 )
		nScale = 2;

	giABWidth  = 10 * nScale;
	giABHeight = 2 * nScale;

	return 1;
}

int CHudAmmo::MsgFunc_AmmoX( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	gWR.SetAmmo( iIndex, abs( iCount ) );

	return 1;
}

int CHudAmmo::MsgFunc_AmmoPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	// Add ammo to the history
	gHR.AddToHistory( HISTSLOT_AMMO, iIndex, abs( iCount ) );

	return 1;
}

int CHudAmmo::MsgFunc_WeapPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iIndex = READ_BYTE();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_WEAP, iIndex );

	return 1;
}

int CHudAmmo::MsgFunc_ItemPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	const char *szName = READ_STRING();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_ITEM, szName );

	return 1;
}

int CHudAmmo::MsgFunc_HideWeapon( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	gHUD.m_iHideHUDDisplay = READ_BYTE();

	if ( gEngfuncs.IsSpectateOnly() )
		return 1;

	if ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) )
	{
		static wrect_t nullrc;
		gpActiveSel = NULL;
		SetCrosshair( 0, nullrc, 0, 0, 0 );
	}
	else
	{
		if ( m_pWeapon )
			SetCrosshair( m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255 );
	}

	return 1;
}

int CHudAmmo::MsgFunc_CurWeapon( const char *pszName, int iSize, void *pbuf )
{
	static wrect_t nullrc;
	int fOnTarget = FALSE;

	BEGIN_READ( pbuf, iSize );

	int iState = READ_BYTE();
	int iId    = READ_CHAR();
	int iClip  = READ_CHAR();

	if ( iState > 1 )
	{
		fOnTarget = TRUE;
	}

	if ( iId < 1 )
	{
		SetCrosshair( 0, nullrc, 0, 0, 0 );
		return 0;
	}

	if ( g_iUser1 != OBS_IN_EYE )
	{
		if ( ( iId == -1 ) && ( iClip == -1 ) )
		{
			gHUD.m_fPlayerDead = TRUE;
			gpActiveSel        = NULL;
			return 1;
		}
		gHUD.m_fPlayerDead = FALSE;
	}

	WEAPON *pWeapon = gWR.GetWeapon( iId );

	if ( !pWeapon )
		return 0;

	if ( iClip < -1 )
		pWeapon->iClip = abs( iClip );
	else
		pWeapon->iClip = iClip;

	if ( iState == 0 )
		return 1;

	m_pWeapon = pWeapon;

	if ( gHUD.m_iFOV >= 90 )
	{
		if ( fOnTarget && m_pWeapon->hAutoaim )
			SetCrosshair( m_pWeapon->hAutoaim, m_pWeapon->rcAutoaim, 255, 255, 255 );
		else
			SetCrosshair( m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255 );
	}
	else
	{
		if ( fOnTarget && m_pWeapon->hZoomedAutoaim )
			SetCrosshair( m_pWeapon->hZoomedAutoaim, m_pWeapon->rcZoomedAutoaim, 255, 255, 255 );
		else
			SetCrosshair( m_pWeapon->hZoomedCrosshair, m_pWeapon->rcZoomedCrosshair, 255, 255, 255 );
	}

	m_fFade = 200.0f;
	m_iFlags |= HUD_ACTIVE;

	return 1;
}

int CHudAmmo::MsgFunc_WeaponList( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	WEAPON Weapon;

	strncpy( Weapon.szName, READ_STRING(), MAX_WEAPON_NAME );
	Weapon.szName[sizeof( Weapon.szName ) - 1] = '\0';

	Weapon.iAmmoType = (int)READ_CHAR();

	Weapon.iMax1 = READ_BYTE();
	if ( Weapon.iMax1 == 255 )
		Weapon.iMax1 = -1;

	Weapon.iAmmo2Type = READ_CHAR();
	Weapon.iMax2      = READ_BYTE();
	if ( Weapon.iMax2 == 255 )
		Weapon.iMax2 = -1;

	Weapon.iSlot    = READ_CHAR();
	Weapon.iSlotPos = READ_CHAR();
	Weapon.iId      = READ_CHAR();
	Weapon.iFlags   = READ_BYTE();
	Weapon.iClip    = 0;

	if ( Weapon.iId < 0 || Weapon.iId >= MAX_WEAPONS )
		return 0;

	if ( Weapon.iSlot < 0 || Weapon.iSlot >= MAX_WEAPON_SLOTS + 1 )
		return 0;

	if ( Weapon.iSlotPos < 0 || Weapon.iSlotPos >= MAX_WEAPON_POSITIONS + 1 )
		return 0;

	if ( Weapon.iAmmoType < -1 || Weapon.iAmmoType >= MAX_AMMO_TYPES )
		return 0;

	if ( Weapon.iAmmo2Type < -1 || Weapon.iAmmo2Type >= MAX_AMMO_TYPES )
		return 0;

	if ( Weapon.iAmmoType >= 0 && Weapon.iMax1 == 0 )
		return 0;

	if ( Weapon.iAmmo2Type >= 0 && Weapon.iMax2 == 0 )
		return 0;

	gWR.AddWeapon( &Weapon );

	return 1;
}

int CHudAmmo::Draw( float flTime )
{
	int a, x, y, r, g, b;
	int AmmoWidth;

	if ( !( gHUD.m_iWeaponBits & ( 1 << ( WEAPON_SUIT ) ) ) )
		return 1;

	if ( ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) ) )
		return 1;

	// Draw Weapon Menu
	DrawWList( flTime );

	// Draw ammo pickup history
	gHR.DrawAmmoHistory( flTime );

	if ( !( m_iFlags & HUD_ACTIVE ) )
		return 0;

	if ( !m_pWeapon )
		return 0;

	WEAPON *pw = m_pWeapon;

	if ( ( pw->iAmmoType < 0 ) && ( pw->iAmmo2Type < 0 ) )
		return 0;

	int iFlags = DHN_DRAWZERO;

	AmmoWidth = gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).right - gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).left;

	a = max< int >( MIN_ALPHA, m_fFade );

	if ( m_fFade > 0 )
		m_fFade -= ( gHUD.m_flTimeDelta * 20 );

	UnpackRGB( r, g, b, RGB_YELLOWISH );

	ScaleColors( r, g, b, a );

	y = ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2;
	y += (int)( gHUD.m_iFontHeight * 0.2f );

	if ( m_pWeapon->iAmmoType > 0 )
	{
		int iIconWidth = m_pWeapon->rcAmmo.right - m_pWeapon->rcAmmo.left;

		if ( pw->iClip >= 0 )
		{
			x = ScreenWidth - ( 8 * AmmoWidth ) - iIconWidth;
			x = gHUD.DrawHudNumber( x, y, iFlags | DHN_3DIGITS, pw->iClip, r, g, b );

			wrect_t rc;
			rc.top    = 0;
			rc.left   = 0;
			rc.right  = AmmoWidth;
			rc.bottom = 100;

			int iBarWidth = AmmoWidth / 10;

			x += AmmoWidth / 2;

			UnpackRGB( r, g, b, RGB_YELLOWISH );

			FillRGBA( x, y, iBarWidth, gHUD.m_iFontHeight, r, g, b, a );

			x += iBarWidth + AmmoWidth / 2;

			ScaleColors( r, g, b, a );
			x = gHUD.DrawHudNumber( x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo( pw->iAmmoType ), r, g, b );
		}
		else
		{
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber( x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo( pw->iAmmoType ), r, g, b );
		}

		int iOffset = ( m_pWeapon->rcAmmo.bottom - m_pWeapon->rcAmmo.top ) / 8;
		SPR_Set( m_pWeapon->hAmmo, r, g, b );
		SPR_DrawAdditive( 0, x, y - iOffset, &m_pWeapon->rcAmmo );
	}

	if ( pw->iAmmo2Type > 0 )
	{
		int iIconWidth = m_pWeapon->rcAmmo2.right - m_pWeapon->rcAmmo2.left;

		if ( ( pw->iAmmo2Type != 0 ) && ( gWR.CountAmmo( pw->iAmmo2Type ) > 0 ) )
		{
			y -= gHUD.m_iFontHeight + gHUD.m_iFontHeight / 4;
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber( x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo( pw->iAmmo2Type ), r, g, b );

			SPR_Set( m_pWeapon->hAmmo2, r, g, b );
			int iOffset = ( m_pWeapon->rcAmmo2.bottom - m_pWeapon->rcAmmo2.top ) / 8;
			SPR_DrawAdditive( 0, x, y - iOffset, &m_pWeapon->rcAmmo2 );
		}
	}
	return 1;
}
#endif // !defined( HL_TESTS )

client_sprite_t *GetSpriteList( client_sprite_t *pList, const char *psz, int iRes, int iCount )
{
	if ( !pList )
		return NULL;

	int i              = iCount;
	client_sprite_t *p = pList;

	while ( i-- )
	{
		if ( ( p->iRes == iRes ) && ( !strcmp( psz, p->szName ) ) )
			return p;
		p++;
	}

	return NULL;
}

#if !defined( HL_TESTS )
//=========================================================
// Secondary Ammo HUD Implementation
//=========================================================

DECLARE_MESSAGE( m_AmmoSecondary, SecAmmoVal );
DECLARE_MESSAGE( m_AmmoSecondary, SecAmmoIcon );

int CHudAmmoSecondary ::Init( void )
{
	HOOK_MESSAGE( SecAmmoVal );
	HOOK_MESSAGE( SecAmmoIcon );

	gHUD.AddHudElem( this );
	m_HUD_ammoicon = 0;

	for ( int i = 0; i < MAX_SEC_AMMO_VALUES; i++ )
		m_iAmmoAmounts[i] = -1; // -1 means don't draw this value

	Reset();

	return 1;
}

void CHudAmmoSecondary ::Reset( void )
{
	m_fFade = 0;
}

int CHudAmmoSecondary ::VidInit( void )
{
	return 1;
}

int CHudAmmoSecondary ::Draw( float flTime )
{
	if ( ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) ) )
		return 1;

	// draw secondary ammo icons above normal ammo readout
	int a, x, y, r, g, b, AmmoWidth;
	UnpackRGB( r, g, b, RGB_YELLOWISH );
	a = max< int >( MIN_ALPHA, m_fFade );
	if ( m_fFade > 0 )
		m_fFade -= ( gHUD.m_flTimeDelta * 20 ); // slowly lower alpha to fade out icons
	ScaleColors( r, g, b, a );

	AmmoWidth = gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).right - gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).left;

	y = ScreenHeight - ( gHUD.m_iFontHeight * 4 ); // this is one font height higher than the weapon ammo values
	x = ScreenWidth - AmmoWidth;

	if ( m_HUD_ammoicon )
	{
		// Draw the ammo icon
		x -= ( gHUD.GetSpriteRect( m_HUD_ammoicon ).right - gHUD.GetSpriteRect( m_HUD_ammoicon ).left );
		y -= ( gHUD.GetSpriteRect( m_HUD_ammoicon ).top - gHUD.GetSpriteRect( m_HUD_ammoicon ).bottom );

		SPR_Set( gHUD.GetSprite( m_HUD_ammoicon ), r, g, b );
		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( m_HUD_ammoicon ) );
	}
	else
	{ // move the cursor by the '0' char instead, since we don't have an icon to work with
		x -= AmmoWidth;
		y -= ( gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).top - gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).bottom );
	}

	// draw the ammo counts, in reverse order, from right to left
	for ( int i = MAX_SEC_AMMO_VALUES - 1; i >= 0; i-- )
	{
		if ( m_iAmmoAmounts[i] < 0 )
			continue; // negative ammo amounts imply that they shouldn't be drawn

		// half a char gap between the ammo number and the previous pic
		x -= ( AmmoWidth / 2 );

		// draw the number, right-aligned
		x -= ( gHUD.GetNumWidth( m_iAmmoAmounts[i], DHN_DRAWZERO ) * AmmoWidth );
		gHUD.DrawHudNumber( x, y, DHN_DRAWZERO, m_iAmmoAmounts[i], r, g, b );

		if ( i != 0 )
		{
			// draw the divider bar
			x -= ( AmmoWidth / 2 );
			FillRGBA( x, y, ( AmmoWidth / 10 ), gHUD.m_iFontHeight, r, g, b, a );
		}
	}

	return 1;
}

// Message handler for Secondary Ammo Value
// accepts one value:
//		string:  sprite name
int CHudAmmoSecondary ::MsgFunc_SecAmmoIcon( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_HUD_ammoicon = gHUD.GetSpriteIndex( READ_STRING() );

	return 1;
}

// Message handler for Secondary Ammo Icon
// Sets an ammo value
// takes two values:
//		byte:  ammo index
//		byte:  ammo value
int CHudAmmoSecondary ::MsgFunc_SecAmmoVal( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int index = READ_BYTE();
	if ( index < 0 || index >= MAX_SEC_AMMO_VALUES )
		return 1;

	m_iAmmoAmounts[index] = READ_BYTE();
	m_iFlags |= HUD_ACTIVE;

	// check to see if there is anything left to draw
	int count = 0;
	for ( int i = 0; i < MAX_SEC_AMMO_VALUES; i++ )
	{
		count += max( 0, m_iAmmoAmounts[i] );
	}

	if ( count == 0 )
	{ // the ammo fields are all empty, so turn off this hud area
		m_iFlags &= ~HUD_ACTIVE;
		return 1;
	}

	// make the icons light up
	m_fFade = 200.0f;

	return 1;
}

//=========================================================
// Ammo & Item Pickup History HUD Implementation
//=========================================================

HistoryResource gHR;

#define AMMO_PICKUP_GAP ( gHR.iHistoryGap + 5 )
#define AMMO_PICKUP_PICK_HEIGHT ( 32 + ( gHR.iHistoryGap * 2 ) )
#define AMMO_PICKUP_HEIGHT_MAX ( ScreenHeight - 100 )

#define MAX_ITEM_NAME 32
int HISTORY_DRAW_TIME = 5;

// keep a list of items
struct ITEM_INFO
{
	char szName[MAX_ITEM_NAME];
	HSPRITE spr;
	wrect_t rect;
};

void HistoryResource ::AddToHistory( int iType, int iId, int iCount )
{
	if ( iType == HISTSLOT_AMMO && !iCount )
		return; // no amount, so don't add

	if ( ( ( ( AMMO_PICKUP_GAP * iCurrentHistorySlot ) + AMMO_PICKUP_PICK_HEIGHT ) > AMMO_PICKUP_HEIGHT_MAX ) || ( iCurrentHistorySlot >= MAX_HISTORY ) )
	{ // the pic would have to be drawn too high
		// so start from the bottom
		iCurrentHistorySlot = 0;
	}

	HIST_ITEM *freeslot = &rgAmmoHistory[iCurrentHistorySlot++]; // default to just writing to the first slot
	HISTORY_DRAW_TIME   = CVAR_GET_FLOAT( "hud_drawhistory_time" );

	freeslot->type        = iType;
	freeslot->iId         = iId;
	freeslot->iCount      = iCount;
	freeslot->DisplayTime = gHUD.m_flTime + HISTORY_DRAW_TIME;
}

void HistoryResource ::AddToHistory( int iType, const char *szName, int iCount )
{
	if ( iType != HISTSLOT_ITEM )
		return;

	if ( ( ( ( AMMO_PICKUP_GAP * iCurrentHistorySlot ) + AMMO_PICKUP_PICK_HEIGHT ) > AMMO_PICKUP_HEIGHT_MAX ) || ( iCurrentHistorySlot >= MAX_HISTORY ) )
	{ // the pic would have to be drawn too high
		// so start from the bottom
		iCurrentHistorySlot = 0;
	}

	HIST_ITEM *freeslot = &rgAmmoHistory[iCurrentHistorySlot++]; // default to just writing to the first slot

	// I am really unhappy with all the code in this file

	int i = gHUD.GetSpriteIndex( szName );
	if ( i == -1 )
		return; // unknown sprite name, don't add it to history

	freeslot->iId    = i;
	freeslot->type   = iType;
	freeslot->iCount = iCount;

	HISTORY_DRAW_TIME     = CVAR_GET_FLOAT( "hud_drawhistory_time" );
	freeslot->DisplayTime = gHUD.m_flTime + HISTORY_DRAW_TIME;
}

void HistoryResource ::CheckClearHistory( void )
{
	for ( int i = 0; i < MAX_HISTORY; i++ )
	{
		if ( rgAmmoHistory[i].type )
			return;
	}

	iCurrentHistorySlot = 0;
}

//
// Draw Ammo pickup history
//
int HistoryResource ::DrawAmmoHistory( float flTime )
{
	for ( int i = 0; i < MAX_HISTORY; i++ )
	{
		if ( rgAmmoHistory[i].type )
		{
			rgAmmoHistory[i].DisplayTime = min( rgAmmoHistory[i].DisplayTime, gHUD.m_flTime + HISTORY_DRAW_TIME );

			if ( rgAmmoHistory[i].DisplayTime <= flTime )
			{ // pic drawing time has expired
				memset( &rgAmmoHistory[i], 0, sizeof( HIST_ITEM ) );
				CheckClearHistory();
			}
			else if ( rgAmmoHistory[i].type == HISTSLOT_AMMO )
			{
				wrect_t rcPic;
				HSPRITE *spr = gWR.GetAmmoPicFromWeapon( rgAmmoHistory[i].iId, rcPic );

				int r, g, b;
				UnpackRGB( r, g, b, RGB_YELLOWISH );
				float scale = ( rgAmmoHistory[i].DisplayTime - flTime ) * 80;
				ScaleColors( r, g, b, min< int >( scale, 255 ) );

				// Draw the pic
				int ypos = ScreenHeight - ( AMMO_PICKUP_PICK_HEIGHT + ( AMMO_PICKUP_GAP * i ) );
				int xpos = ScreenWidth - ( rcPic.right - rcPic.left ) - 4;
				if ( spr && *spr ) // weapon isn't loaded yet so just don't draw the pic
				{                  // the dll has to make sure it has sent info the weapons you need
					SPR_Set( *spr, r, g, b );
					SPR_DrawAdditive( 0, xpos, ypos, &rcPic );
				}

				// Draw the number
				gHUD.DrawHudNumberString( xpos - 10, ypos, xpos - 100, rgAmmoHistory[i].iCount, r, g, b );
			}
			else if ( rgAmmoHistory[i].type == HISTSLOT_WEAP )
			{
				WEAPON *weap = gWR.GetWeapon( rgAmmoHistory[i].iId );

				if ( !weap )
					return 1; // we don't know about the weapon yet, so don't draw anything

				int r, g, b;
				UnpackRGB( r, g, b, RGB_YELLOWISH );

				if ( !gWR.HasAmmo( weap ) )
					UnpackRGB( r, g, b, RGB_REDISH ); // if the weapon doesn't have ammo, display it as red

				float scale = ( rgAmmoHistory[i].DisplayTime - flTime ) * 80;
				ScaleColors( r, g, b, min< int >( scale, 255 ) );

				int ypos = ScreenHeight - ( AMMO_PICKUP_PICK_HEIGHT + ( AMMO_PICKUP_GAP * i ) );
				int xpos = ScreenWidth - ( weap->rcInactive.right - weap->rcInactive.left );
				SPR_Set( weap->hInactive, r, g, b );
				SPR_DrawAdditive( 0, xpos, ypos, &weap->rcInactive );
			}
			else if ( rgAmmoHistory[i].type == HISTSLOT_ITEM )
			{
				int r, g, b;

				if ( !rgAmmoHistory[i].iId )
					continue; // sprite not loaded

				wrect_t rect = gHUD.GetSpriteRect( rgAmmoHistory[i].iId );

				UnpackRGB( r, g, b, RGB_YELLOWISH );
				float scale = ( rgAmmoHistory[i].DisplayTime - flTime ) * 80;
				ScaleColors( r, g, b, min< int >( scale, 255 ) );

				int ypos = ScreenHeight - ( AMMO_PICKUP_PICK_HEIGHT + ( AMMO_PICKUP_GAP * i ) );
				int xpos = ScreenWidth - ( rect.right - rect.left ) - 10;

				SPR_Set( gHUD.GetSprite( rgAmmoHistory[i].iId ), r, g, b );
				SPR_DrawAdditive( 0, xpos, ypos, &rect );
			}
		}
	}

	return 1;
}
#endif // !defined( HL_TESTS )

