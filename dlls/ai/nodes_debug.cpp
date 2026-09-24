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
//=========================================================
// nodes_debug.cpp - AI node graph validation, debugging, and visualization tools.
//=========================================================

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/monsters.h"
#include "ai/nodes.h"
#include "core/animation.h"

#if !defined( _WIN32 )
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h> // mkdir
#endif

#ifdef _LINUX
#include <unistd.h>
#define CreateDirectory( p, n ) mkdir( p, 0777 )
#endif

extern DLL_GLOBAL edict_t *g_pBodyQueueHead;

//=========================================================
// CGraph - ShowNodeConnections - draws a line from the given node
// to all connected nodes
//=========================================================
void CGraph ::ShowNodeConnections( int iNode )
{
	Vector vecSpot;
	CNode *pNode;
	CNode *pLinkNode;
	int i;

	if ( !m_fGraphPresent || !m_fGraphPointersSet )
	{
		ALERT( at_aiconsole, "Graph not ready!\n" );
		return;
	}

	if ( iNode < 0 )
	{
		ALERT( at_aiconsole, "Can't show connections for node %d\n", iNode );
		return;
	}

	pNode = &m_pNodes[iNode];

	UTIL_ParticleEffect( pNode->m_vecOrigin, g_vecZero, 255, 20 );

	if ( pNode->m_cNumLinks <= 0 )
	{
		ALERT( at_aiconsole, "**No Connections!\n" );
	}

	for ( i = 0; i < pNode->m_cNumLinks; i++ )
	{
		pLinkNode = &Node( NodeLink( iNode, i ).m_iDestNode );
		vecSpot   = pLinkNode->m_vecOrigin;

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_SHOWLINE );

		WRITE_COORD( m_pNodes[iNode].m_vecOrigin.x );
		WRITE_COORD( m_pNodes[iNode].m_vecOrigin.y );
		WRITE_COORD( m_pNodes[iNode].m_vecOrigin.z + NODE_HEIGHT );

		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z + NODE_HEIGHT );
		MESSAGE_END();
	}
}

LINK_ENTITY_TO_CLASS( testhull, CTestHull );

//=========================================================
// CTestHull::Spawn
//=========================================================
void CTestHull ::Spawn( entvars_t *pevMasterNode )
{
	SET_MODEL( ENT( pev ), "models/player.mdl" );
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid     = SOLID_SLIDEBOX;
	pev->movetype  = MOVETYPE_STEP;
	pev->effects   = 0;
	pev->health    = 50;
	pev->yaw_speed = 8;

	if ( WorldGraph.m_fGraphPresent )
	{
		SetThink( &CTestHull::SUB_Remove );
		pev->nextthink = gpGlobals->time;
	}
	else
	{
		SetThink( &CTestHull::DropDelay );
		pev->nextthink = gpGlobals->time + 1;
	}

	pev->rendermode = kRenderTransTexture;
	pev->renderamt  = 0;
}

//=========================================================
// TestHull::DropDelay - spawns TestHull on top of
// the 0th node and drops it to the ground.
//=========================================================
void CTestHull::DropDelay( void )
{
	UTIL_SetOrigin( VARS( pev ), WorldGraph.m_pNodes[0].m_vecOrigin );

	SetThink( &CTestHull::CallBuildNodeGraph );

	pev->nextthink = gpGlobals->time + 1;
}

//=========================================================
// CTestHull - ShowBadNode - makes a bad node fizzle. When
// there's a problem with node graph generation, the test
// hull will be placed up the bad node's location and will generate
// particles
//=========================================================
void CTestHull ::ShowBadNode( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->angles.y = pev->angles.y + 4;

	UTIL_MakeVectors( pev->angles );

	UTIL_ParticleEffect( pev->origin, g_vecZero, 255, 25 );
	UTIL_ParticleEffect( pev->origin + gpGlobals->v_forward * 64, g_vecZero, 255, 25 );
	UTIL_ParticleEffect( pev->origin - gpGlobals->v_forward * 64, g_vecZero, 255, 25 );
	UTIL_ParticleEffect( pev->origin + gpGlobals->v_right * 64, g_vecZero, 255, 25 );
	UTIL_ParticleEffect( pev->origin - gpGlobals->v_right * 64, g_vecZero, 255, 25 );

	pev->nextthink = gpGlobals->time + 0.1;
}

