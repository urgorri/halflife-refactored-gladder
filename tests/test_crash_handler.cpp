#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <cstdio>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "systems/crash_handler.h"
#include "tests/mock_engine.h"

TEST_CASE( "CrashHandler: ring buffer initialization, recording, and retrieval", "[crash_handler][ring_buffer]" )
{
	ResetMockEngine();
	g_CrashHandler.Reset();

	CHECK( g_CrashHandler.GetTotalEventsLogged() == 0 );
	CHECK( g_CrashHandler.GetActiveEventCount() == 0 );

	// Mock entity
	edict_t edict;
	memset( &edict, 0, sizeof( edict ) );
	edict.v.classname = ALLOC_STRING( "monster_alien_slave" );
	edict.v.targetname = ALLOC_STRING( "slave_ambush_1" );
	edict.v.origin = Vector( 100.0f, -250.0f, 45.0f );

	// 1. Log Think event
	g_CrashHandler.LogThink( &edict, 42.5f );

	CHECK( g_CrashHandler.GetTotalEventsLogged() == 1 );
	CHECK( g_CrashHandler.GetActiveEventCount() == 1 );

	TraceEvent ev;
	REQUIRE( g_CrashHandler.GetEvent( 0, &ev ) );
	CHECK( ev.type == TRACE_EVENT_THINK );
	CHECK( std::string( ev.classname ) == "monster_alien_slave" );
	CHECK( std::string( ev.targetname ) == "slave_ambush_1" );
	CHECK( ev.origin[0] == Catch::Approx( 100.0f ) );
	CHECK( ev.origin[1] == Catch::Approx( -250.0f ) );
	CHECK( ev.origin[2] == Catch::Approx( 45.0f ) );
	CHECK( ev.value1 == Catch::Approx( 42.5f ) );

	// Out-of-bounds retrieval check
	TraceEvent invalidEv;
	CHECK_FALSE( g_CrashHandler.GetEvent( 1, &invalidEv ) );
	CHECK_FALSE( g_CrashHandler.GetEvent( -1, &invalidEv ) );
}

TEST_CASE( "CrashHandler: circular ring buffer wrap-around preserves last N events", "[crash_handler][ring_buffer]" )
{
	ResetMockEngine();
	g_CrashHandler.Reset();

	const int BUFFER_CAP = g_CrashHandler.GetRingBufferSize();
	REQUIRE( BUFFER_CAP == 1024 );

	// Push 1100 events into 1024-entry ring buffer
	const int PUSH_COUNT = 1100;
	for ( int i = 0; i < PUSH_COUNT; ++i )
	{
		g_CrashHandler.LogCustom( "Event_%d", i );
	}

	CHECK( g_CrashHandler.GetTotalEventsLogged() == PUSH_COUNT );
	CHECK( g_CrashHandler.GetActiveEventCount() == BUFFER_CAP );

	// Newest event (index 0) must be Event_1099
	TraceEvent newest;
	REQUIRE( g_CrashHandler.GetEvent( 0, &newest ) );
	CHECK( std::string( newest.details ) == "Event_1099" );

	// Oldest active event in window (index BUFFER_CAP - 1) must be Event_76 (1100 - 1024 = 76)
	TraceEvent oldest;
	REQUIRE( g_CrashHandler.GetEvent( BUFFER_CAP - 1, &oldest ) );
	CHECK( std::string( oldest.details ) == "Event_76" );

	// Event at offset 10 from newest must be Event_1089
	TraceEvent tenth;
	REQUIRE( g_CrashHandler.GetEvent( 10, &tenth ) );
	CHECK( std::string( tenth.details ) == "Event_1089" );
}

