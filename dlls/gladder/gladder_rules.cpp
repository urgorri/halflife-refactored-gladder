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
#include "gladder_leaderboard.h"
#include "gladder_grid_debug.h"
#include "gladder_palette.h"
#include "squidspit_cloud.h"
#include <ctime>
#include "core/skill_manager.h"
#include "ai/squadmonster.h"
#include "items/item_base.h"
#include "weapons/weapon_base.h"
#include "shake.h"
#include "core/util_math.h"

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
      m_bGridIndexAttempted( false ),
      m_iTotalFrags( 0 ),
      m_iCollectiblesCount( 0 ),
      m_flTotalDamageTaken( 0.0f ),
      m_flLastPlayerHealth( 100.0f ),
      m_flLastPlayerArmor( 0.0f ),
      m_flLastTelemetryBroadcast( 0.0f ),
      m_iCurrentSummaryCameraIndex( 0 ),
      m_flNextCameraSwitchTime( 0.0f ),
      m_vecDeathAngles( 0, 0, 0 ),
      m_bPlayerDead( false ),
      m_bPlayerFrozen( false ),
      m_iFrozenPlayerSequence( 0 ),
      m_flFrozenPlayerFrame( 0.0f ),
      m_iFrozenPlayerGaitSequence( 0 ),
      m_bMatchConcluded( false ),
      m_bMatchFrozen( false ),
      m_flMatchFreezeTime( 0.0f ),
      m_iWaveMonstersTotal( 0 ),
      m_iWaveMonstersKilled( 0 ),
      m_bHostilesEliminatedFired( false )
{
	g_fGameOver = FALSE;
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

	RefreshSkillData();
	EnsureDeadMonsterProps();

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
				m_bGridIndexAttempted = true;
				ALERT( at_console, "[Gladder] Loaded %u spatial grid cells from '%s'\n",
				       static_cast<unsigned int>( m_gridIndexer.GetCellCount() ), gridPath.c_str() );
			}
		}
	}
}

CGladderRules::~CGladderRules()
{
	GladderGridDebug_ServerReset();
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

	// Handle match conclusion when session time limit expires (SPEC §2, §3, §9.2, Issue #85, #88)
	if ( !m_bMatchConcluded && m_waveManager.IsMatchOver() )
	{
		HandleMatchConclusion( true );
	}
	else if ( m_bMatchConcluded )
	{
		// Do NOT freeze world entities or overwrite player animation sequence/frame (Issue #88).
		// The world continues running naturally while player inputs (except Tab/Escape) are suppressed.
		if ( !m_bMatchFrozen && flTime >= m_flMatchFreezeTime )
		{
			m_bMatchFrozen = true;
		}
	}

	// Ensure spatial grid cache is loaded or built once all map entities are spawned and active.
	// Only attempt once per map to prevent per-tick console spam if map yields 0 cells (Issue #63).
	if ( !m_bGridIndexAttempted && gpGlobals && gpGlobals->mapname )
	{
		const char *pszMap = STRING( gpGlobals->mapname );
		if ( pszMap && *pszMap )
		{
			m_bGridIndexAttempted = true;
			m_gridIndexer.LoadOrCreate( pszMap );
		}
	}

	// Broadcast periodic telemetry (e.g. every 0.25 seconds) to keep client HUD synchronized.
	// Only broadcast once a player is active in the game to avoid sending network messages before client handshake (Issue #34)
	if ( m_bInitialSpawnDone && ( flTime - m_flLastTelemetryBroadcast >= 0.25f ) )
	{
		m_flLastTelemetryBroadcast = flTime;

		// Reconcile living hostiles with counter in case any monster died without triggering MonsterKilled
		// (e.g. infighting between monsters, crushers, acid, or falling into void)
		if ( m_waveManager.IsWaveActive() && m_iWaveMonstersTotal > 0 )
		{
			int iLiving = CountLivingWaveHostiles();
			int iDerivedKilled = m_iWaveMonstersTotal - iLiving;
			if ( iDerivedKilled > m_iWaveMonstersKilled )
			{
				m_iWaveMonstersKilled = iDerivedKilled;
			}
			if ( iLiving == 0 )
			{
				CheckAllHostilesEliminated();
			}
		}

		BroadcastWaveUpdate();
		BroadcastTelemetryUpdate();
		BroadcastComboUpdate();
	}

	// Maintain wall charger active state and glow extinction upon depletion (SPEC §3, Issue #46)
	GladderModifiers::UpdateWallStations();

	// Real-time spatial grid debug overlay streaming (SPEC §4.5, Issue #105)
	if ( GladderGridDebug_IsServerActive() )
	{
		CBasePlayer *pDbgPlayer = nullptr;
		if ( gpGlobals )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CBaseEntity *pEnt = UTIL_PlayerByIndex( i );
				if ( pEnt && pEnt->IsPlayer() )
				{
					pDbgPlayer = dynamic_cast<CBasePlayer *>( pEnt );
					if ( pDbgPlayer )
						break;
				}
			}
		}
		if ( pDbgPlayer )
		{
			GladderGridDebug_ServerThink( m_gridIndexer, pDbgPlayer );
		}
	}

	// Multi-camera backdrop rotation during summary screen (SPEC §6, §9.2, Issue #52)
	if ( m_waveManager.IsMatchOver() && m_summaryCameras.size() > 1 && flTime >= m_flNextCameraSwitchTime )
	{
		AdvanceSummaryCamera();
	}

	// Lock dead player corpse angles to prevent rotating with mouse view in third person (SPEC §10.3, Issue #53)
	if ( m_bPlayerDead && gpGlobals )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBaseEntity *pPlr = UTIL_PlayerByIndex( i );
			if ( pPlr && pPlr->edict() && !pPlr->IsAlive() )
			{
				pPlr->pev->angles = m_vecDeathAngles;
			}
		}
	}
}

