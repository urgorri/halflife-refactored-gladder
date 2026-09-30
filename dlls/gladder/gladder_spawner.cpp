/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Procedural Spawner, Dynamic Weighted Tier Distribution & Edict Garbage Collection Implementation
 *	(SPEC §2, §3, §4.4, §5, §7.3, §11, GitHub Issue #7)
 *
 ****/

#include "gladder_spawner.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

//
// Predefined monster roster categorized by difficulty tiers
//
static const GladderMonsterDef s_monsterRoster[] = {
	// Tier 1: Basic / Ambient Fodder (Always present across all waves, heavy in early waves)
	{ "monster_headcrab", 1, 1.0f, false, false },
	{ "monster_zombie", 1, 1.0f, false, false },

	// Tier 2: Intermediate Threats
	{ "monster_houndeye", 2, 1.0f, false, false },
	{ "monster_bullchicken", 2, 0.8f, false, false },
	{ "monster_barnacle", 2, 0.6f, false, true }, // Ceiling attached (SPEC §4.4)

	// Tier 3: Advanced Combatants
	{ "monster_vortigaunt", 3, 1.0f, false, false },
	{ "monster_alien_grunt", 3, 0.8f, false, false },
	{ "monster_alien_controller", 3, 0.5f, true, false }, // Flying / airborne (SPEC §4.4)

	// Tier 4: Lethal / Elite Adversaries
	{ "monster_human_grunt", 4, 1.0f, false, false },
	{ "monster_human_assassin", 4, 0.6f, false, false },
};

static const size_t s_monsterRosterCount = sizeof( s_monsterRoster ) / sizeof( s_monsterRoster[0] );

//
// Predefined supplies and pickups categorized by tiers
//
static const GladderPickupDef s_pickupRoster[] = {
	// Medical & Armor
	{ "item_healthkit", 1, 1.0f, false, false, true },
	{ "item_battery", 1, 1.0f, false, false, true },

	// Ammunition
	{ "ammo_9mmclip", 1, 1.0f, false, true, false },
	{ "ammo_9mmAR", 1, 1.0f, false, true, false },
	{ "ammo_buckshot", 1, 1.0f, false, true, false },
	{ "ammo_357", 2, 0.8f, false, true, false },
	{ "ammo_ARgrenades", 2, 0.6f, false, true, false },
	{ "ammo_rpgclip", 3, 0.5f, false, true, false },

	// Weapons
	{ "weapon_glock", 1, 0.8f, true, false, false },
	{ "weapon_shotgun", 1, 0.7f, true, false, false },
	{ "weapon_mp5", 2, 0.8f, true, false, false },
	{ "weapon_357", 2, 0.6f, true, false, false },
	{ "weapon_crossbow", 3, 0.5f, true, false, false },
	{ "weapon_rpg", 3, 0.4f, true, false, false },
	{ "weapon_gauss", 3, 0.4f, true, false, false },
	{ "weapon_egon", 3, 0.3f, true, false, false },
};

static const size_t s_pickupRosterCount = sizeof( s_pickupRoster ) / sizeof( s_pickupRoster[0] );

//
// GladderMapConfig Implementation
//
GladderMapConfig::GladderMapConfig()
    : baseMonsters( 6 ),
      monstersPerWave( 1.5f ),
      maxMonsters( 32 ),
      basePickups( 4 ),
      pickupsPerWave( 0.8f ),
      maxPickups( 16 ),
      allowBarnacles( true ),
      allowFlying( true )
{
}

bool GladderMapConfig::LoadForMap( const char *pszMapName )
{
	if ( !pszMapName || !*pszMapName )
		return false;

	mapName = pszMapName;
	std::string path = "gladder/maps/" + mapName + ".cfg";
	return LoadFromFile( path.c_str() );
}

bool GladderMapConfig::LoadFromFile( const char *pszFilePath )
{
	if ( !pszFilePath || !*pszFilePath )
		return false;

	std::ifstream file( pszFilePath );
	if ( !file.is_open() )
		return false;

	std::string line;
	while ( std::getline( file, line ) )
	{
		// Trim leading whitespace
		size_t first = line.find_first_not_of( " \t\r\n" );
		if ( first == std::string::npos || line[first] == '#' || line[first] == ';' )
			continue; // Comment or blank

		std::istringstream iss( line.substr( first ) );
		std::string key;
		iss >> key;

		if ( key == "whitelist_monster" )
		{
			std::string val;
			if ( iss >> val ) monsterWhitelist.push_back( val );
		}
		else if ( key == "blacklist_monster" )
		{
			std::string val;
			if ( iss >> val ) monsterBlacklist.push_back( val );
		}
		else if ( key == "whitelist_weapon" )
		{
			std::string val;
			if ( iss >> val ) weaponWhitelist.push_back( val );
		}
		else if ( key == "blacklist_weapon" )
		{
			std::string val;
			if ( iss >> val ) weaponBlacklist.push_back( val );
		}
		else if ( key == "base_monsters" )
		{
			iss >> baseMonsters;
		}
		else if ( key == "monsters_per_wave" )
		{
			iss >> monstersPerWave;
		}
		else if ( key == "max_monsters" )
		{
			iss >> maxMonsters;
		}
		else if ( key == "base_pickups" )
		{
			iss >> basePickups;
		}
		else if ( key == "pickups_per_wave" )
		{
			iss >> pickupsPerWave;
		}
		else if ( key == "max_pickups" )
		{
			iss >> maxPickups;
		}
		else if ( key == "allow_barnacles" )
		{
			int val;
			if ( iss >> val ) allowBarnacles = ( val != 0 );
		}
		else if ( key == "allow_flying" )
		{
			int val;
			if ( iss >> val ) allowFlying = ( val != 0 );
		}
	}

	return true;
}

bool GladderMapConfig::IsMonsterAllowed( const char *szClassname ) const
{
	if ( !szClassname || !*szClassname )
		return false;

	// Whitelist check: if non-empty, must be explicitly present
	if ( !monsterWhitelist.empty() )
	{
		bool found = false;
		for ( const auto &w : monsterWhitelist )
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

	// Blacklist check: if present, excluded
	for ( const auto &b : monsterBlacklist )
	{
		if ( b == szClassname )
			return false;
	}

	return true;
}

bool GladderMapConfig::IsWeaponAllowed( const char *szClassname ) const
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

//
// GladderSpawner Implementation
//
GladderSpawner::GladderSpawner()
    : m_iLastMonsterCount( 0 ),
      m_iLastPickupCount( 0 ),
      m_bLambdaSpawned( false )
{
}

GladderSpawner::~GladderSpawner()
{
	PurgeWaveEntities();
}

float GladderSpawner::GetMonsterTierWeight( int tier, int iWaveNumber )
{
	float w = static_cast<float>( (std::max)( 1, iWaveNumber ) );

	switch ( tier )
	{
		case 1:
			// Basic fodder: heavily weighted in early waves, gracefully levels out to ambient floor
			return (std::max)( 15.0f, 75.0f - ( w - 1.0f ) * 5.0f );

		case 2:
			// Intermediate threats: rises during early-mid waves, stabilizes in late waves
			return (std::max)( 10.0f, (std::min)( 35.0f, 20.0f + ( w - 1.0f ) * 3.0f - (std::max)( 0.0f, ( w - 6.0f ) * 2.0f ) ) );

		case 3:
			// Advanced combatants: introduces at wave 2, peaks in mid-late waves
			return ( w < 2.0f ) ? 5.0f : (std::min)( 35.0f, 5.0f + ( w - 2.0f ) * 4.0f );

		case 4:
			// Elite adversaries: appears at wave 4+, steadily scales up to max weight
			return ( w < 4.0f ) ? 0.0f : (std::min)( 35.0f, ( w - 3.0f ) * 4.0f );

		default:
			return 10.0f;
	}
}

float GladderSpawner::GetWeaponTierWeight( int tier, int iWaveNumber )
{
	float w = static_cast<float>( (std::max)( 1, iWaveNumber ) );

	switch ( tier )
	{
		case 1:
			return (std::max)( 10.0f, 80.0f - ( w - 1.0f ) * 8.0f );
		case 2:
			return ( w < 2.0f ) ? 10.0f : (std::min)( 40.0f, 15.0f + ( w - 2.0f ) * 5.0f );
		case 3:
			return ( w < 4.0f ) ? 0.0f : (std::min)( 40.0f, ( w - 3.0f ) * 6.0f );
		default:
			return 10.0f;
	}
}

int GladderSpawner::GetActiveEdictCount()
{
	if ( !gpGlobals )
		return 0;

	int maxEdicts = ( gpGlobals->maxEntities > 0 ) ? gpGlobals->maxEntities : 512;
	int activeCount = 0;

	for ( int i = 1; i < maxEdicts; ++i )
	{
		edict_t *pEdict = INDEXENT( i );
		if ( !FNullEnt( pEdict ) && !pEdict->free && pEdict->v.pContainingEntity != nullptr )
		{
			activeCount++;
		}
	}

	return activeCount;
}

int GladderSpawner::GetFreeEdictCount()
{
	if ( !gpGlobals )
		return 512;

	int maxEdicts = ( gpGlobals->maxEntities > 0 ) ? gpGlobals->maxEntities : 512;
	return (std::max)( 0, maxEdicts - GetActiveEdictCount() );
}

CBaseEntity *GladderSpawner::CreateWaveEntity( const char *szClassname, const Vector &vecOrigin, const Vector &vecAngles )
{
	if ( !szClassname || !*szClassname )
		return nullptr;

	// Edict Budget Protection Check (SPEC §11, GitHub Issue #7)
	int freeEdicts = GetFreeEdictCount();
	if ( freeEdicts <= SAFE_FREE_EDICT_THRESHOLD )
	{
		ALERT( at_console, "[Gladder] Spawner warning: edict budget near exhaustion (%d free, safe threshold %d). Throttling spawn.\n",
		       freeEdicts, SAFE_FREE_EDICT_THRESHOLD );
		return nullptr;
	}

	CBaseEntity *pEnt = CBaseEntity::Create( (char *)szClassname, vecOrigin, vecAngles, nullptr );
	if ( pEnt )
	{
		EHANDLE h;
		h = pEnt;
		m_spawnedEntities.push_back( h );
	}

	return pEnt;
}

const char *GladderSpawner::RollMonsterSpecies( int iWaveNumber, const GladderMapConfig &config, bool &outIsFlying, bool &outIsBarnacle )
{
	outIsFlying = false;
	outIsBarnacle = false;

	// Calculate cumulative probability weights for eligible monsters
	std::vector<float> weights;
	std::vector<size_t> indices;
	weights.reserve( s_monsterRosterCount );
	indices.reserve( s_monsterRosterCount );

	float totalWeight = 0.0f;

	for ( size_t i = 0; i < s_monsterRosterCount; ++i )
	{
		const auto &def = s_monsterRoster[i];

		// Filter map blacklist/whitelist
		if ( !config.IsMonsterAllowed( def.szClassname ) )
			continue;

		// Filter feature toggles
		if ( def.isBarnacle && !config.allowBarnacles )
			continue;
		if ( def.isFlying && !config.allowFlying )
			continue;

		float tierW = GetMonsterTierWeight( def.tier, iWaveNumber );
		float effectiveW = tierW * def.relativeWeight;

		if ( effectiveW > 0.0f )
		{
			totalWeight += effectiveW;
			weights.push_back( totalWeight );
			indices.push_back( i );
		}
	}

	if ( indices.empty() || totalWeight <= 0.0f )
	{
		// Fallback to basic headcrab
		return "monster_headcrab";
	}

	float roll = RANDOM_FLOAT( 0.0f, totalWeight );
	for ( size_t k = 0; k < weights.size(); ++k )
	{
		if ( roll <= weights[k] )
		{
			size_t selectedIdx = indices[k];
			outIsFlying   = s_monsterRoster[selectedIdx].isFlying;
			outIsBarnacle = s_monsterRoster[selectedIdx].isBarnacle;
			return s_monsterRoster[selectedIdx].szClassname;
		}
	}

	size_t lastIdx = indices.back();
	outIsFlying   = s_monsterRoster[lastIdx].isFlying;
	outIsBarnacle = s_monsterRoster[lastIdx].isBarnacle;
	return s_monsterRoster[lastIdx].szClassname;
}

const char *GladderSpawner::RollPickupItem( int iWaveNumber, const GladderMapConfig &config )
{
	std::vector<float> weights;
	std::vector<size_t> indices;
	weights.reserve( s_pickupRosterCount );
	indices.reserve( s_pickupRosterCount );

	float totalWeight = 0.0f;

	for ( size_t i = 0; i < s_pickupRosterCount; ++i )
	{
		const auto &def = s_pickupRoster[i];

		if ( def.isWeapon && !config.IsWeaponAllowed( def.szClassname ) )
			continue;

		float categoryW = 1.0f;
		if ( def.isWeapon )
			categoryW = GetWeaponTierWeight( def.tier, iWaveNumber );
		else if ( def.isHealthArmor )
			categoryW = 35.0f;
		else if ( def.isAmmo )
			categoryW = 45.0f;

		float effectiveW = categoryW * def.relativeWeight;
		if ( effectiveW > 0.0f )
		{
			totalWeight += effectiveW;
			weights.push_back( totalWeight );
			indices.push_back( i );
		}
	}

	if ( indices.empty() || totalWeight <= 0.0f )
		return "item_healthkit";

	float roll = RANDOM_FLOAT( 0.0f, totalWeight );
	for ( size_t k = 0; k < weights.size(); ++k )
	{
		if ( roll <= weights[k] )
			return s_pickupRoster[indices[k]].szClassname;
	}

	return s_pickupRoster[indices.back()].szClassname;
}

void GladderSpawner::SpawnLambdaCollectible( const GladderGridIndexer &indexer )
{
	m_bLambdaSpawned = false;

	// Pick a random valid cell that has clearance
	const GladderGridCell *pCell = indexer.GetRandomCell( -1, GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK );
	if ( !pCell )
		return;

	// Elevate 36 units above surface for prominent floating/bobbing arcade display (SPEC §7.3)
	Vector spawnPos = pCell->origin + Vector( 0.0f, 0.0f, 36.0f );
	CBaseEntity *pLambda = CreateWaveEntity( "item_gladder_lambda", spawnPos, g_vecZero );
	if ( pLambda )
	{
		m_bLambdaSpawned = true;
	}
}

void GladderSpawner::SpawnMonsters( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, int count )
{
	m_iLastMonsterCount = 0;

	for ( int i = 0; i < count; ++i )
	{
		if ( GetFreeEdictCount() <= SAFE_FREE_EDICT_THRESHOLD )
			break;

		bool isFlying = false;
		bool isBarnacle = false;
		const char *szSpecies = RollMonsterSpecies( iWaveNumber, config, isFlying, isBarnacle );

		// Select a suitable grid cell
		uint32_t reqFlags = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
		if ( isBarnacle )
			reqFlags |= GLADDER_CELL_CEILING_VALID;

		const GladderGridCell *pCell = indexer.GetRandomCell( -1, reqFlags );
		if ( !pCell )
		{
			// Try without ceiling requirement if barnacle failed
			pCell = indexer.GetRandomCell( -1, GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK );
			if ( !pCell )
				continue;
			if ( isBarnacle )
				isBarnacle = false; // Downgrade
		}

		// Calculate exact spawn position according to monster mechanics (SPEC §4.4)
		Vector vecSpawnPos = pCell->origin;
		if ( isBarnacle )
		{
			// Attached flush to overhead ceiling geometry
			vecSpawnPos.z = pCell->ceilingZ - 8.0f;
		}
		else if ( isFlying )
		{
			// Variable altitude between ground clearance and ceiling
			float flMinZ = pCell->origin.z + 32.0f;
			float flMaxZ = (std::max)( flMinZ + 16.0f, pCell->ceilingZ - 32.0f );
			vecSpawnPos.z = RANDOM_FLOAT( flMinZ, flMaxZ );
		}
		else
		{
			// Ground resting position
			vecSpawnPos.z += 1.0f;
		}

		Vector vecAngles = Vector( 0.0f, RANDOM_FLOAT( 0.0f, 360.0f ), 0.0f );
		CBaseEntity *pMonster = CreateWaveEntity( szSpecies, vecSpawnPos, vecAngles );
		if ( pMonster )
		{
			m_iLastMonsterCount++;
		}
	}
}

void GladderSpawner::SpawnPickups( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, int count )
{
	m_iLastPickupCount = 0;

	for ( int i = 0; i < count; ++i )
	{
		if ( GetFreeEdictCount() <= SAFE_FREE_EDICT_THRESHOLD )
			break;

		const char *szPickup = RollPickupItem( iWaveNumber, config );
		const GladderGridCell *pCell = indexer.GetRandomCell( -1, GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK );
		if ( !pCell )
			continue;

		Vector vecSpawnPos = pCell->origin + Vector( 0.0f, 0.0f, 4.0f );
		Vector vecAngles = Vector( 0.0f, RANDOM_FLOAT( 0.0f, 360.0f ), 0.0f );

		CBaseEntity *pItem = CreateWaveEntity( szPickup, vecSpawnPos, vecAngles );
		if ( pItem )
		{
			m_iLastPickupCount++;
		}
	}
}

int GladderSpawner::SpawnWave( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config )
{
	if ( indexer.GetCellCount() == 0 )
	{
		ALERT( at_console, "[Gladder] Cannot spawn wave %d: Spatial grid indexer has 0 cells.\n", iWaveNumber );
		return 0;
	}

	// 1. Calculate wave quota based on wave progression curve (SPEC §3)
	int monsterCount = static_cast<int>( config.baseMonsters + ( iWaveNumber - 1 ) * config.monstersPerWave );
	monsterCount = (std::min)( monsterCount, config.maxMonsters );

	int pickupCount = static_cast<int>( config.basePickups + ( iWaveNumber - 1 ) * config.pickupsPerWave );
	pickupCount = (std::min)( pickupCount, config.maxPickups );

	// 2. Spawn exactly 1 Lambda collectible item per wave (SPEC §7.3)
	SpawnLambdaCollectible( indexer );

	// 3. Procedurally generate monsters across spatial grid cells
	SpawnMonsters( iWaveNumber, indexer, config, monsterCount );

	// 4. Procedurally generate randomized pickups & weapons
	SpawnPickups( iWaveNumber, indexer, config, pickupCount );

	ALERT( at_console, "[Gladder] Wave %d spawned: %d monsters, %d supplies, lambda=%s (Tracked total: %u entities)\n",
	       iWaveNumber, m_iLastMonsterCount, m_iLastPickupCount, m_bLambdaSpawned ? "yes" : "no",
	       static_cast<unsigned int>( m_spawnedEntities.size() ) );

	return m_iLastMonsterCount + m_iLastPickupCount + ( m_bLambdaSpawned ? 1 : 0 );
}

void GladderSpawner::PurgeWaveEntities()
{
	// 1. Remove all wave-spawned entities tracked in registration list
	for ( auto &hEnt : m_spawnedEntities )
	{
		CBaseEntity *pEnt = (CBaseEntity *)hEnt;
		if ( pEnt && pEnt->pev )
		{
			UTIL_Remove( pEnt );
		}
	}
	m_spawnedEntities.clear();
	m_bLambdaSpawned = false;

	// 2. Comprehensive edict sweep to catch combat debris, dropped items, projectiles & corpses (SPEC §11)
	if ( !gpGlobals )
		return;

	UTIL_ForEachEntity( []( CBaseEntity *pEnt ) {
		if ( !pEnt || !pEnt->pev )
			return;

		// 0. STRICT PROTECTIONS: Never remove player, permanent brushes, triggers, or wall stations
		if ( pEnt->IsPlayer() )
			return;

		const char *szClass = STRING( pEnt->pev->classname );
		if ( !szClass || !*szClass )
			return;

		if ( FClassnameIs( pEnt->pev, "worldspawn" ) )
			return;

		// Protect Gladder game rules entities & interactive breakables
		if ( FClassnameIs( pEnt->pev, "gladder_breakable" ) ||
		     FClassnameIs( pEnt->pev, "func_healthcharger" ) ||
		     FClassnameIs( pEnt->pev, "func_recharge" ) ||
		     FClassnameIs( pEnt->pev, "trigger_gladder_start" ) ||
		     FClassnameIs( pEnt->pev, "trigger_gladder_finish" ) ||
		     FClassnameIs( pEnt->pev, "trigger_gladder_area" ) ||
		     FClassnameIs( pEnt->pev, "gladder_wave_relay" ) )
		{
			return;
		}

		// Protect suit item
		if ( FClassnameIs( pEnt->pev, "item_suit" ) )
			return;

		// A. Eradicate surviving monsters & corpses
		if ( pEnt->MyMonsterPointer() != nullptr )
		{
			UTIL_Remove( pEnt );
			return;
		}

		// B. Remove dropped weapon boxes
		if ( FClassnameIs( pEnt->pev, "weaponbox" ) )
		{
			UTIL_Remove( pEnt );
			return;
		}

		// C. Remove dropped world weapons and ammo (unowned by players)
		if ( strncmp( szClass, "weapon_", 7 ) == 0 || strncmp( szClass, "ammo_", 5 ) == 0 )
		{
			if ( pEnt->pev->owner == nullptr )
			{
				UTIL_Remove( pEnt );
				return;
			}
		}

		// D. Remove unowned world items (healthkits, batteries, collectibles)
		if ( strncmp( szClass, "item_", 5 ) == 0 )
		{
			if ( pEnt->pev->owner == nullptr )
			{
				UTIL_Remove( pEnt );
				return;
			}
		}

		// E. Remove combat projectiles & temporary debris
		if ( FClassnameIs( pEnt->pev, "rpg_rocket" ) ||
		     FClassnameIs( pEnt->pev, "hvr_rocket" ) ||
		     FClassnameIs( pEnt->pev, "hornet" ) ||
		     FClassnameIs( pEnt->pev, "grenade" ) ||
		     FClassnameIs( pEnt->pev, "monster_snark" ) ||
		     FClassnameIs( pEnt->pev, "gib" ) )
		{
			UTIL_Remove( pEnt );
			return;
		}
	} );
}
