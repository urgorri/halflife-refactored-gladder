/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Persistent Local Map Leaderboards Implementation (SPEC §10.2, GitHub Issue #17)
 *
 ****/

#include "gladder_leaderboard.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR( path ) _mkdir( path )
#else
#include <unistd.h>
#define MKDIR( path ) mkdir( path, 0755 )
#endif

static const char GLADDER_LB_MAGIC[8] = { 'G', 'L', 'A', 'D', 'L', 'B', '0', '1' };
static const uint32_t GLADDER_LB_VERSION = 1;

std::string GladderLeaderboard::s_storageDirectory = "gladder/scores";

void GladderLeaderboard::SetStorageDirectory( const std::string &dir )
{
	s_storageDirectory = dir;
}

std::string GladderLeaderboard::GetStorageDirectory( void )
{
	return s_storageDirectory;
}

std::string GladderLeaderboard::GetScoreFilePath( const std::string &mapname )
{
	std::string cleanMap = mapname;
	// Strip path or extension if provided
	size_t lastSlash = cleanMap.find_last_of( "/\\" );
	if ( lastSlash != std::string::npos )
	{
		cleanMap = cleanMap.substr( lastSlash + 1 );
	}
	size_t dot = cleanMap.find_last_of( '.' );
	if ( dot != std::string::npos )
	{
		cleanMap = cleanMap.substr( 0, dot );
	}
	if ( cleanMap.empty() )
	{
		cleanMap = "default";
	}

	return s_storageDirectory + "/" + cleanMap + ".dat";
}

bool GladderLeaderboard::EnsureDirectoryExists( const std::string &dirPath )
{
	if ( dirPath.empty() )
		return true;

	std::string current;
	for ( size_t i = 0; i < dirPath.size(); ++i )
	{
		char c = dirPath[i];
		if ( c == '/' || c == '\\' )
		{
			if ( !current.empty() )
			{
				MKDIR( current.c_str() );
			}
		}
		current += c;
	}
	if ( !current.empty() )
	{
		MKDIR( current.c_str() );
	}

	struct stat st;
	return ( stat( dirPath.c_str(), &st ) == 0 && ( st.st_mode & S_IFDIR ) );
}

static void HashBytes( uint32_t &checksum, const void *data, size_t size )
{
	const uint8_t *p = static_cast<const uint8_t *>( data );
	for ( size_t i = 0; i < size; ++i )
	{
		checksum ^= p[i];
		checksum *= 16777619u;
	}
}

uint32_t GladderLeaderboard::CalculateChecksum( const std::vector<GladderLeaderboardEntry> &entries )
{
	uint32_t checksum = 2166136261u; // FNV-1a basis
	for ( const auto &e : entries )
	{
		uint64_t ts = e.timestamp;
		int32_t s = e.score;
		int32_t w = e.waves;
		int32_t f = e.frags;
		float t = e.totalTime;
		char g = e.grade;
		uint8_t surv = e.survived ? 1 : 0;

		HashBytes( checksum, &ts, sizeof( ts ) );
		HashBytes( checksum, &s, sizeof( s ) );
		HashBytes( checksum, &w, sizeof( w ) );
		HashBytes( checksum, &f, sizeof( f ) );
		HashBytes( checksum, &t, sizeof( t ) );
		HashBytes( checksum, &g, sizeof( g ) );
		HashBytes( checksum, &surv, sizeof( surv ) );
	}
	return checksum;
}

void GladderLeaderboard::SortAndTrim( std::vector<GladderLeaderboardEntry> &entries )
{
	std::sort( entries.begin(), entries.end(), []( const GladderLeaderboardEntry &a, const GladderLeaderboardEntry &b ) {
		if ( a.score != b.score )
			return a.score > b.score; // Higher score is better
		if ( a.waves != b.waves )
			return a.waves > b.waves; // More waves cleared is better
		if ( a.totalTime != b.totalTime )
			return a.totalTime < b.totalTime; // Faster time is better
		return a.timestamp > b.timestamp; // More recent is better
	} );

	if ( entries.size() > GLADDER_MAX_LEADERBOARD_ENTRIES )
	{
		entries.resize( GLADDER_MAX_LEADERBOARD_ENTRIES );
	}
}

bool GladderLeaderboard::Load( const std::string &mapname, std::vector<GladderLeaderboardEntry> &outEntries )
{
	outEntries.clear();
	std::string filepath = GetScoreFilePath( mapname );

	FILE *fp = fopen( filepath.c_str(), "rb" );
	if ( !fp )
		return false;

	char magic[8];
	if ( fread( magic, 1, 8, fp ) != 8 || memcmp( magic, GLADDER_LB_MAGIC, 8 ) != 0 )
	{
		fclose( fp );
		return false;
	}

	uint32_t version = 0;
	if ( fread( &version, sizeof( uint32_t ), 1, fp ) != 1 || version != GLADDER_LB_VERSION )
	{
		fclose( fp );
		return false;
	}

	uint32_t count = 0;
	if ( fread( &count, sizeof( uint32_t ), 1, fp ) != 1 || count > GLADDER_MAX_LEADERBOARD_ENTRIES )
	{
		fclose( fp );
		return false;
	}

	uint32_t storedChecksum = 0;
	if ( fread( &storedChecksum, sizeof( uint32_t ), 1, fp ) != 1 )
	{
		fclose( fp );
		return false;
	}

	std::vector<GladderLeaderboardEntry> loadedEntries( count );
	for ( uint32_t i = 0; i < count; ++i )
	{
		auto &e = loadedEntries[i];
		int32_t s = 0, w = 0, f = 0;
		uint8_t surv = 0;

		if ( fread( &e.timestamp, sizeof( uint64_t ), 1, fp ) != 1 ||
		     fread( &s, sizeof( int32_t ), 1, fp ) != 1 ||
		     fread( &w, sizeof( int32_t ), 1, fp ) != 1 ||
		     fread( &f, sizeof( int32_t ), 1, fp ) != 1 ||
		     fread( &e.totalTime, sizeof( float ), 1, fp ) != 1 ||
		     fread( &e.grade, sizeof( char ), 1, fp ) != 1 ||
		     fread( &surv, sizeof( uint8_t ), 1, fp ) != 1 )
		{
			fclose( fp );
			return false;
		}

		e.score = s;
		e.waves = w;
		e.frags = f;
		e.survived = ( surv != 0 );
	}
	fclose( fp );

	uint32_t computedChecksum = CalculateChecksum( loadedEntries );
	if ( storedChecksum != computedChecksum )
	{
		return false; // Corrupted file rejection
	}

	SortAndTrim( loadedEntries );
	outEntries = std::move( loadedEntries );
	return true;
}

bool GladderLeaderboard::Save( const std::string &mapname, const std::vector<GladderLeaderboardEntry> &entries )
{
	EnsureDirectoryExists( s_storageDirectory );
	std::string filepath = GetScoreFilePath( mapname );

	std::vector<GladderLeaderboardEntry> copy = entries;
	SortAndTrim( copy );

	FILE *fp = fopen( filepath.c_str(), "wb" );
	if ( !fp )
		return false;

	if ( fwrite( GLADDER_LB_MAGIC, 1, 8, fp ) != 8 )
	{
		fclose( fp );
		return false;
	}

	uint32_t version = GLADDER_LB_VERSION;
	fwrite( &version, sizeof( uint32_t ), 1, fp );

	uint32_t count = static_cast<uint32_t>( copy.size() );
	fwrite( &count, sizeof( uint32_t ), 1, fp );

	uint32_t checksum = CalculateChecksum( copy );
	fwrite( &checksum, sizeof( uint32_t ), 1, fp );

	for ( const auto &e : copy )
	{
		int32_t s = e.score;
		int32_t w = e.waves;
		int32_t f = e.frags;
		uint8_t surv = e.survived ? 1 : 0;

		fwrite( &e.timestamp, sizeof( uint64_t ), 1, fp );
		fwrite( &s, sizeof( int32_t ), 1, fp );
		fwrite( &w, sizeof( int32_t ), 1, fp );
		fwrite( &f, sizeof( int32_t ), 1, fp );
		fwrite( &e.totalTime, sizeof( float ), 1, fp );
		fwrite( &e.grade, sizeof( char ), 1, fp );
		fwrite( &surv, sizeof( uint8_t ), 1, fp );
	}

	fclose( fp );
	return true;
}

bool GladderLeaderboard::SubmitScore(
	const std::string &mapname,
	const GladderLeaderboardEntry &entry,
	int &outRank,
	bool &outIsNewHighScore
)
{
	outRank = -1;
	outIsNewHighScore = false;

	std::vector<GladderLeaderboardEntry> entries;
	Load( mapname, entries );

	entries.push_back( entry );
	SortAndTrim( entries );

	// Determine if submitted entry is within Top 10
	for ( size_t i = 0; i < entries.size(); ++i )
	{
		if ( entries[i].timestamp == entry.timestamp &&
		     entries[i].score == entry.score &&
		     entries[i].waves == entry.waves &&
		     entries[i].frags == entry.frags )
		{
			outRank = static_cast<int>( i + 1 );
			outIsNewHighScore = ( outRank == 1 );
			Save( mapname, entries );
			return true;
		}
	}

	return false;
}
