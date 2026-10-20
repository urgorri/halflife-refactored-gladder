/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Zombie Security Guard Implementation (SPEC §3, Issue #109)
 *
 ****/

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/monsters.h"
#include "ai/schedule.h"
#include "core/skill_manager.h"
#include "gameplay/gamerules.h"
#include "gladder_monster_modifiers.h"

#define ZOMBIE_BARNEY_AE_ATTACK_RIGHT 0x01
#define ZOMBIE_BARNEY_AE_ATTACK_LEFT  0x02
#define ZOMBIE_BARNEY_AE_ATTACK_BOTH  0x03

#define ZOMBIE_BARNEY_FLINCH_DELAY 2 // At most one flinch every n secs

extern DLL_GLOBAL int g_iSkillLevel;

REGISTER_SKILL_DEFAULT( sk_zombie_barney_health, 50.0f, 70.0f, 90.0f );
REGISTER_SKILL_DEFAULT( sk_zombie_barney_dmg_one_slash, 10.0f, 20.0f, 25.0f );
REGISTER_SKILL_DEFAULT( sk_zombie_barney_dmg_both_slash, 25.0f, 35.0f, 40.0f );

class CZombieBarney : public CBaseMonster
{
  public:
	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override;
	void SetActivity( Activity NewActivity ) override;
	int Classify() override;
	void HandleAnimEvent( MonsterEvent_t *pEvent ) override;
	int IgnoreConditions() override;

	float m_flNextFlinch;

	void PainSound() override;
	void AlertSound() override;
	void IdleSound() override;
	void AttackSound();

	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];

	// No range attacks
	BOOL CheckRangeAttack1( float flDot, float flDist ) override { return FALSE; }
	BOOL CheckRangeAttack2( float flDot, float flDist ) override { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType ) override;
};

LINK_ENTITY_TO_CLASS( monster_zombie_barney, CZombieBarney );

const char *CZombieBarney::pAttackHitSounds[] = {
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CZombieBarney::pAttackMissSounds[] = {
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CZombieBarney::pAttackSounds[] = {
	"zombie/zo_attack1.wav",
	"zombie/zo_attack2.wav",
};

const char *CZombieBarney::pIdleSounds[] = {
	"zombie/zo_idle1.wav",
	"zombie/zo_idle2.wav",
	"zombie/zo_idle3.wav",
	"zombie/zo_idle4.wav",
};

const char *CZombieBarney::pAlertSounds[] = {
	"zombie/zo_alert10.wav",
	"zombie/zo_alert20.wav",
	"zombie/zo_alert30.wav",
};

const char *CZombieBarney::pPainSounds[] = {
	"zombie/zo_pain1.wav",
	"zombie/zo_pain2.wav",
};

int CZombieBarney::Classify()
{
	return CLASS_ALIEN_MONSTER;
}

void CZombieBarney::SetActivity( Activity NewActivity )
{
	CBaseMonster::SetActivity( NewActivity );

	if ( NewActivity == ACT_WALK || NewActivity == ACT_RUN )
	{
		float flSpeed = GladderMonsterModifiers::GetZombieMoveSpeed( g_iSkillLevel );
		if ( g_pGameRules )
			flSpeed = g_pGameRules->FlMonsterMoveSpeed( this, NewActivity, flSpeed );
		pev->framerate = flSpeed;
	}
}

void CZombieBarney::SetYawSpeed()
{
	pev->yaw_speed = 120;
}

int CZombieBarney::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// Take 30% damage from bullets
	if ( bitsDamageType == DMG_BULLET )
	{
		Vector vecDir = pev->origin - ( pevInflictor->absmin + pevInflictor->absmax ) * 0.5f;
		vecDir = vecDir.Normalize();
		float flForce = DamageForce( flDamage );
		pev->velocity = pev->velocity + vecDir * flForce;
		flDamage *= 0.3f;
	}

	if ( IsAlive() )
		PainSound();
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CZombieBarney::PainSound()
{
	int pitch = 95 + RANDOM_LONG( 0, 9 );

	if ( RANDOM_LONG( 0, 5 ) < 2 )
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, pPainSounds[RANDOM_LONG( 0, ARRAYSIZE( pPainSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
}

void CZombieBarney::AlertSound()
{
	int pitch = 95 + RANDOM_LONG( 0, 9 );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, pAlertSounds[RANDOM_LONG( 0, ARRAYSIZE( pAlertSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
}

void CZombieBarney::IdleSound()
{
	int pitch = 100 + RANDOM_LONG( -5, 5 );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, pIdleSounds[RANDOM_LONG( 0, ARRAYSIZE( pIdleSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
}

void CZombieBarney::AttackSound()
{
	int pitch = 100 + RANDOM_LONG( -5, 5 );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, pAttackSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
}

void CZombieBarney::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	float flDmgOne = SkillManager::Query( "sk_zombie_barney_dmg_one_slash", 20.0f );
	float flDmgBoth = SkillManager::Query( "sk_zombie_barney_dmg_both_slash", 35.0f );

	switch ( pEvent->event )
	{
		case ZOMBIE_BARNEY_AE_ATTACK_RIGHT:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, flDmgOne, DMG_SLASH );
			if ( pHurt )
			{
				if ( ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) != 0 )
				{
					pHurt->pev->punchangle.z = -18;
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 100;
				}
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, pAttackHitSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackHitSounds ) - 1 )], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
			}
			else
			{
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, pAttackMissSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackMissSounds ) - 1 )], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
			}

			if ( RANDOM_LONG( 0, 1 ) )
				AttackSound();
			break;
		}

		case ZOMBIE_BARNEY_AE_ATTACK_LEFT:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, flDmgOne, DMG_SLASH );
			if ( pHurt )
			{
				if ( ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) != 0 )
				{
					pHurt->pev->punchangle.z = 18;
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * 100;
				}
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, pAttackHitSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackHitSounds ) - 1 )], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
			}
			else
			{
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, pAttackMissSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackMissSounds ) - 1 )], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
			}

			if ( RANDOM_LONG( 0, 1 ) )
				AttackSound();
			break;
		}

		case ZOMBIE_BARNEY_AE_ATTACK_BOTH:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, flDmgBoth, DMG_SLASH );
			if ( pHurt )
			{
				if ( ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) != 0 )
				{
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * -100;
				}
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, pAttackHitSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackHitSounds ) - 1 )], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
			}
			else
			{
				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, pAttackMissSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackMissSounds ) - 1 )], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
			}

			if ( RANDOM_LONG( 0, 1 ) )
				AttackSound();
			break;
		}

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

