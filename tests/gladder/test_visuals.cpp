/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Client Pickup Visual Classification (SPEC §7.2, Issue #12)
 *
 ****/

#include "common/mathlib.h"
#include "common/const.h"
#include "common/entity_state.h"
#include "common/com_model.h"
#include "common/cl_entity.h"
#include "external/catch2/catch_amalgamated.hpp"
#include "cl_dll/gladder/gladder_visuals.h"

TEST_CASE( "GladderVisuals: ClassifyPickupModel categorization (SPEC §7.2)", "[gladder][visuals]" )
{
	SECTION( "Health & Medical Pickups" )
	{
		CHECK( ClassifyPickupModel( "models/w_medkit.mdl" ) == GLADDER_PICKUP_HEALTH );
		CHECK( ClassifyPickupModel( "models/w_healthkit.mdl" ) == GLADDER_PICKUP_HEALTH );
	}

	SECTION( "Armor & HEV Batteries" )
	{
		CHECK( ClassifyPickupModel( "models/w_battery.mdl" ) == GLADDER_PICKUP_HEV );
	}

	SECTION( "Ammunition & Magazines" )
	{
		CHECK( ClassifyPickupModel( "models/w_9mmclip.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/w_9mmARclip.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/w_9mmarclip.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/W_9MMARCLIP.MDL" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/w_argrenade.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/w_357ammo.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/w_shotbox.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/w_crossbow_clip.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/w_rpgammo.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/w_gaussammo.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/w_saw_clip.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/w_m40a1clip.mdl" ) == GLADDER_PICKUP_AMMO );
		CHECK( ClassifyPickupModel( "models/ammo_9mmclip.mdl" ) == GLADDER_PICKUP_AMMO );
	}

	SECTION( "Lambda & Item Collectible" )
	{
		CHECK( ClassifyPickupModel( "models/item_collectible.mdl" ) == GLADDER_PICKUP_LAMBDA );
		CHECK( ClassifyPickupModel( "models/gladder/lambda.mdl" ) == GLADDER_PICKUP_LAMBDA );
		CHECK( ClassifyPickupModel( "models/lambda.mdl" ) == GLADDER_PICKUP_LAMBDA );
	}

	SECTION( "Weapons Arsenal (including dropped ordnance items, SPEC §7.2, Issue #40)" )
	{
		CHECK( ClassifyPickupModel( "models/w_crowbar.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_9mmAR.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_shotgun.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_knife.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_pipe_wrench.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_desert_eagle.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_m40a1.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_saw.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_grenade.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_satchel.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_tripmine.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/v_tripmine.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_sqknest.mdl" ) == GLADDER_PICKUP_WEAPON );
	}

	SECTION( "Live Combat Monsters Excluded from Pickups (SPEC §7.2, Issue #40)" )
	{
		CHECK( ClassifyPickupModel( "models/w_squeak.mdl" ) == GLADDER_PICKUP_NONE );
		CHECK( ClassifyPickupModel( "models/W_SQUEAK.MDL" ) == GLADDER_PICKUP_NONE );

		// Verify AR grenade ammo box remains ammo
		CHECK( ClassifyPickupModel( "models/w_argrenade.mdl" ) == GLADDER_PICKUP_AMMO );
	}

	SECTION( "Non-Pickup Entities" )
	{
		CHECK( ClassifyPickupModel( "models/player.mdl" ) == GLADDER_PICKUP_NONE );
		CHECK( ClassifyPickupModel( "models/headcrab.mdl" ) == GLADDER_PICKUP_NONE );
		CHECK( ClassifyPickupModel( "" ) == GLADDER_PICKUP_NONE );
		CHECK( ClassifyPickupModel( nullptr ) == GLADDER_PICKUP_NONE );
	}
}

TEST_CASE( "GladderVisuals: ShouldApplyPickupVisuals entity state filtering (SPEC §7.2, Issue #40)", "[gladder][visuals]" )
{
	SECTION( "Dropped collectible pickup weapons receive visual modifiers" )
	{
		cl_entity_t ent;
		memset( &ent, 0, sizeof( ent ) );
		ent.curstate.solid = SOLID_TRIGGER;
		ent.curstate.movetype = MOVETYPE_TOSS;
		ent.curstate.owner = 0;

		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_grenade.mdl" ) == true );
		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_satchel.mdl" ) == true );
		CHECK( ShouldApplyPickupVisuals( &ent, "models/v_tripmine.mdl" ) == true );
		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_tripmine.mdl" ) == true );
		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_sqknest.mdl" ) == true );
		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_shotgun.mdl" ) == true );
		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_medkit.mdl" ) == true );
		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_battery.mdl" ) == true );
	}

	SECTION( "Active in-flight thrown grenades do NOT receive visual modifiers" )
	{
		cl_entity_t ent;
		memset( &ent, 0, sizeof( ent ) );
		ent.curstate.solid = SOLID_BBOX;
		ent.curstate.movetype = MOVETYPE_BOUNCE;
		ent.curstate.owner = 1;

		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_grenade.mdl" ) == false );
	}

	SECTION( "Thrown grenades at rest before explosion do NOT receive visual modifiers" )
	{
		cl_entity_t ent;
		memset( &ent, 0, sizeof( ent ) );
		ent.curstate.solid = SOLID_BBOX;
		ent.curstate.movetype = MOVETYPE_NONE;
		ent.curstate.owner = 1;

		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_grenade.mdl" ) == false );
	}

	SECTION( "Active deployed satchel charges do NOT receive visual modifiers" )
	{
		cl_entity_t ent;
		memset( &ent, 0, sizeof( ent ) );
		ent.curstate.solid = SOLID_BBOX;
		ent.curstate.movetype = MOVETYPE_BOUNCE;
		ent.curstate.owner = 1;

		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_satchel.mdl" ) == false );
	}

	SECTION( "Planted tripmines do NOT receive visual modifiers" )
	{
		cl_entity_t ent;
		memset( &ent, 0, sizeof( ent ) );
		ent.curstate.solid = SOLID_NOT;
		ent.curstate.movetype = MOVETYPE_FLY;
		ent.curstate.owner = 1;

		CHECK( ShouldApplyPickupVisuals( &ent, "models/v_tripmine.mdl" ) == false );
		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_tripmine.mdl" ) == false );
	}

	SECTION( "Live snarks in combat do NOT receive visual modifiers regardless of entity state" )
	{
		cl_entity_t ent;
		memset( &ent, 0, sizeof( ent ) );
		ent.curstate.solid = SOLID_TRIGGER;
		ent.curstate.movetype = MOVETYPE_TOSS;
		ent.curstate.owner = 0;

		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_squeak.mdl" ) == false );

		ent.curstate.solid = SOLID_BBOX;
		ent.curstate.movetype = MOVETYPE_BOUNCE;
		ent.curstate.owner = 1;

		CHECK( ShouldApplyPickupVisuals( &ent, "models/w_squeak.mdl" ) == false );
	}

	SECTION( "Null entity fallback safely allows pickup model check" )
	{
		CHECK( ShouldApplyPickupVisuals( nullptr, "models/w_shotgun.mdl" ) == true );
		CHECK( ShouldApplyPickupVisuals( nullptr, "models/w_squeak.mdl" ) == false );
		CHECK( ShouldApplyPickupVisuals( nullptr, "models/player.mdl" ) == false );
	}
}

TEST_CASE( "GladderVisuals: Wall charger qualification for glow shell (SPEC §3, §7.2, Issue #48)", "[gladder][visuals][chargers]" )
{
	cl_entity_t ent;
	memset( &ent, 0, sizeof( ent ) );
	ent.curstate.renderfx = kRenderFxGlowShell;
	ent.curstate.frame = 0; // Active (frame 0)

	SECTION( "Active HealthCharger qualifies for glow shell" )
	{
		ent.curstate.rendercolor.r = 32;
		ent.curstate.rendercolor.g = 255;
		ent.curstate.rendercolor.b = 32;

		CHECK( ShouldApplyChargerGlowShell( &ent, "*1" ) == true );
		CHECK( ShouldApplyChargerGlowShell( &ent, "*14" ) == true );
	}

	SECTION( "Active HEV suit charger qualifies for glow shell" )
	{
		ent.curstate.rendercolor.r = 255;
		ent.curstate.rendercolor.g = 140;
		ent.curstate.rendercolor.b = 20;

		CHECK( ShouldApplyChargerGlowShell( &ent, "*2" ) == true );
	}

	SECTION( "Depleted wall chargers (frame == 1) do NOT qualify (extinguished)" )
	{
		ent.curstate.rendercolor.r = 32;
		ent.curstate.rendercolor.g = 255;
		ent.curstate.rendercolor.b = 32;
		ent.curstate.frame = 1; // Depleted!

		CHECK( ShouldApplyChargerGlowShell( &ent, "*1" ) == false );

		ent.curstate.rendercolor.r = 255;
		ent.curstate.rendercolor.g = 140;
		ent.curstate.rendercolor.b = 20;
		CHECK( ShouldApplyChargerGlowShell( &ent, "*2" ) == false );
	}

	SECTION( "Wall chargers with renderfx removed do NOT qualify" )
	{
		ent.curstate.renderfx = kRenderFxNone;
		ent.curstate.rendercolor.r = 32;
		ent.curstate.rendercolor.g = 255;
		ent.curstate.rendercolor.b = 32;

		CHECK( ShouldApplyChargerGlowShell( &ent, "*1" ) == false );
	}

	SECTION( "Non-brush models do NOT qualify even with charger colors" )
	{
		ent.curstate.rendercolor.r = 32;
		ent.curstate.rendercolor.g = 255;
		ent.curstate.rendercolor.b = 32;

		CHECK( ShouldApplyChargerGlowShell( &ent, "models/w_medkit.mdl" ) == false );
		CHECK( ShouldApplyChargerGlowShell( &ent, "models/player.mdl" ) == false );
	}

	SECTION( "Null entity and invalid model handling" )
	{
		CHECK( ShouldApplyChargerGlowShell( nullptr, "*1" ) == false );
		CHECK( ShouldApplyChargerGlowShell( &ent, nullptr ) == false );
		CHECK( ShouldApplyChargerGlowShell( &ent, "" ) == false );
	}
}

TEST_CASE( "GladderVisuals: Charger bounding box expansion calculation (SPEC §7.2, Issue #48)", "[gladder][visuals][chargers]" )
{
	Vector origin( 100.0f, 200.0f, 50.0f );
	Vector mins( -16.0f, -20.0f, -32.0f );
	Vector maxs( 16.0f, 20.0f, 32.0f );
	const float flDelta = 2.0f; // Expansion offset Delta ≈ 1.5 - 2.5 units

	Vector outMins, outMaxs;
	GladderComputeChargerGlowBounds( origin, mins, maxs, flDelta, outMins, outMaxs );

	// Mins = Origin + mins - Delta = (100 - 16 - 2, 200 - 20 - 2, 50 - 32 - 2) = (82, 178, 16)
	CHECK( outMins.x == Catch::Approx( 82.0f ) );
	CHECK( outMins.y == Catch::Approx( 178.0f ) );
	CHECK( outMins.z == Catch::Approx( 16.0f ) );

	// Maxs = Origin + maxs + Delta = (100 + 16 + 2, 200 + 20 + 2, 50 + 32 + 2) = (118, 222, 84)
	CHECK( outMaxs.x == Catch::Approx( 118.0f ) );
	CHECK( outMaxs.y == Catch::Approx( 222.0f ) );
	CHECK( outMaxs.z == Catch::Approx( 84.0f ) );
}

TEST_CASE( "GladderVisuals: Charger glow shell tracking and depletion extinguishing (SPEC §7.2, Issue #48)", "[gladder][visuals][chargers]" )
{
	GladderClearActiveChargers();
	REQUIRE( GladderGetActiveChargerCount() == 0 );

	model_t mockModel;
	memset( &mockModel, 0, sizeof( mockModel ) );
	mockModel.mins[0] = -12.0f; mockModel.mins[1] = -18.0f; mockModel.mins[2] = -24.0f;
	mockModel.maxs[0] = 12.0f;  mockModel.maxs[1] = 18.0f;  mockModel.maxs[2] = 24.0f;

	cl_entity_t entHealth;
	memset( &entHealth, 0, sizeof( entHealth ) );
	entHealth.index = 42;
	entHealth.origin[0] = 500.0f; entHealth.origin[1] = -200.0f; entHealth.origin[2] = 64.0f;
	entHealth.model = &mockModel;
	entHealth.curstate.renderfx = kRenderFxGlowShell;
	entHealth.curstate.rendercolor.r = 32;
	entHealth.curstate.rendercolor.g = 255;
	entHealth.curstate.rendercolor.b = 32;
	entHealth.curstate.frame = 0; // Active

	SECTION( "Active charger modifier tracks entity for glow shell rendering" )
	{
		GladderChargerVisualModifier( 0, &entHealth, "*5" );
		REQUIRE( GladderGetActiveChargerCount() == 1 );

		GladderActiveChargerInfo info;
		REQUIRE( GladderGetActiveChargerInfo( 0, info ) == true );
		CHECK( info.entityIndex == 42 );
		CHECK( info.origin.x == Catch::Approx( 500.0f ) );
		CHECK( info.r == 32 );
		CHECK( info.g == 255 );
		CHECK( info.b == 32 );
		CHECK( info.mins.x == Catch::Approx( -12.0f ) );
		CHECK( info.maxs.x == Catch::Approx( 12.0f ) );

		// Re-invoking modifier updates existing entry rather than duplicating
		GladderChargerVisualModifier( 0, &entHealth, "*5" );
		CHECK( GladderGetActiveChargerCount() == 1 );
	}

	SECTION( "Depleting charger immediately removes it from active glow tracking" )
	{
		GladderChargerVisualModifier( 0, &entHealth, "*5" );
		REQUIRE( GladderGetActiveChargerCount() == 1 );

		// Wall station depleted: frame transitions to 1
		entHealth.curstate.frame = 1;
		entHealth.curstate.renderfx = kRenderFxNone;

		GladderChargerVisualModifier( 0, &entHealth, "*5" );
		CHECK( GladderGetActiveChargerCount() == 0 );
	}

	GladderClearActiveChargers();
}

