#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "systems/triggers_brush.h"

//=========================================================
// CTriggerCDAudio & PlayCDTrack
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_cdaudio, CTriggerCDAudio );

void PlayCDTrack( int iTrack )
{
	edict_t *pClient = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	if ( !pClient )
		return;

	if ( iTrack < -1 || iTrack > 30 )
	{
		ALERT( at_console, "TriggerCDAudio - Track %d out of range\n", iTrack );
		return;
	}

	if ( iTrack == -1 )
	{
		CLIENT_COMMAND( pClient, "cd stop\n" );
	}
	else
	{
		char string[64];
		sprintf( string, "cd play %3d\n", iTrack );
		CLIENT_COMMAND( pClient, string );
	}
}

void CTriggerCDAudio::Touch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
		return;

	PlayTrack();
}

void CTriggerCDAudio::Spawn( void )
{
	InitTrigger();
}

void CTriggerCDAudio::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	PlayTrack();
}

void CTriggerCDAudio::PlayTrack( void )
{
	PlayCDTrack( (int)pev->health );
	SetTouch( NULL );
	UTIL_Remove( this );
}
