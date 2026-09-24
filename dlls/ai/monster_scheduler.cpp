/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   This source code contains proprietary and confidential information of
 *   Valve LLC and its suppliers.  Access to this code is restricted to
 *   persons who have executed a written SDK license with Valve.  Any access,
 *   use or distribution of this code by or to any unlicensed person is illegal.
 *
 ****/

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/nodes.h"
#include "ai/monsters.h"
#include "ai/monster_scheduler.h"
#include "core/animation.h"
#include "core/saverestore.h"
#include "weapons/weapon_base.h"
#include "gameplay/scripted.h"
#include "ai/squadmonster.h"
#include "core/decals.h"
#include "ai/soundent.h"
#include "gameplay/gamerules.h"
#include "ai/defaultai.h"
#include "ai/schedule.h"
#include "systems/crash_handler.h"

//=========================================================
// 	RouteClear - zeroes out the monster's route array and goal
//=========================================================
void CBaseMonster ::RouteClear( void )
{
	RouteNew();
	m_movementGoal     = MOVEGOAL_NONE;
	m_movementActivity = ACT_IDLE;
	Forget( bits_MEMORY_MOVE_FAILED );
}

//=========================================================
// Route New - clears out a route to be changed, but keeps
//				goal intact.
//=========================================================
void CBaseMonster ::RouteNew( void )
{
	memset( m_Route, 0, sizeof( m_Route ) );
	m_iRouteIndex = 0;
}

//=========================================================
// FRouteClear - returns TRUE is the Route is cleared out
// ( invalid )
//=========================================================
BOOL CBaseMonster ::FRouteClear( void )
{
	if ( m_Route[m_iRouteIndex].iType == 0 || m_movementGoal == MOVEGOAL_NONE )
		return TRUE;

	return FALSE;
}

//=========================================================
// FRefreshRoute - after calculating a path to the monster's
// target, this function copies as many waypoints as possible
// from that path to the monster's Route array
//=========================================================
BOOL CBaseMonster ::FRefreshRoute( void )
{
	CBaseEntity *pPathCorner;
	int i;
	BOOL returnCode;

	RouteNew();

	returnCode = FALSE;

	switch ( m_movementGoal )
	{
	case MOVEGOAL_PATHCORNER:
	{
		// monster is on a path_corner loop
		pPathCorner = m_pGoalEnt;
		i           = 0;

		while ( pPathCorner && i < ROUTE_SIZE )
		{
			m_Route[i].iType       = bits_MF_TO_PATHCORNER;
			m_Route[i].vecLocation = pPathCorner->pev->origin;

			pPathCorner = pPathCorner->GetNextTarget();

			// Last path_corner in list?
			if ( !pPathCorner )
				m_Route[i].iType |= bits_MF_IS_GOAL;

			i++;
		}
	}
		returnCode = TRUE;
		break;

	case MOVEGOAL_ENEMY:
		returnCode = BuildRoute( m_vecEnemyLKP, bits_MF_TO_ENEMY, m_hEnemy );
		break;

	case MOVEGOAL_LOCATION:
		returnCode = BuildRoute( m_vecMoveGoal, bits_MF_TO_LOCATION, NULL );
		break;

	case MOVEGOAL_TARGETENT:
		if ( m_hTargetEnt != NULL )
		{
			returnCode = BuildRoute( m_hTargetEnt->pev->origin, bits_MF_TO_TARGETENT, m_hTargetEnt );
		}
		break;

	case MOVEGOAL_NODE:
		returnCode = FGetNodeRoute( m_vecMoveGoal );
		//			if ( returnCode )
		//				RouteSimplify( NULL );
		break;
	}

	return returnCode;
}

BOOL CBaseMonster::MoveToEnemy( Activity movementAct, float waitTime )
{
	m_movementActivity = movementAct;
	m_moveWaitTime     = waitTime;

	m_movementGoal = MOVEGOAL_ENEMY;
	return FRefreshRoute();
}

BOOL CBaseMonster::MoveToLocation( Activity movementAct, float waitTime, const Vector &goal )
{
	m_movementActivity = movementAct;
	m_moveWaitTime     = waitTime;

	m_movementGoal = MOVEGOAL_LOCATION;
	m_vecMoveGoal  = goal;
	return FRefreshRoute();
}

BOOL CBaseMonster::MoveToTarget( Activity movementAct, float waitTime )
{
	m_movementActivity = movementAct;
	m_moveWaitTime     = waitTime;

	m_movementGoal = MOVEGOAL_TARGETENT;
	return FRefreshRoute();
}

BOOL CBaseMonster::MoveToNode( Activity movementAct, float waitTime, const Vector &goal )
{
	m_movementActivity = movementAct;
	m_moveWaitTime     = waitTime;

	m_movementGoal = MOVEGOAL_NODE;
	m_vecMoveGoal  = goal;
	return FRefreshRoute();
}

