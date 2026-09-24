/***
 *
 *	Behavioral Equivalence Verification - Player Customization Unit Tests
 *	Verifies player decal frame buffering across engine lifecycle events (Build 10210+ and legacy),
 *	frame count boundary validation, entity validation, and disconnect cleanup.
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "core/client_customization.h"
#include "tests/mock_engine.h"

// Helper struct for dummy player backing store without instantiating incomplete vtables
struct MockPlayerHolder
{
	alignas( CBasePlayer ) uint8_t storage[sizeof( CBasePlayer )];

	MockPlayerHolder()
	{
		std::memset( storage, 0, sizeof( storage ) );
		player()->m_nCustomSprayFrames = -1;
	}

	CBasePlayer *player()
	{
		return reinterpret_cast<CBasePlayer *>( storage );
	}
};

TEST_CASE( "PlayerCustomization: SetCustomDecalFrames frame boundary validation", "[player][customization]" )
{
	MockPlayerHolder holder;
	CBasePlayer *pPlayer = holder.player();

	// Initially no custom frames
	CHECK( pPlayer->GetCustomDecalFrames() == -1 );

	// ValidateCustomDecalFrames standalone function
	CHECK( ValidateCustomDecalFrames( 1 ) == 1 );
	CHECK( ValidateCustomDecalFrames( 7 ) == 7 );
	CHECK( ValidateCustomDecalFrames( 0 ) == -1 );
	CHECK( ValidateCustomDecalFrames( -1 ) == -1 );
	CHECK( ValidateCustomDecalFrames( 8 ) == -1 );

	// Valid frame ranges: 1 to 7 inclusive
	for ( int frames = 1; frames < 8; frames++ )
	{
		pPlayer->SetCustomDecalFrames( frames );
		CHECK( pPlayer->GetCustomDecalFrames() == frames );
	}

	// Zero or negative frames clamp to -1 (none)
	pPlayer->SetCustomDecalFrames( 0 );
	CHECK( pPlayer->GetCustomDecalFrames() == -1 );

	pPlayer->SetCustomDecalFrames( -1 );
	CHECK( pPlayer->GetCustomDecalFrames() == -1 );

	pPlayer->SetCustomDecalFrames( -5 );
	CHECK( pPlayer->GetCustomDecalFrames() == -1 );

	// Out-of-bounds >= 8 clamp to -1
	pPlayer->SetCustomDecalFrames( 8 );
	CHECK( pPlayer->GetCustomDecalFrames() == -1 );

	pPlayer->SetCustomDecalFrames( 16 );
	CHECK( pPlayer->GetCustomDecalFrames() == -1 );
}

TEST_CASE( "PlayerCustomization: Build 10210+ early customization buffering sequence", "[player][customization][lifecycle]" )
{
	InitMockEngine();
	ResetMockEngine();
	gpGlobals->maxClients = 32;

	CPlayerCustomizationBuffer::ClearAll();

	int slot = 1;
	edict_t *pEdict = GetMockClientEntity( slot );
	REQUIRE( pEdict != nullptr );
	pEdict->pvPrivateData = nullptr; // Player entity not instantiated yet (pre-ClientPutInServer)

	// Step 1: Client connects
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 0 );

	// Step 2: Build 10210 triggers PlayerCustomization during resource negotiation before ClientPutInServer
	customization_t decalCust;
	std::memset( &decalCust, 0, sizeof( decalCust ) );
	decalCust.resource.type = t_decal;
	decalCust.nUserData2    = 5; // 5 animated spray decal frames

	g_mockAlertMessages.clear();
	CPlayerCustomizationBuffer::HandlePlayerCustomization( pEdict, &decalCust );

	// Must NOT output "Couldn't get player!" alert for valid client slots
	bool hasPlayerAlert = false;
	for ( const auto &msg : g_mockAlertMessages )
	{
		if ( msg.find( "Couldn't get player!" ) != std::string::npos )
		{
			hasPlayerAlert = true;
			break;
		}
	}
	CHECK_FALSE( hasPlayerAlert );

	// Pending decal frames must be buffered
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 5 );

	// Step 3: ClientPutInServer spawns the player and applies buffered customization
	MockPlayerHolder holder;
	CBasePlayer *pPlayer = holder.player();
	pEdict->pvPrivateData = pPlayer;

	CPlayerCustomizationBuffer::ApplyPendingDecalFrames( slot, pPlayer );

	CHECK( pPlayer->GetCustomDecalFrames() == 5 );
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 0 );
}

TEST_CASE( "PlayerCustomization: Legacy engine post-spawn customization sequence", "[player][customization][lifecycle]" )
{
	InitMockEngine();
	ResetMockEngine();
	gpGlobals->maxClients = 32;

	int slot = 2;
	edict_t *pEdict = GetMockClientEntity( slot );
	REQUIRE( pEdict != nullptr );

	CPlayerCustomizationBuffer::ClearPendingDecalFrames( slot );

	// In legacy engines, ClientPutInServer runs before PlayerCustomization
	MockPlayerHolder holder;
	CBasePlayer *pPlayer = holder.player();
	pEdict->pvPrivateData = pPlayer;

	CPlayerCustomizationBuffer::ApplyPendingDecalFrames( slot, pPlayer );
	CHECK( pPlayer->GetCustomDecalFrames() == -1 );

	// Engine triggers PlayerCustomization with instantiated player
	customization_t decalCust;
	std::memset( &decalCust, 0, sizeof( decalCust ) );
	decalCust.resource.type = t_decal;
	decalCust.nUserData2    = 4;

	CPlayerCustomizationBuffer::HandlePlayerCustomization( pEdict, &decalCust );

	CHECK( pPlayer->GetCustomDecalFrames() == 4 );
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 0 );
}

TEST_CASE( "PlayerCustomization: Disconnect and connect reset pending buffer", "[player][customization][lifecycle]" )
{
	InitMockEngine();
	ResetMockEngine();
	gpGlobals->maxClients = 32;

	int slot = 3;
	edict_t *pEdict = GetMockClientEntity( slot );
	REQUIRE( pEdict != nullptr );
	pEdict->pvPrivateData = nullptr;

	customization_t decalCust;
	std::memset( &decalCust, 0, sizeof( decalCust ) );
	decalCust.resource.type = t_decal;
	decalCust.nUserData2    = 6;

	CPlayerCustomizationBuffer::HandlePlayerCustomization( pEdict, &decalCust );
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 6 );

	// Client disconnects before spawning -> buffer should be cleared
	CPlayerCustomizationBuffer::ClearPendingDecalFrames( slot );
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 0 );

	// Re-buffer and test slot reset
	CPlayerCustomizationBuffer::HandlePlayerCustomization( pEdict, &decalCust );
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 6 );

	CPlayerCustomizationBuffer::ClearPendingDecalFrames( slot );
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 0 );
}

TEST_CASE( "PlayerCustomization: NULL customization and non-player entity error handling", "[player][customization]" )
{
	InitMockEngine();
	ResetMockEngine();
	gpGlobals->maxClients = 32;

	// NULL entity pointer
	g_mockAlertMessages.clear();
	customization_t cust;
	std::memset( &cust, 0, sizeof( cust ) );
	cust.resource.type = t_decal;
	cust.nUserData2    = 3;

	CPlayerCustomizationBuffer::HandlePlayerCustomization( nullptr, &cust );
	REQUIRE_FALSE( g_mockAlertMessages.empty() );
	CHECK( g_mockAlertMessages.back().find( "Couldn't get player!" ) != std::string::npos );

	// NULL customization pointer on unspawned client
	int slot = 1;
	edict_t *pEdict = GetMockClientEntity( slot );
	REQUIRE( pEdict != nullptr );
	pEdict->pvPrivateData = nullptr;

	g_mockAlertMessages.clear();
	CPlayerCustomizationBuffer::HandlePlayerCustomization( pEdict, nullptr );
	REQUIRE_FALSE( g_mockAlertMessages.empty() );
	CHECK( g_mockAlertMessages.back().find( "NULL customization!" ) != std::string::npos );

	// NULL customization pointer on spawned client
	MockPlayerHolder holder;
	CBasePlayer *pPlayer = holder.player();
	pEdict->pvPrivateData = pPlayer;

	g_mockAlertMessages.clear();
	CPlayerCustomizationBuffer::HandlePlayerCustomization( pEdict, nullptr );
	REQUIRE_FALSE( g_mockAlertMessages.empty() );
	CHECK( g_mockAlertMessages.back().find( "NULL customization!" ) != std::string::npos );
}

TEST_CASE( "PlayerCustomization: Non-decal and unknown resource types handling", "[player][customization]" )
{
	InitMockEngine();
	ResetMockEngine();
	gpGlobals->maxClients = 32;

	int slot = 4;
	edict_t *pEdict = GetMockClientEntity( slot );
	REQUIRE( pEdict != nullptr );
	pEdict->pvPrivateData = nullptr;

	CPlayerCustomizationBuffer::ClearPendingDecalFrames( slot );

	// Sound, skin, model types should be ignored without buffer mutation or error alert
	customization_t nonDecalCust;
	std::memset( &nonDecalCust, 0, sizeof( nonDecalCust ) );
	nonDecalCust.resource.type = t_sound;
	g_mockAlertMessages.clear();
	CPlayerCustomizationBuffer::HandlePlayerCustomization( pEdict, &nonDecalCust );
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 0 );
	CHECK( g_mockAlertMessages.empty() );

	nonDecalCust.resource.type = t_skin;
	CPlayerCustomizationBuffer::HandlePlayerCustomization( pEdict, &nonDecalCust );
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 0 );
	CHECK( g_mockAlertMessages.empty() );

	nonDecalCust.resource.type = t_model;
	CPlayerCustomizationBuffer::HandlePlayerCustomization( pEdict, &nonDecalCust );
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 0 );
	CHECK( g_mockAlertMessages.empty() );

	// Unknown resource type triggers error alert
	customization_t unknownCust;
	std::memset( &unknownCust, 0, sizeof( unknownCust ) );
	unknownCust.resource.type = (resourcetype_t)99;
	g_mockAlertMessages.clear();
	CPlayerCustomizationBuffer::HandlePlayerCustomization( pEdict, &unknownCust );
	CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( slot ) == 0 );
	REQUIRE_FALSE( g_mockAlertMessages.empty() );
	CHECK( g_mockAlertMessages.back().find( "Unknown customization type!" ) != std::string::npos );
}

TEST_CASE( "PlayerCustomization: ServerActivate and ServerDeactivate reset all pending buffers", "[player][customization][lifecycle]" )
{
	InitMockEngine();
	ResetMockEngine();
	gpGlobals->maxClients = 32;

	customization_t decalCust;
	std::memset( &decalCust, 0, sizeof( decalCust ) );
	decalCust.resource.type = t_decal;

	for ( int i = 1; i <= 5; i++ )
	{
		edict_t *pEnt = GetMockClientEntity( i );
		pEnt->pvPrivateData = nullptr;
		decalCust.nUserData2 = i;
		CPlayerCustomizationBuffer::HandlePlayerCustomization( pEnt, &decalCust );
		CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( i ) == i );
	}

	// ClearAll (simulating ServerActivate / ServerDeactivate) wipes all pending buffers
	CPlayerCustomizationBuffer::ClearAll();
	for ( int i = 1; i <= 5; i++ )
	{
		CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( i ) == 0 );
	}

	// Re-populate and test Reset()
	for ( int i = 1; i <= 5; i++ )
	{
		edict_t *pEnt = GetMockClientEntity( i );
		decalCust.nUserData2 = i;
		CPlayerCustomizationBuffer::HandlePlayerCustomization( pEnt, &decalCust );
		CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( i ) == i );
	}

	CPlayerCustomizationBuffer::Reset();
	for ( int i = 1; i <= 5; i++ )
	{
		CHECK( CPlayerCustomizationBuffer::GetPendingDecalFrames( i ) == 0 );
	}
}
