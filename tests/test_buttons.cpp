#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <cmath>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "systems/doors.h"
#include "systems/buttons.h"
#include "tests/mock_engine.h"

TEST_CASE( "Buttons: CMomentaryRotButton zero move distance division guard (#158)", "[systems][buttons]" )
{
	ResetMockEngine();

	CMomentaryRotButton rotButton;
	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	rotButton.pev = &edict.v;

	rotButton.pev->angles = Vector( 0, 0, 0 );
	rotButton.m_start = Vector( 0, 0, 0 );
	rotButton.m_end = Vector( 0, 0, 0 );
	rotButton.m_flMoveDistance = 0.0f; // Zero move distance (distance omitted or 0 in map)
	rotButton.pev->ideal_yaw = 0.0f;

	SECTION( "Use() with zero move distance does not divide by zero or set ideal_yaw to inf/nan" )
	{
		rotButton.Use( nullptr, nullptr, USE_TOGGLE, 0.0f );

		CHECK( !std::isnan( rotButton.pev->ideal_yaw ) );
		CHECK( !std::isinf( rotButton.pev->ideal_yaw ) );
		CHECK( rotButton.pev->ideal_yaw == 0.0f );
	}

	SECTION( "Return() with zero move distance does not divide by zero" )
	{
		rotButton.Return();

		CHECK( !std::isnan( rotButton.pev->ideal_yaw ) );
		CHECK( !std::isinf( rotButton.pev->ideal_yaw ) );
	}
}
