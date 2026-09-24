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
// nodes_graph.cpp - AI node graph management, allocation, and serialization.
//=========================================================

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/monsters.h"
#include "ai/nodes.h"
#include "core/animation.h"
#include "systems/doors.h"

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

CGraph WorldGraph;

LINK_ENTITY_TO_CLASS( info_node, CNodeEnt );
LINK_ENTITY_TO_CLASS( info_node_air, CNodeEnt );

//=========================================================
// CGraph - InitGraph - prepares the graph for use. Frees any
// memory currently in use by the world graph, NULLs
// all pointers, and zeros the node count.
//=========================================================
void CGraph ::InitGraph( void )
{
	// Make the graph unavailable
	//
	m_fGraphPresent     = FALSE;
	m_fGraphPointersSet = FALSE;
	m_fRoutingComplete  = FALSE;

	// Free the link pool
	//
	if ( m_pLinkPool )
	{
		free( m_pLinkPool );
		m_pLinkPool = NULL;
	}

	// Free the node info
	//
	if ( m_pNodes )
	{
		free( m_pNodes );
		m_pNodes = NULL;
	}

	if ( m_di )
	{
		free( m_di );
		m_di = NULL;
	}

	// Free the routing info.
	//
	if ( m_pRouteInfo )
	{
		free( m_pRouteInfo );
		m_pRouteInfo = NULL;
	}

	if ( m_pHashLinks )
	{
		free( m_pHashLinks );
		m_pHashLinks = NULL;
	}

	// Zero node and link counts
	//
	m_cNodes     = 0;
	m_cLinks     = 0;
	m_nRouteInfo = 0;

	m_iLastActiveIdleSearch = 0;
	m_iLastCoverSearch      = 0;
}

//=========================================================
// CGraph - AllocNodes - temporary function that mallocs a
// reasonable number of nodes so we can build the path which
// will be saved to disk.
//=========================================================
int CGraph ::AllocNodes( void )
{
	//  malloc all of the nodes
	WorldGraph.m_pNodes = (CNode *)calloc( sizeof( CNode ), MAX_NODES );

	// could not malloc space for all the nodes!
	if ( !WorldGraph.m_pNodes )
	{
		ALERT( at_aiconsole, "**ERROR**\nCouldn't malloc %d nodes!\n", WorldGraph.m_cNodes );
		return FALSE;
	}

	return TRUE;
}

//=========================================================
// nodes start out as ents in the world. As they are spawned,
// the node info is recorded then the ents are discarded.
//=========================================================
void CNodeEnt ::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "hinttype" ) )
	{
		m_sHintType    = (short)atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}

	if ( FStrEq( pkvd->szKeyName, "activity" ) )
	{
		m_sHintActivity = (short)atoi( pkvd->szValue );
		pkvd->fHandled  = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

//=========================================================
//=========================================================
void CNodeEnt ::Spawn( void )
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid    = SOLID_NOT; // always solid_not

	if ( WorldGraph.m_fGraphPresent )
	{ // graph loaded from disk, so discard all these node ents as soon as they spawn
		REMOVE_ENTITY( edict() );
		return;
	}

	if ( WorldGraph.m_cNodes == 0 )
	{ // this is the first node to spawn, spawn the test hull entity that builds and walks the node tree
		CTestHull *pHull = GetClassPtr( (CTestHull *)NULL );
		pHull->Spawn( pev );
	}

	if ( WorldGraph.m_cNodes >= MAX_NODES )
	{
		ALERT( at_aiconsole, "cNodes > MAX_NODES\n" );
		return;
	}

	WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_vecOriginPeek =
	    WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_vecOrigin = pev->origin;
	WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_flHintYaw     = pev->angles.y;
	WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_sHintType     = m_sHintType;
	WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_sHintActivity = m_sHintActivity;

	if ( FClassnameIs( pev, "info_node_air" ) )
		WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_afNodeInfo = bits_NODE_AIR;
	else
		WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_afNodeInfo = 0;

	WorldGraph.m_cNodes++;

	REMOVE_ENTITY( edict() );
}

