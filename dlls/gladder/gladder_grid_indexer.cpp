/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Spatial Grid Indexer, Surface Raycasting & Binary Cache (.grid) Implementation
 *	(SPEC §4, GitHub Issue #6)
 *
 ****/

#include "gladder_grid_indexer.h"
#include "gladder_entities.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#define MKDIR( dir ) _mkdir( dir )
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR( dir ) mkdir( dir, 0755 )
#endif

// Helper to ensure target directories exist for binary cache
static void EnsureGridDirectoryExists( const std::string &filePath )
{
	// Ensure "maps" and "maps/grid" exist
	MKDIR( "maps" );
	MKDIR( "maps/grid" );
}

GladderGridIndexer::GladderGridIndexer()
    : m_bLoaded( false )
{
}

GladderGridIndexer::~GladderGridIndexer()
{
	Clear();
}

void GladderGridIndexer::Clear()
{
	m_cells.clear();
	m_mapName.clear();
	m_bLoaded = false;
}

std::string GladderGridIndexer::GetGridFilePath( const char *pszMapName )
{
	if ( !pszMapName || !*pszMapName )
		return "maps/grid/unknown.grid";

	// Strip any path or extension if provided
	std::string base = pszMapName;
	size_t lastSlash = base.find_last_of( "/\\" );
	if ( lastSlash != std::string::npos )
		base = base.substr( lastSlash + 1 );

	size_t lastDot = base.find_last_of( '.' );
	if ( lastDot != std::string::npos )
		base = base.substr( 0, lastDot );

	return "maps/grid/" + base + ".grid";
}

bool GladderGridIndexer::LoadOrCreate( const char *pszMapName )
{
	if ( !pszMapName || !*pszMapName )
		return false;

	m_mapName = pszMapName;
	std::string gridPath = GetGridFilePath( pszMapName );

	// 1. Try loading cached fast path (SPEC §4.1)
	if ( LoadFromFile( gridPath.c_str() ) )
	{
		ALERT( at_console, "[Gladder] Loaded %u spatial grid cells from '%s'\n",
		       static_cast<unsigned int>( m_cells.size() ), gridPath.c_str() );
		return true;
	}

	// 2. Cache miss: generate 32-unit grid from geometry & bounding areas
	ALERT( at_console, "[Gladder] Spatial grid cache missing for '%s'. Indexing geometry...\n", pszMapName );
	bool bBuilt = BuildGrid( pszMapName );
	if ( bBuilt && !m_cells.empty() )
	{
		// 3. Serialize to disk for subsequent instant loads
		SaveToFile( gridPath.c_str() );
		ALERT( at_console, "[Gladder] Generated and cached %u spatial cells to '%s'\n",
		       static_cast<unsigned int>( m_cells.size() ), gridPath.c_str() );
	}
	else
	{
		ALERT( at_console, "[Gladder] WARNING: Spatial grid generation produced 0 valid cells for '%s'\n", pszMapName );
	}

	return bBuilt;
}

bool GladderGridIndexer::LoadFromFile( const char *pszFilePath )
{
	if ( !pszFilePath || !*pszFilePath )
		return false;

	FILE *pFile = fopen( pszFilePath, "rb" );
	if ( !pFile )
		return false;

	GladderGridHeader header;
	if ( fread( &header, sizeof( header ), 1, pFile ) != 1 )
	{
		fclose( pFile );
		return false;
	}

	// Verify magic and format version
	if ( memcmp( header.magic, "GLADGRID", 8 ) != 0 || header.version != 1 )
	{
		ALERT( at_console, "[Gladder] Corrupt or incompatible grid cache header in '%s'\n", pszFilePath );
		fclose( pFile );
		return false;
	}

	m_cells.clear();
	m_cells.reserve( header.cellCount );

	std::vector<GladderGridDiskCell> diskCells( header.cellCount );
	if ( header.cellCount > 0 )
	{
		size_t readCount = fread( diskCells.data(), sizeof( GladderGridDiskCell ), header.cellCount, pFile );
		if ( readCount != header.cellCount )
		{
			ALERT( at_console, "[Gladder] Truncated grid cache file '%s' (expected %u cells, read %u)\n",
			       pszFilePath, header.cellCount, static_cast<unsigned int>( readCount ) );
			fclose( pFile );
			m_cells.clear();
			return false;
		}

		for ( size_t i = 0; i < header.cellCount; ++i )
		{
			const auto &dc = diskCells[i];
			GladderGridCell cell;
			cell.origin.x = dc.origin[0];
			cell.origin.y = dc.origin[1];
			cell.origin.z = dc.origin[2];
			cell.normal.x = dc.normal[0];
			cell.normal.y = dc.normal[1];
			cell.normal.z = dc.normal[2];
			cell.areaId   = dc.areaId;
			cell.flags    = dc.flags;
			cell.ceilingZ = dc.ceilingZ;
			m_cells.push_back( cell );
		}
	}

	fclose( pFile );
	m_bLoaded = true;
	return true;
}

bool GladderGridIndexer::SaveToFile( const char *pszFilePath ) const
{
	if ( !pszFilePath || !*pszFilePath )
		return false;

	EnsureGridDirectoryExists( pszFilePath );

	FILE *pFile = fopen( pszFilePath, "wb" );
	if ( !pFile )
	{
		ALERT( at_console, "[Gladder] Error opening grid cache '%s' for writing\n", pszFilePath );
		return false;
	}

	GladderGridHeader header;
	memset( &header, 0, sizeof( header ) );
	memcpy( header.magic, "GLADGRID", 8 );
	header.version   = 1;
	header.cellCount = static_cast<uint32_t>( m_cells.size() );
	header.areaCount = 0; // Calculated on load if needed
	header.flags     = 0;

	if ( fwrite( &header, sizeof( header ), 1, pFile ) != 1 )
	{
		fclose( pFile );
		return false;
	}

	std::vector<GladderGridDiskCell> diskCells;
	diskCells.reserve( m_cells.size() );

	for ( const auto &c : m_cells )
	{
		GladderGridDiskCell dc;
		dc.origin[0] = c.origin.x;
		dc.origin[1] = c.origin.y;
		dc.origin[2] = c.origin.z;
		dc.normal[0] = c.normal.x;
		dc.normal[1] = c.normal.y;
		dc.normal[2] = c.normal.z;
		dc.areaId    = c.areaId;
		dc.flags     = c.flags;
		dc.ceilingZ  = c.ceilingZ;
		diskCells.push_back( dc );
	}

	if ( !diskCells.empty() )
	{
		size_t written = fwrite( diskCells.data(), sizeof( GladderGridDiskCell ), diskCells.size(), pFile );
		if ( written != diskCells.size() )
		{
			fclose( pFile );
			return false;
		}
	}

	fclose( pFile );
	return true;
}

bool GladderGridIndexer::BuildGrid( const char *pszMapName )
{
	Clear();
	m_mapName = pszMapName ? pszMapName : "";

	// 1. Scan for all registered trigger_gladder_area bounding volumes
	int areaCount = 0;
	CBaseEntity *pEnt = nullptr;

	while ( ( pEnt = UTIL_FindEntityByClassname( pEnt, "trigger_gladder_area" ) ) != nullptr )
	{
		CTriggerGladderArea *pArea = static_cast<CTriggerGladderArea *>( pEnt );
		if ( pArea )
		{
			Vector mins = pArea->GetMins();
			Vector maxs = pArea->GetMaxs();
			const char *pszAreaName = pArea->GetAreaName();
			int areaId = pArea->GetAreaId();

			ALERT( at_console, "[Gladder] Indexing area '%s' (ID %d): mins(%.1f, %.1f, %.1f) maxs(%.1f, %.1f, %.1f)\n",
			       ( pszAreaName && *pszAreaName ) ? pszAreaName : "unnamed", areaId,
			       mins.x, mins.y, mins.z, maxs.x, maxs.y, maxs.z );

			IndexArea( mins, maxs, areaId );
			areaCount++;
		}
	}

	// 2. If no trigger_gladder_area volumes exist in the map, scan world bounds (SPEC §4.1)
	if ( areaCount == 0 )
	{
		Vector mins = Vector( -2048.0f, -2048.0f, -512.0f );
		Vector maxs = Vector( 2048.0f, 2048.0f, 512.0f );

		if ( INDEXENT( 0 ) )
		{
			entvars_t *pWorldVars = VARS( INDEXENT( 0 ) );
			if ( pWorldVars && pWorldVars->maxs.x > pWorldVars->mins.x )
			{
				mins = pWorldVars->mins;
				maxs = pWorldVars->maxs;
			}
		}

		ALERT( at_console, "[Gladder] No trigger_gladder_area found, indexing world bounds: mins(%.1f, %.1f, %.1f) maxs(%.1f, %.1f, %.1f)\n",
		       mins.x, mins.y, mins.z, maxs.x, maxs.y, maxs.z );
		IndexArea( mins, maxs, 0 );
	}

	m_bLoaded = !m_cells.empty();
	return m_bLoaded;
}

bool GladderGridIndexer::TraceCellCandidate( float x, float y, float zTop, float zBottom, int32_t areaId, GladderGridCell &outCell )
{
	// 1. Descend from zTop if initial coordinate penetrates solid ceiling architecture
	Vector vecStart = Vector( x, y, zTop - 1.0f );
	while ( vecStart.z > zBottom + 36.0f && POINT_CONTENTS( vecStart ) == CONTENTS_SOLID )
	{
		vecStart.z -= 16.0f;
	}

	if ( vecStart.z <= zBottom + 36.0f || POINT_CONTENTS( vecStart ) == CONTENTS_SOLID )
		return false; // Entire vertical column is solid architecture (pillar/wall)

	Vector vecEnd = Vector( x, y, zBottom - 32.0f );

	// Downward raycast to find walkable solid surface
	TraceResult tr;
	TRACE_LINE( vecStart, vecEnd, TRUE, NULL, &tr );

	// Must hit a solid surface within bounds
	if ( tr.flFraction >= 1.0f || tr.fStartSolid || tr.fAllSolid )
		return false;

	// Surface normal slope check (GoldSrc walkable slope: normal.z >= 0.7)
	if ( tr.vecPlaneNormal.z < WALKABLE_NORMAL_Z )
		return false;

	Vector vecSurface = tr.vecEndPos;

	// Check if surface point is erroneously inside solid brush
	if ( POINT_CONTENTS( vecSurface + Vector( 0.0f, 0.0f, 4.0f ) ) == CONTENTS_SOLID )
		return false;

	// Trace upward to locate overhead ceiling geometry (SPEC §4.3 & §4.4)
	TraceResult trCeiling;
	TRACE_LINE( vecSurface + Vector( 0.0f, 0.0f, 1.0f ), Vector( x, y, zTop + 1024.0f ), TRUE, NULL, &trCeiling );

	float flCeilingZ = ( trCeiling.flFraction < 1.0f ) ? trCeiling.vecEndPos.z : ( zTop + 1024.0f );
	float flClearance = flCeilingZ - vecSurface.z;

	// Vertical height clearance verification
	if ( flClearance < MIN_CLEARANCE )
		return false;

	// Standing hull obstruction sweep (GoldSrc human_hull: 32x32x72)
	// In GoldSrc, human_hull origin is centered at entity waist: mins(-16,-16,-36), maxs(16,16,36).
	// To position feet resting on the walkable surface, the hull origin must be at vecSurface.z + 37.0f.
	Vector vecHullPos = vecSurface + Vector( 0.0f, 0.0f, 37.0f );
	TraceResult trHull;
	TRACE_HULL( vecHullPos, vecHullPos, TRUE, human_hull, NULL, &trHull );

	if ( trHull.fStartSolid || trHull.fAllSolid )
		return false;

	// Populate valid candidate cell
	outCell.origin   = vecSurface + Vector( 0.0f, 0.0f, 1.0f );
	outCell.normal   = tr.vecPlaneNormal;
	outCell.areaId   = areaId;
	outCell.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
	if ( trCeiling.flFraction < 1.0f )
		outCell.flags |= GLADDER_CELL_CEILING_VALID;
	outCell.ceilingZ = flCeilingZ;

	return true;
}

void GladderGridIndexer::IndexArea( const Vector &vecMins, const Vector &vecMaxs, int32_t areaId )
{
	float flStep = GRID_STEP;

	// Ensure bounds are non-degenerate
	if ( vecMaxs.x <= vecMins.x || vecMaxs.y <= vecMins.y || vecMaxs.z <= vecMins.z )
		return;

	// 32-unit regular sampling interval across X and Y
	for ( float x = vecMins.x + ( flStep * 0.5f ); x <= vecMaxs.x - ( flStep * 0.5f ); x += flStep )
	{
		for ( float y = vecMins.y + ( flStep * 0.5f ); y <= vecMaxs.y - ( flStep * 0.5f ); y += flStep )
		{
			GladderGridCell cell;
			if ( TraceCellCandidate( x, y, vecMaxs.z, vecMins.z, areaId, cell ) )
			{
				m_cells.push_back( cell );
			}
		}
	}
}

void GladderGridIndexer::AddCell( const GladderGridCell &cell )
{
	m_cells.push_back( cell );
	m_bLoaded = true;
}

std::vector<GladderGridCell> GladderGridIndexer::GetCellsForArea( int32_t areaId ) const
{
	std::vector<GladderGridCell> result;
	for ( const auto &c : m_cells )
	{
		if ( c.areaId == areaId )
			result.push_back( c );
	}
	return result;
}

const GladderGridCell *GladderGridIndexer::FindNearestCell( const Vector &pos, float maxDist ) const
{
	const GladderGridCell *pBest = nullptr;
	float flBestDistSq = maxDist * maxDist;

	for ( const auto &c : m_cells )
	{
		Vector diff = c.origin - pos;
		float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
		if ( distSq < flBestDistSq )
		{
			flBestDistSq = distSq;
			pBest = &c;
		}
	}

	return pBest;
}

const GladderGridCell *GladderGridIndexer::GetRandomCell( int32_t areaId, uint32_t requiredFlags ) const
{
	if ( m_cells.empty() )
		return nullptr;

	std::vector<size_t> eligible;
	eligible.reserve( m_cells.size() );

	for ( size_t i = 0; i < m_cells.size(); ++i )
	{
		const auto &c = m_cells[i];
		if ( ( c.flags & requiredFlags ) != requiredFlags )
			continue;
		if ( areaId >= 0 && c.areaId != areaId )
			continue;
		eligible.push_back( i );
	}

	if ( eligible.empty() )
		return nullptr;

	size_t idx = static_cast<size_t>( RANDOM_LONG( 0, static_cast<long>( eligible.size() - 1 ) ) );
	return &m_cells[eligible[idx]];
}

// Console command: gladder_reindex_grid
void Gladder_ReindexGrid_Cmd( void )
{
	const char *pszMap = gpGlobals ? STRING( gpGlobals->mapname ) : "unknown";
	ALERT( at_console, "[Gladder] Forcing spatial grid reindex for map '%s'...\n", pszMap );

	GladderGridIndexer indexer;
	if ( indexer.BuildGrid( pszMap ) )
	{
		std::string path = GladderGridIndexer::GetGridFilePath( pszMap );
		indexer.SaveToFile( path.c_str() );
		ALERT( at_console, "[Gladder] Spatial grid reindex complete: %u cells saved to '%s'\n",
		       static_cast<unsigned int>( indexer.GetCellCount() ), path.c_str() );
	}
	else
	{
		ALERT( at_console, "[Gladder] Spatial grid reindex failed: no valid cells found.\n" );
	}
}
