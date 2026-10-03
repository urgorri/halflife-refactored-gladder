/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Gladder Game Rules Implementation
 *
 ****/

#include "gladder_rules.h"
#include "gladder_entities.h"
#include "gladder_breakable.h"
#include "gladder_usermsg.h"
#include "gladder_audio.h"
#include "core/skill_manager.h"
#include "items/item_base.h"
#include "weapons/weapon_base.h"
#include "shake.h"

// Gladder gamemode cvar (1 enables Gladder mode).
// NOTE: value MUST be initialized to 1.0f here. ConditionGladder() checks gladder.value
// before CVAR_REGISTER() runs (which is what sets value from the string). Without this
// explicit initialization the struct leaves value=0.0f, the condition always returns false,
// and the factory falls back to CHalfLifeRules — CGladderRules is never instantiated.
cvar_t gladder         = { "gladder",         "1",   FCVAR_SERVER | FCVAR_ARCHIVE, 1.0f,   nullptr };
cvar_t gladder_timelimit = { "gladder_timelimit", "600", FCVAR_SERVER | FCVAR_ARCHIVE, 600.0f, nullptr };
cvar_t gladder_autoswitch_on_pickup = { "gladder_autoswitch_on_pickup", "0", FCVAR_SERVER | FCVAR_ARCHIVE, 0.0f, nullptr };

// Early cvar registration: called from ConditionGladder() so the engine has the "gladder"
// cvar available on the console before CGladderRules is ever constructed.
static bool s_bGladderCvarsRegistered = false;
static void Gladder_RegisterCvars( void )
{
	if ( s_bGladderCvarsRegistered )
		return;
	if ( !g_engfuncs.pfnCVarRegister )
		return;
	CVAR_REGISTER( &gladder );
	CVAR_REGISTER( &gladder_timelimit );
	CVAR_REGISTER( &gladder_autoswitch_on_pickup );
	s_bGladderCvarsRegistered = true;
}

CGladderRules::CGladderRules()
    : m_bInitialSpawnDone( false ),
      m_bRestoreAttempted( false ),
      m_bPendingSavePurge( false ),
      m_iTotalFrags( 0 ),
      m_iCollectiblesCount( 0 ),
      m_flTotalDamageTaken( 0.0f ),
      m_flLastPlayerHealth( 100.0f ),
      m_flLastPlayerArmor( 0.0f ),
      m_flLastTelemetryBroadcast( 0.0f )
{
	m_comboTracker.Reset();
	m_modifiers.Reset();

	// Scrub any residual save files on disk to prevent loading stale or unauthorized runs
	Gladder_PurgeSaveFiles();

	// Ensure cvars are registered with the engine (idempotent if already done by ConditionGladder)
	Gladder_RegisterCvars();

	// Register Gladder-specific server console commands
	if ( g_engfuncs.pfnAddServerCommand )
	{
		g_engfuncs.pfnAddServerCommand( (char *)"gladder_reindex_grid", Gladder_ReindexGrid_Cmd );
	}


	float flLimit = gladder_timelimit.value;
	if ( flLimit <= 0.0f )
		flLimit = 600.0f;
	m_waveManager.SetSessionTimeLimit( flLimit );

	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	m_waveManager.InitializeMatch( flTime );

	// Precache essential Gladder cues and spawner roster (monsters, weapons, pickups)
	GladderAudio::Precache();
	m_spawner.Precache();

	CGameRules::RefreshSkillData();

	// Initialize spatial grid cache and map profile if mapname is valid
	if ( gpGlobals && gpGlobals->mapname )
	{
		const char *pszMap = STRING( gpGlobals->mapname );
		if ( pszMap && *pszMap )
		{
			// Try fast-path cache load (SPEC §4.1). If missing on disk, defer generation
			// to Think() once all map entities (trigger_gladder_area) are spawned.
			std::string gridPath = GladderGridIndexer::GetGridFilePath( pszMap );
			if ( m_gridIndexer.LoadFromFile( gridPath.c_str() ) )
			{
				ALERT( at_console, "[Gladder] Loaded %u spatial grid cells from '%s'\n",
				       static_cast<unsigned int>( m_gridIndexer.GetCellCount() ), gridPath.c_str() );
			}
			m_mapConfig.LoadForMap( pszMap );
		}
	}
}

CGladderRules::~CGladderRules()
{
}

