#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "systems/effects.h"
#include "tests/mock_engine.h"

// Concrete test entity to verify virtual UpdateOnRemove dispatch
class CTestRemovableParent : public CBaseEntity
{
  public:
	bool m_bUpdateOnRemoveCalled = false;
	CSprite *m_pChildSprite      = nullptr;
	CBeam *m_pChildBeam          = nullptr;

	void UpdateOnRemove( void ) override
	{
		m_bUpdateOnRemoveCalled = true;
		if ( m_pChildSprite )
		{
			UTIL_Remove( m_pChildSprite );
			m_pChildSprite = nullptr;
		}
		if ( m_pChildBeam )
		{
			UTIL_Remove( m_pChildBeam );
			m_pChildBeam = nullptr;
		}
		CBaseEntity::UpdateOnRemove();
	}
};

TEST_CASE( "Effects: CSprite AnimateThink self-terminates when MOVETYPE_FOLLOW aiment is invalid or freed (#176)", "[effects][sprite][lifecycle]" )
{
	ResetMockEngine();

	edict_t spriteEdict;
	memset( &spriteEdict, 0, sizeof( spriteEdict ) );

	CSprite sprite;
	sprite.pev = &spriteEdict.v;
	sprite.pev->classname = MAKE_STRING( "env_sprite" );
	sprite.pev->framerate = 10.0f;
	gpGlobals->time       = 1.0f;

	SECTION( "Sprite with valid aiment animates normally and does not self-terminate" )
	{
		edict_t parentEdict;
		memset( &parentEdict, 0, sizeof( parentEdict ) );
		parentEdict.free    = 0;
		parentEdict.v.flags = 0;

		sprite.pev->movetype = MOVETYPE_FOLLOW;
		sprite.pev->aiment   = &parentEdict;

		gpGlobals->time = 1.1f;
		sprite.AnimateThink();

		CHECK( ( sprite.pev->flags & FL_KILLME ) == 0 );
		CHECK( sprite.pev->nextthink == Catch::Approx( 1.2f ) );
	}

	SECTION( "Sprite with MOVETYPE_FOLLOW and nullptr aiment is removed" )
	{
		sprite.pev->movetype = MOVETYPE_FOLLOW;
		sprite.pev->aiment   = nullptr;

		sprite.AnimateThink();

		CHECK( ( sprite.pev->flags & FL_KILLME ) != 0 );
	}

	SECTION( "Sprite with MOVETYPE_FOLLOW and parent marked FL_KILLME is removed" )
	{
		edict_t parentEdict;
		memset( &parentEdict, 0, sizeof( parentEdict ) );
		parentEdict.free    = 0;
		parentEdict.v.flags = FL_KILLME;

		sprite.pev->movetype = MOVETYPE_FOLLOW;
		sprite.pev->aiment   = &parentEdict;

		sprite.AnimateThink();

		CHECK( ( sprite.pev->flags & FL_KILLME ) != 0 );
	}

	SECTION( "Sprite with MOVETYPE_FOLLOW and parent marked free is removed" )
	{
		edict_t parentEdict;
		memset( &parentEdict, 0, sizeof( parentEdict ) );
		parentEdict.free    = 1;
		parentEdict.v.flags = 0;

		sprite.pev->movetype = MOVETYPE_FOLLOW;
		sprite.pev->aiment   = &parentEdict;

		sprite.AnimateThink();

		CHECK( ( sprite.pev->flags & FL_KILLME ) != 0 );
	}

	SECTION( "Sprite with MOVETYPE_NOCLIP (non-follow) is not removed even if aiment is null" )
	{
		sprite.pev->movetype = MOVETYPE_NOCLIP;
		sprite.pev->aiment   = nullptr;

		gpGlobals->time = 1.1f;
		sprite.AnimateThink();

		CHECK( ( sprite.pev->flags & FL_KILLME ) == 0 );
		CHECK( sprite.pev->nextthink == Catch::Approx( 1.2f ) );
	}

	SECTION( "CSprite::SetAttachment initializes AnimateThink and MOVETYPE_FOLLOW" )
	{
		edict_t parentEdict;
		memset( &parentEdict, 0, sizeof( parentEdict ) );
		parentEdict.free    = 0;
		parentEdict.v.flags = 0;

		sprite.m_pfnThink    = nullptr;
		sprite.pev->movetype = MOVETYPE_NONE;

		gpGlobals->time = 5.0f;
		sprite.SetAttachment( &parentEdict, 2 );

		CHECK( sprite.pev->movetype == MOVETYPE_FOLLOW );
		CHECK( sprite.pev->aiment == &parentEdict );
		CHECK( sprite.pev->body == 2 );
		CHECK( sprite.m_pfnThink != nullptr );
		CHECK( sprite.pev->nextthink == Catch::Approx( 5.1f ) );
	}
}

