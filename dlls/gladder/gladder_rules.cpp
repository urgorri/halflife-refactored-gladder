/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Gladder Game Rules Implementation
 *
 ****/

#include "gladder_rules.h"
#include "gladder_entities.h"
#include "gladder_usermsg.h"
#include "core/skill_manager.h"
#include "items/item_base.h"
#include "weapons/weapon_base.h"

// Gladder gamemode cvar (1 enables Gladder mode)
cvar_t gladder = { "gladder", "1", FCVAR_SERVER | FCVAR_ARCHIVE };
cvar_t gladder_timelimit = { "gladder_timelimit", "600", FCVAR_SERVER | FCVAR_ARCHIVE }; // default 10 minutes

// Block server console commands for saving and loading
static void Gladder_BlockSaveCmd( void )
{
	ALERT( at_console, "Saving and loading are disabled in Half-Life: Gladder.\n" );
}

CGladderRules::CGladderRules()
    : m_bInitialSpawnDone( false ),
      m_iTotalFrags( 0 ),
      m_iCollectiblesCount( 0 ),
      m_flLastTelemetryBroadcast( 0.0f )
{
	// Register cvars
	CVAR_REGISTER( &gladder );
	CVAR_REGISTER( &gladder_timelimit );

	// Intercept and disable host save/load console commands
	if ( g_engfuncs.pfnAddServerCommand )
	{
		g_engfuncs.pfnAddServerCommand( (char *)"save", Gladder_BlockSaveCmd );
		g_engfuncs.pfnAddServerCommand( (char *)"load", Gladder_BlockSaveCmd );
		g_engfuncs.pfnAddServerCommand( (char *)"quicksave", Gladder_BlockSaveCmd );
		g_engfuncs.pfnAddServerCommand( (char *)"quickload", Gladder_BlockSaveCmd );
		g_engfuncs.pfnAddServerCommand( (char *)"autosave", Gladder_BlockSaveCmd );
	}

	float flLimit = gladder_timelimit.value;
	if ( flLimit <= 0.0f )
		flLimit = 600.0f;
	m_waveManager.SetSessionTimeLimit( flLimit );

	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	m_waveManager.InitializeMatch( flTime );

	// Precache essential Gladder cues
	PRECACHE_SOUND( "debris/beamstart1.wav" );
	PRECACHE_SOUND( "buttons/bell1.wav" );

	CGameRules::RefreshSkillData();
}

CGladderRules::~CGladderRules()
{
}

void CGladderRules::Think( void )
{
	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	m_waveManager.Tick( flTime );

	// Broadcast periodic telemetry (e.g. every 0.25 seconds) to keep client HUD synchronized
	if ( flTime - m_flLastTelemetryBroadcast >= 0.25f )
	{
		m_flLastTelemetryBroadcast = flTime;
		BroadcastWaveUpdate();
		BroadcastTelemetryUpdate();
	}
}

void CGladderRules::PlayerSpawn( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	// Initial loadout at Point A: guaranteed HEV suit and Crowbar
	if ( !m_bInitialSpawnDone )
	{
		m_bInitialSpawnDone = true;

		// Equip HEV suit: set weapon bitmask directly to guarantee HUD/battery activation
		pPlayer->pev->weapons |= ( 1 << WEAPON_SUIT );

		// Equip Crowbar and select it into hands
		int iAutoWepSwitch = pPlayer->m_iAutoWepSwitch;
		pPlayer->m_iAutoWepSwitch = 1;
		pPlayer->GiveNamedItem( "weapon_crowbar" );
		pPlayer->SelectItem( "weapon_crowbar" );
		pPlayer->m_iAutoWepSwitch = iAutoWepSwitch;

		// Re-initialize match clock from actual player spawn time
		float flTime = gpGlobals ? gpGlobals->time : 0.0f;
		float flLimit = gladder_timelimit.value;
		if ( flLimit <= 0.0f )
			flLimit = 600.0f;
		m_waveManager.SetSessionTimeLimit( flLimit );
		m_waveManager.InitializeMatch( flTime );
	}

	BroadcastWaveUpdate( pPlayer );
	BroadcastTelemetryUpdate( pPlayer );
}

