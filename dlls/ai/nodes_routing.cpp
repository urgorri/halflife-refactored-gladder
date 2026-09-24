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
// nodes_routing.cpp - AI pathfinding, routing computation, and nearest-node lookup.
//=========================================================

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/monsters.h"
#include "ai/nodes.h"
#include "core/animation.h"


int CGraph::HullIndex( const CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->pev )
		return NODE_HUMAN_HULL;

	if ( pEntity->pev->movetype == MOVETYPE_FLY )
		return NODE_FLY_HULL;

	if ( pEntity->pev->mins == Vector( -12, -12, 0 ) )
		return NODE_SMALL_HULL;
	else if ( pEntity->pev->mins == VEC_HUMAN_HULL_MIN )
		return NODE_HUMAN_HULL;
	else if ( pEntity->pev->mins == Vector( -32, -32, 0 ) )
		return NODE_LARGE_HULL;

	return NODE_HUMAN_HULL;
}

int CGraph::NodeType( const CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->pev )
		return bits_NODE_LAND;

	if ( pEntity->pev->movetype == MOVETYPE_FLY )
	{
		if ( pEntity->pev->waterlevel != 0 )
		{
			return bits_NODE_WATER;
		}
		else
		{
			return bits_NODE_AIR;
		}
	}
	return bits_NODE_LAND;
}

// Sum up graph weights on the path from iStart to iDest to determine path length
float CGraph::PathLength( int iStart, int iDest, int iHull, int afCapMask )
{
	if ( !m_fGraphPresent || !m_fGraphPointersSet || !m_fRoutingComplete || !m_pRouteInfo || !m_pNodes || m_cNodes <= 0 )
	{
		return 0;
	}

	if ( iStart < 0 || iStart >= m_cNodes || iDest < 0 || iDest >= m_cNodes )
	{
		return 0;
	}

	float distance = 0;
	int iNext;

	int iMaxLoop = m_cNodes;

	int iCurrentNode = iStart;
	int iCap         = CapIndex( afCapMask );

	while ( iCurrentNode != iDest )
	{
		if ( iMaxLoop-- <= 0 )
		{
			ALERT( at_console, "Route Failure\n" );
			return 0;
		}

		iNext = NextNodeInRoute( iCurrentNode, iDest, iHull, iCap );
		if ( iCurrentNode == iNext || iNext < 0 || iNext >= m_cNodes )
		{
			return 0;
		}

		int iLink;
		HashSearch( iCurrentNode, iNext, iLink );
		if ( iLink < 0 || iLink >= m_cLinks || !m_pLinkPool )
		{
			ALERT( at_console, "HashLinks is broken from %d to %d.\n", iCurrentNode, iDest );
			return 0;
		}
		CLink &link = Link( iLink );
		distance += link.m_flWeight;

		iCurrentNode = iNext;
	}

	return distance;
}

// Parse the routing table at iCurrentNode for the next node on the shortest path to iDest
int CGraph::NextNodeInRoute( int iCurrentNode, int iDest, int iHull, int iCap )
{
	if ( !m_fGraphPresent || !m_fGraphPointersSet || !m_fRoutingComplete || !m_pRouteInfo || !m_pNodes || m_cNodes <= 0 )
	{
		return iCurrentNode;
	}

	if ( iCurrentNode < 0 || iCurrentNode >= m_cNodes || iDest < 0 || iDest >= m_cNodes )
	{
		return iCurrentNode;
	}

	if ( iHull < 0 || iHull >= MAX_NODE_HULLS || iCap < 0 || iCap >= 2 )
	{
		return iCurrentNode;
	}

	int offset = m_pNodes[iCurrentNode].m_pNextBestNode[iHull][iCap];
	if ( offset < 0 || offset >= m_nRouteInfo )
	{
		return iCurrentNode;
	}

	int iNext       = iCurrentNode;
	int nCount      = iDest + 1;
	char *pRoute    = m_pRouteInfo + offset;
	char *pRouteEnd = m_pRouteInfo + m_nRouteInfo;

	// Until we decode the next best node
	//
	while ( nCount > 0 && pRoute < pRouteEnd )
	{
		char ch = *pRoute++;
		if ( ch < 0 )
		{
			// Sequence phrase
			//
			ch = -ch;
			if ( nCount <= ch )
			{
				iNext  = iDest;
				nCount = 0;
			}
			else
			{
				nCount = nCount - ch;
			}
		}
		else
		{
			// Repeat phrase
			//
			if ( pRoute >= pRouteEnd )
			{
				break;
			}
			if ( nCount <= ch + 1 )
			{
				iNext = iCurrentNode + *pRoute;
				if ( iNext >= m_cNodes )
					iNext -= m_cNodes;
				else if ( iNext < 0 )
					iNext += m_cNodes;
				nCount = 0;
			}
			else
			{
				nCount = nCount - ch - 1;
			}
			pRoute++;
		}
	}

	if ( iNext < 0 || iNext >= m_cNodes )
	{
		return iCurrentNode;
	}

	return iNext;
}

