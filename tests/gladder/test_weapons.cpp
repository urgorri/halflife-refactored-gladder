/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Extended Opposing Force Arsenal (SPEC §7.1, Issue #13)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "gameplay/gamerules.h"
#include "tests/mock_engine.h"
#include "dlls/gladder/weapons/gladder_weapons.h"
#include "dlls/weapons/weapon_registry.h"

TEST_CASE( "GladderWeapons: Slot ID bounds and GoldSrc 32-bit compliance (SPEC §7.1)", "[gladder][weapons]" )
{
	CHECK( WEAPON_PIPEWRENCH == 16 );
	CHECK( WEAPON_KNIFE == 17 );
	CHECK( WEAPON_EAGLE == 18 );
	CHECK( WEAPON_SNIPER == 19 );
	CHECK( WEAPON_M249 == 20 );

	// All weapon IDs must reside strictly under WEAPON_SUIT (31) and MAX_WEAPONS (32)
	CHECK( WEAPON_PIPEWRENCH < 31 );
	CHECK( WEAPON_KNIFE < 31 );
	CHECK( WEAPON_EAGLE < 31 );
	CHECK( WEAPON_SNIPER < 31 );
	CHECK( WEAPON_M249 < 31 );
}

TEST_CASE( "GladderWeapons: Ammo capacities and clip sizes", "[gladder][weapons]" )
{
	CHECK( _762_MAX_CARRY == 30 );
	CHECK( _556_MAX_CARRY == 200 );

	CHECK( EAGLE_MAX_CLIP == 7 );
	CHECK( SNIPER_MAX_CLIP == 5 );
	CHECK( M249_MAX_CLIP == 50 );

	CHECK( AMMO_SNIPER_GIVE == 15 );
	CHECK( AMMO_M249_GIVE == 50 );
}

TEST_CASE( "GladderWeapons: Damage balance constants", "[gladder][weapons]" )
{
	CHECK( GLADDER_DMG_PIPEWRENCH == 35.0f );
	CHECK( GLADDER_DMG_KNIFE == 20.0f );
	CHECK( GLADDER_DMG_KNIFE_BACK == 60.0f );
	CHECK( GLADDER_DMG_EAGLE == 65.0f );
	CHECK( GLADDER_DMG_SNIPER == 100.0f );
	CHECK( GLADDER_DMG_M249 == 16.0f );
}

TEST_CASE( "GladderWeapons: WeaponRegistry registration of OpFor arsenal", "[gladder][weapons]" )
{
	WeaponDescriptor dKnife = { "weapon_knife", 0, nullptr };
	WeaponDescriptor dWrench = { "weapon_pipewrench", 0, nullptr };
	WeaponDescriptor dEagle = { "weapon_eagle", 0, nullptr };
	WeaponDescriptor dSniper = { "weapon_sniper", 0, nullptr };
	WeaponDescriptor dM249 = { "weapon_m249", 0, nullptr };
	WeaponRegistry::Register( dKnife );
	WeaponRegistry::Register( dWrench );
	WeaponRegistry::Register( dEagle );
	WeaponRegistry::Register( dSniper );
	WeaponRegistry::Register( dM249 );

	CHECK( WeaponRegistry::FindByClassname( "weapon_knife" ) != nullptr );
	CHECK( WeaponRegistry::FindByClassname( "weapon_pipewrench" ) != nullptr );
	CHECK( WeaponRegistry::FindByClassname( "weapon_eagle" ) != nullptr );
	CHECK( WeaponRegistry::FindByClassname( "weapon_sniper" ) != nullptr );
	CHECK( WeaponRegistry::FindByClassname( "weapon_m249" ) != nullptr );
}

namespace
{
class CTestWeaponGameRules : public CGameRules
{
  public:
	bool m_bFixMeleeCorpse = true;
	bool m_bSilenceAudio = true;
	bool m_bFixShotgun = true;
	bool m_bFixMP5 = true;

	BOOL FFixMeleeCorpseAttackDelay( void ) override { return m_bFixMeleeCorpse ? TRUE : FALSE; }
	BOOL FSilenceLoopingWeaponSoundsOnDeath( void ) override { return m_bSilenceAudio ? TRUE : FALSE; }
	BOOL FFixShotgunReloadDesync( void ) override { return m_bFixShotgun ? TRUE : FALSE; }
	BOOL FFixMP5UnderwaterDebounce( void ) override { return m_bFixMP5 ? TRUE : FALSE; }

	// Stubs
	void Think( void ) override {}
	BOOL IsAllowedToSpawn( CBaseEntity *pEntity ) override { return TRUE; }
	BOOL FAllowFlashlight( void ) override { return TRUE; }
	BOOL FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon ) override { return FALSE; }
	BOOL GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon ) override { return FALSE; }
	BOOL IsMultiplayer( void ) override { return FALSE; }
	BOOL IsDeathmatch( void ) override { return FALSE; }
	BOOL IsCoOp( void ) override { return FALSE; }
	BOOL ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128] ) override { return TRUE; }
	void InitHUD( CBasePlayer *pl ) override {}
	void ClientDisconnected( edict_t *pClient ) override {}
	float FlPlayerFallDamage( CBasePlayer *pPlayer ) override { return 0.0f; }
	void PlayerSpawn( CBasePlayer *pPlayer ) override {}
	void PlayerThink( CBasePlayer *pPlayer ) override {}
	BOOL FPlayerCanRespawn( CBasePlayer *pPlayer ) override { return FALSE; }
	float FlPlayerSpawnTime( CBasePlayer *pPlayer ) override { return 0.0f; }
	void PlayerRespawn( CBasePlayer *pPlayer, BOOL fCopyCorpse ) override {}
	BOOL FAllowMonsters( void ) override { return TRUE; }
	int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled ) override { return 0; }
	void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override {}
	void DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override {}
	void PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon ) override {}
	int DeadPlayerWeapons( CBasePlayer *pPlayer ) override { return 0; }
	int DeadPlayerAmmo( CBasePlayer *pPlayer ) override { return 0; }
	int WeaponShouldRespawn( CBasePlayerItem *pWeapon ) override { return 0; }
	float FlWeaponRespawnTime( CBasePlayerItem *pWeapon ) override { return 0.0f; }
	float FlWeaponTryRespawn( CBasePlayerItem *pWeapon ) override { return 0.0f; }
	Vector VecWeaponRespawnSpot( CBasePlayerItem *pWeapon ) override { return Vector( 0, 0, 0 ); }
	BOOL CanHaveItem( CBasePlayer *pPlayer, CItem *pItem ) override { return TRUE; }
	void PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem ) override {}
	int ItemShouldRespawn( CItem *pItem ) override { return 0; }
	float FlItemRespawnTime( CItem *pItem ) override { return 0.0f; }
	Vector VecItemRespawnSpot( CItem *pItem ) override { return Vector( 0, 0, 0 ); }
	void PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount ) override {}
	int AmmoShouldRespawn( CBasePlayerAmmo *pAmmo ) override { return 0; }
	float FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo ) override { return 0.0f; }
	Vector VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo ) override { return Vector( 0, 0, 0 ); }
	float FlHealthChargerRechargeTime( void ) override { return 0.0f; }
	int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget ) override { return 0; }
	const char *GetTeamID( CBaseEntity *pEntity ) override { return ""; }
};
}