void CGladderRules::PlayerSpawn( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	m_bPlayerDead = false;
	m_bMatchConcluded = false;
	m_bMatchFrozen = false;
	m_flMatchFreezeTime = 0.0f;
	m_bPlayerFrozen = false;
	m_iFrozenPlayerSequence = 0;
	m_flFrozenPlayerFrame = 0.0f;
	m_iFrozenPlayerGaitSequence = 0;
	g_fGameOver = FALSE;
	m_vecDeathAngles = g_vecZero;
	m_summaryCameras.clear();
	m_iCurrentSummaryCameraIndex = 0;
	m_flNextCameraSwitchTime = 0.0f;
	if ( pPlayer->pev )
	{
		pPlayer->pev->flags &= ~FL_FROZEN;
		pPlayer->pev->flags &= ~FL_NOTARGET;
		pPlayer->pev->flags &= ~FL_MONSTERCLIP;
		pPlayer->pev->maxspeed = 0.0f;
		pPlayer->pev->fuser4 = 0.0f;
	}

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

	if ( m_bMatchConcluded )
	{
		// Suppress all player gameplay inputs except scoreboard/summary toggle (IN_SCORE / Tab)
		// and engine pause menu (Escape). Do NOT set FL_FROZEN or g_fGameOver so that player
		// death animations and world simulation continue naturally without resetting to idle.
		pPlayer->pev->button &= IN_SCORE;
		pPlayer->m_afButtonPressed &= IN_SCORE;
		pPlayer->m_afButtonReleased &= IN_SCORE;
		pPlayer->pev->impulse = 0;
		pPlayer->pev->maxspeed = 1.0f;
		pPlayer->m_flNextAttack = gpGlobals ? ( gpGlobals->time + 9999.0f ) : 9999.0f;

		if ( !m_bPlayerDead )
		{
			pPlayer->pev->velocity.x = 0.0f;
			pPlayer->pev->velocity.y = 0.0f;
			pPlayer->pev->avelocity = g_vecZero;
		}
	}
	else
	{
		// Recover from movement slow debuffs (SPEC §3, Issue #116)
		float flCurrentTime = gpGlobals ? gpGlobals->time : 0.0f;
		if ( pPlayer->pev->fuser4 > 0.0f && flCurrentTime >= pPlayer->pev->fuser4 )
		{
			pPlayer->pev->maxspeed = 0.0f;
			pPlayer->pev->fuser4 = 0.0f;
		}
	}

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

	// Full map reload preserving brush entities (SPEC §10.4, Issue #82)
	if ( FStrEq( pcmd, "gladder_restart" ) )
	{
		const char *pszMap = ( gpGlobals && gpGlobals->mapname ) ? STRING( gpGlobals->mapname ) : nullptr;
		if ( pszMap && *pszMap )
		{
			char szCmd[128];
			snprintf( szCmd, sizeof( szCmd ), "map %s\n", pszMap );
			SERVER_COMMAND( szCmd );
		}
		else
		{
			SERVER_COMMAND( "restart\n" );
		}
		return TRUE;
	}

	// Real-time spatial grid debug overlay toggle (SPEC §4.5, Issue #105)
	if ( strncmp( pcmd, "gladder_grid_debug", 18 ) == 0 )
	{
		GladderGridDebug_HandleClientCommand( pPlayer, pcmd );
		return TRUE;
	}

	// Camera test commands for level designers and verification (Issue #16, #52)
	if ( FStrEq( pcmd, "gladder_test_camera" ) )
	{
		if ( m_summaryCameras.empty() )
		{
			CollectSummaryCameras();
		}

		if ( !m_summaryCameras.empty() )
		{
			if ( m_summaryCameras.size() > 1 )
			{
				AdvanceSummaryCamera();
			}
			else
			{
				ShiftPlayerCamera( pPlayer );
			}
			CBaseEntity *pCam = m_summaryCameras[m_iCurrentSummaryCameraIndex];
			if ( pCam )
			{
				const char *pszName = pCam->pev->targetname ? STRING( pCam->pev->targetname ) : STRING( pCam->pev->classname );
				ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE,
					UTIL_VarArgs( "[Gladder] Switched view to camera %u/%u ('%s') at (%.0f, %.0f, %.0f), angles (%.1f, %.1f, %.1f).\n",
						static_cast<unsigned int>( m_iCurrentSummaryCameraIndex + 1 ),
						static_cast<unsigned int>( m_summaryCameras.size() ),
						pszName ? pszName : "unnamed",
						pCam->pev->origin.x, pCam->pev->origin.y, pCam->pev->origin.z,
						pCam->pev->angles.x, pCam->pev->angles.y, pCam->pev->angles.z ) );
			}
		}
		else
		{
			ShiftPlayerCamera( pPlayer );
			ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "[Gladder] Switched view to dynamic fallback camera.\n" );
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "gladder_reset_camera" ) )
	{
		SET_VIEW( pPlayer->edict(), pPlayer->edict() );
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "[Gladder] View reset to first-person.\n" );
		return TRUE;
	}

	// Once match has concluded, suppress any other client commands (e.g. drop, weapon switches, kill)
	if ( m_bMatchConcluded )
	{
		return TRUE;
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
	m_bPlayerDead = true;
	HandleMatchConclusion( false, pVictim );
}

void CGladderRules::HandleMatchConclusion( bool bSurvived, CBasePlayer *pPlayer )
{
	if ( m_bMatchConcluded )
		return;

	m_bMatchConcluded = true;
	float flTime = gpGlobals ? gpGlobals->time : 0.0f;

	if ( !pPlayer && gpGlobals )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBaseEntity *pPlr = UTIL_PlayerByIndex( i );
			if ( pPlr && pPlr->edict() )
			{
				pPlayer = dynamic_cast<CBasePlayer *>( pPlr );
				if ( pPlayer )
					break;
			}
		}
	}

	if ( pPlayer && pPlayer->pev )
	{
		// Terminate any active looping weapon audio (SPEC §7.7, Issue #66)
		EMIT_SOUND( ENT( pPlayer->pev ), CHAN_WEAPON, "common/null.wav", 1.0f, ATTN_NORM );

		// Make player untargetable by monsters and block all non-Tab inputs without setting FL_FROZEN!
		// Setting FL_FROZEN (via EnableControl(FALSE)) causes CBasePlayer::SetAnimation(PLAYER_DIE)
		// to force playerAnim = PLAYER_IDLE, which broke the death pose and showed idle instead.
		pPlayer->pev->flags |= FL_NOTARGET;
		pPlayer->pev->button &= IN_SCORE;
		pPlayer->pev->impulse = 0;
		pPlayer->pev->maxspeed = 1.0f;
		pPlayer->m_flNextAttack = flTime + 9999.0f;

		if ( m_bPlayerDead )
		{
			m_vecDeathAngles = pPlayer->pev->angles;
		}
		else
		{
			pPlayer->pev->velocity = g_vecZero;
			pPlayer->pev->avelocity = g_vecZero;
		}
	}

	m_flMatchFreezeTime = flTime + 1.0f;
	m_bMatchFrozen = false;

	if ( !m_waveManager.IsMatchOver() )
	{
		m_waveManager.EndMatch( flTime, bSurvived );
	}

	m_modifiers.RevertMutators();
	GladderAudio::PlayCue( GLADDER_CUE_MATCH_END, pPlayer );
	CalculateFinalScore( bSurvived );

	// Trigger Defeat (3) or Victory (2) relays
	Gladder_FireWaveRelays( bSurvived ? 2 : 3, pPlayer );

	BroadcastWaveUpdate( pPlayer );
	BroadcastTelemetryUpdate( pPlayer );
}

