/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Persistent Local Map Leaderboards (SPEC §10.2, GitHub Issue #17)
 *
 ****/

#pragma once

#ifndef GLADDER_LEADERBOARD_H
#define GLADDER_LEADERBOARD_H

#include <cstdint>
#include <string>
#include <vector>

// Maximum entries stored per map
constexpr size_t GLADDER_MAX_LEADERBOARD_ENTRIES = 10;

// Structure representing a single recorded match run
struct GladderLeaderboardEntry
{
	uint64_t timestamp = 0;   // Epoch timestamp (seconds)
	int score          = 0;   // Final composite score
	int waves          = 0;   // Waves completed
	int frags          = 0;   // Total monsters eliminated
	float totalTime    = 0.0f;// Total duration of the run (seconds)
	char grade         = 'F'; // Performance grade (S, A, B, C, D, F)
	bool survived      = false;// True if survived until time expiration, false if KIA
};

class GladderLeaderboard
{
  public:
	// Submit a match result for a map. Returns true if score qualified for Top 10.
	// outRank: 1-indexed ranking (1..10) if qualified, or -1 if unranked.
	// outIsNewHighScore: true if the score achieved the #1 rank.
	static bool SubmitScore(
		const std::string &mapname,
		const GladderLeaderboardEntry &entry,
		int &outRank,
		bool &outIsNewHighScore
	);

	// Load Top 10 entries for a map
	static bool Load( const std::string &mapname, std::vector<GladderLeaderboardEntry> &outEntries );

	// Save Top 10 entries for a map
	static bool Save( const std::string &mapname, const std::vector<GladderLeaderboardEntry> &entries );

	// Sorts a list of entries and trims to Top 10
	static void SortAndTrim( std::vector<GladderLeaderboardEntry> &entries );

	// Computes file path for a map: gladder/scores/<mapname>.dat
	static std::string GetScoreFilePath( const std::string &mapname );

	// Helper to set base storage path (defaults to "gladder/scores")
	static void SetStorageDirectory( const std::string &dir );
	static std::string GetStorageDirectory( void );

	// Ensure destination directory exists
	static bool EnsureDirectoryExists( const std::string &dirPath );

  private:
	static uint32_t CalculateChecksum( const std::vector<GladderLeaderboardEntry> &entries );
	static std::string s_storageDirectory;
};

#endif // GLADDER_LEADERBOARD_H