//=========================================================
// CGraph - FindShortestPath
//
// accepts a capability mask (afCapMask), and will only
// find a path usable by a monster with those capabilities
// returns the number of nodes copied into supplied array
//=========================================================
int CGraph ::FindShortestPath( int *piPath, int iStart, int iDest, int iHull, int afCapMask )
{
	int iVisitNode;
	int iCurrentNode;
	int iNumPathNodes;
	int iHullMask;

	if ( !m_fGraphPresent || !m_fGraphPointersSet || m_cNodes <= 0 || !m_pNodes )
	{ // protect us in the case that the node graph isn't available or built
		ALERT( at_aiconsole, "Graph not ready!\n" );
		return FALSE;
	}

	if ( iStart < 0 || iStart >= m_cNodes || iDest < 0 || iDest >= m_cNodes )
	{ // The start or dest node is bad?
		ALERT( at_aiconsole, "Can't build a path, invalid iStart (%d) or iDest (%d)!\n", iStart, iDest );
		return FALSE;
	}

	if ( iStart == iDest )
	{
		piPath[0] = iStart;
		piPath[1] = iDest;
		return 2;
	}

	// Is routing information present.
	//
	if ( m_fRoutingComplete )
	{
		int iCap = CapIndex( afCapMask );

		iNumPathNodes           = 0;
		piPath[iNumPathNodes++] = iStart;
		iCurrentNode            = iStart;
		int iNext;

		// Until we arrive at the destination
		//
		while ( iCurrentNode != iDest )
		{
			iNext = NextNodeInRoute( iCurrentNode, iDest, iHull, iCap );
			if ( iCurrentNode == iNext || iNext < 0 || iNext >= m_cNodes )
			{
				return 0;
			}
			if ( iNumPathNodes >= MAX_PATH_SIZE )
			{
				break;
			}
			piPath[iNumPathNodes++] = iNext;
			iCurrentNode            = iNext;
		}
	}
	else
	{
		CQueuePriority queue;

		switch ( iHull )
		{
		case NODE_SMALL_HULL:
			iHullMask = bits_LINK_SMALL_HULL;
			break;
		case NODE_HUMAN_HULL:
			iHullMask = bits_LINK_HUMAN_HULL;
			break;
		case NODE_LARGE_HULL:
			iHullMask = bits_LINK_LARGE_HULL;
			break;
		case NODE_FLY_HULL:
			iHullMask = bits_LINK_FLY_HULL;
			break;
		}

		// Mark all the nodes as unvisited.
		//
		int i;
		for ( i = 0; i < m_cNodes; i++ )
		{
			m_pNodes[i].m_flClosestSoFar = -1.0;
		}

		m_pNodes[iStart].m_flClosestSoFar = 0.0;
		m_pNodes[iStart].m_iPreviousNode  = iStart; // tag this as the origin node
		queue.Insert( iStart, 0.0 );                // insert start node

		while ( !queue.Empty() )
		{
			// now pull a node out of the queue
			float flCurrentDistance;
			iCurrentNode = queue.Remove( flCurrentDistance );

			if ( iCurrentNode == iDest )
				break;

			CNode *pCurrentNode = &m_pNodes[iCurrentNode];

			for ( i = 0; i < pCurrentNode->m_cNumLinks; i++ )
			{ // run through all of this node's neighbors

				iVisitNode = INodeLink( iCurrentNode, i );
				if ( ( m_pLinkPool[m_pNodes[iCurrentNode].m_iFirstLink + i].m_afLinkInfo & iHullMask ) != iHullMask )
				{ // monster is too large to walk this connection
					continue;
				}
				// check the connection from the current node to the node we're about to mark visited and push into the queue
				if ( m_pLinkPool[m_pNodes[iCurrentNode].m_iFirstLink + i].m_pLinkEnt != NULL )
				{ // there's a brush ent in the way! Don't mark this node or put it into the queue unless the monster can negotiate it

					if ( !HandleLinkEnt( iCurrentNode, m_pLinkPool[m_pNodes[iCurrentNode].m_iFirstLink + i].m_pLinkEnt, afCapMask, NODEGRAPH_STATIC ) )
					{ // monster should not try to go this way.
						continue;
					}
				}
				float flOurDistance = flCurrentDistance + m_pLinkPool[m_pNodes[iCurrentNode].m_iFirstLink + i].m_flWeight;
				if ( m_pNodes[iVisitNode].m_flClosestSoFar < -0.5 || flOurDistance < m_pNodes[iVisitNode].m_flClosestSoFar - 0.001 )
				{
					m_pNodes[iVisitNode].m_flClosestSoFar = flOurDistance;
					m_pNodes[iVisitNode].m_iPreviousNode  = iCurrentNode;

					queue.Insert( iVisitNode, flOurDistance );
				}
			}
		}
		if ( m_pNodes[iDest].m_flClosestSoFar < -0.5 )
		{ // Destination is unreachable, no path found.
			return 0;
		}

		// now we must walk backwards through the m_iPreviousNode field, and count how many connections there are in the path
		iCurrentNode  = iDest;
		iNumPathNodes = 1; // count the dest

		while ( iCurrentNode != iStart && iNumPathNodes < m_cNodes )
		{
			if ( iCurrentNode < 0 || iCurrentNode >= m_cNodes )
				return 0;
			int iPrev = m_pNodes[iCurrentNode].m_iPreviousNode;
			if ( iPrev == iCurrentNode || iPrev < 0 || iPrev >= m_cNodes )
				return 0;
			iNumPathNodes++;
			iCurrentNode = iPrev;
		}

		if ( iCurrentNode != iStart )
		{
			return 0;
		}

		iCurrentNode = iDest;
		for ( i = iNumPathNodes - 1; i >= 0; i-- )
		{
			if ( i < MAX_PATH_SIZE )
			{
				piPath[i] = iCurrentNode;
			}
			if ( iCurrentNode >= 0 && iCurrentNode < m_cNodes )
				iCurrentNode = m_pNodes[iCurrentNode].m_iPreviousNode;
		}

		if ( iNumPathNodes > MAX_PATH_SIZE )
		{
			iNumPathNodes = MAX_PATH_SIZE;
		}
	}

	return iNumPathNodes;
}

