#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "systems/func_break.h"
#include "tests/mock_engine.h"

// Subclass demonstrating downstream override capability (#162)
class CTestBreakableSubclass : public CBreakable
{
  public:
	bool m_bCustomDieInvoked = false;

	void Die( void )
	{
		m_bCustomDieInvoked = true;
	}
};

TEST_CASE( "Breakable: CBreakable::Die virtual dispatch allows subclass override (#162)", "[systems][breakable][virtual]" )
{
	ResetMockEngine();

	CTestBreakableSubclass testBreakable;
	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	edict.v.pContainingEntity = &edict;
	testBreakable.pev = &edict.v;
	testBreakable.m_Material = matGlass;
	testBreakable.pev->health = 0;
	testBreakable.m_iszSpawnObject = 0;
	testBreakable.m_Explosion = expRandom;
	testBreakable.pev->impulse = 0; // ExplosionMagnitude()

	// Call Die() polymorphically through base class pointer
	CBreakable *pBase = &testBreakable;
	pBase->Die();

	// When CBreakable::Die is virtual, the subclass override is invoked.
	// In the red phase (non-virtual), pBase->Die() statically invokes CBreakable::Die()
	// and m_bCustomDieInvoked remains false.
	CHECK( testBreakable.m_bCustomDieInvoked == true );
}

TEST_CASE( "Breakable: CBreakable::Die base execution cleans up entity", "[systems][breakable]" )
{
	ResetMockEngine();

	CBreakable breakable;
	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	edict.v.pContainingEntity = &edict;
	breakable.pev = &edict.v;
	breakable.m_Material = matGlass;
	breakable.pev->health = 0;
	breakable.m_iszSpawnObject = 0;
	breakable.m_Explosion = expRandom;
	breakable.pev->impulse = 0;

	breakable.Die();

	// Canonical base Die() sets think to SUB_Remove with a 0.1s delay
	CHECK( breakable.m_pfnThink == &CBaseEntity::SUB_Remove );
	CHECK( breakable.pev->nextthink > 0.0f );
}
