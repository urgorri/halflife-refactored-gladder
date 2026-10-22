#include "sound_local.h"

void EMIT_SOUND_DYN( edict_t *entity, int channel, const char *sample, float volume, float attenuation,
                     int flags, int pitch )
{
	if ( sample && *sample == '!' )
	{
		char name[32];
		if ( SENTENCEG_Lookup( sample, name ) >= 0 )
			EMIT_SOUND_DYN2( entity, channel, name, volume, attenuation, flags, pitch );
		else
			ALERT( at_aiconsole, "Unable to find %s in sentences.txt\n", sample );
	}
	else
		EMIT_SOUND_DYN2( entity, channel, sample, volume, attenuation, flags, pitch );
}

// play a specific sentence over the HEV suit speaker - just pass player entity, and !sentencename

int fTextureTypeInit = FALSE;

#define CTEXTURESMAX 512 // max number of textures loaded

int gcTextures = 0;
char grgszTextureName[CTEXTURESMAX][CBTEXTURENAMEMAX]; // texture names
char grgchTextureType[CTEXTURESMAX];                   // parallel array of texture types

// open materials.txt,  get size, alloc space,
// save in array.  Only works first time called,
// ignored on subsequent calls.

void TEXTURETYPE_Init()
{
	char buffer[512];
	int i, j;
	byte *pMemFile;
	int fileSize = 0;
	int filePos  = 0;

	if ( fTextureTypeInit )
		return;

	memset( &( grgszTextureName[0][0] ), 0, CTEXTURESMAX * CBTEXTURENAMEMAX );
	memset( grgchTextureType, 0, CTEXTURESMAX );

	gcTextures = 0;
	memset( buffer, 0, 512 );

	pMemFile = g_engfuncs.pfnLoadFileForMe( "sound/materials.txt", &fileSize );
	if ( !pMemFile )
		return;

	// for each line in the file...
	while ( memfgets( pMemFile, fileSize, filePos, buffer, 511 ) != NULL && ( gcTextures < CTEXTURESMAX ) )
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

	g_engfuncs.pfnFreeFile( pMemFile );

	fTextureTypeInit = TRUE;
}

// given texture name, find texture type
// if not found, return type 'concrete'

// NOTE: this routine should ONLY be called if the
// current texture under the player changes!

char TEXTURETYPE_Find( char *name )
{
	// CONSIDER: pre-sort texture names and perform faster binary search here

	for ( int i = 0; i < gcTextures; i++ )
	{
		if ( !strnicmp( name, &( grgszTextureName[i][0] ), CBTEXTURENAMEMAX - 1 ) )
			return ( grgchTextureType[i] );
	}

	return CHAR_TEX_CONCRETE;
}

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play