void CGladderRules::FreezeMatchWorld( void )
{
	if ( !gpGlobals )
		return;

	int maxEdicts = ( gpGlobals->maxEntities > 0 ) ? gpGlobals->maxEntities : 512;
	for ( int i = 1; i < maxEdicts; ++i )
	{
		edict_t *pEdict = INDEXENT( i );
		if ( FNullEnt( pEdict ) || pEdict->free || !pEdict->v.pContainingEntity )
			continue;

		CBaseEntity *pEnt = CBaseEntity::Instance( pEdict );
		if ( !pEnt || pEnt->IsPlayer() )
			continue;

		// 1. Freeze monsters in place: halt velocity, physics, animation, and schedules
		CBaseMonster *pMonster = pEnt->MyMonsterPointer();
		if ( pMonster )
		{
			pMonster->pev->velocity = g_vecZero;
			pMonster->pev->avelocity = g_vecZero;
			pMonster->pev->framerate = 0.0f;
			pMonster->pev->movetype = MOVETYPE_NONE;
			pMonster->pev->effects |= EF_NOINTERP;
			pMonster->pev->takedamage = DAMAGE_NO;
			pMonster->pev->flags |= FL_FROZEN;
			pMonster->pev->nextthink = 0.0f;
			pMonster->m_pSchedule = nullptr;
			pMonster->m_iTaskStatus = TASKSTATUS_NEW;
			pMonster->m_iScheduleIndex = 0;
			// Note: Do NOT reset m_Activity or sequence so monster remains in exact frozen pose!
			EMIT_SOUND( pMonster->edict(), CHAN_VOICE, "common/null.wav", 1.0f, ATTN_NORM );
			EMIT_SOUND( pMonster->edict(), CHAN_BODY, "common/null.wav", 1.0f, ATTN_NORM );
			continue;
		}

		// 2. Freeze in-flight combat projectiles, deployables, and thrown explosives
		const char *pszClassname = STRING( pEnt->pev->classname );
		if ( pszClassname && (
		     strncmp( pszClassname, "projectile_", 11 ) == 0 ||
		     strncmp( pszClassname, "monster_satchel", 15 ) == 0 ||
		     strncmp( pszClassname, "monster_tripmine", 16 ) == 0 ||
		     strncmp( pszClassname, "monster_snark", 13 ) == 0 ||
		     strcmp( pszClassname, "grenade" ) == 0 ||
		     strcmp( pszClassname, "rpg_rocket" ) == 0 ||
		     strcmp( pszClassname, "bolt" ) == 0 ||
		     strcmp( pszClassname, "hornet" ) == 0 ) )
		{
			pEnt->pev->velocity = g_vecZero;
			pEnt->pev->avelocity = g_vecZero;
			pEnt->pev->framerate = 0.0f;
			pEnt->pev->movetype = MOVETYPE_NONE;
			pEnt->pev->effects |= EF_NOINTERP;
			pEnt->pev->takedamage = DAMAGE_NO;
			pEnt->pev->nextthink = 0.0f;
		}
	}
}

void CGladderRules::MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	int iFragsBefore = m_comboTracker.GetTotalFrags();
	m_comboTracker.OnMonsterKilled( pVictim, pKiller, pInflictor, flTime );
	m_iTotalFrags = m_comboTracker.GetTotalFrags();
	if ( m_iTotalFrags != iFragsBefore )
	{
		BroadcastComboUpdate();
	}

	// Wave hostile tracking & finish gate enforcement (SPEC §2, §9.1, Issue #118)
	if ( GladderComboTracker::IsValidCombatant( pVictim ) && m_waveManager.IsWaveActive() )
	{
		m_iWaveMonstersKilled++;
		BroadcastWaveUpdate();
		CheckAllHostilesEliminated( pKiller ? CBaseEntity::Instance( pKiller ) : nullptr );
	}
}

int CGladderRules::CountLivingWaveHostiles( void ) const
{
	if ( !gpGlobals )
		return 0;

	int iLivingCount = 0;
	for ( int i = 1; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pEdict = INDEXENT( i );
		if ( !pEdict || pEdict->free || ( pEdict->v.flags & FL_KILLME ) )
			continue;

		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
		if ( !pEntity || !pEntity->pev || pEntity->IsPlayer() )
			continue;

		CBaseMonster *pMonster = pEntity->MyMonsterPointer();
		if ( !pMonster )
			continue;

		if ( !GladderComboTracker::IsValidCombatant( pMonster ) )
			continue;

		// Monster must be alive, actively take damage, and not currently dying
		if ( pMonster->IsAlive() && pMonster->pev->deadflag == DEAD_NO && pMonster->pev->takedamage != DAMAGE_NO )
		{
			iLivingCount++;
		}
	}

	return iLivingCount;
}

bool CGladderRules::AreWaveHostilesEliminated( void ) const
{
	if ( m_iWaveMonstersTotal <= 0 )
		return true;

	if ( m_iWaveMonstersKilled >= m_iWaveMonstersTotal )
		return true;

	// Infallible fallback: if no living combatant monsters exist anywhere in the world,
	// wave hostiles are guaranteed to be eliminated (even if monsters killed each other or died to hazards)
	return ( CountLivingWaveHostiles() == 0 );
}

void CGladderRules::CheckAllHostilesEliminated( CBaseEntity *pActivator )
{
	if ( m_iWaveMonstersTotal > 0 && AreWaveHostilesEliminated() && !m_bHostilesEliminatedFired )
	{
		m_bHostilesEliminatedFired = true;
		Gladder_FireClearTargets( pActivator );
		Gladder_FireWaveRelays( 4, pActivator );
	}
}