void CGladderRules::PlayerRespawn( CBasePlayer *pPlayer, BOOL fCopyCorpse )
{
	// Save/load elimination: Reload is completely blocked!
	// In Gladder, dying concludes the gauntlet run and transitions to summary screen.
}

BOOL CGladderRules::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if ( !pcmd )
		return FALSE;

	// Block all manual save/load/reload console commands
	if ( FStrEq( pcmd, "save" ) ||
	     FStrEq( pcmd, "load" ) ||
	     FStrEq( pcmd, "reload" ) ||
	     FStrEq( pcmd, "autosave" ) ||
	     FStrEq( pcmd, "quickload" ) ||
	     FStrEq( pcmd, "quicksave" ) )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "Save/Load and Reload are disabled in Half-Life: Gladder.\n" );
		return TRUE; // handled and suppressed
	}

	return FALSE;
}

void CGladderRules::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	m_waveManager.EndMatch( flTime, false );

	// Trigger Defeat Relays (event = 3)
	Gladder_FireWaveRelays( 3, pVictim );

	BroadcastWaveUpdate( pVictim );
	BroadcastTelemetryUpdate( pVictim );
}

void CGladderRules::MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	m_iTotalFrags++;
	BroadcastTelemetryUpdate();
}

float CGladderRules::FlHealthChargerCapacity( void )
{
	// Diminishing capacity per wave: degrades by 10 points per wave down to a minimum of 20
	int iWave = m_waveManager.GetWaveNumber();
	float baseCapacity = gSkillData.healthchargerCapacity; // default 50
	float degraded = baseCapacity - static_cast<float>( ( iWave - 1 ) * 10 );
	if ( degraded < 20.0f )
		degraded = 20.0f;
	return degraded;
}

float CGladderRules::FlHEVChargerCapacity( void )
{
	int iWave = m_waveManager.GetWaveNumber();
	float baseCapacity = gSkillData.suitchargerCapacity; // default 75
	float degraded = baseCapacity - static_cast<float>( ( iWave - 1 ) * 15 );
	if ( degraded < 25.0f )
		degraded = 25.0f;
	return degraded;
}

void CGladderRules::OnWaveTriggerStart( CBaseEntity *pActivator )
{
	if ( m_waveManager.GetState() == GLADDER_STATE_MATCH_OVER )
		return;

	// Only start if waiting
	if ( m_waveManager.GetState() == GLADDER_STATE_WAITING_FOR_START ||
	     m_waveManager.GetState() == GLADDER_STATE_WAVE_COMPLETED )
	{
		float flTime = gpGlobals ? gpGlobals->time : 0.0f;
		m_waveManager.StartWave( flTime );

		// Acoustic cue for wave start
		if ( pActivator && pActivator->edict() )
		{
			EMIT_SOUND( pActivator->edict(), CHAN_BODY, "debris/beamstart1.wav", 1.0f, ATTN_NORM );
		}

		// Fire start relays (event = 0)
		Gladder_FireWaveRelays( 0, pActivator );

		BroadcastWaveUpdate();
		BroadcastTelemetryUpdate();
	}
}

void CGladderRules::OnWaveTriggerFinish( CBaseEntity *pActivator )
{
	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	m_waveManager.CompleteWave( flTime );

	// Acoustic cue for triumphant wave completion
	if ( pActivator && pActivator->edict() )
	{
		EMIT_SOUND( pActivator->edict(), CHAN_ITEM, "buttons/bell1.wav", 1.0f, ATTN_NORM );
	}

	// Fire wave completion relays (event = 1)
	Gladder_FireWaveRelays( 1, pActivator );

	// Garbage collection of old wave entities & diminishing wall station recharge
	PurgeWaveEntities();
	RechargeWallStations();

	// Advance engine skill tier based on wave milestones
	// Wave 1-4: Easy (skill 1), Wave 5-9: Medium (skill 2), Wave 10+: Hard (skill 3)
	int iWave = m_waveManager.GetWaveNumber();
	int newSkill = SKILL_EASY;
	if ( iWave >= 10 )
		newSkill = SKILL_HARD;
	else if ( iWave >= 5 )
		newSkill = SKILL_MEDIUM;

	if ( SkillManager::GetSkillLevel() != newSkill )
	{
		SkillManager::SetSkillLevel( newSkill );
		CGameRules::RefreshSkillData();
	}

	// If match completed due to time limit
	if ( m_waveManager.IsMatchOver() )
	{
		Gladder_FireWaveRelays( 2, pActivator );
	}

	BroadcastWaveUpdate();
	BroadcastTelemetryUpdate();
}