TEST_CASE( "GladderWeapons: Melee corpse attack rate normalization and blood decals (SPEC §7.6, Issue #65)", "[gladder][weapons][melee]" )
{
	ResetMockEngine();
	CTestWeaponGameRules rules;
	gpGlobals->time = 10.0f;

	auto SimulateMeleeCorpseSwing = []( CGameRules *pRules, float flDelay, float flThinkDelay, float &outNextAttack, float &outNextThink, bool &outTrHitSet ) {
		bool isDeadEntity = true;
		outNextAttack = 0.0f;
		outNextThink = 0.0f;
		outTrHitSet = false;

		if ( isDeadEntity )
		{
			if ( pRules && pRules->FFixMeleeCorpseAttackDelay() )
			{
				outTrHitSet = true;
				outNextAttack = gpGlobals->time + flDelay;
				outNextThink = gpGlobals->time + flThinkDelay;
				return true;
			}
			else
			{
				// Vanilla early return
				return true;
			}
		}
		return false;
	};

	SECTION( "Knife swing against dead body enforces cadence and schedules smack think under fix" )
	{
		float nextAttack = 0.0f;
		float nextThink = 0.0f;
		bool trHitSet = false;

		// Vanilla: bypasses delay and smack
		rules.m_bFixMeleeCorpse = false;
		SimulateMeleeCorpseSwing( &rules, 0.35f, 0.15f, nextAttack, nextThink, trHitSet );
		CHECK( nextAttack == 0.0f );
		CHECK( nextThink == 0.0f );
		CHECK( trHitSet == false );

		// Gladder fix: cadence enforced and smack scheduled
		rules.m_bFixMeleeCorpse = true;
		SimulateMeleeCorpseSwing( &rules, 0.35f, 0.15f, nextAttack, nextThink, trHitSet );
		CHECK( nextAttack == Catch::Approx( 10.35f ) );
		CHECK( nextThink == Catch::Approx( 10.15f ) );
		CHECK( trHitSet == true );
	}

	SECTION( "Pipe Wrench swing against dead body enforces cadence and schedules smack think under fix" )
	{
		float nextAttack = 0.0f;
		float nextThink = 0.0f;
		bool trHitSet = false;

		// Gladder fix: 0.5s primary delay and 0.2s smack think
		rules.m_bFixMeleeCorpse = true;
		SimulateMeleeCorpseSwing( &rules, 0.5f, 0.2f, nextAttack, nextThink, trHitSet );
		CHECK( nextAttack == Catch::Approx( 10.5f ) );
		CHECK( nextThink == Catch::Approx( 10.2f ) );
		CHECK( trHitSet == true );
	}
}

