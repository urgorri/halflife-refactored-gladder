/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client Network User Messages & Data Cache (SPEC §9.1, §9.2, §12, GitHub Issue #14)
 *
 ****/

#pragma once

#ifndef GLADDER_CL_USERMSG_H
#define GLADDER_CL_USERMSG_H

#include <cstdint>
#include <string>
#include <vector>

struct GladderWaveMsg
{
	int iWaveState;
	int iWaveNumber;
	float flSessionTimeRemaining;
	float flLapTime;
	int iMonstersKilled;
	int iMonstersTotal;

	GladderWaveMsg()
	    : iWaveState( 0 ), iWaveNumber( 1 ), flSessionTimeRemaining( 0.0f ),
	      flLapTime( 0.0f ), iMonstersKilled( 0 ), iMonstersTotal( 0 )
	{
	}
};

struct GladderTelemetryMsg
{
	float flFastestLap  = 0.0f;
	float flSlowestLap  = 0.0f;
	float flAverageLap  = 0.0f;
	int iCompletedLaps  = 0;
	int iFrags          = 0;
	int iCollectibles   = 0;
};

struct GladderComboMsg
{
	int iComboMultiplier = 1;
	float flDecayFraction = 0.0f; // 0.0f to 1.0f
};

struct GladderNotifyMsg
{
	int iType        = 0; // 0 = STARTED, 1 = COMPLETED
	int iWaveNumber  = 1;
	int iMutatorType = 0;
};

struct GladderSpeciesFrag
{
	std::string speciesName;
	int kills;

	GladderSpeciesFrag() : speciesName(), kills( 0 ) {}
	GladderSpeciesFrag( std::string name, int k ) : speciesName( std::move( name ) ), kills( k ) {}
};

struct GladderSummaryMsg
{
	bool bSurvived             = false;
	int iWavesCompleted        = 0;
	int iFinalScore            = 0;
	char cRankGrade            = 'F';
	int iMaxComboStreak        = 0;
	int iTotalFrags            = 0;
	int iCollectibles          = 0;
	float flTotalMatchTime     = 0.0f;
	float flAverageLapTime     = 0.0f;
	float flFastestLapTime     = 0.0f;
	float flSlowestLapTime     = 0.0f;
	float flTotalDamage        = 0.0f;
	int iBaseCombatPoints      = 0;
	int iComboBonusPoints      = 0;
	int iWaveBonusPoints       = 0;
	int iLambdaBonusPoints     = 0;
	int iSurvivalBonusPoints   = 0;
	int iDamagePenalty         = 0;
	std::vector<GladderSpeciesFrag> speciesLog;
};

struct GladderLambdaMsg
{
	int iScoreBonus       = 0;
	int iCollectibleCount = 0;
};

// Safe buffer parsing functions (used by client hooks and test suites)
bool Gladder_ParseWaveMsg( const void *pbuf, int iSize, GladderWaveMsg &outMsg );
bool Gladder_ParseTelemetryMsg( const void *pbuf, int iSize, GladderTelemetryMsg &outMsg );
bool Gladder_ParseComboMsg( const void *pbuf, int iSize, GladderComboMsg &outMsg );
bool Gladder_ParseNotifyMsg( const void *pbuf, int iSize, GladderNotifyMsg &outMsg );
bool Gladder_ParseSummaryMsg( const void *pbuf, int iSize, GladderSummaryMsg &outMsg );
bool Gladder_ParseLambdaMsg( const void *pbuf, int iSize, GladderLambdaMsg &outMsg );

#endif // GLADDER_CL_USERMSG_H