void CGladderRules::RefreshSkillData( void )
{
	CHalfLifeRules::RefreshSkillData();
	GladderMonsterModifiers::ApplySkillDataOverrides( g_iSkillLevel );
}

void CGladderRules::OnEntitySpawned( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->pev )
		return;

	// 1. Pre-placed dead monster environmental props (SPEC §3, §11.1, Issue #123)
	if ( GladderMonsterModifiers::IsDeadMonsterProp( pEntity ) )
	{
		GladderMonsterModifiers::ConfigureDeadMonsterProp( pEntity );
		return;
	}

	// 2. Bullsquid spit projectile non-damaging green smoke trail (SPEC §3, Issue #121)
	if ( FClassnameIs( pEntity->pev, "squidspit" ) )
	{
		if ( g_engfuncs.pfnMessageBegin && pEntity->edict() )
		{
			int iSmokeSprite = g_engfuncs.pfnModelIndex ? MODEL_INDEX( "sprites/smoke.spr" ) : 0;
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMFOLLOW );
			WRITE_SHORT( pEntity->entindex() );
			WRITE_SHORT( iSmokeSprite );
			WRITE_BYTE( 3 ); // life (~0.3s in 0.1s units)
			WRITE_BYTE( 10 ); // width
			WRITE_BYTE( GladderPalette::Health.r );
			WRITE_BYTE( GladderPalette::Health.g );
			WRITE_BYTE( GladderPalette::Health.b );
			WRITE_BYTE( 200 ); // brightness
			MESSAGE_END();
		}
	}
}

float CGladderRules::FlMonsterYawSpeed( CBaseMonster *pMonster, float flDefaultYawSpeed )
{
	return GladderMonsterModifiers::GetModernYawSpeed( pMonster, flDefaultYawSpeed );
}

float CGladderRules::FlMonsterCastSpeed( CBaseMonster *pMonster, Activity activity, float flDefaultSpeed )
{
	if ( !pMonster || !pMonster->pev )
		return flDefaultSpeed;

	// Heavy monster pain flinch recovery resistance (SPEC §3, Issue #116, #121)
	if ( activity == ACT_SMALL_FLINCH || activity == ACT_BIG_FLINCH )
	{
		if ( GladderMonsterModifiers::IsHeavyFlinchResistantMonster( pMonster ) )
		{
			return flDefaultSpeed * GladderMonsterModifiers::GetHeavyMonsterFlinchResistance( g_iSkillLevel );
		}
		return flDefaultSpeed;
	}

	float flSpeed = flDefaultSpeed;
	if ( FClassnameIs( pMonster->pev, "monster_houndeye" ) && activity == ACT_RANGE_ATTACK1 )
	{
		flSpeed = GladderMonsterModifiers::GetHoundeyeCastSpeed( g_iSkillLevel );
	}
	else if ( ( FClassnameIs( pMonster->pev, "monster_alien_slave" ) ||
	            FClassnameIs( pMonster->pev, "monster_vortigaunt" ) ) &&
	          activity == ACT_RANGE_ATTACK1 )
	{
		flSpeed = GladderMonsterModifiers::GetVortigauntCastSpeed( g_iSkillLevel );
	}

	if ( GladderModifiers::IsEliteChampion( pMonster ) )
	{
		flSpeed *= GladderModifiers::CHAMPION_FRAMERATE_SCALE;
	}

	return flSpeed;
}

float CGladderRules::FlMonsterMoveSpeed( CBaseMonster *pMonster, Activity activity, float flDefaultSpeed )
{
	if ( !pMonster || !pMonster->pev )
		return flDefaultSpeed;

	float flSpeed = flDefaultSpeed;
	if ( FClassnameIs( pMonster->pev, "monster_headcrab" ) || FClassnameIs( pMonster->pev, "monster_babycrab" ) )
	{
		if ( activity == ACT_RUN || activity == ACT_WALK )
		{
			flSpeed = GladderMonsterModifiers::GetHeadcrabMoveSpeed( g_iSkillLevel );
		}
		else if ( activity == ACT_RANGE_ATTACK1 )
		{
			flSpeed = GladderMonsterModifiers::GetHeadcrabJumpSpeed( g_iSkillLevel );
		}
	}
	else if ( FClassnameIs( pMonster->pev, "monster_zombie" ) ||
	          FClassnameIs( pMonster->pev, "monster_zombie_barney" ) ||
	          FClassnameIs( pMonster->pev, "monster_zombie_soldier" ) )
	{
		if ( activity == ACT_WALK || activity == ACT_RUN )
		{
			flSpeed = GladderMonsterModifiers::GetZombieMoveSpeed( g_iSkillLevel );
		}
	}
	else if ( FClassnameIs( pMonster->pev, "monster_alien_controller" ) )
	{
		flSpeed = flDefaultSpeed * GladderMonsterModifiers::GetControllerMoveSpeed( g_iSkillLevel );
	}

	if ( GladderModifiers::IsEliteChampion( pMonster ) )
	{
		flSpeed *= GladderModifiers::CHAMPION_FRAMERATE_SCALE;
	}

	return flSpeed;
}

float CGladderRules::FlMonsterProjectileSpread( CBaseMonster *pMonster, const char *pszProjectile, float flDefaultSpread )
{
	(void)pMonster;
	if ( pszProjectile && FStrEq( pszProjectile, "squidspit" ) )
	{
		return GladderMonsterModifiers::GetBullsquidSpitSpread( g_iSkillLevel );
	}
	return flDefaultSpread;
}

float CGladderRules::FlHornetTrackAgility( CBaseEntity *pHornet, float flDefaultWeight )
{
	(void)pHornet;
	return GladderMonsterModifiers::GetHornetTrackAgility( g_iSkillLevel, flDefaultWeight );
}

float CGladderRules::FlMonsterWaitTime( CBaseMonster *pMonster, float flDefaultWait )
{
	if ( pMonster && pMonster->pev &&
	     ( FClassnameIs( pMonster->pev, "monster_human_grunt" ) ||
	       FClassnameIs( pMonster->pev, "monster_male_assassin" ) ) )
	{
		return GladderMonsterModifiers::GetHGruntWaitTime( g_iSkillLevel, flDefaultWait );
	}
	return flDefaultWait;
}

float CGladderRules::FlMonsterGrenadeCheckDelay( CBaseMonster *pMonster, float flDefaultDelay )
{
	if ( pMonster && pMonster->pev &&
	     ( FClassnameIs( pMonster->pev, "monster_human_grunt" ) ||
	       FClassnameIs( pMonster->pev, "monster_male_assassin" ) ) )
	{
		return GladderMonsterModifiers::GetHGruntGrenadeCheckDelay( g_iSkillLevel, flDefaultDelay );
	}
	return flDefaultDelay;
}

