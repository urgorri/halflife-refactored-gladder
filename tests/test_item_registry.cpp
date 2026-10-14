/***
 *
 *	Behavioral Equivalence Verification - Item Registry Unit Tests (Layer 3)
 *	Verifies ItemRegistry and Extensible Player Item Interface (#89)
 *
 ****/

#include <new>
#include <stddef.h>
#include <cstring>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

#include "external/catch2/catch_amalgamated.hpp"
#include "tests/mock_engine.h"
#include "dlls/items/item_registry.h"

// Static tracking for mock precache order
static std::vector<int> s_precacheCallOrder;
static void MockItemPrecache100() { s_precacheCallOrder.push_back( 100 ); }
static void MockItemPrecache200() { s_precacheCallOrder.push_back( 200 ); }
static void MockItemPrecache300() { s_precacheCallOrder.push_back( 300 ); }

TEST_CASE( "ItemRegistry: registration, ordering, and lookup (#89)", "[items][registry]" )
{
	ItemRegistry::Clear();
	REQUIRE( ItemRegistry::GetDescriptors().empty() );

	ItemDescriptor d1 = { "item_battery", 300, MockItemPrecache300 };
	ItemDescriptor d2 = { "item_suit", 100, MockItemPrecache100 };
	ItemDescriptor d3 = { "item_antidote", 200, MockItemPrecache200 };

	// Register out of priority order
	ItemRegistry::Register( d1 );
	ItemRegistry::Register( d2 );
	ItemRegistry::Register( d3 );

	REQUIRE( ItemRegistry::GetDescriptors().size() == 3 );

	SECTION( "Duplicate registration is ignored" )
	{
		ItemRegistry::Register( d1 );
		REQUIRE( ItemRegistry::GetDescriptors().size() == 3 );
	}

	SECTION( "FindByClassname finds correct descriptors" )
	{
		const ItemDescriptor *pBattery = ItemRegistry::FindByClassname( "item_battery" );
		REQUIRE( pBattery != nullptr );
		CHECK( pBattery->iPriorityOrder == 300 );

		const ItemDescriptor *pSuit = ItemRegistry::FindByClassname( "item_suit" );
		REQUIRE( pSuit != nullptr );
		CHECK( pSuit->iPriorityOrder == 100 );

		CHECK( ItemRegistry::FindByClassname( "item_nonexistent" ) == nullptr );
		CHECK( ItemRegistry::FindByClassname( nullptr ) == nullptr );
	}

	SECTION( "PrecacheAll executes in ascending priority order" )
	{
		s_precacheCallOrder.clear();
		ItemRegistry::PrecacheAll();

		REQUIRE( s_precacheCallOrder.size() == 3 );
		CHECK( s_precacheCallOrder[0] == 100 );
		CHECK( s_precacheCallOrder[1] == 200 );
		CHECK( s_precacheCallOrder[2] == 300 );

		// Descriptors in storage should now be sorted
		const auto &descs = ItemRegistry::GetDescriptors();
		CHECK( descs[0].iPriorityOrder == 100 );
		CHECK( descs[1].iPriorityOrder == 200 );
		CHECK( descs[2].iPriorityOrder == 300 );
	}
}

