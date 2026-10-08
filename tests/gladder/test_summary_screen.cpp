/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Post-Match Summary Screen (SPEC §6, §9.2, GitHub Issue #16)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "tests/mock_client_engine.h"
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

TEST_CASE( "CHudGladderSummary: In-Game Leaderboard Peek Mode (#16)", "[gladder][hud][summary]" )
{
	CHudGladderSummary summary;
	CHECK( summary.IsActive() == false );

	summary.ShowLeaderboardOnly();
	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == true );

	summary.Dismiss();
	CHECK( summary.IsActive() == false );
}

TEST_CASE( "CHudGladderSummary: Post-match summary overrides peek leaderboard and toggles cleanly (#58)", "[gladder][hud][summary]" )
{
	CHudGladderSummary summary;

	// In-game peek mode active
	summary.ShowLeaderboardOnly();
	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == true );

	// Match ends: SetSummaryData arrives and MUST prioritize the summary page
	GladderSummaryMsg msg;
	msg.iFinalScore = 120000;
	msg.bSurvived   = false;
	summary.SetSummaryData( msg, 50.0f );

	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == false );

	// Pressing TAB toggles to leaderboard
	summary.ToggleLeaderboardTab();
	CHECK( summary.IsShowingLeaderboard() == true );

	// Pressing TAB again returns to summary
	summary.ToggleLeaderboardTab();
	CHECK( summary.IsShowingLeaderboard() == false );

	// Dismissing clears active state and resets leaderboard visibility
	summary.Dismiss();
	CHECK( summary.IsActive() == false );
	CHECK( summary.IsShowingLeaderboard() == false );

	// ToggleDisplay opens to summary page
	summary.ToggleDisplay();
	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == false );
	summary.ToggleDisplay();
	CHECK( summary.IsActive() == false );
}

TEST_CASE( "CHudGladderSummary: Undismissable post-match summary screen ignores dismiss inputs (SPEC §9.2, #61)", "[gladder][hud][summary]" )
{
	ResetMockClientEngine();
	CHudGladderSummary summary;

	GladderSummaryMsg msg;
	msg.iFinalScore = 200000;
	msg.bSurvived   = true;
	msg.cRankGrade  = 'S';
	summary.SetSummaryData( msg, 10.0f );

	REQUIRE( summary.IsActive() == true );
	REQUIRE( summary.IsShowingLeaderboard() == false );

	// Simulate USE key down and up transitions through Think()
	in_use.state = 1;
	summary.Think();
	CHECK( summary.IsActive() == true );

	in_use.state = 0;
	summary.Think();
	CHECK( summary.IsActive() == true );

	// Pressing TAB toggles to leaderboard
	in_score.state = 1;
	summary.Think();
	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == true );

	in_score.state = 0;
	summary.Think();
	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == true );

	// Pressing TAB again returns to summary view
	in_score.state = 1;
	summary.Think();
	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == false );

	in_score.state = 0;
	summary.Think();
	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == false );

	// Releasing TAB in post-match summary mode NEVER dismisses the screen
	CHECK( summary.IsActive() == true );
}

TEST_CASE( "CHudGladderSummary: In-game peek mode dismisses on TAB key release (SPEC §9.2, #61)", "[gladder][hud][summary]" )
{
	ResetMockClientEngine();
	CHudGladderSummary summary;
	CHECK( summary.IsActive() == false );

	// Alive during gameplay: player presses and holds TAB
	in_score.state = 1;
	summary.Think();
	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == true );

	// Keeping TAB pressed maintains leaderboard peek
	summary.Think();
	CHECK( summary.IsActive() == true );
	CHECK( summary.IsShowingLeaderboard() == true );

	// Player releases TAB: immediately dismisses leaderboard peek
	in_score.state = 0;
	summary.Think();
	CHECK( summary.IsActive() == false );
	CHECK( summary.IsShowingLeaderboard() == false );
}

