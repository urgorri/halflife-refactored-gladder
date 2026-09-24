#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "core/saverestore.h"
#include "gameplay/gamerules.h"
#include "systems/triggers_brush.h"
#include "systems/triggers_point.h"

extern void SetMovedir( entvars_t *pev );
extern DLL_GLOBAL BOOL g_fGameOver;
extern int gmsgServerName;

#define MAX_ENTITY 512

LINK_ENTITY_TO_CLASS( trigger, CBaseTrigger );

void CBaseTrigger::InitTrigger()
{
	if ( pev->angles != g_vecZero )
		SetMovedir( pev );
	pev->solid    = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	if ( CVAR_GET_FLOAT( "showtriggers" ) == 0 )
		SetBits( pev->effects, EF_NODRAW );
}

void CBaseTrigger::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "damage" ) )
	{
		pev->dmg       = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "count" ) )
	{
		m_cTriggersLeft = (int)atof( pkvd->szValue );
		pkvd->fHandled  = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "damagetype" ) )
	{
		m_bitsDamageInflict = atoi( pkvd->szValue );
		pkvd->fHandled      = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CBaseTrigger::CounterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_cTriggersLeft--;
	m_hActivator = pActivator;

	if ( m_cTriggersLeft < 0 )
		return;

	BOOL fTellActivator =
	    ( m_hActivator != 0 ) &&
	    FClassnameIs( m_hActivator->pev, "player" ) &&
	    !FBitSet( pev->spawnflags, SPAWNFLAG_NOMESSAGE );
	if ( m_cTriggersLeft != 0 )
	{
		if ( fTellActivator )
		{
			switch ( m_cTriggersLeft )
			{
			case 1:
				ALERT( at_console, "Only 1 more to go..." );
				break;
			case 2:
				ALERT( at_console, "Only 2 more to go..." );
				break;
			case 3:
				ALERT( at_console, "Only 3 more to go..." );
				break;
			default:
				ALERT( at_console, "There are more to go..." );
				break;
			}
		}
		return;
	}

	if ( fTellActivator )
		ALERT( at_console, "Sequence completed!" );

	ActivateMultiTrigger( m_hActivator );
}

void CBaseTrigger::TeleportTouch( CBaseEntity *pOther )
{
	entvars_t *pevToucher = pOther->pev;
	edict_t *pentTarget   = NULL;

	if ( !FBitSet( pevToucher->flags, FL_CLIENT | FL_MONSTER ) )
		return;

	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	if ( !( pev->spawnflags & SF_TRIGGER_ALLOWMONSTERS ) )
	{
		if ( FBitSet( pevToucher->flags, FL_MONSTER ) )
			return;
	}

	if ( ( pev->spawnflags & SF_TRIGGER_NOCLIENTS ) )
	{
		if ( pOther->IsPlayer() )
			return;
	}

	pentTarget = FIND_ENTITY_BY_TARGETNAME( pentTarget, STRING( pev->target ) );
	if ( FNullEnt( pentTarget ) )
		return;

	Vector tmp = VARS( pentTarget )->origin;

	if ( pOther->IsPlayer() )
		tmp.z -= pOther->pev->mins.z;

	tmp.z++;

	pevToucher->flags &= ~FL_ONGROUND;

	UTIL_SetOrigin( pevToucher, tmp );

	pevToucher->angles = pentTarget->v.angles;

	if ( pOther->IsPlayer() )
		pevToucher->v_angle = pentTarget->v.angles;

	pevToucher->fixangle = TRUE;
	pevToucher->velocity = pevToucher->basevelocity = g_vecZero;
}

void CBaseTrigger::MultiTouch( CBaseEntity *pOther )
{
	entvars_t *pevToucher = pOther->pev;

	if ( ( ( pevToucher->flags & FL_CLIENT ) && !( pev->spawnflags & SF_TRIGGER_NOCLIENTS ) ) ||
	     ( ( pevToucher->flags & FL_MONSTER ) && ( pev->spawnflags & SF_TRIGGER_ALLOWMONSTERS ) ) ||
	     ( ( pev->spawnflags & SF_TRIGGER_PUSHABLES ) && FClassnameIs( pevToucher, "func_pushable" ) ) )
	{
		ActivateMultiTrigger( pOther );
	}
}