static inline ULONG Hash( void *p, int len )
{
	CRC32_t ulCrc;
	CRC32_INIT( &ulCrc );
	CRC32_PROCESS_BUFFER( &ulCrc, p, len );
	return CRC32_FINAL( ulCrc );
}

void CGraph ::CheckNode( Vector vecOrigin, int iNode )
{
	if ( !m_pNodes || !m_di || iNode < 0 || iNode >= m_cNodes )
		return;

	// Have we already seen this point before?.
	//
	if ( m_di[iNode].m_CheckedEvent == m_CheckedCounter )
		return;
	m_di[iNode].m_CheckedEvent = m_CheckedCounter;

	float flDist = ( vecOrigin - m_pNodes[iNode].m_vecOriginPeek ).Length();

	if ( flDist < m_flShortest )
	{
		TraceResult tr;

		// make sure that vecOrigin can trace to this node!
		UTIL_TraceLine( vecOrigin, m_pNodes[iNode].m_vecOriginPeek, ignore_monsters, 0, &tr );

		if ( tr.flFraction == 1.0 )
		{
			m_iNearest   = iNode;
			m_flShortest = flDist;

			UpdateRange( m_minX, m_maxX, CALC_RANGE( vecOrigin.x, m_RegionMin[0], m_RegionMax[0] ), m_pNodes[iNode].m_Region[0] );
			UpdateRange( m_minY, m_maxY, CALC_RANGE( vecOrigin.y, m_RegionMin[1], m_RegionMax[1] ), m_pNodes[iNode].m_Region[1] );
			UpdateRange( m_minZ, m_maxZ, CALC_RANGE( vecOrigin.z, m_RegionMin[2], m_RegionMax[2] ), m_pNodes[iNode].m_Region[2] );

			// From maxCircle, calculate maximum bounds box. All points must be
			// simultaneously inside all bounds of the box.
			//
			m_minBoxX = CALC_RANGE( vecOrigin.x - flDist, m_RegionMin[0], m_RegionMax[0] );
			m_maxBoxX = CALC_RANGE( vecOrigin.x + flDist, m_RegionMin[0], m_RegionMax[0] );
			m_minBoxY = CALC_RANGE( vecOrigin.y - flDist, m_RegionMin[1], m_RegionMax[1] );
			m_maxBoxY = CALC_RANGE( vecOrigin.y + flDist, m_RegionMin[1], m_RegionMax[1] );
			m_minBoxZ = CALC_RANGE( vecOrigin.z - flDist, m_RegionMin[2], m_RegionMax[2] );
			m_maxBoxZ = CALC_RANGE( vecOrigin.z + flDist, m_RegionMin[2], m_RegionMax[2] );
		}
	}
}