//=========================================================
// CStack Constructor
//=========================================================
CStack ::CStack( void )
{
	m_level = 0;
}

//=========================================================
// pushes a value onto the stack
//=========================================================
void CStack ::Push( int value )
{
	if ( m_level >= MAX_STACK_NODES )
	{
		printf( "Error!\n" );
		return;
	}
	m_stack[m_level] = value;
	m_level++;
}

//=========================================================
// pops a value off of the stack
//=========================================================
int CStack ::Pop( void )
{
	if ( m_level <= 0 )
		return -1;

	m_level--;
	return m_stack[m_level];
}

//=========================================================
// returns the value on the top of the stack
//=========================================================
int CStack ::Top( void )
{
	return m_stack[m_level - 1];
}

//=========================================================
// copies every element on the stack into an array LIFO
//=========================================================
void CStack ::CopyToArray( int *piArray )
{
	int i;

	for ( i = 0; i < m_level; i++ )
	{
		piArray[i] = m_stack[i];
	}
}

//=========================================================
// CQueue constructor
//=========================================================
CQueue ::CQueue( void )
{
	m_cSize = 0;
	m_head  = 0;
	m_tail  = -1;
}

//=========================================================
// inserts a value into the queue
//=========================================================
void CQueue ::Insert( int iValue, float fPriority )
{
	if ( Full() )
	{
		printf( "Queue is full!\n" );
		return;
	}

	m_tail++;

	if ( m_tail == MAX_STACK_NODES )
	{ // wrap around
		m_tail = 0;
	}

	m_queue[m_tail].Id       = iValue;
	m_queue[m_tail].Priority = fPriority;
	m_cSize++;
}

//=========================================================
// removes a value from the queue (FIFO)
//=========================================================
int CQueue ::Remove( float &fPriority )
{
	if ( m_head == MAX_STACK_NODES )
	{ // wrap
		m_head = 0;
	}

	m_cSize--;
	fPriority = m_queue[m_head].Priority;
	return m_queue[m_head++].Id;
}

//=========================================================
// CQueue constructor
//=========================================================
CQueuePriority ::CQueuePriority( void )
{
	m_cSize = 0;
}

//=========================================================
// inserts a value into the priority queue
//=========================================================
void CQueuePriority ::Insert( int iValue, float fPriority )
{
	if ( Full() )
	{
		printf( "Queue is full!\n" );
		return;
	}

	m_heap[m_cSize].Priority = fPriority;
	m_heap[m_cSize].Id       = iValue;
	m_cSize++;
	Heap_SiftUp();
}

//=========================================================
// removes the smallest item from the priority queue
//
//=========================================================
int CQueuePriority ::Remove( float &fPriority )
{
	int iReturn = m_heap[0].Id;
	fPriority   = m_heap[0].Priority;

	m_cSize--;

	m_heap[0] = m_heap[m_cSize];

	Heap_SiftDown( 0 );
	return iReturn;
}

#define HEAP_LEFT_CHILD( x ) ( 2 * ( x ) + 1 )
#define HEAP_RIGHT_CHILD( x ) ( 2 * ( x ) + 2 )
#define HEAP_PARENT( x ) ( ( ( x ) - 1 ) / 2 )

void CQueuePriority::Heap_SiftDown( int iSubRoot )
{
	int parent = iSubRoot;
	int child  = HEAP_LEFT_CHILD( parent );

	struct tag_HEAP_NODE Ref = m_heap[parent];

	while ( child < m_cSize )
	{
		int rightchild = HEAP_RIGHT_CHILD( parent );
		if ( rightchild < m_cSize )
		{
			if ( m_heap[rightchild].Priority < m_heap[child].Priority )
			{
				child = rightchild;
			}
		}
		if ( Ref.Priority <= m_heap[child].Priority )
			break;

		m_heap[parent] = m_heap[child];
		parent         = child;
		child          = HEAP_LEFT_CHILD( parent );
	}
	m_heap[parent] = Ref;
}

