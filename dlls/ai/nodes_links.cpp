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
// nodes_links.cpp - AI node graph link creation, validation, and hash indexing.
//=========================================================

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/monsters.h"
#include "ai/nodes.h"
#include "core/animation.h"
#include "systems/doors.h"

extern DLL_GLOBAL edict_t *g_pBodyQueueHead;

//=========================================================
// CGraph - LinkEntForLink - sometimes the ent that blocks
// a path is a usable door, in which case the monster just
// needs to face the door and fire it. In other cases, the
// monster needs to operate a button or lever to get the
// door to open. This function will return a pointer to the
// button if the monster needs to hit a button to open the
// door, or returns a pointer to the door if the monster
// need only use the door.
//
// pNode is the node the monster will be standing on when it
// will need to stop and trigger the ent.
//=========================================================
entvars_t *CGraph ::LinkEntForLink( CLink *pLink, CNode *pNode )
{
	edict_t *pentSearch;
	edict_t *pentTrigger;
	entvars_t *pevTrigger;
	entvars_t *pevLinkEnt;
	TraceResult tr;

	pevLinkEnt = pLink->m_pLinkEnt;
	if ( !pevLinkEnt )
		return NULL;

	pentSearch = NULL; // start search at the top of the ent list.

	if ( FClassnameIs( pevLinkEnt, "func_door" ) || FClassnameIs( pevLinkEnt, "func_door_rotating" ) )
	{
		if ( ( pevLinkEnt->spawnflags & SF_DOOR_USE_ONLY ) )
		{ // door is use only, so the door is all the monster has to worry about
			return pevLinkEnt;
		}

		while ( 1 )
		{
			pentTrigger = FIND_ENTITY_BY_TARGET( pentSearch, STRING( pevLinkEnt->targetname ) ); // find the button or trigger

			if ( FNullEnt( pentTrigger ) )
			{ // no trigger found
				return pevLinkEnt;
			}

			pentSearch = pentTrigger;
			pevTrigger = VARS( pentTrigger );

			if ( FClassnameIs( pevTrigger, "func_button" ) || FClassnameIs( pevTrigger, "func_rot_button" ) )
			{ // only buttons are handled right now.

				// trace from the node to the trigger, make sure it's one we can see from the node.
				UTIL_TraceLine( pNode->m_vecOrigin, VecBModelOrigin( pevTrigger ), ignore_monsters, g_pBodyQueueHead, &tr );

				if ( VARS( tr.pHit ) == pevTrigger )
				{ // good to go!
					return VARS( tr.pHit );
				}
			}
		}
	}
	else
	{
		ALERT( at_aiconsole, "Unsupported PathEnt:\n'%s'\n", STRING( pevLinkEnt->classname ) );
		return NULL;
	}
}