void CBaseTrigger::ActivateMultiTrigger( CBaseEntity *pActivator )
{
	if ( pev->nextthink > gpGlobals->time )
		return;

	if ( !UTIL_IsMasterTriggered( m_sMaster, pActivator ) )
		return;

	if ( FClassnameIs( pev, "trigger_secret" ) )
	{
		if ( pev->enemy == NULL || !FClassnameIs( pev->enemy, "player" ) )
			return;
		gpGlobals->found_secrets++;
	}

	if ( !FStringNull( pev->noise ) )
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, (char *)STRING( pev->noise ), 1, ATTN_NORM );

	m_hActivator = pActivator;
	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );

	if ( pev->message && pActivator->IsPlayer() )
		UTIL_ShowMessage( STRING( pev->message ), pActivator );

	if ( m_flWait > 0 )
	{
		SetThink( &CBaseTrigger::MultiWaitOver );
		pev->nextthink = gpGlobals->time + m_flWait;
	}
	else
	{
		SetTouch( NULL );
		pev->nextthink = gpGlobals->time + 0.1;
		SetThink( &CBaseTrigger::SUB_Remove );
	}
}

void CBaseTrigger::MultiWaitOver( void )
{
	SetThink( NULL );
}

void CBaseTrigger::ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->solid == SOLID_NOT )
	{
		pev->solid = SOLID_TRIGGER;
		gpGlobals->force_retouch++;
	}
	else
	{
		pev->solid = SOLID_NOT;
	}
	UTIL_SetOrigin( pev, pev->origin );
}

void CBaseTrigger::HurtTouch( CBaseEntity *pOther )
{
	float fldmg;

	if ( !pOther->pev->takedamage )
		return;

	if ( ( pev->spawnflags & SF_TRIGGER_HURT_CLIENTONLYTOUCH ) && !pOther->IsPlayer() )
		return;

	if ( ( pev->spawnflags & SF_TRIGGER_HURT_NO_CLIENTS ) && pOther->IsPlayer() )
		return;

	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( pev->dmgtime > gpGlobals->time )
		{
			if ( gpGlobals->time != pev->pain_finished )
			{
				if ( pOther->IsPlayer() )
				{
					int playerMask = 1 << ( pOther->entindex() - 1 );
					if ( pev->impulse & playerMask )
						return;
					pev->impulse |= playerMask;
				}
				else
					return;
			}
		}
		else
		{
			pev->impulse = 0;
			if ( pOther->IsPlayer() )
			{
				int playerMask = 1 << ( pOther->entindex() - 1 );
				pev->impulse |= playerMask;
			}
		}
	}
	else
	{
		if ( pev->dmgtime > gpGlobals->time && gpGlobals->time != pev->pain_finished )
			return;
	}

	fldmg = pev->dmg * 0.5;

	if ( fldmg < 0 )
	{
		BOOL bApplyHeal = TRUE;

		if ( g_pGameRules->IsMultiplayer() && pOther->IsPlayer() )
			bApplyHeal = pOther->pev->deadflag == DEAD_NO;

		if ( bApplyHeal )
			pOther->TakeHealth( -fldmg, m_bitsDamageInflict );
	}
	else
		pOther->TakeDamage( pev, pev, fldmg, m_bitsDamageInflict );

	pev->pain_finished = gpGlobals->time;
	pev->dmgtime       = gpGlobals->time + 0.5;

	if ( pev->target )
	{
		if ( pev->spawnflags & SF_TRIGGER_HURT_CLIENTONLYFIRE )
		{
			if ( !pOther->IsPlayer() )
				return;
		}

		SUB_UseTargets( pOther, USE_TOGGLE, 0 );
		if ( pev->spawnflags & SF_TRIGGER_HURT_TARGETONCE )
			pev->target = 0;
	}
}

