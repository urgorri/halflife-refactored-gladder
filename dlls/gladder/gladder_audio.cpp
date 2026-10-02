/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Arcade Audio Cues & Soundscapes (SPEC §8, GitHub Issue #11)
 *
 ****/

#include "gladder_audio.h"
#include <string>

static const char *s_defaultCuePaths[GLADDER_CUE_COUNT] = {
	"buttons/bell1.wav",       // GLADDER_CUE_WAVE_START
	"debris/beamstart14.wav",  // GLADDER_CUE_WAVE_COMPLETE
	"ambience/siren.wav",      // GLADDER_CUE_MUTATOR_WARNING
	"items/suitchargeno1.wav", // GLADDER_CUE_COLLECTIBLE
	"buttons/button10.wav"     // GLADDER_CUE_MATCH_END
};

static std::string s_activeCuePaths[GLADDER_CUE_COUNT];
static bool s_bPathsInitialized = false;

static void EnsurePathsInitialized( void )
{
	if ( !s_bPathsInitialized )
	{
		for ( int i = 0; i < GLADDER_CUE_COUNT; ++i )
		{
			s_activeCuePaths[i] = s_defaultCuePaths[i];
		}
		s_bPathsInitialized = true;
	}
}

void GladderAudio::Precache( void )
{
	EnsurePathsInitialized();
	for ( int i = 0; i < GLADDER_CUE_COUNT; ++i )
	{
		if ( !s_activeCuePaths[i].empty() )
		{
			PRECACHE_SOUND( (char *)s_activeCuePaths[i].c_str() );
		}
	}
}

void GladderAudio::PlayCue( EGladderAudioCue cue, CBaseEntity *pEntity, float flVolume )
{
	if ( cue < 0 || cue >= GLADDER_CUE_COUNT )
		return;

	EnsurePathsInitialized();
	const char *pszSound = s_activeCuePaths[cue].c_str();
	if ( !pszSound || !*pszSound )
		return;

	edict_t *pEdict = nullptr;
	if ( pEntity && pEntity->edict() )
	{
		pEdict = pEntity->edict();
	}
	else if ( gpGlobals )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( 1 );
		if ( pPlayer && pPlayer->edict() )
		{
			pEdict = pPlayer->edict();
		}
	}

	if ( pEdict )
	{
		EMIT_SOUND_DYN( pEdict, CHAN_STATIC, pszSound, flVolume, ATTN_NORM, 0, PITCH_NORM );
	}
}

const char *GladderAudio::GetCueSoundPath( EGladderAudioCue cue )
{
	if ( cue < 0 || cue >= GLADDER_CUE_COUNT )
		return "";

	EnsurePathsInitialized();
	return s_activeCuePaths[cue].c_str();
}

void GladderAudio::SetCueSoundPath( EGladderAudioCue cue, const char *pszPath )
{
	if ( cue < 0 || cue >= GLADDER_CUE_COUNT )
		return;

	EnsurePathsInitialized();
	s_activeCuePaths[cue] = pszPath ? pszPath : "";
}

void GladderAudio::ResetCueSoundPaths( void )
{
	s_bPathsInitialized = false;
	EnsurePathsInitialized();
}
