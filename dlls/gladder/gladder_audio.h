/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Arcade Audio Cues & Soundscapes (SPEC §8, GitHub Issue #11)
 *
 ****/

#pragma once

#ifndef GLADDER_AUDIO_H
#define GLADDER_AUDIO_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"

enum EGladderAudioCue
{
	GLADDER_CUE_WAVE_START = 0,
	GLADDER_CUE_WAVE_COMPLETE,
	GLADDER_CUE_MUTATOR_WARNING,
	GLADDER_CUE_COLLECTIBLE,
	GLADDER_CUE_MATCH_END,
	GLADDER_CUE_DENY,
	GLADDER_CUE_ALL_HOSTILES_ELIMINATED,
	GLADDER_CUE_COUNT
};

class GladderAudio
{
  public:
	// Precaches all sound assets associated with arcade audio cues
	static void Precache( void );

	// Dispatches a designated audio cue through an entity or broadcasts to all players
	static void PlayCue( EGladderAudioCue cue, CBaseEntity *pEntity = nullptr, float flVolume = 1.0f );

	// Terminates an active looping audio cue on an entity or player
	static void StopCue( EGladderAudioCue cue, CBaseEntity *pEntity = nullptr );

	// Cue sound path accessor & override for custom map/mod soundscapes
	static const char *GetCueSoundPath( EGladderAudioCue cue );
	static void SetCueSoundPath( EGladderAudioCue cue, const char *pszPath );
	static void ResetCueSoundPaths( void );
};

#endif // GLADDER_AUDIO_H