#ifdef _DEBUG
void DrawRoute( entvars_t *pev, WayPoint_t *m_Route, int m_iRouteIndex, int r, int g, int b )
{
	int i;

	if ( m_Route[m_iRouteIndex].iType == 0 )
	{
		ALERT( at_aiconsole, "Can't draw route!\n" );
		return;
	}

	//	UTIL_ParticleEffect ( m_Route[ m_iRouteIndex ].vecLocation, g_vecZero, 255, 25 );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
	WRITE_BYTE( TE_BEAMPOINTS );
	WRITE_COORD( pev->origin.x );
	WRITE_COORD( pev->origin.y );
	WRITE_COORD( pev->origin.z );
	WRITE_COORD( m_Route[m_iRouteIndex].vecLocation.x );
	WRITE_COORD( m_Route[m_iRouteIndex].vecLocation.y );
	WRITE_COORD( m_Route[m_iRouteIndex].vecLocation.z );

	WRITE_SHORT( g_sModelIndexLaser );
	WRITE_BYTE( 0 );   // frame start
	WRITE_BYTE( 10 );  // framerate
	WRITE_BYTE( 1 );   // life
	WRITE_BYTE( 16 );  // width
	WRITE_BYTE( 0 );   // noise
	WRITE_BYTE( r );   // r, g, b
	WRITE_BYTE( g );   // r, g, b
	WRITE_BYTE( b );   // r, g, b
	WRITE_BYTE( 255 ); // brightness
	WRITE_BYTE( 10 );  // speed
	MESSAGE_END();

	for ( i = m_iRouteIndex; i < ROUTE_SIZE - 1; i++ )
	{
		if ( ( m_Route[i].iType & bits_MF_IS_GOAL ) || ( m_Route[i + 1].iType == 0 ) )
			break;

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( m_Route[i].vecLocation.x );
		WRITE_COORD( m_Route[i].vecLocation.y );
		WRITE_COORD( m_Route[i].vecLocation.z );
		WRITE_COORD( m_Route[i + 1].vecLocation.x );
		WRITE_COORD( m_Route[i + 1].vecLocation.y );
		WRITE_COORD( m_Route[i + 1].vecLocation.z );
		WRITE_SHORT( g_sModelIndexLaser );
		WRITE_BYTE( 0 );   // frame start
		WRITE_BYTE( 10 );  // framerate
		WRITE_BYTE( 1 );   // life
		WRITE_BYTE( 8 );   // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( r );   // r, g, b
		WRITE_BYTE( g );   // r, g, b
		WRITE_BYTE( b );   // r, g, b
		WRITE_BYTE( 255 ); // brightness
		WRITE_BYTE( 10 );  // speed
		MESSAGE_END();

		//		UTIL_ParticleEffect ( m_Route[ i ].vecLocation, g_vecZero, 255, 25 );
	}
}
#endif

int ShouldSimplify( int routeType )
{
	routeType &= ~bits_MF_IS_GOAL;

	if ( ( routeType == bits_MF_TO_PATHCORNER ) || ( routeType & bits_MF_DONT_SIMPLIFY ) )
		return FALSE;
	return TRUE;
}

//=========================================================
// RouteSimplify
//
// Attempts to make the route more direct by cutting out
// unnecessary nodes & cutting corners.
//
//=========================================================
void CBaseMonster ::RouteSimplify( CBaseEntity *pTargetEnt )
{
	// BUGBUG: this doesn't work 100% yet
	int i, count, outCount;
	Vector vecStart;
	WayPoint_t outRoute[ROUTE_SIZE * 4]; // Enlarged to prevent stack overrun under heavy triangulation

	count = 0;

	for ( i = m_iRouteIndex; i < ROUTE_SIZE; i++ )
	{
		if ( !m_Route[i].iType )
			break;
		else
			count++;
		if ( m_Route[i].iType & bits_MF_IS_GOAL )
			break;
	}
	// Can't simplify a direct route!
	if ( count < 2 )
	{
		//		DrawRoute( pev, m_Route, m_iRouteIndex, 0, 0, 255 );
		return;
	}

	g_CrashHandler.LogCustom( "ROUTE_SIMP: [%3d] %-20s -> RouteSimplify count=%d target=%s",
	                          entindex(), STRING( pev->classname ), count,
	                          pTargetEnt ? STRING( pTargetEnt->pev->classname ) : "null" );

	outCount = 0;
	vecStart = pev->origin;
	for ( i = 0; i < count - 1; i++ )
	{
		if ( outCount >= (int)( ARRAYSIZE( outRoute ) - 4 ) )
			break;

		if ( m_iRouteIndex + i >= ROUTE_SIZE || m_iRouteIndex + i + 1 >= ROUTE_SIZE )
			break;

		// Don't eliminate path_corners
		if ( !ShouldSimplify( m_Route[m_iRouteIndex + i].iType ) )
		{
			outRoute[outCount] = m_Route[m_iRouteIndex + i];
		}
		else if ( CheckLocalMove( vecStart, m_Route[m_iRouteIndex + i + 1].vecLocation, pTargetEnt, NULL ) == LOCALMOVE_VALID )
		{
			// Skip vert
			continue;
		}
		else
		{
			Vector vecTest, vecSplit;

			// Halfway between this and next
			vecTest = ( m_Route[m_iRouteIndex + i + 1].vecLocation + m_Route[m_iRouteIndex + i].vecLocation ) * 0.5;

			// Halfway between this and previous
			vecSplit = ( m_Route[m_iRouteIndex + i].vecLocation + vecStart ) * 0.5;

			int iType = ( m_Route[m_iRouteIndex + i].iType | bits_MF_TO_DETOUR ) & ~bits_MF_NOT_TO_MASK;
			if ( CheckLocalMove( vecStart, vecTest, pTargetEnt, NULL ) == LOCALMOVE_VALID )
			{
				outRoute[outCount].iType       = iType;
				outRoute[outCount].vecLocation = vecTest;
			}
			else if ( CheckLocalMove( vecSplit, vecTest, pTargetEnt, NULL ) == LOCALMOVE_VALID )
			{
				outRoute[outCount].iType           = iType;
				outRoute[outCount].vecLocation     = vecSplit;
				outCount++; // Adding an extra point
				outRoute[outCount].iType           = iType;
				outRoute[outCount].vecLocation     = vecTest;
			}
			else
			{
				outRoute[outCount] = m_Route[m_iRouteIndex + i];
			}
		}
		// Get last point
		vecStart = outRoute[outCount].vecLocation;
		outCount++;
	}

	if ( outCount < (int)( ARRAYSIZE( outRoute ) - 2 ) && ( m_iRouteIndex + i ) < ROUTE_SIZE )
	{
		outRoute[outCount] = m_Route[m_iRouteIndex + i];
		outCount++;
	}

	// Terminate
	if ( outCount < (int)ARRAYSIZE( outRoute ) )
	{
		outRoute[outCount].iType = 0;
	}

	// Copy the simplified route, disable for testing
	m_iRouteIndex = 0;
	for ( i = 0; i < ROUTE_SIZE && i < outCount; i++ )
	{
		m_Route[i] = outRoute[i];
	}

	// Terminate route
	if ( i < ROUTE_SIZE )
		m_Route[i].iType = 0;
}