TEST_CASE( "CBasePlayer: legacy item accessors preserve MAX_ITEMS bounds (#89)", "[player][items][legacy]" )
{
	// Allocate raw buffer aligned as CBasePlayer to verify member accessors without full engine entity creation
	alignas( CBasePlayer ) unsigned char playerBuffer[sizeof( CBasePlayer )];
	memset( playerBuffer, 0, sizeof( playerBuffer ) );
	CBasePlayer *pPlayer = reinterpret_cast<CBasePlayer *>( playerBuffer );

	SECTION( "Bounds checking and value preservation" )
	{
		// Default zeroed state
		for ( int i = 0; i < MAX_ITEMS; ++i )
		{
			CHECK( pPlayer->GetItem( i ) == 0 );
			CHECK( pPlayer->GetLegacyItem( i ) == 0 );
		}

		// Valid indices [0, MAX_ITEMS-1]
		pPlayer->SetItem( 0, 42 );
		CHECK( pPlayer->GetItem( 0 ) == 42 );
		CHECK( pPlayer->GetLegacyItem( 0 ) == 42 );
		CHECK( pPlayer->m_rgItems[0] == 42 );

		pPlayer->SetLegacyItem( MAX_ITEMS - 1, 99 );
		CHECK( pPlayer->GetItem( MAX_ITEMS - 1 ) == 99 );
		CHECK( pPlayer->GetLegacyItem( MAX_ITEMS - 1 ) == 99 );
		CHECK( pPlayer->m_rgItems[MAX_ITEMS - 1] == 99 );

		// Out-of-bounds reads return 0 safely
		CHECK( pPlayer->GetItem( -1 ) == 0 );
		CHECK( pPlayer->GetItem( MAX_ITEMS ) == 0 );
		CHECK( pPlayer->GetLegacyItem( 100 ) == 0 );

		// Out-of-bounds writes are safely ignored
		pPlayer->SetItem( -1, 123 );
		pPlayer->SetItem( MAX_ITEMS, 456 );
		pPlayer->SetLegacyItem( 999, 789 );
	}

	SECTION( "Layout invariant: m_rgItems size and offset" )
	{
		static_assert( sizeof( pPlayer->m_rgItems ) == MAX_ITEMS * sizeof( int ), "m_rgItems size mismatch" );
#if defined( _MSC_VER )
		static_assert( offsetof( CBasePlayer, m_rgItems ) == 748, "CBasePlayer::m_rgItems offset shifted on MSVC" );
#elif defined( __GNUC__ )
		static_assert( offsetof( CBasePlayer, m_rgItems ) == 768, "CBasePlayer::m_rgItems offset shifted on GCC" );
#endif
	}
}

TEST_CASE( "ItemRegistry: extensible auxiliary player custom item interface (#89)", "[player][items][custom]" )
{
	// Use simulated player pointers to verify auxiliary storage without instantiating full engine hierarchy
	const CBasePlayer *pPlayer1 = reinterpret_cast<const CBasePlayer *>( 0x1000 );
	const CBasePlayer *pPlayer2 = reinterpret_cast<const CBasePlayer *>( 0x2000 );

	ItemRegistry::ClearAllPlayerCustomItems();

	SECTION( "Default state returns 0 count and false for any item" )
	{
		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer1, "item_nightvision" ) == 0 );
		CHECK( ItemRegistry::HasPlayerCustomItem( pPlayer1, "item_nightvision" ) == false );
		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer1, nullptr ) == 0 );
		CHECK( ItemRegistry::HasPlayerCustomItem( pPlayer1, nullptr ) == false );
		CHECK( ItemRegistry::GetPlayerCustomItemCount( nullptr, "item_nightvision" ) == 0 );
	}

	SECTION( "AddPlayerCustomItem increments count and enables HasPlayerCustomItem" )
	{
		ItemRegistry::AddPlayerCustomItem( pPlayer1, "item_kevlar", 1 );
		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer1, "item_kevlar" ) == 1 );
		CHECK( ItemRegistry::HasPlayerCustomItem( pPlayer1, "item_kevlar" ) == true );

		ItemRegistry::AddPlayerCustomItem( pPlayer1, "item_kevlar", 2 );
		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer1, "item_kevlar" ) == 3 );

		// Independent players have separate inventories
		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer2, "item_kevlar" ) == 0 );
		CHECK( ItemRegistry::HasPlayerCustomItem( pPlayer2, "item_kevlar" ) == false );
	}

	SECTION( "SetPlayerCustomItemCount explicitly sets or removes item" )
	{
		ItemRegistry::SetPlayerCustomItemCount( pPlayer1, "item_keycard", 5 );
		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer1, "item_keycard" ) == 5 );
		CHECK( ItemRegistry::HasPlayerCustomItem( pPlayer1, "item_keycard" ) == true );

		// Setting count to 0 or negative removes item
		ItemRegistry::SetPlayerCustomItemCount( pPlayer1, "item_keycard", 0 );
		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer1, "item_keycard" ) == 0 );
		CHECK( ItemRegistry::HasPlayerCustomItem( pPlayer1, "item_keycard" ) == false );
	}

	SECTION( "ClearPlayerCustomItems clears only target player" )
	{
		ItemRegistry::AddPlayerCustomItem( pPlayer1, "item_boots", 1 );
		ItemRegistry::AddPlayerCustomItem( pPlayer2, "item_boots", 1 );

		ItemRegistry::ClearPlayerCustomItems( pPlayer1 );
		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer1, "item_boots" ) == 0 );
		CHECK( ItemRegistry::HasPlayerCustomItem( pPlayer1, "item_boots" ) == false );

		// Player 2 remains intact
		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer2, "item_boots" ) == 1 );
		CHECK( ItemRegistry::HasPlayerCustomItem( pPlayer2, "item_boots" ) == true );
	}

	SECTION( "ClearAllPlayerCustomItems clears all players" )
	{
		ItemRegistry::AddPlayerCustomItem( pPlayer1, "item_scanner", 2 );
		ItemRegistry::AddPlayerCustomItem( pPlayer2, "item_scanner", 4 );

		ItemRegistry::ClearAllPlayerCustomItems();

		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer1, "item_scanner" ) == 0 );
		CHECK( ItemRegistry::GetPlayerCustomItemCount( pPlayer2, "item_scanner" ) == 0 );
	}
}