//=========================================================
// CTriggerMultiple & CTriggerOnce
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_multiple, CTriggerMultiple );

void CTriggerMultiple::Spawn( void )
{
	if ( m_flWait == 0 )
		m_flWait = 0.2;

	InitTrigger();
	ASSERTSZ( pev->health == 0, "trigger_multiple with health" );
	SetTouch( &CTriggerMultiple::MultiTouch );
}

LINK_ENTITY_TO_CLASS( trigger_once, CTriggerOnce );

void CTriggerOnce::Spawn( void )
{
	m_flWait = -1;
	CTriggerMultiple::Spawn();
}

//=========================================================
// CTriggerCounter
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_counter, CTriggerCounter );

void CTriggerCounter::Spawn( void )
{
	m_flWait = -1;
	if ( m_cTriggersLeft == 0 )
		m_cTriggersLeft = 2;
	SetUse( &CBaseTrigger::CounterUse );
}

//=========================================================
// CTriggerHurt
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_hurt, CTriggerHurt );

void CTriggerHurt::Spawn( void )
{
	InitTrigger();
	SetTouch( &CTriggerHurt::HurtTouch );

	if ( !FStringNull( pev->targetname ) )
		SetUse( &CBaseTrigger::ToggleUse );
	else
		SetUse( NULL );

	if ( m_bitsDamageInflict & DMG_RADIATION )
	{
		SetThink( &CTriggerHurt::RadiationThink );
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.0, 0.5 );
	}

	if ( FBitSet( pev->spawnflags, SF_TRIGGER_HURT_START_OFF ) )
		pev->solid = SOLID_NOT;

	UTIL_SetOrigin( pev, pev->origin );
}

void CTriggerHurt::RadiationThink( void )
{
	edict_t *pentPlayer;
	CBasePlayer *pPlayer = NULL;
	float flRange;
	entvars_t *pevTarget;
	Vector vecSpot1, vecSpot2, vecRange;
	Vector origin, view_ofs;

	origin   = pev->origin;
	view_ofs = pev->view_ofs;

	pev->origin   = ( pev->absmin + pev->absmax ) * 0.5;
	pev->view_ofs = pev->view_ofs * 0.0;

	pentPlayer = FIND_CLIENT_IN_PVS( edict() );

	pev->origin   = origin;
	pev->view_ofs = view_ofs;

	if ( !FNullEnt( pentPlayer ) )
	{
		pPlayer   = GetClassPtr( (CBasePlayer *)VARS( pentPlayer ) );
		pevTarget = VARS( pentPlayer );

		vecSpot1 = ( pev->absmin + pev->absmax ) * 0.5;
		vecSpot2 = ( pevTarget->absmin + pevTarget->absmax ) * 0.5;

		vecRange = vecSpot1 - vecSpot2;
		flRange  = vecRange.Length();

		if ( pPlayer->m_flgeigerRange >= flRange )
			pPlayer->m_flgeigerRange = flRange;
	}

	pev->nextthink = gpGlobals->time + 0.25;
}

//=========================================================
// CTriggerPush
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_push, CTriggerPush );

void CTriggerPush::KeyValue( KeyValueData *pkvd )
{
	CBaseTrigger::KeyValue( pkvd );
}

void CTriggerPush::Spawn()
{
	if ( pev->angles == g_vecZero )
		pev->angles.y = 360;
	InitTrigger();

	if ( pev->speed == 0 )
		pev->speed = 100;

	if ( FBitSet( pev->spawnflags, SF_TRIGGER_PUSH_START_OFF ) )
		pev->solid = SOLID_NOT;

	SetUse( &CBaseTrigger::ToggleUse );
	UTIL_SetOrigin( pev, pev->origin );
}