void CQueuePriority::Heap_SiftUp( void )
{
	int child = m_cSize - 1;
	while ( child )
	{
		int parent = HEAP_PARENT( child );
		if ( m_heap[parent].Priority <= m_heap[child].Priority )
			break;

		struct tag_HEAP_NODE Tmp;
		Tmp            = m_heap[child];
		m_heap[child]  = m_heap[parent];
		m_heap[parent] = Tmp;

		child = parent;
	}
}

//=========================================================
// CGraph - FLoadGraph - attempts to load a node graph from disk.
// if the current level is maps/snar.bsp, maps/graphs/snar.nod
// will be loaded. If file cannot be loaded, the node tree
// will be created and saved to disk.
//=========================================================
int CGraph ::FLoadGraph( char *szMapName )
{
	char szFilename[MAX_PATH];
	int iVersion;
	int length;
	byte *aMemFile;
	byte *pMemFile;

	// make sure the directories have been made
	char szDirName[MAX_PATH];
	GET_GAME_DIR( szDirName );
	strcat( szDirName, "/maps" );
#ifdef _WIN32
	CreateDirectory( szDirName, NULL );
#else
	mkdir( szDirName, 0755 );
#endif
	strcat( szDirName, "/graphs" );
#ifdef _WIN32
	CreateDirectory( szDirName, NULL );
#else
	mkdir( szDirName, 0755 );
#endif

	strcpy( szFilename, "maps/graphs/" );
	strcat( szFilename, szMapName );
	strcat( szFilename, ".nod" );

	pMemFile = aMemFile = LOAD_FILE_FOR_ME( szFilename, &length );

	if ( !aMemFile )
	{
		return FALSE;
	}
	else
	{
		// Read the graph version number
		//
		length -= sizeof( int );
		if ( length < 0 )
			goto ShortFile;
		memcpy( &iVersion, pMemFile, sizeof( int ) );
		pMemFile += sizeof( int );

		if ( iVersion != GRAPH_VERSION )
		{
			// This file was written by a different build of the dll!
			//
			ALERT( at_aiconsole, "**ERROR** Graph version is %d, expected %d\n", iVersion, GRAPH_VERSION );
			goto ShortFile;
		}

		// Read the graph class
		//
		length -= sizeof( CGraph );
		if ( length < 0 )
			goto ShortFile;
		memcpy( this, pMemFile, sizeof( CGraph ) );
		pMemFile += sizeof( CGraph );

		// Set the pointers to zero, just in case we run out of memory.
		//
		m_pNodes     = NULL;
		m_pLinkPool  = NULL;
		m_di         = NULL;
		m_pRouteInfo = NULL;
		m_pHashLinks = NULL;

		// Malloc for the nodes
		//
		m_pNodes = (CNode *)calloc( sizeof( CNode ), m_cNodes );

		if ( !m_pNodes )
		{
			ALERT( at_aiconsole, "**ERROR**\nCouldn't malloc %d nodes!\n", m_cNodes );
			goto NoMemory;
		}

		// Read in all the nodes
		//
		length -= sizeof( CNode ) * m_cNodes;
		if ( length < 0 )
			goto ShortFile;
		memcpy( m_pNodes, pMemFile, sizeof( CNode ) * m_cNodes );
		pMemFile += sizeof( CNode ) * m_cNodes;

		// Malloc for the link pool
		//
		m_pLinkPool = (CLink *)calloc( sizeof( CLink ), m_cLinks );

		if ( !m_pLinkPool )
		{
			ALERT( at_aiconsole, "**ERROR**\nCouldn't malloc %d link!\n", m_cLinks );
			goto NoMemory;
		}

		// Read in all the links
		//
		length -= sizeof( CLink ) * m_cLinks;
		if ( length < 0 )
			goto ShortFile;
		memcpy( m_pLinkPool, pMemFile, sizeof( CLink ) * m_cLinks );
		pMemFile += sizeof( CLink ) * m_cLinks;

		// Malloc for the sorting info.
		//
		m_di = (DIST_INFO *)calloc( sizeof( DIST_INFO ), m_cNodes );
		if ( !m_di )
		{
			ALERT( at_aiconsole, "***ERROR**\nCouldn't malloc %d entries sorting nodes!\n", m_cNodes );
			goto NoMemory;
		}

		// Read it in.
		//
		length -= sizeof( DIST_INFO ) * m_cNodes;
		if ( length < 0 )
			goto ShortFile;
		memcpy( m_di, pMemFile, sizeof( DIST_INFO ) * m_cNodes );
		pMemFile += sizeof( DIST_INFO ) * m_cNodes;

		// Malloc for the routing info.
		//
		m_fRoutingComplete = FALSE;
		m_pRouteInfo       = (char *)calloc( sizeof( char ), m_nRouteInfo );
		if ( !m_pRouteInfo )
		{
			ALERT( at_aiconsole, "***ERROR**\nCounldn't malloc %d route bytes!\n", m_nRouteInfo );
			goto NoMemory;
		}
		m_CheckedCounter = 0;
		for ( int i = 0; i < m_cNodes; i++ )
		{
			m_di[i].m_CheckedEvent = 0;
		}

		// Read in the route information.
		//
		length -= sizeof( char ) * m_nRouteInfo;
		if ( length < 0 )
			goto ShortFile;
		memcpy( m_pRouteInfo, pMemFile, sizeof( char ) * m_nRouteInfo );
		pMemFile += sizeof( char ) * m_nRouteInfo;
		m_fRoutingComplete = TRUE;

		// malloc for the hash links
		//
		m_pHashLinks = (short *)calloc( sizeof( short ), m_nHashLinks );
		if ( !m_pHashLinks )
		{
			ALERT( at_aiconsole, "***ERROR**\nCounldn't malloc %d hash link bytes!\n", m_nHashLinks );
			goto NoMemory;
		}

		// Read in the hash link information
		//
		length -= sizeof( short ) * m_nHashLinks;
		if ( length < 0 )
			goto ShortFile;
		memcpy( m_pHashLinks, pMemFile, sizeof( short ) * m_nHashLinks );
		pMemFile += sizeof( short ) * m_nHashLinks;

		// Set the graph present flag, clear the pointers set flag
		//
		m_fGraphPresent     = TRUE;
		m_fGraphPointersSet = FALSE;

		FREE_FILE( aMemFile );

		if ( length != 0 )
		{
			ALERT( at_aiconsole, "***WARNING***:Node graph was longer than expected by %d bytes.!\n", length );
		}

		return TRUE;
	}

ShortFile:
NoMemory:
	FREE_FILE( aMemFile );
	return FALSE;
}