int CGladderRules::IMonsterProjectileBurstCount( CBaseMonster *pMonster, const char *pszProjectile, int iDefaultCount )
{
	(void)pszProjectile;
	if ( pMonster && pMonster->pev && FClassnameIs( pMonster->pev, "monster_alien_controller" ) )
	{
		return GladderMonsterModifiers::GetControllerBurstCount( g_iSkillLevel, iDefaultCount );
	}
	return iDefaultCount;
}

void CGladderRules::OnMonsterAttackDetonation( CBaseMonster *pMonster, const char *pszAttackType, const Vector &vecOrigin )
{
	if ( !pszAttackType || !FStrEq( pszAttackType, "sonic_blast" ) )
		return;

	BYTE bRed = 188, bGreen = 220, bBlue = 255;
	CSquadMonster *pSquad = pMonster ? pMonster->MySquadMonsterPointer() : nullptr;
	if ( pSquad && pSquad->InSquad() )
	{
		switch ( pSquad->SquadCount() )
		{
			case 2:
				bRed   = 101;
				bGreen = 133;
				bBlue  = 221;
				break;
			case 3:
				bRed   = 67;
				bGreen = 85;
				bBlue  = 255;
				break;
			case 4:
				bRed   = 62;
				bGreen = 33;
				bBlue  = 211;
				break;
			default:
				break;
		}
	}

	if ( g_engfuncs.pfnMessageBegin )
	{
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_BYTE( 20 ); // radius (20 * 10 = 200u)
		WRITE_BYTE( bRed );
		WRITE_BYTE( bGreen );
		WRITE_BYTE( bBlue );
		WRITE_BYTE( 5 );  // life (0.5s in 0.1s units)
		WRITE_BYTE( 40 ); // decay rate (fast decay)
		MESSAGE_END();
	}
}

void CGladderRules::OnMonsterProjectileImpact( CBaseEntity *pProjectile, const TraceResult &tr )
{
	if ( !pProjectile || !pProjectile->pev )
		return;

	if ( FClassnameIs( pProjectile->pev, "squidspit" ) )
	{
		// Spawn lingering toxic cloud on surface/geometry impact (SPEC §3, Issue #116, #121)
		if ( !tr.pHit || tr.pHit->v.takedamage == DAMAGE_NO )
		{
			Vector vecPos = ( tr.vecEndPos != g_vecZero ) ? tr.vecEndPos : pProjectile->pev->origin;
			float flDuration = GladderMonsterModifiers::GetToxicCloudDuration( g_iSkillLevel );
			if ( flDuration > 0.0f )
			{
				CSquidSpitCloud::Create( vecPos, flDuration );
			}
		}
	}
}

int CGladderRules::FlMonsterScheduleInterruptMask( CBaseMonster *pMonster, Schedule_t *pSchedule, int iDefaultMask )
{
	return GladderMonsterModifiers::GetScheduleInterruptMask( pMonster, pSchedule, iDefaultMask );
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

int CGladderRules::GetMaxAmmo( const char *pszAmmoName, int iDefaultMaxCarry )
{
	// Ammunition and explosive carry limits decoupled and extended towards infinity (SPEC §3, §7.14, Issue #97)
	if ( GladderIsExtendedAmmo( pszAmmoName ) )
	{
		return GLADDER_EXTENDED_AMMO_MAX;
	}

	return CHalfLifeRules::GetMaxAmmo( pszAmmoName, iDefaultMaxCarry );
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
			}
		}

		// Roll and activate wave mutators (Blackout, Low Gravity, Swarm)
		m_modifiers.RollAndApplyWaveMutator( m_waveManager.GetWaveNumber() );

		// Procedurally spawn wave threats, weapons & supplies across indexed spatial grid
		CBasePlayer *pPlayer = ( pActivator && pActivator->IsPlayer() ) ? dynamic_cast<CBasePlayer *>( pActivator ) : nullptr;
		m_spawner.SpawnWave( m_waveManager.GetWaveNumber(), m_gridIndexer, m_mapConfig, m_modifiers.GetSwarmSpecies(), pPlayer, m_modifiers.GetActiveMutator() );

		// Hostile elimination tracking (SPEC §2, §9.1, Issue #118)
		m_iWaveMonstersTotal = m_spawner.GetLastSpawnedMonsterCount();
		m_iWaveMonstersKilled = 0;
		m_bHostilesEliminatedFired = false;

		// Transition start triggers to inactive Gray and finish triggers to locked Gray (SPEC §2, §6, §7.2, Issue #122)
		UTIL_ForEachEntity( []( CBaseEntity *pEnt ) {
			if ( !pEnt || !pEnt->pev )
				return;

			if ( FClassnameIs( pEnt->pev, "trigger_gladder_start" ) )
			{
				CTriggerGladderStart *pStart = dynamic_cast<CTriggerGladderStart *>( pEnt );
				if ( pStart )
				{
					pStart->SetActiveState( false );
				}
				else
				{
					pEnt->pev->rendercolor = Vector( GladderPalette::TriggersInactive.r, GladderPalette::TriggersInactive.g, GladderPalette::TriggersInactive.b );
				}
			}
			else if ( FClassnameIs( pEnt->pev, "trigger_gladder_finish" ) )
			{
				CTriggerGladderFinish *pFinish = dynamic_cast<CTriggerGladderFinish *>( pEnt );
				if ( pFinish )
				{
					pFinish->SetUnlockedState( false );
				}
				else
				{
					pEnt->pev->rendercolor = Vector( GladderPalette::TriggersInactive.r, GladderPalette::TriggersInactive.g, GladderPalette::TriggersInactive.b );
				}
			}
		} );

		// Acoustic cue for wave start & mutator alert
		GladderAudio::PlayCue( GLADDER_CUE_WAVE_START, pActivator );
		if ( m_modifiers.GetActiveMutator() != GLADDER_MUTATOR_NONE )
		{
			GladderAudio::PlayCue( GLADDER_CUE_MUTATOR_WARNING, pActivator );
		}

		// Fire start relays (event = 0)
		Gladder_FireWaveRelays( 0, pActivator );

		// Enforce FL_MONSTERCLIP across monsters and pushables for start/finish trigger clip boundaries
		EnsureMonsterClipFlags();
		EnsureDeadMonsterProps();

		BroadcastWaveNotify( nullptr, 0, m_waveManager.GetWaveNumber(), static_cast<int>( m_modifiers.GetActiveMutator() ), m_iWaveMonstersTotal );
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
	int iCompletedWave = m_waveManager.GetWaveNumber();
	m_waveManager.CompleteWave( flTime );

