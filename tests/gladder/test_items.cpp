/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Collectible Item & Arcade Audio Cues (SPEC §7.3, §8, Issue #11)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "dlls/gladder/gladder_audio.h"
#include "dlls/gladder/gladder_items.h"
#include "dlls/gladder/gladder_scoring.h"
#include "dlls/items/item_registry.h"

// Stubs for test environment linkage
CBaseEntity *UTIL_PlayerByIndex( int playerIndex ) { return nullptr; }
void CItem::Spawn( void ) {}
CBaseEntity *CItem::Respawn( void ) { return this; }

TEST_CASE( "GladderAudio: default sound cue paths and reset (SPEC §8)", "[gladder][audio]" )
{
	GladderAudio::ResetCueSoundPaths();

	CHECK( std::string( GladderAudio::GetCueSoundPath( GLADDER_CUE_WAVE_START ) ) == "buttons/bell1.wav" );
	CHECK( std::string( GladderAudio::GetCueSoundPath( GLADDER_CUE_WAVE_COMPLETE ) ) == "debris/beamstart14.wav" );
	CHECK( std::string( GladderAudio::GetCueSoundPath( GLADDER_CUE_MUTATOR_WARNING ) ) == "ambience/siren.wav" );
	CHECK( std::string( GladderAudio::GetCueSoundPath( GLADDER_CUE_COLLECTIBLE ) ) == "items/suitchargeno1.wav" );
	CHECK( std::string( GladderAudio::GetCueSoundPath( GLADDER_CUE_MATCH_END ) ) == "buttons/button10.wav" );
}

TEST_CASE( "GladderAudio: custom sound path overrides", "[gladder][audio]" )
{
	GladderAudio::ResetCueSoundPaths();

	GladderAudio::SetCueSoundPath( GLADDER_CUE_COLLECTIBLE, "gladder/lambda_pickup.wav" );
	CHECK( std::string( GladderAudio::GetCueSoundPath( GLADDER_CUE_COLLECTIBLE ) ) == "gladder/lambda_pickup.wav" );

	// Out of bounds check
	CHECK( std::string( GladderAudio::GetCueSoundPath( (EGladderAudioCue)999 ) ) == "" );

	// Reset restores default
	GladderAudio::ResetCueSoundPaths();
	CHECK( std::string( GladderAudio::GetCueSoundPath( GLADDER_CUE_COLLECTIBLE ) ) == "items/suitchargeno1.wav" );
}

TEST_CASE( "GladderAudio: safe execution when no entity provided", "[gladder][audio]" )
{
	// Must not crash or fault when entity or player edict is null
	REQUIRE_NOTHROW( GladderAudio::PlayCue( GLADDER_CUE_WAVE_START, nullptr ) );
	REQUIRE_NOTHROW( GladderAudio::PlayCue( GLADDER_CUE_COLLECTIBLE, nullptr ) );
	REQUIRE_NOTHROW( GladderAudio::PlayCue( (EGladderAudioCue)-1, nullptr ) );
	REQUIRE_NOTHROW( GladderAudio::PlayCue( (EGladderAudioCue)999, nullptr ) );
}

TEST_CASE( "CGladderItemLambda: ItemRegistry registration and scoring constants (SPEC §7.3)", "[gladder][items]" )
{
	CHECK( GladderScoring::LAMBDA_COLLECTIBLE_POINTS == 2500 );

	CGladderItemLambda::RegisterItems();

	// Verify custom item registry contains both primary classname and alias
	const ItemDescriptor *pLambda = ItemRegistry::FindByClassname( "item_gladder_lambda" );
	CHECK( pLambda != nullptr );

	const ItemDescriptor *pCollectible = ItemRegistry::FindByClassname( "item_gladder_collectible" );
	CHECK( pCollectible != nullptr );
}
