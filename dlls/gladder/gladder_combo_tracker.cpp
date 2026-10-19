/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Kill Streak Combo Tracker & Itemized Species Kill Log
 *	(SPEC §3, §9.1, §9.2, GitHub Issue #8)
 *
 ****/

#include "gladder_combo_tracker.h"
#include "ai/monsters.h"
#include <algorithm>

GladderComboTracker::GladderComboTracker()
    : m_iCurrentStreak( 0 ),
      m_flComboWindowDuration( DEFAULT_COMBO_WINDOW ),
      m_flComboExpireTime( 0.0f ),
      m_flLastKillTime( 0.0f ),
      m_flNormalizedGauge( 0.0f )
{
	Reset();
}

GladderComboTracker::~GladderComboTracker()
{
}

void GladderComboTracker::Reset( void )
{
	m_stats.Reset();
	m_iCurrentStreak        = 0;
	m_flComboWindowDuration = DEFAULT_COMBO_WINDOW;
	m_flComboExpireTime     = 0.0f;
	m_flLastKillTime        = 0.0f;
	m_flNormalizedGauge     = 0.0f;
}

void GladderComboTracker::Update( float flCurrentTime )
{
	if ( m_iCurrentStreak > 0 )
	{
		if ( flCurrentTime < m_flComboExpireTime )
		{
			float flRemaining = m_flComboExpireTime - flCurrentTime;
			float flWindow = ( m_flComboWindowDuration > 0.0f ) ? m_flComboWindowDuration : DEFAULT_COMBO_WINDOW;
			m_flNormalizedGauge = (std::max)( 0.0f, (std::min)( 1.0f, flRemaining / flWindow ) );
		}
		else
		{
			// Window expired: combo lost and reset
			m_flNormalizedGauge = 0.0f;
			m_iCurrentStreak    = 0;
		}
	}
	else
	{
		m_flNormalizedGauge = 0.0f;
	}
}

static bool ResolvesToClient( entvars_t *pev )
{
	if ( !pev )
		return false;

	if ( pev->flags & FL_CLIENT )
		return true;

	// Follow owner chain up to 4 levels to guard against circular references
	edict_t *pOwner = pev->owner;
	int depth = 0;
	while ( pOwner && !FNullEnt( pOwner ) && !pOwner->free && depth < 4 )
	{
		if ( pOwner->v.flags & FL_CLIENT )
			return true;
		pOwner = pOwner->v.owner;
		depth++;
	}

	return false;
}

bool GladderComboTracker::IsPlayerInstigated( entvars_t *pKiller, entvars_t *pInflictor )
{
	return ResolvesToClient( pKiller ) || ResolvesToClient( pInflictor );
}

bool GladderComboTracker::IsValidCombatant( CBaseMonster *pVictim )
{
	if ( !pVictim || !pVictim->pev )
		return false;

	// Exclude bioweapons and projectiles (e.g. hornets) (SPEC §3, §9.1, Issue #77)
	int iClass = pVictim->Classify();
	if ( iClass == CLASS_PLAYER_BIOWEAPON || iClass == CLASS_ALIEN_BIOWEAPON )
		return false;

	// Exclude player-deployed ordnance and combat projectiles (SPEC §9.1, Issue #100)
	// Independent of Classify() which is dynamic on CSqueakGrenade
	if ( FClassnameIs( pVictim->pev, "monster_snark" ) ||
	     FClassnameIs( pVictim->pev, "monster_satchel" ) ||
	     FClassnameIs( pVictim->pev, "monster_tripmine" ) ||
	     FClassnameIs( pVictim->pev, "grenade" ) ||
	     FClassnameIs( pVictim->pev, "hornet" ) ||
	     FClassnameIs( pVictim->pev, "bolt" ) ||
	     FClassnameIs( pVictim->pev, "rpg_rocket" ) ||
	     FClassnameIs( pVictim->pev, "gonomeguts" ) )
	{
		return false;
	}

	// Exclude pre-placed dead corpses and dead props (SPEC §3, §9.1, Issue #77)
	if ( pVictim->m_MonsterState == MONSTERSTATE_DEAD || pVictim->pev->deadflag == DEAD_DEAD )
		return false;

	const char *pszClassname = STRING( pVictim->pev->classname );
	if ( pszClassname && strstr( pszClassname, "_dead" ) != nullptr )
		return false;

	return true;
}

void GladderComboTracker::OnMonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor, float flCurrentTime )
{
	if ( !pVictim || !pVictim->pev )
		return;

	// Combat kills and combo progression must be attributed strictly to direct and indirect player actions (SPEC §3, §9.1, Issue #60)
	if ( !IsPlayerInstigated( pKiller, pInflictor ) )
		return;

	// Non-combatants such as dead monster props and projectiles are strictly excluded (SPEC §3, §9.1, Issue #77)
	if ( !IsValidCombatant( pVictim ) )
		return;

	// Extract classname for itemized kill log
	std::string szClassname;
	if ( pVictim->pev->classname )
	{
		const char *psz = STRING( pVictim->pev->classname );
		if ( psz && *psz )
		{
			szClassname = psz;
		}
	}
	if ( szClassname.empty() )
	{
		szClassname = "monster_unknown";
	}

	// Record to species log
	m_stats.speciesKills[szClassname]++;
	m_stats.totalFrags++;

	// Detect headshot
	bool bHeadshot = ( pVictim->m_LastHitGroup == HITGROUP_HEAD );
	if ( bHeadshot )
	{
		m_stats.headshotKills++;
	}

	// Calculate streak progression
	if ( m_iCurrentStreak > 0 && flCurrentTime <= m_flComboExpireTime )
	{
		m_iCurrentStreak++;
	}
	else
	{
		m_iCurrentStreak = 1;
	}

	// Record milestone tiers
	if ( m_iCurrentStreak == 2 )
	{
		m_stats.x2Combos++;
	}
	else if ( m_iCurrentStreak == 3 )
	{
		m_stats.x3Combos++;
	}
	else if ( m_iCurrentStreak >= 4 )
	{
		m_stats.x4PlusCombos++;
	}

	if ( m_iCurrentStreak > m_stats.maxComboStreak )
	{
		m_stats.maxComboStreak = m_iCurrentStreak;
	}

	// Refresh combo window
	float flWindow = m_flComboWindowDuration;
	if ( bHeadshot )
	{
		flWindow += HEADSHOT_WINDOW_BONUS;
	}

	m_flLastKillTime    = flCurrentTime;
	m_flComboExpireTime = flCurrentTime + flWindow;
	m_flNormalizedGauge = 1.0f;
}