//=========================================================
// CGraph - FindNearestNode - returns the index of the node nearest
// the given vector -1 is failure (couldn't find a valid
// near node )
//=========================================================
int CGraph ::FindNearestNode( const Vector &vecOrigin, CBaseEntity *pEntity )
{
	return FindNearestNode( vecOrigin, NodeType( pEntity ) );
}

int CGraph ::FindNearestNode( const Vector &vecOrigin, int afNodeTypes )
{
	int i;

	if ( !m_fGraphPresent || !m_fGraphPointersSet || m_cNodes <= 0 || !m_pNodes || !m_di )
	{ // protect us in the case that the node graph isn't available
		ALERT( at_aiconsole, "Graph not ready!\n" );
		return -1;
	}

	// Check with the cache
	//
	ULONG iHash = ( CACHE_SIZE - 1 ) & Hash( (void *)(const float *)vecOrigin, sizeof( vecOrigin ) );
	if ( m_Cache[iHash].v == vecOrigin )
	{
		return m_Cache[iHash].n;
	}

	// Mark all points as unchecked.
	//
	m_CheckedCounter++;
	if ( m_CheckedCounter == 0 )
	{
		for ( int iNode = 0; iNode < m_cNodes; iNode++ )
		{
			m_di[iNode].m_CheckedEvent = 0;
		}
		m_CheckedCounter++;
	}

	m_iNearest   = -1;
	m_flShortest = 999999.0; // just a big number.

	m_minX    = 0;
	m_maxX    = 255;
	m_minY    = 0;
	m_maxY    = 255;
	m_minZ    = 0;
	m_maxZ    = 255;
	m_minBoxX = 0;
	m_maxBoxX = 255;
	m_minBoxY = 0;
	m_maxBoxY = 255;
	m_minBoxZ = 0;
	m_maxBoxZ = 255;

	int halfX = ( m_minX + m_maxX ) / 2;
	int halfY = ( m_minY + m_maxY ) / 2;
	int halfZ = ( m_minZ + m_maxZ ) / 2;

	int j;

	for ( i = halfX; i >= m_minX; i-- )
	{
		for ( j = m_RangeStart[0][i]; j <= m_RangeEnd[0][i]; j++ )
		{
			if ( j < 0 || j >= m_cNodes )
				continue;
			int nodeIdx = m_di[j].m_SortedBy[0];
			if ( nodeIdx < 0 || nodeIdx >= m_cNodes )
				continue;
			if ( !( m_pNodes[nodeIdx].m_afNodeInfo & afNodeTypes ) )
				continue;

			int rgY = m_pNodes[nodeIdx].m_Region[1];
			if ( rgY > m_maxBoxY )
				break;
			if ( rgY < m_minBoxY )
				continue;

			int rgZ = m_pNodes[nodeIdx].m_Region[2];
			if ( rgZ < m_minBoxZ )
				continue;
			if ( rgZ > m_maxBoxZ )
				continue;
			CheckNode( vecOrigin, nodeIdx );
		}
	}

	for ( i = max( m_minY, halfY + 1 ); i <= m_maxY; i++ )
	{
		for ( j = m_RangeStart[1][i]; j <= m_RangeEnd[1][i]; j++ )
		{
			if ( j < 0 || j >= m_cNodes )
				continue;
			int nodeIdx = m_di[j].m_SortedBy[1];
			if ( nodeIdx < 0 || nodeIdx >= m_cNodes )
				continue;
			if ( !( m_pNodes[nodeIdx].m_afNodeInfo & afNodeTypes ) )
				continue;

			int rgZ = m_pNodes[nodeIdx].m_Region[2];
			if ( rgZ > m_maxBoxZ )
				break;
			if ( rgZ < m_minBoxZ )
				continue;
			int rgX = m_pNodes[nodeIdx].m_Region[0];
			if ( rgX < m_minBoxX )
				continue;
			if ( rgX > m_maxBoxX )
				continue;
			CheckNode( vecOrigin, nodeIdx );
		}
	}

	for ( i = min( m_maxZ, halfZ ); i >= m_minZ; i-- )
	{
		for ( j = m_RangeStart[2][i]; j <= m_RangeEnd[2][i]; j++ )
		{
			if ( j < 0 || j >= m_cNodes )
				continue;
			int nodeIdx = m_di[j].m_SortedBy[2];
			if ( nodeIdx < 0 || nodeIdx >= m_cNodes )
				continue;
			if ( !( m_pNodes[nodeIdx].m_afNodeInfo & afNodeTypes ) )
				continue;

			int rgX = m_pNodes[nodeIdx].m_Region[0];
			if ( rgX > m_maxBoxX )
				break;
			if ( rgX < m_minBoxX )
				continue;
			int rgY = m_pNodes[nodeIdx].m_Region[1];
			if ( rgY < m_minBoxY )
				continue;
			if ( rgY > m_maxBoxY )
				continue;
			CheckNode( vecOrigin, nodeIdx );
		}
	}

	for ( i = max( m_minX, halfX + 1 ); i <= m_maxX; i++ )
	{
		for ( j = m_RangeStart[0][i]; j <= m_RangeEnd[0][i]; j++ )
		{
			if ( j < 0 || j >= m_cNodes )
				continue;
			int nodeIdx = m_di[j].m_SortedBy[0];
			if ( nodeIdx < 0 || nodeIdx >= m_cNodes )
				continue;
			if ( !( m_pNodes[nodeIdx].m_afNodeInfo & afNodeTypes ) )
				continue;

			int rgY = m_pNodes[nodeIdx].m_Region[1];
			if ( rgY > m_maxBoxY )
				break;
			if ( rgY < m_minBoxY )
				continue;

			int rgZ = m_pNodes[nodeIdx].m_Region[2];
			if ( rgZ < m_minBoxZ )
				continue;
			if ( rgZ > m_maxBoxZ )
				continue;
			CheckNode( vecOrigin, nodeIdx );
		}
	}

	for ( i = min( m_maxY, halfY ); i >= m_minY; i-- )
	{
		for ( j = m_RangeStart[1][i]; j <= m_RangeEnd[1][i]; j++ )
		{
			if ( j < 0 || j >= m_cNodes )
				continue;
			int nodeIdx = m_di[j].m_SortedBy[1];
			if ( nodeIdx < 0 || nodeIdx >= m_cNodes )
				continue;
			if ( !( m_pNodes[nodeIdx].m_afNodeInfo & afNodeTypes ) )
				continue;

			int rgZ = m_pNodes[nodeIdx].m_Region[2];
			if ( rgZ > m_maxBoxZ )
				break;
			if ( rgZ < m_minBoxZ )
				continue;
			int rgX = m_pNodes[nodeIdx].m_Region[0];
			if ( rgX < m_minBoxX )
				continue;
			if ( rgX > m_maxBoxX )
				continue;
			CheckNode( vecOrigin, nodeIdx );
		}
	}

	for ( i = max( m_minZ, halfZ + 1 ); i <= m_maxZ; i++ )
	{
		for ( j = m_RangeStart[2][i]; j <= m_RangeEnd[2][i]; j++ )
		{
			if ( j < 0 || j >= m_cNodes )
				continue;
			int nodeIdx = m_di[j].m_SortedBy[2];
			if ( nodeIdx < 0 || nodeIdx >= m_cNodes )
				continue;
			if ( !( m_pNodes[nodeIdx].m_afNodeInfo & afNodeTypes ) )
				continue;

			int rgX = m_pNodes[nodeIdx].m_Region[0];
			if ( rgX > m_maxBoxX )
				break;
			if ( rgX < m_minBoxX )
				continue;
			int rgY = m_pNodes[nodeIdx].m_Region[1];
			if ( rgY < m_minBoxY )
				continue;
			if ( rgY > m_maxBoxY )
				continue;
			CheckNode( vecOrigin, nodeIdx );
		}
	}

	m_Cache[iHash].v = vecOrigin;
	m_Cache[iHash].n = m_iNearest;
	return m_iNearest;
}

