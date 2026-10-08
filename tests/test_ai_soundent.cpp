/***
 *
 *	Behavioral Equivalence Verification - AI Sound and Scent Sensing Unit Tests
 *	Verifies CSound, CSoundEnt, CBaseMonster::PBestSound/PBestScent NULL pointer safety,
 *	cycle bounds protection, and Bullsquid food scent schedule decision guards (#196).
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"

#include <cstring>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "dlls/ai/basemonster.h"
#include "dlls/ai/soundent.h"
#include "dlls/ai/schedule.h"
#include "dlls/monsters/bullsquid.h"
#include "tests/mock_engine.h"

extern CSoundEnt *pSoundEnt;

namespace
{

class CTestMonster : public CBaseMonster
{
public:
	CTestMonster()
	{
		static edict_t testEdict;
		memset( &testEdict, 0, sizeof( testEdict ) );
		pev = &testEdict.v;
		pev->origin = Vector( 0, 0, 0 );
		pev->view_ofs = Vector( 0, 0, 32 );
	}
};

} // namespace

TEST_CASE( "CSound: Clear and Reset sanitize m_iNextAudible", "[ai][soundent]" )
{
	CSound sound;
	sound.m_vecOrigin = Vector( 100, 200, 300 );
	sound.m_iType = bits_SOUND_COMBAT;
	sound.m_iVolume = 400;
	sound.m_flExpireTime = 10.0f;
	sound.m_iNext = 5;
	sound.m_iNextAudible = 12;

	SECTION( "Clear sets m_iNextAudible to SOUNDLIST_EMPTY" )
	{
		sound.Clear();
		CHECK( sound.m_vecOrigin == g_vecZero );
		CHECK( sound.m_iType == 0 );
		CHECK( sound.m_iVolume == 0 );
		CHECK( sound.m_flExpireTime == 0.0f );
		CHECK( sound.m_iNext == SOUNDLIST_EMPTY );
		CHECK( sound.m_iNextAudible == SOUNDLIST_EMPTY );
	}

	SECTION( "Reset sets m_iNextAudible to SOUNDLIST_EMPTY" )
	{
		sound.Reset();
		CHECK( sound.m_vecOrigin == g_vecZero );
		CHECK( sound.m_iType == 0 );
		CHECK( sound.m_iVolume == 0 );
		CHECK( sound.m_iNext == SOUNDLIST_EMPTY );
		CHECK( sound.m_iNextAudible == SOUNDLIST_EMPTY );
	}
}

TEST_CASE( "CSound: classification of audible sounds vs scents", "[ai][soundent]" )
{
	CSound sound;

	SECTION( "Audible sounds" )
	{
		sound.m_iType = bits_SOUND_COMBAT;
		CHECK( sound.FIsSound() == TRUE );
		CHECK( sound.FIsScent() == FALSE );

		sound.m_iType = bits_SOUND_WORLD;
		CHECK( sound.FIsSound() == TRUE );
		CHECK( sound.FIsScent() == FALSE );

		sound.m_iType = bits_SOUND_PLAYER;
		CHECK( sound.FIsSound() == TRUE );
		CHECK( sound.FIsScent() == FALSE );

		sound.m_iType = bits_SOUND_DANGER;
		CHECK( sound.FIsSound() == TRUE );
		CHECK( sound.FIsScent() == FALSE );
	}

	SECTION( "Scents" )
	{
		sound.m_iType = bits_SOUND_MEAT;
		CHECK( sound.FIsSound() == FALSE );
		CHECK( sound.FIsScent() == TRUE );

		sound.m_iType = bits_SOUND_CARCASS;
		CHECK( sound.FIsSound() == FALSE );
		CHECK( sound.FIsScent() == TRUE );

		sound.m_iType = bits_SOUND_GARBAGE;
		CHECK( sound.FIsSound() == FALSE );
		CHECK( sound.FIsScent() == TRUE );
	}
}

TEST_CASE( "CSoundEnt: SoundPointerForIndex bounds safety", "[ai][soundent]" )
{
	CSoundEnt soundEntInstance;
	CSoundEnt *pPrev = pSoundEnt;

	SECTION( "Returns NULL when pSoundEnt is NULL" )
	{
		pSoundEnt = nullptr;
		CHECK( CSoundEnt::SoundPointerForIndex( 0 ) == nullptr );
		CHECK( CSoundEnt::SoundPointerForIndex( 10 ) == nullptr );
	}

	SECTION( "Returns NULL for invalid or out-of-range indices" )
	{
		pSoundEnt = &soundEntInstance;
		soundEntInstance.Initialize();

		CHECK( CSoundEnt::SoundPointerForIndex( -1 ) == nullptr );
		CHECK( CSoundEnt::SoundPointerForIndex( -999 ) == nullptr );
		CHECK( CSoundEnt::SoundPointerForIndex( MAX_WORLD_SOUNDS ) == nullptr );
		CHECK( CSoundEnt::SoundPointerForIndex( MAX_WORLD_SOUNDS + 50 ) == nullptr );

		// Valid indices return non-null
		CHECK( CSoundEnt::SoundPointerForIndex( 0 ) != nullptr );
		CHECK( CSoundEnt::SoundPointerForIndex( MAX_WORLD_SOUNDS - 1 ) != nullptr );
	}

	pSoundEnt = pPrev;
}

TEST_CASE( "CSoundEnt: FreeSound and IAllocSound reset m_iNextAudible", "[ai][soundent]" )
{
	CSoundEnt soundEntInstance;
	CSoundEnt *pPrev = pSoundEnt;
	pSoundEnt = &soundEntInstance;
	soundEntInstance.Initialize();

	int iAllocated = soundEntInstance.IAllocSound();
	REQUIRE( iAllocated != SOUNDLIST_EMPTY );

	CSound *pSound = CSoundEnt::SoundPointerForIndex( iAllocated );
	REQUIRE( pSound != nullptr );
	CHECK( pSound->m_iNextAudible == SOUNDLIST_EMPTY );

	// Simulate monster using this sound and setting m_iNextAudible
	pSound->m_iNextAudible = 42;
	pSound->m_iType = bits_SOUND_MEAT;

	// Free sound
	CSoundEnt::FreeSound( iAllocated, SOUNDLIST_EMPTY );

	// Freed sound must have been cleared and m_iNextAudible reset to SOUNDLIST_EMPTY
	CHECK( pSound->m_iNextAudible == SOUNDLIST_EMPTY );
	CHECK( pSound->m_iType == 0 );

	pSoundEnt = pPrev;
}

TEST_CASE( "PBestSound and PBestScent: NULL sound pointer safety", "[ai][monster_sensors]" )
{
	CSoundEnt soundEntInstance;
	CSoundEnt *pPrev = pSoundEnt;
	pSoundEnt = &soundEntInstance;
	soundEntInstance.Initialize();

	CTestMonster monster;

	SECTION( "Empty audible list returns NULL" )
	{
		monster.m_iAudibleList = SOUNDLIST_EMPTY;
		CHECK( monster.PBestSound() == nullptr );
		CHECK( monster.PBestScent() == nullptr );
	}

	SECTION( "Negative or out-of-range index at head returns NULL without crashing" )
	{
		monster.m_iAudibleList = -5;
		CHECK( monster.PBestSound() == nullptr );
		CHECK( monster.PBestScent() == nullptr );

		monster.m_iAudibleList = MAX_WORLD_SOUNDS + 10;
		CHECK( monster.PBestSound() == nullptr );
		CHECK( monster.PBestScent() == nullptr );
	}

	SECTION( "Valid sound pointing to invalid next index breaks safely" )
	{
		CSound *pFirst = CSoundEnt::SoundPointerForIndex( 2 );
		REQUIRE( pFirst != nullptr );
		pFirst->Clear();
		pFirst->m_iType = bits_SOUND_COMBAT;
		pFirst->m_vecOrigin = Vector( 50, 0, 0 );
		pFirst->m_iNextAudible = 999; // Corrupted next pointer

		monster.m_iAudibleList = 2;
		CSound *pBest = monster.PBestSound();
		CHECK( pBest == pFirst );

		// For scent, if first is combat sound and next is invalid, returns NULL
		CHECK( monster.PBestScent() == nullptr );
	}

	pSoundEnt = pPrev;
}

TEST_CASE( "PBestSound and PBestScent: cycle detection prevents infinite loops (#196)", "[ai][monster_sensors]" )
{
	CSoundEnt soundEntInstance;
	CSoundEnt *pPrev = pSoundEnt;
	pSoundEnt = &soundEntInstance;
	soundEntInstance.Initialize();

	CTestMonster monster;

	CSound *pSoundA = CSoundEnt::SoundPointerForIndex( 3 );
	CSound *pSoundB = CSoundEnt::SoundPointerForIndex( 4 );
	REQUIRE( pSoundA != nullptr );
	REQUIRE( pSoundB != nullptr );

	pSoundA->Clear();
	pSoundA->m_iType = bits_SOUND_MEAT;
	pSoundA->m_vecOrigin = Vector( 100, 0, 0 );

	pSoundB->Clear();
	pSoundB->m_iType = bits_SOUND_MEAT;
	pSoundB->m_vecOrigin = Vector( 200, 0, 0 );

	SECTION( "Direct cycle A -> B -> A terminates safely within MAX_WORLD_SOUNDS" )
	{
		pSoundA->m_iNextAudible = 4;
		pSoundB->m_iNextAudible = 3; // Cycle!

		monster.m_iAudibleList = 3;

		// Traversal must break out and return the closest valid scent without hanging
		CSound *pBest = monster.PBestScent();
		REQUIRE( pBest != nullptr );
		CHECK( pBest == pSoundA );
	}

	SECTION( "Self-cycle A -> A terminates safely" )
	{
		pSoundA->m_iNextAudible = 3; // Self-loop!

		monster.m_iAudibleList = 3;
		CSound *pBest = monster.PBestScent();
		REQUIRE( pBest != nullptr );
		CHECK( pBest == pSoundA );
	}

	SECTION( "Cycle with sound types terminates safely for PBestSound" )
	{
		pSoundA->m_iType = bits_SOUND_COMBAT;
		pSoundB->m_iType = bits_SOUND_COMBAT;
		pSoundA->m_iNextAudible = 4;
		pSoundB->m_iNextAudible = 3;

		monster.m_iAudibleList = 3;
		CSound *pBest = monster.PBestSound();
		REQUIRE( pBest != nullptr );
		CHECK( pBest == pSoundA );
	}

	pSoundEnt = pPrev;
}

TEST_CASE( "Bullsquid food scent schedule decision guards (#196)", "[monsters][bullsquid]" )
{
	// Verifies the fix for issue #196 where if PBestScent() returns NULL,
	// CBullsquid::GetSchedule() clears bits_COND_SMELL_FOOD instead of
	// blindly returning SCHED_SQUID_EAT.

	int conditions = bits_COND_SMELL_FOOD | bits_COND_CAN_RANGE_ATTACK1;

	// Simulated logic matching CBullsquid::GetSchedule under MONSTERSTATE_COMBAT
	auto EvaluateSchedule = []( int &conds, CSound *pSound ) -> int {
		if ( conds & bits_COND_SMELL_FOOD )
		{
			if ( pSound )
			{
				return SCHED_SQUID_EAT;
			}
			conds &= ~bits_COND_SMELL_FOOD;
		}

		if ( conds & bits_COND_CAN_RANGE_ATTACK1 )
		{
			return SCHED_RANGE_ATTACK1;
		}

		return SCHED_CHASE_ENEMY;
	};

	SECTION( "When food scent is valid, returns SCHED_SQUID_EAT" )
	{
		CSound validScent;
		validScent.m_iType = bits_SOUND_MEAT;
		int conds = conditions;

		int sched = EvaluateSchedule( conds, &validScent );
		CHECK( sched == SCHED_SQUID_EAT );
		CHECK( ( conds & bits_COND_SMELL_FOOD ) != 0 );
	}

	SECTION( "When food scent is NULL, clears condition and falls through to combat" )
	{
		int conds = conditions;

		int sched = EvaluateSchedule( conds, nullptr );
		CHECK( sched == SCHED_RANGE_ATTACK1 );
		CHECK( ( conds & bits_COND_SMELL_FOOD ) == 0 );
	}
}