TEST_CASE( "CrashHandler: dispatch event logging variants", "[crash_handler][events]" )
{
	ResetMockEngine();
	g_CrashHandler.Reset();

	edict_t touchedEdict;
	memset( &touchedEdict, 0, sizeof( touchedEdict ) );
	touchedEdict.v.classname = ALLOC_STRING( "func_breakable" );
	touchedEdict.v.targetname = ALLOC_STRING( "bust_ceiling" );
	touchedEdict.v.origin = Vector( 1200.0f, -400.0f, 250.0f );

	edict_t otherEdict;
	memset( &otherEdict, 0, sizeof( otherEdict ) );
	otherEdict.v.classname = ALLOC_STRING( "player" );

	edict_t activatorEdict;
	memset( &activatorEdict, 0, sizeof( activatorEdict ) );
	activatorEdict.v.classname = ALLOC_STRING( "env_beam" );
	activatorEdict.v.targetname = ALLOC_STRING( "beam_zap" );

	// 1. Touch event
	g_CrashHandler.LogTouch( &touchedEdict, &otherEdict );
	TraceEvent touchEv;
	REQUIRE( g_CrashHandler.GetEvent( 0, &touchEv ) );
	CHECK( touchEv.type == TRACE_EVENT_TOUCH );
	CHECK( std::string( touchEv.classname ) == "func_breakable" );
	CHECK( std::string( touchEv.target_classname ) == "player" );

	// 2. Use event
	g_CrashHandler.LogUse( &touchedEdict, &otherEdict, &activatorEdict, USE_ON, 1.5f );
	TraceEvent useEv;
	REQUIRE( g_CrashHandler.GetEvent( 0, &useEv ) );
	CHECK( useEv.type == TRACE_EVENT_USE );
	CHECK( std::string( useEv.classname ) == "func_breakable" );
	CHECK( std::string( useEv.caller_classname ) == "player" );
	CHECK( std::string( useEv.target_classname ) == "env_beam" );
	CHECK( useEv.use_type == USE_ON );
	CHECK( useEv.value1 == Catch::Approx( 1.5f ) );

	// 3. Blocked event
	g_CrashHandler.LogBlocked( &touchedEdict, &otherEdict );
	TraceEvent blockEv;
	REQUIRE( g_CrashHandler.GetEvent( 0, &blockEv ) );
	CHECK( blockEv.type == TRACE_EVENT_BLOCKED );
	CHECK( std::string( blockEv.classname ) == "func_breakable" );
	CHECK( std::string( blockEv.target_classname ) == "player" );

	// 4. FireTargets event
	g_CrashHandler.LogFireTargets( "hallway_relay", &otherEdict, &activatorEdict, USE_TOGGLE, 0.0f );
	TraceEvent fireEv;
	REQUIRE( g_CrashHandler.GetEvent( 0, &fireEv ) );
	CHECK( fireEv.type == TRACE_EVENT_FIRE_TARGETS );
	CHECK( std::string( fireEv.targetname ) == "hallway_relay" );
	CHECK( std::string( fireEv.caller_classname ) == "player" );
	CHECK( std::string( fireEv.target_classname ) == "env_beam" );
	CHECK( fireEv.use_type == USE_TOGGLE );

	// 5. Schedule Change event
	g_CrashHandler.LogScheduleChange( &otherEdict, "ChaseEnemy", 12 );
	TraceEvent schedEv;
	REQUIRE( g_CrashHandler.GetEvent( 0, &schedEv ) );
	CHECK( schedEv.type == TRACE_EVENT_SCHEDULE_CHANGE );
	CHECK( std::string( schedEv.details ) == "ChaseEnemy" );
	CHECK( schedEv.int_val == 12 );

	// 6. Anim Event
	g_CrashHandler.LogAnimEvent( &otherEdict, 1001, "zap_sound" );
	TraceEvent animEv;
	REQUIRE( g_CrashHandler.GetEvent( 0, &animEv ) );
	CHECK( animEv.type == TRACE_EVENT_ANIM_EVENT );
	CHECK( animEv.int_val == 1001 );
	CHECK( std::string( animEv.details ) == "zap_sound" );
}

