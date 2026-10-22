/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Custom Network User Messages (SPEC §9.1, §9.2, §12, GitHub Issue #14)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "cl_dll/gladder/gladder_cl_usermsg.h"
#include "dlls/gladder/gladder_usermsg.h"
#include <vector>
#include <cstring>

static inline void WriteInt16( std::vector<uint8_t> &buf, int16_t val )
{
	buf.push_back( static_cast<uint8_t>( val & 0xFF ) );
	buf.push_back( static_cast<uint8_t>( ( val >> 8 ) & 0xFF ) );
}

static inline void WriteInt32( std::vector<uint8_t> &buf, int32_t val )
{
	buf.push_back( static_cast<uint8_t>( val & 0xFF ) );
	buf.push_back( static_cast<uint8_t>( ( val >> 8 ) & 0xFF ) );
	buf.push_back( static_cast<uint8_t>( ( val >> 16 ) & 0xFF ) );
	buf.push_back( static_cast<uint8_t>( ( val >> 24 ) & 0xFF ) );
}

static inline void WriteCoord( std::vector<uint8_t> &buf, float val )
{
	int16_t raw = static_cast<int16_t>( val * 8.0f );
	WriteInt16( buf, raw );
}

TEST_CASE( "Gladder UserMsg: GladWave Serialization Symmetry (#14, #118)", "[gladder][hud][usermsg]" )
{
	std::vector<uint8_t> buf;
	buf.push_back( 1 ); // Active
	WriteInt16( buf, 7 ); // Wave 7
	WriteCoord( buf, 450.0f ); // Time left
	WriteCoord( buf, 35.5f ); // Lap time
	WriteInt16( buf, 14 ); // Monsters killed
	WriteInt16( buf, 20 ); // Monsters total

	REQUIRE( buf.size() == GLADDER_MSG_WAVE_SIZE );

	GladderWaveMsg msg;
	REQUIRE( Gladder_ParseWaveMsg( buf.data(), static_cast<int>( buf.size() ), msg ) == true );
	CHECK( msg.iWaveState == 1 );
	CHECK( msg.iWaveNumber == 7 );
	CHECK( msg.flSessionTimeRemaining == Catch::Approx( 450.0f ).epsilon( 0.2f ) );
	CHECK( msg.flLapTime == Catch::Approx( 35.5f ).epsilon( 0.2f ) );
	CHECK( msg.iMonstersKilled == 14 );
	CHECK( msg.iMonstersTotal == 20 );

	// Truncated buffer check
	CHECK( Gladder_ParseWaveMsg( buf.data(), 5, msg ) == false );

	// Legacy 7-byte buffer backwards-compatibility check
	std::vector<uint8_t> legacyBuf( buf.begin(), buf.begin() + 7 );
	GladderWaveMsg legacyMsg;
	REQUIRE( Gladder_ParseWaveMsg( legacyBuf.data(), static_cast<int>( legacyBuf.size() ), legacyMsg ) == true );
	CHECK( legacyMsg.iWaveState == 1 );
	CHECK( legacyMsg.iMonstersKilled == 0 );
	CHECK( legacyMsg.iMonstersTotal == 0 );
}

TEST_CASE( "Gladder UserMsg: GladTelem Serialization Symmetry (#14)", "[gladder][hud][usermsg]" )
{
	std::vector<uint8_t> buf;
	WriteCoord( buf, 28.0f ); // Fastest
	WriteCoord( buf, 64.0f ); // Slowest
	WriteCoord( buf, 42.0f ); // Average
	WriteInt16( buf, 8 );    // Completed laps
	WriteInt16( buf, 95 );   // Frags
	WriteInt16( buf, 12 );   // Collectibles

	REQUIRE( buf.size() == GLADDER_MSG_TELEMETRY_SIZE );

	GladderTelemetryMsg msg;
	REQUIRE( Gladder_ParseTelemetryMsg( buf.data(), static_cast<int>( buf.size() ), msg ) == true );
	CHECK( msg.flFastestLap == Catch::Approx( 28.0f ).epsilon( 0.2f ) );
	CHECK( msg.flSlowestLap == Catch::Approx( 64.0f ).epsilon( 0.2f ) );
	CHECK( msg.flAverageLap == Catch::Approx( 42.0f ).epsilon( 0.2f ) );
	CHECK( msg.iCompletedLaps == 8 );
	CHECK( msg.iFrags == 95 );
	CHECK( msg.iCollectibles == 12 );
}

TEST_CASE( "Gladder UserMsg: GladCombo Serialization Symmetry (#14)", "[gladder][hud][usermsg]" )
{
	std::vector<uint8_t> buf;
	buf.push_back( 4 );   // Multiplier x4
	buf.push_back( 191 ); // ~75% decay fraction

	REQUIRE( buf.size() == GLADDER_MSG_COMBO_SIZE );

	GladderComboMsg msg;
	REQUIRE( Gladder_ParseComboMsg( buf.data(), static_cast<int>( buf.size() ), msg ) == true );
	CHECK( msg.iComboMultiplier == 4 );
	CHECK( msg.flDecayFraction == Catch::Approx( 0.749f ).epsilon( 0.01f ) );

	// Truncated buffer check
	CHECK( Gladder_ParseComboMsg( buf.data(), 1, msg ) == false );
}

TEST_CASE( "Gladder UserMsg: GladNotify Serialization Symmetry (#14, #25, #127)", "[gladder][hud][usermsg]" )
{
	std::vector<uint8_t> buf;
	buf.push_back( 0 ); // Started
	buf.push_back( 5 ); // Wave 5
	buf.push_back( 2 ); // Low gravity
	WriteInt16( buf, 24 ); // Hostile quota

	REQUIRE( buf.size() == GLADDER_MSG_NOTIFY_SIZE );

	GladderNotifyMsg msg;
	REQUIRE( Gladder_ParseNotifyMsg( buf.data(), static_cast<int>( buf.size() ), msg ) == true );
	CHECK( msg.iType == 0 );
	CHECK( msg.iWaveNumber == 5 );
	CHECK( msg.iMutatorType == 2 );
	CHECK( msg.iHostileQuota == 24 );

	// Complete notification carries no quota
	buf[0] = 1;
	buf[2] = 0;
	buf[3] = 0; // Quota low byte
	buf[4] = 0; // Quota high byte
	REQUIRE( buf.size() == GLADDER_MSG_NOTIFY_SIZE );
	REQUIRE( Gladder_ParseNotifyMsg( buf.data(), static_cast<int>( buf.size() ), msg ) == true );
	CHECK( msg.iType == 1 );
	CHECK( msg.iWaveNumber == 5 );
	CHECK( msg.iMutatorType == 0 );
	CHECK( msg.iHostileQuota == 0 );

	// Legacy 3-byte buffer is rejected (quota field absent)
	CHECK( Gladder_ParseNotifyMsg( buf.data(), 3, msg ) == false );
	// Empty buffer is rejected
	CHECK( Gladder_ParseNotifyMsg( buf.data(), 0, msg ) == false );
}

TEST_CASE( "Gladder UserMsg: GladSumm Serialization Symmetry (#14, #16)", "[gladder][hud][usermsg]" )
{
	std::vector<uint8_t> buf;
	buf.push_back( 1 ); // Survived
	WriteInt16( buf, 15 ); // Waves
	WriteInt32( buf, 275000 ); // Final Score
	buf.push_back( 'S' ); // Rank Grade
	WriteInt16( buf, 8 ); // Max combo
	WriteInt16( buf, 210 ); // Total frags
	WriteInt16( buf, 16 ); // Collectibles
	WriteCoord( buf, 600.0f ); // Total match time
	WriteCoord( buf, 40.0f ); // Avg lap
	WriteCoord( buf, 25.0f ); // Fastest
	WriteCoord( buf, 55.0f ); // Slowest
	WriteCoord( buf, 120.0f ); // Damage taken
	WriteInt32( buf, 150000 ); // Combat
	WriteInt32( buf, 45000 ); // Combos
	WriteInt32( buf, 30000 ); // Waves
	WriteInt32( buf, 40000 ); // Lambdas
	WriteInt32( buf, 15000 ); // Survival
	WriteInt32( buf, 5000 ); // Penalty

	// Species pairs
	buf.push_back( 3 ); // 3 species

	// Species 1: Headcrab (45)
	const char *sp1 = "Headcrabs";
	buf.insert( buf.end(), sp1, sp1 + strlen( sp1 ) + 1 );
	WriteInt16( buf, 45 );

	// Species 2: Zombies (30)
	const char *sp2 = "Zombies";
	buf.insert( buf.end(), sp2, sp2 + strlen( sp2 ) + 1 );
	WriteInt16( buf, 30 );

	// Species 3: Vortigaunts (18)
	const char *sp3 = "Vortigaunts";
	buf.insert( buf.end(), sp3, sp3 + strlen( sp3 ) + 1 );
	WriteInt16( buf, 18 );

	GladderSummaryMsg msg;
	REQUIRE( Gladder_ParseSummaryMsg( buf.data(), static_cast<int>( buf.size() ), msg ) == true );
	CHECK( msg.bSurvived == true );
	CHECK( msg.iWavesCompleted == 15 );
	CHECK( msg.iFinalScore == 275000 );
	CHECK( msg.cRankGrade == 'S' );
	CHECK( msg.iMaxComboStreak == 8 );
	CHECK( msg.iTotalFrags == 210 );
	CHECK( msg.iCollectibles == 16 );
	CHECK( msg.flTotalMatchTime == Catch::Approx( 600.0f ).epsilon( 0.2f ) );
	CHECK( msg.iBaseCombatPoints == 150000 );
	CHECK( msg.speciesLog.size() == 3 );
	CHECK( msg.speciesLog[0].speciesName == "Headcrabs" );
	CHECK( msg.speciesLog[0].kills == 45 );
	CHECK( msg.speciesLog[1].speciesName == "Zombies" );
	CHECK( msg.speciesLog[1].kills == 30 );
	CHECK( msg.speciesLog[2].speciesName == "Vortigaunts" );
	CHECK( msg.speciesLog[2].kills == 18 );
}

TEST_CASE( "Gladder UserMsg: GladLambda Serialization Symmetry (SPEC §7.3, Issue #92)", "[gladder][hud][usermsg]" )
{
	std::vector<uint8_t> buf;
	WriteInt32( buf, 1000 ); // Score bonus
	WriteInt16( buf, 5 );    // Collectible count

	REQUIRE( buf.size() == GLADDER_MSG_LAMBDA_SIZE );

	GladderLambdaMsg msg;
	SECTION( "Valid buffer parsing" )
	{
		REQUIRE( Gladder_ParseLambdaMsg( buf.data(), static_cast<int>( buf.size() ), msg ) == true );
		CHECK( msg.iScoreBonus == 1000 );
		CHECK( msg.iCollectibleCount == 5 );
	}

	SECTION( "Oversized buffer parsing" )
	{
		buf.push_back( 0xFF );
		buf.push_back( 0xEE );
		REQUIRE( Gladder_ParseLambdaMsg( buf.data(), static_cast<int>( buf.size() ), msg ) == true );
		CHECK( msg.iScoreBonus == 1000 );
		CHECK( msg.iCollectibleCount == 5 );
	}

	SECTION( "Truncated buffer fails gracefully" )
	{
		CHECK( Gladder_ParseLambdaMsg( buf.data(), 5, msg ) == false );
		CHECK( Gladder_ParseLambdaMsg( buf.data(), 0, msg ) == false );
	}

	SECTION( "Null buffer fails gracefully" )
	{
		CHECK( Gladder_ParseLambdaMsg( nullptr, 6, msg ) == false );
	}
}