#ifndef HL_TESTS
	// Instant black screen before Point A teleportation with smooth fade-in reveal (SPEC §2, §6, Issue #57)
	if ( pActivator && pActivator->IsPlayer() )
	{
		UTIL_ScreenFade( pActivator, Vector( 0, 0, 0 ), 0.8f, 0.2f, 255, FFADE_IN );
	}

	// Explicitly broadcast decal purge client command to all connected players during transition (SPEC §11.3, Issue #49)
	if ( gpGlobals )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBaseEntity *pEnt = UTIL_PlayerByIndex( i );
			if ( pEnt && pEnt->IsPlayer() && pEnt->edict() )
			{
				CLIENT_COMMAND( pEnt->edict(), "r_cleardecals\n" );
			}
		}
	}
#endif

	// Terminate active mutator alert warning siren on wave completion (SPEC §3, §8, Issue #70)
	GladderAudio::StopCue( GLADDER_CUE_MUTATOR_WARNING, pActivator );

	// Acoustic cue for triumphant wave completion
	GladderAudio::PlayCue( GLADDER_CUE_WAVE_COMPLETE, pActivator );

	// Fire wave completion relays (event = 1)
	Gladder_FireWaveRelays( 1, pActivator );

	// Transmit the exact completed wave number, not the incremented incoming wave (SPEC §12.2, Issue #56)
	BroadcastWaveNotify( nullptr, 1, iCompletedWave, 0 );

	// Revert active mutators back to default
	m_modifiers.RevertMutators();

	// Reset wave environment: GC entities, recharge wall stations, restore breakables
	ResetWave();

	// Advance engine skill tier based on wave milestones
	GladderModifiers::ApplySkillProgression( m_waveManager.GetWaveNumber() );

	// If match completed due to time limit or final wave
	if ( m_waveManager.IsMatchOver() && !m_bMatchConcluded )
	{
		CBasePlayer *pPlayer = dynamic_cast<CBasePlayer *>( pActivator );
		HandleMatchConclusion( true, pPlayer );
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
	ResetStartTriggers();
	ResetFinishTriggers();
	EnsureDeadMonsterProps();
	m_iWaveMonstersTotal = 0;
	m_iWaveMonstersKilled = 0;
	m_bHostilesEliminatedFired = false;
}

void CGladderRules::ResetStartTriggers( void )
{
	UTIL_ForEachEntity( []( CBaseEntity *pEnt ) {
		if ( !pEnt || !pEnt->pev )
			return;

		if ( FClassnameIs( pEnt->pev, "trigger_gladder_start" ) )
		{
			CTriggerGladderStart *pStart = dynamic_cast<CTriggerGladderStart *>( pEnt );
			if ( pStart )
			{
				pStart->ResetTrigger();
			}
			else
			{
				pEnt->pev->rendercolor = Vector( GladderPalette::Triggers.r, GladderPalette::Triggers.g, GladderPalette::Triggers.b );
			}
		}
	} );
}

void CGladderRules::ResetFinishTriggers( void )
{
	UTIL_ForEachEntity( []( CBaseEntity *pEnt ) {
		if ( !pEnt || !pEnt->pev )
			return;

		if ( FClassnameIs( pEnt->pev, "trigger_gladder_finish" ) )
		{
			CTriggerGladderFinish *pFinish = dynamic_cast<CTriggerGladderFinish *>( pEnt );
			if ( pFinish )
			{
				pFinish->ResetTrigger();
			}
			else
			{
				pEnt->pev->rendercolor = Vector( GladderPalette::TriggersInactive.r, GladderPalette::TriggersInactive.g, GladderPalette::TriggersInactive.b );
			}
		}
	} );
}

void CGladderRules::EnsureDeadMonsterProps( void )
{
	UTIL_ForEachEntity( []( CBaseEntity *pEnt ) {
		if ( !pEnt || !pEnt->pev )
			return;

		if ( GladderMonsterModifiers::IsDeadMonsterProp( pEnt ) )
		{
			GladderMonsterModifiers::ConfigureDeadMonsterProp( pEnt );
		}
	} );
}

void CGladderRules::EnsureMonsterClipFlags( void )
{
	UTIL_ForEachEntity( []( CBaseEntity *pEnt ) {
		if ( !pEnt || !pEnt->pev || ( pEnt->pev->flags & FL_CLIENT ) || pEnt->IsPlayer() )
			return;

		if ( ( pEnt->MyMonsterPointer() || ( pEnt->pev->flags & FL_MONSTER ) || FClassnameIs( pEnt->pev, "func_pushable" ) ) &&
		     !( pEnt->pev->flags & FL_CLIENT ) && !pEnt->IsPlayer() )
		{
			pEnt->pev->flags |= FL_MONSTERCLIP;
		}
	} );
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

	// 1. Submit to persistent map leaderboards (SPEC §10.2, Issue #17)
	GladderLeaderboardEntry lbEntry;
	lbEntry.timestamp = static_cast<uint64_t>( time( nullptr ) );
	lbEntry.score     = m_lastScoreBreakdown.finalCompositeScore;
	lbEntry.waves     = m_lastScoreBreakdown.wavesCompleted;
	lbEntry.frags     = m_lastScoreBreakdown.totalFrags;
	lbEntry.totalTime = m_lastScoreBreakdown.totalMatchTime;
	lbEntry.grade     = m_lastScoreBreakdown.rankGrade;
	lbEntry.survived  = m_lastScoreBreakdown.bSurvived;

	int rank = -1;
	bool isNewHigh = false;
	const char *pszMap = ( gpGlobals && gpGlobals->mapname ) ? STRING( gpGlobals->mapname ) : "default";
	GladderLeaderboard::SubmitScore( pszMap ? pszMap : "default", lbEntry, rank, isNewHigh );

	// 2. Seamlessly shift player viewport to designated trigger_camera or dynamic fallback (SPEC §6, Issue #16)
	if ( gpGlobals )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBaseEntity *pPlr = UTIL_PlayerByIndex( i );
			if ( pPlr && pPlr->edict() )
			{
				ShiftPlayerCamera( pPlr );
			}
		}
	}

	// 3. Broadcast full summary payload to client HUD (SPEC §9.2, Issue #14, #16)
	BroadcastSummaryUpdate( nullptr, m_lastScoreBreakdown, m_comboTracker.GetStats() );
}

