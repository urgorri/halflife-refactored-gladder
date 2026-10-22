/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#include "pm_local.h"
static int gcTextures = 0;
static char grgszTextureName[CTEXTURESMAX][CBTEXTURENAMEMAX];
static char grgchTextureType[CTEXTURESMAX];

void PM_SwapTextures( int i, int j )
{
	char chTemp;
	char szTemp[CBTEXTURENAMEMAX];

	strncpy( szTemp, grgszTextureName[i], sizeof( szTemp ) - 1 );
	szTemp[sizeof( szTemp ) - 1] = '\0';
	chTemp                       = grgchTextureType[i];

	strncpy( grgszTextureName[i], grgszTextureName[j], sizeof( grgszTextureName[i] ) - 1 );
	grgszTextureName[i][sizeof( grgszTextureName[i] ) - 1] = '\0';
	grgchTextureType[i]                                    = grgchTextureType[j];

	strncpy( grgszTextureName[j], szTemp, sizeof( grgszTextureName[j] ) - 1 );
	grgszTextureName[j][sizeof( grgszTextureName[j] ) - 1] = '\0';
	grgchTextureType[j]                                    = chTemp;
}

void PM_SortTextures( void )
{
	// Bubble sort, yuck, but this only occurs at startup and it's only 512 elements...
	//
	int i, j;

	for ( i = 0; i < gcTextures; i++ )
	{
		for ( j = i + 1; j < gcTextures; j++ )
		{
			if ( stricmp( grgszTextureName[i], grgszTextureName[j] ) > 0 )
			{
				// Swap
				//
				PM_SwapTextures( i, j );
			}
		}
	}
}

void PM_InitTextureTypes()
{
	char buffer[512];
	int i, j;
	byte *pMemFile;
	int fileSize, filePos;
	static qboolean bTextureTypeInit = false;

	if ( bTextureTypeInit )
		return;

	memset( &( grgszTextureName[0][0] ), 0, CTEXTURESMAX * CBTEXTURENAMEMAX );
	memset( grgchTextureType, 0, CTEXTURESMAX );

	gcTextures = 0;
	memset( buffer, 0, 512 );

	fileSize = pmove->COM_FileSize( "sound/materials.txt" );
	pMemFile = pmove->COM_LoadFile( "sound/materials.txt", 5, NULL );
	if ( !pMemFile )
		return;

	filePos = 0;
	// for each line in the file...
	while ( pmove->memfgets( pMemFile, fileSize, &filePos, buffer, 511 ) != NULL && ( gcTextures < CTEXTURESMAX ) )
	{
		// skip whitespace
		i = 0;
		while ( buffer[i] && isspace( buffer[i] ) )
			i++;

		if ( !buffer[i] )
			continue;

		// skip comment lines
		if ( buffer[i] == '/' || !isalpha( buffer[i] ) )
			continue;

		// get texture type
		grgchTextureType[gcTextures] = toupper( buffer[i++] );

		// skip whitespace
		while ( buffer[i] && isspace( buffer[i] ) )
			i++;

		if ( !buffer[i] )
			continue;

		// get sentence name
		j = i;
		while ( buffer[j] && !isspace( buffer[j] ) )
			j++;

		if ( !buffer[j] )
			continue;

		// null-terminate name and save in sentences array
		j         = min( j, CBTEXTURENAMEMAX - 1 + i );
		buffer[j] = 0;
		strncpy( &( grgszTextureName[gcTextures][0] ), &( buffer[i] ), CBTEXTURENAMEMAX - 1 );
		grgszTextureName[gcTextures][CBTEXTURENAMEMAX - 1] = '\0';
		gcTextures++;
	}

	// Must use engine to free since we are in a .dll
	pmove->COM_FreeFile( pMemFile );

	PM_SortTextures();

	bTextureTypeInit = true;
}

char PM_FindTextureType( char *name )
{
	int left, right, pivot;
	int val;

	assert( pm_shared_initialized );

	left  = 0;
	right = gcTextures - 1;

	while ( left <= right )
	{
		pivot = ( left + right ) / 2;

		val = strnicmp( name, grgszTextureName[pivot], CBTEXTURENAMEMAX - 1 );
		if ( val == 0 )
		{
			return grgchTextureType[pivot];
		}
		else if ( val > 0 )
		{
			left = pivot + 1;
		}
		else if ( val < 0 )
		{
			right = pivot - 1;
		}
	}

	return CHAR_TEX_CONCRETE;
}

