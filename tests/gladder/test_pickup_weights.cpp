/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit & Monte Carlo Simulation Tests: Pickup Weights & Ammo Coverage (SPEC §3, GitHub Issue #93)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "dlls/gladder/gladder_pickup_weights.h"
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "dlls/gladder/gladder_spawner.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace
{

class SimpleRng
{
  private:
	uint32_t m_state;

  public:
	explicit SimpleRng( uint32_t seed ) : m_state( seed ) {}

	uint32_t Next()
	{
		m_state = m_state * 1664525u + 1013904223u;
		return m_state;
	}

	float NextFloat( float minVal, float maxVal )
	{
		float norm = static_cast<float>( Next() ) / 4294967295.0f;
		return minVal + norm * ( maxVal - minVal );
	}
};

const GladderWeightedPickup *RollItem( const std::vector<GladderWeightedPickup> &items, SimpleRng &rng )
{
	if ( items.empty() )
		return nullptr;

	float total = 0.0f;
	for ( const auto &it : items )
		total += it.flWeight;

	if ( total <= 0.0f )
		return nullptr;

	float roll = rng.NextFloat( 0.0f, total );
	float accum = 0.0f;
	for ( const auto &it : items )
	{
		accum += it.flWeight;
		if ( roll <= accum )
			return &it;
	}
	return &items.back();
}

float SumCategoryWeight( const std::vector<GladderWeightedPickup> &items, int iCategory )
{
	float sum = 0.0f;
	for ( const auto &item : items )
	{
		if ( item.iCategory == iCategory )
			sum += item.flWeight;
	}
	return sum;
}

int CountCategoryEntries( const std::vector<GladderWeightedPickup> &items, int iCategory )
{
	int count = 0;
	for ( const auto &item : items )
	{
		if ( item.iCategory == iCategory )
			count++;
	}
	return count;
}

// ---------------------------------------------------------------------------
// Seeded supply-diversity harness (SPEC §3 "Supply Variety & Survival Realism Audit", Issue #129)
//
// Replays the production supply pipeline: the real ComputePickupWeights() output, the same
// weighted-roll arithmetic used by GladderSpawner::RollPickupItem(), the same per-run class and
// pity bookkeeping, and the production supply volume curve read from GladderMapConfig defaults.
// ---------------------------------------------------------------------------

int SimulatePickupCount( int iWaveNumber, const GladderMapConfig &config )
{
	int count = static_cast<int>( config.basePickups + ( iWaveNumber - 1 ) * config.pickupsPerWave );
	return (std::min)( count, config.maxPickups );
}

struct RunSimulation
{
	std::unordered_map<std::string, int> classCounts;
	int categoryCounts[GLADDER_CAT_COUNT];
	int iTotalPickups;

	RunSimulation()
	{
		for ( int i = 0; i < GLADDER_CAT_COUNT; ++i )
			categoryCounts[i] = 0;
		iTotalPickups = 0;
	}

	int GetClassCount( const char *szClassname ) const
	{
		std::unordered_map<std::string, int>::const_iterator it = classCounts.find( std::string( szClassname ) );
		return ( it != classCounts.end() ) ? it->second : 0;
	}

	float GetClassShare( const char *szClassname ) const
	{
		if ( iTotalPickups <= 0 )
			return 0.0f;
		return static_cast<float>( GetClassCount( szClassname ) ) / static_cast<float>( iTotalPickups );
	}

	float GetCategoryShare( int iCategory ) const
	{
		if ( iTotalPickups <= 0 )
			return 0.0f;
		return static_cast<float>( categoryCounts[iCategory] ) / static_cast<float>( iTotalPickups );
	}
};

const char *const s_ordnanceClassnames[] = {
	"weapon_handgrenade", "weapon_tripmine", "weapon_satchel", "weapon_snark"
};

bool IsOrdnanceClass( const std::string &szClassname )
{
	for ( const char *szOrdnance : s_ordnanceClassnames )
	{
		if ( szClassname == szOrdnance )
			return true;
	}
	return false;
}

// Replays a full gauntlet run, mirroring GladderSpawner::RollPickupItem() bookkeeping wave by wave.
RunSimulation SimulateGauntletRun( uint32_t seed, int iWaves, const std::vector<std::string> &initialLoadout,
                                   const std::unordered_map<std::string, float> &ammoReserves )
{
	SimpleRng rng( seed );
	RunSimulation run;

	const GladderMapConfig config;

	GladderPickupWeightInput input;
	input.ownedWeapons = initialLoadout;
	input.ammoReserveFractions = ammoReserves;

	const auto &ammoMappings = GetGladderAmmoMappings();
	for ( const auto &mapping : ammoMappings )
		input.wavesSinceLastAmmoSpawn[mapping.szClassname] = 0;

	for ( int wave = 1; wave <= iWaves; ++wave )
	{
		input.iWaveNumber = wave;
		if ( wave > 1 )
		{
			for ( auto &kv : input.wavesSinceLastAmmoSpawn )
				kv.second++;
		}

		const int pickupCount = SimulatePickupCount( wave, config );
		const int maxFixedWeaponsThisWave = ( pickupCount >= 14 ) ? 2 : 1;
		int fixedWeaponsSpawnedThisWave = 0;

		for ( int pickup = 0; pickup < pickupCount; ++pickup )
		{
			input.allowFixedWeapons = ( fixedWeaponsSpawnedThisWave < maxFixedWeaponsThisWave );

			const std::vector<GladderWeightedPickup> weightedItems = ComputePickupWeights( input );
			const GladderWeightedPickup *chosen = RollItem( weightedItems, rng );
			if ( !chosen )
				continue;

			run.classCounts[chosen->szClassname]++;
			run.categoryCounts[chosen->iCategory]++;
			run.iTotalPickups++;

			input.runClassCounts[chosen->szClassname]++;
			input.iRunTotalSpawns++;

			if ( chosen->iCategory == GLADDER_CAT_AMMO )
			{
				input.wavesSinceLastAmmoSpawn[chosen->szClassname] = 0;
				if ( std::strcmp( chosen->szClassname, "ammo_gaussclip" ) == 0 ||
				     std::strcmp( chosen->szClassname, "ammo_egonclip" ) == 0 )
				{
					input.wavesSinceLastAmmoSpawn["ammo_gaussclip"] = 0;
					input.wavesSinceLastAmmoSpawn["ammo_egonclip"] = 0;
				}
			}
			else if ( chosen->iCategory == GLADDER_CAT_WEAPON )
			{
				fixedWeaponsSpawnedThisWave++;
				input.ownedWeapons.push_back( chosen->szClassname );
			}
		}
	}

	return run;
}

const std::vector<std::string> &GetFreshLoadout()
{
	static const std::vector<std::string> s_freshLoadout( 1, "weapon_crowbar" );
	return s_freshLoadout;
}

const std::vector<std::string> &GetFullLoadout()
{
	static const std::vector<std::string> s_fullLoadout = {
		"weapon_knife", "weapon_pipewrench", "weapon_glock", "weapon_shotgun",
		"weapon_mp5", "weapon_357", "weapon_eagle", "weapon_crossbow",
		"weapon_sniper", "weapon_m249", "weapon_rpg", "weapon_gauss", "weapon_egon"
	};
	return s_fullLoadout;
}

} // namespace

TEST_CASE( "GladderPickupWeights: Ammo mapping data completeness (SPEC §3, Issue #93)", "[gladder][spawner][weights]" )
{
	const auto &mappings = GetGladderAmmoMappings();
	REQUIRE( mappings.size() >= 11 );

	for ( const auto &m : mappings )
	{
		INFO( "Checking mapping for ammo: " << m.szClassname );
		CHECK( m.szClassname != nullptr );
		CHECK( std::strlen( m.szClassname ) > 0 );
		CHECK( m.szAmmoType != nullptr );
		CHECK( std::strlen( m.szAmmoType ) > 0 );
		CHECK( m.iTier >= 1 );
		CHECK( m.iTier <= 3 );
		CHECK( m.flBaseWeight > 0.0f );
		CHECK( m.iMaxCarry > 0 );
		CHECK( m.compatibleWeapons[0] != nullptr );
	}

	// Verify uranium ammo mapping covers Gauss and Egon
	bool foundGaussClip = false;
	bool foundEgonClip = false;
	bool foundCrossbow = false;

	for ( const auto &m : mappings )
	{
		if ( std::strcmp( m.szClassname, "ammo_gaussclip" ) == 0 )
		{
			foundGaussClip = true;
			CHECK( std::strcmp( m.szAmmoType, "uranium" ) == 0 );
		}
		if ( std::strcmp( m.szClassname, "ammo_egonclip" ) == 0 )
		{
			foundEgonClip = true;
			CHECK( std::strcmp( m.szAmmoType, "uranium" ) == 0 );
		}
		if ( std::strcmp( m.szClassname, "ammo_crossbow" ) == 0 )
		{
			foundCrossbow = true;
			CHECK( std::strcmp( m.szAmmoType, "bolts" ) == 0 );
		}
	}

	CHECK( foundGaussClip );
	CHECK( foundEgonClip );
	CHECK( foundCrossbow );
}

TEST_CASE( "GladderPickupWeights: Pure weight computation & category integrity", "[gladder][spawner][weights]" )
{
	GladderPickupWeightInput input;
	input.iWaveNumber = 1;

	auto weights = ComputePickupWeights( input );
	REQUIRE( !weights.empty() );

	float totalWeight = 0.0f;
	float ammoWeight = 0.0f;
	float healthWeight = 0.0f;
	float ordnanceWeight = 0.0f;
	float weaponWeight = 0.0f;

	for ( const auto &item : weights )
	{
		CHECK( item.flWeight > 0.0f );
		totalWeight += item.flWeight;

		switch ( item.iCategory )
		{
			case GLADDER_CAT_AMMO:
				ammoWeight += item.flWeight;
				break;
			case GLADDER_CAT_HEALTH_ARMOR:
				healthWeight += item.flWeight;
				break;
			case GLADDER_CAT_ORDNANCE:
				ordnanceWeight += item.flWeight;
				break;
			case GLADDER_CAT_WEAPON:
				weaponWeight += item.flWeight;
				break;
			default:
				FAIL( "Unknown category encountered" );
		}
	}

	// In wave 1 with fresh loadout, categories must sum to 100.0f
	CHECK( totalWeight == Catch::Approx( 100.0f ) );
	// Category ratios: Ammo (59.5%), Health (22.0%), Ordnance (11.5%), Weapons (7.0%)
	CHECK( ammoWeight == Catch::Approx( 59.5f ) );
	CHECK( healthWeight == Catch::Approx( 22.0f ) );
	CHECK( ordnanceWeight == Catch::Approx( 11.5f ) );
	CHECK( weaponWeight == Catch::Approx( 7.0f ) );
}

TEST_CASE( "GladderPickupWeights: Loadout-aware multipliers (unowned, scarcity, pity)", "[gladder][spawner][weights]" )
{
	SECTION( "Unowned weapon ammo is down-weighted compared to owned weapon ammo" )
	{
		GladderPickupWeightInput inputGlock;
		inputGlock.iWaveNumber = 3;
		inputGlock.ownedWeapons.push_back( "weapon_glock" );

		auto weights = ComputePickupWeights( inputGlock );

		float glockAmmoWeight = 0.0f;
		float sniperAmmoWeight = 0.0f;

		for ( const auto &w : weights )
		{
			if ( std::strcmp( w.szClassname, "ammo_9mmclip" ) == 0 )
				glockAmmoWeight = w.flWeight;
			if ( std::strcmp( w.szClassname, "ammo_762" ) == 0 )
				sniperAmmoWeight = w.flWeight;
		}

		REQUIRE( glockAmmoWeight > 0.0f );
		REQUIRE( sniperAmmoWeight > 0.0f );
		// Glock is owned (mult 1.0), sniper is unowned (mult 0.1).
		// Relative base weights: 9mm = 1.0, 762 = 0.6.
		// Expected ratio: (1.0 * 1.0) / (0.6 * 0.1) = 16.6x
		CHECK( glockAmmoWeight > sniperAmmoWeight * 10.0f );
	}

	SECTION( "Low reserve triggers scarcity multiplier (3x boost)" )
	{
		GladderPickupWeightInput inputNormal;
		inputNormal.iWaveNumber = 3;
		inputNormal.ownedWeapons.push_back( "weapon_shotgun" );
		inputNormal.ammoReserveFractions["ammo_buckshot"] = 0.8f;

		GladderPickupWeightInput inputScare;
		inputScare.iWaveNumber = 3;
		inputScare.ownedWeapons.push_back( "weapon_shotgun" );
		inputScare.ammoReserveFractions["ammo_buckshot"] = 0.1f; // < 0.25 threshold

		auto weightsNormal = ComputePickupWeights( inputNormal );
		auto weightsScare = ComputePickupWeights( inputScare );

		float buckshotNormal = 0.0f;
		float buckshotScare = 0.0f;

		for ( const auto &w : weightsNormal )
			if ( std::strcmp( w.szClassname, "ammo_buckshot" ) == 0 ) buckshotNormal = w.flWeight;

		for ( const auto &w : weightsScare )
			if ( std::strcmp( w.szClassname, "ammo_buckshot" ) == 0 ) buckshotScare = w.flWeight;

		REQUIRE( buckshotNormal > 0.0f );
		REQUIRE( buckshotScare > 0.0f );
		// Under scarcity, buckshot's share of ammo pool is substantially higher
		CHECK( buckshotScare > buckshotNormal * 1.3f );
	}

	SECTION( "Pity boost triggers when ammo unspawned for >= 3 waves (3x boost)" )
	{
		GladderPickupWeightInput inputNoPity;
		inputNoPity.iWaveNumber = 4;
		inputNoPity.ownedWeapons.push_back( "weapon_rpg" );
		inputNoPity.wavesSinceLastAmmoSpawn["ammo_rpgclip"] = 1;

		GladderPickupWeightInput inputPity;
		inputPity.iWaveNumber = 4;
		inputPity.ownedWeapons.push_back( "weapon_rpg" );
		inputPity.wavesSinceLastAmmoSpawn["ammo_rpgclip"] = 3; // >= 3 threshold

		auto weightsNoPity = ComputePickupWeights( inputNoPity );
		auto weightsPity = ComputePickupWeights( inputPity );

		float rpgNoPity = 0.0f;
		float rpgPity = 0.0f;

		for ( const auto &w : weightsNoPity )
			if ( std::strcmp( w.szClassname, "ammo_rpgclip" ) == 0 ) rpgNoPity = w.flWeight;

		for ( const auto &w : weightsPity )
			if ( std::strcmp( w.szClassname, "ammo_rpgclip" ) == 0 ) rpgPity = w.flWeight;

		REQUIRE( rpgNoPity > 0.0f );
		REQUIRE( rpgPity > 0.0f );
		CHECK( rpgPity > rpgNoPity * 1.3f );
	}
}

TEST_CASE( "GladderPickupWeights: Fixed weapon ownership and fallback behavior (SPEC §3)", "[gladder][spawner][weights]" )
{
	SECTION( "Owned fixed weapons are excluded from spawn pool" )
	{
		GladderPickupWeightInput input;
		input.iWaveNumber = 3;
		input.ownedWeapons.push_back( "weapon_glock" );
		input.ownedWeapons.push_back( "weapon_shotgun" );

		auto weights = ComputePickupWeights( input );
		for ( const auto &w : weights )
		{
			CHECK( std::strcmp( w.szClassname, "weapon_glock" ) != 0 );
			CHECK( std::strcmp( w.szClassname, "weapon_shotgun" ) != 0 );
		}
	}

	SECTION( "Tier 3 weapons (Gauss, Egon, RPG) are gated to wave 3+" )
	{
		GladderPickupWeightInput wave1;
		wave1.iWaveNumber = 1;
		auto weights1 = ComputePickupWeights( wave1 );
		for ( const auto &w : weights1 )
		{
			CHECK( std::strcmp( w.szClassname, "weapon_gauss" ) != 0 );
			CHECK( std::strcmp( w.szClassname, "weapon_egon" ) != 0 );
			CHECK( std::strcmp( w.szClassname, "weapon_rpg" ) != 0 );
		}

		GladderPickupWeightInput wave3;
		wave3.iWaveNumber = 3;
		auto weights3 = ComputePickupWeights( wave3 );
		bool foundGauss = false;
		for ( const auto &w : weights3 )
		{
			if ( std::strcmp( w.szClassname, "weapon_gauss" ) == 0 ) foundGauss = true;
		}
		CHECK( foundGauss );
	}

	SECTION( "High-lethality melee weapons (knife, pipewrench) are promoted to Tier 2 (SPEC §7.13, Issue #119)" )
	{
		const GladderFixedWeaponDef *pDefKnife = GetFixedWeaponDef( "weapon_knife" );
		REQUIRE( pDefKnife != nullptr );
		CHECK( pDefKnife->tier == 2 );
		CHECK( pDefKnife->relativeWeight == Catch::Approx( 0.6f ) );

		const GladderFixedWeaponDef *pDefWrench = GetFixedWeaponDef( "weapon_pipewrench" );
		REQUIRE( pDefWrench != nullptr );
		CHECK( pDefWrench->tier == 2 );
		CHECK( pDefWrench->relativeWeight == Catch::Approx( 0.6f ) );

		const GladderFixedWeaponDef *pDefSniper = GetFixedWeaponDef( "weapon_sniper" );
		REQUIRE( pDefSniper != nullptr );
		CHECK( pDefSniper->tier == 3 );
		CHECK( pDefSniper->relativeWeight == Catch::Approx( 0.5f ) );

		const GladderFixedWeaponDef *pDefM249 = GetFixedWeaponDef( "weapon_m249" );
		REQUIRE( pDefM249 != nullptr );
		CHECK( pDefM249->tier == 3 );
		CHECK( pDefM249->relativeWeight == Catch::Approx( 0.5f ) );

		const GladderFixedWeaponDef *pDefEagle = GetFixedWeaponDef( "weapon_eagle" );
		REQUIRE( pDefEagle != nullptr );
		CHECK( pDefEagle->tier == 2 );
		CHECK( pDefEagle->relativeWeight == Catch::Approx( 0.6f ) );
	}

	SECTION( "When all fixed weapons are owned, weapon budget reallocates to ordnance or ammo" )
	{
		GladderPickupWeightInput input;
		input.iWaveNumber = 5;
		const char *allWeapons[] = {
			"weapon_knife", "weapon_pipewrench", "weapon_glock", "weapon_shotgun",
			"weapon_mp5", "weapon_357", "weapon_eagle", "weapon_crossbow",
			"weapon_sniper", "weapon_m249", "weapon_rpg", "weapon_gauss", "weapon_egon"
		};
		for ( const char *wep : allWeapons )
			input.ownedWeapons.push_back( wep );

		auto weights = ComputePickupWeights( input );
		float weaponWeight = 0.0f;
		float ordnanceWeight = 0.0f;

		for ( const auto &item : weights )
		{
			if ( item.iCategory == GLADDER_CAT_WEAPON ) weaponWeight += item.flWeight;
			if ( item.iCategory == GLADDER_CAT_ORDNANCE ) ordnanceWeight += item.flWeight;
		}

		CHECK( weaponWeight == 0.0f );
		// 11.5 base ordnance + 7.0 fallback from weapons = 18.5
		CHECK( ordnanceWeight == Catch::Approx( 18.5f ) );
	}
}

TEST_CASE( "GladderPickupWeights: 10,000-run Monte Carlo simulation (SPEC §3 ammo coverage guarantee)", "[gladder][spawner][weights][montecarlo]" )
{
	constexpr int NUM_RUNS = 10000;
	SimpleRng rng( 42424242u );

	SECTION( "Full loadout owned: Every ammo type appears in >= 90% of 10-wave runs" )
	{
		const char *allAmmoTypes[] = {
			"ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot", "ammo_357",
			"ammo_ARgrenades", "ammo_crossbow", "ammo_gaussclip", "ammo_egonclip",
			"ammo_762", "ammo_556", "ammo_rpgclip"
		};
		constexpr size_t numAmmoTypes = sizeof( allAmmoTypes ) / sizeof( allAmmoTypes[0] );

		std::unordered_map<std::string, int> ammoRunHits;
		for ( const char *a : allAmmoTypes )
			ammoRunHits[a] = 0;

		std::vector<std::string> fullLoadout = {
			"weapon_knife", "weapon_pipewrench", "weapon_glock", "weapon_shotgun",
			"weapon_mp5", "weapon_357", "weapon_eagle", "weapon_crossbow",
			"weapon_sniper", "weapon_m249", "weapon_rpg", "weapon_gauss", "weapon_egon"
		};

		for ( int run = 0; run < NUM_RUNS; ++run )
		{
			std::unordered_map<std::string, bool> hitInRun;
			for ( const char *a : allAmmoTypes )
				hitInRun[a] = false;

			GladderPickupWeightInput input;
			input.ownedWeapons = fullLoadout;
			for ( const char *a : allAmmoTypes )
				input.wavesSinceLastAmmoSpawn[a] = 0;

			for ( int wave = 1; wave <= 10; ++wave )
			{
				input.iWaveNumber = wave;
				// Pity wave progression
				if ( wave > 1 )
				{
					for ( auto &kv : input.wavesSinceLastAmmoSpawn )
						kv.second++;
				}

				auto weightedItems = ComputePickupWeights( input );
				int pickupCount = (std::min)( 16, static_cast<int>( 4 + ( wave - 1 ) * 0.8f ) );

				for ( int p = 0; p < pickupCount; ++p )
				{
					const auto *chosen = RollItem( weightedItems, rng );
					if ( chosen && chosen->iCategory == GLADDER_CAT_AMMO )
					{
						hitInRun[chosen->szClassname] = true;
						input.wavesSinceLastAmmoSpawn[chosen->szClassname] = 0;
						if ( std::strcmp( chosen->szClassname, "ammo_gaussclip" ) == 0 ||
						     std::strcmp( chosen->szClassname, "ammo_egonclip" ) == 0 )
						{
							input.wavesSinceLastAmmoSpawn["ammo_gaussclip"] = 0;
							input.wavesSinceLastAmmoSpawn["ammo_egonclip"] = 0;
						}
					}
				}
			}

			for ( const char *a : allAmmoTypes )
			{
				if ( hitInRun[a] )
					ammoRunHits[a]++;
			}
		}

		for ( const char *a : allAmmoTypes )
		{
			float appearanceRate = static_cast<float>( ammoRunHits[a] ) / static_cast<float>( NUM_RUNS );
			INFO( "Ammo appearance rate for " << a << ": " << ( appearanceRate * 100.0f ) << "%" );
			// SPEC §3 Guarantee: At least 90% appearance frequency across 10-wave gauntlet
			CHECK( appearanceRate >= 0.90f );
		}
	}

	SECTION( "Fresh gauntlet run (crowbar only): Category ratios hold (ammo > 55%, weapons < 20%)" )
	{
		uint64_t totalPickups = 0;
		uint64_t ammoPickups = 0;
		uint64_t weaponPickups = 0;

		const char *allAmmoTypes[] = {
			"ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot", "ammo_357",
			"ammo_ARgrenades", "ammo_crossbow", "ammo_gaussclip", "ammo_egonclip",
			"ammo_762", "ammo_556", "ammo_rpgclip"
		};

		for ( int run = 0; run < NUM_RUNS; ++run )
		{
			GladderPickupWeightInput input;
			input.ownedWeapons.push_back( "weapon_crowbar" );
			for ( const char *a : allAmmoTypes )
				input.wavesSinceLastAmmoSpawn[a] = 0;

			for ( int wave = 1; wave <= 10; ++wave )
			{
				input.iWaveNumber = wave;
				if ( wave > 1 )
				{
					for ( auto &kv : input.wavesSinceLastAmmoSpawn )
						kv.second++;
				}

				auto weightedItems = ComputePickupWeights( input );
				int pickupCount = (std::min)( 16, static_cast<int>( 4 + ( wave - 1 ) * 0.8f ) );

				for ( int p = 0; p < pickupCount; ++p )
				{
					const auto *chosen = RollItem( weightedItems, rng );
					if ( !chosen ) continue;

					totalPickups++;
					if ( chosen->iCategory == GLADDER_CAT_AMMO )
						ammoPickups++;
					else if ( chosen->iCategory == GLADDER_CAT_WEAPON )
						weaponPickups++;

					if ( chosen->iCategory == GLADDER_CAT_AMMO )
						input.wavesSinceLastAmmoSpawn[chosen->szClassname] = 0;
					else if ( chosen->iCategory == GLADDER_CAT_WEAPON )
						input.ownedWeapons.push_back( chosen->szClassname );
				}
			}
		}

		REQUIRE( totalPickups > 0 );
		float ammoRatio = static_cast<float>( ammoPickups ) / static_cast<float>( totalPickups );
		float weaponRatio = static_cast<float>( weaponPickups ) / static_cast<float>( totalPickups );

		INFO( "Overall Ammo share: " << ( ammoRatio * 100.0f ) << "%" );
		INFO( "Overall Weapon share: " << ( weaponRatio * 100.0f ) << "%" );

		// SPEC §3: Ammunition must represent the majority (> 55%) of drops
		CHECK( ammoRatio > 0.55f );
		// SPEC §3: Weapons must serve as rare upgrades (< 20%)
		CHECK( weaponRatio < 0.20f );
	}

	SECTION( "Gauss acquired: Uranium ammo coverage is >= 90% in 10-wave gauntlet" )
	{
		const char *allAmmoTypes[] = {
			"ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot", "ammo_357",
			"ammo_ARgrenades", "ammo_crossbow", "ammo_gaussclip", "ammo_egonclip",
			"ammo_762", "ammo_556", "ammo_rpgclip"
		};

		int uraniumHitRuns = 0;

		for ( int run = 0; run < NUM_RUNS; ++run )
		{
			bool gotUranium = false;
			GladderPickupWeightInput input;
			input.ownedWeapons.push_back( "weapon_crowbar" );
			for ( const char *a : allAmmoTypes )
				input.wavesSinceLastAmmoSpawn[a] = 0;

			for ( int wave = 1; wave <= 10; ++wave )
			{
				input.iWaveNumber = wave;
				if ( wave == 3 )
				{
					// Player acquires Gauss rifle at wave 3
					input.ownedWeapons.push_back( "weapon_gauss" );
				}

				if ( wave > 1 )
				{
					for ( auto &kv : input.wavesSinceLastAmmoSpawn )
						kv.second++;
				}

				auto weightedItems = ComputePickupWeights( input );
				int pickupCount = (std::min)( 16, static_cast<int>( 4 + ( wave - 1 ) * 0.8f ) );

				for ( int p = 0; p < pickupCount; ++p )
				{
					const auto *chosen = RollItem( weightedItems, rng );
					if ( !chosen ) continue;

					if ( wave >= 3 && ( std::strcmp( chosen->szClassname, "ammo_gaussclip" ) == 0 ||
					                    std::strcmp( chosen->szClassname, "ammo_egonclip" ) == 0 ) )
					{
						gotUranium = true;
					}

					if ( chosen->iCategory == GLADDER_CAT_AMMO )
					{
						input.wavesSinceLastAmmoSpawn[chosen->szClassname] = 0;
						if ( std::strcmp( chosen->szClassname, "ammo_gaussclip" ) == 0 ||
						     std::strcmp( chosen->szClassname, "ammo_egonclip" ) == 0 )
						{
							input.wavesSinceLastAmmoSpawn["ammo_gaussclip"] = 0;
							input.wavesSinceLastAmmoSpawn["ammo_egonclip"] = 0;
						}
					}
				}
			}

			if ( gotUranium )
				uraniumHitRuns++;
		}

		float uraniumCoverage = static_cast<float>( uraniumHitRuns ) / static_cast<float>( NUM_RUNS );
		INFO( "Uranium appearance rate from wave 3+: " << ( uraniumCoverage * 100.0f ) << "%" );
		// SPEC §3 Guarantee: Uranium ammo appears in >= 90% of runs when Gauss/Egon is owned
		CHECK( uraniumCoverage >= 0.90f );
	}
}

//
// Supply variety guarantees & survival realism audit (SPEC §3, Issue #129)
//
// Mirrors MAX_CLASS_SHARE from dlls/gladder/gladder_pickup_weights.cpp: no single supply class is
// allowed to hold more than a quarter of a run's supply drops.
static const float kMaxClassShare = 0.25f;

TEST_CASE( "GladderPickupWeights: No single supply class dominates a gauntlet run (SPEC §3, Issue #129)", "[gladder][spawner][weights][diversity]" )
{
	const uint32_t seeds[] = { 1u, 7u, 4242u, 99991u, 271828u, 1234567890u };
	const int waveCounts[] = { 10, 15 };
	const std::unordered_map<std::string, float> noReserves;

	for ( size_t s = 0; s < sizeof( seeds ) / sizeof( seeds[0] ); ++s )
	{
		for ( size_t w = 0; w < sizeof( waveCounts ) / sizeof( waveCounts[0] ); ++w )
		{
			INFO( "seed=" << seeds[s] << " waves=" << waveCounts[w] );
			RunSimulation run = SimulateGauntletRun( seeds[s], waveCounts[w], GetFreshLoadout(), noReserves );
			REQUIRE( run.iTotalPickups > 0 );

			int distinctClasses = 0;
			for ( const auto &entry : run.classCounts )
			{
				if ( entry.second <= 0 )
					continue;
				distinctClasses++;
				CHECK( run.GetClassShare( entry.first.c_str() ) <= kMaxClassShare );
			}

			// The run must never collapse onto a single supply class
			CHECK( distinctClasses >= 3 );
		}
	}
}

TEST_CASE( "GladderPickupWeights: Ammo, health and armor coverage plus ordnance/weapon variety (SPEC §3, Issue #129)", "[gladder][spawner][weights][diversity]" )
{
	// Documented minimum shares for a seeded 10-wave run (SPEC §3 supply variety audit)
	const float MIN_AMMO_SHARE   = 0.50f;
	const float MIN_HEALTH_SHARE = 0.05f;
	const float MIN_ARMOR_SHARE  = 0.05f;
	const std::unordered_map<std::string, float> noReserves;

	for ( uint32_t seed = 1u; seed <= 12u; ++seed )
	{
		INFO( "seed=" << seed );
		RunSimulation run = SimulateGauntletRun( seed, 10, GetFreshLoadout(), noReserves );
		REQUIRE( run.iTotalPickups > 0 );

		CHECK( run.GetCategoryShare( GLADDER_CAT_AMMO ) >= MIN_AMMO_SHARE );
		CHECK( run.GetClassShare( "item_healthkit" ) >= MIN_HEALTH_SHARE );
		CHECK( run.GetClassShare( "item_battery" ) >= MIN_ARMOR_SHARE );

		// At least three distinct ordnance or fixed-weapon classes must be offered per run
		int distinctOrdnance = 0;
		int distinctWeapons = 0;
		for ( const auto &entry : run.classCounts )
		{
			if ( entry.second <= 0 )
				continue;
			if ( IsOrdnanceClass( entry.first ) )
				distinctOrdnance++;
			else if ( entry.first.compare( 0, 7, "weapon_" ) == 0 )
				distinctWeapons++;
		}
		CHECK( distinctOrdnance >= 1 );
		CHECK( distinctOrdnance + distinctWeapons >= 3 );
	}
}

TEST_CASE( "GladderPickupWeights: Owned ammo coverage with a full loadout (SPEC §3, Issue #129)", "[gladder][spawner][weights][diversity]" )
{
	const char *const s_ownedAmmo[] = {
		"ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot", "ammo_357",
		"ammo_ARgrenades", "ammo_crossbow", "ammo_gaussclip", "ammo_egonclip",
		"ammo_762", "ammo_556", "ammo_rpgclip"
	};
	const size_t numOwnedAmmo = sizeof( s_ownedAmmo ) / sizeof( s_ownedAmmo[0] );
	constexpr int NUM_RUNS = 40;

	std::unordered_map<std::string, int> runsWithClass;
	for ( size_t i = 0; i < numOwnedAmmo; ++i )
		runsWithClass[s_ownedAmmo[i]] = 0;

	int runsWithCompleteCoverage = 0;
	const std::unordered_map<std::string, float> saturatedReserves;

	for ( int r = 0; r < NUM_RUNS; ++r )
	{
		// Fully loaded player: only the diversity guarantees can force an unseen ammo class in
		RunSimulation run = SimulateGauntletRun( 1000u + static_cast<uint32_t>( r ), 10, GetFullLoadout(), saturatedReserves );

		bool bAllCovered = true;
		for ( size_t i = 0; i < numOwnedAmmo; ++i )
		{
			if ( run.GetClassCount( s_ownedAmmo[i] ) > 0 )
				runsWithClass[s_ownedAmmo[i]]++;
			else
				bAllCovered = false;
		}
		if ( bAllCovered )
			runsWithCompleteCoverage++;
	}

	for ( size_t i = 0; i < numOwnedAmmo; ++i )
	{
		float appearanceRate = static_cast<float>( runsWithClass[s_ownedAmmo[i]] ) / static_cast<float>( NUM_RUNS );
		INFO( "Owned ammo coverage for " << s_ownedAmmo[i] << ": " << ( appearanceRate * 100.0f ) << "%" );
		CHECK( appearanceRate >= 0.95f );
	}

	float completeCoverageRate = static_cast<float>( runsWithCompleteCoverage ) / static_cast<float>( NUM_RUNS );
	INFO( "Runs where every owned ammo class appeared: " << ( completeCoverageRate * 100.0f ) << "%" );
	CHECK( completeCoverageRate >= 0.80f );
}

TEST_CASE( "GladderPickupWeights: Attrition response shifts share without breaking the ordnance band (SPEC §3, Issue #129)", "[gladder][spawner][weights][diversity]" )
{
	// Saturated baseline: every reserve is full (the harness defaults to 1.0).
	const std::unordered_map<std::string, float> saturatedReserves;

	// Attrition: the player has burned through shotgun (buckshot) and MP5 (9mm) ammunition.
	std::unordered_map<std::string, float> attritionReserves;
	attritionReserves["ammo_buckshot"] = 0.05f;
	attritionReserves["buckshot"]      = 0.05f;
	attritionReserves["ammo_9mmclip"]  = 0.05f;
	attritionReserves["ammo_9mmAR"]    = 0.05f;
	attritionReserves["9mm"]           = 0.05f;

	constexpr int NUM_RUNS = 20;
	float flSaturatedBuckshot = 0.0f;
	float flAttritionBuckshot = 0.0f;
	float flAttritionHealth   = 0.0f;
	float flAttritionOrdnance = 0.0f;

	for ( int r = 0; r < NUM_RUNS; ++r )
	{
		uint32_t seed = 500u + static_cast<uint32_t>( r );
		RunSimulation saturated = SimulateGauntletRun( seed, 10, GetFullLoadout(), saturatedReserves );
		RunSimulation attrition = SimulateGauntletRun( seed, 10, GetFullLoadout(), attritionReserves );

		flSaturatedBuckshot += saturated.GetClassShare( "ammo_buckshot" );
		flAttritionBuckshot += attrition.GetClassShare( "ammo_buckshot" );
		flAttritionHealth   += attrition.GetCategoryShare( GLADDER_CAT_HEALTH_ARMOR );
		flAttritionOrdnance += attrition.GetCategoryShare( GLADDER_CAT_ORDNANCE );
	}

	flSaturatedBuckshot /= static_cast<float>( NUM_RUNS );
	flAttritionBuckshot /= static_cast<float>( NUM_RUNS );
	flAttritionHealth   /= static_cast<float>( NUM_RUNS );
	flAttritionOrdnance /= static_cast<float>( NUM_RUNS );

	INFO( "buckshot share saturated=" << ( flSaturatedBuckshot * 100.0f ) << "% attrition=" << ( flAttritionBuckshot * 100.0f ) << "%" );
	INFO( "attrition health share=" << ( flAttritionHealth * 100.0f ) << "%" );
	INFO( "attrition ordnance share=" << ( flAttritionOrdnance * 100.0f ) << "%" );

	// Depleted ammo classes must gain share compared to the saturated baseline
	CHECK( flAttritionBuckshot > flSaturatedBuckshot * 1.20f );

	// Health/armor must keep covering the player without starving the other categories
	CHECK( flAttritionHealth >= 0.15f );
	CHECK( flAttritionHealth <= 0.30f );

	// Ordnance keeps appearing in modest amounts: above the documented 8% floor and well below the
	// 25% domination cap. The instantaneous macro weight stays inside 11.5%-18.5% (see the
	// freed-budget rotation case below, which asserts the weight band directly).
	CHECK( flAttritionOrdnance >= 0.08f );
	CHECK( flAttritionOrdnance <= 0.25f );
}

TEST_CASE( "GladderPickupWeights: Freed weapon budget rotates instead of collapsing onto one entry (SPEC §3, Issue #129)", "[gladder][spawner][weights][diversity]" )
{
	SECTION( "No ordnance spawned yet: freed weapon budget feeds ordnance across multiple entries" )
	{
		GladderPickupWeightInput input;
		input.iWaveNumber = 5;
		input.ownedWeapons = GetFullLoadout();

		auto weights = ComputePickupWeights( input );

		CHECK( SumCategoryWeight( weights, GLADDER_CAT_WEAPON ) == 0.0f );
		// 11.5 base ordnance + 7.0 rotated freed budget
		CHECK( SumCategoryWeight( weights, GLADDER_CAT_ORDNANCE ) == Catch::Approx( 18.5f ) );
		// The freed budget is spread over several ordnance entries, never a single one
		CHECK( CountCategoryEntries( weights, GLADDER_CAT_ORDNANCE ) >= 3 );

		float totalWeight = 0.0f;
		for ( const auto &item : weights )
			totalWeight += item.flWeight;

		for ( const auto &item : weights )
		{
			if ( item.iCategory == GLADDER_CAT_ORDNANCE )
				CHECK( item.flWeight / totalWeight <= kMaxClassShare );
		}
	}

	SECTION( "Ordnance already at its floor: freed weapon budget rotates to ammunition" )
	{
		GladderPickupWeightInput input;
		input.iWaveNumber = 5;
		input.ownedWeapons = GetFullLoadout();
		// The run has already spawned two ordnance supplies out of ten picks (20% >= 8% floor)
		input.runClassCounts["weapon_handgrenade"] = 2;
		input.iRunTotalSpawns = 10;

		auto weights = ComputePickupWeights( input );

		CHECK( SumCategoryWeight( weights, GLADDER_CAT_WEAPON ) == 0.0f );
		// 59.5 base ammo + 7.0 rotated freed budget
		CHECK( SumCategoryWeight( weights, GLADDER_CAT_AMMO ) == Catch::Approx( 66.5f ) );
		// Ordnance keeps its base macro allocation only
		CHECK( SumCategoryWeight( weights, GLADDER_CAT_ORDNANCE ) == Catch::Approx( 11.5f ) );
	}
}