CBaseEntity *CGladderRules::GetOrCreateDynamicCamera( CBaseEntity *pPlr )
{
	if ( !pPlr )
	{
		pPlr = gpGlobals ? UTIL_PlayerByIndex( 1 ) : nullptr;
	}
	if ( !pPlr || !pPlr->edict() )
		return nullptr;

	const char *pszPlayerModel = ( pPlr->pev->model ) ? STRING( pPlr->pev->model ) : "models/player.mdl";
	UTIL_MakeVectors( pPlr->pev->v_angle );
	Vector vecCamPos = pPlr->pev->origin + Vector( 0, 0, 90 ) - gpGlobals->v_forward * 140;
	TraceResult tr;
	UTIL_TraceLine( pPlr->pev->origin + Vector( 0, 0, 36 ), vecCamPos, ignore_monsters, pPlr->edict(), &tr );
	vecCamPos = tr.vecEndPos + tr.vecPlaneNormal * 4;

	Vector vecAngles = UTIL_VecToAngles( ( pPlr->pev->origin + Vector( 0, 0, 20 ) ) - vecCamPos );
	vecAngles.x = -vecAngles.x;

	CBaseEntity *pFallCam = UTIL_FindEntityByTargetname( nullptr, "gladder_dynamic_camera" );
	if ( !pFallCam )
	{
		pFallCam = CBaseEntity::Create( "info_target", vecCamPos, vecAngles, nullptr );
		if ( pFallCam )
		{
			pFallCam->pev->targetname = MAKE_STRING( "gladder_dynamic_camera" );
		}
	}
	if ( pFallCam && pFallCam->edict() )
	{
		if ( pFallCam->pev->modelindex == 0 )
		{
			SET_MODEL( pFallCam->edict(), pszPlayerModel );
		}
		pFallCam->pev->rendermode = kRenderTransTexture;
		pFallCam->pev->renderamt  = 0;
		pFallCam->pev->solid      = SOLID_NOT;
		pFallCam->pev->movetype   = MOVETYPE_NONE;
		pFallCam->pev->angles     = vecAngles;
		pFallCam->pev->v_angle    = vecAngles;
		UTIL_SetOrigin( pFallCam->pev, vecCamPos );
	}
	return pFallCam;
}

void CGladderRules::CollectSummaryCameras( CBaseEntity *pPlr )
{
	m_summaryCameras.clear();
	m_iCurrentSummaryCameraIndex = 0;

	// 1. Dynamic third-person camera is ALWAYS index 0 (SPEC §6, §9.2, Issue #72)
	CBaseEntity *pDynamicCam = GetOrCreateDynamicCamera( pPlr );
	if ( pDynamicCam && pDynamicCam->edict() && !FNullEnt( pDynamicCam->edict() ) )
	{
		EHANDLE hCam;
		hCam = pDynamicCam;
		m_summaryCameras.push_back( hCam );
	}

	// 2. Sequential map-placed cameras follow index 0
	CBaseEntity *pCam = nullptr;
	while ( ( pCam = UTIL_FindEntityByClassname( pCam, "gladder_camera" ) ) != nullptr )
	{
		if ( pCam->edict() && !FNullEnt( pCam->edict() ) )
		{
			EHANDLE hCam;
			hCam = pCam;
			m_summaryCameras.push_back( hCam );
		}
	}

	if ( m_summaryCameras.size() <= 1 )
	{
		while ( ( pCam = UTIL_FindEntityByTargetname( pCam, "gladder_camera" ) ) != nullptr )
		{
			if ( pCam->edict() && !FNullEnt( pCam->edict() ) )
			{
				EHANDLE hCam;
				hCam = pCam;
				m_summaryCameras.push_back( hCam );
			}
		}
	}

	if ( m_summaryCameras.size() <= 1 )
	{
		while ( ( pCam = UTIL_FindEntityByClassname( pCam, "trigger_camera" ) ) != nullptr )
		{
			if ( pCam->edict() && !FNullEnt( pCam->edict() ) )
			{
				EHANDLE hCam;
				hCam = pCam;
				m_summaryCameras.push_back( hCam );
			}
		}
	}
}

void CGladderRules::ActivateSummaryCamera( CBaseEntity *pCamera, CBaseEntity *pPlr )
{
	if ( !pCamera || !pCamera->edict() || !pPlr || !pPlr->edict() )
		return;

	// If this is the dynamic third-person camera, refresh its transform relative to the player
	if ( pCamera->pev->targetname && FStrEq( STRING( pCamera->pev->targetname ), "gladder_dynamic_camera" ) )
	{
		GetOrCreateDynamicCamera( pPlr );
		SET_VIEW( pPlr->edict(), pCamera->edict() );
		return;
	}

	const char *pszPlayerModel = ( pPlr->pev->model ) ? STRING( pPlr->pev->model ) : "models/player.mdl";

	Vector vecCamAngles = pCamera->pev->angles;
	if ( pCamera->pev->target)
	{
		const char *pszTarget = STRING( pCamera->pev->target );
		if ( pszTarget && pszTarget[0] )
		{
			CBaseEntity *pTarget = UTIL_FindEntityByTargetname( nullptr, pszTarget );
			if ( pTarget && pTarget->edict() )
			{
				Vector vecDir = pTarget->pev->origin - pCamera->pev->origin;
				vecCamAngles = UTIL_VecToAngles( vecDir );
				vecCamAngles.x = -vecCamAngles.x; // Pitch inversion for view angles in GoldSrc
			}
		}
	}
	else if ( vecCamAngles == g_vecZero )
	{
		Vector vecDir = ( pPlr->pev->origin + Vector( 0, 0, 20 ) ) - pCamera->pev->origin;
		vecCamAngles = UTIL_VecToAngles( vecDir );
		vecCamAngles.x = -vecCamAngles.x;
	}

	pCamera->pev->angles  = vecCamAngles;
	pCamera->pev->v_angle = vecCamAngles;

	// CRITICAL FOR GOLDSRC ENGINE:
	// Entities with modelindex == 0 are omitted from server delta packets (SV_WriteEntitiesToClient).
	// Without a valid modelindex, the client never receives origin updates and defaults cl_entities[i].origin
	// to (0, 0, 0). Assigning a precached model with renderamt=0 ensures the camera is networked cleanly.
	if ( pCamera->pev->modelindex == 0 )
	{
		SET_MODEL( pCamera->edict(), pszPlayerModel );
	}
	pCamera->pev->rendermode = kRenderTransTexture;
	pCamera->pev->renderamt  = 0;
	pCamera->pev->solid      = SOLID_NOT;
	pCamera->pev->movetype   = MOVETYPE_NONE;
	UTIL_SetOrigin( pCamera->pev, pCamera->pev->origin );

	SET_VIEW( pPlr->edict(), pCamera->edict() );
}

