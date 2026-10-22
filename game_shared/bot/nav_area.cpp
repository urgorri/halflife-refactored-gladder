// nav_area.cpp
// AI Navigation areas
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#pragma warning( disable : 4530 ) // STL uses exceptions, but we are not compiling with them - ignore warning
#pragma warning( disable : 4786 ) // long STL names get truncated in browse info.

#include <list>
#include <vector>
#include <algorithm>

#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>

#ifdef _WIN32
#include <io.h>

#else
#include <unistd.h>
#define _write write
#define _close close
#define MAX_OSPATH PATH_MAX
#endif

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "core/player.h"
#include "gamerules.h"
#include "bot_util.h"

/// Hostage entity interaction logic
#include "cs_bot.h"
#include "cs_bot_manager.h"
#include "hostage.h"

#include "nav.h"
#include "nav_node.h"
#include "nav_area.h"

#include "pm_shared.h" // for OBS_ROAMING

extern void HintMessageToAllPlayers( const char *message );

unsigned int CNavArea::m_nextID = 1;
NavAreaList TheNavAreaList;

NavLadderList TheNavLadderList;

unsigned int CNavArea::m_masterMarker = 1;
CNavArea *CNavArea::m_openList        = NULL;

bool CNavArea::m_isReset       = false;
static float lastDrawTimestamp = 0.0f;

//--------------------------------------------------------------------------------------------------------------
/**
 * This list contains all "good-sized" areas used to compute "approach points"
 */

