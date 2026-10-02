/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: GladderScoring & Arcade Performance Grading (SPEC §3, §9.2, Issue #9)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "dlls/gladder/gladder_scoring.h"

TEST_CASE( "GladderScoring: Base species point calculation by tier (SPEC §3)", "[gladder][scoring]" )
{
	// Tier 1: Fodder
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_headcrab" ) == 50 );
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_babycrab" ) == 50 );
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_snark" ) == 50 );

	// Tier 2: Basic threats
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_zombie" ) == 100 );
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_houndeye" ) == 100 );
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_barnacle" ) == 100 );

	// Tier 3: Medium threats
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_bullchicken" ) == 250 );
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_vortigaunt" ) == 250 );
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_alien_slave" ) == 250 );

	// Tier 4: Heavy threats
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_alien_grunt" ) == 500 );
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_human_grunt" ) == 500 );
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_human_assassin" ) == 500 );
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_alien_controller" ) == 500 );

	// Tier 5: Apex threats
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_gargantua" ) == 1000 );
	CHECK( GladderScoring::GetSpeciesBasePoints( "monster_bigmomma" ) == 1000 );
}

TEST_CASE( "GladderScoring: Composite formula components tallying", "[gladder][scoring]" )
{
	GladderKillStats kills;
	kills.speciesKills["monster_headcrab"] = 10;   // 10 * 50 = 500
	kills.speciesKills["monster_zombie"]   = 5;    // 5 * 100 = 500
	kills.speciesKills["monster_human_grunt"] = 2; // 2 * 500 = 1000
	// Base combat total = 2000

	kills.totalFrags     = 17;
	kills.headshotKills  = 4;  // 4 * 50 = 200
	kills.x2Combos       = 3;  // 3 * 150 = 450
	kills.x3Combos       = 2;  // 2 * 350 = 700
	kills.x4PlusCombos   = 1;  // 1 * 750 = 750
	kills.maxComboStreak = 5;  // 5 * 250 = 1250
	// Combo total = 200 + 450 + 700 + 750 + 1250 = 3350

	int waves = 5;
	float avgLap = 45.0f; // <= 60s -> qualifies for pacing bonus (5 * 1000 base + 5 * 500 pacing = 7500)
	int lambdas = 2;      // 2 * 2500 = 5000
	float damageTaken = 100.0f; // -50 pts penalty
	bool bSurvived = true;      // +5000 pts

	auto bd = GladderScoring::CalculateScore( kills, waves, lambdas, 300.0f, avgLap, 35.0f, 55.0f, damageTaken, bSurvived );

	CHECK( bd.baseCombatPoints == 2000 );
	CHECK( bd.comboBonusPoints == 3350 );
	CHECK( bd.waveBonusPoints == 7500 );
	CHECK( bd.lambdaBonusPoints == 5000 );
	CHECK( bd.survivalBonusPoints == 5000 );
	CHECK( bd.damagePenaltyPoints == 50 );

	int expectedFinal = 2000 + 3350 + 7500 + 5000 + 5000 - 50;
	CHECK( bd.finalCompositeScore == expectedFinal );
	CHECK( bd.rankGrade == 'B' ); // waves=5 (< 7 required for A), score > 20000 -> B rank
}

TEST_CASE( "GladderScoring: Performance rank boundaries (S, A, B, C, D, F)", "[gladder][scoring]" )
{
	std::string title;

	// S Rank: waves >= 10, score >= 35000, maxCombo >= 5
	CHECK( GladderScoring::DetermineRankGrade( 40000, 12, 6, 50.0f, true, title ) == 'S' );
	CHECK( title == "S - GAUNTLET MASTER" );

	// A Rank: waves >= 7, score >= 20000, maxCombo >= 3
	CHECK( GladderScoring::DetermineRankGrade( 25000, 8, 4, 120.0f, true, title ) == 'A' );
	CHECK( title == "A - VETERAN RUNNER" );

	// B Rank: waves >= 4, score >= 10000
	CHECK( GladderScoring::DetermineRankGrade( 12000, 5, 2, 200.0f, false, title ) == 'B' );
	CHECK( title == "B - SOLID CONTENDER" );

	// C Rank: waves >= 2, score >= 4000
	CHECK( GladderScoring::DetermineRankGrade( 5000, 3, 1, 150.0f, false, title ) == 'C' );
	CHECK( title == "C - APPRENTICE RUNNER" );

	// D Rank: waves >= 1, score >= 1000
	CHECK( GladderScoring::DetermineRankGrade( 1500, 1, 1, 90.0f, false, title ) == 'D' );
	CHECK( title == "D - NOVICE SURVIVOR" );

	// F Rank: 0 waves or low score
	CHECK( GladderScoring::DetermineRankGrade( 300, 0, 0, 100.0f, false, title ) == 'F' );
	CHECK( title == "F - FALLEN RECRUIT" );
}

TEST_CASE( "GladderScoring: Edge cases and death flow preservation", "[gladder][scoring]" )
{
	// Death is not a failure: player dying at wave 6 keeps all accumulated combat points
	GladderKillStats kills;
	kills.speciesKills["monster_headcrab"] = 20;
	kills.totalFrags = 20;
	kills.maxComboStreak = 3;

	auto bd = GladderScoring::CalculateScore( kills, 3, 1, 120.0f, 40.0f, 30.0f, 50.0f, 150.0f, false );

	CHECK( bd.survivalBonusPoints == 0 );
	CHECK( bd.baseCombatPoints == 1000 );
	CHECK( bd.lambdaBonusPoints == 2500 );
	CHECK( bd.finalCompositeScore > 0 );
	CHECK( bd.totalDamageTaken == Catch::Approx( 150.0f ) );
	CHECK_FALSE( bd.bSurvived );
}