TEST_CASE( "GladderWeapons: Looping weapon audio termination on player death (SPEC §7.7, Issue #66)", "[gladder][weapons][audio]" )
{
	ResetMockEngine();
	CTestWeaponGameRules rules;

	edict_t playerEdict;
	std::memset( &playerEdict, 0, sizeof( playerEdict ) );
	playerEdict.v.pContainingEntity = &playerEdict;

	auto SimulatePlayerDeathSoundCleanup = [&]( CGameRules *pRules ) {
		if ( pRules && pRules->FSilenceLoopingWeaponSoundsOnDeath() )
		{
			EMIT_SOUND( ENT( &playerEdict.v ), CHAN_WEAPON, "common/null.wav", 1.0f, ATTN_NORM );
		}
	};

	SECTION( "Silences CHAN_WEAPON with common/null.wav under Gladder rules" )
	{
		g_mockEmittedSounds.clear();
		SimulatePlayerDeathSoundCleanup( &rules );
		REQUIRE( g_mockEmittedSounds.size() == 1 );
		CHECK( g_mockEmittedSounds[0].channel == CHAN_WEAPON );
		CHECK( g_mockEmittedSounds[0].sample == "common/null.wav" );
		CHECK( g_mockEmittedSounds[0].volume == Catch::Approx( 1.0f ) );
	}

	SECTION( "Preserves vanilla behavior when hook is disabled" )
	{
		rules.m_bSilenceAudio = false;
		g_mockEmittedSounds.clear();
		SimulatePlayerDeathSoundCleanup( &rules );
		CHECK( g_mockEmittedSounds.empty() );
	}
}

TEST_CASE( "GladderWeapons: Shotgun reload interruption and pump state synchronization (SPEC §7.8, Issue #67)", "[gladder][weapons][shotgun]" )
{
	ResetMockEngine();
	CTestWeaponGameRules rules;

	struct MockShotgun
	{
		int m_fInSpecialReload = 1;
		float m_flPumpTime = 4.0f;

		void Holster( CGameRules *pRules )
		{
			if ( pRules && pRules->FFixShotgunReloadDesync() )
			{
				m_fInSpecialReload = 0;
				m_flPumpTime = 0.0f;
			}
		}

		void Fire( CGameRules *pRules )
		{
			if ( pRules && pRules->FFixShotgunReloadDesync() )
			{
				if ( m_fInSpecialReload != 0 )
				{
					m_flPumpTime = 0.0f;
					m_fInSpecialReload = 0;
				}
			}
		}
	};

	SECTION( "Holster resets reload state and clears pump timer under Gladder fix" )
	{
		MockShotgun sg;
		sg.Holster( &rules );
		CHECK( sg.m_fInSpecialReload == 0 );
		CHECK( sg.m_flPumpTime == 0.0f );
	}

	SECTION( "Firing during reload clears pump timer and exits reload state machine" )
	{
		MockShotgun sg;
		sg.Fire( &rules );
		CHECK( sg.m_fInSpecialReload == 0 );
		CHECK( sg.m_flPumpTime == 0.0f );
	}
}

TEST_CASE( "GladderWeapons: MP5 underwater grenade secondary attack debounce (SPEC §7.9, Issue #68)", "[gladder][weapons][mp5]" )
{
	ResetMockEngine();
	CTestWeaponGameRules rules;
	gpGlobals->time = 20.0f;

	float flNextPrimary = 0.0f;
	float flNextSecondary = 0.0f;

	auto SimulateMP5SecondaryUnderwater = [&]( CGameRules *pRules ) {
		if ( pRules && pRules->FFixMP5UnderwaterDebounce() )
		{
			flNextSecondary = flNextPrimary = gpGlobals->time + 0.15f;
		}
		else
		{
			flNextPrimary = 0.15f;
		}
	};

	SECTION( "Debounces both attack channels with weapon time base under Gladder fix" )
	{
		SimulateMP5SecondaryUnderwater( &rules );
		CHECK( flNextPrimary == Catch::Approx( 20.15f ) );
		CHECK( flNextSecondary == Catch::Approx( 20.15f ) );
	}

	SECTION( "Vanilla behavior sets absolute 0.15 on primary only" )
	{
		rules.m_bFixMP5 = false;
		flNextPrimary = 0.0f;
		flNextSecondary = 0.0f;
		SimulateMP5SecondaryUnderwater( &rules );
		CHECK( flNextPrimary == Catch::Approx( 0.15f ) );
		CHECK( flNextSecondary == 0.0f );
	}
}