void CTriggerPush::Touch( CBaseEntity *pOther )
{
	entvars_t *pevToucher = pOther->pev;

	switch ( pevToucher->movetype )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
	case MOVETYPE_NOCLIP:
	case MOVETYPE_FOLLOW:
		return;
	}

	if ( pevToucher->solid != SOLID_NOT && pevToucher->solid != SOLID_BSP )
	{
		if ( FBitSet( pev->spawnflags, SF_TRIG_PUSH_ONCE ) )
		{
			pevToucher->velocity = pevToucher->velocity + ( pev->speed * pev->movedir );
			if ( pevToucher->velocity.z > 0 )
				pevToucher->flags &= ~FL_ONGROUND;
			UTIL_Remove( this );
		}
		else
		{
			Vector vecPush = ( pev->speed * pev->movedir );
			if ( pevToucher->flags & FL_BASEVELOCITY )
				vecPush = vecPush + pevToucher->basevelocity;

			pevToucher->basevelocity = vecPush;
			pevToucher->flags |= FL_BASEVELOCITY;
		}
	}
}

//=========================================================
// CTriggerTeleport & info_teleport_destination
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_teleport, CTriggerTeleport );

void CTriggerTeleport::Spawn( void )
{
	InitTrigger();
	SetTouch( &CTriggerTeleport::TeleportTouch );
}

LINK_ENTITY_TO_CLASS( info_teleport_destination, CPointEntity );

//=========================================================
// CLadder
//=========================================================
LINK_ENTITY_TO_CLASS( func_ladder, CLadder );

void CLadder::KeyValue( KeyValueData *pkvd )
{
	CBaseTrigger::KeyValue( pkvd );
}

void CLadder::Precache( void )
{
	pev->solid = SOLID_NOT;
	pev->skin  = CONTENTS_LADDER;
	if ( CVAR_GET_FLOAT( "showtriggers" ) == 0 )
	{
		pev->rendermode = kRenderTransTexture;
		pev->renderamt  = 0;
	}
	pev->effects &= ~EF_NODRAW;
}

void CLadder::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	pev->movetype = MOVETYPE_PUSH;
}

//=========================================================
// CTriggerVolume
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_transition, CTriggerVolume );

void CTriggerVolume::Spawn( void )
{
	pev->solid    = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	pev->model      = NULL;
	pev->modelindex = 0;
}

//=========================================================
// CTriggerMonsterJump
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_monsterjump, CTriggerMonsterJump );

void CTriggerMonsterJump::Spawn( void )
{
	SetMovedir( pev );
	InitTrigger();

	pev->nextthink = 0;
	pev->speed     = 200;
	m_flHeight     = 150;

	if ( !FStringNull( pev->targetname ) )
	{
		pev->solid = SOLID_NOT;
		UTIL_SetOrigin( pev, pev->origin );
		SetUse( &CBaseTrigger::ToggleUse );
	}
}

void CTriggerMonsterJump::Think( void )
{
	pev->solid = SOLID_NOT;
	UTIL_SetOrigin( pev, pev->origin );
	SetThink( NULL );
}

void CTriggerMonsterJump::Touch( CBaseEntity *pOther )
{
	entvars_t *pevOther = pOther->pev;

	if ( !FBitSet( pevOther->flags, FL_MONSTER ) )
		return;

	pevOther->origin.z += 1;

	if ( FBitSet( pevOther->flags, FL_ONGROUND ) )
		pevOther->flags &= ~FL_ONGROUND;

	pevOther->velocity = pev->movedir * pev->speed;
	pevOther->velocity.z += m_flHeight;
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// CTriggerGravity
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_gravity, CTriggerGravity );

void CTriggerGravity::Spawn( void )
{
	InitTrigger();
	SetTouch( &CTriggerGravity::GravityTouch );
}

void CTriggerGravity::GravityTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
		return;

	pOther->pev->gravity = pev->gravity;
}

