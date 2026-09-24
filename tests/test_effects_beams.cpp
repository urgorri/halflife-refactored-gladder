#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "customentity.h"
#include "systems/effects.h"
#include "tests/mock_engine.h"

TEST_CASE( "Effects: env_beam ServerSide initial nodraw and toggle state machine", "[effects][beam][lifecycle]" )
{
	ResetMockEngine();

	CLightning beam;
	edict_t edict;
	memset( &edict, 0, sizeof( edict ) );
	beam.pev = &edict.v;

	beam.m_iszSpriteName = MAKE_STRING( "sprites/laserbeam.spr" );
	beam.pev->targetname = MAKE_STRING( "test_beam" );
	beam.m_life = 0.0f; // Server-side beam
	beam.pev->spawnflags = 0; // NOT SF_BEAM_STARTON

	beam.Spawn();

	// Canonical rule 1: Non-starton server beam with targetname must spawn inactive and NODRAW
	CHECK( beam.m_active == 0 );
	CHECK( ( beam.pev->effects & EF_NODRAW ) != 0 );
	CHECK( beam.pev->nextthink == 0.0f );

	// Trigger USE_TOGGLE: Should activate beam
	beam.ToggleUse( nullptr, nullptr, USE_TOGGLE, 0.0f );
	CHECK( beam.m_active == 1 );
	CHECK( ( beam.pev->effects & EF_NODRAW ) == 0 );

	// Trigger USE_TOGGLE again: Should deactivate beam
	beam.ToggleUse( nullptr, nullptr, USE_TOGGLE, 0.0f );
	CHECK( beam.m_active == 0 );
	CHECK( ( beam.pev->effects & EF_NODRAW ) != 0 );
	CHECK( beam.pev->nextthink == 0.0f );

	// Trigger USE_ON when inactive: Should activate
	beam.ToggleUse( nullptr, nullptr, USE_ON, 0.0f );
	CHECK( beam.m_active == 1 );
	CHECK( ( beam.pev->effects & EF_NODRAW ) == 0 );

	// Trigger USE_ON when already active: Should remain active (no-op)
	beam.ToggleUse( nullptr, nullptr, USE_ON, 0.0f );
	CHECK( beam.m_active == 1 );
	CHECK( ( beam.pev->effects & EF_NODRAW ) == 0 );

	// Trigger USE_OFF when active: Should deactivate
	beam.ToggleUse( nullptr, nullptr, USE_OFF, 0.0f );
	CHECK( beam.m_active == 0 );
	CHECK( ( beam.pev->effects & EF_NODRAW ) != 0 );
	CHECK( beam.pev->nextthink == 0.0f );
}

TEST_CASE( "Effects: env_beam ClientSide StrikeUse single-trigger deactivation", "[effects][beam][lifecycle]" )
{
	ResetMockEngine();

	CLightning beam;
	edict_t edict;
	memset( &edict, 0, sizeof( edict ) );
	beam.pev = &edict.v;

	beam.m_iszSpriteName = MAKE_STRING( "sprites/laserbeam.spr" );
	beam.pev->targetname = MAKE_STRING( "test_client_beam" );
	beam.m_life = 1.0f; // Client-side beam
	beam.pev->spawnflags = 0; // Non-toggle, one-shot

	beam.Spawn();

	// Initial state
	CHECK( beam.m_pfnUse != nullptr );

	// Fire one-shot StrikeUse
	beam.StrikeUse( nullptr, nullptr, USE_ON, 0.0f );

	// Canonical rule 4: One-shot non-toggle beam must clear use handler to prevent re-triggering
	CHECK( beam.m_pfnUse == nullptr );
}

TEST_CASE( "Effects: CBeam GetStartPos safely falls back to pev->origin when start entity resolves to nullptr", "[effects][beam][null_safety]" )
{
	ResetMockEngine();

	CBeam beam;
	edict_t edict;
	memset( &edict, 0, sizeof( edict ) );
	beam.pev = &edict.v;
	beam.pev->origin = Vector( 100.0f, 200.0f, 300.0f );

	// Set type to BEAM_ENTS with non-existent start entity index
	beam.SetType( BEAM_ENTS );
	beam.SetStartEntity( 42 );

	// stub_PEntityOfEntIndex returns nullptr in mock engine
	// Before fix, this would crash (Access Violation on nullptr->v.origin)
	const Vector &startPos = beam.GetStartPos();
	CHECK( startPos == beam.pev->origin );
}

TEST_CASE( "Effects: env_beam BeamUpdateVars gracefully handles non-existent start or end entities", "[effects][beam][null_safety]" )
{
	ResetMockEngine();

	CLightning beam;
	edict_t edict;
	memset( &edict, 0, sizeof( edict ) );
	beam.pev = &edict.v;
	beam.pev->origin = Vector( 50.0f, 60.0f, 70.0f );

	beam.m_iszSpriteName = MAKE_STRING( "sprites/laserbeam.spr" );
	beam.m_iszStartEntity = MAKE_STRING( "non_existent_start" );
	beam.m_iszEndEntity = MAKE_STRING( "non_existent_end" );
	beam.m_life = 0.0f;

	// In mock engine, find entity by string returns nullptr.
	// BeamUpdateVars must abort safely without dereferencing null pointers or crashing in RelinkBeam.
	beam.BeamUpdateVars();

	// Verify beam did not crash and retained safe origin
	CHECK( beam.GetStartPos() == beam.pev->origin );
}