float TEXTURETYPE_PlaySound( TraceResult *ptr, Vector vecSrc, Vector vecEnd, int iBulletType )
{
	// hit the world, try to play sound based on texture material type

	char chTextureType;
	float fvol;
	float fvolbar;
	char szbuffer[64];
	const char *pTextureName;
	float rgfl1[3];
	float rgfl2[3];
	char *rgsz[4];
	int cnt;
	float fattn = ATTN_NORM;

	if ( !g_pGameRules->PlayTextureSounds() )
		return 0.0;

	CBaseEntity *pEntity = CBaseEntity::Instance( ptr->pHit );

	chTextureType = 0;

	if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		// hit body
		chTextureType = CHAR_TEX_FLESH;
	else
	{
		// hit world

		// find texture under strike, get material type

		// copy trace vector into array for trace_texture

		vecSrc.CopyToArray( rgfl1 );
		vecEnd.CopyToArray( rgfl2 );

		// get texture from entity or world (world is ent(0))
		if ( pEntity )
			pTextureName = TRACE_TEXTURE( ENT( pEntity->pev ), rgfl1, rgfl2 );
		else
			pTextureName = TRACE_TEXTURE( ENT( 0 ), rgfl1, rgfl2 );

		if ( pTextureName )
		{
			// strip leading '-0' or '+0~' or '{' or '!'
			if ( *pTextureName == '-' || *pTextureName == '+' )
				pTextureName += 2;

			if ( *pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ' )
				pTextureName++;
			// '}}'
			strncpy( szbuffer, pTextureName, CBTEXTURENAMEMAX - 1 );
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;

			// ALERT ( at_console, "texture hit: %s\n", szbuffer);

			// get texture type
			chTextureType = TEXTURETYPE_Find( szbuffer );
		}
	}

	switch ( chTextureType )
	{
	default:
	case CHAR_TEX_CONCRETE:
		fvol    = 0.9;
		fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		cnt     = 2;
		break;
	case CHAR_TEX_METAL:
		fvol    = 0.9;
		fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt     = 2;
		break;
	case CHAR_TEX_DIRT:
		fvol    = 0.9;
		fvolbar = 0.1;
		rgsz[0] = "player/pl_dirt1.wav";
		rgsz[1] = "player/pl_dirt2.wav";
		rgsz[2] = "player/pl_dirt3.wav";
		cnt     = 3;
		break;
	case CHAR_TEX_VENT:
		fvol    = 0.5;
		fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt     = 2;
		break;
	case CHAR_TEX_GRATE:
		fvol    = 0.9;
		fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt     = 2;
		break;
	case CHAR_TEX_TILE:
		fvol    = 0.8;
		fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt     = 4;
		break;
	case CHAR_TEX_SLOSH:
		fvol    = 0.9;
		fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt     = 4;
		break;
	case CHAR_TEX_WOOD:
		fvol    = 0.9;
		fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt     = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		fvol    = 0.8;
		fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt     = 3;
		break;
	case CHAR_TEX_FLESH:
		if ( iBulletType == BULLET_PLAYER_CROWBAR )
			return 0.0; // crowbar already makes this sound
		fvol    = 1.0;
		fvolbar = 0.2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		fattn   = 1.0;
		cnt     = 2;
		break;
	}

	// did we hit a breakable?

	if ( pEntity && FClassnameIs( pEntity->pev, "func_breakable" ) )
	{
		// drop volumes, the object will already play a damaged sound
		fvol /= 1.5;
		fvolbar /= 2.0;
	}
	else if ( chTextureType == CHAR_TEX_COMPUTER )
	{
		// play random spark if computer

		if ( ptr->flFraction != 1.0 && RANDOM_LONG( 0, 1 ) )
		{
			UTIL_Sparks( ptr->vecEndPos );

			float flVolume = RANDOM_FLOAT( 0.7, 1.0 ); // random volume range
			switch ( RANDOM_LONG( 0, 1 ) )
			{
			case 0:
				UTIL_EmitAmbientSound( ENT( 0 ), ptr->vecEndPos, "buttons/spark5.wav", flVolume, ATTN_NORM, 0, 100 );
				break;
			case 1:
				UTIL_EmitAmbientSound( ENT( 0 ), ptr->vecEndPos, "buttons/spark6.wav", flVolume, ATTN_NORM, 0, 100 );
				break;
				// case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark5.wav", flVolume, ATTN_NORM);	break;
				// case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark6.wav", flVolume, ATTN_NORM);	break;
			}
		}
	}

	// play material hit sound
	UTIL_EmitAmbientSound( ENT( 0 ), ptr->vecEndPos, rgsz[RANDOM_LONG( 0, cnt - 1 )], fvol, fattn, 0, 96 + RANDOM_LONG( 0, 0xf ) );
	// EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, rgsz[RANDOM_LONG(0,cnt-1)], fvol, ATTN_NORM, 0, 96 + RANDOM_LONG(0,0xf));

	return fvolbar;
}

// ===================================================================================
//
// Speaker class. Used for announcements per level, for door lock/unlock spoken voice.
//