void CZombieBarney::Spawn()
{
	Precache();

	SET_MODEL( ENT( pev ), "models/zombie_barney.mdl" );
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid      = SOLID_SLIDEBOX;
	pev->movetype   = MOVETYPE_STEP;
	m_bloodColor    = BLOOD_COLOR_GREEN;
	pev->health     = SkillManager::Query( "sk_zombie_barney_health", 70.0f );
	pev->view_ofs   = VEC_VIEW;
	m_flFieldOfView = 0.5f;
	m_MonsterState  = MONSTERSTATE_NONE;
	m_afCapability  = bits_CAP_DOORS_GROUP;

	MonsterInit();
}

void CZombieBarney::Precache()
{
	size_t i;

	PRECACHE_MODEL( "models/zombie_barney.mdl" );

	for ( i = 0; i < ARRAYSIZE( pAttackHitSounds ); i++ )
		PRECACHE_SOUND( (char *)pAttackHitSounds[i] );

	for ( i = 0; i < ARRAYSIZE( pAttackMissSounds ); i++ )
		PRECACHE_SOUND( (char *)pAttackMissSounds[i] );

	for ( i = 0; i < ARRAYSIZE( pAttackSounds ); i++ )
		PRECACHE_SOUND( (char *)pAttackSounds[i] );

	for ( i = 0; i < ARRAYSIZE( pIdleSounds ); i++ )
		PRECACHE_SOUND( (char *)pIdleSounds[i] );

	for ( i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PRECACHE_SOUND( (char *)pAlertSounds[i] );

	for ( i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PRECACHE_SOUND( (char *)pPainSounds[i] );
}

int CZombieBarney::IgnoreConditions()
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if ( m_Activity == ACT_MELEE_ATTACK1 )
	{
		if ( m_flNextFlinch >= gpGlobals->time )
			iIgnore |= ( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE );
	}

	if ( m_Activity == ACT_SMALL_FLINCH || m_Activity == ACT_BIG_FLINCH )
	{
		if ( m_flNextFlinch < gpGlobals->time )
			m_flNextFlinch = gpGlobals->time + ZOMBIE_BARNEY_FLINCH_DELAY;
	}

	return iIgnore;
}