TEST_CASE( "CHudGladderSummary: Leaderboard renders as column-aligned tabular grid (SPEC §10.2, #73)", "[gladder][hud][summary][leaderboard]" )
{
	ResetMockClientEngine();
	SetMockScreenInfo( 640, 480 );

	CHudGladderSummary summary;
	summary.Init();

	std::vector<GladderLeaderboardEntry> mockEntries;
	GladderLeaderboardEntry e1;
	e1.score = 250000;
	e1.waves = 12;
	e1.frags = 145;
	e1.totalTime = 385.0f; // 06:25
	e1.grade = 'S';
	e1.survived = true;

	GladderLeaderboardEntry e2;
	e2.score = 110000;
	e2.waves = 6;
	e2.frags = 72;
	e2.totalTime = 210.0f; // 03:30
	e2.grade = 'B';
	e2.survived = false;

	mockEntries.push_back( e1 );
	mockEntries.push_back( e2 );

	summary.ShowLeaderboardOnly();
	summary.SetLeaderboard( mockEntries, 1, true );

	g_mockConsoleStrings.clear();
	summary.Draw( 10.0f );

	// Verify grid coordinates (cx = 320 for 640x480)
	int cx = 320;
	int tableX = cx - 230; // 90
	int colRankX   = tableX;       // 90
	int colScoreX  = tableX + 55;  // 145
	int colWavesX  = tableX + 150; // 240
	int colFragsX  = tableX + 215; // 305
	int colTimeX   = tableX + 280; // 370
	int colGradeX  = tableX + 350; // 440
	int colStatusX = tableX + 410; // 500

	// Verify headers rendered at their dedicated column coordinates
	bool foundRankHeader = false;
	bool foundScoreHeader = false;
	bool foundWavesHeader = false;
	bool foundFragsHeader = false;
	bool foundTimeHeader = false;
	bool foundGradeHeader = false;
	bool foundStatusHeader = false;

	for ( const auto &call : g_mockConsoleStrings )
	{
		if ( call.x == colRankX && call.text == "RANK" ) foundRankHeader = true;
		if ( call.x == colScoreX && call.text == "SCORE" ) foundScoreHeader = true;
		if ( call.x == colWavesX && call.text == "WAVES" ) foundWavesHeader = true;
		if ( call.x == colFragsX && call.text == "FRAGS" ) foundFragsHeader = true;
		if ( call.x == colTimeX && call.text == "TIME" ) foundTimeHeader = true;
		if ( call.x == colGradeX && call.text == "GRADE" ) foundGradeHeader = true;
		if ( call.x == colStatusX && call.text == "STATUS" ) foundStatusHeader = true;
	}

	CHECK( foundRankHeader == true );
	CHECK( foundScoreHeader == true );
	CHECK( foundWavesHeader == true );
	CHECK( foundFragsHeader == true );
	CHECK( foundTimeHeader == true );
	CHECK( foundGradeHeader == true );
	CHECK( foundStatusHeader == true );

	// Verify entry cells are rendered at the exact matching column coordinates
	bool foundRank1 = false;
	bool foundScore1 = false;
	bool foundWaves1 = false;
	bool foundFrags1 = false;
	bool foundTime1 = false;
	bool foundGrade1 = false;
	bool foundStatus1 = false;

	bool foundRank2 = false;
	bool foundScore2 = false;
	bool foundWaves2 = false;
	bool foundFrags2 = false;
	bool foundTime2 = false;
	bool foundGrade2 = false;
	bool foundStatus2 = false;

	for ( const auto &call : g_mockConsoleStrings )
	{
		// Entry 1
		if ( call.x == colRankX && call.text == "#1" ) foundRank1 = true;
		if ( call.x == colScoreX && call.text == "250000" ) foundScore1 = true;
		if ( call.x == colWavesX && call.text == "12" ) foundWaves1 = true;
		if ( call.x == colFragsX && call.text == "145" ) foundFrags1 = true;
		if ( call.x == colTimeX && call.text == "06:25" ) foundTime1 = true;
		if ( call.x == colGradeX && call.text == "S" ) foundGrade1 = true;
		if ( call.x == colStatusX && call.text == "SURVIVED" ) foundStatus1 = true;

		// Entry 2
		if ( call.x == colRankX && call.text == "#2" ) foundRank2 = true;
		if ( call.x == colScoreX && call.text == "110000" ) foundScore2 = true;
		if ( call.x == colWavesX && call.text == "6" ) foundWaves2 = true;
		if ( call.x == colFragsX && call.text == "72" ) foundFrags2 = true;
		if ( call.x == colTimeX && call.text == "03:30" ) foundTime2 = true;
		if ( call.x == colGradeX && call.text == "B" ) foundGrade2 = true;
		if ( call.x == colStatusX && call.text == "KIA" ) foundStatus2 = true;
	}

	CHECK( foundRank1 == true );
	CHECK( foundScore1 == true );
	CHECK( foundWaves1 == true );
	CHECK( foundFrags1 == true );
	CHECK( foundTime1 == true );
	CHECK( foundGrade1 == true );
	CHECK( foundStatus1 == true );

	CHECK( foundRank2 == true );
	CHECK( foundScore2 == true );
	CHECK( foundWaves2 == true );
	CHECK( foundFrags2 == true );
	CHECK( foundTime2 == true );
	CHECK( foundGrade2 == true );
	CHECK( foundStatus2 == true );
}

