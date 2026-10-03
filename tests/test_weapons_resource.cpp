/***
 *
 *	Behavioral Equivalence Verification - WeaponsResource Unit Tests
 *	Verifies WeaponsResource crosshair fallback handler extensibility (#174)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "tests/mock_client_engine.h"
#include "cl_dll/hud/hud_ammo.h"

extern int ScreenWidth;
extern int ScreenHeight;

// Test state recorder for fallback invocations
static int s_fallbackCallCount = 0;
static WEAPON *s_pLastWeapon = nullptr;
static int s_lastResolution = 0;

static void MockCrosshairFallback( WEAPON *pWeapon, int iResolution )
{
	s_fallbackCallCount++;
	s_pLastWeapon   = pWeapon;
	s_lastResolution = iResolution;

	if ( pWeapon )
	{
		pWeapon->hCrosshair        = (HSPRITE)0x1337;
		pWeapon->rcCrosshair.left   = 10;
		pWeapon->rcCrosshair.top    = 20;
		pWeapon->rcCrosshair.right  = 30;
		pWeapon->rcCrosshair.bottom = 40;
	}
}

static void ResetTestState( void )
{
	s_fallbackCallCount = 0;
	s_pLastWeapon       = nullptr;
	s_lastResolution    = 0;
	WeaponsResource::SetCrosshairFallbackHandler( nullptr );
	ResetMockClientEngine();
	ScreenWidth  = 640;
	ScreenHeight = 480;
}

TEST_CASE( "WeaponsResource: Crosshair Fallback Registration Lifecycle (#174)", "[hud][weapons_resource]" )
{
	ResetTestState();

	// By default, no handler is registered
	CHECK( WeaponsResource::GetCrosshairFallbackHandler() == nullptr );

	// Register custom handler
	WeaponsResource::SetCrosshairFallbackHandler( MockCrosshairFallback );
	CHECK( WeaponsResource::GetCrosshairFallbackHandler() == MockCrosshairFallback );

	// Deregister handler
	WeaponsResource::SetCrosshairFallbackHandler( nullptr );
	CHECK( WeaponsResource::GetCrosshairFallbackHandler() == nullptr );
}

TEST_CASE( "WeaponsResource: Behavioral Equivalence - Default NULL Crosshair without Handler (#174)", "[hud][weapons_resource]" )
{
	ResetTestState();
	InitMockClientEngine();

	// Ensure no handler registered (pure vanilla state)
	REQUIRE( WeaponsResource::GetCrosshairFallbackHandler() == nullptr );

	// Mock sprite list without any "crosshair" entry (like crowbar or grenade)
	client_sprite_t weaponSpritesNoCrosshair[] = {
		{ "weapon", "640hud1", 1, 640, { 0, 0, 170, 45 } },
		{ "weapon_s", "640hud1", 2, 640, { 0, 45, 170, 90 } },
		{ "ammo", "640hud1", 3, 640, { 0, 90, 32, 122 } },
	};
	SetMockSpriteList( weaponSpritesNoCrosshair, 3 );

	WeaponsResource wr;
	wr.Init();

	WEAPON crowbar;
	memset( &crowbar, 0, sizeof( crowbar ) );
	strncpy( crowbar.szName, "weapon_crowbar", sizeof( crowbar.szName ) - 1 );
	crowbar.iId = 1;

	wr.LoadWeaponSprites( &crowbar );

	// Vanilla Half-Life contract: weapons without crosshair sprites have NULL crosshairs
	CHECK( crowbar.hCrosshair == 0 );
	CHECK( crowbar.rcCrosshair.left == 0 );
	CHECK( crowbar.rcCrosshair.top == 0 );
	CHECK( crowbar.rcCrosshair.right == 0 );
	CHECK( crowbar.rcCrosshair.bottom == 0 );
	CHECK( crowbar.hZoomedCrosshair == 0 );

	// Test grenade as well
	WEAPON handgrenade;
	memset( &handgrenade, 0, sizeof( handgrenade ) );
	strncpy( handgrenade.szName, "weapon_handgrenade", sizeof( handgrenade.szName ) - 1 );
	handgrenade.iId = 2;

	wr.LoadWeaponSprites( &handgrenade );

	CHECK( handgrenade.hCrosshair == 0 );
	CHECK( handgrenade.hZoomedCrosshair == 0 );
	CHECK( s_fallbackCallCount == 0 );

	ResetTestState();
}

TEST_CASE( "WeaponsResource: Crosshair Fallback Invocation when Crosshair Missing (#174)", "[hud][weapons_resource]" )
{
	ResetTestState();
	InitMockClientEngine();

	WeaponsResource::SetCrosshairFallbackHandler( MockCrosshairFallback );
	REQUIRE( WeaponsResource::GetCrosshairFallbackHandler() == MockCrosshairFallback );

	// Sprite list missing "crosshair"
	client_sprite_t weaponSpritesNoCrosshair[] = {
		{ "weapon", "640hud1", 1, 640, { 0, 0, 170, 45 } },
		{ "weapon_s", "640hud1", 2, 640, { 0, 45, 170, 90 } },
	};
	SetMockSpriteList( weaponSpritesNoCrosshair, 2 );

	WeaponsResource wr;
	wr.Init();

	WEAPON weapon;
	memset( &weapon, 0, sizeof( weapon ) );
	strncpy( weapon.szName, "weapon_custom_melee", sizeof( weapon.szName ) - 1 );
	weapon.iId = 5;

	wr.LoadWeaponSprites( &weapon );

	// Fallback handler MUST have been invoked exactly once
	CHECK( s_fallbackCallCount == 1 );
	CHECK( s_pLastWeapon == &weapon );
	CHECK( s_lastResolution == 640 );

	// Weapon crosshair should now reflect fallback values
	CHECK( weapon.hCrosshair == (HSPRITE)0x1337 );
	CHECK( weapon.rcCrosshair.left == 10 );
	CHECK( weapon.rcCrosshair.top == 20 );
	CHECK( weapon.rcCrosshair.right == 30 );
	CHECK( weapon.rcCrosshair.bottom == 40 );

	// Zoomed crosshair defaults to weapon crosshair when zoom sprite is absent
	CHECK( weapon.hZoomedCrosshair == (HSPRITE)0x1337 );
	CHECK( weapon.rcZoomedCrosshair.left == 10 );
	CHECK( weapon.rcZoomedCrosshair.top == 20 );
	CHECK( weapon.rcZoomedCrosshair.right == 30 );
	CHECK( weapon.rcZoomedCrosshair.bottom == 40 );

	ResetTestState();
}

TEST_CASE( "WeaponsResource: Crosshair Fallback Bypassed when Crosshair Exists (#174)", "[hud][weapons_resource]" )
{
	ResetTestState();
	InitMockClientEngine();

	WeaponsResource::SetCrosshairFallbackHandler( MockCrosshairFallback );

	// Sprite list WITH an explicit "crosshair" entry
	client_sprite_t weaponSpritesWithCrosshair[] = {
		{ "weapon", "640hud1", 1, 640, { 0, 170, 0, 45 } },
		{ "crosshair", "640hud2", 2, 640, { 100, 124, 200, 224 } },
	};
	SetMockSpriteList( weaponSpritesWithCrosshair, 2 );

	WeaponsResource wr;
	wr.Init();

	WEAPON glock;
	memset( &glock, 0, sizeof( glock ) );
	strncpy( glock.szName, "weapon_9mmhandgun", sizeof( glock.szName ) - 1 );
	glock.iId = 3;

	wr.LoadWeaponSprites( &glock );

	// Fallback handler must NOT be invoked because crosshair was present in sprite list
	CHECK( s_fallbackCallCount == 0 );
	CHECK( glock.hCrosshair != 0 );
	CHECK( glock.hCrosshair != (HSPRITE)0x1337 );
	CHECK( glock.rcCrosshair.left == 100 );
	CHECK( glock.rcCrosshair.right == 124 );
	CHECK( glock.rcCrosshair.top == 200 );
	CHECK( glock.rcCrosshair.bottom == 224 );

	ResetTestState();
}

TEST_CASE( "WeaponsResource: Resolution Forwarding to Fallback Handler (#174)", "[hud][weapons_resource]" )
{
	ResetTestState();
	InitMockClientEngine();

	WeaponsResource::SetCrosshairFallbackHandler( MockCrosshairFallback );

	client_sprite_t weaponSprites[] = {
		{ "weapon", "hud", 1, 640, { 0, 0, 10, 10 } },
		{ "weapon", "hud", 1, 320, { 0, 0, 10, 10 } },
		{ "weapon", "hud", 1, 1280, { 0, 0, 10, 10 } },
		{ "weapon", "hud", 1, 2560, { 0, 0, 10, 10 } },
	};
	SetMockSpriteList( weaponSprites, 4 );

	WeaponsResource wr;
	wr.Init();

	WEAPON weapon;
	memset( &weapon, 0, sizeof( weapon ) );
	strncpy( weapon.szName, "weapon_crowbar", sizeof( weapon.szName ) - 1 );
	weapon.iId = 1;

	// Test 320 res
	ScreenWidth  = 320;
	ScreenHeight = 240;
	s_fallbackCallCount = 0;
	wr.LoadWeaponSprites( &weapon );
	CHECK( s_fallbackCallCount == 1 );
	CHECK( s_lastResolution == 320 );

	// Test 640 res
	ScreenWidth  = 800;
	ScreenHeight = 600;
	s_fallbackCallCount = 0;
	wr.LoadWeaponSprites( &weapon );
	CHECK( s_fallbackCallCount == 1 );
	CHECK( s_lastResolution == 640 );

	// Test 1280 res
	ScreenWidth  = 1920;
	ScreenHeight = 1080;
	s_fallbackCallCount = 0;
	wr.LoadWeaponSprites( &weapon );
	CHECK( s_fallbackCallCount == 1 );
	CHECK( s_lastResolution == 1280 );

	// Test 2560 res
	ScreenWidth  = 3840;
	ScreenHeight = 2160;
	s_fallbackCallCount = 0;
	wr.LoadWeaponSprites( &weapon );
	CHECK( s_fallbackCallCount == 1 );
	CHECK( s_lastResolution == 2560 );

	ResetTestState();
}
