/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Gladder Camera: End-of-Run Summary Dynamic Backdrop Camera (SPEC §6, §9.2, Issue #52)
 *
 ****/

#include "gladder_entities.h"

LINK_ENTITY_TO_CLASS( gladder_camera, CGladderCamera );

void CGladderCamera::Precache( void )
{
	PRECACHE_MODEL( "models/player.mdl" );
}

void CGladderCamera::Spawn( void )
{
	Precache();
	CTriggerCamera::Spawn();

	// In GoldSrc, entities with modelindex == 0 are omitted from SV_WriteEntitiesToClient.
	// Setting player.mdl with renderamt=0 ensures the camera's origin and angles are networked
	// cleanly to clients while remaining completely invisible.
	SET_MODEL( edict(), "models/player.mdl" );
	pev->rendermode = kRenderTransTexture;
	pev->renderamt  = 0;
	pev->solid      = SOLID_NOT;
	pev->movetype   = MOVETYPE_NONE;
}