TEST_CASE( "Effects: CBeam self-terminates when endpoint entity is removed or freed (#176)", "[effects][beam][lifecycle]" )
{
	ResetMockEngine();

	edict_t beamEdict;
	memset( &beamEdict, 0, sizeof( beamEdict ) );

	CBeam beam;
	beam.pev = &beamEdict.v;
	beam.m_pfnThink     = nullptr;
	beam.pev->classname = MAKE_STRING( "beam" );
	gpGlobals->time     = 2.0f;

	SECTION( "BEAM_ENTPOINT: Alive end entity preserves beam" )
	{
		edict_t *pEnd = GetMockClientEntity( 1 );
		pEnd->free    = 0;
		pEnd->v.flags = 0;

		beam.PointEntInit( Vector( 0, 0, 0 ), 1 );

		CHECK( beam.m_pfnThink != nullptr );
		CHECK( beam.pev->nextthink == Catch::Approx( 2.1f ) );

		beam.Think();
		CHECK( ( beam.pev->flags & FL_KILLME ) == 0 );
	}

	SECTION( "BEAM_ENTPOINT: Freed end entity removes beam on Think" )
	{
		edict_t *pEnd = GetMockClientEntity( 1 );
		pEnd->free    = 1;
		pEnd->v.flags = 0;

		beam.PointEntInit( Vector( 0, 0, 0 ), 1 );

		beam.Think();
		CHECK( ( beam.pev->flags & FL_KILLME ) != 0 );
	}

	SECTION( "BEAM_ENTPOINT: FL_KILLME end entity removes beam on Think" )
	{
		edict_t *pEnd = GetMockClientEntity( 1 );
		pEnd->free    = 0;
		pEnd->v.flags = FL_KILLME;

		beam.PointEntInit( Vector( 0, 0, 0 ), 1 );

		beam.Think();
		CHECK( ( beam.pev->flags & FL_KILLME ) != 0 );
	}

	SECTION( "BEAM_ENTPOINT: Non-existent end entity removes beam on Think" )
	{
		// Entity 50 exceeds client entities range (1-32), returning nullptr
		beam.PointEntInit( Vector( 0, 0, 0 ), 50 );

		beam.Think();
		CHECK( ( beam.pev->flags & FL_KILLME ) != 0 );
	}

	SECTION( "BEAM_ENTS: Both endpoints alive preserves beam" )
	{
		edict_t *pStart = GetMockClientEntity( 1 );
		pStart->free    = 0;
		pStart->v.flags = 0;

		edict_t *pEnd = GetMockClientEntity( 2 );
		pEnd->free    = 0;
		pEnd->v.flags = 0;

		beam.EntsInit( 1, 2 );

		beam.Think();
		CHECK( ( beam.pev->flags & FL_KILLME ) == 0 );
	}

	SECTION( "BEAM_ENTS: Start entity marked FL_KILLME removes beam" )
	{
		edict_t *pStart = GetMockClientEntity( 1 );
		pStart->free    = 0;
		pStart->v.flags = FL_KILLME;

		edict_t *pEnd = GetMockClientEntity( 2 );
		pEnd->free    = 0;
		pEnd->v.flags = 0;

		beam.EntsInit( 1, 2 );

		beam.Think();
		CHECK( ( beam.pev->flags & FL_KILLME ) != 0 );
	}

	SECTION( "BEAM_ENTS: End entity marked FL_KILLME removes beam" )
	{
		edict_t *pStart = GetMockClientEntity( 1 );
		pStart->free    = 0;
		pStart->v.flags = 0;

		edict_t *pEnd = GetMockClientEntity( 2 );
		pEnd->free    = 0;
		pEnd->v.flags = FL_KILLME;

		beam.EntsInit( 1, 2 );

		beam.Think();
		CHECK( ( beam.pev->flags & FL_KILLME ) != 0 );
	}

	SECTION( "BeamThink direct call reschedules nextthink for alive entities" )
	{
		edict_t *pEnd = GetMockClientEntity( 1 );
		pEnd->free    = 0;
		pEnd->v.flags = 0;

		beam.PointEntInit( Vector( 0, 0, 0 ), 1 );
		gpGlobals->time = 10.0f;
		beam.BeamThink();

		CHECK( ( beam.pev->flags & FL_KILLME ) == 0 );
		CHECK( beam.pev->nextthink == Catch::Approx( 10.1f ) );
	}
}

TEST_CASE( "Core: Virtual UpdateOnRemove cleans up child effects upon entity removal (#176)", "[core][lifecycle][cleanup]" )
{
	ResetMockEngine();

	edict_t parentEdict;
	memset( &parentEdict, 0, sizeof( parentEdict ) );

	CTestRemovableParent parent;
	parent.pev = &parentEdict.v;

	edict_t childSpriteEdict;
	memset( &childSpriteEdict, 0, sizeof( childSpriteEdict ) );
	CSprite childSprite;
	childSprite.pev = &childSpriteEdict.v;

	edict_t childBeamEdict;
	memset( &childBeamEdict, 0, sizeof( childBeamEdict ) );
	CBeam childBeam;
	childBeam.pev = &childBeamEdict.v;

	parent.m_pChildSprite = &childSprite;
	parent.m_pChildBeam   = &childBeam;

	UTIL_Remove( &parent );

	CHECK( parent.m_bUpdateOnRemoveCalled == true );
	CHECK( ( parent.pev->flags & FL_KILLME ) != 0 );
	CHECK( ( childSprite.pev->flags & FL_KILLME ) != 0 );
	CHECK( ( childBeam.pev->flags & FL_KILLME ) != 0 );
	CHECK( parent.m_pChildSprite == nullptr );
	CHECK( parent.m_pChildBeam == nullptr );
}
