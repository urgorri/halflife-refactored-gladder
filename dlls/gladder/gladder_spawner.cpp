/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Procedural Spawner, Dynamic Weighted Tier Distribution & Edict Garbage Collection Implementation
 *	(SPEC §2, §3, §4.4, §5, §7.3, §11, GitHub Issue #7)
 *
 ****/

#include "gladder_spawner.h"
#include "gladder_modifiers.h"
#include "gladder_pickup_weights.h"
#include "core/player.h"
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
	{ "item_healthkit", 1, 1.0f, false, false, true, false },
	{ "item_battery", 1, 1.0f, false, false, true, false },

	// Ammunition
	{ "ammo_9mmclip", 1, 1.0f, false, true, false, false },
	{ "ammo_9mmAR", 1, 1.0f, false, true, false, false },
	{ "ammo_buckshot", 1, 1.0f, false, true, false, false },
	{ "ammo_357", 2, 0.8f, false, true, false, false },
	{ "ammo_ARgrenades", 2, 0.6f, false, true, false, false },
	{ "ammo_crossbow", 3, 0.6f, false, true, false, false },
	{ "ammo_gaussclip", 3, 0.5f, false, true, false, false },
	{ "ammo_egonclip", 3, 0.5f, false, true, false, false },
	{ "ammo_762", 2, 0.6f, false, true, false, false },
	{ "ammo_556", 2, 0.7f, false, true, false, false },
	{ "ammo_rpgclip", 3, 0.5f, false, true, false, false },

	// Fixed Weapons (SPEC §3)
	{ "weapon_knife", 1, 0.7f, true, false, false, false },
	{ "weapon_pipewrench", 1, 0.6f, true, false, false, false },
	{ "weapon_glock", 1, 0.8f, true, false, false, false },
	{ "weapon_shotgun", 1, 0.7f, true, false, false, false },
	{ "weapon_mp5", 2, 0.8f, true, false, false, false },
	{ "weapon_357", 2, 0.6f, true, false, false, false },
	{ "weapon_eagle", 2, 0.6f, true, false, false, false },
	{ "weapon_crossbow", 3, 0.6f, true, false, false, false },
	{ "weapon_sniper", 3, 0.5f, true, false, false, false },
	{ "weapon_m249", 3, 0.5f, true, false, false, false },
	{ "weapon_rpg", 3, 0.6f, true, false, false, false },
	{ "weapon_gauss", 3, 0.6f, true, false, false, false },
	{ "weapon_egon", 3, 0.5f, true, false, false, false },

	// Consumable Ordnance Weapons (SPEC §3, §7, Issue #78)
	{ "weapon_handgrenade", 1, 0.8f, false, false, false, true },
	{ "weapon_tripmine", 2, 0.6f, false, false, false, true },
	{ "weapon_satchel", 2, 0.6f, false, false, false, true },
	{ "weapon_snark", 3, 0.5f, false, false, false, true },
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
      m_iLastChampionCount( 0 ),
      m_bLambdaSpawned( false )
{
}

GladderSpawner::~GladderSpawner()
{
	PurgeWaveEntities();
}

void GladderSpawner::Precache( void )
{
	// 1. Precache full monster roster for procedural waves (prevents Host_Error in GoldSrc)
	for ( size_t i = 0; i < s_monsterRosterCount; ++i )
	{
		UTIL_PrecacheOther( s_monsterRoster[i].szClassname );
	}

	// 2. Precache full pickup & ammo roster
	for ( size_t i = 0; i < s_pickupRosterCount; ++i )
	{
		UTIL_PrecacheOther( s_pickupRoster[i].szClassname );
	}

	// 3. Precache custom Lambda collectible entity
	UTIL_PrecacheOther( "item_gladder_lambda" );

	ALERT( at_console, "[Gladder] Precached %u monster types and %u pickups for procedural spawning\n",
	       static_cast<unsigned int>( s_monsterRosterCount ),
	       static_cast<unsigned int>( s_pickupRosterCount ) );
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
	return ::GetWeaponTierWeight( tier, iWaveNumber );
}

float GladderSpawner::GetOrdnanceTierWeight( int tier, int iWaveNumber )
{
	return ::GetOrdnanceTierWeight( tier, iWaveNumber );
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

		if ( pEnt->MyMonsterPointer() || ( pEnt->pev && ( pEnt->pev->flags & FL_MONSTER ) ) )
		{
			pEnt->pev->flags |= FL_MONSTERCLIP;
		}
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

static std::unordered_map<std::string, int> s_wavesSinceLastAmmoSpawn;
static int s_iLastPityWave = 0;

void GladderSpawner::ResetPickupPity( void )
{
	s_wavesSinceLastAmmoSpawn.clear();
	const auto &ammoMappings = GetGladderAmmoMappings();
	for ( const auto &m : ammoMappings )
	{
		s_wavesSinceLastAmmoSpawn[m.szClassname] = 0;
	}
	s_iLastPityWave = 0;
}

void GladderSpawner::UpdateAmmoPityTracking( int iWaveNumber )
{
	if ( s_iLastPityWave == 0 )
	{
		s_iLastPityWave = iWaveNumber;
		return;
	}

	if ( iWaveNumber > s_iLastPityWave )
	{
		int diff = iWaveNumber - s_iLastPityWave;
		for ( auto &kv : s_wavesSinceLastAmmoSpawn )
		{
			kv.second += diff;
		}
		s_iLastPityWave = iWaveNumber;
	}
}

void GladderSpawner::RecordAmmoSpawned( const char *szPickup )
{
	if ( !szPickup || !*szPickup )
		return;

	if ( strncmp( szPickup, "ammo_", 5 ) == 0 )
	{
		s_wavesSinceLastAmmoSpawn[szPickup] = 0;
		if ( strcmp( szPickup, "ammo_gaussclip" ) == 0 || strcmp( szPickup, "ammo_egonclip" ) == 0 )
		{
			s_wavesSinceLastAmmoSpawn["ammo_gaussclip"] = 0;
			s_wavesSinceLastAmmoSpawn["ammo_egonclip"] = 0;
		}
	}
}

const char *GladderSpawner::RollPickupItem( int iWaveNumber, const GladderMapConfig &config, CBasePlayer *pPlayer )
{
	if ( !pPlayer && gpGlobals )
	{
		pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( 1 );
	}

	GladderPickupWeightInput input;
	input.iWaveNumber = iWaveNumber;
	input.weaponWhitelist = config.weaponWhitelist;
	input.weaponBlacklist = config.weaponBlacklist;

	if ( pPlayer )
	{
		for ( size_t i = 0; i < s_pickupRosterCount; ++i )
		{
			const auto &def = s_pickupRoster[i];
			if ( def.isWeapon )
			{
				if ( pPlayer->HasNamedPlayerItem( def.szClassname ) )
				{
					input.ownedWeapons.push_back( def.szClassname );
				}
			}
		}

		const auto &ammoMappings = GetGladderAmmoMappings();
		for ( const auto &mapping : ammoMappings )
		{
			int ammoIdx = CBasePlayer::GetAmmoIndex( mapping.szAmmoType );
			if ( ammoIdx > 0 )
			{
				int count = pPlayer->AmmoInventory( ammoIdx );
				int maxCarry = mapping.iMaxCarry > 0 ? mapping.iMaxCarry : 100;
				float frac = static_cast<float>( count ) / static_cast<float>( maxCarry );
				input.ammoReserveFractions[mapping.szClassname] = frac;
				input.ammoReserveFractions[mapping.szAmmoType] = frac;
			}
		}
	}

	UpdateAmmoPityTracking( iWaveNumber );
	input.wavesSinceLastAmmoSpawn = s_wavesSinceLastAmmoSpawn;

	std::vector<GladderWeightedPickup> weightedPickups = ComputePickupWeights( input );
	if ( weightedPickups.empty() )
		return "item_healthkit";

	float totalWeight = 0.0f;
	for ( const auto &item : weightedPickups )
	{
		totalWeight += item.flWeight;
	}

	if ( totalWeight <= 0.0f )
		return "item_healthkit";

	float roll = RANDOM_FLOAT( 0.0f, totalWeight );
	float accum = 0.0f;
	const char *szSelected = weightedPickups.back().szClassname;

	for ( const auto &item : weightedPickups )
	{
		accum += item.flWeight;
		if ( roll <= accum )
		{
			szSelected = item.szClassname;
			break;
		}
	}

	RecordAmmoSpawned( szSelected );
	return szSelected;
}

const GladderPickupDef *GladderSpawner::GetPickupRoster( size_t &outCount )
{
	outCount = s_pickupRosterCount;
	return s_pickupRoster;
}

bool GladderSpawner::IsLargeMonster( const char *szClassname )
{
	if ( !szClassname || !*szClassname )
		return false;

	return ( strcmp( szClassname, "monster_bullchicken" ) == 0 ||
	         strcmp( szClassname, "monster_alien_grunt" ) == 0 ||
	         strcmp( szClassname, "monster_gargantua" ) == 0 ||
	         strcmp( szClassname, "monster_bigmomma" ) == 0 );
}

void GladderSpawner::SpawnLambdaCollectible( const GladderGridIndexer &indexer, std::vector<bool> *pOccupiedCells )
{
	m_bLambdaSpawned = false;

	// Pick a random valid cell that has clearance and is unoccupied
	int cellIdx = indexer.GetRandomCellIndex( -1, GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK, pOccupiedCells );
	if ( cellIdx < 0 )
		return;

	const GladderGridCell &cell = indexer.GetCell( static_cast<size_t>( cellIdx ) );

	// Elevate 36 units above surface for prominent floating/bobbing arcade display (SPEC §7.3)
	Vector spawnPos = cell.origin + Vector( 0.0f, 0.0f, 36.0f );

	// Ensure spawn position does not intersect solid world or breakables (SPEC §11.2, Issue #90)
	if ( GladderIsPositionSolidOrBrush( spawnPos ) || GladderIsPositionSolidOrBrush( cell.origin + Vector( 0.0f, 0.0f, 8.0f ) ) )
	{
		if ( pOccupiedCells && cellIdx < static_cast<int>( pOccupiedCells->size() ) )
		{
			( *pOccupiedCells )[static_cast<size_t>( cellIdx )] = true;
		}
		return;
	}

	CBaseEntity *pLambda = CreateWaveEntity( "item_gladder_lambda", spawnPos, g_vecZero );
	if ( pLambda )
	{
		m_bLambdaSpawned = true;
		if ( pOccupiedCells && cellIdx < static_cast<int>( pOccupiedCells->size() ) )
		{
			( *pOccupiedCells )[static_cast<size_t>( cellIdx )] = true;
		}
	}
}

void GladderSpawner::SpawnMonsters( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, int count, std::vector<bool> *pOccupiedCells, const std::string &szSwarmSpecies )
{
	m_iLastMonsterCount = 0;

	if ( indexer.GetCellCount() == 0 || count <= 0 )
		return;

	std::vector<bool> localOccupied;
	if ( !pOccupiedCells )
	{
		localOccupied.assign( indexer.GetCellCount(), false );
		pOccupiedCells = &localOccupied;
	}

	struct SpawnedMonsterRecord
	{
		Vector pos;
		bool isLarge;
		bool isBarnacle;
		bool isFlying;
	};
	std::vector<SpawnedMonsterRecord> spawned;
	spawned.reserve( count );

	for ( int i = 0; i < count; ++i )
	{
		if ( GetFreeEdictCount() <= SAFE_FREE_EDICT_THRESHOLD )
		{
			ALERT( at_console, "[Gladder] Spawner edict budget threshold reached (%d active). Stopping monster spawn at wave %d.\n",
			       GetActiveEdictCount(), iWaveNumber );
			break;
		}

		bool isFlying = false;
		bool isBarnacle = false;
		const char *szSpecies = nullptr;
		if ( !szSwarmSpecies.empty() )
		{
			szSpecies = szSwarmSpecies.c_str();
		}
		else
		{
			szSpecies = RollMonsterSpecies( iWaveNumber, config, isFlying, isBarnacle );
		}
		bool isLarge = IsLargeMonster( szSpecies );
		int hullNum = isLarge ? large_hull : human_hull;

		// Select required flags for spatial candidate
		uint32_t reqFlags = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
		if ( isBarnacle )
		{
			reqFlags |= GLADDER_CELL_CEILING_VALID;
		}
		else if ( isLarge )
		{
			reqFlags |= GLADDER_CELL_LARGE_CLEARANCE;
		}

		// Collect all unoccupied candidate cells matching required flags
		std::vector<size_t> candidates;
		for ( size_t cIdx = 0; cIdx < indexer.GetCellCount(); ++cIdx )
		{
			if ( ( *pOccupiedCells )[cIdx] )
				continue;

			const auto &c = indexer.GetCell( cIdx );
			if ( ( c.flags & reqFlags ) == reqFlags )
			{
				candidates.push_back( cIdx );
			}
		}

		// Fallback for barnacle or large monster if strict flags yield no candidates
		if ( candidates.empty() )
		{
			if ( isBarnacle )
			{
				isBarnacle = false;
				szSpecies  = "monster_headcrab";
				reqFlags   = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
				hullNum    = human_hull;
				isLarge    = false;
			}
			else if ( isLarge )
			{
				// Degrade monster species to a standard human-sized threat (zombie)
				// instead of forcing a 64x64 large hull into a narrow 32x32 space that would embed into walls
				szSpecies = "monster_zombie";
				reqFlags  = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
				hullNum   = human_hull;
				isLarge   = false;
			}

			for ( size_t cIdx = 0; cIdx < indexer.GetCellCount(); ++cIdx )
			{
				if ( ( *pOccupiedCells )[cIdx] )
					continue;

				const auto &c = indexer.GetCell( cIdx );
				if ( ( c.flags & reqFlags ) == reqFlags )
				{
					candidates.push_back( cIdx );
				}
			}
		}

		if ( candidates.empty() )
		{
			// No more unoccupied cells available on the grid
			ALERT( at_console, "[Gladder] No free grid cells remaining for monster %d (%s). Stopping wave spawns.\n",
			       i + 1, szSpecies );
			break;
		}

		// Test candidate cells without duplicates to find one that satisfies distance and clearance
		bool bSpawnSuccess = false;
		size_t remaining = candidates.size();

		while ( remaining > 0 )
		{
			size_t pick = static_cast<size_t>( RANDOM_LONG( 0, static_cast<long>( remaining - 1 ) ) );
			size_t chosenIdx = candidates[pick];
			std::swap( candidates[pick], candidates[remaining - 1] );
			remaining--;

			const auto &cell = indexer.GetCell( chosenIdx );

			// Calculate exact spawn position according to monster mechanics (SPEC §4.4)
			Vector vecSpawnPos = cell.origin;
			if ( isBarnacle )
			{
				vecSpawnPos.z = cell.ceilingZ - 8.0f;
			}
			else if ( isFlying )
			{
				float flMinZ = cell.origin.z + 32.0f;
				float flMaxZ = (std::max)( flMinZ + 16.0f, cell.ceilingZ - 32.0f );
				vecSpawnPos.z = RANDOM_FLOAT( flMinZ, flMaxZ );
			}
			else
			{
				vecSpawnPos.z += 8.0f;
			}

			// 1. Distance separation check against already spawned monsters
			bool bTooClose = false;
			for ( const auto &prev : spawned )
			{
				if ( isBarnacle || prev.isBarnacle )
				{
					if ( isBarnacle && prev.isBarnacle )
					{
						if ( ( vecSpawnPos - prev.pos ).Length2D() < 48.0f )
						{
							bTooClose = true;
							break;
						}
					}
					continue;
				}

				if ( isFlying || prev.isFlying )
				{
					if ( ( vecSpawnPos - prev.pos ).Length() < 48.0f )
					{
						bTooClose = true;
						break;
					}
					continue;
				}

				// Ground monsters: ensure axis-aligned bounding boxes (AABBs) cannot overlap even on 45-degree diagonals
				float flDist2D = ( vecSpawnPos - prev.pos ).Length2D();
				float flMinSeparation = 54.0f;
				if ( isLarge && prev.isLarge )
				{
					flMinSeparation = 96.0f;
				}
				else if ( isLarge || prev.isLarge )
				{
					flMinSeparation = 76.0f;
				}

				if ( flDist2D < flMinSeparation )
				{
					bTooClose = true;
					break;
				}
			}

			if ( bTooClose )
				continue;

			// 2. Live collision clearance sweep (detect breakables, walls, other entities)
			Vector vecHullCheck = vecSpawnPos + Vector( 0.0f, 0.0f, 36.0f );
			if ( isBarnacle )
			{
				vecHullCheck = vecSpawnPos - Vector( 0.0f, 0.0f, 16.0f );
			}

			TraceResult trHull;
			UTIL_TraceHull( vecHullCheck, vecHullCheck, dont_ignore_monsters, hullNum, NULL, &trHull );
			if ( trHull.fStartSolid || trHull.fAllSolid || trHull.flFraction < 1.0f )
			{
				continue;
			}

			// Ensure lateral clearance for ground monsters so bounding box doesn't touch walls or solid brush entities (SPEC §4.3, Issue #37, Issue #89)
			// Checks 8 directions: 4 cardinal and 4 diagonal sweeps.
			if ( !isBarnacle && !isFlying )
			{
				float flRadius = isLarge ? 32.0f : 16.0f;
				float flDiag = flRadius * 0.7071f;
				if ( GladderIsPositionSolidOrBrush( vecHullCheck + Vector( flRadius, 0.0f, 0.0f ) ) ||
				     GladderIsPositionSolidOrBrush( vecHullCheck - Vector( flRadius, 0.0f, 0.0f ) ) ||
				     GladderIsPositionSolidOrBrush( vecHullCheck + Vector( 0.0f, flRadius, 0.0f ) ) ||
				     GladderIsPositionSolidOrBrush( vecHullCheck - Vector( 0.0f, flRadius, 0.0f ) ) ||
				     GladderIsPositionSolidOrBrush( vecHullCheck + Vector( flDiag, flDiag, 0.0f ) ) ||
				     GladderIsPositionSolidOrBrush( vecHullCheck + Vector( -flDiag, flDiag, 0.0f ) ) ||
				     GladderIsPositionSolidOrBrush( vecHullCheck + Vector( flDiag, -flDiag, 0.0f ) ) ||
				     GladderIsPositionSolidOrBrush( vecHullCheck - Vector( flDiag, flDiag, 0.0f ) ) )
				{
					continue;
				}
			}

			// 3. Valid candidate confirmed: instantiate monster entity
			Vector vecAngles = Vector( 0.0f, RANDOM_FLOAT( 0.0f, 360.0f ), 0.0f );
			CBaseEntity *pMonster = CreateWaveEntity( szSpecies, vecSpawnPos, vecAngles );
			if ( pMonster )
			{
				if ( pMonster->pev )
				{
					pMonster->pev->flags |= FL_MONSTERCLIP;
				}

				// Use exact Half-Life CBaseMonster::StartMonster stuck-in-wall detection (SPEC §4.3, Issue #89):
				// In monsters.cpp, StartMonster raises origin.z += 1, calls DROP_TO_FLOOR, and checks
				// WALK_MOVE( ENT( pev ), 0, 0, WALKMOVE_NORMAL ) at MonsterInitThink (0.1s after spawn).
				// Run this exact check immediately upon creation (with FL_MONSTERCLIP active); if it fails,
				// disable and remove the entity immediately and skip to the next grid cell.
				if ( pMonster->pev && !isBarnacle && !isFlying && pMonster->pev->movetype != MOVETYPE_FLY )
				{
					int iDrop = DROP_TO_FLOOR( ENT( pMonster->pev ) );
					int iWalk = WALK_MOVE( ENT( pMonster->pev ), 0.0f, 0.0f, WALKMOVE_NORMAL );
					if ( iDrop <= 0 || !iWalk || ( pMonster->pev->effects & EF_BRIGHTFIELD ) )
					{
						if ( !m_spawnedEntities.empty() && m_spawnedEntities.back() == pMonster )
						{
							m_spawnedEntities.pop_back();
						}
						pMonster->pev->solid = SOLID_NOT;
						pMonster->pev->movetype = MOVETYPE_NONE;
						pMonster->pev->takedamage = DAMAGE_NO;
						pMonster->pev->effects |= EF_NODRAW;
						pMonster->pev->nextthink = 0.0f;
						pMonster->SetThink( NULL );
						UTIL_SetOrigin( pMonster->pev, pMonster->pev->origin );
						UTIL_Remove( pMonster );
						( *pOccupiedCells )[chosenIdx] = true;
						continue;
					}
					vecSpawnPos = pMonster->pev->origin;
				}

				m_iLastMonsterCount++;

				if ( pMonster->MyMonsterPointer() && GladderModifiers::ShouldSpawnAsChampion( iWaveNumber ) )
				{
					GladderModifiers::MakeEliteChampion( pMonster->MyMonsterPointer() );
					m_iLastChampionCount++;
				}

				( *pOccupiedCells )[chosenIdx] = true;

				// Mark surrounding cells within separation radius as occupied so pickups and monsters cannot overlap
				float flExclusionDist = isLarge ? 76.0f : 54.0f;
				for ( size_t k = 0; k < indexer.GetCellCount(); ++k )
				{
					if ( !( *pOccupiedCells )[k] )
					{
						const auto &otherCell = indexer.GetCell( k );
						if ( ( otherCell.origin - vecSpawnPos ).Length2D() < flExclusionDist )
						{
							( *pOccupiedCells )[k] = true;
						}
					}
				}

				SpawnedMonsterRecord rec;
				rec.pos = vecSpawnPos;
				rec.isLarge = isLarge;
				rec.isBarnacle = isBarnacle;
				rec.isFlying = isFlying;
				spawned.push_back( rec );
				m_spawnedMonsterPositions.push_back( vecSpawnPos );

				bSpawnSuccess = true;
				break;
			}
		}

		if ( !bSpawnSuccess )
		{
			ALERT( at_console, "[Gladder] Spatial clearance exhausted for wave %d (spawned %d of %d monsters).\n",
			       iWaveNumber, m_iLastMonsterCount, count );
			break;
		}
	}
}

void GladderSpawner::SpawnPickups( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, int count, std::vector<bool> *pOccupiedCells )
{
	m_iLastPickupCount = 0;

	if ( indexer.GetCellCount() == 0 || count <= 0 )
		return;

	std::vector<bool> localOccupied;
	if ( !pOccupiedCells )
	{
		localOccupied.assign( indexer.GetCellCount(), false );
		pOccupiedCells = &localOccupied;
	}

	for ( int i = 0; i < count; ++i )
	{
		if ( GetFreeEdictCount() <= SAFE_FREE_EDICT_THRESHOLD )
			break;

		int cellIdx = indexer.GetRandomCellIndex( -1, GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK, pOccupiedCells );
		if ( cellIdx < 0 )
		{
			// No more unoccupied cells for pickups
			break;
		}

		const auto &cell = indexer.GetCell( static_cast<size_t>( cellIdx ) );
		CBasePlayer *pLocalPlayer = gpGlobals ? (CBasePlayer *)UTIL_PlayerByIndex( 1 ) : nullptr;
		const char *szPickup = RollPickupItem( iWaveNumber, config, pLocalPlayer );

		Vector vecSpawnPos = cell.origin + Vector( 0.0f, 0.0f, 4.0f );
		Vector vecAngles = Vector( 0.0f, RANDOM_FLOAT( 0.0f, 360.0f ), 0.0f );

		// Ensure candidate cell does not overlap any spawned monster position
		bool bMonsterNearby = false;
		for ( const auto &mPos : m_spawnedMonsterPositions )
		{
			if ( ( vecSpawnPos - mPos ).Length2D() < 48.0f )
			{
				bMonsterNearby = true;
				break;
			}
		}

		if ( bMonsterNearby )
		{
			( *pOccupiedCells )[static_cast<size_t>( cellIdx )] = true;
			continue;
		}

		// Live hull check for pickup (point_hull or head_hull)
		TraceResult trHull;
		UTIL_TraceHull( vecSpawnPos + Vector( 0, 0, 16 ), vecSpawnPos + Vector( 0, 0, 16 ), dont_ignore_monsters, head_hull, NULL, &trHull );
		if ( trHull.fStartSolid || trHull.fAllSolid )
		{
			( *pOccupiedCells )[static_cast<size_t>( cellIdx )] = true;
			continue;
		}

		// Ensure pickup does not spawn inside solid brush geometry or breakables (SPEC §11.2, Issue #90)
		if ( GladderIsPositionSolidOrBrush( vecSpawnPos ) || GladderIsPositionSolidOrBrush( vecSpawnPos + Vector( 0.0f, 0.0f, 8.0f ) ) )
		{
			( *pOccupiedCells )[static_cast<size_t>( cellIdx )] = true;
			continue;
		}

		CBaseEntity *pItem = CreateWaveEntity( szPickup, vecSpawnPos, vecAngles );
		if ( pItem )
		{
			m_iLastPickupCount++;
			( *pOccupiedCells )[static_cast<size_t>( cellIdx )] = true;
		}
	}
}

int GladderSpawner::SpawnWave( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, const std::string &szSwarmSpecies )
{
	m_iLastChampionCount = 0;
	m_spawnedMonsterPositions.clear();

	if ( indexer.GetCellCount() == 0 )
	{
		ALERT( at_console, "[Gladder] Cannot spawn wave %d: Spatial grid indexer has 0 cells.\n", iWaveNumber );
		return 0;
	}

	// Track occupied cells across the entire wave generation to ensure no two entities share the same cell
	std::vector<bool> occupiedCells( indexer.GetCellCount(), false );

	// 1. Calculate wave quota based on wave progression curve (SPEC §3)
	int monsterCount = static_cast<int>( config.baseMonsters + ( iWaveNumber - 1 ) * config.monstersPerWave );
	monsterCount = (std::min)( monsterCount, config.maxMonsters );

	int pickupCount = static_cast<int>( config.basePickups + ( iWaveNumber - 1 ) * config.pickupsPerWave );
	pickupCount = (std::min)( pickupCount, config.maxPickups );

	// 2. Spawn exactly 1 Lambda collectible item per wave (SPEC §7.3)
	SpawnLambdaCollectible( indexer, &occupiedCells );

	// 3. Procedurally generate monsters across spatial grid cells
	SpawnMonsters( iWaveNumber, indexer, config, monsterCount, &occupiedCells, szSwarmSpecies );

	// 4. Procedurally generate randomized pickups & weapons
	SpawnPickups( iWaveNumber, indexer, config, pickupCount, &occupiedCells );

	ALERT( at_console, "[Gladder] Wave %d spawned: %d monsters (%d champions), %d supplies, lambda=%s (Tracked total: %u entities)\n",
	       iWaveNumber, m_iLastMonsterCount, m_iLastChampionCount, m_iLastPickupCount, m_bLambdaSpawned ? "yes" : "no",
	       static_cast<unsigned int>( m_spawnedEntities.size() ) );

	return m_iLastMonsterCount + m_iLastPickupCount + ( m_bLambdaSpawned ? 1 : 0 );
}

void GladderSpawner::PurgeWaveEntities()
{
	// 1. Remove all wave-spawned entities tracked in registration list (uncollected items, active monsters)
	for ( auto &hEnt : m_spawnedEntities )
	{
		CBaseEntity *pEnt = (CBaseEntity *)hEnt;
		if ( pEnt && pEnt->pev )
		{
			// Never remove weapons or items currently collected/carried by a player!
			if ( pEnt->pev->owner != nullptr )
			{
				CBaseEntity *pOwner = CBaseEntity::Instance( pEnt->pev->owner );
				if ( ( pOwner && pOwner->IsPlayer() ) || ( pEnt->pev->owner->v.flags & FL_CLIENT ) )
				{
					continue;
				}
			}

			UTIL_Remove( pEnt );
		}
	}
	m_spawnedEntities.clear();
	m_spawnedMonsterPositions.clear();
	m_bLambdaSpawned = false;
	ResetPickupPity();

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

		// A. Remove orphaned attached effect sprites (SPEC §11, Issue #42)
		if ( FClassnameIs( pEnt->pev, "env_sprite" ) )
		{
			if ( pEnt->pev->movetype == MOVETYPE_FOLLOW )
			{
				if ( !pEnt->pev->aiment || pEnt->pev->aiment->free || ( pEnt->pev->aiment->v.flags & FL_KILLME ) )
				{
					UTIL_Remove( pEnt );
					return;
				}
			}
		}

		// B. Remove continuous beams & laser spots (SPEC §11, Issue #42)
		if ( FClassnameIs( pEnt->pev, "beam" ) || FClassnameIs( pEnt->pev, "laser_spot" ) )
		{
			UTIL_Remove( pEnt );
			return;
		}

		// C. Eradicate surviving monsters & corpses
		if ( pEnt->MyMonsterPointer() != nullptr )
		{
			UTIL_Remove( pEnt );
			return;
		}

		// D. Remove dropped weapon boxes
		if ( FClassnameIs( pEnt->pev, "weaponbox" ) )
		{
			UTIL_Remove( pEnt );
			return;
		}

		// E. Remove dropped world weapons and ammo (unowned by players)
		if ( strncmp( szClass, "weapon_", 7 ) == 0 || strncmp( szClass, "ammo_", 5 ) == 0 )
		{
			if ( pEnt->pev->owner == nullptr )
			{
				UTIL_Remove( pEnt );
				return;
			}
		}

		// F. Remove unowned world items (healthkits, batteries, collectibles)
		if ( strncmp( szClass, "item_", 5 ) == 0 )
		{
			if ( pEnt->pev->owner == nullptr )
			{
				UTIL_Remove( pEnt );
				return;
			}
		}

		// G. Remove combat projectiles, monster attacks, unexploded ordnance & temporary debris (SPEC §11, Issue #42)
		if ( FClassnameIs( pEnt->pev, "rpg_rocket" ) ||
		     FClassnameIs( pEnt->pev, "hvr_rocket" ) ||
		     FClassnameIs( pEnt->pev, "hornet" ) ||
		     FClassnameIs( pEnt->pev, "grenade" ) ||
		     FClassnameIs( pEnt->pev, "monster_snark" ) ||
		     FClassnameIs( pEnt->pev, "gib" ) ||
		     FClassnameIs( pEnt->pev, "squidspit" ) ||
		     FClassnameIs( pEnt->pev, "bmortar" ) ||
		     FClassnameIs( pEnt->pev, "bolt" ) ||
		     FClassnameIs( pEnt->pev, "controller_head_ball" ) ||
		     FClassnameIs( pEnt->pev, "controller_energy_ball" ) ||
		     FClassnameIs( pEnt->pev, "nihilanth_energy_ball" ) ||
		     FClassnameIs( pEnt->pev, "monster_satchel" ) ||
		     FClassnameIs( pEnt->pev, "monster_tripmine" ) )
		{
			UTIL_Remove( pEnt );
			return;
		}
	} );
}