static const char *rgszStepSounds[STEP_LADDER + 1][5] = {
    /* STEP_CONCRETE = 0 */ { "player/pl_step1.wav", "player/pl_step3.wav", "player/pl_step2.wav", "player/pl_step4.wav", NULL },
    /* STEP_METAL    = 1 */ { "player/pl_metal1.wav", "player/pl_metal3.wav", "player/pl_metal2.wav", "player/pl_metal4.wav", NULL },
    /* STEP_DIRT     = 2 */ { "player/pl_dirt1.wav", "player/pl_dirt3.wav", "player/pl_dirt2.wav", "player/pl_dirt4.wav", NULL },
    /* STEP_VENT     = 3 */ { "player/pl_duct1.wav", "player/pl_duct3.wav", "player/pl_duct2.wav", "player/pl_duct4.wav", NULL },
    /* STEP_GRATE    = 4 */ { "player/pl_grate1.wav", "player/pl_grate3.wav", "player/pl_grate2.wav", "player/pl_grate4.wav", NULL },
    /* STEP_TILE     = 5 */ { "player/pl_tile1.wav", "player/pl_tile3.wav", "player/pl_tile2.wav", "player/pl_tile4.wav", "player/pl_tile5.wav" },
    /* STEP_SLOSH    = 6 */ { "player/pl_slosh1.wav", "player/pl_slosh3.wav", "player/pl_slosh2.wav", "player/pl_slosh4.wav", NULL },
    /* STEP_WADE     = 7 */ { "player/pl_wade1.wav", "player/pl_wade2.wav", "player/pl_wade3.wav", "player/pl_wade4.wav", NULL },
    /* STEP_LADDER   = 8 */ { "player/pl_ladder1.wav", "player/pl_ladder3.wav", "player/pl_ladder2.wav", "player/pl_ladder4.wav", NULL } };

void PM_PlayWadeSound( float fvol )
{
	static const char *rgszWadeSounds[4] = {
	    "player/pl_wade1.wav",
	    "player/pl_wade2.wav",
	    "player/pl_wade3.wav",
	    "player/pl_wade4.wav" };
	int idx = pmove->RandomLong( 0, 3 );
	pmove->PM_PlaySound( CHAN_BODY, rgszWadeSounds[idx], fvol, ATTN_NORM, 0, PITCH_NORM );
}

void PM_PlayStepSound( int step, float fvol )
{
	static int iSkipStep = 0;
	int irand;
	vec3_t hvel;
	const char *pszSound;

	pmove->iStepLeft = !pmove->iStepLeft;

	if ( !pmove->runfuncs )
	{
		return;
	}

	irand = pmove->RandomLong( 0, 1 ) + ( pmove->iStepLeft * 2 );

	// Footstep movement audio configuration
	if ( pmove->multiplayer && !pmove->movevars->footsteps )
		return;

	VectorCopy( pmove->velocity, hvel );
	hvel[2] = 0.0;

	if ( pmove->multiplayer && ( !g_onladder && Length( hvel ) <= 220 ) )
		return;

	// Clamp to valid range, defaulting to concrete
	if ( step < 0 || step > STEP_LADDER )
		step = STEP_CONCRETE;

	if ( step == STEP_WADE )
	{
		if ( iSkipStep == 0 )
		{
			iSkipStep++;
			return;
		}

		if ( iSkipStep++ == 3 )
		{
			iSkipStep = 0;
		}
	}
	else if ( step == STEP_TILE )
	{
		if ( !pmove->RandomLong( 0, 4 ) )
			irand = 4;
	}

	pszSound = rgszStepSounds[step][irand];
	if ( pszSound )
	{
		pmove->PM_PlaySound( CHAN_BODY, pszSound, fvol, ATTN_NORM, 0, PITCH_NORM );
	}
}

int PM_MapTextureTypeStepType( char chTextureType )
{
	switch ( chTextureType )
	{
	default:
	case CHAR_TEX_CONCRETE:
		return STEP_CONCRETE;
	case CHAR_TEX_METAL:
		return STEP_METAL;
	case CHAR_TEX_DIRT:
		return STEP_DIRT;
	case CHAR_TEX_VENT:
		return STEP_VENT;
	case CHAR_TEX_GRATE:
		return STEP_GRATE;
	case CHAR_TEX_TILE:
		return STEP_TILE;
	case CHAR_TEX_SLOSH:
		return STEP_SLOSH;
	}
}