//=========================================================
// CGraph - HandleLinkEnt - a brush ent is between two
// nodes that would otherwise be able to see each other.
// Given the monster's capability, determine whether
// or not the monster can go this way.
//=========================================================
int CGraph ::HandleLinkEnt( int iNode, entvars_t *pevLinkEnt, int afCapMask, NODEQUERY queryType )
{
	CBaseEntity *pDoor;

	if ( !m_fGraphPresent || !m_fGraphPointersSet )
	{ // protect us in the case that the node graph isn't available
		ALERT( at_aiconsole, "Graph not ready!\n" );
		return FALSE;
	}

	if ( FNullEnt( pevLinkEnt ) )
	{
		ALERT( at_aiconsole, "dead path ent!\n" );
		return TRUE;
	}

	// func_door
	if ( FClassnameIs( pevLinkEnt, "func_door" ) || FClassnameIs( pevLinkEnt, "func_door_rotating" ) )
	{ // ent is a door.

		pDoor = ( CBaseEntity::Instance( pevLinkEnt ) );
		if ( !pDoor )
		{
			return FALSE;
		}

		if ( ( pevLinkEnt->spawnflags & SF_DOOR_USE_ONLY ) )
		{ // door is use only.

			if ( ( afCapMask & bits_CAP_OPEN_DOORS ) )
			{ // let monster right through if he can open doors
				return TRUE;
			}
			else
			{
				// monster should try for it if the door is open and looks as if it will stay that way
				if ( pDoor->GetToggleState() == TS_AT_TOP && ( pevLinkEnt->spawnflags & SF_DOOR_NO_AUTO_RETURN ) )
				{
					return TRUE;
				}

				return FALSE;
			}
		}
		else
		{ // door must be opened with a button or trigger field.

			// monster should try for it if the door is open and looks as if it will stay that way
			if ( pDoor->GetToggleState() == TS_AT_TOP && ( pevLinkEnt->spawnflags & SF_DOOR_NO_AUTO_RETURN ) )
			{
				return TRUE;
			}
			if ( ( afCapMask & bits_CAP_OPEN_DOORS ) )
			{
				if ( !( pevLinkEnt->spawnflags & SF_DOOR_NOMONSTERS ) || queryType == NODEGRAPH_STATIC )
					return TRUE;
			}

			return FALSE;
		}
	}
	// func_breakable
	else if ( FClassnameIs( pevLinkEnt, "func_breakable" ) && queryType == NODEGRAPH_STATIC )
	{
		return TRUE;
	}
	else
	{
		ALERT( at_aiconsole, "Unhandled Ent in Path %s\n", STRING( pevLinkEnt->classname ) );
		return FALSE;
	}

	return FALSE;
}