extern BOOL gTouchDisabled;
void CTestHull::CallBuildNodeGraph( void )
{
	gTouchDisabled = TRUE;
	BuildNodeGraph();
	gTouchDisabled = FALSE;
}

//=========================================================
// BuildNodeGraph - think function called by the empty walk
// hull that is spawned by the first node to spawn.
//=========================================================
void CTestHull ::BuildNodeGraph( void )
{
	TraceResult tr;
	FILE *file;

	char szNrpFilename[MAX_PATH];

	CLink *pTempPool;

	CNode *pSrcNode;
	CNode *pDestNode;

	BOOL fSkipRemainingHulls;
	BOOL fPairsValid;

	int i, j, hull;

	int iBadNode;

	int cMaxInitialLinks = 0;
	int cMaxValidLinks   = 0;

	int iPoolIndex = 0;
	int cPoolLinks;

	Vector vecDirToCheckNode;
	Vector vecDirToTestNode;
	Vector vecStepCheckDir;
	Vector vecTraceSpot;
	Vector vecSpot;

	Vector2D vec2DirToCheckNode;
	Vector2D vec2DirToTestNode;
	Vector2D vec2StepCheckDir;
	Vector2D vec2TraceSpot;
	Vector2D vec2Spot;

	float flYaw;
	float flDist;
	int step;

	SetThink( &CTestHull::SUB_Remove );
	pev->nextthink = gpGlobals->time;

	pTempPool = (CLink *)calloc( sizeof( CLink ), ( WorldGraph.m_cNodes * MAX_NODE_INITIAL_LINKS ) );
	if ( !pTempPool )
	{
		ALERT( at_aiconsole, "**Could not malloc TempPool!\n" );
		return;
	}

	// make sure directories have been made
	GET_GAME_DIR( szNrpFilename );
	strcat( szNrpFilename, "/maps" );
#ifdef _WIN32
	CreateDirectory( szNrpFilename, NULL );
#else
	mkdir( szNrpFilename, 0755 );
#endif
	strcat( szNrpFilename, "/graphs" );
#ifdef _WIN32
	CreateDirectory( szNrpFilename, NULL );
#else
	mkdir( szNrpFilename, 0755 );
#endif

	strcat( szNrpFilename, "/" );
	strcat( szNrpFilename, STRING( gpGlobals->mapname ) );
	strcat( szNrpFilename, ".nrp" );

	file = fopen( szNrpFilename, "w+" );

	if ( !file )
	{ // file error
		ALERT( at_aiconsole, "Couldn't create %s!\n", szNrpFilename );

		if ( pTempPool )
		{
			free( pTempPool );
		}

		return;
	}

	fprintf( file, "Node Graph Report for map:  %s.bsp\n", STRING( gpGlobals->mapname ) );
	fprintf( file, "%d Total Nodes\n\n", WorldGraph.m_cNodes );

	for ( i = 0; i < WorldGraph.m_cNodes; i++ )
	{ // print all node numbers and their locations to the file.
		WorldGraph.m_pNodes[i].m_cNumLinks  = 0;
		WorldGraph.m_pNodes[i].m_iFirstLink = 0;
		memset( WorldGraph.m_pNodes[i].m_pNextBestNode, 0, sizeof( WorldGraph.m_pNodes[i].m_pNextBestNode ) );

		fprintf( file, "Node#         %4d\n", i );
		fprintf( file, "Location      %4d,%4d,%4d\n", (int)WorldGraph.m_pNodes[i].m_vecOrigin.x, (int)WorldGraph.m_pNodes[i].m_vecOrigin.y, (int)WorldGraph.m_pNodes[i].m_vecOrigin.z );
		fprintf( file, "HintType:     %4d\n", WorldGraph.m_pNodes[i].m_sHintType );
		fprintf( file, "HintActivity: %4d\n", WorldGraph.m_pNodes[i].m_sHintActivity );
		fprintf( file, "HintYaw:      %4f\n", WorldGraph.m_pNodes[i].m_flHintYaw );
		fprintf( file, "-------------------------------------------------------------------------------\n" );
	}
	fprintf( file, "\n\n" );

	// Automatically recognize WATER nodes and drop the LAND nodes to the floor.
	//
	for ( i = 0; i < WorldGraph.m_cNodes; i++ )
	{
		if ( WorldGraph.m_pNodes[i].m_afNodeInfo & bits_NODE_AIR )
		{
			// do nothing
		}
		else if ( UTIL_PointContents( WorldGraph.m_pNodes[i].m_vecOrigin ) == CONTENTS_WATER )
		{
			WorldGraph.m_pNodes[i].m_afNodeInfo |= bits_NODE_WATER;
		}
		else
		{
			WorldGraph.m_pNodes[i].m_afNodeInfo |= bits_NODE_LAND;

			// trace to the ground, then pop up 8 units and place node there to make it
			// easier for them to connect (think stairs, chairs, and bumps in the floor).
			// After the routing is done, push them back down.
			//
			TraceResult tr;

			UTIL_TraceLine( WorldGraph.m_pNodes[i].m_vecOrigin,
			                WorldGraph.m_pNodes[i].m_vecOrigin - Vector( 0, 0, 384 ),
			                ignore_monsters,
			                g_pBodyQueueHead, //!!!HACKHACK no real ent to supply here, using a global we don't care about
			                &tr );

			// This trace is ONLY used if we hit an entity flagged with FL_WORLDBRUSH
			TraceResult trEnt;
			UTIL_TraceLine( WorldGraph.m_pNodes[i].m_vecOrigin,
			                WorldGraph.m_pNodes[i].m_vecOrigin - Vector( 0, 0, 384 ),
			                dont_ignore_monsters,
			                g_pBodyQueueHead, //!!!HACKHACK no real ent to supply here, using a global we don't care about
			                &trEnt );

			// Did we hit something closer than the floor?
			if ( trEnt.flFraction < tr.flFraction )
			{
				// If it was a world brush entity, copy the node location
				if ( trEnt.pHit && ( trEnt.pHit->v.flags & FL_WORLDBRUSH ) )
					tr.vecEndPos = trEnt.vecEndPos;
			}

			WorldGraph.m_pNodes[i].m_vecOriginPeek.z =
			    WorldGraph.m_pNodes[i].m_vecOrigin.z = tr.vecEndPos.z + NODE_HEIGHT;
		}
	}

	cPoolLinks = WorldGraph.LinkVisibleNodes( pTempPool, file, &iBadNode );

	if ( !cPoolLinks )
	{
		ALERT( at_aiconsole, "**ConnectVisibleNodes FAILED!\n" );

		SetThink( &CTestHull::ShowBadNode ); // send the hull off to show the offending node.
		// pev->solid = SOLID_NOT;
		pev->origin = WorldGraph.m_pNodes[iBadNode].m_vecOrigin;

		if ( pTempPool )
		{
			free( pTempPool );
		}

		if ( file )
		{ // close the file
			fclose( file );
		}

		return;
	}

	// send the walkhull to all of this node's connections now. We'll do this here since
	// so much of it relies on being able to control the test hull.
	if ( file )
	{
		fprintf( file, "----------------------------------------------------------------------------\n" );
		fprintf( file, "Walk Rejection:\n" );
	}

	for ( i = 0; i < WorldGraph.m_cNodes; i++ )
	{
		pSrcNode = &WorldGraph.m_pNodes[i];

		if ( file )
		{
			fprintf( file, "-------------------------------------------------------------------------------\n" );
			fprintf( file, "Node %4d:\n\n", i );
		}

		for ( j = 0; j < pSrcNode->m_cNumLinks; j++ )
		{
			// assume that all hulls can walk this link, then eliminate the ones that can't.
			pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo = bits_LINK_SMALL_HULL | bits_LINK_HUMAN_HULL | bits_LINK_LARGE_HULL | bits_LINK_FLY_HULL;

			// do a check for each hull size.

			// if we can't fit a tiny hull through a connection, no other hulls with fit either, so we
			// should just fall out of the loop. Do so by setting the SkipRemainingHulls flag.
			fSkipRemainingHulls = FALSE;
			for ( hull = 0; hull < MAX_NODE_HULLS; hull++ )
			{
				if ( fSkipRemainingHulls && ( hull == NODE_HUMAN_HULL || hull == NODE_LARGE_HULL ) ) // skip the remaining walk hulls
					continue;

				switch ( hull )
				{
				case NODE_SMALL_HULL:
					UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );
					break;
				case NODE_HUMAN_HULL:
					UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
					break;
				case NODE_LARGE_HULL:
					UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 64 ) );
					break;
				case NODE_FLY_HULL:
					UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 64 ) );
					break;
				}

				UTIL_SetOrigin( pev, pSrcNode->m_vecOrigin ); // place the hull on the node

				if ( !FBitSet( pev->flags, FL_ONGROUND ) )
				{
					ALERT( at_aiconsole, "OFFGROUND!\n" );
				}

				// now build a yaw that points to the dest node, and get the distance.
				if ( j < 0 )
				{
					ALERT( at_aiconsole, "**** j = %d ****\n", j );
					if ( pTempPool )
					{
						free( pTempPool );
					}

					if ( file )
					{ // close the file
						fclose( file );
					}
					return;
				}

				pDestNode = &WorldGraph.m_pNodes[pTempPool[pSrcNode->m_iFirstLink + j].m_iDestNode];

				vecSpot = pDestNode->m_vecOrigin;

				if ( hull < NODE_FLY_HULL )
				{
					int SaveFlags = pev->flags;
					int MoveMode  = WALKMOVE_WORLDONLY;
					if ( pSrcNode->m_afNodeInfo & bits_NODE_WATER )
					{
						pev->flags |= FL_SWIM;
						MoveMode = WALKMOVE_NORMAL;
					}

					flYaw = UTIL_VecToYaw( pDestNode->m_vecOrigin - pev->origin );

					flDist = ( vecSpot - pev->origin ).Length2D();

					int fWalkFailed = FALSE;

					// in this loop we take tiny steps from the current node to the nodes that it links to, one at a time.
					for ( step = 0; step < flDist && !fWalkFailed; step += HULL_STEP_SIZE )
					{
						float stepSize = HULL_STEP_SIZE;

						if ( ( step + stepSize ) >= ( flDist - 1 ) )
							stepSize = ( flDist - step ) - 1;

						if ( !WALK_MOVE( ENT( pev ), flYaw, stepSize, MoveMode ) )
						{ // can't take the next step

							fWalkFailed = TRUE;
							break;
						}
					}

					if ( !fWalkFailed && ( pev->origin - vecSpot ).Length() > 64 )
					{
						fWalkFailed = TRUE;
					}

					if ( fWalkFailed )
					{
						// now me must eliminate the hull that couldn't walk this connection
						switch ( hull )
						{
						case NODE_SMALL_HULL: // if this hull can't fit, nothing can, so drop the connection
							if ( file )
								fprintf( file, "NODE_SMALL_HULL step %f\n", (float)step );
							pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo &= ~( bits_LINK_SMALL_HULL | bits_LINK_HUMAN_HULL | bits_LINK_LARGE_HULL );
							fSkipRemainingHulls = TRUE; // don't bother checking larger hulls
							break;
						case NODE_HUMAN_HULL:
							if ( file )
								fprintf( file, "NODE_HUMAN_HULL step %f\n", (float)step );
							pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo &= ~( bits_LINK_HUMAN_HULL | bits_LINK_LARGE_HULL );
							fSkipRemainingHulls = TRUE; // don't bother checking larger hulls
							break;
						case NODE_LARGE_HULL:
							if ( file )
								fprintf( file, "NODE_LARGE_HULL step %f\n", (float)step );
							pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo &= ~bits_LINK_LARGE_HULL;
							break;
						}
					}
					pev->flags = SaveFlags;
				}
				else
				{
					TraceResult tr;

					UTIL_TraceHull( pSrcNode->m_vecOrigin + Vector( 0, 0, 32 ), pDestNode->m_vecOriginPeek + Vector( 0, 0, 32 ), ignore_monsters, large_hull, ENT( pev ), &tr );
					if ( tr.fStartSolid || tr.flFraction < 1.0 )
					{
						pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo &= ~bits_LINK_FLY_HULL;
					}
				}
			}

			if ( pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo == 0 )
			{
				if ( file )
				{
					fprintf( file, "Rejected Node %3d - Unreachable by Any Hull\n", pTempPool[pSrcNode->m_iFirstLink + j].m_iDestNode );
				}
				pTempPool[pSrcNode->m_iFirstLink + j] = pTempPool[pSrcNode->m_iFirstLink + ( pSrcNode->m_cNumLinks - 1 )];

				pSrcNode->m_cNumLinks--;
				cPoolLinks--; // we just removed a link, so decrement the total number of links in the pool.
				j--;
			}
		}
	}

	if ( file )
	{
		fprintf( file, "-------------------------------------------------------------------------------\n\n\n" );
	}

	cPoolLinks -= WorldGraph.RejectInlineLinks( pTempPool, file );

	// now malloc a pool just large enough to hold the links that are actually used
	WorldGraph.m_pLinkPool = (CLink *)calloc( sizeof( CLink ), cPoolLinks );

	if ( !WorldGraph.m_pLinkPool )
	{ // couldn't make the link pool!
		ALERT( at_aiconsole, "Couldn't malloc LinkPool!\n" );
		if ( pTempPool )
		{
			free( pTempPool );
		}
		if ( file )
		{ // close the file
			fclose( file );
		}

		return;
	}
	WorldGraph.m_cLinks = cPoolLinks;

	// copy only the used portions of the TempPool into the graph's link pool
	int iFinalPoolIndex = 0;
	int iOldFirstLink;

	for ( i = 0; i < WorldGraph.m_cNodes; i++ )
	{
		iOldFirstLink = WorldGraph.m_pNodes[i].m_iFirstLink; // store this, because we have to re-assign it before entering the copy loop

		WorldGraph.m_pNodes[i].m_iFirstLink = iFinalPoolIndex;

		for ( j = 0; j < WorldGraph.m_pNodes[i].m_cNumLinks; j++ )
		{
			WorldGraph.m_pLinkPool[iFinalPoolIndex++] = pTempPool[iOldFirstLink + j];
		}
	}

	// Node sorting numbers linked nodes close to each other
	//
	WorldGraph.SortNodes();

	// This is used for HashSearch
	//
	WorldGraph.BuildLinkLookups();

	fPairsValid = TRUE; // assume that the connection pairs are all valid to start

	if ( file )
	{
		fprintf( file, "\n\n-------------------------------------------------------------------------------\n" );
		fprintf( file, "Link Pairings:\n" );
	}

	// link integrity check. The idea here is that if Node A links to Node B, node B should
	// link to node A. If not, we have a situation that prevents us from using a basic
	// optimization in the FindNearestLink function.
	for ( i = 0; i < WorldGraph.m_cNodes; i++ )
	{
		for ( j = 0; j < WorldGraph.m_pNodes[i].m_cNumLinks; j++ )
		{
			int iLink;
			WorldGraph.HashSearch( WorldGraph.INodeLink( i, j ), i, iLink );
			if ( iLink < 0 )
			{
				fPairsValid = FALSE; // unmatched link pair.
				if ( file )
				{
					fprintf( file, "WARNING: Node %3d does not connect back to Node %3d\n", WorldGraph.INodeLink( i, j ), i );
				}
			}
		}
	}

	// !!!LATER - if all connections are properly paired, when can enable an optimization in the pathfinding code
	// (in the find nearest line function)
	if ( fPairsValid )
	{
		if ( file )
		{
			fprintf( file, "\nAll Connections are Paired!\n" );
		}
	}

	if ( file )
	{
		fprintf( file, "-------------------------------------------------------------------------------\n" );
		fprintf( file, "\n\n-------------------------------------------------------------------------------\n" );
		fprintf( file, "Total Number of Connections in Pool: %d\n", cPoolLinks );
		fprintf( file, "-------------------------------------------------------------------------------\n" );
		fprintf( file, "Connection Pool: %d bytes\n", (int)( sizeof( CLink ) * cPoolLinks ) );
		fprintf( file, "-------------------------------------------------------------------------------\n" );
	}

	ALERT( at_aiconsole, "%d Nodes, %d Connections\n", WorldGraph.m_cNodes, cPoolLinks );

	// This is used for FindNearestNode
	//
	WorldGraph.BuildRegionTables();

	// Push all of the LAND nodes down to the ground now. Leave the water and air nodes alone.
	//
	for ( i = 0; i < WorldGraph.m_cNodes; i++ )
	{
		if ( ( WorldGraph.m_pNodes[i].m_afNodeInfo & bits_NODE_LAND ) )
		{
			WorldGraph.m_pNodes[i].m_vecOrigin.z -= NODE_HEIGHT;
		}
	}

	if ( pTempPool )
	{ // free the temp pool
		free( pTempPool );
	}

	if ( file )
	{
		fclose( file );
	}

	// We now have some graphing capabilities.
	//
	WorldGraph.m_fGraphPresent     = TRUE;  // graph is in memory.
	WorldGraph.m_fGraphPointersSet = TRUE;  // since the graph was generated, the pointers are ready
	WorldGraph.m_fRoutingComplete  = FALSE; // Optimal routes aren't computed, yet.

	// Compute and compress the routing information.
	//
	WorldGraph.ComputeStaticRoutingTables();

	// save the node graph for this level
	WorldGraph.FSaveGraph( (char *)STRING( gpGlobals->mapname ) );
	ALERT( at_console, "Done.\n" );
}