void CGladderRules::AdvanceSummaryCamera( void )
{
	if ( m_summaryCameras.empty() )
		return;

	m_iCurrentSummaryCameraIndex = ( m_iCurrentSummaryCameraIndex + 1 ) % m_summaryCameras.size();
	float flTime = gpGlobals ? gpGlobals->time : 0.0f;
	m_flNextCameraSwitchTime = flTime + 5.0f;

	CBaseEntity *pCam = m_summaryCameras[m_iCurrentSummaryCameraIndex];
	if ( pCam && pCam->edict() && gpGlobals )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBaseEntity *pPlr = UTIL_PlayerByIndex( i );
			if ( pPlr && pPlr->edict() )
			{
				ActivateSummaryCamera( pCam, pPlr );
			}
		}
	}
}

void CGladderRules::ShiftPlayerCamera( CBaseEntity *pPlr )
{
	if ( !pPlr || !pPlr->edict() )
		return;

	if ( m_summaryCameras.empty() )
	{
		CollectSummaryCameras( pPlr );
	}

	if ( !m_summaryCameras.empty() )
	{
		float flTime = gpGlobals ? gpGlobals->time : 0.0f;
		m_flNextCameraSwitchTime = flTime + 5.0f;
		m_iCurrentSummaryCameraIndex = 0;
		CBaseEntity *pCam = m_summaryCameras[0];
		if ( pCam && pCam->edict() )
		{
			ActivateSummaryCamera( pCam, pPlr );
			return;
		}
	}

	CBaseEntity *pFallCam = GetOrCreateDynamicCamera( pPlr );
	if ( pFallCam && pFallCam->edict() )
	{
		SET_VIEW( pPlr->edict(), pFallCam->edict() );
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
	WRITE_SHORT( m_iWaveMonstersKilled );
	WRITE_SHORT( m_iWaveMonstersTotal );
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
	WRITE_SHORT( m_iCollectiblesCount );
	MESSAGE_END();
}

void CGladderRules::BroadcastComboUpdate( CBasePlayer *pPlayer )
{
	if ( !gmsgGladderCombo )
		return;

	int msgDest = pPlayer ? MSG_ONE : MSG_ALL;
	edict_t *pTarget = pPlayer ? pPlayer->edict() : nullptr;

	int multiplier = m_comboTracker.GetCurrentMultiplier();
	float gauge = m_comboTracker.GetNormalizedGauge();
	uint8_t decayFraction255 = static_cast<uint8_t>( (std::max)( 0.0f, (std::min)( 1.0f, gauge ) ) * 255.0f );

	MESSAGE_BEGIN( msgDest, gmsgGladderCombo, nullptr, pTarget );
	WRITE_BYTE( multiplier );
	WRITE_BYTE( decayFraction255 );
	MESSAGE_END();
}

void CGladderRules::BroadcastWaveNotify( CBasePlayer *pPlayer, int iType, int iWaveNumber, int iMutatorType, int iHostileQuota )
{
	if ( !gmsgGladderNotify )
		return;

	int msgDest = pPlayer ? MSG_ONE : MSG_ALL;
	edict_t *pTarget = pPlayer ? pPlayer->edict() : nullptr;

	int iQuota = ( iHostileQuota > 0 ) ? iHostileQuota : 0;
	if ( iQuota > 65535 )
		iQuota = 65535;

	MESSAGE_BEGIN( msgDest, gmsgGladderNotify, nullptr, pTarget );
	WRITE_BYTE( iType );
	WRITE_BYTE( iWaveNumber );
	WRITE_BYTE( iMutatorType );
	WRITE_SHORT( iQuota );
	MESSAGE_END();
}

void CGladderRules::BroadcastSummaryUpdate( CBasePlayer *pPlayer, const GladderScoreBreakdown &score, const GladderKillStats &kills )
{
	if ( !gmsgGladderSummary )
		return;

	int msgDest = pPlayer ? MSG_ONE : MSG_ALL;
	edict_t *pTarget = pPlayer ? pPlayer->edict() : nullptr;

	MESSAGE_BEGIN( msgDest, gmsgGladderSummary, nullptr, pTarget );
	WRITE_BYTE( score.bSurvived ? 1 : 0 );
	WRITE_SHORT( score.wavesCompleted );
	WRITE_LONG( score.finalCompositeScore );
	WRITE_CHAR( score.rankGrade );
	WRITE_SHORT( score.maxComboStreak );
	WRITE_SHORT( score.totalFrags );
	WRITE_SHORT( score.collectiblesCount );
	WRITE_COORD( score.totalMatchTime );
	WRITE_COORD( score.averageLapTime );
	WRITE_COORD( score.fastestLapTime );
	WRITE_COORD( score.slowestLapTime );
	WRITE_COORD( score.totalDamageTaken );
	WRITE_LONG( score.baseCombatPoints );
	WRITE_LONG( score.comboBonusPoints );
	WRITE_LONG( score.waveBonusPoints );
	WRITE_LONG( score.lambdaBonusPoints );
	WRITE_LONG( score.survivalBonusPoints );
	WRITE_LONG( score.damagePenaltyPoints );

	// Species kill pairs (capped at 255)
	uint8_t speciesCount = static_cast<uint8_t>( (std::min)( size_t( 255 ), kills.speciesKills.size() ) );
	WRITE_BYTE( speciesCount );
	uint8_t written = 0;
	for ( const auto &pair : kills.speciesKills )
	{
		if ( written >= speciesCount )
			break;
		std::string friendly = GladderComboTracker::GetFriendlySpeciesName( pair.first );
		WRITE_STRING( friendly.c_str() );
		WRITE_SHORT( pair.second );
		written++;
	}
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
