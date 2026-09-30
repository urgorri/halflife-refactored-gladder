#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <cstddef>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "engine/studio.h"
#include "core/animation.h"
#include "tests/mock_engine.h"

// Test fixture verifying boundary checks for SetBodygroup and GetBodygroup (#157)
struct StudioModelTestBuffer
{
	studiohdr_t hdr;
	mstudiobodyparts_t parts[2];
	mstudiobodyparts_t sentinel; // Slot at index 2 (out of bounds)
};

TEST_CASE( "Animation: SetBodygroup boundary validation (#157)", "[animation][bodygroup]" )
{
	StudioModelTestBuffer buffer;
	std::memset( &buffer, 0, sizeof( buffer ) );

	buffer.hdr.numbodyparts = 2; // Valid indices are 0 and 1
	buffer.hdr.bodypartindex = offsetof( StudioModelTestBuffer, parts );

	// Group 0: 2 models, base 1
	buffer.parts[0].nummodels = 2;
	buffer.parts[0].base = 1;

	// Group 1: 3 models, base 2
	buffer.parts[1].nummodels = 3;
	buffer.parts[1].base = 2;

	// Sentinel at index 2: configured to alter pev.body if improperly accessed
	buffer.sentinel.nummodels = 5;
	buffer.sentinel.base = 1;

	entvars_t pev;
	std::memset( &pev, 0, sizeof( pev ) );

	SECTION( "Null model pointer is handled safely" )
	{
		pev.body = 10;
		SetBodygroup( nullptr, &pev, 0, 1 );
		CHECK( pev.body == 10 );
	}

	SECTION( "Valid bodygroups update pev.body correctly" )
	{
		pev.body = 0;
		SetBodygroup( &buffer.hdr, &pev, 0, 1 );
		CHECK( pev.body == 1 );

		SetBodygroup( &buffer.hdr, &pev, 1, 2 );
		CHECK( pev.body == 5 ); // 1 + 2 * 2
	}

	SECTION( "Exact boundary iGroup == numbodyparts must not alter pev.body or access out of bounds" )
	{
		pev.body = 42;
		// iGroup == 2 is out of bounds for numbodyparts == 2.
		// Before fix (iGroup > numbodyparts), 2 > 2 is false, accessing sentinel and mutating pev.body.
		SetBodygroup( &buffer.hdr, &pev, 2, 1 );
		CHECK( pev.body == 42 );
	}

	SECTION( "iGroup > numbodyparts must not alter pev.body" )
	{
		pev.body = 42;
		SetBodygroup( &buffer.hdr, &pev, 3, 1 );
		CHECK( pev.body == 42 );
	}

	SECTION( "Negative iGroup must not alter pev.body or access preceding memory" )
	{
		pev.body = 42;
		SetBodygroup( &buffer.hdr, &pev, -1, 1 );
		CHECK( pev.body == 42 );
	}
}

TEST_CASE( "Animation: GetBodygroup boundary validation (#157)", "[animation][bodygroup]" )
{
	StudioModelTestBuffer buffer;
	std::memset( &buffer, 0, sizeof( buffer ) );

	buffer.hdr.numbodyparts = 2; // Valid indices are 0 and 1
	buffer.hdr.bodypartindex = offsetof( StudioModelTestBuffer, parts );

	// Group 0: 2 models, base 1
	buffer.parts[0].nummodels = 2;
	buffer.parts[0].base = 1;

	// Group 1: 3 models, base 2
	buffer.parts[1].nummodels = 3;
	buffer.parts[1].base = 2;

	// Sentinel at index 2: configured to return non-zero if accessed
	buffer.sentinel.nummodels = 5;
	buffer.sentinel.base = 1;

	entvars_t pev;
	std::memset( &pev, 0, sizeof( pev ) );
	pev.body = 1; // Corresponds to group 0 value 1, group 1 value 0

	SECTION( "Null model pointer returns 0" )
	{
		CHECK( GetBodygroup( nullptr, &pev, 0 ) == 0 );
	}

	SECTION( "Valid bodygroups return expected values" )
	{
		CHECK( GetBodygroup( &buffer.hdr, &pev, 0 ) == 1 );
		CHECK( GetBodygroup( &buffer.hdr, &pev, 1 ) == 0 );
	}

	SECTION( "Exact boundary iGroup == numbodyparts must return 0" )
	{
		// Before fix (iGroup > numbodyparts), 2 > 2 is false, accessing sentinel and returning (1 / 1) % 5 = 1 != 0
		CHECK( GetBodygroup( &buffer.hdr, &pev, 2 ) == 0 );
	}

	SECTION( "iGroup > numbodyparts returns 0" )
	{
		CHECK( GetBodygroup( &buffer.hdr, &pev, 3 ) == 0 );
	}

	SECTION( "Negative iGroup returns 0" )
	{
		CHECK( GetBodygroup( &buffer.hdr, &pev, -1 ) == 0 );
	}
}