int GladderComboTracker::GetCurrentMultiplier( void ) const
{
	if ( m_iCurrentStreak <= 1 || m_flNormalizedGauge <= 0.0f )
		return 1;

	return m_iCurrentStreak;
}

float GladderComboTracker::GetRemainingWindow( void ) const
{
	if ( m_iCurrentStreak <= 0 )
		return 0.0f;

	return (std::max)( 0.0f, m_flComboExpireTime - m_flLastKillTime );
}

int GladderComboTracker::GetSpeciesKills( const std::string &species ) const
{
	auto it = m_stats.speciesKills.find( species );
	if ( it != m_stats.speciesKills.end() )
	{
		return it->second;
	}
	return 0;
}

std::string GladderComboTracker::GetFriendlySpeciesName( const std::string &classname )
{
	static const std::unordered_map<std::string, std::string> s_friendlyNames = {
		{ "monster_headcrab",          "Headcrabs" },
		{ "monster_babycrab",          "Baby Headcrabs" },
		{ "monster_zombie",            "Zombies" },
		{ "monster_zombie_barney",     "Zombie Guards" },
		{ "monster_zombie_soldier",    "Zombie Soldiers" },
		{ "monster_gonome",            "Gonomes" },
		{ "monster_houndeye",          "Houndeyes" },
		{ "monster_bullchicken",       "Bullsquids" },
		{ "monster_alien_slave",       "Vortigaunts" },
		{ "monster_vortigaunt",        "Vortigaunts" },
		{ "monster_alien_grunt",       "Alien Grunts" },
		{ "monster_human_grunt",       "HECU Grunts" },
		{ "monster_human_assassin",    "Black Ops Assassins" },
		{ "monster_male_assassin",     "Male Assassins" },
		{ "monster_alien_controller",  "Alien Controllers" },
		{ "monster_gargantua",         "Gargantuas" },
		{ "monster_bigmomma",          "Big Mommas" },
		{ "monster_barnacle",          "Barnacles" },
		{ "monster_ichthyosaur",       "Ichthyosaurs" },
		{ "monster_tentacle",          "Tentacles" },
		{ "monster_bloater",           "Bloaters" },
		{ "monster_barney",            "Security Guards" },
		{ "monster_scientist",         "Scientists" }
	};

	auto it = s_friendlyNames.find( classname );
	if ( it != s_friendlyNames.end() )
	{
		return it->second;
	}

	// Default fallback: strip "monster_" prefix and capitalize
	if ( classname.rfind( "monster_", 0 ) == 0 )
	{
		std::string name = classname.substr( 8 );
		if ( !name.empty() )
		{
			name[0] = static_cast<char>( toupper( name[0] ) );
		}
		return name;
	}

	return classname;
}
