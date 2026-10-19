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

	// Verify grid coordinates (cx = 320 for 640x480, lbWidth = 520, tableX = 60) (SPEC §10.2, #73, #112)
	int lbWidth = (std::max)( 520, (std::min)( 960, static_cast<int>( 640 * 0.75f ) ) );
	int tableX    = ( 640 - lbWidth ) / 2;
	int colRankX   = tableX;
	int colScoreX  = tableX + static_cast<int>( lbWidth * 0.12f );
	int colWavesX  = tableX + static_cast<int>( lbWidth * 0.32f );
	int colFragsX  = tableX + static_cast<int>( lbWidth * 0.46f );
	int colTimeX   = tableX + static_cast<int>( lbWidth * 0.60f );
	int colGradeX  = tableX + static_cast<int>( lbWidth * 0.74f );
	int colStatusX = tableX + static_cast<int>( lbWidth * 0.87f );

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

TEST_CASE( "CHudGladderSummary: gladder_restart resolves map and issues reload command (SPEC §10.4, Issue #82)", "[gladder][hud][restart]" )
{
	InitMockClientEngine();
	CHudGladderSummary summary;
	g_mockClientCmds.clear();

	SECTION( "Resolves active map name and issues map reload" )
	{
		g_mockMapName = "maps/gl01.bsp";
		CHudGladderSummary::RestartMatch();
		REQUIRE_FALSE( g_mockClientCmds.empty() );
		CHECK( g_mockClientCmds.back() == "map gl01\n" );
	}

	SECTION( "Falls back to restart if map name is empty" )
	{
		g_mockMapName = "";
		CHudGladderSummary::RestartMatch();
		REQUIRE_FALSE( g_mockClientCmds.empty() );
		CHECK( g_mockClientCmds.back() == "restart\n" );
	}
}

TEST_CASE( "CHudGladderSummary: Summary and Leaderboard footer displays ESC shortcut (SPEC §9.2, Issue #113)", "[gladder][hud][summary][footer]" )
{
	ResetMockClientEngine();
	SetMockScreenInfo( 640, 480 );

	CHudGladderSummary summary;
	summary.Init();

	GladderSummaryMsg msg;
	msg.iFinalScore = 100000;
	msg.bSurvived   = true;
	msg.cRankGrade  = 'A';
	summary.SetSummaryData( msg, 10.0f );

	// 1. On summary page: verify ESC footer prompt is present
	g_mockConsoleStrings.clear();
	summary.Draw( 10.0f );

	bool foundSummaryFooter = false;
	for ( const auto &call : g_mockConsoleStrings )
	{
		if ( call.text.find( "[ESC] Game Menu / Restart Match" ) != std::string::npos &&
		     call.text.find( "[TAB] View High Scores" ) != std::string::npos )
		{
			foundSummaryFooter = true;
			break;
		}
	}
	CHECK( foundSummaryFooter == true );

	// 2. On leaderboard page: verify ESC footer prompt is present
	summary.ToggleLeaderboardTab();
	g_mockConsoleStrings.clear();
	summary.Draw( 10.0f );

	bool foundLeaderboardFooter = false;
	for ( const auto &call : g_mockConsoleStrings )
	{
		if ( call.text.find( "[ESC] Game Menu / Restart Match" ) != std::string::npos &&
		     call.text.find( "[TAB] Return to Summary" ) != std::string::npos )
		{
			foundLeaderboardFooter = true;
			break;
		}
	}
	CHECK( foundLeaderboardFooter == true );
}

TEST_CASE( "CHudGladderSummary: Responsive multi-resolution layout scaling (SPEC §9.2, §10.2, Issue #112)", "[gladder][hud][summary][responsive]" )
{
	ResetMockClientEngine();
	CHudGladderSummary summary;
	summary.Init();

	GladderSummaryMsg msg;
	msg.iFinalScore = 150000;
	msg.bSurvived   = true;
	msg.cRankGrade  = 'S';
	msg.speciesLog.push_back( { "Headcrabs", 10 } );
	summary.SetSummaryData( msg, 10.0f );

	const int testResolutions[][2] = {
		{ 640, 480 },
		{ 1024, 768 },
		{ 1920, 1080 },
		{ 2560, 1440 }
	};

	for ( const auto &res : testResolutions )
	{
		int w = res[0];
		int h = res[1];
		SetMockScreenInfo( w, h );

		// 1. Verify summary multi-column layout is centered and non-overlapping
		g_mockConsoleStrings.clear();
		summary.Draw( 10.0f );

		int col1X = -1, col2X = -1, col3X = -1;
		for ( const auto &call : g_mockConsoleStrings )
		{
			if ( call.text == "-- SCORE BREAKDOWN --" ) col1X = call.x;
			if ( call.text == "-- PACING TELEMETRY --" ) col2X = call.x;
			if ( call.text == "-- SPECIES KILL LOG --" ) col3X = call.x;
		}

		REQUIRE( col1X > 0 );
		REQUIRE( col2X > col1X );
		REQUIRE( col3X > col2X );

		// Verify columns are spaced with at least 150px column width
		CHECK( ( col2X - col1X ) >= 150 );
		CHECK( ( col3X - col2X ) >= 150 );

		// Verify symmetric centering: left margin roughly equals right margin
		int expectedContainerW = (std::max)( 560, (std::min)( 1100, static_cast<int>( w * 0.82f ) ) );
		if ( expectedContainerW > w - 32 ) expectedContainerW = w - 32;
		int expectedStartX = ( w - expectedContainerW ) / 2;
		CHECK( col1X == expectedStartX );
	}
}