void CGladderRules::Think( void )
{
	if ( m_bPendingSavePurge )
	{
		m_bPendingSavePurge = false;
		Gladder_PurgeSaveFiles();
	}

	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	m_waveManager.Tick( flTime );
	m_comboTracker.Update( flTime );

	// Ensure spatial grid cache is loaded or built once all map entities are spawned and active
	if ( !m_gridIndexer.IsLoaded() && gpGlobals && gpGlobals->mapname )
	{
		const char *pszMap = STRING( gpGlobals->mapname );
		if ( pszMap && *pszMap )
		{
			m_gridIndexer.LoadOrCreate( pszMap );
		}
	}

	// Broadcast periodic telemetry (e.g. every 0.25 seconds) to keep client HUD synchronized.
	// Only broadcast once a player is active in the game to avoid sending network messages before client handshake (Issue #34)
	if ( m_bInitialSpawnDone && ( flTime - m_flLastTelemetryBroadcast >= 0.25f ) )
	{
		m_flLastTelemetryBroadcast = flTime;
		BroadcastWaveUpdate();
		BroadcastTelemetryUpdate();
	}

	// Maintain wall charger active state and glow extinction upon depletion (SPEC §3, Issue #46)
	GladderModifiers::UpdateWallStations();
}

void CGladderRules::PlayerSpawn( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	// Strictly reject player spawning if a savegame restore was attempted (SPEC §10.3)
	if ( m_bRestoreAttempted )
	{
		SERVER_COMMAND( "disconnect\n" );
		return;
	}

	// Initial loadout at Point A: guaranteed HEV suit and Crowbar
	if ( !m_bInitialSpawnDone )
	{
		m_bInitialSpawnDone = true;

		// Initialize wall charger glows and capacity (SPEC §3, Issue #46)
		RechargeWallStations();

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
		m_flLastPlayerHealth = pPlayer->pev->health;
		m_flLastPlayerArmor  = pPlayer->pev->armorvalue;
	}

	// Neutralize client save/load commands & quicksave/quickload keybinds via client-side aliases
	if ( pPlayer->edict() )
	{
		Gladder_InstallClientSaveAliases( pPlayer->edict() );
	}

	m_flLastPlayerHealth = pPlayer->pev->health;
	m_flLastPlayerArmor  = pPlayer->pev->armorvalue;

	BroadcastWaveUpdate( pPlayer );
	BroadcastTelemetryUpdate( pPlayer );
}

void CGladderRules::PlayerThink( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->pev )
		return;

	float curHealth = pPlayer->pev->health;
	float curArmor  = pPlayer->pev->armorvalue;

	if ( curHealth < m_flLastPlayerHealth )
	{
		m_flTotalDamageTaken += ( m_flLastPlayerHealth - curHealth );
	}
	if ( curArmor < m_flLastPlayerArmor )
	{
		m_flTotalDamageTaken += ( m_flLastPlayerArmor - curArmor );
	}

	m_flLastPlayerHealth = curHealth;
	m_flLastPlayerArmor  = curArmor;
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

void CGladderRules::OnSaveDenied( void )
{
	ALERT( at_warning, "[Gladder] Save denied: saving is disabled in Half-Life: Gladder (SPEC §10.3).\n" );
	m_bPendingSavePurge = true;
}

void CGladderRules::OnRestoreDenied( void )
{
	ALERT( at_error, "[Gladder] RESTORE DENIED: Loading savegames is strictly forbidden in Half-Life: Gladder (SPEC §10.3).\n" );
	m_bRestoreAttempted = true;
	SERVER_COMMAND( "disconnect\n" );
}

void CGladderRules::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	m_waveManager.EndMatch( flTime, false );
	m_modifiers.RevertMutators();
	GladderAudio::PlayCue( GLADDER_CUE_MATCH_END, pVictim );
	CalculateFinalScore( false );

	// Trigger Defeat Relays (event = 3)
	Gladder_FireWaveRelays( 3, pVictim );

	BroadcastWaveUpdate( pVictim );
	BroadcastTelemetryUpdate( pVictim );
}

void CGladderRules::MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	m_comboTracker.OnMonsterKilled( pVictim, pKiller, pInflictor, flTime );
	m_iTotalFrags = m_comboTracker.GetTotalFrags();
}

float CGladderRules::FlMonsterYawSpeed( CBaseMonster *pMonster, float flDefaultYawSpeed )
{
	return GladderMonsterModifiers::GetModernYawSpeed( pMonster, flDefaultYawSpeed );
}

