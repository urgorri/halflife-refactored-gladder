#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <string>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "systems/triggers_brush.h"
#include "tests/mock_engine.h"

TEST_CASE( "Triggers: PlayCDTrack format string and out of range validation (#146)", "[triggers][audio]" )
{
	ResetMockEngine();

	SECTION( "Out of range positive track number logs formatted alert" )
	{
		PlayCDTrack( 42 );
		REQUIRE( !g_mockAlertMessages.empty() );
		CHECK( g_mockAlertMessages.back() == "TriggerCDAudio - Track 42 out of range\n" );
		CHECK( g_mockClientCommands.empty() );
	}

	SECTION( "Out of range negative track number logs formatted alert" )
	{
		PlayCDTrack( -5 );
		REQUIRE( !g_mockAlertMessages.empty() );
		CHECK( g_mockAlertMessages.back() == "TriggerCDAudio - Track -5 out of range\n" );
		CHECK( g_mockClientCommands.empty() );
	}

	SECTION( "Track -1 issues cd stop command" )
	{
		PlayCDTrack( -1 );
		CHECK( g_mockAlertMessages.empty() );
		REQUIRE( !g_mockClientCommands.empty() );
		CHECK( g_mockClientCommands.back() == "cd stop\n" );
	}

	SECTION( "Valid track issues formatted cd play command" )
	{
		PlayCDTrack( 7 );
		CHECK( g_mockAlertMessages.empty() );
		REQUIRE( !g_mockClientCommands.empty() );
		CHECK( g_mockClientCommands.back() == "cd play   7\n" );
	}
}