//=========================================================
// CFrictionModifier
//=========================================================
LINK_ENTITY_TO_CLASS( func_friction, CFrictionModifier );

TYPEDESCRIPTION CFrictionModifier::m_SaveData[] =
    {
        DEFINE_FIELD( CFrictionModifier, m_frictionFraction, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CFrictionModifier, CBaseEntity );

void CFrictionModifier::Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	pev->movetype = MOVETYPE_NONE;
	SetTouch( &CFrictionModifier::ChangeFriction );
}

void CFrictionModifier::ChangeFriction( CBaseEntity *pOther )
{
	if ( pOther->pev->movetype != MOVETYPE_BOUNCEMISSILE && pOther->pev->movetype != MOVETYPE_BOUNCE )
		pOther->pev->friction = m_frictionFraction;
}

void CFrictionModifier::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "modifier" ) )
	{
		m_frictionFraction = atof( pkvd->szValue ) / 100.0;
		pkvd->fHandled     = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

//=========================================================
// CTriggerEndSection
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_endsection, CTriggerEndSection );

void CTriggerEndSection::EndSectionUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator && !pActivator->IsNetClient() )
		return;

	SetUse( NULL );

	if ( pev->message )
		g_engfuncs.pfnEndSection( STRING( pev->message ) );

	UTIL_Remove( this );
}

void CTriggerEndSection::Spawn( void )
{
	if ( g_pGameRules->IsDeathmatch() )
	{
		REMOVE_ENTITY( ENT( pev ) );
		return;
	}

	InitTrigger();
	SetUse( &CTriggerEndSection::EndSectionUse );
	if ( !( pev->spawnflags & SF_ENDSECTION_USEONLY ) )
		SetTouch( &CTriggerEndSection::EndSectionTouch );
}

void CTriggerEndSection::EndSectionTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsNetClient() )
		return;

	SetTouch( NULL );

	if ( pev->message )
		g_engfuncs.pfnEndSection( STRING( pev->message ) );

	UTIL_Remove( this );
}

void CTriggerEndSection::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "section" ) )
	{
		pev->message   = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}


//=========================================================
// CTriggerSave
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_autosave, CTriggerSave );

void CTriggerSave::Spawn( void )
{
	if ( !g_pGameRules->FAllowAutoSave() )
	{
		REMOVE_ENTITY( ENT( pev ) );
		return;
	}

	InitTrigger();
	SetTouch( &CTriggerSave::SaveTouch );
}

void CTriggerSave::SaveTouch( CBaseEntity *pOther )
{
	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	if ( !pOther->IsPlayer() )
		return;

	SetTouch( NULL );
	UTIL_Remove( this );
	SERVER_COMMAND( "autosave\n" );
}

//=========================================================
// CChangeLevel & NextLevel
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_changelevel, CChangeLevel );

TYPEDESCRIPTION CChangeLevel::m_SaveData[] =
    {
        DEFINE_ARRAY( CChangeLevel, m_szMapName, FIELD_CHARACTER, cchMapNameMost ),
        DEFINE_ARRAY( CChangeLevel, m_szLandmarkName, FIELD_CHARACTER, cchMapNameMost ),
        DEFINE_FIELD( CChangeLevel, m_changeTarget, FIELD_STRING ),
        DEFINE_FIELD( CChangeLevel, m_changeTargetDelay, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CChangeLevel, CBaseTrigger );

void CChangeLevel::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "map" ) )
	{
		if ( strlen( pkvd->szValue ) >= cchMapNameMost )
			ALERT( at_error, "Map name '%s' too long (32 chars)\n", pkvd->szValue );
		strcpy( m_szMapName, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "landmark" ) )
	{
		if ( strlen( pkvd->szValue ) >= cchMapNameMost )
			ALERT( at_error, "Landmark name '%s' too long (32 chars)\n", pkvd->szValue );
		strcpy( m_szLandmarkName, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "changetarget" ) )
	{
		m_changeTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "changedelay" ) )
	{
		m_changeTargetDelay = atof( pkvd->szValue );
		pkvd->fHandled      = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}

