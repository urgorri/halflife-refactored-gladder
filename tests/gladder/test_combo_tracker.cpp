/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: GladderComboTracker & Species Kill Log (SPEC §3, §9.1, §9.2, Issue #8, Issue #60)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/basemonster.h"
#include "ai/monsters.h"
#include "tests/mock_engine.h"
#include "dlls/gladder/gladder_combo_tracker.h"
#include "dlls/systems/func_break.h"
#include "dlls/systems/explode.h"

// Helper dummy monster for kill simulation
class CTestComboMonster : public CBaseMonster
{
  public:
	CTestComboMonster( const char *szClassname, int hitgroup = HITGROUP_GENERIC )
	{
		pev = new entvars_t();
		memset( pev, 0, sizeof( entvars_t ) );
		pev->classname = MAKE_STRING( szClassname );
		m_LastHitGroup = hitgroup;
		m_iClassify = CLASS_NONE;
	}

	int m_iClassify;
	int Classify( void ) override { return m_iClassify; }

	~CTestComboMonster()
	{
		if ( pev )
		{
			delete pev;
			pev = nullptr;
		}
	}
};

// Helper player client edict for valid kill attribution
struct CTestPlayerClient
{
	edict_t edict;
	CTestPlayerClient()
	{
		memset( &edict, 0, sizeof( edict ) );
		edict.v.flags = FL_CLIENT;
		edict.v.pContainingEntity = &edict;
	}
};

TEST_CASE( "GladderComboTracker: Basic streak progression and decay timer (SPEC §3, Issue #8, Issue #60)", "[gladder][combo]" )
{
	ResetMockEngine();
	CTestPlayerClient player;
	GladderComboTracker tracker;
	tracker.Reset();

	CHECK( tracker.GetCurrentStreak() == 0 );
	CHECK( tracker.GetCurrentMultiplier() == 1 );
	CHECK( tracker.GetNormalizedGauge() == Catch::Approx( 0.0f ) );
	CHECK_FALSE( tracker.IsComboActive() );

	// Kill 1 at t = 1.0s
	CTestComboMonster m1( "monster_headcrab" );
	tracker.OnMonsterKilled( &m1, &player.edict.v, nullptr, 1.0f );

	CHECK( tracker.GetCurrentStreak() == 1 );
	CHECK( tracker.GetCurrentMultiplier() == 1 ); // x1 active, no multiplier yet
	CHECK( tracker.GetNormalizedGauge() == Catch::Approx( 1.0f ) );
	CHECK( tracker.GetTotalFrags() == 1 );

	// Kill 2 at t = 2.0s (within 3.5s window -> streak 2)
	CTestComboMonster m2( "monster_zombie" );
	tracker.OnMonsterKilled( &m2, &player.edict.v, nullptr, 2.0f );

	CHECK( tracker.GetCurrentStreak() == 2 );
	CHECK( tracker.GetCurrentMultiplier() == 2 );
	CHECK( tracker.IsComboActive() );
	CHECK( tracker.GetStats().x2Combos == 1 );

	// Kill 3 at t = 3.0s -> streak 3
	CTestComboMonster m3( "monster_houndeye" );
	tracker.OnMonsterKilled( &m3, &player.edict.v, nullptr, 3.0f );

	CHECK( tracker.GetCurrentStreak() == 3 );
	CHECK( tracker.GetCurrentMultiplier() == 3 );
	CHECK( tracker.GetStats().x3Combos == 1 );

	// Kill 4 at t = 4.0s -> streak 4 (x4+)
	CTestComboMonster m4( "monster_bullchicken" );
	tracker.OnMonsterKilled( &m4, &player.edict.v, nullptr, 4.0f );

	CHECK( tracker.GetCurrentStreak() == 4 );
	CHECK( tracker.GetCurrentMultiplier() == 4 );
	CHECK( tracker.GetStats().x4PlusCombos == 1 );
	CHECK( tracker.GetMaxStreak() == 4 );

	// Advance time past expire time (t = 4.0 + 3.5 = 7.5s -> test at 8.0s)
	tracker.Update( 8.0f );

	CHECK( tracker.GetCurrentStreak() == 0 );
	CHECK( tracker.GetCurrentMultiplier() == 1 );
	CHECK( tracker.GetNormalizedGauge() == Catch::Approx( 0.0f ) );
	CHECK_FALSE( tracker.IsComboActive() );
	CHECK( tracker.GetMaxStreak() == 4 ); // Max streak persists across match
}

TEST_CASE( "GladderComboTracker: Normalized gauge decay curve", "[gladder][combo]" )
{
	ResetMockEngine();
	CTestPlayerClient player;
	GladderComboTracker tracker;
	tracker.SetComboWindow( 4.0f );

	CTestComboMonster m1( "monster_headcrab" );
	tracker.OnMonsterKilled( &m1, &player.edict.v, nullptr, 10.0f );

	// Immediately after kill: gauge is 1.0
	tracker.Update( 10.0f );
	CHECK( tracker.GetNormalizedGauge() == Catch::Approx( 1.0f ) );

	// Halfway through window: 10.0 + 2.0 = 12.0s -> gauge is 0.5
	tracker.Update( 12.0f );
	CHECK( tracker.GetNormalizedGauge() == Catch::Approx( 0.5f ) );

	// 75% through window: 10.0 + 3.0 = 13.0s -> gauge is 0.25
	tracker.Update( 13.0f );
	CHECK( tracker.GetNormalizedGauge() == Catch::Approx( 0.25f ) );

	// Expired: 14.1s -> gauge is 0.0
	tracker.Update( 14.1f );
	CHECK( tracker.GetNormalizedGauge() == Catch::Approx( 0.0f ) );
	CHECK( tracker.GetCurrentStreak() == 0 );
}

TEST_CASE( "GladderComboTracker: Headshot detection and bonus window extension", "[gladder][combo]" )
{
	ResetMockEngine();
	CTestPlayerClient player;
	GladderComboTracker tracker;
	tracker.SetComboWindow( 3.0f );

	// Normal body kill at t = 5.0s -> expires at 8.0s
	CTestComboMonster mNormal( "monster_human_grunt", HITGROUP_CHEST );
	tracker.OnMonsterKilled( &mNormal, &player.edict.v, nullptr, 5.0f );
	CHECK( tracker.GetStats().headshotKills == 0 );

	// Headshot kill at t = 6.0s -> window extended by HEADSHOT_WINDOW_BONUS (1.0s) -> expires at 6.0 + 3.0 + 1.0 = 10.0s
	CTestComboMonster mHeadshot( "monster_human_grunt", HITGROUP_HEAD );
	tracker.OnMonsterKilled( &mHeadshot, &player.edict.v, nullptr, 6.0f );

	CHECK( tracker.GetStats().headshotKills == 1 );
	CHECK( tracker.GetCurrentStreak() == 2 );

	// At t = 9.5s, without bonus it would have expired (window was 3.0s, kill was at 6.0s -> 9.0s),
	// but with headshot bonus it remains active until 10.0s!
	tracker.Update( 9.5f );
	CHECK( tracker.IsComboActive() );
	CHECK( tracker.GetNormalizedGauge() > 0.0f );

	// At t = 10.1s, it has expired
	tracker.Update( 10.1f );
	CHECK_FALSE( tracker.IsComboActive() );
}

TEST_CASE( "GladderComboTracker: Itemized species kill logging and friendly names", "[gladder][combo]" )
{
	ResetMockEngine();
	CTestPlayerClient player;
	GladderComboTracker tracker;

	CTestComboMonster crab( "monster_headcrab" );
	CTestComboMonster zombie( "monster_zombie" );
	CTestComboMonster grunt( "monster_human_grunt" );

	tracker.OnMonsterKilled( &crab, &player.edict.v, nullptr, 1.0f );
	tracker.OnMonsterKilled( &crab, &player.edict.v, nullptr, 2.0f );
	tracker.OnMonsterKilled( &crab, &player.edict.v, nullptr, 3.0f );
	tracker.OnMonsterKilled( &zombie, &player.edict.v, nullptr, 4.0f );
	tracker.OnMonsterKilled( &grunt, &player.edict.v, nullptr, 5.0f );

	const auto &stats = tracker.GetStats();
	CHECK( stats.totalFrags == 5 );
	CHECK( tracker.GetSpeciesKills( "monster_headcrab" ) == 3 );
	CHECK( tracker.GetSpeciesKills( "monster_zombie" ) == 1 );
	CHECK( tracker.GetSpeciesKills( "monster_human_grunt" ) == 1 );
	CHECK( tracker.GetSpeciesKills( "monster_vortigaunt" ) == 0 );

	// Verify friendly names
	CHECK( GladderComboTracker::GetFriendlySpeciesName( "monster_headcrab" ) == "Headcrabs" );
	CHECK( GladderComboTracker::GetFriendlySpeciesName( "monster_bullchicken" ) == "Bullsquids" );
	CHECK( GladderComboTracker::GetFriendlySpeciesName( "monster_human_grunt" ) == "HECU Grunts" );
	CHECK( GladderComboTracker::GetFriendlySpeciesName( "monster_human_assassin" ) == "Black Ops Assassins" );
	CHECK( GladderComboTracker::GetFriendlySpeciesName( "monster_alien_slave" ) == "Vortigaunts" );
	CHECK( GladderComboTracker::GetFriendlySpeciesName( "monster_custom_alien" ) == "Custom_alien" );
}

TEST_CASE( "GladderComboTracker: Discard non-player deaths (environmental hazards, infighting, worldspawn) (SPEC §3, §9.1, Issue #60)", "[gladder][combo][attribution]" )
{
	ResetMockEngine();
	GladderComboTracker tracker;

	CTestComboMonster victim( "monster_headcrab" );

	// 1. Null killer and null inflictor (falling damage, suicide, or script)
	tracker.OnMonsterKilled( &victim, nullptr, nullptr, 1.0f );
	CHECK( tracker.GetTotalFrags() == 0 );
	CHECK( tracker.GetCurrentStreak() == 0 );
	CHECK( tracker.GetNormalizedGauge() == Catch::Approx( 0.0f ) );
	CHECK( tracker.GetStats().speciesKills.empty() );

	// 2. Monster infighting: Bullsquid attacks and kills Headcrab
	edict_t bullsquidEdict;
	memset( &bullsquidEdict, 0, sizeof( bullsquidEdict ) );
	bullsquidEdict.v.flags = FL_MONSTER;
	bullsquidEdict.v.pContainingEntity = &bullsquidEdict;

	tracker.OnMonsterKilled( &victim, &bullsquidEdict.v, &bullsquidEdict.v, 2.0f );
	CHECK( tracker.GetTotalFrags() == 0 );
	CHECK( tracker.GetCurrentStreak() == 0 );
	CHECK( tracker.GetStats().speciesKills.empty() );

	// 3. Environmental hazard: trigger_hurt or toxic slime
	edict_t triggerHurtEdict;
	memset( &triggerHurtEdict, 0, sizeof( triggerHurtEdict ) );
	triggerHurtEdict.v.pContainingEntity = &triggerHurtEdict;

	tracker.OnMonsterKilled( &victim, &triggerHurtEdict.v, nullptr, 3.0f );
	CHECK( tracker.GetTotalFrags() == 0 );
	CHECK( tracker.GetCurrentStreak() == 0 );
	CHECK( tracker.GetStats().speciesKills.empty() );

	// 4. Worldspawn (world geometry crush / pit)
	edict_t worldspawnEdict;
	memset( &worldspawnEdict, 0, sizeof( worldspawnEdict ) );
	worldspawnEdict.v.pContainingEntity = &worldspawnEdict;

	tracker.OnMonsterKilled( &victim, &worldspawnEdict.v, &worldspawnEdict.v, 4.0f );
	CHECK( tracker.GetTotalFrags() == 0 );
	CHECK( tracker.GetCurrentStreak() == 0 );
	CHECK( tracker.GetStats().speciesKills.empty() );
}

TEST_CASE( "GladderComboTracker: Direct and indirect player kill attribution (SPEC §3, §9.1, Issue #60)", "[gladder][combo][attribution]" )
{
	ResetMockEngine();
	CTestPlayerClient player;
	GladderComboTracker tracker;

	CTestComboMonster m1( "monster_headcrab" );
	CTestComboMonster m2( "monster_zombie" );
	CTestComboMonster m3( "monster_houndeye" );

	// 1. Direct player kill: killer has FL_CLIENT
	tracker.OnMonsterKilled( &m1, &player.edict.v, nullptr, 1.0f );
	CHECK( tracker.GetTotalFrags() == 1 );
	CHECK( tracker.GetCurrentStreak() == 1 );
	CHECK( tracker.GetSpeciesKills( "monster_headcrab" ) == 1 );

	// 2. Indirect projectile kill: rocket / grenade inflictor with owner pointing to player
	edict_t grenadeEdict;
	memset( &grenadeEdict, 0, sizeof( grenadeEdict ) );
	grenadeEdict.v.owner = &player.edict;
	grenadeEdict.v.pContainingEntity = &grenadeEdict;

	tracker.OnMonsterKilled( &m2, &grenadeEdict.v, &grenadeEdict.v, 2.0f );
	CHECK( tracker.GetTotalFrags() == 2 );
	CHECK( tracker.GetCurrentStreak() == 2 );
	CHECK( tracker.GetSpeciesKills( "monster_zombie" ) == 1 );

	// 3. Indirect breakable crate explosion: killer resolves to player via breakable explosion (#184)
	edict_t explosionEdict;
	memset( &explosionEdict, 0, sizeof( explosionEdict ) );
	explosionEdict.v.owner = &player.edict;
	explosionEdict.v.pContainingEntity = &explosionEdict;

	// In upstream CEnvExplosion::Use, RadiusDamage receives VARS(pev->owner) as pevAttacker
	tracker.OnMonsterKilled( &m3, &player.edict.v, &explosionEdict.v, 3.0f );
	CHECK( tracker.GetTotalFrags() == 3 );
	CHECK( tracker.GetCurrentStreak() == 3 );
	CHECK( tracker.GetSpeciesKills( "monster_houndeye" ) == 1 );
}

TEST_CASE( "GladderComboTracker: IsPlayerInstigated validation logic (SPEC §3, Issue #60)", "[gladder][combo][attribution]" )
{
	CTestPlayerClient player;

	// Null entities
	CHECK_FALSE( GladderComboTracker::IsPlayerInstigated( nullptr, nullptr ) );

	// Direct client
	CHECK( GladderComboTracker::IsPlayerInstigated( &player.edict.v, nullptr ) );
	CHECK( GladderComboTracker::IsPlayerInstigated( nullptr, &player.edict.v ) );

	// Monster without client owner
	edict_t monster;
	memset( &monster, 0, sizeof( monster ) );
	monster.v.flags = FL_MONSTER;
	CHECK_FALSE( GladderComboTracker::IsPlayerInstigated( &monster.v, nullptr ) );

	// Owned by player
	edict_t ownedEntity;
	memset( &ownedEntity, 0, sizeof( ownedEntity ) );
	ownedEntity.v.owner = &player.edict;
	CHECK( GladderComboTracker::IsPlayerInstigated( &ownedEntity.v, nullptr ) );
	CHECK( GladderComboTracker::IsPlayerInstigated( nullptr, &ownedEntity.v ) );

	// Chained owner pointing to player (e.g. projectile spawned by player weapon)
	edict_t childEntity;
	memset( &childEntity, 0, sizeof( childEntity ) );
	childEntity.v.owner = &ownedEntity;
	CHECK( GladderComboTracker::IsPlayerInstigated( &childEntity.v, nullptr ) );

	// Freed owner fallback
	edict_t freedPlayer;
	memset( &freedPlayer, 0, sizeof( freedPlayer ) );
	freedPlayer.v.flags = FL_CLIENT;
	freedPlayer.free = 1;
	edict_t freedChild;
	memset( &freedChild, 0, sizeof( freedChild ) );
	freedChild.v.owner = &freedPlayer;
	CHECK_FALSE( GladderComboTracker::IsPlayerInstigated( &freedChild.v, nullptr ) );
}

TEST_CASE( "GladderComboTracker: Exclude dead monster props and projectile entities from frags and combos (SPEC §3, §9.1, Issue #77)", "[gladder][combo][exclusion]" )
{
	ResetMockEngine();
	CTestPlayerClient player;
	GladderComboTracker tracker;

	// 1. Dead monster corpse props
	const char *deadProps[] = {
		"monster_barney_dead",
		"monster_scientist_dead",
		"monster_hgrunt_dead",
		"monster_hevsuit_dead"
	};

	for ( const char *propName : deadProps )
	{
		CTestComboMonster deadProp( propName );
		deadProp.pev->deadflag = DEAD_DEAD;
		deadProp.m_MonsterState = MONSTERSTATE_DEAD;

		CHECK_FALSE( GladderComboTracker::IsValidCombatant( &deadProp ) );

		tracker.OnMonsterKilled( &deadProp, &player.edict.v, nullptr, 1.0f );
		CHECK( tracker.GetTotalFrags() == 0 );
		CHECK( tracker.GetCurrentStreak() == 0 );
		CHECK( tracker.GetStats().speciesKills.empty() );
	}

	// 2. Hostile biological weapon projectile entities (hornet)
	CTestComboMonster hornet( "hornet" );
	hornet.m_iClassify = CLASS_ALIEN_BIOWEAPON;
	CHECK_FALSE( GladderComboTracker::IsValidCombatant( &hornet ) );

	tracker.OnMonsterKilled( &hornet, &player.edict.v, nullptr, 2.0f );
	CHECK( tracker.GetTotalFrags() == 0 );
	CHECK( tracker.GetCurrentStreak() == 0 );
	CHECK( tracker.GetStats().speciesKills.empty() );

	// Player bioweapon projectile hornet
	CTestComboMonster playerHornet( "hornet" );
	playerHornet.m_iClassify = CLASS_PLAYER_BIOWEAPON;
	CHECK_FALSE( GladderComboTracker::IsValidCombatant( &playerHornet ) );

	tracker.OnMonsterKilled( &playerHornet, &player.edict.v, nullptr, 3.0f );
	CHECK( tracker.GetTotalFrags() == 0 );
	CHECK( tracker.GetCurrentStreak() == 0 );
	CHECK( tracker.GetStats().speciesKills.empty() );

	// 3. Living hostile monsters: must still be counted properly
	CTestComboMonster livingMonster( "monster_headcrab" );
	livingMonster.pev->deadflag = DEAD_NO;
	livingMonster.m_MonsterState = MONSTERSTATE_COMBAT;
	livingMonster.m_iClassify = CLASS_ALIEN_MONSTER;
	CHECK( GladderComboTracker::IsValidCombatant( &livingMonster ) );

	tracker.OnMonsterKilled( &livingMonster, &player.edict.v, nullptr, 4.0f );
	CHECK( tracker.GetTotalFrags() == 1 );
	CHECK( tracker.GetCurrentStreak() == 1 );
	CHECK( tracker.GetSpeciesKills( "monster_headcrab" ) == 1 );
}