void CGladderRules::PurgeWaveEntities( void )
{
	if ( !gpGlobals )
		return;

	// Garbage collect leftover monsters and dropped items to protect edict budget
	for ( int i = 1; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pEdict = g_engfuncs.pfnPEntityOfEntIndex( i );
		if ( FNullEnt( pEdict ) || pEdict->free )
			continue;

		CBaseEntity *pEnt = CBaseEntity::Instance( pEdict );
		if ( !pEnt )
			continue;

		// 1. Eradicate surviving monsters
		if ( pEnt->MyMonsterPointer() && !pEnt->IsPlayer() )
		{
			UTIL_Remove( pEnt );
			continue;
		}

		// 2. Remove dropped weapon boxes and ammo
		if ( FClassnameIs( pEnt->pev, "weaponbox" ) )
		{
			UTIL_Remove( pEnt );
			continue;
		}

		// 3. Remove world dropped items (weapons/ammo lying on floor not owned by players)
		if ( strncmp( STRING( pEnt->pev->classname ), "weapon_", 7 ) == 0 ||
		     strncmp( STRING( pEnt->pev->classname ), "ammo_", 5 ) == 0 )
		{
			if ( pEnt->pev->owner == nullptr )
			{
				UTIL_Remove( pEnt );
				continue;
			}
		}
	}
}

void CGladderRules::RechargeWallStations( void )
{
	// Re-energize all wall chargers with current wave's diminishing capacity
	// Wall chargers reset their visual state when frame is set to 0.
	CBaseEntity *pCharger = nullptr;
	while ( ( pCharger = UTIL_FindEntityByClassname( pCharger, "func_healthcharger" ) ) != nullptr )
	{
		pCharger->pev->frame = 0;
	}

	pCharger = nullptr;
	while ( ( pCharger = UTIL_FindEntityByClassname( pCharger, "func_recharge" ) ) != nullptr )
	{
		pCharger->pev->frame = 0;
	}
}

void CGladderRules::BroadcastWaveUpdate( CBasePlayer *pPlayer )
{
	if ( !gmsgGladderWave )
		return;

	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	int msgDest = pPlayer ? MSG_ONE : MSG_ALL;
	edict_t *pTarget = pPlayer ? pPlayer->edict() : nullptr;

	MESSAGE_BEGIN( msgDest, gmsgGladderWave, nullptr, pTarget );
	WRITE_BYTE( static_cast<int>( m_waveManager.GetState() ) );
	WRITE_SHORT( m_waveManager.GetWaveNumber() );
	WRITE_COORD( m_waveManager.GetSessionTimeRemaining( flTime ) );
	WRITE_COORD( m_waveManager.GetActiveLapTime( flTime ) );
	MESSAGE_END();
}

void CGladderRules::BroadcastTelemetryUpdate( CBasePlayer *pPlayer )
{
	if ( !gmsgGladderTelemetry )
		return;

	int msgDest = pPlayer ? MSG_ONE : MSG_ALL;
	edict_t *pTarget = pPlayer ? pPlayer->edict() : nullptr;

	MESSAGE_BEGIN( msgDest, gmsgGladderTelemetry, nullptr, pTarget );
	WRITE_COORD( m_waveManager.GetFastestLapTime() );
	WRITE_COORD( m_waveManager.GetSlowestLapTime() );
	WRITE_COORD( m_waveManager.GetAverageLapTime() );
	WRITE_SHORT( m_waveManager.GetCompletedWavesCount() );
	WRITE_SHORT( m_iTotalFrags );
	MESSAGE_END();
}

CGameRules *CreateGladderRules( void )
{
	return new CGladderRules();
}

bool ConditionGladder( void )
{
	// Active when not in deathmatch and gladder cvar is enabled (or deathmatch is 0)
	if ( !gpGlobals || gpGlobals->deathmatch != 0 )
		return false;

	return ( gladder.value != 0.0f );
}

// Priority 100 ensures CGladderRules takes precedence over vanilla singleplayer (priority 30)
REGISTER_GAMERULES( "gladder", CreateGladderRules, ConditionGladder, 100 );
