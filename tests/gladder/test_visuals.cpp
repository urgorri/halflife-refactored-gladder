/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Client Pickup Visual Classification (SPEC §7.2, Issue #12)
 *
 ****/

#include "common/mathlib.h"
#include "common/const.h"
#include "common/entity_state.h"
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