//=========================================================
// CGraph - LinkVisibleNodes - the first, most basic
// function of node graph creation, this connects every
// node to every other node that it can see. Expects a
// pointer to an empty connection pool and a file pointer
// to write progress to. Returns the total number of initial
// links.
//=========================================================
int CGraph ::LinkVisibleNodes( CLink *pLinkPool, FILE *file, int *piBadNode )
{
	int i, j, z;
	edict_t *pTraceEnt;
	int cTotalLinks, cLinksThisNode, cMaxInitialLinks;
	TraceResult tr;

	*piBadNode = 0;

	if ( m_cNodes <= 0 )
	{
		ALERT( at_aiconsole, "No Nodes!\n" );
		return FALSE;
	}

	if ( file )
	{
		fprintf( file, "----------------------------------------------------------------------------\n" );
		fprintf( file, "LinkVisibleNodes - Initial Connections\n" );
		fprintf( file, "----------------------------------------------------------------------------\n" );
	}

	cTotalLinks      = 0; // start with no connections
	cMaxInitialLinks = 0;

	for ( i = 0; i < m_cNodes; i++ )
	{
		cLinksThisNode = 0; // reset this count for each node.

		if ( file )
		{
			fprintf( file, "Node #%4d:\n\n", i );
		}

		for ( z = 0; z < MAX_NODE_INITIAL_LINKS; z++ )
		{                                               // clear out the important fields in the link pool for this node
			pLinkPool[cTotalLinks + z].m_iSrcNode  = i; // so each link knows which node it originates from
			pLinkPool[cTotalLinks + z].m_iDestNode = 0;
			pLinkPool[cTotalLinks + z].m_pLinkEnt  = NULL;
		}

		m_pNodes[i].m_iFirstLink = cTotalLinks;

		// now build a list of every other node that this node can see
		for ( j = 0; j < m_cNodes; j++ )
		{
			if ( j == i )
			{ // don't connect to self!
				continue;
			}

			if ( ( m_pNodes[i].m_afNodeInfo & bits_NODE_GROUP_REALM ) != ( m_pNodes[j].m_afNodeInfo & bits_NODE_GROUP_REALM ) )
			{
				continue;
			}

			tr.pHit   = NULL;
			pTraceEnt = 0;

			UTIL_TraceLine( m_pNodes[i].m_vecOrigin,
			                m_pNodes[j].m_vecOrigin,
			                ignore_monsters,
			                g_pBodyQueueHead,
			                &tr );

			if ( tr.fStartSolid )
				continue;

			if ( tr.flFraction != 1.0 )
			{ // trace hit a brush ent, trace backwards to make sure that this ent is the only thing in the way.

				pTraceEnt = tr.pHit;

				UTIL_TraceLine( m_pNodes[j].m_vecOrigin,
				                m_pNodes[i].m_vecOrigin,
				                ignore_monsters,
				                g_pBodyQueueHead,
				                &tr );

				if ( tr.pHit == pTraceEnt && !FClassnameIs( tr.pHit, "worldspawn" ) )
				{
					pLinkPool[cTotalLinks].m_pLinkEnt = VARS( tr.pHit );
					memcpy( pLinkPool[cTotalLinks].m_szLinkEntModelname, STRING( VARS( tr.pHit )->model ), 4 );

					if ( !FBitSet( VARS( tr.pHit )->flags, FL_GRAPHED ) )
					{
						VARS( tr.pHit )->flags += FL_GRAPHED;
					}
				}
				else
				{
					continue;
				}
			}

			if ( file )
			{
				fprintf( file, "%4d", j );

				if ( !FNullEnt( pLinkPool[cTotalLinks].m_pLinkEnt ) )
				{
					fprintf( file, "  Entity on connection: %s, name: %s  Model: %s", STRING( VARS( pTraceEnt )->classname ), STRING( VARS( pTraceEnt )->targetname ), STRING( VARS( tr.pHit )->model ) );
				}

				fprintf( file, "\n" );
			}

			pLinkPool[cTotalLinks].m_iDestNode = j;
			cLinksThisNode++;
			cTotalLinks++;

			if ( cLinksThisNode == MAX_NODE_INITIAL_LINKS )
			{
				ALERT( at_aiconsole, "**LinkVisibleNodes:\nNode %d has NodeLinks > MAX_NODE_INITIAL_LINKS", i );
				if ( file )
				{
					fprintf( file, "** NODE %d HAS NodeLinks > MAX_NODE_INITIAL_LINKS **\n", i );
				}
				*piBadNode = i;
				return FALSE;
			}
			else if ( cTotalLinks > MAX_NODE_INITIAL_LINKS * m_cNodes )
			{
				ALERT( at_aiconsole, "**LinkVisibleNodes:\nTotalLinks > MAX_NODE_INITIAL_LINKS * NUMNODES" );
				*piBadNode = i;
				return FALSE;
			}

			if ( cLinksThisNode == 0 )
			{
				if ( file )
				{
					fprintf( file, "**NO INITIAL LINKS**\n" );
				}
			}

			WorldGraph.m_pNodes[i].m_cNumLinks = cLinksThisNode;

			if ( cLinksThisNode > cMaxInitialLinks )
			{
				cMaxInitialLinks = cLinksThisNode;
			}
		}

		if ( file )
		{
			fprintf( file, "----------------------------------------------------------------------------\n" );
		}
	}

	if ( file )
	{
		fprintf( file, "\n%4d Total Initial Connections - %4d Maximum connections for a single node.\n", cTotalLinks, cMaxInitialLinks );
		fprintf( file, "----------------------------------------------------------------------------\n\n\n" );
	}

	return cTotalLinks;
}