//=========================================================
// AdvanceRoute - poorly named function that advances the
// m_iRouteIndex. If it goes beyond ROUTE_SIZE, the route
// is refreshed.
//=========================================================
void CBaseMonster ::AdvanceRoute( float distance )
{

	if ( m_iRouteIndex == ROUTE_SIZE - 1 )
	{
		// time to refresh the route.
		if ( !FRefreshRoute() )
		{
			ALERT( at_aiconsole, "Can't Refresh Route!!\n" );
		}
	}
	else
	{
		if ( !( m_Route[m_iRouteIndex].iType & bits_MF_IS_GOAL ) )
		{
			// If we've just passed a path_corner, advance m_pGoalEnt
			if ( ( m_Route[m_iRouteIndex].iType & ~bits_MF_NOT_TO_MASK ) == bits_MF_TO_PATHCORNER )
				m_pGoalEnt = m_pGoalEnt->GetNextTarget();

			// IF both waypoints are nodes, then check for a link for a door and operate it.
			//
			if ( ( m_Route[m_iRouteIndex].iType & bits_MF_TO_NODE ) == bits_MF_TO_NODE && ( m_Route[m_iRouteIndex + 1].iType & bits_MF_TO_NODE ) == bits_MF_TO_NODE )
			{
				// ALERT(at_aiconsole, "SVD: Two nodes. ");

				int iSrcNode  = WorldGraph.FindNearestNode( m_Route[m_iRouteIndex].vecLocation, this );
				int iDestNode = WorldGraph.FindNearestNode( m_Route[m_iRouteIndex + 1].vecLocation, this );

				int iLink;
				WorldGraph.HashSearch( iSrcNode, iDestNode, iLink );

				if ( iLink >= 0 && iLink < WorldGraph.m_cLinks && WorldGraph.m_pLinkPool && WorldGraph.m_pLinkPool[iLink].m_pLinkEnt != NULL )
				{
					// ALERT(at_aiconsole, "A link. ");
					if ( WorldGraph.HandleLinkEnt( iSrcNode, WorldGraph.m_pLinkPool[iLink].m_pLinkEnt, m_afCapability, CGraph::NODEGRAPH_DYNAMIC ) )
					{
						// ALERT(at_aiconsole, "usable.");
						entvars_t *pevDoor = WorldGraph.m_pLinkPool[iLink].m_pLinkEnt;
						if ( pevDoor )
						{
							m_flMoveWaitFinished = OpenDoorAndWait( pevDoor );
							//							ALERT( at_aiconsole, "Wating for door %.2f\n", m_flMoveWaitFinished-gpGlobals->time );
						}
					}
				}
				// ALERT(at_aiconsole, "\n");
			}
			m_iRouteIndex++;
		}
		else // At goal!!!
		{
			if ( distance < m_flGroundSpeed * 0.2 /* FIX */ )
			{
				MovementComplete();
			}
		}
	}
}

int CBaseMonster ::RouteClassify( int iMoveFlag )
{
	int movementGoal;

	movementGoal = MOVEGOAL_NONE;

	if ( iMoveFlag & bits_MF_TO_TARGETENT )
		movementGoal = MOVEGOAL_TARGETENT;
	else if ( iMoveFlag & bits_MF_TO_ENEMY )
		movementGoal = MOVEGOAL_ENEMY;
	else if ( iMoveFlag & bits_MF_TO_PATHCORNER )
		movementGoal = MOVEGOAL_PATHCORNER;
	else if ( iMoveFlag & bits_MF_TO_NODE )
		movementGoal = MOVEGOAL_NODE;
	else if ( iMoveFlag & bits_MF_TO_LOCATION )
		movementGoal = MOVEGOAL_LOCATION;

	return movementGoal;
}

//=========================================================
// BuildRoute
//=========================================================
BOOL CBaseMonster ::BuildRoute( const Vector &vecGoal, int iMoveFlag, CBaseEntity *pTarget )
{
	float flDist;
	Vector vecApex;
	int iLocalMove;

	g_CrashHandler.LogCustom( "BUILD_ROUTE: [%3d] %-20s -> BuildRoute goal=(%.1f, %.1f, %.1f), flag=%d, target=%s",
	                          entindex(), STRING( pev->classname ), vecGoal.x, vecGoal.y, vecGoal.z, iMoveFlag,
	                          pTarget ? STRING( pTarget->pev->classname ) : "null" );

	RouteNew();
	m_movementGoal = RouteClassify( iMoveFlag );

	// so we don't end up with no moveflags
	m_Route[0].vecLocation = vecGoal;
	m_Route[0].iType       = iMoveFlag | bits_MF_IS_GOAL;

	// check simple local move
	iLocalMove = CheckLocalMove( pev->origin, vecGoal, pTarget, &flDist );

	if ( iLocalMove == LOCALMOVE_VALID )
	{
		// monster can walk straight there!
		return TRUE;
	}
	// try to triangulate around any obstacles.
	else if ( iLocalMove != LOCALMOVE_INVALID_DONT_TRIANGULATE && FTriangulate( pev->origin, vecGoal, flDist, pTarget, &vecApex ) )
	{
		// there is a slightly more complicated path that allows the monster to reach vecGoal
		m_Route[0].vecLocation = vecApex;
		m_Route[0].iType       = ( iMoveFlag | bits_MF_TO_DETOUR );

		m_Route[1].vecLocation = vecGoal;
		m_Route[1].iType       = iMoveFlag | bits_MF_IS_GOAL;

		/*
		WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(MSG_BROADCAST, TE_SHOWLINE);
		WRITE_COORD(MSG_BROADCAST, vecApex.x );
		WRITE_COORD(MSG_BROADCAST, vecApex.y );
		WRITE_COORD(MSG_BROADCAST, vecApex.z );
		WRITE_COORD(MSG_BROADCAST, vecApex.x );
		WRITE_COORD(MSG_BROADCAST, vecApex.y );
		WRITE_COORD(MSG_BROADCAST, vecApex.z + 128 );
		*/

		RouteSimplify( pTarget );
		return TRUE;
	}

	// last ditch, try nodes
	if ( FGetNodeRoute( vecGoal ) )
	{
		//		ALERT ( at_console, "Can get there on nodes\n" );
		m_vecMoveGoal = vecGoal;
		RouteSimplify( pTarget );
		return TRUE;
	}

	// b0rk
	return FALSE;
}

