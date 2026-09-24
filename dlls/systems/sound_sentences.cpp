#include "sound_local.h"

// ==================== SENTENCE GROUPS, UTILITY FUNCTIONS  ======================================

#define CSENTENCE_LRU_MAX 32 // max number of elements per sentence group

// group of related sentences

typedef struct sentenceg
{
	char szgroupname[CBSENTENCENAME_MAX];
	int count;
	unsigned char rgblru[CSENTENCE_LRU_MAX];

} SENTENCEG;

#define CSENTENCEG_MAX 200 // max number of sentence groups
// globals

SENTENCEG rgsentenceg[CSENTENCEG_MAX];
int fSentencesInit = FALSE;

char gszallsentencenames[CVOXFILESENTENCEMAX][CBSENTENCENAME_MAX];
int gcallsentences = 0;

// randomize list of sentence name indices

void USENTENCEG_InitLRU( unsigned char *plru, int count )
{
	int i, j, k;
	unsigned char swapVal;

	if ( !fSentencesInit )
		return;

	if ( count > CSENTENCE_LRU_MAX )
		count = CSENTENCE_LRU_MAX;

	for ( i = 0; i < count; i++ )
		plru[i] = (unsigned char)i;

	// randomize array
	for ( i = 0; i < ( count * 4 ); i++ )
	{
		j       = RANDOM_LONG( 0, count - 1 );
		k       = RANDOM_LONG( 0, count - 1 );
		swapVal = plru[j];
		plru[j] = plru[k];
		plru[k] = swapVal;
	}
}

// ignore lru. pick next sentence from sentence group. Go in order until we hit the last sentence,
// then repeat list if freset is true.  If freset is false, then repeat last sentence.
// ipick is passed in as the requested sentence ordinal.
// ipick 'next' is returned.
// return of -1 indicates an error.

int USENTENCEG_PickSequential( int isentenceg, char *szfound, int ipick, int freset )
{
	char *szgroupname;
	unsigned char count;
	char sznum[8];

	if ( !fSentencesInit )
		return -1;

	if ( isentenceg < 0 || isentenceg >= CSENTENCEG_MAX )
		return -1;

	szgroupname = rgsentenceg[isentenceg].szgroupname;
	count       = rgsentenceg[isentenceg].count;

	if ( count == 0 )
		return -1;

	if ( ipick >= count )
		ipick = count - 1;

	strcpy( szfound, "!" );
	strcat( szfound, szgroupname );
	sprintf( sznum, "%d", ipick );
	strcat( szfound, sznum );

	if ( ipick >= count )
	{
		if ( freset )
			// reset at end of list
			return 0;
		else
			return count;
	}

	return ipick + 1;
}

// pick a random sentence from rootname0 to rootnameX.
// picks from the rgsentenceg[isentenceg] least
// recently used, modifies lru array. returns the sentencename.
// note, lru must be seeded with 0-n randomized sentence numbers, with the
// rest of the lru filled with -1. The first integer in the lru is
// actually the size of the list.  Returns ipick, the ordinal
// of the picked sentence within the group.

int USENTENCEG_Pick( int isentenceg, char *szfound )
{
	char *szgroupname;
	unsigned char *plru;
	unsigned char i;
	unsigned char count;
	char sznum[8];
	unsigned char ipick;
	int ffound = FALSE;

	if ( !fSentencesInit )
		return -1;

	if ( isentenceg < 0 || isentenceg >= CSENTENCEG_MAX )
		return -1;

	szgroupname = rgsentenceg[isentenceg].szgroupname;
	count       = rgsentenceg[isentenceg].count;
	plru        = rgsentenceg[isentenceg].rgblru;

	while ( !ffound )
	{
		for ( i = 0; i < count; i++ )
			if ( plru[i] != 0xFF )
			{
				ipick   = plru[i];
				plru[i] = 0xFF;
				ffound  = TRUE;
				break;
			}

		if ( !ffound )
			USENTENCEG_InitLRU( plru, count );
		else
		{
			strcpy( szfound, "!" );
			strcat( szfound, szgroupname );
			sprintf( sznum, "%d", ipick );
			strcat( szfound, sznum );
			return ipick;
		}
	}
	return -1;
}

// ===================== SENTENCE GROUPS, MAIN ROUTINES ========================

// Given sentence group rootname (name without number suffix),
// get sentence group index (isentenceg). Returns -1 if no such name.

int SENTENCEG_GetIndex( const char *szgroupname )
{
	int i;

	if ( !fSentencesInit || !szgroupname )
		return -1;

	// search rgsentenceg for match on szgroupname

	i = 0;
	while ( i < CSENTENCEG_MAX && rgsentenceg[i].count )
	{
		if ( !strcmp( szgroupname, rgsentenceg[i].szgroupname ) )
			return i;
		i++;
	}

	return -1;
}

