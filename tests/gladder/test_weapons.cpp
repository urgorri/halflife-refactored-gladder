/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Extended Opposing Force Arsenal (SPEC §7.1, Issue #13)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "tests/mock_engine.h"
#include "dlls/gladder/weapons/gladder_weapons.h"
#include "dlls/weapons/weapon_registry.h"

TEST_CASE( "GladderWeapons: Slot ID bounds and GoldSrc 32-bit compliance (SPEC §7.1)", "[gladder][weapons]" )
{
	CHECK( WEAPON_PIPEWRENCH == 16 );
	CHECK( WEAPON_KNIFE == 17 );
	CHECK( WEAPON_EAGLE == 18 );
	CHECK( WEAPON_SNIPER == 19 );
	CHECK( WEAPON_M249 == 20 );

	// All weapon IDs must reside strictly under WEAPON_SUIT (31) and MAX_WEAPONS (32)
	CHECK( WEAPON_PIPEWRENCH < 31 );
	CHECK( WEAPON_KNIFE < 31 );
	CHECK( WEAPON_EAGLE < 31 );
	CHECK( WEAPON_SNIPER < 31 );
	CHECK( WEAPON_M249 < 31 );
}

TEST_CASE( "GladderWeapons: Ammo capacities and clip sizes", "[gladder][weapons]" )
{
	CHECK( _762_MAX_CARRY == 30 );
	CHECK( _556_MAX_CARRY == 200 );

	CHECK( EAGLE_MAX_CLIP == 7 );
	CHECK( SNIPER_MAX_CLIP == 5 );
	CHECK( M249_MAX_CLIP == 50 );

	CHECK( AMMO_SNIPER_GIVE == 15 );
	CHECK( AMMO_M249_GIVE == 50 );
}

TEST_CASE( "GladderWeapons: Damage balance constants", "[gladder][weapons]" )
{
	CHECK( GLADDER_DMG_PIPEWRENCH == 35.0f );
	CHECK( GLADDER_DMG_KNIFE == 20.0f );
	CHECK( GLADDER_DMG_KNIFE_BACK == 60.0f );
	CHECK( GLADDER_DMG_EAGLE == 65.0f );
	CHECK( GLADDER_DMG_SNIPER == 100.0f );
	CHECK( GLADDER_DMG_M249 == 16.0f );
}

TEST_CASE( "GladderWeapons: WeaponRegistry registration of OpFor arsenal", "[gladder][weapons]" )
{
	WeaponDescriptor dKnife = { "weapon_knife", 0, nullptr };
	WeaponDescriptor dWrench = { "weapon_pipewrench", 0, nullptr };
	WeaponDescriptor dEagle = { "weapon_eagle", 0, nullptr };
	WeaponDescriptor dSniper = { "weapon_sniper", 0, nullptr };
	WeaponDescriptor dM249 = { "weapon_m249", 0, nullptr };
	WeaponRegistry::Register( dKnife );
	WeaponRegistry::Register( dWrench );
	WeaponRegistry::Register( dEagle );
	WeaponRegistry::Register( dSniper );
	WeaponRegistry::Register( dM249 );

	CHECK( WeaponRegistry::FindByClassname( "weapon_knife" ) != nullptr );
	CHECK( WeaponRegistry::FindByClassname( "weapon_pipewrench" ) != nullptr );
	CHECK( WeaponRegistry::FindByClassname( "weapon_eagle" ) != nullptr );
	CHECK( WeaponRegistry::FindByClassname( "weapon_sniper" ) != nullptr );
	CHECK( WeaponRegistry::FindByClassname( "weapon_m249" ) != nullptr );
}
