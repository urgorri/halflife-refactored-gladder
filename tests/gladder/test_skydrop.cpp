/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Sky-Drop Deployment & Tactical Overview (SPEC §2, §12.1, Issue #141)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "gladder/gladder_cl_usermsg.h"
#include "gladder/hud_gladder_skydrop.h"
#include <vector>
#include <cmath>

static void WriteInt16( std::vector<uint8_t> &buf, int16_t val )
{
	buf.push_back( static_cast<uint8_t>( val & 0xFF ) );
	buf.push_back( static_cast<uint8_t>( ( val >> 8 ) & 0xFF ) );
}

static void WriteCoord( std::vector<uint8_t> &buf, float val )
{
	int16_t raw = static_cast<int16_t>( std::round( val * 8.0f ) );
	WriteInt16( buf, raw );
}

TEST_CASE( "Sky-Drop Deploy Message Parsing", "[gladder][skydrop][usermsg]" )
{
	SECTION( "Valid message parses accurately" )
	{
		std::vector<uint8_t> buf;
		WriteCoord( buf, 256.0f );  // dropX
		WriteCoord( buf, -512.0f ); // dropY
		WriteCoord( buf, 64.0f );   // dropZ
		WriteInt16( buf, 4 );       // iWaveNumber
		WriteInt16( buf, 18 );      // iHostileQuota
		buf.push_back( 2 );         // iMutatorType (Low Gravity)
		WriteCoord( buf, 2.8f );    // flBriefingDuration
		buf.push_back( 2 );         // gateCount: 2 gates

		// Gate 1: (1024, 2048, 0)
		WriteCoord( buf, 1024.0f );
		WriteCoord( buf, 2048.0f );
		WriteCoord( buf, 0.0f );

		// Gate 2: (-1024, -2048, 128)
		WriteCoord( buf, -1024.0f );
		WriteCoord( buf, -2048.0f );
		WriteCoord( buf, 128.0f );

		GladderDeployMsg msg;
		bool ok = Gladder_ParseDeployMsg( buf.data(), static_cast<int>( buf.size() ), msg );
		REQUIRE( ok );

		CHECK( msg.dropX == Catch::Approx( 256.0f ).margin( 0.2f ) );
		CHECK( msg.dropY == Catch::Approx( -512.0f ).margin( 0.2f ) );
		CHECK( msg.dropZ == Catch::Approx( 64.0f ).margin( 0.2f ) );
		CHECK( msg.iWaveNumber == 4 );
		CHECK( msg.iHostileQuota == 18 );
		CHECK( msg.iMutatorType == 2 );
		CHECK( msg.flBriefingDuration == Catch::Approx( 2.8f ).margin( 0.2f ) );
		REQUIRE( msg.extractionGates.size() == 2 );
		CHECK( msg.extractionGates[0].x == Catch::Approx( 1024.0f ).margin( 0.2f ) );
		CHECK( msg.extractionGates[0].y == Catch::Approx( 2048.0f ).margin( 0.2f ) );
		CHECK( msg.extractionGates[1].x == Catch::Approx( -1024.0f ).margin( 0.2f ) );
		CHECK( msg.extractionGates[1].y == Catch::Approx( -2048.0f ).margin( 0.2f ) );
	}

	SECTION( "Truncated buffer fails gracefully" )
	{
		std::vector<uint8_t> shortBuf( 8, 0 );
		GladderDeployMsg msg;
		bool ok = Gladder_ParseDeployMsg( shortBuf.data(), static_cast<int>( shortBuf.size() ), msg );
		CHECK_FALSE( ok );
	}

	SECTION( "Null buffer fails gracefully" )
	{
		GladderDeployMsg msg;
		CHECK_FALSE( Gladder_ParseDeployMsg( nullptr, 20, msg ) );
	}
}

TEST_CASE( "Overview Projector Coordinate Mapping", "[gladder][skydrop][overview]" )
{
	GladderOverviewProjector projector;

	SECTION( "Parses standard overview buffer" )
	{
		const char *sampleOverview =
		    "// Sample overview script\n"
		    "global {\n"
		    "  zoom 2.0\n"
		    "  origin 1000 -500 0\n"
		    "  rotated 0\n"
		    "}\n"
		    "layer {\n"
		    "  image \"overviews/sample.bmp\"\n"
		    "  height 0\n"
		    "}\n";

		bool ok = projector.ParseFromBuffer( sampleOverview );
		REQUIRE( ok );
		CHECK( projector.GetZoom() == Catch::Approx( 2.0f ) );
		CHECK( projector.GetOriginX() == Catch::Approx( 1000.0f ) );
		CHECK( projector.GetOriginY() == Catch::Approx( -500.0f ) );
		CHECK( projector.GetRotated() == 0 );
	}

	SECTION( "Maps center world origin to box center" )
	{
		projector.SetData( 1000.0f, -500.0f, 1.0f, 0 );
		int boxCenterX = 400;
		int boxCenterY = 300;
		int boxSide    = 200;

		int screenX = 0, screenY = 0;
		projector.WorldToScreen( 1000.0f, -500.0f, boxCenterX, boxCenterY, boxSide, screenX, screenY );

		CHECK( screenX == boxCenterX );
		CHECK( screenY == boxCenterY );
	}

	SECTION( "Positive world offsets move right and up" )
	{
		projector.SetData( 0.0f, 0.0f, 1.0f, 0 );
		int boxCenterX = 400;
		int boxCenterY = 300;
		int boxSide    = 400;

		int screenX = 0, screenY = 0;
		// Full map is 4096 units. Moving +1024 X moves +1/4 of map right
		projector.WorldToScreen( 1024.0f, 1024.0f, boxCenterX, boxCenterY, boxSide, screenX, screenY );

		CHECK( screenX > boxCenterX );
		CHECK( screenY < boxCenterY ); // Screen Y increases downwards
	}

	SECTION( "Rotated overview applies 90-degree transformation" )
	{
		projector.SetData( 0.0f, 0.0f, 1.0f, 1 );
		int boxCenterX = 400;
		int boxCenterY = 300;
		int boxSide    = 400;

		int screenX = 0, screenY = 0;
		projector.WorldToScreen( 1024.0f, 0.0f, boxCenterX, boxCenterY, boxSide, screenX, screenY );

		CHECK( screenX == boxCenterX );
		CHECK( screenY < boxCenterY );
	}
}

TEST_CASE( "CHudGladderSkyDrop Lifecycle", "[gladder][skydrop][hud]" )
{
	CHudGladderSkyDrop hud;
	CHECK_FALSE( hud.IsActive() );

	GladderDeployMsg msg;
	msg.dropX              = 100.0f;
	msg.dropY              = 200.0f;
	msg.flBriefingDuration = 2.8f;
	msg.iHostileQuota      = 12;

	hud.TriggerDeployment( msg, 10.0f );
	CHECK( hud.IsActive() );
	CHECK( hud.GetStartTime() == 10.0f );
	CHECK( hud.GetDeployMsg().iHostileQuota == 12 );

	hud.Reset();
	CHECK_FALSE( hud.IsActive() );
}
