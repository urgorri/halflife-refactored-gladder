/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: HUD Elements & Dynamic Combo Gauge (SPEC §9.1, GitHub Issue #15)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "cl_dll/gladder/hud_gladder_combo.h"
#include "cl_dll/gladder/hud_gladder_overlay.h"
#include "cl_dll/gladder/hud_gladder_lambda.h"
#include "cl_dll/gladder/gladder_grid_debug_client.h"


TEST_CASE( "CHudGladderCombo: Multiplier and Decay Meter Tracking (#15)", "[gladder][hud][combo]" )
{
	CHudGladderCombo combo;
	CHECK( combo.IsActive() == false );
	CHECK( combo.GetComboMultiplier() == 1 );
	CHECK( combo.GetDecayFraction() == 0.0f );

	// Multiplier x2 with full window (1.0)
	combo.SetComboState( 2, 1.0f, 10.0f );
	CHECK( combo.IsActive() == true );
	CHECK( combo.GetComboMultiplier() == 2 );
	CHECK( combo.GetDecayFraction() == 1.0f );

	// Decaying window
	combo.SetComboState( 2, 0.5f, 11.5f );
	CHECK( combo.IsActive() == true );
	CHECK( combo.GetDecayFraction() == 0.5f );

	// Multiplier x3 fresh kill
	combo.SetComboState( 3, 1.0f, 12.0f );
	CHECK( combo.IsActive() == true );
	CHECK( combo.GetComboMultiplier() == 3 );

	// Window fully decayed to 0
	combo.SetComboState( 1, 0.0f, 15.5f );
	CHECK( combo.IsActive() == false );
	CHECK( combo.GetComboMultiplier() == 1 );
}

TEST_CASE( "CHudGladderOverlay: Telemetry and Collectibles Field Tracking (#15)", "[gladder][hud][overlay]" )
{
	CHudGladderOverlay overlay;
	CHECK( overlay.GetCollectibles() == 0 );

	overlay.SetTelemetry( 25.0f, 50.0f, 35.0f, 5, 80, 8 );
	CHECK( overlay.GetCollectibles() == 8 );
	CHECK( overlay.GetFrags() == 80 );
	CHECK( overlay.GetCompletedLaps() == 5 );
	CHECK( overlay.GetAverageLap() == 35.0f );

	overlay.Reset();
	CHECK( overlay.GetCollectibles() == 0 );
	CHECK( overlay.GetFrags() == 0 );
}

TEST_CASE( "CHudGladderOverlay: Real-time HUD enemy counter and dynamic visibility lifecycle (SPEC §2, §9.1, Issue #118)", "[gladder][hud][hostiles]" )
{
	CHudGladderOverlay overlay;
	CHECK( overlay.IsHostilesCounterVisible() == false );
	CHECK( overlay.GetHostilesKilled() == 0 );
	CHECK( overlay.GetHostilesTotal() == 0 );

	// 1. Initialized on wave start via MsgFunc_GladderWave (state = 1, killed = 0, total = 20)
	std::vector<uint8_t> startBuf;
	startBuf.push_back( 1 ); // Active wave
	// Wave 1
	startBuf.push_back( 1 ); startBuf.push_back( 0 );
	// Time left 600.0f
	int16_t timeLeft = static_cast<int16_t>( 600.0f * 8.0f );
	startBuf.push_back( timeLeft & 0xFF ); startBuf.push_back( ( timeLeft >> 8 ) & 0xFF );
	// Lap time 0.0f
	startBuf.push_back( 0 ); startBuf.push_back( 0 );
	// Killed 0, Total 20
	startBuf.push_back( 0 ); startBuf.push_back( 0 );
	startBuf.push_back( 20 ); startBuf.push_back( 0 );

	overlay.MsgFunc_GladderWave( "GladWave", static_cast<int>( startBuf.size() ), startBuf.data() );
	CHECK( overlay.IsHostilesCounterVisible() == true );
	CHECK( overlay.GetHostilesKilled() == 0 );
	CHECK( overlay.GetHostilesTotal() == 20 );

	// 2. Incremental update during combat (e.g. 14 / 20)
	startBuf[7] = 14; startBuf[8] = 0;
	overlay.MsgFunc_GladderWave( "GladWave", static_cast<int>( startBuf.size() ), startBuf.data() );
	CHECK( overlay.IsHostilesCounterVisible() == true );
	CHECK( overlay.GetHostilesKilled() == 14 );
	CHECK( overlay.GetHostilesTotal() == 20 );

	// 3. Fully cleared wave (20 / 20)
	startBuf[7] = 20; startBuf[8] = 0;
	overlay.MsgFunc_GladderWave( "GladWave", static_cast<int>( startBuf.size() ), startBuf.data() );
	CHECK( overlay.IsHostilesCounterVisible() == true );
	CHECK( overlay.GetHostilesKilled() == 20 );
	CHECK( overlay.GetHostilesTotal() == 20 );

	// 4. Wave finishes: state transitions to GLADDER_STATE_WAVE_COMPLETED (2) or STANDBY (0) -> counter hides
	startBuf[0] = 2; // Completed
	overlay.MsgFunc_GladderWave( "GladWave", static_cast<int>( startBuf.size() ), startBuf.data() );
	CHECK( overlay.IsHostilesCounterVisible() == false );

	// 5. New wave commences: state transitions back to active (1) with fresh total (24) -> counter reappears fresh
	startBuf[0] = 1; // Active
	startBuf[7] = 0; startBuf[8] = 0; // 0 killed
	startBuf[9] = 24; startBuf[10] = 0; // 24 total
	overlay.MsgFunc_GladderWave( "GladWave", static_cast<int>( startBuf.size() ), startBuf.data() );
	CHECK( overlay.IsHostilesCounterVisible() == true );
	CHECK( overlay.GetHostilesKilled() == 0 );
	CHECK( overlay.GetHostilesTotal() == 24 );
}

TEST_CASE( "CHudGladderLambda: Center-screen notification lifecycle & replacement (SPEC §7.3, Issue #92)", "[gladder][hud][lambda]" )
{
	CHudGladderLambda lambda;
	CHECK( lambda.GetScoreBonus() == 0 );
	CHECK( lambda.GetCollectibleCount() == 0 );
	CHECK( lambda.IsDisplaying( 0.0f ) == false );

	SECTION( "Trigger sets score, collectible count, formatted text, and active display window" )
	{
		lambda.TriggerNotification( 1000, 3, 10.0f );
		CHECK( lambda.GetScoreBonus() == 1000 );
		CHECK( lambda.GetCollectibleCount() == 3 );
		CHECK( std::string( lambda.GetText() ).find( "LAMBDA COLLECTED +1000" ) != std::string::npos );
		CHECK( std::string( lambda.GetText() ).find( "TOTAL: 3" ) != std::string::npos );

		// Inside display window
		CHECK( lambda.IsDisplaying( 9.9f ) == false );
		CHECK( lambda.IsDisplaying( 10.0f ) == true );
		CHECK( lambda.IsDisplaying( 11.2f ) == true ); // hold time boundary
		CHECK( lambda.IsDisplaying( 11.6f ) == true ); // fade time
		CHECK( lambda.IsDisplaying( 11.75f ) == false ); // expired
	}

	SECTION( "Subsequent notification replaces active one without stacking" )
	{
		lambda.TriggerNotification( 1000, 1, 10.0f );
		CHECK( lambda.GetCollectibleCount() == 1 );
		CHECK( lambda.IsDisplaying( 11.0f ) == true );

		// New pickup occurs at 11.0s
		lambda.TriggerNotification( 1000, 2, 11.0f );
		CHECK( lambda.GetCollectibleCount() == 2 );
		CHECK( lambda.GetStartTime() == 11.0f );
		// New window expires at 11.0 + 1.7 = 12.7s
		CHECK( lambda.IsDisplaying( 12.0f ) == true );
		CHECK( lambda.IsDisplaying( 12.6f ) == true );
		CHECK( lambda.IsDisplaying( 12.8f ) == false );
	}

	SECTION( "Reset clears all notification state" )
	{
		lambda.TriggerNotification( 1000, 5, 20.0f );
		REQUIRE( lambda.IsDisplaying( 20.5f ) == true );

		lambda.Reset();
		CHECK( lambda.GetScoreBonus() == 0 );
		CHECK( lambda.GetCollectibleCount() == 0 );
		CHECK( lambda.GetText()[0] == '\0' );
		CHECK( lambda.IsDisplaying( 20.5f ) == false );
	}
}

TEST_CASE( "Spatial Grid Debug - Client State & Zero-Cost Lifecycle", "[gladder][hud][grid_debug]" )
{
	GladderGridDebug_ClientReset();
	GladderGridDebug_SetClientActive( false );

	REQUIRE_FALSE( GladderGridDebug_IsClientActive() );
	REQUIRE( GladderGridDebug_GetClientCellCount() == 0 );

	SECTION( "Deallocates memory upon deactivation" )
	{
		GladderGridDebug_SetClientActive( true );
		REQUIRE( GladderGridDebug_IsClientActive() );

		// Simulate receiving a chunk
		uint8_t packet[2 + 9] = { 0 };
		packet[0] = GLADDER_GRID_DBG_OP_CHUNK;
		packet[1] = 1; // 1 cell
		// coord x = 100.0 (raw = 800)
		int16_t rx = 800;
		packet[2] = rx & 0xFF;
		packet[3] = ( rx >> 8 ) & 0xFF;
		// coord y = 0
		packet[4] = 0; packet[5] = 0;
		// coord z = 32.0 (raw = 256)
		int16_t rz = 256;
		packet[6] = rz & 0xFF;
		packet[7] = ( rz >> 8 ) & 0xFF;
		packet[8] = 1; // areaId
		packet[9] = GLADDER_CELL_FLAG_CLEARANCE_OK; // flags
		packet[10] = 0; // occupied

		REQUIRE( GladderGridDebug_ParseUserMessage( packet, sizeof( packet ) ) );
		REQUIRE( GladderGridDebug_GetClientCellCount() == 1 );

		// Turning off frees buffer
		GladderGridDebug_SetClientActive( false );
		REQUIRE_FALSE( GladderGridDebug_IsClientActive() );
		REQUIRE( GladderGridDebug_GetClientCellCount() == 0 );
		REQUIRE( GladderGridDebug_GetClientCells().capacity() == 0 );
	}

	SECTION( "Opcode Clear discards cache" )
	{
		uint8_t clearPacket[1] = { GLADDER_GRID_DBG_OP_CLEAR };
		REQUIRE( GladderGridDebug_ParseUserMessage( clearPacket, sizeof( clearPacket ) ) );
		REQUIRE( GladderGridDebug_GetClientCellCount() == 0 );
	}

	SECTION( "Malformed packets return false safely" )
	{
		uint8_t truncated[1] = { GLADDER_GRID_DBG_OP_CHUNK };
		REQUIRE_FALSE( GladderGridDebug_ParseUserMessage( truncated, sizeof( truncated ) ) );

		uint8_t truncatedChunk[4] = { GLADDER_GRID_DBG_OP_CHUNK, 2, 0, 0 }; // Claims 2 cells but missing payload
		REQUIRE_FALSE( GladderGridDebug_ParseUserMessage( truncatedChunk, sizeof( truncatedChunk ) ) );

		REQUIRE_FALSE( GladderGridDebug_ParseUserMessage( nullptr, 0 ) );
	}

	SECTION( "Double-buffered batch protocol preserves active cells until batch end" )
	{
		GladderGridDebug_SetClientActive( true );

		uint8_t beginPacket[1] = { GLADDER_GRID_DBG_OP_BATCH_BEGIN };
		REQUIRE( GladderGridDebug_ParseUserMessage( beginPacket, sizeof( beginPacket ) ) );

		// Stream 1 cell with normal (11 bytes payload)
		uint8_t chunkWithNormal[2 + 11] = { 0 };
		chunkWithNormal[0] = GLADDER_GRID_DBG_OP_CHUNK;
		chunkWithNormal[1] = 1; // 1 cell
		// coord x = 64.0 (raw = 512)
		int16_t rx = 512;
		chunkWithNormal[2] = rx & 0xFF;
		chunkWithNormal[3] = ( rx >> 8 ) & 0xFF;
		// coord y = 0
		chunkWithNormal[4] = 0; chunkWithNormal[5] = 0;
		// coord z = 16.0 (raw = 128)
		int16_t rz = 128;
		chunkWithNormal[6] = rz & 0xFF;
		chunkWithNormal[7] = ( rz >> 8 ) & 0xFF;
		// normal nx = 0, ny = 0 (127 -> 1.0)
		chunkWithNormal[8] = 0;  // nx
		chunkWithNormal[9] = 0;  // ny
		chunkWithNormal[10] = 3; // areaId
		chunkWithNormal[11] = GLADDER_CELL_FLAG_LARGE_CLEARANCE; // flags
		chunkWithNormal[12] = 1; // occupied

		REQUIRE( GladderGridDebug_ParseUserMessage( chunkWithNormal, sizeof( chunkWithNormal ) ) );

		// Double-buffering invariant: active display buffer must NOT change until batch completes
		REQUIRE( GladderGridDebug_GetClientCellCount() == 0 );

		// Batch commit
		uint8_t endPacket[1] = { GLADDER_GRID_DBG_OP_BATCH_END };
		REQUIRE( GladderGridDebug_ParseUserMessage( endPacket, sizeof( endPacket ) ) );

		REQUIRE( GladderGridDebug_GetClientCellCount() == 1 );
		const auto &cells = GladderGridDebug_GetClientCells();
		REQUIRE( cells[0].x == Catch::Approx( 64.0f ) );
		REQUIRE( cells[0].z == Catch::Approx( 16.0f ) );
		REQUIRE( cells[0].areaId == 3 );
		REQUIRE( cells[0].isOccupied == true );
		REQUIRE( cells[0].nz == Catch::Approx( 1.0f ) );
	}

	GladderGridDebug_ClientReset();
	GladderGridDebug_SetClientActive( false );
}


TEST_CASE( "CHudGladderOverlay: Enforces gl_clear 1 by default on Init and VidInit (SPEC §7.2.1, Issue #140)", "[gladder][hud][client]" )
{
	CHudGladderOverlay overlay;
	overlay.Init();
	overlay.VidInit();
	CHECK( ( overlay.m_iFlags & HUD_ACTIVE ) != 0 );
}