//=========================================================
// CGraph - RejectInlineLinks - expects a pointer to a link
// pool, and a pointer to and already-open file ( if you
// want status reports written to disk ). RETURNS the number
// of connections that were rejected
//=========================================================
int CGraph ::RejectInlineLinks( CLink *pLinkPool, FILE *file )
{
	int i, j, k;

	int cRejectedLinks;
	BOOL fRestartLoop;

	CNode *pSrcNode;
	CNode *pCheckNode;
	CNode *pTestNode;

	float flDistToTestNode, flDistToCheckNode;

	Vector2D vec2DirToTestNode, vec2DirToCheckNode;

	if ( file )
	{
		fprintf( file, "----------------------------------------------------------------------------\n" );
		fprintf( file, "InLine Rejection:\n" );
		fprintf( file, "----------------------------------------------------------------------------\n" );
	}

	cRejectedLinks = 0;

	for ( i = 0; i < m_cNodes; i++ )
	{
		pSrcNode = &m_pNodes[i];

		if ( file )
		{
			fprintf( file, "Node %3d:\n", i );
		}

		for ( j = 0; j < pSrcNode->m_cNumLinks; j++ )
		{
			pCheckNode = &m_pNodes[pLinkPool[pSrcNode->m_iFirstLink + j].m_iDestNode];

			vec2DirToCheckNode = ( pCheckNode->m_vecOrigin - pSrcNode->m_vecOrigin ).Make2D();
			flDistToCheckNode  = vec2DirToCheckNode.Length();
			vec2DirToCheckNode = vec2DirToCheckNode.Normalize();

			pLinkPool[pSrcNode->m_iFirstLink + j].m_flWeight = flDistToCheckNode;

			fRestartLoop = FALSE;
			for ( k = 0; k < pSrcNode->m_cNumLinks && !fRestartLoop; k++ )
			{
				if ( k == j )
				{
					continue;
				}

				pTestNode = &m_pNodes[pLinkPool[pSrcNode->m_iFirstLink + k].m_iDestNode];

				vec2DirToTestNode = ( pTestNode->m_vecOrigin - pSrcNode->m_vecOrigin ).Make2D();

				flDistToTestNode  = vec2DirToTestNode.Length();
				vec2DirToTestNode = vec2DirToTestNode.Normalize();

				if ( DotProduct( vec2DirToCheckNode, vec2DirToTestNode ) >= 0.998 )
				{
					if ( flDistToTestNode < flDistToCheckNode )
					{
						if ( file )
						{
							fprintf( file, "REJECTED NODE %3d through Node %3d, Dot = %8f\n", pLinkPool[pSrcNode->m_iFirstLink + j].m_iDestNode, pLinkPool[pSrcNode->m_iFirstLink + k].m_iDestNode, DotProduct( vec2DirToCheckNode, vec2DirToTestNode ) );
						}

						pLinkPool[pSrcNode->m_iFirstLink + j] = pLinkPool[pSrcNode->m_iFirstLink + ( pSrcNode->m_cNumLinks - 1 )];
						pSrcNode->m_cNumLinks--;
						j--;

						cRejectedLinks++;

						fRestartLoop = TRUE;
					}
				}
			}
		}

		if ( file )
		{
			fprintf( file, "----------------------------------------------------------------------------\n\n" );
		}
	}

	return cRejectedLinks;
}

struct tagNodePair
{
	short iSrc;
	short iDest;
};

void CGraph::HashInsert( int iSrcNode, int iDestNode, int iKey )
{
	if ( !m_pHashLinks || m_nHashLinks <= 0 )
	{
		return;
	}

	struct tagNodePair np;

	np.iSrc  = iSrcNode;
	np.iDest = iDestNode;
	CRC32_t dwHash;
	CRC32_INIT( &dwHash );
	CRC32_PROCESS_BUFFER( &dwHash, &np, sizeof( np ) );
	dwHash = CRC32_FINAL( dwHash );

	int di            = m_HashPrimes[dwHash & 15];
	int i             = ( dwHash >> 4 ) % m_nHashLinks;
	int maxIterations = m_nHashLinks;
	while ( m_pHashLinks[i] != ENTRY_STATE_EMPTY && maxIterations-- > 0 )
	{
		i += di;
		if ( i >= m_nHashLinks )
			i -= m_nHashLinks;
	}
	if ( i >= 0 && i < m_nHashLinks )
	{
		m_pHashLinks[i] = iKey;
	}
}