//=========================================================
// CGraph - FSaveGraph - It's not rocket science.
// this WILL overwrite existing files.
//=========================================================
int CGraph ::FSaveGraph( char *szMapName )
{
	int iVersion = GRAPH_VERSION;
	char szFilename[MAX_PATH];
	FILE *file;

	if ( !m_fGraphPresent || !m_fGraphPointersSet )
	{ // protect us in the case that the node graph isn't available or built
		ALERT( at_aiconsole, "Graph not ready!\n" );
		return FALSE;
	}

	// make sure directories have been made
	GET_GAME_DIR( szFilename );
	strcat( szFilename, "/maps" );
#ifdef _WIN32
	CreateDirectory( szFilename, NULL );
#else
	mkdir( szFilename, 0755 );
#endif
	strcat( szFilename, "/graphs" );
#ifdef _WIN32
	CreateDirectory( szFilename, NULL );
#else
	mkdir( szFilename, 0755 );
#endif

	strcat( szFilename, "/" );
	strcat( szFilename, szMapName );
	strcat( szFilename, ".nod" );

	file = fopen( szFilename, "wb" );

	ALERT( at_aiconsole, "Created: %s\n", szFilename );

	if ( !file )
	{ // couldn't create
		ALERT( at_aiconsole, "Couldn't Create: %s\n", szFilename );
		return FALSE;
	}
	else
	{
		// write the version
		fwrite( &iVersion, sizeof( int ), 1, file );

		// write the CGraph class
		fwrite( this, sizeof( CGraph ), 1, file );

		// write the nodes
		fwrite( m_pNodes, sizeof( CNode ), m_cNodes, file );

		// write the links
		fwrite( m_pLinkPool, sizeof( CLink ), m_cLinks, file );

		fwrite( m_di, sizeof( DIST_INFO ), m_cNodes, file );

		// Write the route info.
		//
		if ( m_pRouteInfo && m_nRouteInfo )
		{
			fwrite( m_pRouteInfo, sizeof( char ), m_nRouteInfo, file );
		}

		if ( m_pHashLinks && m_nHashLinks )
		{
			fwrite( m_pHashLinks, sizeof( short ), m_nHashLinks, file );
		}
		fclose( file );
		return TRUE;
	}
}