void CChangeLevel::Spawn( void )
{
	if ( FStrEq( m_szMapName, "" ) )
		ALERT( at_console, "a trigger_changelevel doesn't have a map" );

	if ( FStrEq( m_szLandmarkName, "" ) )
		ALERT( at_console, "trigger_changelevel to %s doesn't have a landmark", m_szMapName );

	if ( !FStringNull( pev->targetname ) )
	{
		SetUse( &CChangeLevel::UseChangeLevel );
	}
	InitTrigger();
	if ( !( pev->spawnflags & SF_CHANGELEVEL_USEONLY ) )
		SetTouch( &CChangeLevel::TouchChangeLevel );
//	ALERT( at_console, "TRANSITION: %s (%s)\n", m_szMapName, m_szLandmarkName );
}

void CChangeLevel::ExecuteChangeLevel( void )
{
	MESSAGE_BEGIN( MSG_ALL, SVC_CDTRACK );
	WRITE_BYTE( 3 );
	WRITE_BYTE( 3 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, SVC_INTERMISSION );
	MESSAGE_END();
}

FILE_GLOBAL char st_szNextMap[cchMapNameMost];
FILE_GLOBAL char st_szNextSpot[cchMapNameMost];

edict_t *CChangeLevel::FindLandmark( const char *pLandmarkName )
{
	edict_t *pentLandmark;

	pentLandmark = FIND_ENTITY_BY_STRING( NULL, "targetname", pLandmarkName );
	while ( !FNullEnt( pentLandmark ) )
	{
		// Found the landmark
		if ( FClassnameIs( pentLandmark, "info_landmark" ) )
			return pentLandmark;
		else
			pentLandmark = FIND_ENTITY_BY_STRING( pentLandmark, "targetname", pLandmarkName );
	}
	ALERT( at_error, "Can't find landmark %s\n", pLandmarkName );
	return NULL;
}

//=========================================================
// CChangeLevel :: Use - allows level transitions to be 
// triggered by buttons, etc.
//
//=========================================================
void CChangeLevel::UseChangeLevel( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	ChangeLevelNow( pActivator );
}

void CChangeLevel::ChangeLevelNow( CBaseEntity *pActivator )
{
	edict_t *pentLandmark;
	LEVELLIST levels[16];

	ASSERT( !FStrEq( m_szMapName, "" ) );

	// Don't work in deathmatch
	if ( g_pGameRules->IsDeathmatch() )
		return;

	// Some people are firing these multiple times in a frame, disable
	if ( gpGlobals->time == pev->dmgtime )
		return;

	pev->dmgtime = gpGlobals->time;

	CBaseEntity *pPlayer = CBaseEntity::Instance( g_engfuncs.pfnPEntityOfEntIndex( 1 ) );
	if ( !InTransitionVolume( pPlayer, m_szLandmarkName ) )
	{
		ALERT( at_aiconsole, "Player isn't in the transition volume %s, aborting\n", m_szLandmarkName );
		return;
	}

	// Create an entity to fire the changetarget
	if ( m_changeTarget )
	{
		CFireAndDie *pFireAndDie = GetClassPtr( (CFireAndDie *)NULL );
		if ( pFireAndDie )
		{
			// Set target and delay
			pFireAndDie->pev->target = m_changeTarget;
			pFireAndDie->m_flDelay   = m_changeTargetDelay;
			pFireAndDie->pev->origin = pPlayer->pev->origin;
			// Call spawn
			DispatchSpawn( pFireAndDie->edict() );
		}
	}
	// This object will get removed in the call to CHANGE_LEVEL, copy the params into "safe" memory
	strcpy( st_szNextMap, m_szMapName );

	m_hActivator = pActivator;
	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
	st_szNextSpot[0] = 0; // Init landmark to NULL

	// look for a landmark entity		
	pentLandmark = FindLandmark( m_szLandmarkName );
	if ( !FNullEnt( pentLandmark ) )
	{
		strcpy( st_szNextSpot, m_szLandmarkName );
		gpGlobals->vecLandmarkOffset = VARS( pentLandmark )->origin;
	}
//	ALERT( at_console, "Level touches %d levels\n", ChangeList( levels, 16 ) );
	ALERT( at_console, "CHANGE LEVEL: %s %s\n", st_szNextMap, st_szNextSpot );
	CHANGE_LEVEL( st_szNextMap, st_szNextSpot );
}

