/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Procedural Pickup Weight Calculation & Loadout-Aware Ammo Balancing
 *	(SPEC §3, GitHub Issue #93)
 *
 ****/

#include "gladder_pickup_weights.h"
#include <cstring>

namespace
{

// Category Macro Weight Allocation (SPEC §3, Issue #93):
// - Ammunition: 59.5% (Majority of supply drops, SPEC > 55%)
// - Health & Armor: 22.0% (Attrition recovery, SPEC ~20-25%)
// - Consumable Ordnance: 11.5% (Tactical expendables)
// - Fixed Weapons: 7.0% (Rare tactical upgrades, total weapons < 20%)
static constexpr float CAT_WEIGHT_AMMO     = 59.5f;
static constexpr float CAT_WEIGHT_HEALTH   = 22.0f;
static constexpr float CAT_WEIGHT_ORDNANCE = 11.5f;
static constexpr float CAT_WEIGHT_WEAPON   = 7.0f;

// Loadout & Scarcity Multipliers (SPEC §3, Issue #93):
static constexpr float MULT_AMMO_OWNED     = 1.0f;
static constexpr float MULT_AMMO_UNOWNED   = 0.1f;
static constexpr float MULT_SCARCITY_BOOST = 3.0f; // Multiplier when reserve fraction < 0.25
static constexpr float MULT_PITY_BOOST     = 3.0f; // Multiplier when unspawned for >= 3 waves

struct HealthArmorDef
{
	const char *szClassname;
	float flBaseWeight;
};

static const HealthArmorDef s_healthArmorDefs[] = {
	{ "item_healthkit", 1.0f },
	{ "item_battery",   1.0f }
};

struct OrdnanceDef
{
	const char *szClassname;
	int tier;
	float relativeWeight;
};

static const OrdnanceDef s_ordnanceDefs[] = {
	{ "weapon_handgrenade", 1, 0.8f },
	{ "weapon_tripmine",    2, 0.6f },
	{ "weapon_satchel",     2, 0.6f },
	{ "weapon_snark",       3, 0.5f }
};

struct FixedWeaponDef
{
	const char *szClassname;
	int tier;
	float relativeWeight;
};

static const FixedWeaponDef s_fixedWeaponDefs[] = {
	{ "weapon_knife",      1, 0.7f },
	{ "weapon_pipewrench", 1, 0.6f },
	{ "weapon_glock",      1, 0.8f },
	{ "weapon_shotgun",    1, 0.7f },
	{ "weapon_mp5",        2, 0.8f },
	{ "weapon_357",        2, 0.6f },
	{ "weapon_eagle",      2, 0.6f },
	{ "weapon_crossbow",   3, 0.6f },
	{ "weapon_sniper",     3, 0.5f },
	{ "weapon_m249",       3, 0.5f },
	{ "weapon_rpg",        3, 0.6f },
	{ "weapon_gauss",      3, 0.6f },
	{ "weapon_egon",       3, 0.5f }
};

static const std::vector<GladderAmmoMapping> &GetStaticAmmoMappings()
{
	static const std::vector<GladderAmmoMapping> s_mappings = {
		GladderAmmoMapping( "ammo_9mmclip",   "9mm",        1, 1.0f,  "weapon_glock",    "weapon_mp5",   nullptr, 250 ),
		GladderAmmoMapping( "ammo_9mmAR",     "9mm",        1, 0.9f,  "weapon_glock",    "weapon_mp5",   nullptr, 250 ),
		GladderAmmoMapping( "ammo_buckshot",  "buckshot",   1, 1.0f,  "weapon_shotgun",  nullptr,        nullptr, 125 ),
		GladderAmmoMapping( "ammo_357",       "357",        2, 0.8f,  "weapon_357",      "weapon_eagle", nullptr, 36 ),
		GladderAmmoMapping( "ammo_ARgrenades","ARgrenades", 2, 0.65f, "weapon_mp5",      nullptr,        nullptr, 10 ),
		GladderAmmoMapping( "ammo_crossbow",  "bolts",      3, 0.65f, "weapon_crossbow", nullptr,        nullptr, 50 ),
		GladderAmmoMapping( "ammo_gaussclip", "uranium",    3, 0.6f,  "weapon_gauss",    "weapon_egon",  nullptr, 100 ),
		GladderAmmoMapping( "ammo_egonclip",  "uranium",    3, 0.6f,  "weapon_gauss",    "weapon_egon",  nullptr, 100 ),
		GladderAmmoMapping( "ammo_762",       "762",        2, 0.65f, "weapon_sniper",   nullptr,        nullptr, 30 ),
		GladderAmmoMapping( "ammo_556",       "556",        2, 0.7f,  "weapon_m249",     nullptr,        nullptr, 200 ),
		GladderAmmoMapping( "ammo_rpgclip",   "rockets",    3, 0.6f,  "weapon_rpg",      nullptr,        nullptr, 5 )
	};
	return s_mappings;
}

} // namespace

const std::vector<GladderAmmoMapping> &GetGladderAmmoMappings( void )
{
	return GetStaticAmmoMappings();
}

bool GladderPickupWeightInput::IsWeaponAllowed( const char *szClassname ) const
{
	if ( !szClassname || !*szClassname )
		return false;

	if ( !weaponWhitelist.empty() )
	{
		bool found = false;
		for ( const auto &w : weaponWhitelist )
		{
			if ( w == szClassname )
			{
				found = true;
				break;
			}
		}
		if ( !found )
			return false;
	}

	for ( const auto &b : weaponBlacklist )
	{
		if ( b == szClassname )
			return false;
	}

	return true;
}

bool GladderPickupWeightInput::HasWeapon( const char *szClassname ) const
{
	if ( !szClassname || !*szClassname )
		return false;

	for ( const auto &w : ownedWeapons )
	{
		if ( w == szClassname )
			return true;
	}
	return false;
}

float GladderPickupWeightInput::GetAmmoReserveFraction( const char *szClassname, const char *szAmmoType ) const
{
	if ( szClassname )
	{
		auto it = ammoReserveFractions.find( szClassname );
		if ( it != ammoReserveFractions.end() )
			return it->second;
	}
	if ( szAmmoType )
	{
		auto it = ammoReserveFractions.find( szAmmoType );
		if ( it != ammoReserveFractions.end() )
			return it->second;
	}
	return 1.0f;
}

int GladderPickupWeightInput::GetWavesSinceLastAmmoSpawn( const char *szClassname ) const
{
	if ( !szClassname )
		return 0;

	auto it = wavesSinceLastAmmoSpawn.find( szClassname );
	if ( it != wavesSinceLastAmmoSpawn.end() )
		return it->second;

	return 0;
}

float GetWeaponTierWeight( int tier, int iWaveNumber )
{
	float w = static_cast<float>( (std::max)( 1, iWaveNumber ) );

	switch ( tier )
	{
		case 1:
			return (std::max)( 4.0f, 20.0f - ( w - 1.0f ) * 2.0f );
		case 2:
			return ( w < 2.0f ) ? 3.0f : (std::min)( 15.0f, 5.0f + ( w - 2.0f ) * 2.0f );
		case 3:
			// Tier 3 weapons (Gauss, Egon, RPG, Crossbow, Sniper, M249) unlock at wave 3+
			return ( w < 3.0f ) ? 0.0f : (std::min)( 15.0f, 5.0f + ( w - 3.0f ) * 2.5f );
		default:
			return 5.0f;
	}
}

float GetOrdnanceTierWeight( int tier, int iWaveNumber )
{
	float w = static_cast<float>( (std::max)( 1, iWaveNumber ) );

	switch ( tier )
	{
		case 1:
			return (std::max)( 10.0f, 25.0f - ( w - 1.0f ) * 2.0f );
		case 2:
			return ( w < 2.0f ) ? 0.0f : (std::min)( 20.0f, 8.0f + ( w - 2.0f ) * 2.5f );
		case 3:
			return ( w < 3.0f ) ? 0.0f : (std::min)( 15.0f, 5.0f + ( w - 3.0f ) * 2.5f );
		default:
			return 8.0f;
	}
}

std::vector<GladderWeightedPickup> ComputePickupWeights( const GladderPickupWeightInput &input )
{
	std::vector<GladderWeightedPickup> result;

	// 1. Calculate intra-category weights for Ammunition
	const auto &ammoMappings = GetStaticAmmoMappings();
	std::vector<float> ammoItemWeights;
	ammoItemWeights.reserve( ammoMappings.size() );
	float sumAmmo = 0.0f;

	for ( const auto &ammo : ammoMappings )
	{
		bool bOwned = false;
		for ( int k = 0; k < 4 && ammo.compatibleWeapons[k] != nullptr; ++k )
		{
			if ( input.HasWeapon( ammo.compatibleWeapons[k] ) )
			{
				bOwned = true;
				break;
			}
		}

		float mult = bOwned ? MULT_AMMO_OWNED : MULT_AMMO_UNOWNED;
		if ( bOwned )
		{
			float frac = input.GetAmmoReserveFraction( ammo.szClassname, ammo.szAmmoType );
			if ( frac >= 0.0f && frac < 0.25f )
			{
				mult *= MULT_SCARCITY_BOOST;
			}

			int wavesSince = (std::max)( input.GetWavesSinceLastAmmoSpawn( ammo.szClassname ),
			                             input.GetWavesSinceLastAmmoSpawn( ammo.szAmmoType ) );
			if ( wavesSince >= 3 )
			{
				mult *= MULT_PITY_BOOST;
			}
		}

		float itemWeight = ammo.flBaseWeight * mult;
		if ( itemWeight < 0.0f )
			itemWeight = 0.0f;

		ammoItemWeights.push_back( itemWeight );
		sumAmmo += itemWeight;
	}

	// 2. Intra-category weights for Health & Armor
	size_t healthArmorCount = sizeof( s_healthArmorDefs ) / sizeof( s_healthArmorDefs[0] );
	std::vector<float> healthItemWeights;
	healthItemWeights.reserve( healthArmorCount );
	float sumHealth = 0.0f;

	for ( size_t i = 0; i < healthArmorCount; ++i )
	{
		float itemWeight = s_healthArmorDefs[i].flBaseWeight;
		healthItemWeights.push_back( itemWeight );
		sumHealth += itemWeight;
	}

	// 3. Intra-category weights for Consumable Ordnance
	size_t ordnanceCount = sizeof( s_ordnanceDefs ) / sizeof( s_ordnanceDefs[0] );
	std::vector<float> ordnanceItemWeights;
	std::vector<size_t> eligibleOrdnanceIndices;
	ordnanceItemWeights.reserve( ordnanceCount );
	eligibleOrdnanceIndices.reserve( ordnanceCount );
	float sumOrdnance = 0.0f;

	for ( size_t i = 0; i < ordnanceCount; ++i )
	{
		const auto &def = s_ordnanceDefs[i];
		if ( input.IsWeaponAllowed( def.szClassname ) )
		{
			float tierW = GetOrdnanceTierWeight( def.tier, input.iWaveNumber );
			float itemWeight = tierW * def.relativeWeight;
			if ( itemWeight > 0.0f )
			{
				eligibleOrdnanceIndices.push_back( i );
				ordnanceItemWeights.push_back( itemWeight );
				sumOrdnance += itemWeight;
			}
		}
	}

	// 4. Intra-category weights for Fixed Weapons
	size_t fixedWeaponCount = sizeof( s_fixedWeaponDefs ) / sizeof( s_fixedWeaponDefs[0] );
	std::vector<float> weaponItemWeights;
	std::vector<size_t> eligibleWeaponIndices;
	weaponItemWeights.reserve( fixedWeaponCount );
	eligibleWeaponIndices.reserve( fixedWeaponCount );
	float sumWeapon = 0.0f;
	if ( input.allowFixedWeapons )
	{
		for ( size_t i = 0; i < fixedWeaponCount; ++i )
		{
			const auto &def = s_fixedWeaponDefs[i];
			if ( input.IsWeaponAllowed( def.szClassname ) && !input.HasWeapon( def.szClassname ) )
			{
				float tierW = GetWeaponTierWeight( def.tier, input.iWaveNumber );
				float itemWeight = tierW * def.relativeWeight;
				if ( itemWeight > 0.0f )
				{
					eligibleWeaponIndices.push_back( i );
					weaponItemWeights.push_back( itemWeight );
					sumWeapon += itemWeight;
				}
			}
		}
	}

	// Macro category weighting
	float wAmmo     = ( sumAmmo > 0.0f ) ? CAT_WEIGHT_AMMO : 0.0f;
	float wHealth   = ( sumHealth > 0.0f ) ? CAT_WEIGHT_HEALTH : 0.0f;
	float wOrdnance = ( sumOrdnance > 0.0f ) ? CAT_WEIGHT_ORDNANCE : 0.0f;
	float wWeapon   = ( sumWeapon > 0.0f ) ? CAT_WEIGHT_WEAPON : 0.0f;

	// Fallback when player owns all fixed weapons (or none are eligible)
	if ( wWeapon <= 0.0f )
	{
		if ( sumOrdnance > 0.0f )
			wOrdnance += CAT_WEIGHT_WEAPON;
		else if ( sumAmmo > 0.0f )
			wAmmo += CAT_WEIGHT_WEAPON;
	}

	// Fallback if ordnance is empty
	if ( wOrdnance <= 0.0f && sumAmmo > 0.0f )
	{
		wAmmo += CAT_WEIGHT_ORDNANCE;
	}

	// Populate final calibrated pickup weights
	// Ammunition
	if ( wAmmo > 0.0f && sumAmmo > 0.0f )
	{
		for ( size_t i = 0; i < ammoMappings.size(); ++i )
		{
			if ( ammoItemWeights[i] > 0.0f )
			{
				float finalWeight = wAmmo * ( ammoItemWeights[i] / sumAmmo );
				result.push_back( GladderWeightedPickup( ammoMappings[i].szClassname, finalWeight, GLADDER_CAT_AMMO ) );
			}
		}
	}

	// Health & Armor
	if ( wHealth > 0.0f && sumHealth > 0.0f )
	{
		for ( size_t i = 0; i < healthArmorCount; ++i )
		{
			if ( healthItemWeights[i] > 0.0f )
			{
				float finalWeight = wHealth * ( healthItemWeights[i] / sumHealth );
				result.push_back( GladderWeightedPickup( s_healthArmorDefs[i].szClassname, finalWeight, GLADDER_CAT_HEALTH_ARMOR ) );
			}
		}
	}

	// Consumable Ordnance
	if ( wOrdnance > 0.0f && sumOrdnance > 0.0f )
	{
		for ( size_t k = 0; k < eligibleOrdnanceIndices.size(); ++k )
		{
			size_t idx = eligibleOrdnanceIndices[k];
			float finalWeight = wOrdnance * ( ordnanceItemWeights[k] / sumOrdnance );
			result.push_back( GladderWeightedPickup( s_ordnanceDefs[idx].szClassname, finalWeight, GLADDER_CAT_ORDNANCE ) );
		}
	}

	// Fixed Weapons
	if ( wWeapon > 0.0f && sumWeapon > 0.0f )
	{
		for ( size_t k = 0; k < eligibleWeaponIndices.size(); ++k )
		{
			size_t idx = eligibleWeaponIndices[k];
			float finalWeight = wWeapon * ( weaponItemWeights[k] / sumWeapon );
			result.push_back( GladderWeightedPickup( s_fixedWeaponDefs[idx].szClassname, finalWeight, GLADDER_CAT_WEAPON ) );
		}
	}

	if ( result.empty() )
	{
		result.push_back( GladderWeightedPickup( "item_healthkit", 1.0f, GLADDER_CAT_HEALTH_ARMOR ) );
	}

	return result;
}