//=========================================================
// CGraph - FSetGraphPointers - Takes the modelnames of
// all of the brush ents that block connections in the node
// graph and resolves them into pointers to those entities.
// this is done after loading the graph from disk, whereupon
// the pointers are not valid.
//=========================================================
int CGraph ::FSetGraphPointers( void )
{
	int i;
	edict_t *pentLinkEnt;

	for ( i = 0; i < m_cLinks; i++ )
	{ // go through all of the links

		if ( m_pLinkPool[i].m_pLinkEnt != NULL )
		{
			char name[5];
			// when graphs are saved, any valid pointers are will be non-zero, signifying that we should
			// reset those pointers upon reloading. Any pointers that were NULL when the graph was saved
			// will be NULL when reloaded, and will ignored by this function.

			// m_szLinkEntModelname is not necessarily NULL terminated (so we can store it in a more alignment-friendly 4 bytes)
			memcpy( name, m_pLinkPool[i].m_szLinkEntModelname, 4 );
			name[4]     = 0;
			pentLinkEnt = FIND_ENTITY_BY_STRING( NULL, "model", name );

			if ( FNullEnt( pentLinkEnt ) )
			{
				// the ent isn't around anymore? Either there is a major problem, or it was removed from the world
				// ( like a func_breakable that's been destroyed or something ). Make sure that LinkEnt is null.
				ALERT( at_aiconsole, "**Could not find model %s\n", name );
				m_pLinkPool[i].m_pLinkEnt = NULL;
			}
			else
			{
				m_pLinkPool[i].m_pLinkEnt = VARS( pentLinkEnt );

				if ( !FBitSet( m_pLinkPool[i].m_pLinkEnt->flags, FL_GRAPHED ) )
				{
					m_pLinkPool[i].m_pLinkEnt->flags += FL_GRAPHED;
				}
			}
		}
	}

	// the pointers are now set.
	m_fGraphPointersSet = TRUE;
	return TRUE;
}

//=========================================================
// CGraph - CheckNODFile - this function checks the date of
// the BSP file that was just loaded and the date of the a
// ssociated .NOD file. If the NOD file is not present, or
// is older than the BSP file, we rebuild it.
//
// returns FALSE if the .NOD file doesn't qualify and needs
// to be rebuilt.
//=========================================================
int CGraph ::CheckNODFile( char *szMapName )
{
	int retValue;

	char szBspFilename[MAX_PATH];
	char szGraphFilename[MAX_PATH];

	strcpy( szBspFilename, "maps/" );
	strcat( szBspFilename, szMapName );
	strcat( szBspFilename, ".bsp" );

	strcpy( szGraphFilename, "maps/graphs/" );
	strcat( szGraphFilename, szMapName );
	strcat( szGraphFilename, ".nod" );

	retValue = TRUE;

	int iCompare;
	if ( COMPARE_FILE_TIME( szBspFilename, szGraphFilename, &iCompare ) )
	{
		if ( iCompare > 0 )
		{ // BSP file is newer.
			ALERT( at_aiconsole, ".NOD File will be updated\n\n" );
			retValue = FALSE;
		}
	}
	else
	{
		retValue = FALSE;
	}

	return retValue;
}