void CGraph::HashSearch( int iSrcNode, int iDestNode, int &iKey )
{
	iKey = ENTRY_STATE_EMPTY;
	if ( !m_pHashLinks || m_nHashLinks <= 0 || !m_pLinkPool || m_cLinks <= 0 )
	{
		return;
	}

	struct tagNodePair np;

	np.iSrc  = iSrcNode;
	np.iDest = iDestNode;
	CRC32_t dwHash;
	CRC32_INIT( &dwHash );
	CRC32_PROCESS_BUFFER( &dwHash, &np, sizeof( np ) );
	dwHash = CRC32_FINAL( dwHash );

	int di            = m_HashPrimes[dwHash & 15];
	int i             = ( dwHash >> 4 ) % m_nHashLinks;
	int maxIterations = m_nHashLinks;
	while ( m_pHashLinks[i] != ENTRY_STATE_EMPTY && maxIterations-- > 0 )
	{
		int linkIdx = m_pHashLinks[i];
		if ( linkIdx >= 0 && linkIdx < m_cLinks )
		{
			CLink &link = m_pLinkPool[linkIdx];
			if ( iSrcNode == link.m_iSrcNode && iDestNode == link.m_iDestNode )
			{
				break;
			}
		}
		i += di;
		if ( i >= m_nHashLinks )
			i -= m_nHashLinks;
	}
	if ( i >= 0 && i < m_nHashLinks )
	{
		iKey = m_pHashLinks[i];
	}
}

#define NUMBER_OF_PRIMES 177

static int Primes[NUMBER_OF_PRIMES] =
    { 1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911, 919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997, 1009, 1013, 1019, 1021, 1031, 1033, 1039, 0 };

void CGraph::HashChoosePrimes( int TableSize )
{
	int LargestPrime = TableSize / 2;
	if ( LargestPrime > Primes[NUMBER_OF_PRIMES - 2] )
	{
		LargestPrime = Primes[NUMBER_OF_PRIMES - 2];
	}
	int Spacing = LargestPrime / 16;

	int iPrime, iZone;
	for ( iZone = 1, iPrime = 0; iPrime < 16; iZone += Spacing )
	{
		int Lower = Primes[0];
		for ( int jPrime = 0; Primes[jPrime] != 0; jPrime++ )
		{
			if ( jPrime != 0 && TableSize % Primes[jPrime] == 0 )
				continue;
			int Upper = Primes[jPrime];
			if ( Lower <= iZone && iZone <= Upper )
			{
				if ( iZone - Lower <= Upper - iZone )
				{
					m_HashPrimes[iPrime++] = Lower;
				}
				else
				{
					m_HashPrimes[iPrime++] = Upper;
				}
				break;
			}
			Lower = Upper;
		}
	}

	for ( iPrime = 0; iPrime < 16; iPrime += 2 )
	{
		m_HashPrimes[iPrime] = TableSize - m_HashPrimes[iPrime];
	}

	for ( iPrime = 0; iPrime < 16 - 1; iPrime++ )
	{
		int Pick                  = RANDOM_LONG( 0, 15 - iPrime );
		int primeSwap             = m_HashPrimes[Pick];
		m_HashPrimes[Pick]        = m_HashPrimes[15 - iPrime];
		m_HashPrimes[15 - iPrime] = primeSwap;
	}
}

void CGraph::BuildLinkLookups( void )
{
	m_nHashLinks = 3 * m_cLinks / 2 + 3;

	HashChoosePrimes( m_nHashLinks );
	m_pHashLinks = (short *)calloc( sizeof( short ), m_nHashLinks );
	if ( !m_pHashLinks )
	{
		ALERT( at_aiconsole, "Couldn't allocated Link Lookup Table.\n" );
		return;
	}
	int i;
	for ( i = 0; i < m_nHashLinks; i++ )
	{
		m_pHashLinks[i] = ENTRY_STATE_EMPTY;
	}

	for ( i = 0; i < m_cLinks; i++ )
	{
		CLink &link = Link( i );
		HashInsert( link.m_iSrcNode, link.m_iDestNode, i );
	}
}
