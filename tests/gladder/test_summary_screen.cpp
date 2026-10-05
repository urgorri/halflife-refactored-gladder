/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Post-Match Summary Screen (SPEC §6, §9.2, GitHub Issue #16)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "cl_dll/gladder/hud_gladder_summary.h"

TEST_CASE( "CHudGladderSummary: Activation and Rolling Tally Calculation (#16)", "[gladder][hud][summary]" )
{
	CHudGladderSummary summary;
	CHECK( summary.IsActive() == false );

	GladderSummaryMsg msg;
	msg.bSurvived      = true;
	msg.iWavesCompleted = 10;
	msg.iFinalScore    = 150000;
	msg.cRankGrade     = 'A';

	summary.SetSummaryData( msg, 100.0f );
	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == false );

	// Tally fraction and score progression
	CHECK( summary.GetTallyFraction( 100.0f ) == 0.0f );
	CHECK( summary.GetTallyScore( 100.0f ) == 0 );

	CHECK( summary.GetTallyFraction( 100.75f ) == Catch::Approx( 0.5f ).epsilon( 0.01f ) );
	CHECK( summary.GetTallyScore( 100.75f ) == 75000 );

	CHECK( summary.GetTallyFraction( 101.5f ) == 1.0f );
	CHECK( summary.GetTallyScore( 101.5f ) == 150000 );

	// Beyond tally duration, score remains at maximum
	CHECK( summary.GetTallyFraction( 105.0f ) == 1.0f );
	CHECK( summary.GetTallyScore( 105.0f ) == 150000 );
}

TEST_CASE( "CHudGladderSummary: Tab Switching and Dismissal (#16)", "[gladder][hud][summary]" )
{
	CHudGladderSummary summary;
	GladderSummaryMsg msg;
	msg.iFinalScore = 80000;
	summary.SetSummaryData( msg, 10.0f );

	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == false );

	// Switch to leaderboard tab
	summary.ToggleLeaderboardTab();
	CHECK( summary.IsShowingLeaderboard() == true );

	// Switch back
	summary.ToggleLeaderboardTab();
	CHECK( summary.IsShowingLeaderboard() == false );

	// Dismiss
	summary.Dismiss();
	CHECK( summary.IsActive() == false );
}