/*
====================
PM_CatagorizeTextureType

Determine texture info for the texture we are standing on.
====================
*/
void PM_CatagorizeTextureType( void )
{
	vec3_t start, end;
	const char *pTextureName;

	VectorCopy( pmove->origin, start );
	VectorCopy( pmove->origin, end );

	// Straight down
	end[2] -= 64;

	// Fill in default values, just in case.
	pmove->sztexturename[0] = '\0';
	pmove->chtexturetype    = CHAR_TEX_CONCRETE;

	pTextureName = pmove->PM_TraceTexture( pmove->onground, start, end );
	if ( !pTextureName )
		return;

	// strip leading '-0' or '+0~' or '{' or '!'
	if ( *pTextureName == '-' || *pTextureName == '+' )
		pTextureName += 2;

	if ( *pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ' )
		pTextureName++;
	// '}}'

	strncpy( pmove->sztexturename, pTextureName, CBTEXTURENAMEMAX - 1 );
	pmove->sztexturename[CBTEXTURENAMEMAX - 1] = 0;

	// get texture type
	pmove->chtexturetype = PM_FindTextureType( pmove->sztexturename );
}

void PM_UpdateStepSound( void )
{
	int fWalking;
	float fvol;
	vec3_t knee;
	vec3_t feet;
	vec3_t center;
	float height;
	float speed;
	float velrun;
	float velwalk;
	float flduck;
	int fLadder;
	int step;

	if ( pmove->flTimeStepSound > 0 )
		return;

	if ( pmove->flags & FL_FROZEN )
		return;

	PM_CatagorizeTextureType();

	speed = Length( pmove->velocity );

	// determine if we are on a ladder
	// The Barnacle Grapple sets the FL_IMMUNE_LAVA flag to indicate that the player is not on a ladder - Solokiller
	fLadder = ( pmove->movetype == MOVETYPE_FLY ) && !( pmove->flags & FL_IMMUNE_LAVA ); // IsOnLadder();

	// Gait locomotion velocity thresholds
	if ( ( pmove->flags & FL_DUCKING ) || fLadder )
	{
		velwalk = 60; // These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		velrun  = 80; // Walking gait velocity threshold
		flduck  = 100;
	}
	else
	{
		velwalk = 120;
		velrun  = 210;
		flduck  = 0;
	}

	// If we're on a ladder or on the ground, and we're moving fast enough,
	//  play step sound.  Also, if pmove->flTimeStepSound is zero, get the new
	//  sound right away - we just started moving in new level.
	if ( ( fLadder || ( pmove->onground != -1 ) ) &&
	     ( Length( pmove->velocity ) > 0.0 ) &&
	     ( speed >= velwalk || !pmove->flTimeStepSound ) )
	{
		fWalking = speed < velrun;

		VectorCopy( pmove->origin, center );
		VectorCopy( pmove->origin, knee );
		VectorCopy( pmove->origin, feet );

		height = pmove->player_maxs[pmove->usehull][2] - pmove->player_mins[pmove->usehull][2];

		knee[2] = pmove->origin[2] - 0.3 * height;
		feet[2] = pmove->origin[2] - 0.5 * height;

		// find out what we're stepping in or on...
		if ( fLadder )
		{
			step                   = STEP_LADDER;
			fvol                   = 0.35;
			pmove->flTimeStepSound = 350;
		}
		else if ( pmove->PM_PointContents( knee, NULL ) == CONTENTS_WATER )
		{
			step                   = STEP_WADE;
			fvol                   = 0.65;
			pmove->flTimeStepSound = 600;
		}
		else if ( pmove->PM_PointContents( feet, NULL ) == CONTENTS_WATER )
		{
			step                   = STEP_SLOSH;
			fvol                   = fWalking ? 0.2 : 0.5;
			pmove->flTimeStepSound = fWalking ? 400 : 300;
		}
		else
		{
			// find texture under player, if different from current texture,
			// get material type
			step                   = PM_MapTextureTypeStepType( pmove->chtexturetype );
			pmove->flTimeStepSound = fWalking ? 400 : 300;

			switch ( pmove->chtexturetype )
			{
			case CHAR_TEX_DIRT:
				fvol = fWalking ? 0.25 : 0.55;
				break;
			case CHAR_TEX_VENT:
				fvol = fWalking ? 0.4 : 0.7;
				break;
			default:
				fvol = fWalking ? 0.2 : 0.5;
				break;
			}
		}

		pmove->flTimeStepSound += flduck; // slower step time if ducking

		// play the sound
		// 35% volume if ducking
		if ( pmove->flags & FL_DUCKING )
		{
			fvol *= 0.35;
		}

		PM_PlayStepSound( step, fvol );
	}
}

/*
=================
PM_PlayWaterSounds

=================
*/
void PM_PlayWaterSounds( void )
{
	// Did we enter or leave water?
	if ( ( pmove->oldwaterlevel == 0 && pmove->waterlevel != 0 ) ||
	     ( pmove->oldwaterlevel != 0 && pmove->waterlevel == 0 ) )
	{
		PM_PlayWadeSound( 1.0f );
	}
}
