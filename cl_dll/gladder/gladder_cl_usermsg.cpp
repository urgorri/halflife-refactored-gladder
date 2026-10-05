/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client Network User Messages Parsing Implementation (SPEC §9.1, §9.2, §12, GitHub Issue #14)
 *
 ****/

#include "gladder_cl_usermsg.h"
#include <cstring>

static inline int16_t ReadInt16( const uint8_t *p )
{
	return static_cast<int16_t>( p[0] | ( p[1] << 8 ) );
}

static inline int32_t ReadInt32( const uint8_t *p )
{
	return static_cast<int32_t>( p[0] | ( p[1] << 8 ) | ( p[2] << 16 ) | ( p[3] << 24 ) );
}

static inline float ReadCoord( const uint8_t *p )
{
	int16_t raw = ReadInt16( p );
	return static_cast<float>( raw ) * 0.125f;
}

bool Gladder_ParseWaveMsg( const void *pbuf, int iSize, GladderWaveMsg &outMsg )
{
	if ( !pbuf || iSize < 7 )
		return false;

	const uint8_t *p = static_cast<const uint8_t *>( pbuf );
	outMsg.iWaveState              = p[0];
	outMsg.iWaveNumber             = ReadInt16( p + 1 );
	outMsg.flSessionTimeRemaining = ReadCoord( p + 3 );
	outMsg.flLapTime              = ReadCoord( p + 5 );

	return true;
}

bool Gladder_ParseTelemetryMsg( const void *pbuf, int iSize, GladderTelemetryMsg &outMsg )
{
	if ( !pbuf || iSize < 10 )
		return false;

	const uint8_t *p = static_cast<const uint8_t *>( pbuf );
	outMsg.flFastestLap  = ReadCoord( p + 0 );
	outMsg.flSlowestLap  = ReadCoord( p + 2 );
	outMsg.flAverageLap  = ReadCoord( p + 4 );
	outMsg.iCompletedLaps= ReadInt16( p + 6 );
	outMsg.iFrags        = ReadInt16( p + 8 );

	if ( iSize >= 12 )
	{
		outMsg.iCollectibles = ReadInt16( p + 10 );
	}
	else
	{
		outMsg.iCollectibles = 0;
	}

	return true;
}

bool Gladder_ParseComboMsg( const void *pbuf, int iSize, GladderComboMsg &outMsg )
{
	if ( !pbuf || iSize < 2 )
		return false;

	const uint8_t *p = static_cast<const uint8_t *>( pbuf );
	outMsg.iComboMultiplier = p[0];
	outMsg.flDecayFraction  = static_cast<float>( p[1] ) / 255.0f;

	return true;
}

bool Gladder_ParseNotifyMsg( const void *pbuf, int iSize, GladderNotifyMsg &outMsg )
{
	if ( !pbuf || iSize < 3 )
		return false;

	const uint8_t *p = static_cast<const uint8_t *>( pbuf );
	outMsg.iType        = p[0];
	outMsg.iWaveNumber  = p[1];
	outMsg.iMutatorType = p[2];

	return true;
}

bool Gladder_ParseSummaryMsg( const void *pbuf, int iSize, GladderSummaryMsg &outMsg )
{
	if ( !pbuf || iSize < 49 )
		return false;

	const uint8_t *p = static_cast<const uint8_t *>( pbuf );
	outMsg.bSurvived            = ( p[0] != 0 );
	outMsg.iWavesCompleted       = ReadInt16( p + 1 );
	outMsg.iFinalScore           = ReadInt32( p + 3 );
	outMsg.cRankGrade            = static_cast<char>( p[7] );
	outMsg.iMaxComboStreak       = ReadInt16( p + 8 );
	outMsg.iTotalFrags           = ReadInt16( p + 10 );
	outMsg.iCollectibles         = ReadInt16( p + 12 );
	outMsg.flTotalMatchTime      = ReadCoord( p + 14 );
	outMsg.flAverageLapTime      = ReadCoord( p + 16 );
	outMsg.flFastestLapTime      = ReadCoord( p + 18 );
	outMsg.flSlowestLapTime      = ReadCoord( p + 20 );
	outMsg.flTotalDamage         = ReadCoord( p + 22 );
	outMsg.iBaseCombatPoints     = ReadInt32( p + 24 );
	outMsg.iComboBonusPoints     = ReadInt32( p + 28 );
	outMsg.iWaveBonusPoints      = ReadInt32( p + 32 );
	outMsg.iLambdaBonusPoints    = ReadInt32( p + 36 );
	outMsg.iSurvivalBonusPoints  = ReadInt32( p + 40 );
	outMsg.iDamagePenalty        = ReadInt32( p + 44 );

	int speciesCount = p[48];
	int offset = 49;

	outMsg.speciesLog.clear();
	for ( int i = 0; i < speciesCount && offset < iSize; ++i )
	{
		// Read null-terminated string
		int strLen = 0;
		while ( offset + strLen < iSize && p[offset + strLen] != '\0' )
		{
			strLen++;
		}
		if ( offset + strLen >= iSize )
			break;

		std::string speciesName( reinterpret_cast<const char *>( p + offset ), strLen );
		offset += strLen + 1;

		if ( offset + 2 > iSize )
			break;

		int kills = ReadInt16( p + offset );
		offset += 2;

		GladderSpeciesFrag frag( speciesName, kills );
		outMsg.speciesLog.push_back( frag );
	}

	return true;
}