void CGraph ::ComputeStaticRoutingTables( void )
{
	int nRoutes = m_cNodes * m_cNodes;
#define FROM_TO( x, y ) ( ( x ) * m_cNodes + ( y ) )
	short *Routes = new short[nRoutes];

	int *pMyPath                  = new int[m_cNodes];
	unsigned short *BestNextNodes = new unsigned short[m_cNodes];
	char *pRoute                  = new char[m_cNodes * 2];

	if ( Routes && pMyPath && BestNextNodes && pRoute )
	{
		int nTotalCompressedSize = 0;
		for ( int iHull = 0; iHull < MAX_NODE_HULLS; iHull++ )
		{
			for ( int iCap = 0; iCap < 2; iCap++ )
			{
				int iCapMask;
				switch ( iCap )
				{
				case 0:
					iCapMask = 0;
					break;

				case 1:
					iCapMask = bits_CAP_OPEN_DOORS | bits_CAP_AUTO_DOORS | bits_CAP_USE;
					break;
				}

				// Initialize Routing table to uncalculated.
				//
				int iFrom;
				for ( iFrom = 0; iFrom < m_cNodes; iFrom++ )
				{
					for ( int iTo = 0; iTo < m_cNodes; iTo++ )
					{
						Routes[FROM_TO( iFrom, iTo )] = -1;
					}
				}

				for ( iFrom = 0; iFrom < m_cNodes; iFrom++ )
				{
					for ( int iTo = m_cNodes - 1; iTo >= 0; iTo-- )
					{
						if ( Routes[FROM_TO( iFrom, iTo )] != -1 )
							continue;

						int cPathSize = FindShortestPath( pMyPath, iFrom, iTo, iHull, iCapMask );

						// Use the computed path to update the routing table.
						//
						if ( cPathSize > 1 )
						{
							for ( int iNode = 0; iNode < cPathSize - 1; iNode++ )
							{
								int iStart = pMyPath[iNode];
								int iNext  = pMyPath[iNode + 1];
								for ( int iNode1 = iNode + 1; iNode1 < cPathSize; iNode1++ )
								{
									int iEnd                        = pMyPath[iNode1];
									Routes[FROM_TO( iStart, iEnd )] = iNext;
								}
							}
						}
						else
						{
							Routes[FROM_TO( iFrom, iTo )] = iFrom;
							Routes[FROM_TO( iTo, iFrom )] = iTo;
						}
					}
				}

				for ( iFrom = 0; iFrom < m_cNodes; iFrom++ )
				{
					for ( int iTo = 0; iTo < m_cNodes; iTo++ )
					{
						BestNextNodes[iTo] = Routes[FROM_TO( iFrom, iTo )];
					}

					// Compress this node's routing table.
					//
					int iLastNode      = 9999999; // just really big.
					int cSequence      = 0;
					int cRepeats       = 0;
					int CompressedSize = 0;
					char *p            = pRoute;
					for ( int i = 0; i < m_cNodes; i++ )
					{
						BOOL CanRepeat   = ( ( BestNextNodes[i] == iLastNode ) && cRepeats < 127 );
						BOOL CanSequence = ( BestNextNodes[i] == i && cSequence < 128 );

						if ( cRepeats )
						{
							if ( CanRepeat )
							{
								cRepeats++;
							}
							else
							{
								// Emit the repeat phrase.
								//
								CompressedSize += 2; // (count-1, iLastNode-i)
								*p++  = cRepeats - 1;
								int a = iLastNode - iFrom;
								int b = iLastNode - iFrom + m_cNodes;
								int c = iLastNode - iFrom - m_cNodes;
								if ( -128 <= a && a <= 127 )
								{
									*p++ = a;
								}
								else if ( -128 <= b && b <= 127 )
								{
									*p++ = b;
								}
								else if ( -128 <= c && c <= 127 )
								{
									*p++ = c;
								}
								else
								{
									ALERT( at_aiconsole, "Nodes need sorting (%d,%d)!\n", iLastNode, iFrom );
								}
								cRepeats = 0;

								if ( CanSequence )
								{
									cSequence++;
								}
								else
								{
									cRepeats++;
								}
							}
						}
						else if ( cSequence )
						{
							if ( CanSequence )
							{
								cSequence++;
							}
							else
							{
								if ( cSequence == 1 && CanRepeat )
								{
									cRepeats  = 2;
									cSequence = 0;
								}
								else
								{
									CompressedSize += 1; // (-count)
									*p++      = -cSequence;
									cSequence = 0;
									cRepeats++;
								}
							}
						}
						else
						{
							if ( CanSequence )
							{
								cSequence++;
							}
							else
							{
								cRepeats++;
							}
						}
						iLastNode = BestNextNodes[i];
					}
					if ( cRepeats )
					{
						CompressedSize += 2;
						*p++  = cRepeats - 1;
						int a = iLastNode - iFrom;
						int b = iLastNode - iFrom + m_cNodes;
						int c = iLastNode - iFrom - m_cNodes;
						if ( -128 <= a && a <= 127 )
						{
							*p++ = a;
						}
						else if ( -128 <= b && b <= 127 )
						{
							*p++ = b;
						}
						else if ( -128 <= c && c <= 127 )
						{
							*p++ = c;
						}
						else
						{
							ALERT( at_aiconsole, "Nodes need sorting (%d,%d)!\n", iLastNode, iFrom );
						}
					}
					if ( cSequence )
					{
						CompressedSize += 1;
						*p++ = -cSequence;
					}

					int nRoute = p - pRoute;
					if ( m_pRouteInfo )
					{
						int i;
						for ( i = 0; i < m_nRouteInfo - nRoute; i++ )
						{
							if ( memcmp( m_pRouteInfo + i, pRoute, nRoute ) == 0 )
							{
								break;
							}
						}
						if ( i < m_nRouteInfo - nRoute )
						{
							m_pNodes[iFrom].m_pNextBestNode[iHull][iCap] = i;
						}
						else
						{
							char *Tmp = (char *)calloc( sizeof( char ), ( m_nRouteInfo + nRoute ) );
							memcpy( Tmp, m_pRouteInfo, m_nRouteInfo );
							free( m_pRouteInfo );
							m_pRouteInfo = Tmp;
							memcpy( m_pRouteInfo + m_nRouteInfo, pRoute, nRoute );
							m_pNodes[iFrom].m_pNextBestNode[iHull][iCap] = m_nRouteInfo;
							m_nRouteInfo += nRoute;
							nTotalCompressedSize += CompressedSize;
						}
					}
					else
					{
						m_nRouteInfo = nRoute;
						m_pRouteInfo = (char *)calloc( sizeof( char ), nRoute );
						memcpy( m_pRouteInfo, pRoute, nRoute );
						m_pNodes[iFrom].m_pNextBestNode[iHull][iCap] = 0;
						nTotalCompressedSize += CompressedSize;
					}
				}
			}
		}
		ALERT( at_aiconsole, "Size of Routes = %d\n", nTotalCompressedSize );
	}
	if ( Routes )
		delete[] Routes;
	if ( BestNextNodes )
		delete[] BestNextNodes;
	if ( pRoute )
		delete[] pRoute;
	if ( pMyPath )
		delete[] pMyPath;
	Routes        = 0;
	BestNextNodes = 0;
	pRoute        = 0;
	pMyPath       = 0;

	m_fRoutingComplete = TRUE;
}