// given sentence group index, play random sentence for given entity.
// returns ipick - which sentence was picked to
// play from the group. Ipick is only needed if you plan on stopping
// the sound before playback is done (see SENTENCEG_Stop).

int SENTENCEG_PlayRndI( edict_t *entity, int isentenceg,
                        float volume, float attenuation, int flags, int pitch )
{
	char name[64];
	int ipick;

	if ( !fSentencesInit )
		return -1;

	if ( isentenceg < 0 || isentenceg >= CSENTENCEG_MAX )
		return -1;

	name[0] = 0;

	ipick = USENTENCEG_Pick( isentenceg, name );
	if ( ipick >= 0 && name[0] )
		EMIT_SOUND_DYN( entity, CHAN_VOICE, name, volume, attenuation, flags, pitch );
	return ipick;
}

// same as above, but takes sentence group name instead of index

int SENTENCEG_PlayRndSz( edict_t *entity, const char *szgroupname,
                         float volume, float attenuation, int flags, int pitch )
{
	char name[64];
	int ipick;
	int isentenceg;

	if ( !fSentencesInit )
		return -1;

	name[0] = 0;

	isentenceg = SENTENCEG_GetIndex( szgroupname );
	if ( isentenceg < 0 )
	{
		ALERT( at_console, "No such sentence group %s\n", szgroupname );
		return -1;
	}

	ipick = USENTENCEG_Pick( isentenceg, name );
	if ( ipick >= 0 && name[0] )
		EMIT_SOUND_DYN( entity, CHAN_VOICE, name, volume, attenuation, flags, pitch );

	return ipick;
}

// play sentences in sequential order from sentence group.  Reset after last sentence.

int SENTENCEG_PlaySequentialSz( edict_t *entity, const char *szgroupname,
                                float volume, float attenuation, int flags, int pitch, int ipick, int freset )
{
	char name[64];
	int ipicknext;
	int isentenceg;

	if ( !fSentencesInit )
		return -1;

	name[0] = 0;

	isentenceg = SENTENCEG_GetIndex( szgroupname );
	if ( isentenceg < 0 )
		return -1;

	ipicknext = USENTENCEG_PickSequential( isentenceg, name, ipick, freset );
	if ( ipicknext >= 0 && name[0] )
		EMIT_SOUND_DYN( entity, CHAN_VOICE, name, volume, attenuation, flags, pitch );
	return ipicknext;
}

// for this entity, for the given sentence within the sentence group, stop
// the sentence.

void SENTENCEG_Stop( edict_t *entity, int isentenceg, int ipick )
{
	char buffer[64];
	char sznum[8];

	if ( !fSentencesInit )
		return;

	if ( isentenceg < 0 || isentenceg >= CSENTENCEG_MAX || ipick < 0 )
		return;

	strcpy( buffer, "!" );
	strcat( buffer, rgsentenceg[isentenceg].szgroupname );
	sprintf( sznum, "%d", ipick );
	strcat( buffer, sznum );

	STOP_SOUND( entity, CHAN_VOICE, buffer );
}

// open sentences.txt, scan for groups, build rgsentenceg
// Should be called from world spawn, only works on the
// first call and is ignored subsequently.

