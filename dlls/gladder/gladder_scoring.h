/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Composite Score Calculation & Arcade Performance Grading
 *	(SPEC §3, §9.2, GitHub Issue #9)
 *
 ****/

#pragma once

#ifndef GLADDER_SCORING_H
#define GLADDER_SCORING_H

#include "core/extdll.h"
#include "gladder_combo_tracker.h"
#include <string>

// Comprehensive score breakdown for end-of-run summary and leaderboards
struct GladderScoreBreakdown
{
	int baseCombatPoints      = 0;
	int comboBonusPoints       = 0;
	int waveBonusPoints        = 0;
	int lambdaBonusPoints      = 0;
	int survivalBonusPoints    = 0;
	int damagePenaltyPoints    = 0;
	int finalCompositeScore    = 0;
	char rankGrade             = 'F'; // 'S', 'A', 'B', 'C', 'D', 'F'
	std::string rankTitle      = "F - ELIMINATED";

	// Match metadata snapshot
	int wavesCompleted         = 0;
	int totalFrags             = 0;
	int collectiblesCount      = 0;
	int maxComboStreak         = 0;
	float totalMatchTime       = 0.0f;
	float averageLapTime       = 0.0f;
	float fastestLapTime       = 0.0f;
	float slowestLapTime       = 0.0f;
	float totalDamageTaken     = 0.0f;
	bool bSurvived             = false;
};

class GladderScoring
{
  public:
	// Points awarded per collected Lambda insignia
	static constexpr int LAMBDA_COLLECTIBLE_POINTS = 2500;

	// Bonus awarded for surviving until time expiration
	static constexpr int SURVIVAL_COMPLETION_BONUS = 5000;

	// Bonus points per wave completed
	static constexpr int WAVE_COMPLETION_POINTS = 1000;

	// Calculate granular score breakdown and determine performance rank
	static GladderScoreBreakdown CalculateScore(
		const GladderKillStats &kills,
		int iWavesCompleted,
		int iCollectibles,
		float flTotalTime,
		float flAverageLapTime,
		float flFastestLapTime,
		float flSlowestLapTime,
		float flDamageTaken,
		bool bSurvived
	);

	// Get base point value for defeating a specific monster species
	static int GetSpeciesBasePoints( const std::string &szClassname );

	// Determine letter rank grade (S, A, B, C, D, F) and title
	static char DetermineRankGrade( int finalScore, int wavesCompleted, int maxCombo, float damageTaken, bool bSurvived, std::string &outTitle );
};

#endif // GLADDER_SCORING_H