BOOL CGladderRules::FCanMonsterDropItem( CBaseMonster *pMonster, const char *pszItemName )
{
	// All weapons, ammunition, and medical supplies in Gladder are governed strictly
	// by the procedural spawner; disallow monster item drops (SPEC §3, Issue #44)
	return FALSE;
}

BOOL CGladderRules::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	if ( !pPlayer || !pPlayer->m_pActiveItem )
		return TRUE; // Always switch if player currently has no weapon drawn

	if ( gladder_autoswitch_on_pickup.value == 0.0f )
		return FALSE; // Suppress autoswitch by default (SPEC §7.5, Issue #45)

	return CHalfLifeRules::FShouldSwitchWeapon( pPlayer, pWeapon );
}

float CGladderRules::FlHealthChargerCapacity( void )
{
	return GladderModifiers::CalculateHealthChargerCapacity( m_waveManager.GetWaveNumber(), gSkillData.healthchargerCapacity );
}

float CGladderRules::FlHEVChargerCapacity( void )
{
	return GladderModifiers::CalculateHEVChargerCapacity( m_waveManager.GetWaveNumber(), gSkillData.suitchargerCapacity );
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

		// Ensure spatial grid is loaded
		if ( !m_gridIndexer.IsLoaded() && gpGlobals && gpGlobals->mapname )
		{
			const char *pszMap = STRING( gpGlobals->mapname );
			if ( pszMap && *pszMap )
			{
				m_gridIndexer.LoadOrCreate( pszMap );
				m_mapConfig.LoadForMap( pszMap );
			}
		}

		// Roll and activate wave mutators (Blackout, Low Gravity, Swarm)
		m_modifiers.RollAndApplyWaveMutator( m_waveManager.GetWaveNumber() );

		// Procedurally spawn wave threats, weapons & supplies across indexed spatial grid
		m_spawner.SpawnWave( m_waveManager.GetWaveNumber(), m_gridIndexer, m_mapConfig, m_modifiers.GetSwarmSpecies() );

		// Acoustic cue for wave start & mutator alert
		GladderAudio::PlayCue( GLADDER_CUE_WAVE_START, pActivator );
		if ( m_modifiers.GetActiveMutator() != GLADDER_MUTATOR_NONE )
		{
			GladderAudio::PlayCue( GLADDER_CUE_MUTATOR_WARNING, pActivator );
		}

		// Fire start relays (event = 0)
		Gladder_FireWaveRelays( 0, pActivator );

		BroadcastWaveUpdate();
		BroadcastTelemetryUpdate();
	}
}

void CGladderRules::OnStaticDecal( const Vector &origin, int decalIndex, int entityIndex, int modelIndex )
{
	m_staticDecals.push_back( { origin, decalIndex, entityIndex, modelIndex } );
}

void CGladderRules::RestoreStaticDecals( void )
{
	if ( g_engfuncs.pfnStaticDecal )
	{
		for ( const auto &decal : m_staticDecals )
		{
			g_engfuncs.pfnStaticDecal( decal.origin, decal.decalIndex, decal.entityIndex, decal.modelIndex );
		}
	}
}

void CGladderRules::OnWaveTriggerFinish( CBaseEntity *pActivator )
{
	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	m_waveManager.CompleteWave( flTime );

#ifndef HL_TESTS
	// Smooth screen fade-to-black transition before teleporting back to Point A (SPEC §11.3, Issue #49)
	if ( pActivator && pActivator->IsPlayer() )
	{
		UTIL_ScreenFade( pActivator, Vector( 0, 0, 0 ), 0.5f, 0.5f, 255, FFADE_OUT );
	}
#endif

	// Acoustic cue for triumphant wave completion
	GladderAudio::PlayCue( GLADDER_CUE_WAVE_COMPLETE, pActivator );

	// Fire wave completion relays (event = 1)
	Gladder_FireWaveRelays( 1, pActivator );

	// Revert active mutators back to default
	m_modifiers.RevertMutators();

	// Reset wave environment: GC entities, recharge wall stations, restore breakables
	ResetWave();

	// Advance engine skill tier based on wave milestones
	GladderModifiers::ApplySkillProgression( m_waveManager.GetWaveNumber() );

	// If match completed due to time limit
	if ( m_waveManager.IsMatchOver() )
	{
		CalculateFinalScore( true );
		Gladder_FireWaveRelays( 2, pActivator );
	}

	BroadcastWaveUpdate();
	BroadcastTelemetryUpdate();
}

