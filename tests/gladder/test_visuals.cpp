/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Client Pickup Visual Classification (SPEC §7.2, Issue #12)
 *
 ****/

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

	SECTION( "Weapons Arsenal" )
	{
		CHECK( ClassifyPickupModel( "models/w_crowbar.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_shotgun.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_knife.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_pipe_wrench.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_desert_eagle.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_m40a1.mdl" ) == GLADDER_PICKUP_WEAPON );
		CHECK( ClassifyPickupModel( "models/w_saw.mdl" ) == GLADDER_PICKUP_WEAPON );
	}

	SECTION( "Non-Pickup Entities" )
	{
		CHECK( ClassifyPickupModel( "models/player.mdl" ) == GLADDER_PICKUP_NONE );
		CHECK( ClassifyPickupModel( "models/headcrab.mdl" ) == GLADDER_PICKUP_NONE );
		CHECK( ClassifyPickupModel( "" ) == GLADDER_PICKUP_NONE );
		CHECK( ClassifyPickupModel( nullptr ) == GLADDER_PICKUP_NONE );
	}
}