//=========================================================
// InsertWaypoint - Rebuilds the existing route so that the
// supplied vector and moveflags are the first waypoint in
// the route, and fills the rest of the route with as much
// of the pre-existing route as possible
//=========================================================
void CBaseMonster ::InsertWaypoint( Vector vecLocation, int afMoveFlags )
{
	int i, type;

	// we have to save some Index and Type information from the real
	// path_corner or node waypoint that the monster was trying to reach. This makes sure that data necessary
	// to refresh the original path exists even in the new waypoints that don't correspond directy to a path_corner
	// or node.
	type = afMoveFlags | ( m_Route[m_iRouteIndex].iType & ~bits_MF_NOT_TO_MASK );

	for ( i = ROUTE_SIZE - 1; i > 0; i-- )
		m_Route[i] = m_Route[i - 1];

	m_Route[m_iRouteIndex].vecLocation = vecLocation;
	m_Route[m_iRouteIndex].iType       = type;
}

//=========================================================
// FTriangulate - tries to overcome local obstacles by
// triangulating a path around them.
//
// iApexDist is how far the obstruction that we are trying
// to triangulate around is from the monster.
//=========================================================
BOOL CBaseMonster ::FTriangulate( const Vector &vecStart, const Vector &vecEnd, float flDist, CBaseEntity *pTargetEnt, Vector *pApex )
{
	Vector vecDir;
	Vector vecForward;
	Vector vecLeft;    // the spot we'll try to triangulate to on the left
	Vector vecRight;   // the spot we'll try to triangulate to on the right
	Vector vecTop;     // the spot we'll try to triangulate to on the top
	Vector vecBottom;  // the spot we'll try to triangulate to on the bottom
	Vector vecFarSide; // the spot that we'll move to after hitting the triangulated point, before moving on to our normal goal.
	int i;
	float sizeX, sizeZ;

	// If the hull width is less than 24, use 24 because CheckLocalMove uses a min of
	// 24.
	sizeX = pev->size.x;
	if ( sizeX < 24.0 )
		sizeX = 24.0;
	else if ( sizeX > 48.0 )
		sizeX = 48.0;
	sizeZ = pev->size.z;
	// if (sizeZ < 24.0)
	//	sizeZ = 24.0;

	vecForward = ( vecEnd - vecStart ).Normalize();

	Vector vecDirUp( 0, 0, 1 );
	vecDir = CrossProduct( vecForward, vecDirUp );

	// start checking right about where the object is, picking two equidistant starting points, one on
	// the left, one on the right. As we progress through the loop, we'll push these away from the obstacle,
	// hoping to find a way around on either side. pev->size.x is added to the ApexDist in order to help select
	// an apex point that insures that the monster is sufficiently past the obstacle before trying to turn back
	// onto its original course.

	vecLeft  = pev->origin + ( vecForward * ( flDist + sizeX ) ) - vecDir * ( sizeX * 3 );
	vecRight = pev->origin + ( vecForward * ( flDist + sizeX ) ) + vecDir * ( sizeX * 3 );
	if ( pev->movetype == MOVETYPE_FLY )
	{
		vecTop    = pev->origin + ( vecForward * flDist ) + ( vecDirUp * sizeZ * 3 );
		vecBottom = pev->origin + ( vecForward * flDist ) - ( vecDirUp * sizeZ * 3 );
	}

	vecFarSide = m_Route[m_iRouteIndex].vecLocation;

	vecDir = vecDir * sizeX * 2;
	if ( pev->movetype == MOVETYPE_FLY )
		vecDirUp = vecDirUp * sizeZ * 2;

	for ( i = 0; i < 8; i++ )
	{
		if ( CheckLocalMove( pev->origin, vecRight, pTargetEnt, NULL ) == LOCALMOVE_VALID )
		{
			if ( CheckLocalMove( vecRight, vecFarSide, pTargetEnt, NULL ) == LOCALMOVE_VALID )
			{
				if ( pApex )
				{
					*pApex = vecRight;
				}

				return TRUE;
			}
		}
		if ( CheckLocalMove( pev->origin, vecLeft, pTargetEnt, NULL ) == LOCALMOVE_VALID )
		{
			if ( CheckLocalMove( vecLeft, vecFarSide, pTargetEnt, NULL ) == LOCALMOVE_VALID )
			{
				if ( pApex )
				{
					*pApex = vecLeft;
				}

				return TRUE;
			}
		}

		if ( pev->movetype == MOVETYPE_FLY )
		{
			if ( CheckLocalMove( pev->origin, vecTop, pTargetEnt, NULL ) == LOCALMOVE_VALID )
			{
				if ( CheckLocalMove( vecTop, vecFarSide, pTargetEnt, NULL ) == LOCALMOVE_VALID )
				{
					if ( pApex )
					{
						*pApex = vecTop;
						// ALERT(at_aiconsole, "triangulate over\n");
					}

					return TRUE;
				}
			}
#if 1
			if ( CheckLocalMove( pev->origin, vecBottom, pTargetEnt, NULL ) == LOCALMOVE_VALID )
			{
				if ( CheckLocalMove( vecBottom, vecFarSide, pTargetEnt, NULL ) == LOCALMOVE_VALID )
				{
					if ( pApex )
					{
						*pApex = vecBottom;
						// ALERT(at_aiconsole, "triangulate under\n");
					}

					return TRUE;
				}
			}
#endif
		}

		vecRight = vecRight + vecDir;
		vecLeft  = vecLeft - vecDir;
		if ( pev->movetype == MOVETYPE_FLY )
		{
			vecTop    = vecTop + vecDirUp;
			vecBottom = vecBottom - vecDirUp;
		}
	}

	return FALSE;
}

