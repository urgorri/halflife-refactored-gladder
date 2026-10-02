/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: GladderComboTracker & Species Kill Log (SPEC §3, §9.1, §9.2, Issue #8)
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
	}

	~CTestComboMonster()
	{
		if ( pev )
		{
			delete pev;
			pev = nullptr;
		}
	}
};

TEST_CASE( "GladderComboTracker: Basic streak progression and decay timer (SPEC §3, Issue #8)", "[gladder][combo]" )
{
	ResetMockEngine();
	GladderComboTracker tracker;
	tracker.Reset();

	CHECK( tracker.GetCurrentStreak() == 0 );
	CHECK( tracker.GetCurrentMultiplier() == 1 );
	CHECK( tracker.GetNormalizedGauge() == Catch::Approx( 0.0f ) );
	CHECK_FALSE( tracker.IsComboActive() );

	// Kill 1 at t = 1.0s
	CTestComboMonster m1( "monster_headcrab" );
	tracker.OnMonsterKilled( &m1, nullptr, nullptr, 1.0f );

	CHECK( tracker.GetCurrentStreak() == 1 );
	CHECK( tracker.GetCurrentMultiplier() == 1 ); // x1 active, no multiplier yet
	CHECK( tracker.GetNormalizedGauge() == Catch::Approx( 1.0f ) );
	CHECK( tracker.GetTotalFrags() == 1 );

	// Kill 2 at t = 2.0s (within 3.5s window -> streak 2)
	CTestComboMonster m2( "monster_zombie" );
	tracker.OnMonsterKilled( &m2, nullptr, nullptr, 2.0f );

	CHECK( tracker.GetCurrentStreak() == 2 );
	CHECK( tracker.GetCurrentMultiplier() == 2 );
	CHECK( tracker.IsComboActive() );
	CHECK( tracker.GetStats().x2Combos == 1 );

	// Kill 3 at t = 3.0s -> streak 3
	CTestComboMonster m3( "monster_houndeye" );
	tracker.OnMonsterKilled( &m3, nullptr, nullptr, 3.0f );

	CHECK( tracker.GetCurrentStreak() == 3 );
	CHECK( tracker.GetCurrentMultiplier() == 3 );
	CHECK( tracker.GetStats().x3Combos == 1 );

	// Kill 4 at t = 4.0s -> streak 4 (x4+)
	CTestComboMonster m4( "monster_bullchicken" );
	tracker.OnMonsterKilled( &m4, nullptr, nullptr, 4.0f );

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
	GladderComboTracker tracker;
	tracker.SetComboWindow( 4.0f );

	CTestComboMonster m1( "monster_headcrab" );
	tracker.OnMonsterKilled( &m1, nullptr, nullptr, 10.0f );

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
	GladderComboTracker tracker;
	tracker.SetComboWindow( 3.0f );

	// Normal body kill at t = 5.0s -> expires at 8.0s
	CTestComboMonster mNormal( "monster_human_grunt", HITGROUP_CHEST );
	tracker.OnMonsterKilled( &mNormal, nullptr, nullptr, 5.0f );
	CHECK( tracker.GetStats().headshotKills == 0 );

	// Headshot kill at t = 6.0s -> window extended by HEADSHOT_WINDOW_BONUS (1.0s) -> expires at 6.0 + 3.0 + 1.0 = 10.0s
	CTestComboMonster mHeadshot( "monster_human_grunt", HITGROUP_HEAD );
	tracker.OnMonsterKilled( &mHeadshot, nullptr, nullptr, 6.0f );

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
	GladderComboTracker tracker;

	CTestComboMonster crab( "monster_headcrab" );
	CTestComboMonster zombie( "monster_zombie" );
	CTestComboMonster grunt( "monster_human_grunt" );

	tracker.OnMonsterKilled( &crab, nullptr, nullptr, 1.0f );
	tracker.OnMonsterKilled( &crab, nullptr, nullptr, 2.0f );
	tracker.OnMonsterKilled( &crab, nullptr, nullptr, 3.0f );
	tracker.OnMonsterKilled( &zombie, nullptr, nullptr, 4.0f );
	tracker.OnMonsterKilled( &grunt, nullptr, nullptr, 5.0f );

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