void CGladderRules::ResetWave( void )
{
	PurgeWaveEntities();
	RechargeWallStations();
	ResetBreakableEntities();
	RestoreStaticDecals();
}

void CGladderRules::PurgeWaveEntities( void )
{
	m_spawner.PurgeWaveEntities();
}

void CGladderRules::ResetBreakableEntities( void )
{
	UTIL_ForEachEntity( []( CBaseEntity *pEnt ) {
		if ( FClassnameIs( pEnt->pev, "gladder_breakable" ) )
		{
			CGladderBreakable *pBreakable = dynamic_cast<CGladderBreakable *>( pEnt );
			if ( pBreakable )
			{
				pBreakable->Reset();
			}
		}
	} );
}

void CGladderRules::RechargeWallStations( void )
{
	GladderModifiers::RechargeWallStations();
}

void CGladderRules::CalculateFinalScore( bool bSurvived )
{
	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	float flElapsed = (std::max)( 0.0f, m_waveManager.GetSessionTimeLimit() - m_waveManager.GetSessionTimeRemaining( flTime ) );
	m_lastScoreBreakdown = GladderScoring::CalculateScore(
		m_comboTracker.GetStats(),
		m_waveManager.GetCompletedWavesCount(),
		m_iCollectiblesCount,
		flElapsed,
		m_waveManager.GetAverageLapTime(),
		m_waveManager.GetFastestLapTime(),
		m_waveManager.GetSlowestLapTime(),
		m_flTotalDamageTaken,
		bSurvived
	);

	if ( bSurvived )
	{
		GladderAudio::PlayCue( GLADDER_CUE_MATCH_END );
	}

	ALERT( at_console, "[Gladder] === MATCH SUMMARY ===\n" );
	ALERT( at_console, "[Gladder] Status: %s\n", bSurvived ? "SURVIVED - TIME EXPIRED" : "KIA - FALLEN IN COMBAT" );
	ALERT( at_console, "[Gladder] Final Grade: %c (%s)\n", m_lastScoreBreakdown.rankGrade, m_lastScoreBreakdown.rankTitle.c_str() );
	ALERT( at_console, "[Gladder] Composite Score: %d (Combat: %d, Combos: %d, Waves: %d, Lambdas: %d, Survival: %d, DmgPenalty: -%d)\n",
	       m_lastScoreBreakdown.finalCompositeScore,
	       m_lastScoreBreakdown.baseCombatPoints,
	       m_lastScoreBreakdown.comboBonusPoints,
	       m_lastScoreBreakdown.waveBonusPoints,
	       m_lastScoreBreakdown.lambdaBonusPoints,
	       m_lastScoreBreakdown.survivalBonusPoints,
	       m_lastScoreBreakdown.damagePenaltyPoints );
	ALERT( at_console, "[Gladder] Waves Cleared: %d, Frags: %d, Max Combo: x%d, Lambdas: %d, Dmg Taken: %.1f\n",
	       m_lastScoreBreakdown.wavesCompleted,
	       m_lastScoreBreakdown.totalFrags,
	       m_lastScoreBreakdown.maxComboStreak,
	       m_lastScoreBreakdown.collectiblesCount,
	       m_lastScoreBreakdown.totalDamageTaken );
	ALERT( at_console, "[Gladder] ======================\n" );
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
	if ( !gpGlobals || gpGlobals->deathmatch != 0 )
		return false;

	// Register Gladder CVARs as early as possible so the engine knows about them
	// even before CGladderRules is instantiated. Safe to call after GiveFnptrsToDll.
	Gladder_RegisterCvars();

	// Prefer the engine-tracked cvar value (updated from cfg overrides) when available;
	// fall back to the statically initialized 1.0f before the first engine map load.
	cvar_t *pRegistered = g_engfuncs.pfnCVarGetPointer ? CVAR_GET_POINTER( "gladder" ) : nullptr;
	if ( pRegistered )
		return ( pRegistered->value != 0.0f );

	return ( gladder.value != 0.0f );
}

// Priority 100 ensures CGladderRules takes precedence over vanilla singleplayer (priority 30)
REGISTER_GAMERULES( "gladder", CreateGladderRules, ConditionGladder, 100 );