//=========================================================
// Move - take a single step towards the next ROUTE location
//=========================================================
#define DIST_TO_CHECK 200

void CBaseMonster ::Move( float flInterval )
{
	float flWaypointDist;
	float flCheckDist;
	float flDist; // how far the lookahead check got before hitting an object.
	Vector vecDir;
	Vector vecApex;
	CBaseEntity *pTargetEnt;

	// Don't move if no valid route
	if ( FRouteClear() )
	{
		// If we still have a movement goal, then this is probably a route truncated by SimplifyRoute()
		// so refresh it.
		if ( m_movementGoal == MOVEGOAL_NONE || !FRefreshRoute() )
		{
			ALERT( at_aiconsole, "Tried to move with no route!\n" );
			TaskFail();
			return;
		}
	}

	if ( m_iRouteIndex < 0 || m_iRouteIndex >= ROUTE_SIZE )
	{
		TaskFail();
		return;
	}

	if ( m_flMoveWaitFinished > gpGlobals->time )
		return;

	// if the monster is moving directly towards an entity (enemy for instance), we'll set this pointer
	// to that entity for the CheckLocalMove and Triangulate functions.
	pTargetEnt = NULL;

	// local move to waypoint.
	vecDir         = ( m_Route[m_iRouteIndex].vecLocation - pev->origin ).Normalize();
	flWaypointDist = ( m_Route[m_iRouteIndex].vecLocation - pev->origin ).Length2D();

	MakeIdealYaw( m_Route[m_iRouteIndex].vecLocation );
	ChangeYaw( pev->yaw_speed );

	// if the waypoint is closer than CheckDist, CheckDist is the dist to waypoint
	if ( flWaypointDist < DIST_TO_CHECK )
	{
		flCheckDist = flWaypointDist;
	}
	else
	{
		flCheckDist = DIST_TO_CHECK;
	}

	if ( ( m_Route[m_iRouteIndex].iType & ( ~bits_MF_NOT_TO_MASK ) ) == bits_MF_TO_ENEMY )
	{
		// only on a PURE move to enemy ( i.e., ONLY MF_TO_ENEMY set, not MF_TO_ENEMY and DETOUR )
		pTargetEnt = m_hEnemy;
	}
	else if ( ( m_Route[m_iRouteIndex].iType & ~bits_MF_NOT_TO_MASK ) == bits_MF_TO_TARGETENT )
	{
		pTargetEnt = m_hTargetEnt;
	}

	// !!!BUGBUG - CheckDist should be derived from ground speed.
	// If this fails, it should be because of some dynamic entity blocking this guy.
	// We've already checked this path, so we should wait and time out if the entity doesn't move
	flDist = 0;
	if ( CheckLocalMove( pev->origin, pev->origin + vecDir * flCheckDist, pTargetEnt, &flDist ) != LOCALMOVE_VALID )
	{
		CBaseEntity *pBlocker;

		// Can't move, stop
		Stop();
		// Blocking entity is in global trace_ent
		pBlocker = CBaseEntity::Instance( gpGlobals->trace_ent );
		if ( pBlocker )
		{
			DispatchBlocked( edict(), pBlocker->edict() );
		}

		if ( pBlocker && m_moveWaitTime > 0 && pBlocker->IsMoving() && !pBlocker->IsPlayer() && ( gpGlobals->time - m_flMoveWaitFinished ) > 3.0 )
		{
			// Can we still move toward our target?
			if ( flDist < m_flGroundSpeed )
			{
				// No, Wait for a second
				m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime;
				return;
			}
			// Ok, still enough room to take a step
		}
		else
		{
			// try to triangulate around whatever is in the way.
			if ( FTriangulate( pev->origin, m_Route[m_iRouteIndex].vecLocation, flDist, pTargetEnt, &vecApex ) )
			{
				InsertWaypoint( vecApex, bits_MF_TO_DETOUR );
				RouteSimplify( pTargetEnt );
			}
			else
			{
				//				ALERT ( at_aiconsole, "Couldn't Triangulate\n" );
				Stop();
				// Only do this once until your route is cleared
				if ( m_moveWaitTime > 0 && !( m_afMemory & bits_MEMORY_MOVE_FAILED ) )
				{
					FRefreshRoute();
					if ( FRouteClear() )
					{
						TaskFail();
					}
					else
					{
						// Don't get stuck
						if ( ( gpGlobals->time - m_flMoveWaitFinished ) < 0.2 )
							Remember( bits_MEMORY_MOVE_FAILED );

						m_flMoveWaitFinished = gpGlobals->time + 0.1;
					}
				}
				else
				{
					TaskFail();
					ALERT( at_aiconsole, "%s Failed to move (%d)!\n", STRING( pev->classname ), HasMemory( bits_MEMORY_MOVE_FAILED ) );
					// ALERT( at_aiconsole, "%f, %f, %f\n", pev->origin.z, (pev->origin + (vecDir * flCheckDist)).z, m_Route[m_iRouteIndex].vecLocation.z );
				}
				return;
			}
		}
	}

	// close enough to the target, now advance to the next target. This is done before actually reaching
	// the target so that we get a nice natural turn while moving.
	if ( ShouldAdvanceRoute( flWaypointDist ) ) ///!!!BUGBUG- magic number
	{
		AdvanceRoute( flWaypointDist );
	}

	// Might be waiting for a door
	if ( m_flMoveWaitFinished > gpGlobals->time )
	{
		Stop();
		return;
	}

	// Stop navigation advancement beyond verified line-of-sight distance.
	if ( flCheckDist < m_flGroundSpeed * flInterval )
	{
		flInterval = flCheckDist / m_flGroundSpeed;
		// ALERT( at_console, "%.02f\n", flInterval );
	}
	MoveExecute( pTargetEnt, vecDir, flInterval );

	if ( MovementIsComplete() )
	{
		Stop();
		RouteClear();
	}
}

