/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Composite Score Calculation & Arcade Performance Grading Implementation
 *	(SPEC §3, §9.2, GitHub Issue #9)
 *
 ****/

#include "gladder_scoring.h"
#include <algorithm>

int GladderScoring::GetSpeciesBasePoints( const std::string &szClassname )
{
	// Tier 1: Fodder
	if ( szClassname == "monster_headcrab" || szClassname == "monster_babycrab" || szClassname == "monster_snark" )
		return 50;

	// Tier 2: Basic threats
	if ( szClassname == "monster_zombie" || szClassname == "monster_houndeye" || szClassname == "monster_barnacle" )
		return 100;

	// Tier 3: Medium threats
	if ( szClassname == "monster_bullchicken" || szClassname == "monster_alien_slave" || szClassname == "monster_vortigaunt" || szClassname == "monster_bloater" )
		return 250;

	// Tier 4: Heavy threats
	if ( szClassname == "monster_alien_grunt" || szClassname == "monster_human_grunt" || szClassname == "monster_human_assassin" || szClassname == "monster_alien_controller" )
		return 500;

	// Tier 5: Apex / Boss monsters
	if ( szClassname == "monster_gargantua" || szClassname == "monster_bigmomma" || szClassname == "monster_ichthyosaur" || szClassname == "monster_tentacle" )
		return 1000;

	return 100;
}

char GladderScoring::DetermineRankGrade( int finalScore, int wavesCompleted, int maxCombo, float damageTaken, bool bSurvived, std::string &outTitle )
{
	// S Rank: Exceptional mastery (deep wave run, high combat throughput, strong combo control)
	if ( wavesCompleted >= 10 && finalScore >= 35000 && maxCombo >= 5 )
	{
		outTitle = "S - GAUNTLET MASTER";
		return 'S';
	}

	// A Rank: High wave count and excellent performance
	if ( wavesCompleted >= 7 && finalScore >= 20000 && maxCombo >= 3 )
	{
		outTitle = "A - VETERAN RUNNER";
		return 'A';
	}

	// B Rank: Solid run with consistent pacing
	if ( wavesCompleted >= 4 && finalScore >= 10000 )
	{
		outTitle = "B - SOLID CONTENDER";
		return 'B';
	}

	// C Rank: Moderate progress or cautious play
	if ( wavesCompleted >= 2 && finalScore >= 4000 )
	{
		outTitle = "C - APPRENTICE RUNNER";
		return 'C';
	}

	// D Rank: Early progress
	if ( wavesCompleted >= 1 && finalScore >= 1000 )
	{
		outTitle = "D - NOVICE SURVIVOR";
		return 'D';
	}

	// F Rank: Rapid death or insufficient momentum
	outTitle = "F - FALLEN RECRUIT";
	return 'F';
}

GladderScoreBreakdown GladderScoring::CalculateScore(
	const GladderKillStats &kills,
	int iWavesCompleted,
	int iCollectibles,
	float flTotalTime,
	float flAverageLapTime,
	float flFastestLapTime,
	float flSlowestLapTime,
	float flDamageTaken,
	bool bSurvived
)
{
	GladderScoreBreakdown bd;

	// 1. Base Combat Points (per species tier)
	bd.baseCombatPoints = 0;
	for ( const auto &pair : kills.speciesKills )
	{
		int basePts = GetSpeciesBasePoints( pair.first );
		bd.baseCombatPoints += ( pair.second * basePts );
	}

	// 2. Combo Streak Bonus
	bd.comboBonusPoints = ( kills.x2Combos * 150 )
	                    + ( kills.x3Combos * 350 )
	                    + ( kills.x4PlusCombos * 750 )
	                    + ( kills.maxComboStreak * 250 )
	                    + ( kills.headshotKills * 50 );

	// 3. Wave Completion Milestone Bonus + Pacing Incentive
	bd.waveBonusPoints = iWavesCompleted * WAVE_COMPLETION_POINTS;
	if ( iWavesCompleted > 0 && flAverageLapTime > 0.0f && flAverageLapTime <= 60.0f )
	{
		// Aggressive pacing speed bonus: +500 pts per completed wave under 60s avg
		bd.waveBonusPoints += ( iWavesCompleted * 500 );
	}

	// 4. Lambda Collectibles Bonus
	bd.lambdaBonusPoints = iCollectibles * LAMBDA_COLLECTIBLE_POINTS;

	// 5. Survival Bonus
	bd.survivalBonusPoints = bSurvived ? SURVIVAL_COMPLETION_BONUS : 0;

	// 6. Damage Sustained Penalty (0.5 pts deduction per HP/Armor lost)
	bd.damagePenaltyPoints = static_cast<int>( (std::max)( 0.0f, flDamageTaken * 0.5f ) );

	// 7. Final Composite Score
	int rawScore = bd.baseCombatPoints
	             + bd.comboBonusPoints
	             + bd.waveBonusPoints
	             + bd.lambdaBonusPoints
	             + bd.survivalBonusPoints
	             - bd.damagePenaltyPoints;

	bd.finalCompositeScore = (std::max)( 0, rawScore );

	// 8. Performance Rank Rating
	bd.rankGrade = DetermineRankGrade( bd.finalCompositeScore, iWavesCompleted, kills.maxComboStreak, flDamageTaken, bSurvived, bd.rankTitle );

	// Populate metadata snapshot
	bd.wavesCompleted     = iWavesCompleted;
	bd.totalFrags         = kills.totalFrags;
	bd.collectiblesCount  = iCollectibles;
	bd.maxComboStreak     = kills.maxComboStreak;
	bd.totalMatchTime     = flTotalTime;
	bd.averageLapTime     = flAverageLapTime;
	bd.fastestLapTime     = flFastestLapTime;
	bd.slowestLapTime     = flSlowestLapTime;
	bd.totalDamageTaken   = flDamageTaken;
	bd.bSurvived          = bSurvived;

	return bd;
}