void CNavArea::Initialize( void )
{
	m_marker         = 0;
	m_parent         = NULL;
	m_parentHow      = GO_NORTH;
	m_attributeFlags = 0;
	m_place          = 0;

	for ( int i = 0; i < MAX_AREA_TEAMS; ++i )
	{
		m_danger[i]          = 0.0f;
		m_dangerTimestamp[i] = 0.0f;

		m_clearedTimestamp[i] = 0.0f;
	}

	m_approachCount = 0;

	// set an ID for splitting and other interactive editing - loads will overwrite this
	m_id = m_nextID++;

	m_prevHash = NULL;
	m_nextHash = NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor used during normal runtime.
 */
CNavArea::CNavArea( void )
{
	Initialize();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Assumes Z is flat
 */
CNavArea::CNavArea( const Vector *corner, const Vector *otherCorner )
{
	Initialize();

	if ( corner->x < otherCorner->x )
	{
		m_extent.lo.x = corner->x;
		m_extent.hi.x = otherCorner->x;
	}
	else
	{
		m_extent.hi.x = corner->x;
		m_extent.lo.x = otherCorner->x;
	}

	if ( corner->y < otherCorner->y )
	{
		m_extent.lo.y = corner->y;
		m_extent.hi.y = otherCorner->y;
	}
	else
	{
		m_extent.hi.y = corner->y;
		m_extent.lo.y = otherCorner->y;
	}

	m_extent.lo.z = corner->z;
	m_extent.hi.z = corner->z;

	m_center.x = ( m_extent.lo.x + m_extent.hi.x ) / 2.0f;
	m_center.y = ( m_extent.lo.y + m_extent.hi.y ) / 2.0f;
	m_center.z = ( m_extent.lo.z + m_extent.hi.z ) / 2.0f;

	m_neZ = corner->z;
	m_swZ = otherCorner->z;
}

//--------------------------------------------------------------------------------------------------------------
/**
 *
 */
CNavArea::CNavArea( const Vector *nwCorner, const Vector *neCorner, const Vector *seCorner, const Vector *swCorner )
{
	Initialize();

	m_extent.lo = *nwCorner;
	m_extent.hi = *seCorner;

	m_center.x = ( m_extent.lo.x + m_extent.hi.x ) / 2.0f;
	m_center.y = ( m_extent.lo.y + m_extent.hi.y ) / 2.0f;
	m_center.z = ( m_extent.lo.z + m_extent.hi.z ) / 2.0f;

	m_neZ = neCorner->z;
	m_swZ = swCorner->z;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor used during generation phase.
 */
CNavArea::CNavArea( CNavNode *nwNode, CNavNode *neNode, CNavNode *seNode, CNavNode *swNode )
{
	Initialize();

	m_extent.lo = *nwNode->GetPosition();
	m_extent.hi = *seNode->GetPosition();

	m_center.x = ( m_extent.lo.x + m_extent.hi.x ) / 2.0f;
	m_center.y = ( m_extent.lo.y + m_extent.hi.y ) / 2.0f;
	m_center.z = ( m_extent.lo.z + m_extent.hi.z ) / 2.0f;

	m_neZ = neNode->GetPosition()->z;
	m_swZ = swNode->GetPosition()->z;

	m_node[NORTH_WEST] = nwNode;
	m_node[NORTH_EAST] = neNode;
	m_node[SOUTH_EAST] = seNode;
	m_node[SOUTH_WEST] = swNode;

	// mark internal nodes as part of this area
	AssignNodes( this );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Destructor
 */
CNavArea::~CNavArea()
{
	// if we are resetting the system, don't bother cleaning up - all areas are being destroyed
	if ( m_isReset )
		return;

	// tell the other areas we are going away
	NavAreaList::iterator iter;
	for ( iter = TheNavAreaList.begin(); iter != TheNavAreaList.end(); ++iter )
	{
		CNavArea *area = *iter;

		if ( area == this )
			continue;

		area->OnDestroyNotify( this );
	}

	// unhook from ladders
	for ( int i = 0; i < NUM_LADDER_DIRECTIONS; ++i )
	{
		for ( NavLadderList::iterator liter = m_ladder[i].begin(); liter != m_ladder[i].end(); ++liter )
		{
			CNavLadder *ladder = *liter;

			ladder->OnDestroyNotify( this );
		}
	}

	// remove the area from the grid
	TheNavAreaGrid.RemoveNavArea( this );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * This is invoked when an area is going away.
 * Remove any references we have to it.
 */
void CNavArea::OnDestroyNotify( CNavArea *dead )
{
	NavConnect con;
	con.area = dead;
	for ( int d = 0; d < NUM_DIRECTIONS; ++d )
		m_connect[d].remove( con );

	m_overlapList.remove( dead );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Connect this area to given area in given direction
 */

void DestroyLadders( void )
{
	while ( !TheNavLadderList.empty() )
	{
		CNavLadder *ladder = TheNavLadderList.front();
		TheNavLadderList.pop_front();
		delete ladder;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Free navigation map data.
 */
void DestroyNavigationMap( void )
{
	CNavArea::m_isReset = true;

	// remove each element of the list and delete them
	while ( !TheNavAreaList.empty() )
	{
		CNavArea *area = TheNavAreaList.front();
		TheNavAreaList.pop_front();
		delete area;
	}

	CNavArea::m_isReset = false;

	// destroy ladder representations
	DestroyLadders();

	// destroy all hiding spots
	DestroyHidingSpots();

	// destroy navigation nodes created during map learning
	CNavNode *node, *next;
	for ( node = CNavNode::m_list; node; node = next )
	{
		next = node->m_next;
		delete node;
	}
	CNavNode::m_list = NULL;

	// reset the grid
	TheNavAreaGrid.Reset();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Strip the "analyzed" data out of all navigation areas
 */
void StripNavigationAreas( void )
{
	NavAreaList::iterator iter;
	for ( iter = TheNavAreaList.begin(); iter != TheNavAreaList.end(); ++iter )
	{
		CNavArea *area = *iter;

		area->Strip();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Remove "analyzed" data from nav area
 */
void CNavArea::Strip( void )
{
	m_approachCount = 0;
	m_spotEncounterList.clear(); // memory leak
}

//--------------------------------------------------------------------------------------------------------------

void MarkJumpAreas( void )
{
	for ( NavAreaList::iterator iter = TheNavAreaList.begin(); iter != TheNavAreaList.end(); ++iter )
	{
		CNavArea *area = *iter;
		Vector u, v;

		// compute our unit surface normal
		u.x = area->m_extent.hi.x - area->m_extent.lo.x;
		u.y = 0.0f;
		u.z = area->m_neZ - area->m_extent.lo.z;

		v.x = 0.0f;
		v.y = area->m_extent.hi.y - area->m_extent.lo.y;
		v.z = area->m_swZ - area->m_extent.lo.z;

		Vector normal = CrossProduct( u, v );
		normal.NormalizeInPlace();

		if ( normal.z < MaxUnitZSlope )
			area->SetAttributes( area->GetAttributes() | NAV_JUMP );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * This function uses the CNavNodes that have been sampled from the map to
 * generate CNavAreas - rectangular areas of "walkable" space. These areas
 * are connected to each other, allowing the AI to know how to move from
 * area to area.
 *
 * This is a "greedy" algorithm that attempts to cover the walkable area
 * with the fewest, largest, rectangles.
 */
void GenerateNavigationAreaMesh( void )
{
	// haven't yet seen a map use larger than 30...
	int tryWidth       = 50;
	int tryHeight      = 50;
	int uncoveredNodes = CNavNode::GetListLength();

	while ( uncoveredNodes > 0 )
	{
		for ( CNavNode *node = CNavNode::GetFirst(); node; node = node->GetNext() )
		{
			if ( node->IsCovered() )
				continue;

			if ( TestArea( node, tryWidth, tryHeight ) )
			{
				int covered = BuildArea( node, tryWidth, tryHeight );
				if ( covered < 0 )
				{
					CONSOLE_ECHO( "GenerateNavigationAreaMesh: Error - Data corrupt.\n" );
					return;
				}

				uncoveredNodes -= covered;
			}
		}

		if ( tryWidth >= tryHeight )
			--tryWidth;
		else
			--tryHeight;

		if ( tryWidth <= 0 || tryHeight <= 0 )
			break;
	}

	Extent extent;
	extent.lo.x = 9999999999.9f;
	extent.lo.y = 9999999999.9f;
	extent.hi.x = -9999999999.9f;
	extent.hi.y = -9999999999.9f;

	// compute total extent
	NavAreaList::iterator iter;
	for ( iter = TheNavAreaList.begin(); iter != TheNavAreaList.end(); ++iter )
	{
		CNavArea *area           = *iter;
		const Extent *areaExtent = area->GetExtent();

		if ( areaExtent->lo.x < extent.lo.x )
			extent.lo.x = areaExtent->lo.x;
		if ( areaExtent->lo.y < extent.lo.y )
			extent.lo.y = areaExtent->lo.y;
		if ( areaExtent->hi.x > extent.hi.x )
			extent.hi.x = areaExtent->hi.x;
		if ( areaExtent->hi.y > extent.hi.y )
			extent.hi.y = areaExtent->hi.y;
	}

	// add the areas to the grid
	TheNavAreaGrid.Initialize( extent.lo.x, extent.hi.x, extent.lo.y, extent.hi.y );

	for ( iter = TheNavAreaList.begin(); iter != TheNavAreaList.end(); ++iter )
		TheNavAreaGrid.AddNavArea( *iter );

	ConnectGeneratedAreas();
	MergeGeneratedAreas();
	SquareUpAreas();
	MarkJumpAreas();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if 'pos' is within 2D extents of area.
 */

return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return direction from this area to the given point
 */
NavDirType CNavArea::ComputeDirection( Vector *point ) const
{
	if ( point->x >= m_extent.lo.x && point->x <= m_extent.hi.x )
	{
		if ( point->y < m_extent.lo.y )
			return NORTH;
		else if ( point->y > m_extent.hi.y )
			return SOUTH;
	}
	else if ( point->y >= m_extent.lo.y && point->y <= m_extent.hi.y )
	{
		if ( point->x < m_extent.lo.x )
			return WEST;
		else if ( point->x > m_extent.hi.x )
			return EAST;
	}

	// find closest direction
	Vector to = *point - m_center;

	if ( abs( to.x ) > abs( to.y ) )
	{
		if ( to.x > 0.0f )
			return EAST;
		return WEST;
	}
	else
	{
		if ( to.y > 0.0f )
			return SOUTH;
		return NORTH;
	}

	return NUM_DIRECTIONS;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw area for debugging
 */
void CNavArea::Draw( byte red, byte green, byte blue, int duration )
{
	Vector nw, ne, sw, se;

	nw   = m_extent.lo;
	se   = m_extent.hi;
	ne.x = se.x;
	ne.y = nw.y;
	ne.z = m_neZ;
	sw.x = nw.x;
	sw.y = se.y;
	sw.z = m_swZ;

	nw.z += cv_bot_nav_zdraw.value;
	ne.z += cv_bot_nav_zdraw.value;
	sw.z += cv_bot_nav_zdraw.value;
	se.z += cv_bot_nav_zdraw.value;

	float border = 2.0f;
	nw.x += border;
	nw.y += border;
	ne.x -= border;
	ne.y += border;
	sw.x += border;
	sw.y -= border;
	se.x -= border;
	se.y -= border;

	UTIL_DrawBeamPoints( nw, ne, duration, red, green, blue );
	UTIL_DrawBeamPoints( ne, se, duration, red, green, blue );
	UTIL_DrawBeamPoints( se, sw, duration, red, green, blue );
	UTIL_DrawBeamPoints( sw, nw, duration, red, green, blue );

	if ( GetAttributes() & NAV_CROUCH )
		UTIL_DrawBeamPoints( nw, se, duration, red, green, blue );

	if ( GetAttributes() & NAV_JUMP )
	{
		UTIL_DrawBeamPoints( nw, se, duration, red, green, blue );
		UTIL_DrawBeamPoints( ne, sw, duration, red, green, blue );
	}

	if ( GetAttributes() & NAV_PRECISE )
	{
		float size = 8.0f;
		Vector up( m_center.x, m_center.y - size, m_center.z + cv_bot_nav_zdraw.value );
		Vector down( m_center.x, m_center.y + size, m_center.z + cv_bot_nav_zdraw.value );
		UTIL_DrawBeamPoints( up, down, duration, red, green, blue );

		Vector left( m_center.x - size, m_center.y, m_center.z + cv_bot_nav_zdraw.value );
		Vector right( m_center.x + size, m_center.y, m_center.z + cv_bot_nav_zdraw.value );
		UTIL_DrawBeamPoints( left, right, duration, red, green, blue );
	}

	if ( GetAttributes() & NAV_NO_JUMP )
	{
		float size = 8.0f;
		Vector up( m_center.x, m_center.y - size, m_center.z + cv_bot_nav_zdraw.value );
		Vector down( m_center.x, m_center.y + size, m_center.z + cv_bot_nav_zdraw.value );
		Vector left( m_center.x - size, m_center.y, m_center.z + cv_bot_nav_zdraw.value );
		Vector right( m_center.x + size, m_center.y, m_center.z + cv_bot_nav_zdraw.value );
		UTIL_DrawBeamPoints( up, right, duration, red, green, blue );
		UTIL_DrawBeamPoints( right, down, duration, red, green, blue );
		UTIL_DrawBeamPoints( down, left, duration, red, green, blue );
		UTIL_DrawBeamPoints( left, up, duration, red, green, blue );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw selected corner for debugging
 */
void CNavArea::DrawMarkedCorner( NavCornerType corner, byte red, byte green, byte blue, int duration )
{
	Vector nw, ne, sw, se;

	nw   = m_extent.lo;
	se   = m_extent.hi;
	ne.x = se.x;
	ne.y = nw.y;
	ne.z = m_neZ;
	sw.x = nw.x;
	sw.y = se.y;
	sw.z = m_swZ;

	nw.z += cv_bot_nav_zdraw.value;
	ne.z += cv_bot_nav_zdraw.value;
	sw.z += cv_bot_nav_zdraw.value;
	se.z += cv_bot_nav_zdraw.value;

	float border = 2.0f;
	nw.x += border;
	nw.y += border;
	ne.x -= border;
	ne.y += border;
	sw.x += border;
	sw.y -= border;
	se.x -= border;
	se.y -= border;

	switch ( corner )
	{
	case NORTH_WEST:
		UTIL_DrawBeamPoints( nw + Vector( 0, 0, 10 ), nw, duration, red, green, blue );
		break;
	case NORTH_EAST:
		UTIL_DrawBeamPoints( ne + Vector( 0, 0, 10 ), ne, duration, red, green, blue );
		break;
	case SOUTH_EAST:
		UTIL_DrawBeamPoints( se + Vector( 0, 0, 10 ), se, duration, red, green, blue );
		break;
	case SOUTH_WEST:
		UTIL_DrawBeamPoints( sw + Vector( 0, 0, 10 ), sw, duration, red, green, blue );
		break;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Add to open list in decreasing value order
 */
void CNavArea::AddToOpenList( void )
{
	// mark as being on open list for quick check
	m_openMarker = m_masterMarker;

	// if list is empty, add and return
	if ( m_openList == NULL )
	{
		m_openList       = this;
		this->m_prevOpen = NULL;
		this->m_nextOpen = NULL;
		return;
	}

	// insert self in ascending cost order
	CNavArea *area, *last = NULL;
	for ( area = m_openList; area; area = area->m_nextOpen )
	{
		if ( this->GetTotalCost() < area->GetTotalCost() )
			break;

		last = area;
	}

	if ( area )
	{
		// insert before this area
		this->m_prevOpen = area->m_prevOpen;
		if ( this->m_prevOpen )
			this->m_prevOpen->m_nextOpen = this;
		else
			m_openList = this;

		this->m_nextOpen = area;
		area->m_prevOpen = this;
	}
	else
	{
		// append to end of list
		last->m_nextOpen = this;

		this->m_prevOpen = last;
		this->m_nextOpen = NULL;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * A smaller value has been found, update this area on the open list
 * Priority queue position update for modified node
 */
void CNavArea::UpdateOnOpenList( void )
{
	// since value can only decrease, bubble this area up from current spot
	while ( m_prevOpen &&
	        this->GetTotalCost() < m_prevOpen->GetTotalCost() )
	{
		// swap position with predecessor
		CNavArea *other  = m_prevOpen;
		CNavArea *before = other->m_prevOpen;
		CNavArea *after  = this->m_nextOpen;

		this->m_nextOpen = other;
		this->m_prevOpen = before;

		other->m_prevOpen = this;
		other->m_nextOpen = after;

		if ( before )
			before->m_nextOpen = this;
		else
			m_openList = this;

		if ( after )
			after->m_prevOpen = other;
	}
}

//--------------------------------------------------------------------------------------------------------------
void CNavArea::RemoveFromOpenList( void )
{
	if ( m_prevOpen )
		m_prevOpen->m_nextOpen = m_nextOpen;
	else
		m_openList = m_nextOpen;

	if ( m_nextOpen )
		m_nextOpen->m_prevOpen = m_prevOpen;

	// zero is an invalid marker
	m_openMarker = 0;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Clears the open and closed lists for a new search
 */
void CNavArea::ClearSearchLists( void )
{
	// effectively clears all open list pointers and closed flags
	CNavArea::MakeNewMarker();

	m_openList = NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the coordinates of the area's corner.
 * NOTE: Do not retain the returned pointer - it is temporary.
 */
const Vector *CNavArea::GetCorner( NavCornerType corner ) const
{
	static Vector pos;

	switch ( corner )
	{
	case NORTH_WEST:
		return &m_extent.lo;

	case NORTH_EAST:
		pos.x = m_extent.hi.x;
		pos.y = m_extent.lo.y;
		pos.z = m_neZ;
		return &pos;

	case SOUTH_WEST:
		pos.x = m_extent.lo.x;
		pos.y = m_extent.hi.y;
		pos.z = m_swZ;
		return &pos;

	case SOUTH_EAST:
		return &m_extent.hi;
	}

	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if an existing hiding spot is too close to given position
 */

int CNavArea::GetPlayerCount( int teamID, CBasePlayer *ignore ) const
{
	int count = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast< CBasePlayer * >( UTIL_PlayerByIndex( i ) );

		if ( player == ignore )
			continue;

		if ( !IsEntityValid( player ) )
			continue;

		if ( !player->IsPlayer() )
			continue;

		if ( !player->IsAlive() )
			continue;

		if ( teamID == 0 || player->m_iTeam == teamID )
			if ( Contains( &player->pev->origin ) )
				++count;
	}

	return count;
}

//--------------------------------------------------------------------------------------------------------------
static CNavArea *markedArea       = NULL;
static CNavArea *lastSelectedArea = NULL;
static NavCornerType markedCorner = NUM_CORNERS;

static bool isCreatingNavArea = false; ///< if true, we are manually creating a new nav area
static bool isAnchored        = false;
static Vector anchor;

static bool isPlaceMode     = false; ///< if true, we are in place editing mode
static bool isPlacePainting = false; ///< if true, we set an area's place by pointing at it

static float editTimestamp = 0.0f;

CNavArea *GetMarkedArea( void )
{
	return markedArea;
}

/**
 * Draw navigation areas and edit them
 */
void EditNavAreasReset( void )
{
	markedArea        = NULL;
	lastSelectedArea  = NULL;
	isCreatingNavArea = false;
	editTimestamp     = 0.0f;
	isPlacePainting   = false;
	lastDrawTimestamp = 0.0f;
}

void DrawHidingSpots( const CNavArea *area )
{
	const HidingSpotList *list = area->GetHidingSpotList();
	for ( HidingSpotList::const_iterator iter = list->begin(); iter != list->end(); ++iter )
	{
		const HidingSpot *spot = *iter;

		int r, g, b;

		if ( spot->IsIdealSniperSpot() )
		{
			r = 255;
			g = 0;
			b = 0;
		}
		else if ( spot->IsGoodSniperSpot() )
		{
			r = 255;
			g = 0;
			b = 255;
		}
		else if ( spot->HasGoodCover() )
		{
			r = 0;
			g = 255;
			b = 0;
		}
		else
		{
			r = 0;
			g = 0;
			b = 1;
		}

		UTIL_DrawBeamPoints( *spot->GetPosition(), *spot->GetPosition() + Vector( 0, 0, 50 ), 3, r, g, b );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw ourselves and adjacent areas
 */
void CNavArea::DrawConnectedAreas( void )
{
	CBasePlayer *player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;

	CCSBotManager *ctrl  = static_cast< CCSBotManager  *>( TheBots );
	const float maxRange = 500.0f;

	// draw self
	if ( isPlaceMode )
	{
		if ( GetPlace() == 0 )
			Draw( 50, 0, 0, 3 );
		else if ( GetPlace() != ctrl->GetNavPlace() )
			Draw( 0, 0, 200, 3 );
		else
			Draw( 0, 255, 0, 3 );
	}
	else
	{
		Draw( 255, 255, 0, 3 );
		DrawHidingSpots( this );
	}

	// randomize order of directions to make sure all connected areas are
	// drawn, since we may have too many to render all at once
	int dirSet[NUM_DIRECTIONS];
	int i;
	for ( i = 0; i < NUM_DIRECTIONS; ++i )
		dirSet[i] = i;

	// shuffle dirSet[]
	for ( int swapCount = 0; swapCount < 3; ++swapCount )
	{
		int swapI = RANDOM_LONG( 0, NUM_DIRECTIONS - 1 );
		int nextI = swapI + 1;
		if ( nextI >= NUM_DIRECTIONS )
			nextI = 0;

		int tmp       = dirSet[nextI];
		dirSet[nextI] = dirSet[swapI];
		dirSet[swapI] = tmp;
	}

	// draw connected areas
	for ( i = 0; i < NUM_DIRECTIONS; ++i )
	{
		NavDirType dir = (NavDirType)dirSet[i];

		int count = GetAdjacentCount( dir );

		for ( int a = 0; a < count; ++a )
		{
			CNavArea *adj = GetAdjacentArea( dir, a );

			if ( isPlaceMode )
			{
				if ( adj->GetPlace() == 0 )
					adj->Draw( 50, 0, 0, 3 );
				else if ( adj->GetPlace() != ctrl->GetNavPlace() )
					adj->Draw( 0, 0, 200, 3 );
				else
					adj->Draw( 0, 255, 0, 3 );
			}
			else
			{
				if ( adj->IsDegenerate() )
				{
					static IntervalTimer blink;
					static bool blinkOn = false;

					if ( blink.GetElapsedTime() > 1.0f )
					{
						blink.Reset();
						blinkOn = !blinkOn;
					}

					if ( blinkOn )
						adj->Draw( 255, 255, 255, 3 );
					else
						adj->Draw( 255, 0, 255, 3 );
				}
				else
				{
					adj->Draw( 255, 0, 0, 3 );
				}

				DrawHidingSpots( adj );

				Vector from, to;
				Vector hookPos;
				float halfWidth;
				float size = 5.0f;
				ComputePortal( adj, dir, &hookPos, &halfWidth );

				switch ( dir )
				{
				case NORTH:
					from = hookPos + Vector( 0.0f, size, 0.0f );
					to   = hookPos + Vector( 0.0f, -size, 0.0f );
					break;
				case SOUTH:
					from = hookPos + Vector( 0.0f, -size, 0.0f );
					to   = hookPos + Vector( 0.0f, size, 0.0f );
					break;
				case EAST:
					from = hookPos + Vector( -size, 0.0f, 0.0f );
					to   = hookPos + Vector( +size, 0.0f, 0.0f );
					break;
				case WEST:
					from = hookPos + Vector( size, 0.0f, 0.0f );
					to   = hookPos + Vector( -size, 0.0f, 0.0f );
					break;
				}

				from.z = GetZ( &from ) + cv_bot_nav_zdraw.value;
				to.z   = adj->GetZ( &to ) + cv_bot_nav_zdraw.value;

				Vector drawTo;
				adj->GetClosestPointOnArea( &to, &drawTo );

				if ( adj->IsConnected( this, OppositeDirection( dir ) ) )
					UTIL_DrawBeamPoints( from, drawTo, 3, 0, 255, 255 );
				else
					UTIL_DrawBeamPoints( from, drawTo, 3, 0, 0, 255 );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Raise/lower a corner
 */

* /
    class PlaceFloodFillFunctor
{
  public:
	PlaceFloodFillFunctor( CNavArea *area )
	{
		m_initialPlace = area->GetPlace();
	}

	bool operator()( CNavArea *area )
	{
		CCSBotManager *ctrl = static_cast< CCSBotManager * >( TheBots );

		if ( area->GetPlace() != m_initialPlace )
			return false;

		area->SetPlace( ctrl->GetNavPlace() );

		return true;
	}

  private:
	unsigned int m_initialPlace;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Draw navigation areas and edit them
 */
void EditNavAreas( NavEditCmdType cmd )
{
	CCSBotManager *ctrl = static_cast< CCSBotManager * >( TheBots );

	CBasePlayer *player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;

	// don't draw too often on fast video cards or the areas may not appear (odd video effect)
	float drawTimestamp     = gpGlobals->time;
	const float maxDrawRate = 0.05f;

	bool doDraw;
	if ( drawTimestamp - lastDrawTimestamp < maxDrawRate )
	{
		doDraw = false;
	}
	else
	{
		doDraw            = true;
		lastDrawTimestamp = drawTimestamp;
	}

	const float maxRange = 1000.0f; // 500

	int beamTime = 1;

	if ( doDraw )
	{
		// show ladder connections
		for ( NavLadderList::iterator iter = TheNavLadderList.begin(); iter != TheNavLadderList.end(); ++iter )
		{
			CNavLadder *ladder = *iter;

			float dx = player->pev->origin.x - ladder->m_bottom.x;
			float dy = player->pev->origin.y - ladder->m_bottom.y;
			if ( dx * dx + dy * dy > maxRange * maxRange )
				continue;

			UTIL_DrawBeamPoints( ladder->m_top, ladder->m_bottom, beamTime, 255, 0, 255 );

			Vector bottom = ladder->m_bottom;
			Vector top    = ladder->m_top;

			AddDirectionVector( &top, ladder->m_dir, HalfHumanWidth );
			AddDirectionVector( &bottom, ladder->m_dir, HalfHumanWidth );

			UTIL_DrawBeamPoints( top, bottom, beamTime, 0, 0, 255 );

			if ( ladder->m_bottomArea )
				UTIL_DrawBeamPoints( bottom + Vector( 0, 0, GenerationStepSize ), *ladder->m_bottomArea->GetCenter(), beamTime, 0, 0, 255 );

			if ( ladder->m_topForwardArea )
				UTIL_DrawBeamPoints( top, *ladder->m_topForwardArea->GetCenter(), beamTime, 0, 0, 255 );

			if ( ladder->m_topLeftArea )
				UTIL_DrawBeamPoints( top, *ladder->m_topLeftArea->GetCenter(), beamTime, 0, 0, 255 );

			if ( ladder->m_topRightArea )
				UTIL_DrawBeamPoints( top, *ladder->m_topRightArea->GetCenter(), beamTime, 0, 0, 255 );

			if ( ladder->m_topBehindArea )
				UTIL_DrawBeamPoints( top, *ladder->m_topBehindArea->GetCenter(), beamTime, 0, 0, 255 );
		}

		// draw approach points for marked area
		if ( cv_bot_traceview.value == 3 && markedArea )
		{
			Vector ap;
			float halfWidth;
			for ( int i = 0; i < markedArea->GetApproachInfoCount(); ++i )
			{
				const CNavArea::ApproachInfo *info = markedArea->GetApproachInfo( i );

				// compute approach point
				if ( info->hereToNextHow <= GO_WEST )
				{
					info->here.area->ComputePortal( info->next.area, (NavDirType)info->hereToNextHow, &ap, &halfWidth );
					ap.z = info->next.area->GetZ( &ap );
				}
				else
				{
					// use the area's center as an approach point
					ap = *info->here.area->GetCenter();
				}

				UTIL_DrawBeamPoints( ap + Vector( 0, 0, 50 ), ap + Vector( 10, 0, 0 ), beamTime, 255, 100, 0 );
				UTIL_DrawBeamPoints( ap + Vector( 0, 0, 50 ), ap + Vector( -10, 0, 0 ), beamTime, 255, 100, 0 );
				UTIL_DrawBeamPoints( ap + Vector( 0, 0, 50 ), ap + Vector( 0, 10, 0 ), beamTime, 255, 100, 0 );
				UTIL_DrawBeamPoints( ap + Vector( 0, 0, 50 ), ap + Vector( 0, -10, 0 ), beamTime, 255, 100, 0 );
			}
		}
	}

	Vector dir;
	UTIL_MakeVectorsPrivate( player->pev->v_angle, dir, NULL, NULL );

	Vector from = player->pev->origin + player->pev->view_ofs; // eye position
	Vector to   = from + maxRange * dir;

	TraceResult result;
	UTIL_TraceLine( from, to, ignore_monsters, ignore_glass, ENT( player->pev ), &result );

	if ( result.flFraction != 1.0f )
	{
		// draw cursor
		Vector cursor    = result.vecEndPos;
		float cursorSize = 10.0f;

		if ( doDraw )
		{
			UTIL_DrawBeamPoints( cursor + Vector( 0, 0, cursorSize ), cursor, beamTime, 255, 255, 255 );
			UTIL_DrawBeamPoints( cursor + Vector( cursorSize, 0, 0 ), cursor + Vector( -cursorSize, 0, 0 ), beamTime, 255, 255, 255 );
			UTIL_DrawBeamPoints( cursor + Vector( 0, cursorSize, 0 ), cursor + Vector( 0, -cursorSize, 0 ), beamTime, 255, 255, 255 );

			// show surface normal
			// UTIL_DrawBeamPoints( cursor + 50.0f * result.vecPlaneNormal, cursor, beamTime, 255, 0, 255 );
		}

		if ( isCreatingNavArea )
		{
			if ( isAnchored )
			{
				// show drag rectangle
				if ( doDraw )
				{
					float z = anchor.z + 2.0f;
					UTIL_DrawBeamPoints( Vector( cursor.x, cursor.y, z ), Vector( anchor.x, cursor.y, z ), beamTime, 0, 255, 255 );
					UTIL_DrawBeamPoints( Vector( anchor.x, anchor.y, z ), Vector( anchor.x, cursor.y, z ), beamTime, 0, 255, 255 );
					UTIL_DrawBeamPoints( Vector( anchor.x, anchor.y, z ), Vector( cursor.x, anchor.y, z ), beamTime, 0, 255, 255 );
					UTIL_DrawBeamPoints( Vector( cursor.x, cursor.y, z ), Vector( cursor.x, anchor.y, z ), beamTime, 0, 255, 255 );
				}
			}
			else
			{
				// anchor starting corner
				anchor     = cursor;
				isAnchored = true;
			}
		}

		// find the area the player is pointing at
		CNavArea *area = TheNavAreaGrid.GetNearestNavArea( &result.vecEndPos );

		if ( area )
		{
			// if area changed, print its ID
			if ( area != lastSelectedArea )
			{
				lastSelectedArea = area;

				char buffer[80];
				char attrib[80];
				char locName[80];

				if ( area->GetPlace() )
				{
					const char *name = TheBotPhrases->IDToName( area->GetPlace() );
					if ( name )
					{
						strncpy( locName, name, sizeof( locName ) - 1 );
						locName[sizeof( locName ) - 1] = '\0';
					}
					else
					{
						strncpy( locName, "ERROR", sizeof( locName ) - 1 );
						locName[sizeof( locName ) - 1] = '\0';
					}
				}
				else
				{
					locName[0] = '\000';
				}

				if ( isPlaceMode )
				{
					attrib[0] = '\000';
				}
				else
				{
					snprintf( attrib, sizeof( attrib ), "%s%s%s%s", ( area->GetAttributes() & NAV_CROUCH ) ? "CROUCH " : "", ( area->GetAttributes() & NAV_JUMP ) ? "JUMP " : "", ( area->GetAttributes() & NAV_PRECISE ) ? "PRECISE " : "", ( area->GetAttributes() & NAV_NO_JUMP ) ? "NO_JUMP " : "" );
				}

				snprintf( buffer, sizeof( buffer ), "Area #%d %s %s\n", area->GetID(), locName, attrib );

				UTIL_SayTextAll( buffer, player );

				// do "place painting"
				if ( isPlacePainting )
				{
					if ( area->GetPlace() != ctrl->GetNavPlace() )
					{
						area->SetPlace( ctrl->GetNavPlace() );
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/lightswitch2.wav", 1, ATTN_NORM, 0, 100 );
					}
				}
			}

			if ( isPlaceMode )
			{
				area->DrawConnectedAreas();

				switch ( cmd )
				{
				case EDIT_TOGGLE_PLACE_MODE:
					EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
					isPlaceMode = false;
					return;

				case EDIT_TOGGLE_PLACE_PAINTING:
				{
					if ( isPlacePainting )
					{
						isPlacePainting = false;
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/latchunlocked2.wav", 1, ATTN_NORM, 0, 100 );
					}
					else
					{
						isPlacePainting = true;

						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/lightswitch2.wav", 1, ATTN_NORM, 0, 100 );

						// paint the initial area
						area->SetPlace( ctrl->GetNavPlace() );
					}
					break;
				}

				case EDIT_PLACE_PICK:
					EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
					ctrl->SetNavPlace( area->GetPlace() );
					break;

				case EDIT_PLACE_FLOODFILL:
					PlaceFloodFillFunctor pff( area );
					SearchSurroundingAreas( area, area->GetCenter(), pff );
					break;
				}
			}
			else // normal editing mode
			{
				// draw the "marked" area
				if ( markedArea && doDraw )
				{
					markedArea->Draw( 0, 255, 255, beamTime );
					if ( markedCorner != NUM_CORNERS )
						markedArea->DrawMarkedCorner( markedCorner, 0, 0, 255, beamTime );

					if ( cv_bot_traceview.value == 11 )
					{
						// draw areas connected to the marked area
						markedArea->DrawConnectedAreas();
					}
				}

				// draw split line
				const Extent *extent = area->GetExtent();

				float yaw = player->pev->v_angle.y;
				while ( yaw > 360.0f )
					yaw -= 360.0f;

				while ( yaw < 0.0f )
					yaw += 360.0f;

				float splitEdge;
				bool splitAlongX;

				if ( ( yaw < 45.0f || yaw > 315.0f ) || ( yaw > 135.0f && yaw < 225.0f ) )
				{
					splitEdge = GenerationStepSize * (int)( result.vecEndPos.y / GenerationStepSize );

					from.x = extent->lo.x;
					from.y = splitEdge;
					from.z = area->GetZ( &from ) + cv_bot_nav_zdraw.value;

					to.x = extent->hi.x;
					to.y = splitEdge;
					to.z = area->GetZ( &to ) + cv_bot_nav_zdraw.value;

					splitAlongX = true;
				}
				else
				{
					splitEdge = GenerationStepSize * (int)( result.vecEndPos.x / GenerationStepSize );

					from.x = splitEdge;
					from.y = extent->lo.y;
					from.z = area->GetZ( &from ) + cv_bot_nav_zdraw.value;

					to.x = splitEdge;
					to.y = extent->hi.y;
					to.z = area->GetZ( &to ) + cv_bot_nav_zdraw.value;

					splitAlongX = false;
				}

				if ( doDraw )
					UTIL_DrawBeamPoints( from, to, beamTime, 255, 255, 255 );

				// draw the area we are pointing at and all connected areas
				if ( doDraw && ( cv_bot_traceview.value != 11 || markedArea == NULL ) )
					area->DrawConnectedAreas();

				// do area-dependant edit commands, if any
				switch ( cmd )
				{
				case EDIT_TOGGLE_PLACE_MODE:
					EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
					isPlaceMode = true;
					return;

				case EDIT_DELETE:
					EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
					TheNavAreaList.remove( area );
					delete area;
					return;

				case EDIT_ATTRIB_CROUCH:
					EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/bell1.wav", 1, ATTN_NORM, 0, 100 );
					area->SetAttributes( area->GetAttributes() ^ NAV_CROUCH );
					break;

				case EDIT_ATTRIB_JUMP:
					EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/bell1.wav", 1, ATTN_NORM, 0, 100 );
					area->SetAttributes( area->GetAttributes() ^ NAV_JUMP );
					break;

				case EDIT_ATTRIB_PRECISE:
					EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/bell1.wav", 1, ATTN_NORM, 0, 100 );
					area->SetAttributes( area->GetAttributes() ^ NAV_PRECISE );
					break;

				case EDIT_ATTRIB_NO_JUMP:
					EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/bell1.wav", 1, ATTN_NORM, 0, 100 );
					area->SetAttributes( area->GetAttributes() ^ NAV_NO_JUMP );
					break;

				case EDIT_SPLIT:
					if ( area->SplitEdit( splitAlongX, splitEdge ) )
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "weapons/knife_hitwall1.wav", 1, ATTN_NORM, 0, 100 );
					else
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					break;

				case EDIT_MERGE:
					if ( markedArea )
					{
						if ( area->MergeEdit( markedArea ) )
							EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
						else
							EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					}
					else
					{
						HintMessageToAllPlayers( "To merge, mark an area, highlight a second area, then invoke the merge command" );
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					}
					break;

				case EDIT_MARK:
					if ( markedArea )
					{
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
						markedArea = NULL;
					}
					else
					{
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip2.wav", 1, ATTN_NORM, 0, 100 );
						markedArea = area;

						int connected = 0;
						connected += markedArea->GetAdjacentCount( NORTH );
						connected += markedArea->GetAdjacentCount( SOUTH );
						connected += markedArea->GetAdjacentCount( EAST );
						connected += markedArea->GetAdjacentCount( WEST );

						char buffer[80];
						snprintf( buffer, sizeof( buffer ), "Marked Area is connected to %d other Areas\n", connected );
						UTIL_SayTextAll( buffer, player );
					}
					break;

				case EDIT_MARK_UNNAMED:
					if ( markedArea )
					{
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
						markedArea = NULL;
					}
					else
					{
						markedArea = NULL;
						for ( NavAreaList::iterator iter = TheNavAreaList.begin(); iter != TheNavAreaList.end(); ++iter )
						{
							CNavArea *area = *iter;
							if ( area->GetPlace() == 0 )
							{
								markedArea = area;
								break;
							}
						}
						if ( !markedArea )
						{
							EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
						}
						else
						{
							EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip2.wav", 1, ATTN_NORM, 0, 100 );

							int connected = 0;
							connected += markedArea->GetAdjacentCount( NORTH );
							connected += markedArea->GetAdjacentCount( SOUTH );
							connected += markedArea->GetAdjacentCount( EAST );
							connected += markedArea->GetAdjacentCount( WEST );

							int totalUnnamedAreas = 0;
							for ( NavAreaList::iterator iter = TheNavAreaList.begin(); iter != TheNavAreaList.end(); ++iter )
							{
								CNavArea *area = *iter;
								if ( area->GetPlace() == 0 )
								{
									++totalUnnamedAreas;
								}
							}

							char buffer[80];
							snprintf( buffer, sizeof( buffer ), "Marked Area is connected to %d other Areas - there are %d total unnamed areas\n", connected, totalUnnamedAreas );
							UTIL_SayTextAll( buffer, player );
						}
					}
					break;

				case EDIT_WARP_TO_MARK:
					if ( markedArea )
					{
						CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayer();
						if ( pLocalPlayer && pLocalPlayer->m_iTeam == SPECTATOR && pLocalPlayer->pev->iuser1 == OBS_ROAMING )
						{
							Vector origin = *markedArea->GetCenter() + Vector( 0, 0, 0.75f * HumanHeight );
							UTIL_SetOrigin( pLocalPlayer->pev, origin );
						}
					}
					else
					{
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					}
					break;

				case EDIT_CONNECT:
					if ( markedArea )
					{
						NavDirType dir = markedArea->ComputeDirection( &cursor );
						if ( dir == NUM_DIRECTIONS )
						{
							EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
						}
						else
						{
							markedArea->ConnectTo( area, dir );
							EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
						}
					}
					else
					{
						HintMessageToAllPlayers( "To connect areas, mark an area, highlight a second area, then invoke the connect command. Make sure the cursor is directly north, south, east, or west of the marked area." );
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					}
					break;

				case EDIT_DISCONNECT:
					if ( markedArea )
					{
						markedArea->Disconnect( area );
						area->Disconnect( markedArea );
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
					}
					else
					{
						HintMessageToAllPlayers( "To disconnect areas, mark an area, highlight a second area, then invoke the disconnect command. This will remove all connections between the two areas." );
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					}
					break;

				case EDIT_SPLICE:
					if ( markedArea )
					{
						if ( area->SpliceEdit( markedArea ) )
							EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
						else
							EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					}
					else
					{
						HintMessageToAllPlayers( "To splice, mark an area, highlight a second area, then invoke the splice command to create an area between them" );
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					}
					break;

				case EDIT_SELECT_CORNER:
					if ( markedArea )
					{
						int corner   = ( markedCorner + 1 ) % ( NUM_CORNERS + 1 );
						markedCorner = (NavCornerType)corner;
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
					}
					else
					{
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					}
					break;

				case EDIT_RAISE_CORNER:
					if ( markedArea )
					{
						markedArea->RaiseCorner( markedCorner, 1 );
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
					}
					else
					{
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					}
					break;

				case EDIT_LOWER_CORNER:
					if ( markedArea )
					{
						markedArea->RaiseCorner( markedCorner, -1 );
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );
					}
					else
					{
						EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
					}
					break;
				}
			}
		}

		// do area-independant edit commands, if any
		switch ( cmd )
		{
		case EDIT_BEGIN_AREA:
		{
			if ( isCreatingNavArea )
			{
				isCreatingNavArea = false;
				EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
			}
			else
			{
				EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip2.wav", 1, ATTN_NORM, 0, 100 );
				isCreatingNavArea = true;
				isAnchored        = false;
			}
			break;
		}

		case EDIT_END_AREA:
		{
			if ( isCreatingNavArea )
			{
				// create the new nav area
				CNavArea *newArea = new CNavArea( &anchor, &cursor );
				TheNavAreaList.push_back( newArea );
				TheNavAreaGrid.AddNavArea( newArea );
				EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM, 0, 100 );

				// if we have a marked area, inter-connect the two
				if ( markedArea )
				{
					const Extent *extent = markedArea->GetExtent();

					if ( anchor.x > extent->hi.x && cursor.x > extent->hi.x )
					{
						markedArea->ConnectTo( newArea, EAST );
						newArea->ConnectTo( markedArea, WEST );
					}
					else if ( anchor.x < extent->lo.x && cursor.x < extent->lo.x )
					{
						markedArea->ConnectTo( newArea, WEST );
						newArea->ConnectTo( markedArea, EAST );
					}
					else if ( anchor.y > extent->hi.y && cursor.y > extent->hi.y )
					{
						markedArea->ConnectTo( newArea, SOUTH );
						newArea->ConnectTo( markedArea, NORTH );
					}
					else if ( anchor.y < extent->lo.y && cursor.y < extent->lo.y )
					{
						markedArea->ConnectTo( newArea, NORTH );
						newArea->ConnectTo( markedArea, SOUTH );
					}

					// propogate marked area to new area
					markedArea = newArea;
				}

				isCreatingNavArea = false;
			}
			else
			{
				EMIT_SOUND_DYN( ENT( UTIL_GetLocalPlayer()->pev ), CHAN_ITEM, "buttons/button11.wav", 1, ATTN_NORM, 0, 100 );
			}
			break;
		}
		}
	}

	// if our last command was not mark (or no command), clear the mark area
	if ( cmd != EDIT_MARK && cmd != EDIT_BEGIN_AREA && cmd != EDIT_END_AREA &&
	     cmd != EDIT_MARK_UNNAMED && cmd != EDIT_WARP_TO_MARK &&
	     cmd != EDIT_SELECT_CORNER && cmd != EDIT_RAISE_CORNER && cmd != EDIT_LOWER_CORNER &&
	     cmd != EDIT_NONE )
		markedArea = NULL;

	// if our last command was not affecting the corner, clear the corner selection
	if ( cmd != EDIT_SELECT_CORNER && cmd != EDIT_RAISE_CORNER && cmd != EDIT_LOWER_CORNER && cmd != EDIT_NONE )
		markedCorner = NUM_CORNERS;

	if ( isCreatingNavArea && cmd != EDIT_BEGIN_AREA && cmd != EDIT_END_AREA && cmd != EDIT_NONE )
		isCreatingNavArea = false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the ground height below this point in "height".
 * Return false if position is invalid (outside of map, in a solid area, etc).
 */

CNavAreaGrid::CNavAreaGrid( void )
    : m_cellSize( 300.0f )
{
	m_grid = NULL;
	Reset();
}

CNavAreaGrid::~CNavAreaGrid()
{
	delete[] m_grid;
	m_grid = NULL;
}

/**
 * Clear the grid
 */
void CNavAreaGrid::Reset( void )
{
	if ( m_grid )
		delete[] m_grid;

	m_grid      = NULL;
	m_gridSizeX = 0;
	m_gridSizeY = 0;

	// clear the hash table
	for ( int i = 0; i < HASH_TABLE_SIZE; ++i )
		m_hashTable[i] = NULL;

	m_areaCount = 0;

	EditNavAreasReset(); // reset static vars
}

/**
 * Allocate the grid and define its extents
 */
void CNavAreaGrid::Initialize( float minX, float maxX, float minY, float maxY )
{
	if ( m_grid )
		Reset();

	m_minX = minX;
	m_minY = minY;

	m_gridSizeX = ( ( maxX - minX ) / m_cellSize ) + 1;
	m_gridSizeY = ( ( maxY - minY ) / m_cellSize ) + 1;

	m_grid = new NavAreaList[m_gridSizeX * m_gridSizeY];
}

/**
 * Add an area to the grid
 */
void CNavAreaGrid::AddNavArea( CNavArea *area )
{
	// add to grid
	const Extent *extent = area->GetExtent();

	int loX = WorldToGridX( extent->lo.x );
	int loY = WorldToGridY( extent->lo.y );
	int hiX = WorldToGridX( extent->hi.x );
	int hiY = WorldToGridY( extent->hi.y );

	for ( int y = loY; y <= hiY; ++y )
		for ( int x = loX; x <= hiX; ++x )
			m_grid[x + y * m_gridSizeX].push_back( const_cast< CNavArea * >( area ) );

	// add to hash table
	int key = ComputeHashKey( area->GetID() );

	if ( m_hashTable[key] )
	{
		// add to head of list in this slot
		area->m_prevHash             = NULL;
		area->m_nextHash             = m_hashTable[key];
		m_hashTable[key]->m_prevHash = area;
		m_hashTable[key]             = area;
	}
	else
	{
		// first entry in this slot
		m_hashTable[key] = area;
		area->m_nextHash = NULL;
		area->m_prevHash = NULL;
	}

	++m_areaCount;
}

/**
 * Remove an area from the grid
 */
void CNavAreaGrid::RemoveNavArea( CNavArea *area )
{
	// add to grid
	const Extent *extent = area->GetExtent();

	int loX = WorldToGridX( extent->lo.x );
	int loY = WorldToGridY( extent->lo.y );
	int hiX = WorldToGridX( extent->hi.x );
	int hiY = WorldToGridY( extent->hi.y );

	for ( int y = loY; y <= hiY; ++y )
		for ( int x = loX; x <= hiX; ++x )
			m_grid[x + y * m_gridSizeX].remove( area );

	// remove from hash table
	int key = ComputeHashKey( area->GetID() );

	if ( area->m_prevHash )
	{
		area->m_prevHash->m_nextHash = area->m_nextHash;
	}
	else
	{
		// area was at start of list
		m_hashTable[key] = area->m_nextHash;

		if ( m_hashTable[key] )
			m_hashTable[key]->m_prevHash = NULL;
	}

	if ( area->m_nextHash )
	{
		area->m_nextHash->m_prevHash = area->m_prevHash;
	}

	--m_areaCount;
}

/**
 * Given a position, return the nav area that IsOverlapping and is *immediately* beneath it
 */
CNavArea *CNavAreaGrid::GetNavArea( const Vector *pos, float beneathLimit ) const
{
	if ( m_grid == NULL )
		return NULL;

	// get list in cell that contains position
	int x             = WorldToGridX( pos->x );
	int y             = WorldToGridY( pos->y );
	NavAreaList *list = &m_grid[x + y * m_gridSizeX];

	// search cell list to find correct area
	CNavArea *use  = NULL;
	float useZ     = -99999999.9f;
	Vector testPos = *pos + Vector( 0, 0, 5 );

	for ( NavAreaList::iterator iter = list->begin(); iter != list->end(); ++iter )
	{
		CNavArea *area = *iter;

		// check if position is within 2D boundaries of this area
		if ( area->IsOverlapping( &testPos ) )
		{
			// project position onto area to get Z
			float z = area->GetZ( &testPos );

			// if area is above us, skip it
			if ( z > testPos.z )
				continue;

			// if area is too far below us, skip it
			if ( z < pos->z - beneathLimit )
				continue;

			// if area is higher than the one we have, use this instead
			if ( z > useZ )
			{
				use  = area;
				useZ = z;
			}
		}
	}

	return use;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given a position in the world, return the nav area that is closest
 * and at the same height, or beneath it.
 * Used to find initial area if we start off of the mesh.
 */
CNavArea *CNavAreaGrid::GetNearestNavArea( const Vector *pos, bool anyZ ) const
{
	if ( m_grid == NULL )
		return NULL;

	CNavArea *close   = NULL;
	float closeDistSq = 99999999.9f;

	// quick check
	close = GetNavArea( pos );
	if ( close )
		return close;

	// ensure source position is well behaved
	Vector source;
	source.x = pos->x;
	source.y = pos->y;
	if ( GetGroundHeight( pos, &source.z ) == false )
		return NULL;

	source.z += HalfHumanHeight;

	/// Grid-accelerated incremental step traversal

	// find closest nav area
	for ( NavAreaList::iterator iter = TheNavAreaList.begin(); iter != TheNavAreaList.end(); ++iter )
	{
		CNavArea *area = *iter;

		Vector areaPos;
		area->GetClosestPointOnArea( &source, &areaPos );

		float distSq = ( areaPos - source ).LengthSquared();

		// keep the closest area
		if ( distSq < closeDistSq )
		{
			// check LOS to area
			if ( !anyZ )
			{
				TraceResult result;
				UTIL_TraceLine( source, areaPos + Vector( 0, 0, HalfHumanHeight ), ignore_monsters, ignore_glass, NULL, &result );
				if ( result.flFraction != 1.0f )
					continue;
			}

			closeDistSq = distSq;
			close       = area;
		}
	}

	return close;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Given an ID, return the associated area
 */
CNavArea *CNavAreaGrid::GetNavAreaByID( unsigned int id ) const
{
	if ( id == 0 )
		return NULL;

	int key = ComputeHashKey( id );

	for ( CNavArea *area = m_hashTable[key]; area; area = area->m_nextHash )
		if ( area->GetID() == id )
			return area;

	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return radio chatter place for given coordinate
 */
unsigned int CNavAreaGrid::GetPlace( const Vector *pos ) const
{
	CNavArea *area = GetNearestNavArea( pos, true );

	if ( area )
		return area->GetPlace();

	return UNDEFINED_PLACE;
}
