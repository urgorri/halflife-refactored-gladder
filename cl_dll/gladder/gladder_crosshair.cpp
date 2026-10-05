/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Universal Weapon Crosshair Fallback Implementation (SPEC §7.4, GitHub Issue #36)
 *
 ****/

#include "gladder_crosshair.h"
#include <cstdio>
#include <cstring>

#if defined( HL_TESTS )
#include "tests/mock_client_engine.h"
#include "cl_dll/hud/hud_ammo.h"

#define SPR_GetList (*g_mockClientEngineFuncs.pfnSPR_GetList)
#define SPR_Load (*g_mockClientEngineFuncs.pfnSPR_Load)
#else
#include "hud.h"
#include "cl_util.h"
#include "hud/hud_ammo.h"
#endif

extern client_sprite_t *GetSpriteList( client_sprite_t *pList, const char *psz, int iRes, int iCount );

void GladderCrosshairFallback( WEAPON *pWeapon, int iRes )
{
	if ( !pWeapon )
		return;

	// Do not override if weapon already has a valid crosshair (custom reticles, scopes)
	if ( pWeapon->hCrosshair != 0 )
		return;

	// 1. Attempt to load crosshair from standard weapon_9mmhandgun sprite list
	int count = 0;
	client_sprite_t *pList = SPR_GetList( "sprites/weapon_9mmhandgun.txt", &count );
	if ( pList )
	{
		client_sprite_t *p = GetSpriteList( pList, "crosshair", iRes, count );
		if ( p )
		{
			char sz[128];
			snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
			pWeapon->hCrosshair = SPR_Load( sz );
			pWeapon->rcCrosshair = p->rc;
			return;
		}
	}

	// 2. Direct fallback to standard crosshair sprite if sprite list didn't resolve
	pWeapon->hCrosshair = SPR_Load( "sprites/crosshair.spr" );
	pWeapon->rcCrosshair.left = 0;
	pWeapon->rcCrosshair.top = 0;
	pWeapon->rcCrosshair.right = 24;
	pWeapon->rcCrosshair.bottom = 24;
}

void Gladder_InitCrosshairFallback( void )
{
	WeaponsResource::SetCrosshairFallbackHandler( GladderCrosshairFallback );
}