BOOL CBaseMonster::ShouldAdvanceRoute( float flWaypointDist )
{
	if ( flWaypointDist <= MONSTER_CUT_CORNER_DIST )
	{
		// ALERT( at_console, "cut %f\n", flWaypointDist );
		return TRUE;
	}

	return FALSE;
}

void CBaseMonster::MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval )
{
	//	float flYaw = UTIL_VecToYaw ( m_Route[ m_iRouteIndex ].vecLocation - pev->origin );// build a yaw that points to the goal.
	//	WALK_MOVE( ENT(pev), flYaw, m_flGroundSpeed * flInterval, WALKMOVE_NORMAL );
	if ( m_IdealActivity != m_movementActivity )
		m_IdealActivity = m_movementActivity;

	float flTotal = m_flGroundSpeed * pev->framerate * flInterval;
	float flStep;
	while ( flTotal > 0.001 )
	{
		// don't walk more than 16 units or stairs stop working
		flStep = min( 16.0f, flTotal );
		UTIL_MoveToOrigin( ENT( pev ), m_Route[m_iRouteIndex].vecLocation, flStep, MOVE_NORMAL );
		flTotal -= flStep;
	}
	// ALERT( at_console, "dist %f\n", m_flGroundSpeed * pev->framerate * flInterval );
}

void CBaseMonster ::MovementComplete( void )
{
	switch ( m_iTaskStatus )
	{
	case TASKSTATUS_NEW:
	case TASKSTATUS_RUNNING:
		m_iTaskStatus = TASKSTATUS_RUNNING_TASK;
		break;

	case TASKSTATUS_RUNNING_MOVEMENT:
		TaskComplete();
		break;

	case TASKSTATUS_RUNNING_TASK:
		ALERT( at_error, "Movement completed twice!\n" );
		break;

	case TASKSTATUS_COMPLETE:
		break;
	}
	m_movementGoal = MOVEGOAL_NONE;
}

int CBaseMonster::TaskIsRunning( void )
{
	if ( m_iTaskStatus != TASKSTATUS_COMPLETE &&
	     m_iTaskStatus != TASKSTATUS_RUNNING_MOVEMENT )
		return 1;

	return 0;
}

//=========================================================
// FindCover - tries to find a nearby node that will hide
// the caller from its enemy.
//
// If supplied, search will return a node at least as far
// away as MinDist, but no farther than MaxDist.
// if MaxDist isn't supplied, it defaults to a reasonable
// value
//=========================================================
// Fallback to nearest waypoint node if direct path is obstructed

// float CGraph::PathLength( int iStart, int iDest, int iHull, int afCapMask )