//
// GLOBALS ASSUMED SET:  st_szNextMap
//
void CChangeLevel::TouchChangeLevel( CBaseEntity *pOther )
{
	if ( !FClassnameIs( pOther->pev, "player" ) )
		return;

	ChangeLevelNow( pOther );
}

void CChangeLevel::TriggerChangeLevel( void )
{
	ChangeLevelNow( m_hActivator );
}

// Add a transition to the list, but ignore duplicates 
// (a designer may have placed multiple trigger_changelevels with the same landmark)
int CChangeLevel::AddTransitionToList( LEVELLIST *pLevelList, int listCount, const char *pMapName, const char *pLandmarkName, edict_t *pentLandmark )
{
	int i;

	if ( !pLevelList || !pMapName || !pLandmarkName || !pentLandmark )
		return 0;

	for ( i = 0; i < listCount; i++ )
	{
		if ( pLevelList[i].pentLandmark == pentLandmark && strcmp( pLevelList[i].mapName, pMapName ) == 0 )
			return 0;
	}
	strcpy( pLevelList[listCount].mapName, pMapName );
	strcpy( pLevelList[listCount].landmarkName, pLandmarkName );
	pLevelList[listCount].pentLandmark      = pentLandmark;
	pLevelList[listCount].vecLandmarkOrigin = VARS( pentLandmark )->origin;

	return 1;
}

int BuildChangeList( LEVELLIST *pLevelList, int maxList )
{
	return CChangeLevel::ChangeList( pLevelList, maxList );
}

int CChangeLevel::InTransitionVolume( CBaseEntity *pEntity, char *pVolumeName )
{
	edict_t *pentVolume;

	if ( pEntity->ObjectCaps() & FCAP_FORCE_TRANSITION )
		return 1;

	// If you're following another entity, follow it through the transition (weapons follow the player)
	if ( pEntity->pev->movetype == MOVETYPE_FOLLOW )
	{
		if ( pEntity->pev->aiment != NULL )
			pEntity = CBaseEntity::Instance( pEntity->pev->aiment );
	}

	int inVolume = 1; // Unless we find a trigger_transition, everything is in the volume

	pentVolume = FIND_ENTITY_BY_TARGETNAME( NULL, pVolumeName );
	while ( !FNullEnt( pentVolume ) )
	{
		CBaseEntity *pVolume = CBaseEntity::Instance( pentVolume );

		if ( pVolume && FClassnameIs( pVolume->pev, "trigger_transition" ) )
		{
			if ( pVolume->Intersects( pEntity ) ) // It touches one, it's in the volume
				return 1;
			else
				inVolume = 0; // Found a trigger_transition, but I don't intersect it -- if I don't find another, don't go!
		}
		pentVolume = FIND_ENTITY_BY_TARGETNAME( pentVolume, pVolumeName );
	}

	return inVolume;
}

// We can only ever move 512 entities across a transition
#define MAX_ENTITY 512

