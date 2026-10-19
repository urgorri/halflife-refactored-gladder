/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Zombie Soldier Implementation (SPEC §3, Issue #109)
 *
 ****/

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/monsters.h"
#include "ai/schedule.h"
#include "core/skill_manager.h"
#include "gladder_monster_modifiers.h"

#define ZOMBIE_SOLDIER_AE_ATTACK_RIGHT 0x01
#define ZOMBIE_SOLDIER_AE_ATTACK_LEFT  0x02
#define ZOMBIE_SOLDIER_AE_ATTACK_BOTH  0x03

#define ZOMBIE_SOLDIER_FLINCH_DELAY 2

extern DLL_GLOBAL int g_iSkillLevel;

REGISTER_SKILL_DEFAULT( sk_zombie_soldier_health, 80.0f, 110.0f, 140.0f );
REGISTER_SKILL_DEFAULT( sk_zombie_soldier_dmg_one_slash, 15.0f, 25.0f, 30.0f );
REGISTER_SKILL_DEFAULT( sk_zombie_soldier_dmg_both_slash, 30.0f, 45.0f, 55.0f );

class CZombieSoldier : public CBaseMonster
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

LINK_ENTITY_TO_CLASS( monster_zombie_soldier, CZombieSoldier );

const char *CZombieSoldier::pAttackHitSounds[] = {
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CZombieSoldier::pAttackMissSounds[] = {
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CZombieSoldier::pAttackSounds[] = {
	"zombie/zo_attack1.wav",
	"zombie/zo_attack2.wav",
};

const char *CZombieSoldier::pIdleSounds[] = {
	"zombie/zo_idle1.wav",
	"zombie/zo_idle2.wav",
	"zombie/zo_idle3.wav",
	"zombie/zo_idle4.wav",
};

const char *CZombieSoldier::pAlertSounds[] = {
	"zombie/zo_alert10.wav",
	"zombie/zo_alert20.wav",
};

const char *CZombieSoldier::pPainSounds[] = {
	"zombie/zo_pain1.wav",
	"zombie/zo_pain2.wav",
};

int CZombieSoldier::Classify()
{
	return CLASS_ALIEN_MONSTER;
}

void CZombieSoldier::SetActivity( Activity NewActivity )
{
	CBaseMonster::SetActivity( NewActivity );

	if ( NewActivity == ACT_WALK || NewActivity == ACT_RUN )
	{
		pev->framerate = GladderMonsterModifiers::GetDifficultyFramerateMultiplier( g_iSkillLevel );
	}
}

void CZombieSoldier::SetYawSpeed()
{
	pev->yaw_speed = 120;
}

int CZombieSoldier::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
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

void CZombieSoldier::PainSound()
{
	int pitch = 95 + RANDOM_LONG( 0, 9 );

	if ( RANDOM_LONG( 0, 5 ) < 2 )
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, pPainSounds[RANDOM_LONG( 0, ARRAYSIZE( pPainSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
}

void CZombieSoldier::AlertSound()
{
	int pitch = 95 + RANDOM_LONG( 0, 9 );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, pAlertSounds[RANDOM_LONG( 0, ARRAYSIZE( pAlertSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
}

void CZombieSoldier::IdleSound()
{
	int pitch = 100 + RANDOM_LONG( -5, 5 );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, pIdleSounds[RANDOM_LONG( 0, ARRAYSIZE( pIdleSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
}

void CZombieSoldier::AttackSound()
{
	int pitch = 100 + RANDOM_LONG( -5, 5 );

	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, pAttackSounds[RANDOM_LONG( 0, ARRAYSIZE( pAttackSounds ) - 1 )], 1.0, ATTN_NORM, 0, pitch );
}

void CZombieSoldier::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	float flDmgOne = SkillManager::Query( "sk_zombie_soldier_dmg_one_slash", 25.0f );
	float flDmgBoth = SkillManager::Query( "sk_zombie_soldier_dmg_both_slash", 45.0f );

	switch ( pEvent->event )
	{
		case ZOMBIE_SOLDIER_AE_ATTACK_RIGHT:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, flDmgOne, DMG_SLASH );
			if ( pHurt )
			{
				if ( ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) != 0 )
				{
					pHurt->pev->punchangle.z = -18;
					pHurt->pev->punchangle.x = 5;
					if ( g_iSkillLevel >= SKILL_MEDIUM )
					{
						Vector vecDir = ( pHurt->pev->origin - pev->origin ).Normalize();
						pHurt->pev->velocity = pHurt->pev->velocity + vecDir * 220.0f + Vector( 0, 0, 120.0f );
						pHurt->pev->flags &= ~FL_ONGROUND;
						pHurt->pev->maxspeed = 175.0f; // 35% speed penalty (SPEC §3, Issue #116)
						pHurt->pev->fuser4 = ( gpGlobals ? gpGlobals->time : 0.0f ) + 1.75f;
					}
					else
					{
						pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 100;
					}
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

		case ZOMBIE_SOLDIER_AE_ATTACK_LEFT:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, flDmgOne, DMG_SLASH );
			if ( pHurt )
			{
				if ( ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) != 0 )
				{
					pHurt->pev->punchangle.z = 18;
					pHurt->pev->punchangle.x = 5;
					if ( g_iSkillLevel >= SKILL_MEDIUM )
					{
						Vector vecDir = ( pHurt->pev->origin - pev->origin ).Normalize();
						pHurt->pev->velocity = pHurt->pev->velocity + vecDir * 220.0f + Vector( 0, 0, 120.0f );
						pHurt->pev->flags &= ~FL_ONGROUND;
						pHurt->pev->maxspeed = 175.0f;
						pHurt->pev->fuser4 = ( gpGlobals ? gpGlobals->time : 0.0f ) + 1.75f;
					}
					else
					{
						pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * 100;
					}
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

		case ZOMBIE_SOLDIER_AE_ATTACK_BOTH:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, flDmgBoth, DMG_SLASH );
			if ( pHurt )
			{
				if ( ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) != 0 )
				{
					pHurt->pev->punchangle.x = 5;
					if ( g_iSkillLevel >= SKILL_MEDIUM )
					{
						Vector vecDir = ( pHurt->pev->origin - pev->origin ).Normalize();
						pHurt->pev->velocity = pHurt->pev->velocity + vecDir * 220.0f + Vector( 0, 0, 120.0f );
						pHurt->pev->flags &= ~FL_ONGROUND;
						pHurt->pev->maxspeed = 175.0f;
						pHurt->pev->fuser4 = ( gpGlobals ? gpGlobals->time : 0.0f ) + 1.75f;
					}
					else
					{
						pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * -100;
					}
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

void CZombieSoldier::Spawn()
{
	Precache();

	SET_MODEL( ENT( pev ), "models/zombie_soldier.mdl" );
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid      = SOLID_SLIDEBOX;
	pev->movetype   = MOVETYPE_STEP;
	m_bloodColor    = BLOOD_COLOR_GREEN;
	pev->health     = SkillManager::Query( "sk_zombie_soldier_health", 110.0f );
	pev->view_ofs   = VEC_VIEW;
	m_flFieldOfView = 0.5f;
	m_MonsterState  = MONSTERSTATE_NONE;
	m_afCapability  = bits_CAP_DOORS_GROUP;

	MonsterInit();
}

void CZombieSoldier::Precache()
{
	size_t i;

	PRECACHE_MODEL( "models/zombie_soldier.mdl" );

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

int CZombieSoldier::IgnoreConditions()
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
			m_flNextFlinch = gpGlobals->time + ZOMBIE_SOLDIER_FLINCH_DELAY;
	}

	return iIgnore;
}

//=========================================================
// DEAD HGRUNT ZOMBIE PROP (SPEC §3, Issue #109)
//=========================================================
class CDeadZombieSoldier : public CBaseMonster
{
  public:
	void Spawn() override;
	int Classify() override { return CLASS_ALIEN_MONSTER; }

	void KeyValue( KeyValueData *pkvd ) override;

	int m_iPose;
	static char *m_szPoses[2];
};

char *CDeadZombieSoldier::m_szPoses[] = { (char *)"dead_on_stomach", (char *)"dead_on_back" };

void CDeadZombieSoldier::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "pose" ) )
	{
		m_iPose        = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_zombie_soldier_dead, CDeadZombieSoldier );

void CDeadZombieSoldier::Spawn()
{
	PRECACHE_MODEL( "models/zombie_soldier.mdl" );
	SET_MODEL( ENT( pev ), "models/zombie_soldier.mdl" );

	pev->effects   = 0;
	pev->yaw_speed = 8;
	pev->sequence  = 0;
	m_bloodColor   = BLOOD_COLOR_RED;

	if ( m_iPose < 0 || m_iPose >= 2 )
		m_iPose = 0;

	pev->sequence = LookupSequence( m_szPoses[m_iPose] );
	if ( pev->sequence == -1 )
	{
		ALERT( at_console, "Dead zombie soldier with bad pose\n" );
	}

	pev->health = 8;
	MonsterInitDead();
}