// Renumber nodes so that nodes that link together are together.
//
#define UNNUMBERED_NODE -1
void CGraph::SortNodes( void )
{
	int iNodeCnt = 0;
	int i;
	m_pNodes[0].m_iPreviousNode = iNodeCnt++;

	for ( i = 1; i < m_cNodes; i++ )
	{
		m_pNodes[i].m_iPreviousNode = UNNUMBERED_NODE;
	}

	for ( i = 0; i < m_cNodes; i++ )
	{
		// Run through all of this node's neighbors
		//
		for ( int j = 0; j < m_pNodes[i].m_cNumLinks; j++ )
		{
			int iDestNode = INodeLink( i, j );
			if ( m_pNodes[iDestNode].m_iPreviousNode == UNNUMBERED_NODE )
			{
				m_pNodes[iDestNode].m_iPreviousNode = iNodeCnt++;
			}
		}
	}

	// Assign remaining node numbers to unlinked nodes.
	//
	for ( i = 0; i < m_cNodes; i++ )
	{
		if ( m_pNodes[i].m_iPreviousNode == UNNUMBERED_NODE )
		{
			m_pNodes[i].m_iPreviousNode = iNodeCnt++;
		}
	}

	// Alter links to reflect new node numbers.
	//
	for ( i = 0; i < m_cLinks; i++ )
	{
		m_pLinkPool[i].m_iSrcNode  = m_pNodes[m_pLinkPool[i].m_iSrcNode].m_iPreviousNode;
		m_pLinkPool[i].m_iDestNode = m_pNodes[m_pLinkPool[i].m_iDestNode].m_iPreviousNode;
	}

	// Rearrange nodes to reflect new node numbering.
	//
	for ( i = 0; i < m_cNodes; i++ )
	{
		while ( m_pNodes[i].m_iPreviousNode != i )
		{
			// Move current node off to where it should be, and bring
			// that other node back into the current slot.
			//
			int iDestNode       = m_pNodes[i].m_iPreviousNode;
			CNode TempNode      = m_pNodes[iDestNode];
			m_pNodes[iDestNode] = m_pNodes[i];
			m_pNodes[i]         = TempNode;
		}
	}
}

