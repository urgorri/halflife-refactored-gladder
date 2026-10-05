/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Persistent Local Map Leaderboards (SPEC §10.2, GitHub Issue #17)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "dlls/gladder/gladder_leaderboard.h"
#include <cstdio>
#include <fstream>

static const char *TEST_SCORE_DIR = "test_scores_tmp";

static void CleanupTestFiles()
{
	std::string path = std::string( TEST_SCORE_DIR ) + "/testmap.dat";
	remove( path.c_str() );
}

TEST_CASE( "GladderLeaderboard: Storage Lifecycle and Score Submission (#17)", "[gladder][storage][leaderboard]" )
{
	CleanupTestFiles();
	GladderLeaderboard::SetStorageDirectory( TEST_SCORE_DIR );

	std::vector<GladderLeaderboardEntry> initial;
	CHECK( GladderLeaderboard::Load( "testmap", initial ) == false );

	GladderLeaderboardEntry e1;
	e1.timestamp = 1000;
	e1.score     = 50000;
	e1.waves     = 5;
	e1.frags     = 60;
	e1.totalTime = 300.0f;
	e1.grade     = 'B';
	e1.survived  = false;

	int rank = 0;
	bool isHighScore = false;
	bool qualified = GladderLeaderboard::SubmitScore( "testmap", e1, rank, isHighScore );

	CHECK( qualified == true );
	CHECK( rank == 1 );
	CHECK( isHighScore == true );

	// Verify loaded back
	std::vector<GladderLeaderboardEntry> loaded;
	REQUIRE( GladderLeaderboard::Load( "testmap", loaded ) == true );
	REQUIRE( loaded.size() == 1 );
	CHECK( loaded[0].score == 50000 );
	CHECK( loaded[0].waves == 5 );
	CHECK( loaded[0].grade == 'B');

	// Submit a higher score (new #1)
	GladderLeaderboardEntry e2;
	e2.timestamp = 2000;
	e2.score     = 120000;
	e2.waves     = 10;
	e2.frags     = 150;
	e2.totalTime = 550.0f;
	e2.grade     = 'S';
	e2.survived  = true;

	qualified = GladderLeaderboard::SubmitScore( "testmap", e2, rank, isHighScore );
	CHECK( qualified == true );
	CHECK( rank == 1 );
	CHECK( isHighScore == true );

	// Submit a lower score (rank #3)
	GladderLeaderboardEntry e3;
	e3.timestamp = 3000;
	e3.score     = 30000;
	e3.waves     = 3;
	e3.frags     = 40;
	e3.totalTime = 200.0f;
	e3.grade     = 'C';
	e3.survived  = false;

	qualified = GladderLeaderboard::SubmitScore( "testmap", e3, rank, isHighScore );
	CHECK( qualified == true );
	CHECK( rank == 3 );
	CHECK( isHighScore == false );

	REQUIRE( GladderLeaderboard::Load( "testmap", loaded ) == true );
	REQUIRE( loaded.size() == 3 );
	CHECK( loaded[0].score == 120000 );
	CHECK( loaded[1].score == 50000 );
	CHECK( loaded[2].score == 30000 );

	CleanupTestFiles();
}

TEST_CASE( "GladderLeaderboard: Top 10 Capacity and Ranking Overflow (#17)", "[gladder][storage][leaderboard]" )
{
	CleanupTestFiles();
	GladderLeaderboard::SetStorageDirectory( TEST_SCORE_DIR );

	// Insert 10 scores with incrementing values 10k..100k
	for ( int i = 1; i <= 10; ++i )
	{
		GladderLeaderboardEntry e;
		e.timestamp = 1000 + i;
		e.score     = i * 10000;
		e.waves     = i;
		e.frags     = i * 10;
		e.totalTime = 100.0f * i;
		e.grade     = 'B';
		e.survived  = true;

		int rank = 0;
		bool isNewHigh = false;
		GladderLeaderboard::SubmitScore( "testmap", e, rank, isNewHigh );
	}

	std::vector<GladderLeaderboardEntry> loaded;
	REQUIRE( GladderLeaderboard::Load( "testmap", loaded ) == true );
	REQUIRE( loaded.size() == 10 );
	CHECK( loaded[0].score == 100000 ); // Top score
	CHECK( loaded[9].score == 10000 );  // 10th score

	// Submit score that qualifies for rank 5 (e.g. 55000)
	GladderLeaderboardEntry mid;
	mid.timestamp = 5000;
	mid.score     = 55000;
	mid.waves     = 6;
	mid.frags     = 65;
	mid.totalTime = 350.0f;
	mid.grade     = 'A';
	mid.survived  = true;

	int rank = 0;
	bool isNewHigh = false;
	bool qualified = GladderLeaderboard::SubmitScore( "testmap", mid, rank, isNewHigh );
	CHECK( qualified == true );
	CHECK( rank == 6 ); // 100k, 90k, 80k, 70k, 60k, 55k -> rank 6
	CHECK( isNewHigh == false );

	REQUIRE( GladderLeaderboard::Load( "testmap", loaded ) == true );
	REQUIRE( loaded.size() == 10 ); // Still capped at 10
	CHECK( loaded[5].score == 55000 );
	CHECK( loaded[9].score == 20000 ); // Old 10k dropped off

	// Submit score that fails to qualify (e.g. 5000)
	GladderLeaderboardEntry low;
	low.timestamp = 6000;
	low.score     = 5000;
	low.waves     = 1;
	low.frags     = 5;
	low.totalTime = 50.0f;
	low.grade     = 'F';
	low.survived  = false;

	qualified = GladderLeaderboard::SubmitScore( "testmap", low, rank, isNewHigh );
	CHECK( qualified == false );
	CHECK( rank == -1 );

	CleanupTestFiles();
}

TEST_CASE( "GladderLeaderboard: Tie-Breaking Priority (#17)", "[gladder][storage][leaderboard]" )
{
	CleanupTestFiles();
	GladderLeaderboard::SetStorageDirectory( TEST_SCORE_DIR );

	std::vector<GladderLeaderboardEntry> entries;

	// Same score (50000), but e1 has more waves (6 vs 4)
	GladderLeaderboardEntry e1;
	e1.timestamp = 1000;
	e1.score     = 50000;
	e1.waves     = 6;
	e1.totalTime = 200.0f;

	GladderLeaderboardEntry e2;
	e2.timestamp = 2000;
	e2.score     = 50000;
	e2.waves     = 4;
	e2.totalTime = 200.0f;

	// Same score and waves (50000, 6 waves), but e3 is faster (150s vs 200s)
	GladderLeaderboardEntry e3;
	e3.timestamp = 3000;
	e3.score     = 50000;
	e3.waves     = 6;
	e3.totalTime = 150.0f;

	entries.push_back( e2 );
	entries.push_back( e1 );
	entries.push_back( e3 );

	GladderLeaderboard::SortAndTrim( entries );

	CHECK( entries[0].totalTime == 150.0f ); // e3: 50k, 6 waves, 150s
	CHECK( entries[1].totalTime == 200.0f ); // e1: 50k, 6 waves, 200s
	CHECK( entries[2].waves == 4 );          // e2: 50k, 4 waves, 200s

	CleanupTestFiles();
}

TEST_CASE( "GladderLeaderboard: Corrupted File Resilience (#17)", "[gladder][storage][leaderboard]" )
{
	CleanupTestFiles();
	GladderLeaderboard::SetStorageDirectory( TEST_SCORE_DIR );
	GladderLeaderboard::EnsureDirectoryExists( TEST_SCORE_DIR );

	std::string path = std::string( TEST_SCORE_DIR ) + "/testmap.dat";

	// Write garbage file
	std::ofstream out( path, std::ios::binary );
	out << "CORRUPTED_GARBAGE_DATA_1234567890";
	out.close();

	std::vector<GladderLeaderboardEntry> loaded;
	// Must fail gracefully without crashing
	CHECK( GladderLeaderboard::Load( "testmap", loaded ) == false );
	CHECK( loaded.empty() );

	CleanupTestFiles();
}