TEST_CASE( "ItemRegistry: impulse 101 extra item registration (#199)", "[items][registry][impulse101]" )
{
	ItemRegistry::Clear();

	SECTION( "Nothing registered leaves the extra list empty (vanilla behavior)" )
	{
		CHECK( ItemRegistry::GetImpulse101Items().empty() );
	}

	SECTION( "Registered classnames are kept in registration order" )
	{
		ItemRegistry::RegisterImpulse101Item( "weapon_knife" );
		ItemRegistry::RegisterImpulse101Item( "weapon_eagle" );
		ItemRegistry::RegisterImpulse101Item( "ammo_556" );

		const auto &items = ItemRegistry::GetImpulse101Items();
		REQUIRE( items.size() == 3 );
		CHECK( std::strcmp( items[0], "weapon_knife" ) == 0 );
		CHECK( std::strcmp( items[1], "weapon_eagle" ) == 0 );
		CHECK( std::strcmp( items[2], "ammo_556" ) == 0 );
	}

	SECTION( "Duplicates (even from distinct pointers) are ignored" )
	{
		char copy[] = "weapon_knife";
		ItemRegistry::RegisterImpulse101Item( "weapon_knife" );
		ItemRegistry::RegisterImpulse101Item( copy );
		ItemRegistry::RegisterImpulse101Item( "weapon_knife" );
		CHECK( ItemRegistry::GetImpulse101Items().size() == 1 );
	}

	SECTION( "Null and empty classnames are ignored" )
	{
		ItemRegistry::RegisterImpulse101Item( nullptr );
		ItemRegistry::RegisterImpulse101Item( "" );
		CHECK( ItemRegistry::GetImpulse101Items().empty() );
	}

	SECTION( "REGISTER_IMPULSE101_ITEM macro registers its classname" )
	{
		REGISTER_IMPULSE101_ITEM( weapon_pipewrench )
		REQUIRE( ItemRegistry::GetImpulse101Items().size() == 1 );
		CHECK( std::strcmp( ItemRegistry::GetImpulse101Items()[0], "weapon_pipewrench" ) == 0 );
	}

	SECTION( "Clear resets the extra list" )
	{
		ItemRegistry::RegisterImpulse101Item( "weapon_sniper" );
		REQUIRE( ItemRegistry::GetImpulse101Items().size() == 1 );
		ItemRegistry::Clear();
		CHECK( ItemRegistry::GetImpulse101Items().empty() );
	}

	ItemRegistry::Clear();
}
