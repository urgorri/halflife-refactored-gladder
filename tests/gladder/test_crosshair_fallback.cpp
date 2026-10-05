/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Universal Weapon Crosshair Fallback (SPEC §7.4, GitHub Issue #36)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "tests/mock_client_engine.h"
#include "cl_dll/hud/hud_ammo.h"
#include "cl_dll/gladder/gladder_crosshair.h"

extern int ScreenWidth;
extern int ScreenHeight;

static void ResetCrosshairTestState( void )
{
	WeaponsResource::SetCrosshairFallbackHandler( nullptr );
	ResetMockClientEngine();
	ScreenWidth  = 640;
	ScreenHeight = 480;
}

TEST_CASE( "Gladder Crosshair: Fallback Handler Registration Lifecycle (#36)", "[gladder][hud][crosshair]" )
{
	ResetCrosshairTestState();

	REQUIRE( WeaponsResource::GetCrosshairFallbackHandler() == nullptr );

	Gladder_InitCrosshairFallback();

	REQUIRE( WeaponsResource::GetCrosshairFallbackHandler() == GladderCrosshairFallback );

	ResetCrosshairTestState();
}

TEST_CASE( "Gladder Crosshair: Melee Weapons Receive Handgun Crosshair (#36)", "[gladder][hud][crosshair]" )
{
	ResetCrosshairTestState();
	InitMockClientEngine();
	Gladder_InitCrosshairFallback();

	// Mock weapon_9mmhandgun sprite list with standard crosshair definition { left, right, top, bottom }
	client_sprite_t handgunSprites[] = {
		{ "crosshair", "crosshair", 1, 640, { 0, 24, 0, 24 } },
		{ "autoaim", "640hud1", 2, 640, { 0, 24, 24, 48 } }
	};
	SetMockNamedSpriteList( "sprites/weapon_9mmhandgun.txt", handgunSprites, 2 );

	client_sprite_t meleeSprites[] = {
		{ "weapon", "640hud1", 1, 640, { 0, 16, 0, 16 } },
		{ "weapon_s", "640hud1", 2, 640, { 0, 16, 16, 32 } }
	};
	SetMockNamedSpriteList( "sprites/weapon_crowbar.txt", meleeSprites, 2 );
	SetMockNamedSpriteList( "sprites/weapon_pipewrench.txt", meleeSprites, 2 );
	SetMockNamedSpriteList( "sprites/weapon_knife.txt", meleeSprites, 2 );

	WeaponsResource wr;
	wr.Init();

	const char *meleeWeapons[] = {
		"weapon_crowbar",
		"weapon_pipewrench",
		"weapon_knife"
	};

	for ( int i = 0; i < 3; ++i )
	{
		WEAPON weapon;
		memset( &weapon, 0, sizeof( weapon ) );
		strncpy( weapon.szName, meleeWeapons[i], sizeof( weapon.szName ) - 1 );
		weapon.iId = i + 1;

		wr.LoadWeaponSprites( &weapon );

		// Crosshair must be populated via fallback
		CHECK( weapon.hCrosshair != 0 );
		CHECK( weapon.rcCrosshair.left == 0 );
		CHECK( weapon.rcCrosshair.right == 24 );
		CHECK( weapon.rcCrosshair.top == 0 );
		CHECK( weapon.rcCrosshair.bottom == 24 );
	}

	ResetCrosshairTestState();
}

TEST_CASE( "Gladder Crosshair: Ordnance Weapons Receive Handgun Crosshair (#36)", "[gladder][hud][crosshair]" )
{
	ResetCrosshairTestState();
	InitMockClientEngine();
	Gladder_InitCrosshairFallback();

	client_sprite_t handgunSprites[] = {
		{ "crosshair", "crosshair", 1, 640, { 0, 24, 0, 24 } }
	};
	SetMockNamedSpriteList( "sprites/weapon_9mmhandgun.txt", handgunSprites, 1 );

	client_sprite_t ordnanceSprites[] = {
		{ "weapon", "640hud1", 1, 640, { 0, 16, 0, 16 } }
	};
	SetMockNamedSpriteList( "sprites/weapon_handgrenade.txt", ordnanceSprites, 1 );
	SetMockNamedSpriteList( "sprites/weapon_satchel.txt", ordnanceSprites, 1 );
	SetMockNamedSpriteList( "sprites/weapon_snark.txt", ordnanceSprites, 1 );

	WeaponsResource wr;
	wr.Init();

	const char *ordnanceWeapons[] = {
		"weapon_handgrenade",
		"weapon_satchel",
		"weapon_snark"
	};

	for ( int i = 0; i < 3; ++i )
	{
		WEAPON weapon;
		memset( &weapon, 0, sizeof( weapon ) );
		strncpy( weapon.szName, ordnanceWeapons[i], sizeof( weapon.szName ) - 1 );
		weapon.iId = 10 + i;

		wr.LoadWeaponSprites( &weapon );

		CHECK( weapon.hCrosshair != 0 );
		CHECK( weapon.rcCrosshair.left == 0 );
		CHECK( weapon.rcCrosshair.right == 24 );
		CHECK( weapon.rcCrosshair.top == 0 );
		CHECK( weapon.rcCrosshair.bottom == 24 );
	}

	ResetCrosshairTestState();
}

TEST_CASE( "Gladder Crosshair: Preserves Existing Weapon Crosshairs (#36)", "[gladder][hud][crosshair]" )
{
	ResetCrosshairTestState();
	InitMockClientEngine();
	Gladder_InitCrosshairFallback();

	// Weapon with an explicit unique crosshair
	client_sprite_t customSprites[] = {
		{ "crosshair", "custom_reticle", 1, 640, { 5, 30, 5, 30 } }
	};
	SetMockNamedSpriteList( "sprites/weapon_357.txt", customSprites, 1 );

	WeaponsResource wr;
	wr.Init();

	WEAPON python;
	memset( &python, 0, sizeof( python ) );
	strncpy( python.szName, "weapon_357", sizeof( python.szName ) - 1 );
	python.iId = 5;

	wr.LoadWeaponSprites( &python );

	CHECK( python.hCrosshair != 0 );
	CHECK( python.rcCrosshair.left == 5 );
	CHECK( python.rcCrosshair.right == 30 );
	CHECK( python.rcCrosshair.top == 5 );
	CHECK( python.rcCrosshair.bottom == 30 );

	ResetCrosshairTestState();
}