TEST_CASE( "CrashHandler: null entity safety across all logging entry points", "[crash_handler][null_safety]" )
{
	ResetMockEngine();
	g_CrashHandler.Reset();

	// None of these should crash or assert
	g_CrashHandler.LogThink( nullptr, 0.0f );
	g_CrashHandler.LogTouch( nullptr, nullptr );
	g_CrashHandler.LogUse( nullptr, nullptr, nullptr, 0, 0.0f );
	g_CrashHandler.LogBlocked( nullptr, nullptr );
	g_CrashHandler.LogFireTargets( nullptr, nullptr, nullptr, 0, 0.0f );
	g_CrashHandler.LogScheduleChange( nullptr, nullptr, 0 );
	g_CrashHandler.LogAnimEvent( nullptr, 0, nullptr );

	CHECK( g_CrashHandler.GetTotalEventsLogged() == 7 );
	CHECK( g_CrashHandler.GetActiveEventCount() == 7 );

	TraceEvent ev;
	REQUIRE( g_CrashHandler.GetEvent( 0, &ev ) );
	CHECK( ev.type == TRACE_EVENT_ANIM_EVENT );
	CHECK( ev.entindex == -1 );
	CHECK( ev.classname[0] == '\0' );
}

TEST_CASE( "CrashHandler: interactive console command and live file logging", "[crash_handler][console_command]" )
{
	ResetMockEngine();
	g_CrashHandler.Reset();

	char outMsg[512];

	// 1. Status command when inactive
	const char *statusArgs[] = { "trace_log", "status" };
	g_CrashHandler.ExecuteCommand( 2, statusArgs, outMsg, sizeof( outMsg ) );
	CHECK( std::string( outMsg ).find( "active=NO" ) != std::string::npos );

	// 2. Start trace file
	const char *testLogFile = "test_execution_trace.log";
	const char *startArgs[] = { "trace_log", "start", testLogFile };
	g_CrashHandler.ExecuteCommand( 3, startArgs, outMsg, sizeof( outMsg ) );
	CHECK( std::string( outMsg ).find( "STARTED" ) != std::string::npos );
	CHECK( g_CrashHandler.IsLiveTracingActive() );

	// 3. Log events while active
	g_CrashHandler.LogCustom( "LiveTraceCheckpoint_Alpha" );
	g_CrashHandler.LogCustom( "LiveTraceCheckpoint_Beta" );

	// 4. Flush command
	const char *flushArgs[] = { "trace_log", "flush" };
	g_CrashHandler.ExecuteCommand( 2, flushArgs, outMsg, sizeof( outMsg ) );
	CHECK( std::string( outMsg ).find( "flushed" ) != std::string::npos );

	// 5. Stop command
	const char *stopArgs[] = { "trace_log", "stop" };
	g_CrashHandler.ExecuteCommand( 2, stopArgs, outMsg, sizeof( outMsg ) );
	CHECK( std::string( outMsg ).find( "STOPPED" ) != std::string::npos );
	CHECK_FALSE( g_CrashHandler.IsLiveTracingActive() );

	// 6. Verify file contents on disk
	FILE *fp = fopen( testLogFile, "r" );
	REQUIRE( fp != nullptr );
	char fileBuffer[2048] = { 0 };
	size_t bytesRead = fread( fileBuffer, 1, sizeof( fileBuffer ) - 1, fp );
	fclose( fp );
	remove( testLogFile );

	CHECK( bytesRead > 0 );
	CHECK( std::string( fileBuffer ).find( "LiveTraceCheckpoint_Alpha" ) != std::string::npos );
	CHECK( std::string( fileBuffer ).find( "LiveTraceCheckpoint_Beta" ) != std::string::npos );

	// 7. Dump snapshot command
	const char *testDumpFile = "test_snapshot_dump.log";
	const char *dumpArgs[] = { "trace_log", "dump", testDumpFile };
	g_CrashHandler.ExecuteCommand( 3, dumpArgs, outMsg, sizeof( outMsg ) );
	CHECK( std::string( outMsg ).find( "dumped successfully" ) != std::string::npos );

	FILE *fpDump = fopen( testDumpFile, "r" );
	REQUIRE( fpDump != nullptr );
	fclose( fpDump );
	remove( testDumpFile );
}