//=========================================================
// returns a hardcoded path.
//=========================================================
void CTestHull ::PathFind( void )
{
	int iPath[50];
	int iPathSize;
	int i;
	CNode *pNode, *pNextNode;

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet )
	{
		ALERT( at_aiconsole, "Graph not ready!\n" );
		return;
	}

	iPathSize = WorldGraph.FindShortestPath( iPath, 0, 19, 0, 0 );

	if ( !iPathSize )
	{
		ALERT( at_aiconsole, "No Path!\n" );
		return;
	}

	ALERT( at_aiconsole, "%d\n", iPathSize );

	pNode = &WorldGraph.m_pNodes[iPath[0]];

	for ( i = 0; i < iPathSize - 1; i++ )
	{
		pNextNode = &WorldGraph.m_pNodes[iPath[i + 1]];

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_SHOWLINE );

		WRITE_COORD( pNode->m_vecOrigin.x );
		WRITE_COORD( pNode->m_vecOrigin.y );
		WRITE_COORD( pNode->m_vecOrigin.z + NODE_HEIGHT );

		WRITE_COORD( pNextNode->m_vecOrigin.x );
		WRITE_COORD( pNextNode->m_vecOrigin.y );
		WRITE_COORD( pNextNode->m_vecOrigin.z + NODE_HEIGHT );
		MESSAGE_END();

		pNode = pNextNode;
	}
}