BOOL CBaseMonster ::FindCover( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist )
{
	int i;
	int iMyHullIndex;
	int iMyNode;
	int iThreatNode;
	float flDist;
	Vector vecLookersOffset;
	TraceResult tr;

	if ( !flMaxDist )
	{
		// user didn't supply a MaxDist, so work up a crazy one.
		flMaxDist = 784;
	}

	if ( flMinDist > 0.5 * flMaxDist )
	{
#if _DEBUG
		ALERT( at_console, "FindCover MinDist (%.0f) too close to MaxDist (%.0f)\n", flMinDist, flMaxDist );
#endif
		flMinDist = 0.5 * flMaxDist;
	}

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet || WorldGraph.m_cNodes <= 0 || !WorldGraph.m_pNodes )
	{
		ALERT( at_aiconsole, "Graph not ready for findcover!\n" );
		return FALSE;
	}

	if ( WorldGraph.m_iLastCoverSearch < 0 || WorldGraph.m_iLastCoverSearch >= WorldGraph.m_cNodes )
	{
		WorldGraph.m_iLastCoverSearch = 0;
	}

	iMyNode      = WorldGraph.FindNearestNode( pev->origin, this );
	iThreatNode  = WorldGraph.FindNearestNode( vecThreat, this );
	iMyHullIndex = WorldGraph.HullIndex( this );

	if ( iMyNode == NO_NODE )
	{
		ALERT( at_aiconsole, "FindCover() - %s has no nearest node!\n", STRING( pev->classname ) );
		return FALSE;
	}
	if ( iThreatNode == NO_NODE )
	{
		// ALERT ( at_aiconsole, "FindCover() - Threat has no nearest node!\n" );
		iThreatNode = iMyNode;
		// return FALSE;
	}

	vecLookersOffset = vecThreat + vecViewOffset; // calculate location of enemy's eyes

	// we'll do a rough sample to find nodes that are relatively nearby
	for ( i = 0; i < WorldGraph.m_cNodes; i++ )
	{
		int nodeNumber = ( i + WorldGraph.m_iLastCoverSearch ) % WorldGraph.m_cNodes;

		CNode &node                   = WorldGraph.Node( nodeNumber );
		WorldGraph.m_iLastCoverSearch = nodeNumber + 1; // next monster that searches for cover node will start where we left off here.

		// could use an optimization here!!
		flDist = ( pev->origin - node.m_vecOrigin ).Length();

		// DON'T do the trace check on a node that is farther away than a node that we've already found to
		// provide cover! Also make sure the node is within the mins/maxs of the search.
		if ( flDist >= flMinDist && flDist < flMaxDist )
		{
			UTIL_TraceLine( node.m_vecOrigin + vecViewOffset, vecLookersOffset, ignore_monsters, ignore_glass, ENT( pev ), &tr );

			// if this node will block the threat's line of sight to me...
			if ( tr.flFraction != 1.0 )
			{
				// ..and is also closer to me than the threat, or the same distance from myself and the threat the node is good.
				if ( ( iMyNode == iThreatNode ) || WorldGraph.PathLength( iMyNode, nodeNumber, iMyHullIndex, m_afCapability ) <= WorldGraph.PathLength( iThreatNode, nodeNumber, iMyHullIndex, m_afCapability ) )
				{
					if ( FValidateCover( node.m_vecOrigin ) && MoveToLocation( ACT_RUN, 0, node.m_vecOrigin ) )
					{
						/*
						MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
						    WRITE_BYTE( TE_SHOWLINE);

						    WRITE_COORD( node.m_vecOrigin.x );
						    WRITE_COORD( node.m_vecOrigin.y );
						    WRITE_COORD( node.m_vecOrigin.z );

						    WRITE_COORD( vecLookersOffset.x );
						    WRITE_COORD( vecLookersOffset.y );
						    WRITE_COORD( vecLookersOffset.z );
						MESSAGE_END();
						*/

						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}

//=========================================================
// BuildNearestRoute - tries to build a route as close to the target
// as possible, even if there isn't a path to the final point.
//
// If supplied, search will return a node at least as far
// away as MinDist from vecThreat, but no farther than MaxDist.
// if MaxDist isn't supplied, it defaults to a reasonable
// value
//=========================================================
BOOL CBaseMonster ::BuildNearestRoute( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist )
{
	int i;
	int iMyHullIndex;
	int iMyNode;
	float flDist;
	Vector vecLookersOffset;
	TraceResult tr;

	if ( !flMaxDist )
	{
		// user didn't supply a MaxDist, so work up a crazy one.
		flMaxDist = 784;
	}

	if ( flMinDist > 0.5 * flMaxDist )
	{
#if _DEBUG
		ALERT( at_console, "FindCover MinDist (%.0f) too close to MaxDist (%.0f)\n", flMinDist, flMaxDist );
#endif
		flMinDist = 0.5 * flMaxDist;
	}

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet || !WorldGraph.m_fRoutingComplete || !WorldGraph.m_pRouteInfo || !WorldGraph.m_pNodes || WorldGraph.m_cNodes <= 0 )
	{
		ALERT( at_aiconsole, "Graph not ready for BuildNearestRoute!\n" );
		return FALSE;
	}

	if ( WorldGraph.m_iLastCoverSearch < 0 || WorldGraph.m_iLastCoverSearch >= WorldGraph.m_cNodes )
	{
		WorldGraph.m_iLastCoverSearch = 0;
	}

	iMyNode      = WorldGraph.FindNearestNode( pev->origin, this );
	iMyHullIndex = WorldGraph.HullIndex( this );

	if ( iMyNode == NO_NODE )
	{
		ALERT( at_aiconsole, "BuildNearestRoute() - %s has no nearest node!\n", STRING( pev->classname ) );
		return FALSE;
	}

	vecLookersOffset = vecThreat + vecViewOffset; // calculate location of enemy's eyes

	// we'll do a rough sample to find nodes that are relatively nearby
	for ( i = 0; i < WorldGraph.m_cNodes; i++ )
	{
		int nodeNumber = ( i + WorldGraph.m_iLastCoverSearch ) % WorldGraph.m_cNodes;

		CNode &node                   = WorldGraph.Node( nodeNumber );
		WorldGraph.m_iLastCoverSearch = nodeNumber + 1; // next monster that searches for cover node will start where we left off here.

		// can I get there?
		if ( WorldGraph.NextNodeInRoute( iMyNode, nodeNumber, iMyHullIndex, 0 ) != iMyNode )
		{
			flDist = ( vecThreat - node.m_vecOrigin ).Length();

			// is it close?
			if ( flDist > flMinDist && flDist < flMaxDist )
			{
				// can I see where I want to be from there?
				UTIL_TraceLine( node.m_vecOrigin + pev->view_ofs, vecLookersOffset, ignore_monsters, edict(), &tr );

				if ( tr.flFraction == 1.0 )
				{
					// try to actually get there
					if ( BuildRoute( node.m_vecOrigin, bits_MF_TO_LOCATION, NULL ) )
					{
						flMaxDist     = flDist;
						m_vecMoveGoal = node.m_vecOrigin;
						return TRUE; // Continue scanning for optimal candidate node
					}
				}
			}
		}
	}

	return FALSE;
}

//=========================================================
// NODE GRAPH
//=========================================================

//=========================================================
// FGetNodeRoute - tries to build an entire node path from
// the callers origin to the passed vector. If this is
// possible, ROUTE_SIZE waypoints will be copied into the
// callers m_Route. TRUE is returned if the operation
// succeeds (path is valid) or FALSE if failed (no path
// exists )
//=========================================================
BOOL CBaseMonster ::FGetNodeRoute( Vector vecDest )
{
	int iPath[MAX_PATH_SIZE] = {0};
	int iSrcNode, iDestNode;
	int iResult;
	int i;
	int iNumToCopy;

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet || WorldGraph.m_cNodes <= 0 || !WorldGraph.m_pNodes )
	{
		return FALSE;
	}

	iSrcNode  = WorldGraph.FindNearestNode( pev->origin, this );
	iDestNode = WorldGraph.FindNearestNode( vecDest, this );

	if ( iSrcNode == -1 )
	{
		// no node nearest self
		//		ALERT ( at_aiconsole, "FGetNodeRoute: No valid node near self!\n" );
		return FALSE;
	}
	else if ( iDestNode == -1 )
	{
		// no node nearest target
		//		ALERT ( at_aiconsole, "FGetNodeRoute: No valid node near target!\n" );
		return FALSE;
	}

	// valid src and dest nodes were found, so it's safe to proceed with
	// find shortest path
	int iNodeHull = WorldGraph.HullIndex( this ); // make this a monster virtual function
	iResult       = WorldGraph.FindShortestPath( iPath, iSrcNode, iDestNode, iNodeHull, m_afCapability );

	if ( !iResult )
	{
#if 1
		ALERT( at_aiconsole, "No Path from %d to %d!\n", iSrcNode, iDestNode );
		return FALSE;
#else
		BOOL bRoutingSave             = WorldGraph.m_fRoutingComplete;
		WorldGraph.m_fRoutingComplete = FALSE;
		iResult                       = WorldGraph.FindShortestPath( iPath, iSrcNode, iDestNode, iNodeHull, m_afCapability );
		WorldGraph.m_fRoutingComplete = bRoutingSave;
		if ( !iResult )
		{
			ALERT( at_aiconsole, "No Path from %d to %d!\n", iSrcNode, iDestNode );
			return FALSE;
		}
		else
		{
			ALERT( at_aiconsole, "Routing is inconsistent!" );
		}
#endif
	}

	// there's a valid path within iPath now, so now we will fill the route array
	// up with as many of the waypoints as it will hold.

	// don't copy ROUTE_SIZE entries if the path returned is shorter
	// than ROUTE_SIZE!!!
	if ( iResult > 0 && iResult < ROUTE_SIZE && iResult <= MAX_PATH_SIZE )
	{
		iNumToCopy = iResult;
	}
	else if ( iResult > 0 )
	{
		iNumToCopy = min( ROUTE_SIZE, MAX_PATH_SIZE );
	}
	else
	{
		return FALSE;
	}

	for ( i = 0; i < iNumToCopy; i++ )
	{
		if ( i < MAX_PATH_SIZE && iPath[i] >= 0 && iPath[i] < WorldGraph.m_cNodes && WorldGraph.m_pNodes )
		{
			m_Route[i].vecLocation = WorldGraph.m_pNodes[iPath[i]].m_vecOrigin;
			m_Route[i].iType       = bits_MF_TO_NODE;
		}
		else
		{
			return FALSE;
		}
	}

	if ( iNumToCopy < ROUTE_SIZE )
	{
		m_Route[iNumToCopy].vecLocation = vecDest;
		m_Route[iNumToCopy].iType       = bits_MF_TO_LOCATION | bits_MF_IS_GOAL;
	}

	return TRUE;
}

//=========================================================
// FindHintNode
//=========================================================
int CBaseMonster ::FindHintNode( void )
{
	int i;
	TraceResult tr;

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet || WorldGraph.m_cNodes <= 0 || !WorldGraph.m_pNodes )
	{
		ALERT( at_aiconsole, "find_hintnode: graph not ready!\n" );
		return NO_NODE;
	}

	if ( WorldGraph.m_iLastActiveIdleSearch >= WorldGraph.m_cNodes )
	{
		WorldGraph.m_iLastActiveIdleSearch = 0;
	}

	for ( i = 0; i < WorldGraph.m_cNodes; i++ )
	{
		int nodeNumber = ( i + WorldGraph.m_iLastActiveIdleSearch ) % WorldGraph.m_cNodes;
		CNode &node    = WorldGraph.Node( nodeNumber );

		if ( node.m_sHintType )
		{
			// this node has a hint. Take it if it is visible, the monster likes it, and the monster has an animation to match the hint's activity.
			if ( FValidateHintType( node.m_sHintType ) )
			{
				if ( !node.m_sHintActivity || LookupActivity( node.m_sHintActivity ) != ACTIVITY_NOT_AVAILABLE )
				{
					UTIL_TraceLine( pev->origin + pev->view_ofs, node.m_vecOrigin + pev->view_ofs, ignore_monsters, ENT( pev ), &tr );

					if ( tr.flFraction == 1.0 )
					{
						WorldGraph.m_iLastActiveIdleSearch = nodeNumber + 1; // next monster that searches for hint nodes will start where we left off.
						return nodeNumber;                                   // take it!
					}
				}
			}
		}
	}

	WorldGraph.m_iLastActiveIdleSearch = 0; // start at the top of the list for the next search.

	return NO_NODE;
}

BOOL CBaseMonster ::FindLateralCover( const Vector &vecThreat, const Vector &vecViewOffset )
{
	TraceResult tr;
	Vector vecBestOnLeft;
	Vector vecBestOnRight;
	Vector vecLeftTest;
	Vector vecRightTest;
	Vector vecStepRight;
	int i;

	UTIL_MakeVectors( pev->angles );
	vecStepRight   = gpGlobals->v_right * COVER_DELTA;
	vecStepRight.z = 0;

	vecLeftTest = vecRightTest = pev->origin;

	for ( i = 0; i < COVER_CHECKS; i++ )
	{
		vecLeftTest  = vecLeftTest - vecStepRight;
		vecRightTest = vecRightTest + vecStepRight;

		// it's faster to check the SightEnt's visibility to the potential spot than to check the local move, so we do that first.
		UTIL_TraceLine( vecThreat + vecViewOffset, vecLeftTest + pev->view_ofs, ignore_monsters, ignore_glass, ENT( pev ) /*pentIgnore*/, &tr );

		if ( tr.flFraction != 1.0 )
		{
			if ( FValidateCover( vecLeftTest ) && CheckLocalMove( pev->origin, vecLeftTest, NULL, NULL ) == LOCALMOVE_VALID )
			{
				if ( MoveToLocation( ACT_RUN, 0, vecLeftTest ) )
				{
					return TRUE;
				}
			}
		}

		// it's faster to check the SightEnt's visibility to the potential spot than to check the local move, so we do that first.
		UTIL_TraceLine( vecThreat + vecViewOffset, vecRightTest + pev->view_ofs, ignore_monsters, ignore_glass, ENT( pev ) /*pentIgnore*/, &tr );

		if ( tr.flFraction != 1.0 )
		{
			if ( FValidateCover( vecRightTest ) && CheckLocalMove( pev->origin, vecRightTest, NULL, NULL ) == LOCALMOVE_VALID )
			{
				if ( MoveToLocation( ACT_RUN, 0, vecRightTest ) )
				{
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}