void CGraph ::TestRoutingTables( void )
{
	int *pMyPath  = new int[m_cNodes];
	int *pMyPath2 = new int[m_cNodes];
	if ( pMyPath && pMyPath2 )
	{
		for ( int iHull = 0; iHull < MAX_NODE_HULLS; iHull++ )
		{
			for ( int iCap = 0; iCap < 2; iCap++ )
			{
				int iCapMask;
				switch ( iCap )
				{
				case 0:
					iCapMask = 0;
					break;

				case 1:
					iCapMask = bits_CAP_OPEN_DOORS | bits_CAP_AUTO_DOORS | bits_CAP_USE;
					break;
				}

				for ( int iFrom = 0; iFrom < m_cNodes; iFrom++ )
				{
					for ( int iTo = 0; iTo < m_cNodes; iTo++ )
					{
						m_fRoutingComplete = FALSE;
						int cPathSize1     = FindShortestPath( pMyPath, iFrom, iTo, iHull, iCapMask );
						m_fRoutingComplete = TRUE;
						int cPathSize2     = FindShortestPath( pMyPath2, iFrom, iTo, iHull, iCapMask );

						if ( cPathSize2 == MAX_PATH_SIZE )
							continue;

						float flDistance1 = 0.0;
						int i;
						for ( i = 0; i < cPathSize1 - 1; i++ )
						{
							if ( pMyPath[i] == pMyPath[i + 1] )
								continue;
							int iVisitNode;
							BOOL bFound = FALSE;
							for ( int iLink = 0; iLink < m_pNodes[pMyPath[i]].m_cNumLinks; iLink++ )
							{
								iVisitNode = INodeLink( pMyPath[i], iLink );
								if ( iVisitNode == pMyPath[i + 1] )
								{
									flDistance1 += m_pLinkPool[m_pNodes[pMyPath[i]].m_iFirstLink + iLink].m_flWeight;
									bFound = TRUE;
									break;
								}
							}
							if ( !bFound )
							{
								ALERT( at_aiconsole, "No link.\n" );
							}
						}

						float flDistance2 = 0.0;
						for ( i = 0; i < cPathSize2 - 1; i++ )
						{
							if ( pMyPath2[i] == pMyPath2[i + 1] )
								continue;
							int iVisitNode;
							BOOL bFound = FALSE;
							for ( int iLink = 0; iLink < m_pNodes[pMyPath2[i]].m_cNumLinks; iLink++ )
							{
								iVisitNode = INodeLink( pMyPath2[i], iLink );
								if ( iVisitNode == pMyPath2[i + 1] )
								{
									flDistance2 += m_pLinkPool[m_pNodes[pMyPath2[i]].m_iFirstLink + iLink].m_flWeight;
									bFound = TRUE;
									break;
								}
							}
							if ( !bFound )
							{
								ALERT( at_aiconsole, "No link.\n" );
							}
						}
						if ( fabs( flDistance1 - flDistance2 ) > 0.10 )
						{
							ALERT( at_aiconsole, "Routing is inconsistent!!!\n" );
							ALERT( at_aiconsole, "(%d to %d |%d/%d)1:", iFrom, iTo, iHull, iCap );
							for ( int k = 0; k < cPathSize1; k++ )
							{
								ALERT( at_aiconsole, "%d ", pMyPath[k] );
							}
							ALERT( at_aiconsole, "\n(%d to %d |%d/%d)2:", iFrom, iTo, iHull, iCap );
							for ( i = 0; i < cPathSize2; i++ )
							{
								ALERT( at_aiconsole, "%d ", pMyPath2[i] );
							}
							ALERT( at_aiconsole, "\n" );
							m_fRoutingComplete = FALSE;
							cPathSize1         = FindShortestPath( pMyPath, iFrom, iTo, iHull, iCapMask );
							m_fRoutingComplete = TRUE;
							cPathSize2         = FindShortestPath( pMyPath2, iFrom, iTo, iHull, iCapMask );
							goto EnoughSaid;
						}
					}
				}
			}
		}
	}

EnoughSaid:

	if ( pMyPath )
		delete[] pMyPath;
	if ( pMyPath2 )
		delete[] pMyPath2;
	pMyPath  = 0;
	pMyPath2 = 0;
}