//=========================================================
// CNodeViewer - Draws a graph of the shorted path from all nodes
// to current location (typically the player).  It then draws
// as many connects as it can per frame, trying not to overflow the buffer
//=========================================================
class CNodeViewer : public CBaseEntity
{
  public:
	void Spawn( void );

	int m_iBaseNode;
	int m_iDraw;
	int m_nVisited;
	int m_aFrom[128];
	int m_aTo[128];
	int m_iHull;
	int m_afNodeType;
	Vector m_vecColor;

	void FindNodeConnections( int iNode );
	void AddNode( int iFrom, int iTo );
	void EXPORT DrawThink( void );
};
LINK_ENTITY_TO_CLASS( node_viewer, CNodeViewer );
LINK_ENTITY_TO_CLASS( node_viewer_human, CNodeViewer );
LINK_ENTITY_TO_CLASS( node_viewer_fly, CNodeViewer );
LINK_ENTITY_TO_CLASS( node_viewer_large, CNodeViewer );

void CNodeViewer::Spawn()
{
	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet )
	{
		ALERT( at_console, "Graph not ready!\n" );
		UTIL_Remove( this );
		return;
	}

	if ( FClassnameIs( pev, "node_viewer_fly" ) )
	{
		m_iHull      = NODE_FLY_HULL;
		m_afNodeType = bits_NODE_AIR;
		m_vecColor   = Vector( 160, 100, 255 );
	}
	else if ( FClassnameIs( pev, "node_viewer_large" ) )
	{
		m_iHull      = NODE_LARGE_HULL;
		m_afNodeType = bits_NODE_LAND | bits_NODE_WATER;
		m_vecColor   = Vector( 100, 255, 160 );
	}
	else
	{
		m_iHull      = NODE_HUMAN_HULL;
		m_afNodeType = bits_NODE_LAND | bits_NODE_WATER;
		m_vecColor   = Vector( 255, 160, 100 );
	}

	m_iBaseNode = WorldGraph.FindNearestNode( pev->origin, m_afNodeType );

	if ( m_iBaseNode < 0 )
	{
		ALERT( at_console, "No nearby node\n" );
		return;
	}

	m_nVisited = 0;

	ALERT( at_aiconsole, "basenode %d\n", m_iBaseNode );

	if ( WorldGraph.m_cNodes < 128 )
	{
		for ( int i = 0; i < WorldGraph.m_cNodes; i++ )
		{
			AddNode( i, WorldGraph.NextNodeInRoute( i, m_iBaseNode, m_iHull, 0 ) );
		}
	}
	else
	{
		FindNodeConnections( m_iBaseNode );

		int start = 0;
		int end;
		do
		{
			end = m_nVisited;
			for ( end = m_nVisited; start < end; start++ )
			{
				FindNodeConnections( m_aFrom[start] );
				FindNodeConnections( m_aTo[start] );
			}
		} while ( end != m_nVisited );
	}

	ALERT( at_aiconsole, "%d nodes\n", m_nVisited );

	m_iDraw = 0;
	SetThink( &CNodeViewer::DrawThink );
	pev->nextthink = gpGlobals->time;
}