void SENTENCEG_Init()
{
	char buffer[512];
	char szgroup[64];
	int i, j;
	int isentencegs;

	if ( fSentencesInit )
		return;

	memset( gszallsentencenames, 0, CVOXFILESENTENCEMAX * CBSENTENCENAME_MAX );
	gcallsentences = 0;

	memset( rgsentenceg, 0, CSENTENCEG_MAX * sizeof( SENTENCEG ) );
	memset( buffer, 0, 512 );
	memset( szgroup, 0, 64 );
	isentencegs = -1;

	int filePos    = 0, fileSize;
	byte *pMemFile = g_engfuncs.pfnLoadFileForMe( "sound/sentences.txt", &fileSize );
	if ( !pMemFile )
		return;

	// for each line in the file...
	while ( memfgets( pMemFile, fileSize, filePos, buffer, 511 ) != NULL )
	{
		// skip whitespace
		i = 0;
		while ( buffer[i] && buffer[i] == ' ' )
			i++;

		if ( !buffer[i] )
			continue;

		if ( buffer[i] == '/' || !isalpha( buffer[i] ) )
			continue;

		// get sentence name
		j = i;
		while ( buffer[j] && buffer[j] != ' ' )
			j++;

		if ( !buffer[j] )
			continue;

		if ( gcallsentences >= CVOXFILESENTENCEMAX )
		{
			ALERT( at_error, "Too many sentences in sentences.txt!\n" );
			break;
		}

		// null-terminate name and save in sentences array
		buffer[j]           = 0;
		const char *pString = buffer + i;

		if ( strlen( pString) >= CBSENTENCENAME_MAX )
			ALERT( at_warning, "Sentence %s longer than %d letters\n", pString, CBSENTENCENAME_MAX - 1 );

		strcpy( gszallsentencenames[gcallsentences++], pString );

		j--;
		if ( j <= i )
			continue;
		if ( !isdigit( buffer[j] ) )
			continue;

		// cut out suffix numbers
		while ( j > i && isdigit( buffer[j] ) )
			j--;

		if ( j <= i )
			continue;

		buffer[j + 1] = 0;

		// if new name doesn't match previous group name,
		// make a new group.

		if ( strcmp( szgroup, &( buffer[i] ) ) )
		{
			// name doesn't match with prev name,
			// copy name into group, init count to 1
			isentencegs++;
			if ( isentencegs >= CSENTENCEG_MAX )
			{
				ALERT( at_error, "Too many sentence groups in sentences.txt!\n" );
				break;
			}

			strcpy( rgsentenceg[isentencegs].szgroupname, &( buffer[i] ) );
			rgsentenceg[isentencegs].count = 1;

			strcpy( szgroup, &( buffer[i] ) );

			continue;
		}
		else
		{
			// name matches with previous, increment group count
			if ( isentencegs >= 0 && isentencegs < CSENTENCEG_MAX )
				rgsentenceg[isentencegs].count++;
		}
	}

	g_engfuncs.pfnFreeFile( pMemFile );

	fSentencesInit = TRUE;

	// init lru lists

	i = 0;

	while ( i < CSENTENCEG_MAX && rgsentenceg[i].count )
	{
		USENTENCEG_InitLRU( &( rgsentenceg[i].rgblru[0] ), rgsentenceg[i].count );
		i++;
	}
}

// convert sentence (sample) name to !sentencenum, return !sentencenum

int SENTENCEG_Lookup( const char *sample, char *sentencenum )
{
	char sznum[8];

	int i;
	// this is a sentence name; lookup sentence number
	// and give to engine as string.
	for ( i = 0; i < gcallsentences; i++ )
		if ( !stricmp( gszallsentencenames[i], sample + 1 ) )
		{
			if ( sentencenum )
			{
				strcpy( sentencenum, "!" );
				sprintf( sznum, "%d", i );
				strcat( sentencenum, sznum );
			}
			return i;
		}
	// sentence name not found!
	return -1;
}

void EMIT_SOUND_SUIT( edict_t *entity, const char *sample )
{
	float fvol;
	int pitch = PITCH_NORM;

	fvol = CVAR_GET_FLOAT( "suitvolume" );
	if ( RANDOM_LONG( 0, 1 ) )
		pitch = RANDOM_LONG( 0, 6 ) + 98;

	if ( fvol > 0.05 )
		EMIT_SOUND_DYN( entity, CHAN_STATIC, sample, fvol, ATTN_NORM, 0, pitch );
}

// play a sentence, randomly selected from the passed in group id, over the HEV suit speaker

void EMIT_GROUPID_SUIT( edict_t *entity, int isentenceg )
{
	float fvol;
	int pitch = PITCH_NORM;

	if ( isentenceg < 0 || isentenceg >= CSENTENCEG_MAX )
		return;

	fvol = CVAR_GET_FLOAT( "suitvolume" );
	if ( RANDOM_LONG( 0, 1 ) )
		pitch = RANDOM_LONG( 0, 6 ) + 98;

	if ( fvol > 0.05 )
		SENTENCEG_PlayRndI( entity, isentenceg, fvol, ATTN_NORM, 0, pitch );
}

// play a sentence, randomly selected from the passed in groupname

void EMIT_GROUPNAME_SUIT( edict_t *entity, const char *groupname )
{
	float fvol;
	int pitch = PITCH_NORM;

	fvol = CVAR_GET_FLOAT( "suitvolume" );
	if ( RANDOM_LONG( 0, 1 ) )
		pitch = RANDOM_LONG( 0, 6 ) + 98;

	if ( fvol > 0.05 )
		SENTENCEG_PlayRndSz( entity, groupname, fvol, ATTN_NORM, 0, pitch );
}

// ===================== MATERIAL TYPE DETECTION, MAIN ROUTINES ========================
//
// Used to detect the texture the player is standing on, map the
// texture name to a material type.  Play footstep sound based
// on material type.