void CGraph::BuildRegionTables( void )
{
	if ( m_di )
	{
		free( m_di );
		m_di = NULL;
	}

	if ( m_cNodes <= 0 || !m_pNodes )
	{
		return;
	}

	// Go ahead and setup for range searching the nodes for FindNearestNodes
	//
	m_di = (DIST_INFO *)calloc( sizeof( DIST_INFO ), m_cNodes );
	if ( !m_di )
	{
		ALERT( at_aiconsole, "Couldn't allocated node ordering array.\n" );
		return;
	}

	// Calculate regions for all the nodes.
	//
	int i;
	for ( i = 0; i < 3; i++ )
	{
		m_RegionMin[i] = 999999999.0;  // just a big number out there;
		m_RegionMax[i] = -999999999.0; // just a big number out there;
	}
	for ( i = 0; i < m_cNodes; i++ )
	{
		if ( m_pNodes[i].m_vecOrigin.x < m_RegionMin[0] )
			m_RegionMin[0] = m_pNodes[i].m_vecOrigin.x;
		if ( m_pNodes[i].m_vecOrigin.y < m_RegionMin[1] )
			m_RegionMin[1] = m_pNodes[i].m_vecOrigin.y;
		if ( m_pNodes[i].m_vecOrigin.z < m_RegionMin[2] )
			m_RegionMin[2] = m_pNodes[i].m_vecOrigin.z;

		if ( m_pNodes[i].m_vecOrigin.x > m_RegionMax[0] )
			m_RegionMax[0] = m_pNodes[i].m_vecOrigin.x;
		if ( m_pNodes[i].m_vecOrigin.y > m_RegionMax[1] )
			m_RegionMax[1] = m_pNodes[i].m_vecOrigin.y;
		if ( m_pNodes[i].m_vecOrigin.z > m_RegionMax[2] )
			m_RegionMax[2] = m_pNodes[i].m_vecOrigin.z;
	}
	for ( i = 0; i < m_cNodes; i++ )
	{
		m_pNodes[i].m_Region[0] = CALC_RANGE( m_pNodes[i].m_vecOrigin.x, m_RegionMin[0], m_RegionMax[0] );
		m_pNodes[i].m_Region[1] = CALC_RANGE( m_pNodes[i].m_vecOrigin.y, m_RegionMin[1], m_RegionMax[1] );
		m_pNodes[i].m_Region[2] = CALC_RANGE( m_pNodes[i].m_vecOrigin.z, m_RegionMin[2], m_RegionMax[2] );
	}

	for ( i = 0; i < 3; i++ )
	{
		int j;
		for ( j = 0; j < NUM_RANGES; j++ )
		{
			m_RangeStart[i][j] = MAX_NODES + 1;
			m_RangeEnd[i][j]   = -1;
		}
		for ( j = 0; j < m_cNodes; j++ )
		{
			m_di[j].m_SortedBy[i] = j;
		}

		for ( j = 0; j < m_cNodes - 1; j++ )
		{
			int jNode  = m_di[j].m_SortedBy[i];
			int jCodeX = m_pNodes[jNode].m_Region[0];
			int jCodeY = m_pNodes[jNode].m_Region[1];
			int jCodeZ = m_pNodes[jNode].m_Region[2];
			int jCode;
			switch ( i )
			{
			case 0:
				jCode = ( jCodeX << 16 ) + ( jCodeY << 8 ) + jCodeZ;
				break;
			case 1:
				jCode = ( jCodeY << 16 ) + ( jCodeZ << 8 ) + jCodeX;
				break;
			case 2:
				jCode = ( jCodeZ << 16 ) + ( jCodeX << 8 ) + jCodeY;
				break;
			}

			for ( int k = j + 1; k < m_cNodes; k++ )
			{
				int kNode  = m_di[k].m_SortedBy[i];
				int kCodeX = m_pNodes[kNode].m_Region[0];
				int kCodeY = m_pNodes[kNode].m_Region[1];
				int kCodeZ = m_pNodes[kNode].m_Region[2];
				int kCode;
				switch ( i )
				{
				case 0:
					kCode = ( kCodeX << 16 ) + ( kCodeY << 8 ) + kCodeZ;
					break;
				case 1:
					kCode = ( kCodeY << 16 ) + ( kCodeZ << 8 ) + kCodeX;
					break;
				case 2:
					kCode = ( kCodeZ << 16 ) + ( kCodeX << 8 ) + kCodeY;
					break;
				}

				if ( kCode < jCode )
				{
					// Swap j and k entries.
					//
					int Tmp               = m_di[j].m_SortedBy[i];
					m_di[j].m_SortedBy[i] = m_di[k].m_SortedBy[i];
					m_di[k].m_SortedBy[i] = Tmp;
				}
			}
		}
	}

	// Generate lookup tables.
	//
	for ( i = 0; i < m_cNodes; i++ )
	{
		int CodeX = m_pNodes[m_di[i].m_SortedBy[0]].m_Region[0];
		int CodeY = m_pNodes[m_di[i].m_SortedBy[1]].m_Region[1];
		int CodeZ = m_pNodes[m_di[i].m_SortedBy[2]].m_Region[2];

		if ( i < m_RangeStart[0][CodeX] )
		{
			m_RangeStart[0][CodeX] = i;
		}
		if ( i < m_RangeStart[1][CodeY] )
		{
			m_RangeStart[1][CodeY] = i;
		}
		if ( i < m_RangeStart[2][CodeZ] )
		{
			m_RangeStart[2][CodeZ] = i;
		}
		if ( m_RangeEnd[0][CodeX] < i )
		{
			m_RangeEnd[0][CodeX] = i;
		}
		if ( m_RangeEnd[1][CodeY] < i )
		{
			m_RangeEnd[1][CodeY] = i;
		}
		if ( m_RangeEnd[2][CodeZ] < i )
		{
			m_RangeEnd[2][CodeZ] = i;
		}
	}

	// Initialize the cache.
	//
	memset( m_Cache, 0, sizeof( m_Cache ) );
}