void CNodeViewer ::FindNodeConnections( int iNode )
{
	AddNode( iNode, WorldGraph.NextNodeInRoute( iNode, m_iBaseNode, m_iHull, 0 ) );
	for ( int i = 0; i < WorldGraph.m_pNodes[iNode].m_cNumLinks; i++ )
	{
		CLink *pToLink = &WorldGraph.NodeLink( iNode, i );
		AddNode( pToLink->m_iDestNode, WorldGraph.NextNodeInRoute( pToLink->m_iDestNode, m_iBaseNode, m_iHull, 0 ) );
	}
}

void CNodeViewer::AddNode( int iFrom, int iTo )
{
	if ( m_nVisited >= 128 )
	{
		return;
	}
	else
	{
		if ( iFrom == iTo )
			return;

		for ( int i = 0; i < m_nVisited; i++ )
		{
			if ( m_aFrom[i] == iFrom && m_aTo[i] == iTo )
				return;
			if ( m_aFrom[i] == iTo && m_aTo[i] == iFrom )
				return;
		}
		m_aFrom[m_nVisited] = iFrom;
		m_aTo[m_nVisited]   = iTo;
		m_nVisited++;
	}
}

void CNodeViewer ::DrawThink( void )
{
	pev->nextthink = gpGlobals->time;

	for ( int i = 0; i < 10; i++ )
	{
		if ( m_iDraw == m_nVisited )
		{
			UTIL_Remove( this );
			return;
		}

		extern short g_sModelIndexLaser;
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( WorldGraph.m_pNodes[m_aFrom[m_iDraw]].m_vecOrigin.x );
		WRITE_COORD( WorldGraph.m_pNodes[m_aFrom[m_iDraw]].m_vecOrigin.y );
		WRITE_COORD( WorldGraph.m_pNodes[m_aFrom[m_iDraw]].m_vecOrigin.z + NODE_HEIGHT );

		WRITE_COORD( WorldGraph.m_pNodes[m_aTo[m_iDraw]].m_vecOrigin.x );
		WRITE_COORD( WorldGraph.m_pNodes[m_aTo[m_iDraw]].m_vecOrigin.y );
		WRITE_COORD( WorldGraph.m_pNodes[m_aTo[m_iDraw]].m_vecOrigin.z + NODE_HEIGHT );
		WRITE_SHORT( g_sModelIndexLaser );
		WRITE_BYTE( 0 );            // framerate
		WRITE_BYTE( 0 );            // framerate
		WRITE_BYTE( 250 );          // life
		WRITE_BYTE( 40 );           // width
		WRITE_BYTE( 0 );            // noise
		WRITE_BYTE( m_vecColor.x ); // r, g, b
		WRITE_BYTE( m_vecColor.y ); // r, g, b
		WRITE_BYTE( m_vecColor.z ); // r, g, b
		WRITE_BYTE( 128 );          // brightness
		WRITE_BYTE( 0 );            // speed
		MESSAGE_END();

		m_iDraw++;
	}
}
