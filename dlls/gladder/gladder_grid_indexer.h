/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Spatial Grid Indexer, Surface Raycasting & Binary Cache (.grid)
 *	(SPEC §4, GitHub Issue #6)
 *
 ****/

#pragma once

#ifndef GLADDER_GRID_INDEXER_H
#define GLADDER_GRID_INDEXER_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include <vector>
#include <string>
#include <cstdint>

// Flags indicating properties of an indexed grid cell
enum GladderGridCellFlags : uint32_t
{
	GLADDER_CELL_VALID         = ( 1u << 0 ), // Walkable solid surface detected
	GLADDER_CELL_CLEARANCE_OK  = ( 1u << 1 ), // Vertical standing clearance confirmed (no overhead obstruction)
	GLADDER_CELL_CEILING_VALID = ( 1u << 2 ), // Valid solid ceiling detected overhead
	GLADDER_CELL_NO_MONSTERS   = ( 1u << 3 ), // Excluded from monster spawns (e.g. staging safe zone)
	GLADDER_CELL_PICKUP_ONLY     = ( 1u << 4 ), // Dedicated for item/ammo/pickup spawns
	GLADDER_CELL_LARGE_CLEARANCE = ( 1u << 5 ), // Large hull (64x64) clearance confirmed
};

// Runtime in-memory grid cell
struct GladderGridCell
{
	Vector origin;       // Spawn coordinate resting on walkable surface
	Vector normal;       // Surface plane normal
	int32_t areaId;      // Associated trigger_gladder_area ID (0 if global world bounds)
	uint32_t flags;      // Bitmask of GladderGridCellFlags
	float ceilingZ;      // Highest ceiling height directly above origin
};

// On-disk binary cache file format structures (fixed alignment for multiplatform consistency)
#pragma pack(push, 1)
struct GladderGridHeader
{
	char magic[8];        // "GLADGRID"
	uint32_t version;     // Format version (currently 1)
	uint32_t cellCount;   // Number of serialized cells
	uint32_t areaCount;   // Number of indexed areas
	uint32_t flags;       // Reserved flags
	uint32_t reserved[3]; // Padding for future expansion
};

struct GladderGridDiskCell
{
	float origin[3];
	float normal[3];
	int32_t areaId;
	uint32_t flags;
	float ceilingZ;
};
#pragma pack(pop)

class GladderGridIndexer
{
  public:
	static constexpr float GRID_STEP         = 32.0f; // 32-unit horizontal sampling interval (SPEC §4.3)
	static constexpr float MIN_CLEARANCE     = 72.0f; // Standard standing clearance (GoldSrc human_hull height)
	static constexpr float WALKABLE_NORMAL_Z = 0.7f;  // Approx 45-degree slope max (GoldSrc walkable slope)

	GladderGridIndexer();
	~GladderGridIndexer();

	// Cache check & loader flow (SPEC §4.1)
	// If maps/grid/<mapname>.grid exists, loads from file directly.
	// Otherwise performs spatial scan, builds grid, and writes cache file.
	bool LoadOrCreate( const char *pszMapName );

	// Explicit file I/O
	bool LoadFromFile( const char *pszFilePath );
	bool SaveToFile( const char *pszFilePath ) const;

	// Grid generation via raycasting & area scan
	bool BuildGrid( const char *pszMapName );
	void IndexArea( const Vector &vecMins, const Vector &vecMaxs, int32_t areaId = 0 );

	// Storage & accessors
	void Clear();
	size_t GetCellCount() const { return m_cells.size(); }
	const GladderGridCell &GetCell( size_t index ) const { return m_cells[index]; }
	const std::vector<GladderGridCell> &GetCells() const { return m_cells; }
	std::vector<GladderGridCell> GetCellsForArea( int32_t areaId ) const;
	bool IsLoaded() const { return m_bLoaded; }
	const std::string &GetMapName() const { return m_mapName; }

	// Spatial queries
	const GladderGridCell *FindNearestCell( const Vector &pos, float maxDist = 256.0f ) const;
	int GetRandomCellIndex( int32_t areaId = -1, uint32_t requiredFlags = ( GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK ), const std::vector<bool> *pOccupiedCells = nullptr ) const;
	const GladderGridCell *GetRandomCell( int32_t areaId = -1, uint32_t requiredFlags = ( GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK ) ) const;

	// Helper for cache path resolution: "maps/grid/<mapname>.grid"
	static std::string GetGridFilePath( const char *pszMapName );

	// Manual cell insertion (useful for testing or author overrides)
	void AddCell( const GladderGridCell &cell );

	// Raycast single cell candidate (public for unit testability)
	bool TraceCellCandidate( float x, float y, float zTop, float zBottom, int32_t areaId, GladderGridCell &outCell );

  private:
	std::vector<GladderGridCell> m_cells;
	std::string m_mapName;
	bool m_bLoaded;
};

// Returns true if a position penetrates static world architecture (CONTENTS_SOLID)
// or is inside any active solid brush entity (func_wall, func_monsterclip, func_door, etc.)
// (SPEC §4.3, Issue #37)
bool GladderIsPositionSolidOrBrush( const Vector &vecPos );

// Console command handler for "gladder_reindex_grid"
void Gladder_ReindexGrid_Cmd( void );

#endif // GLADDER_GRID_INDEXER_H