// This has grown into a complicated beast
// Can we make this more elegant?
// This builds the list of all transitions on this level and which entities are in their PVS's and can / should
// be moved across.
int CChangeLevel::ChangeList( LEVELLIST *pLevelList, int maxList )
{
	edict_t *pentChangelevel, *pentLandmark;
	int i, count;

	count = 0;

	// Find all of the possible level changes on this BSP
	pentChangelevel = FIND_ENTITY_BY_STRING( NULL, "classname", "trigger_changelevel" );
	if ( FNullEnt( pentChangelevel ) )
		return 0;
	while ( !FNullEnt( pentChangelevel ) )
	{
		CChangeLevel *pTrigger;

		pTrigger = GetClassPtr( (CChangeLevel *)VARS( pentChangelevel ) );
		if ( pTrigger )
		{
			// Find the corresponding landmark
			pentLandmark = FindLandmark( pTrigger->m_szLandmarkName );
			if ( pentLandmark )
			{
				// Build a list of unique transitions
				if ( AddTransitionToList( pLevelList, count, pTrigger->m_szMapName, pTrigger->m_szLandmarkName, pentLandmark ) )
				{
					count++;
					if ( count >= maxList ) // FULL!!
						break;
				}
			}
		}
		pentChangelevel = FIND_ENTITY_BY_STRING( pentChangelevel, "classname", "trigger_changelevel" );
	}

	if ( gpGlobals->pSaveData && ( (SAVERESTOREDATA *)gpGlobals->pSaveData )->pTable )
	{
		CSave saveHelper( (SAVERESTOREDATA *)gpGlobals->pSaveData );

		for ( i = 0; i < count; i++ )
		{
			int j, entityCount = 0;
			CBaseEntity *pEntList[MAX_ENTITY];
			int entityFlags[MAX_ENTITY];

			// Follow the linked list of entities in the PVS of the transition landmark
			edict_t *pent = UTIL_EntitiesInPVS( pLevelList[i].pentLandmark );

			// Build a list of valid entities in this linked list (we're going to use pent->v.chain again)
			while ( !FNullEnt( pent ) )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( pent );
				if ( pEntity )
				{
//					ALERT( at_console, "Trying %s\n", STRING(pEntity->pev->classname) );
					int caps = pEntity->ObjectCaps();
					if ( !( caps & FCAP_DONT_SAVE ) )
					{
						int flags = 0;

						// If this entity can be moved or is global, mark it
						if ( caps & FCAP_ACROSS_TRANSITION )
							flags |= FENTTABLE_MOVEABLE;
						if ( pEntity->pev->globalname && !pEntity->IsDormant() )
							flags |= FENTTABLE_GLOBAL;
						if ( flags )
						{
							pEntList[entityCount]    = pEntity;
							entityFlags[entityCount] = flags;
							entityCount++;
							if ( entityCount > MAX_ENTITY )
								ALERT( at_error, "Too many entities across a transition!" );
						}
					}
				}
				pent = pent->v.chain;
			}

			for ( j = 0; j < entityCount; j++ )
			{
				// Check to make sure the entity isn't screened out by a trigger_transition
				if ( entityFlags[j] && InTransitionVolume( pEntList[j], pLevelList[i].landmarkName ) )
				{
					// Mark entity table with 1<<i
					int index = saveHelper.EntityIndex( pEntList[j] );
					// Flag it with the level number
					saveHelper.EntityFlagsSet( index, entityFlags[j] | ( 1 << i ) );
				}
			}
		}
	}

	return count;
}

void NextLevel( void )
{
	edict_t *pent;
	CChangeLevel *pChange;

	pent = FIND_ENTITY_BY_CLASSNAME( NULL, "trigger_changelevel" );

	if ( FNullEnt( pent ) )
	{
		gpGlobals->mapname = ALLOC_STRING( "start" );
		pChange            = GetClassPtr( (CChangeLevel *)NULL );
		strcpy( pChange->m_szMapName, "start" );
	}
	else
		pChange = GetClassPtr( (CChangeLevel *)VARS( pent ) );

	strcpy( st_szNextMap, pChange->m_szMapName );
	g_fGameOver = TRUE;

	if ( pChange->pev->nextthink < gpGlobals->time )
	{
		pChange->SetThink( &CChangeLevel::ExecuteChangeLevel );
		pChange->pev->nextthink = gpGlobals->time + 0.1;
	}
}
