/***
 *
 *	Behavioral Equivalence Verification - Layer 2B: Save/Restore Member Layout
 *
 *	Validates that entity member variable offsets serialized via TYPEDESCRIPTION
 *	tables remain binary-compatible with savegame and level-transition baselines.
 *
 ****/

#include <stddef.h>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons/weapon_base.h"
#include "weapons/weapon_defs.h"
#include "weapons/projectile_hornet.h"
#include "weapons/projectile_rocket.h"
#include "weapons/weapon_gauss.h"
#include "weapons/weapon_egon.h"
#include "weapons/weapon_rpg.h"
#include "weapons/weapon_satchel.h"
#include "weapons/weapon_shotgun.h"
#include "weapons/weapon_box.h"
#include "systems/chargers.h"

#if defined( _MSC_VER )

// ============================================================================
// MSVC 32-bit x86 Layout Assertions
// ============================================================================

// --- CBasePlayer ---
static_assert( offsetof( CBasePlayer, m_flFlashLightTime ) == 712, "CBasePlayer::m_flFlashLightTime offset shifted" );
static_assert( offsetof( CBasePlayer, m_iFlashBattery ) == 716, "CBasePlayer::m_iFlashBattery offset shifted" );
static_assert( offsetof( CBasePlayer, m_afButtonLast ) == 720, "CBasePlayer::m_afButtonLast offset shifted" );
static_assert( offsetof( CBasePlayer, m_afButtonPressed ) == 724, "CBasePlayer::m_afButtonPressed offset shifted" );
static_assert( offsetof( CBasePlayer, m_afButtonReleased ) == 728, "CBasePlayer::m_afButtonReleased offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgItems ) == 748, "CBasePlayer::m_rgItems offset shifted" );
static_assert( offsetof( CBasePlayer, m_afPhysicsFlags ) == 776, "CBasePlayer::m_afPhysicsFlags offset shifted" );
static_assert( offsetof( CBasePlayer, m_flTimeStepSound ) == 784, "CBasePlayer::m_flTimeStepSound offset shifted" );
static_assert( offsetof( CBasePlayer, m_flTimeWeaponIdle ) == 788, "CBasePlayer::m_flTimeWeaponIdle offset shifted" );
static_assert( offsetof( CBasePlayer, m_flSwimTime ) == 792, "CBasePlayer::m_flSwimTime offset shifted" );
static_assert( offsetof( CBasePlayer, m_flDuckTime ) == 796, "CBasePlayer::m_flDuckTime offset shifted" );
static_assert( offsetof( CBasePlayer, m_flWallJumpTime ) == 800, "CBasePlayer::m_flWallJumpTime offset shifted" );
static_assert( offsetof( CBasePlayer, m_flSuitUpdate ) == 804, "CBasePlayer::m_flSuitUpdate offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgSuitPlayList ) == 808, "CBasePlayer::m_rgSuitPlayList offset shifted" );
static_assert( offsetof( CBasePlayer, m_iSuitPlayNext ) == 824, "CBasePlayer::m_iSuitPlayNext offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgiSuitNoRepeat ) == 828, "CBasePlayer::m_rgiSuitNoRepeat offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgflSuitNoRepeatTime ) == 956, "CBasePlayer::m_rgflSuitNoRepeatTime offset shifted" );
static_assert( offsetof( CBasePlayer, m_lastDamageAmount ) == 1084, "CBasePlayer::m_lastDamageAmount offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgpPlayerItems ) == 1204, "CBasePlayer::m_rgpPlayerItems offset shifted" );
static_assert( offsetof( CBasePlayer, m_pActiveItem ) == 1228, "CBasePlayer::m_pActiveItem offset shifted" );
static_assert( offsetof( CBasePlayer, m_pLastItem ) == 1236, "CBasePlayer::m_pLastItem offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgAmmo ) == 1240, "CBasePlayer::m_rgAmmo offset shifted" );
static_assert( offsetof( CBasePlayer, m_idrowndmg ) == 1124, "CBasePlayer::m_idrowndmg offset shifted" );
static_assert( offsetof( CBasePlayer, m_idrownrestored ) == 1128, "CBasePlayer::m_idrownrestored offset shifted" );
static_assert( offsetof( CBasePlayer, m_tSneaking ) == 1172, "CBasePlayer::m_tSneaking offset shifted" );
static_assert( offsetof( CBasePlayer, m_iTrain ) == 1144, "CBasePlayer::m_iTrain offset shifted" );
static_assert( offsetof( CBasePlayer, m_bitsHUDDamage ) == 1132, "CBasePlayer::m_bitsHUDDamage offset shifted" );
static_assert( offsetof( CBasePlayer, m_flFallVelocity ) == 744, "CBasePlayer::m_flFallVelocity offset shifted" );
static_assert( offsetof( CBasePlayer, m_iTargetVolume ) == 692, "CBasePlayer::m_iTargetVolume offset shifted" );
static_assert( offsetof( CBasePlayer, m_iWeaponVolume ) == 696, "CBasePlayer::m_iWeaponVolume offset shifted" );
static_assert( offsetof( CBasePlayer, m_iExtraSoundTypes ) == 700, "CBasePlayer::m_iExtraSoundTypes offset shifted" );
static_assert( offsetof( CBasePlayer, m_iWeaponFlash ) == 704, "CBasePlayer::m_iWeaponFlash offset shifted" );
static_assert( offsetof( CBasePlayer, m_fLongJump ) == 1168, "CBasePlayer::m_fLongJump offset shifted" );
static_assert( offsetof( CBasePlayer, m_fInitHUD ) == 1136, "CBasePlayer::m_fInitHUD offset shifted" );
static_assert( offsetof( CBasePlayer, m_tbdPrev ) == 1088, "CBasePlayer::m_tbdPrev offset shifted" );
static_assert( offsetof( CBasePlayer, m_pTank ) == 1152, "CBasePlayer::m_pTank offset shifted" );
static_assert( offsetof( CBasePlayer, m_iHideHUD ) == 1188, "CBasePlayer::m_iHideHUD offset shifted" );
static_assert( offsetof( CBasePlayer, m_iFOV ) == 1196, "CBasePlayer::m_iFOV offset shifted" );

// --- CBasePlayerItem ---
static_assert( offsetof( CBasePlayerItem, m_pPlayer ) == 112, "CBasePlayerItem::m_pPlayer offset shifted" );
static_assert( offsetof( CBasePlayerItem, m_pNext ) == 116, "CBasePlayerItem::m_pNext offset shifted" );
static_assert( offsetof( CBasePlayerItem, m_iId ) == 120, "CBasePlayerItem::m_iId offset shifted" );

// --- CBasePlayerWeapon ---
static_assert( offsetof( CBasePlayerWeapon, m_flNextPrimaryAttack ) == 140, "CBasePlayerWeapon::m_flNextPrimaryAttack offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_flNextSecondaryAttack ) == 144, "CBasePlayerWeapon::m_flNextSecondaryAttack offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_flTimeWeaponIdle ) == 148, "CBasePlayerWeapon::m_flTimeWeaponIdle offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_iPrimaryAmmoType ) == 152, "CBasePlayerWeapon::m_iPrimaryAmmoType offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_iSecondaryAmmoType ) == 156, "CBasePlayerWeapon::m_iSecondaryAmmoType offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_iClip ) == 160, "CBasePlayerWeapon::m_iClip offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_iDefaultAmmo ) == 176, "CBasePlayerWeapon::m_iDefaultAmmo offset shifted" );

// --- Weapon Subclasses ---
static_assert( offsetof( CGauss, m_fInAttack ) == 76, "CGauss::m_fInAttack offset shifted" );
static_assert( offsetof( CGauss, m_fPrimaryFire ) == 204, "CGauss::m_fPrimaryFire offset shifted" );
static_assert( offsetof( CEgon, m_fireState ) == 80, "CEgon::m_fireState offset shifted" );
static_assert( offsetof( CEgon, m_flAmmoUseTime ) == 188, "CEgon::m_flAmmoUseTime offset shifted" );
static_assert( offsetof( CRpg, m_fSpotActive ) == 192, "CRpg::m_fSpotActive offset shifted" );
static_assert( offsetof( CRpg, m_cActiveRockets ) == 196, "CRpg::m_cActiveRockets offset shifted" );
static_assert( offsetof( CSatchel, m_chargeReady ) == 188, "CSatchel::m_chargeReady offset shifted" );
static_assert( offsetof( CShotgun, m_flNextReload ) == 192, "CShotgun::m_flNextReload offset shifted" );
static_assert( offsetof( CShotgun, m_fInSpecialReload ) == 136, "CShotgun::m_fInSpecialReload offset shifted" );
static_assert( offsetof( CShotgun, m_flPumpTime ) == 132, "CShotgun::m_flPumpTime offset shifted" );
static_assert( offsetof( CWeaponBox, m_rgAmmo ) == 236, "CWeaponBox::m_rgAmmo offset shifted" );
static_assert( offsetof( CWeaponBox, m_rgiszAmmo ) == 108, "CWeaponBox::m_rgiszAmmo offset shifted" );
static_assert( offsetof( CWeaponBox, m_rgpPlayerItems ) == 84, "CWeaponBox::m_rgpPlayerItems offset shifted" );
static_assert( offsetof( CWeaponBox, m_cAmmoTypes ) == 364, "CWeaponBox::m_cAmmoTypes offset shifted" );

// --- Weapon Projectiles ---
static_assert( offsetof( CHornet, m_flStopAttack ) == 664, "CHornet::m_flStopAttack offset shifted" );
static_assert( offsetof( CHornet, m_iHornetType ) == 668, "CHornet::m_iHornetType offset shifted" );
static_assert( offsetof( CHornet, m_flFlySpeed ) == 672, "CHornet::m_flFlySpeed offset shifted" );
static_assert( offsetof( CRpgRocket, m_flIgniteTime ) == 672, "CRpgRocket::m_flIgniteTime offset shifted" );
static_assert( offsetof( CRpgRocket, m_hLauncher ) == 676, "CRpgRocket::m_hLauncher offset shifted" );

// --- Entity Base Classes ---
static_assert( offsetof( CBaseEntity, m_pGoalEnt ) == 8, "CBaseEntity::m_pGoalEnt offset shifted" );
static_assert( offsetof( CBaseEntity, m_pfnThink ) == 16, "CBaseEntity::m_pfnThink offset shifted" );
static_assert( offsetof( CBaseEntity, m_pfnTouch ) == 20, "CBaseEntity::m_pfnTouch offset shifted" );
static_assert( offsetof( CBaseEntity, m_pfnUse ) == 24, "CBaseEntity::m_pfnUse offset shifted" );
static_assert( offsetof( CBaseEntity, m_pfnBlocked ) == 28, "CBaseEntity::m_pfnBlocked offset shifted" );
static_assert( offsetof( CBaseDelay, m_flDelay ) == 84, "CBaseDelay::m_flDelay offset shifted" );
static_assert( offsetof( CBaseDelay, m_iszKillTarget ) == 88, "CBaseDelay::m_iszKillTarget offset shifted" );
static_assert( offsetof( CBaseAnimating, m_flFrameRate ) == 92, "CBaseAnimating::m_flFrameRate offset shifted" );
static_assert( offsetof( CBaseAnimating, m_flGroundSpeed ) == 96, "CBaseAnimating::m_flGroundSpeed offset shifted" );
static_assert( offsetof( CBaseAnimating, m_flLastEventCheck ) == 100, "CBaseAnimating::m_flLastEventCheck offset shifted" );
static_assert( offsetof( CBaseAnimating, m_fSequenceFinished ) == 104, "CBaseAnimating::m_fSequenceFinished offset shifted" );
static_assert( offsetof( CBaseAnimating, m_fSequenceLoops ) == 108, "CBaseAnimating::m_fSequenceLoops offset shifted" );
static_assert( offsetof( CBaseToggle, m_toggle_state ) == 112, "CBaseToggle::m_toggle_state offset shifted" );
static_assert( offsetof( CBaseToggle, m_flActivateFinished ) == 116, "CBaseToggle::m_flActivateFinished offset shifted" );
static_assert( offsetof( CBaseToggle, m_flMoveDistance ) == 120, "CBaseToggle::m_flMoveDistance offset shifted" );
static_assert( offsetof( CBaseToggle, m_flWait ) == 124, "CBaseToggle::m_flWait offset shifted" );
static_assert( offsetof( CBaseToggle, m_flLip ) == 128, "CBaseToggle::m_flLip offset shifted" );
static_assert( offsetof( CBaseToggle, m_flTWidth ) == 132, "CBaseToggle::m_flTWidth offset shifted" );
static_assert( offsetof( CBaseToggle, m_flTLength ) == 136, "CBaseToggle::m_flTLength offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecPosition1 ) == 140, "CBaseToggle::m_vecPosition1 offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecPosition2 ) == 152, "CBaseToggle::m_vecPosition2 offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecAngle1 ) == 164, "CBaseToggle::m_vecAngle1 offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecAngle2 ) == 176, "CBaseToggle::m_vecAngle2 offset shifted" );
static_assert( offsetof( CBaseToggle, m_cTriggersLeft ) == 188, "CBaseToggle::m_cTriggersLeft offset shifted" );
static_assert( offsetof( CBaseToggle, m_flHeight ) == 192, "CBaseToggle::m_flHeight offset shifted" );
static_assert( offsetof( CBaseToggle, m_hActivator ) == 196, "CBaseToggle::m_hActivator offset shifted" );
static_assert( offsetof( CBaseToggle, m_pfnCallWhenMoveDone ) == 204, "CBaseToggle::m_pfnCallWhenMoveDone offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecFinalDest ) == 208, "CBaseToggle::m_vecFinalDest offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecFinalAngle ) == 220, "CBaseToggle::m_vecFinalAngle offset shifted" );

// --- CBaseWallCharger ---
static_assert( offsetof( CBaseWallCharger, m_flNextCharge ) == 240, "CBaseWallCharger::m_flNextCharge offset shifted" );
static_assert( offsetof( CBaseWallCharger, m_iReactivate ) == 244, "CBaseWallCharger::m_iReactivate offset shifted" );
static_assert( offsetof( CBaseWallCharger, m_iJuice ) == 248, "CBaseWallCharger::m_iJuice offset shifted" );
static_assert( offsetof( CBaseWallCharger, m_iOn ) == 252, "CBaseWallCharger::m_iOn offset shifted" );
static_assert( offsetof( CBaseWallCharger, m_flSoundTime ) == 256, "CBaseWallCharger::m_flSoundTime offset shifted" );
static_assert( sizeof( CWallHealth ) == sizeof( CBaseWallCharger ), "CWallHealth size diverges from CBaseWallCharger" );
static_assert( sizeof( CWallRecharge ) == sizeof( CBaseWallCharger ), "CWallRecharge size diverges from CBaseWallCharger" );

#elif defined( __GNUC__ )

// ============================================================================
// GCC 32-bit x86 (Itanium C++ ABI) Layout Assertions
// ============================================================================

// --- CBasePlayer ---
static_assert( offsetof( CBasePlayer, m_flFlashLightTime ) == 732, "CBasePlayer::m_flFlashLightTime offset shifted" );
static_assert( offsetof( CBasePlayer, m_iFlashBattery ) == 736, "CBasePlayer::m_iFlashBattery offset shifted" );
static_assert( offsetof( CBasePlayer, m_afButtonLast ) == 740, "CBasePlayer::m_afButtonLast offset shifted" );
static_assert( offsetof( CBasePlayer, m_afButtonPressed ) == 744, "CBasePlayer::m_afButtonPressed offset shifted" );
static_assert( offsetof( CBasePlayer, m_afButtonReleased ) == 748, "CBasePlayer::m_afButtonReleased offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgItems ) == 768, "CBasePlayer::m_rgItems offset shifted" );
static_assert( offsetof( CBasePlayer, m_afPhysicsFlags ) == 796, "CBasePlayer::m_afPhysicsFlags offset shifted" );
static_assert( offsetof( CBasePlayer, m_flTimeStepSound ) == 804, "CBasePlayer::m_flTimeStepSound offset shifted" );
static_assert( offsetof( CBasePlayer, m_flTimeWeaponIdle ) == 808, "CBasePlayer::m_flTimeWeaponIdle offset shifted" );
static_assert( offsetof( CBasePlayer, m_flSwimTime ) == 812, "CBasePlayer::m_flSwimTime offset shifted" );
static_assert( offsetof( CBasePlayer, m_flDuckTime ) == 816, "CBasePlayer::m_flDuckTime offset shifted" );
static_assert( offsetof( CBasePlayer, m_flWallJumpTime ) == 820, "CBasePlayer::m_flWallJumpTime offset shifted" );
static_assert( offsetof( CBasePlayer, m_flSuitUpdate ) == 824, "CBasePlayer::m_flSuitUpdate offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgSuitPlayList ) == 828, "CBasePlayer::m_rgSuitPlayList offset shifted" );
static_assert( offsetof( CBasePlayer, m_iSuitPlayNext ) == 844, "CBasePlayer::m_iSuitPlayNext offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgiSuitNoRepeat ) == 848, "CBasePlayer::m_rgiSuitNoRepeat offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgflSuitNoRepeatTime ) == 976, "CBasePlayer::m_rgflSuitNoRepeatTime offset shifted" );
static_assert( offsetof( CBasePlayer, m_lastDamageAmount ) == 1104, "CBasePlayer::m_lastDamageAmount offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgpPlayerItems ) == 1224, "CBasePlayer::m_rgpPlayerItems offset shifted" );
static_assert( offsetof( CBasePlayer, m_pActiveItem ) == 1248, "CBasePlayer::m_pActiveItem offset shifted" );
static_assert( offsetof( CBasePlayer, m_pLastItem ) == 1256, "CBasePlayer::m_pLastItem offset shifted" );
static_assert( offsetof( CBasePlayer, m_rgAmmo ) == 1260, "CBasePlayer::m_rgAmmo offset shifted" );
static_assert( offsetof( CBasePlayer, m_idrowndmg ) == 1144, "CBasePlayer::m_idrowndmg offset shifted" );
static_assert( offsetof( CBasePlayer, m_idrownrestored ) == 1148, "CBasePlayer::m_idrownrestored offset shifted" );
static_assert( offsetof( CBasePlayer, m_tSneaking ) == 1192, "CBasePlayer::m_tSneaking offset shifted" );
static_assert( offsetof( CBasePlayer, m_iTrain ) == 1164, "CBasePlayer::m_iTrain offset shifted" );
static_assert( offsetof( CBasePlayer, m_bitsHUDDamage ) == 1152, "CBasePlayer::m_bitsHUDDamage offset shifted" );
static_assert( offsetof( CBasePlayer, m_flFallVelocity ) == 764, "CBasePlayer::m_flFallVelocity offset shifted" );
static_assert( offsetof( CBasePlayer, m_iTargetVolume ) == 712, "CBasePlayer::m_iTargetVolume offset shifted" );
static_assert( offsetof( CBasePlayer, m_iWeaponVolume ) == 716, "CBasePlayer::m_iWeaponVolume offset shifted" );
static_assert( offsetof( CBasePlayer, m_iExtraSoundTypes ) == 720, "CBasePlayer::m_iExtraSoundTypes offset shifted" );
static_assert( offsetof( CBasePlayer, m_iWeaponFlash ) == 724, "CBasePlayer::m_iWeaponFlash offset shifted" );
static_assert( offsetof( CBasePlayer, m_fLongJump ) == 1188, "CBasePlayer::m_fLongJump offset shifted" );
static_assert( offsetof( CBasePlayer, m_fInitHUD ) == 1156, "CBasePlayer::m_fInitHUD offset shifted" );
static_assert( offsetof( CBasePlayer, m_tbdPrev ) == 1108, "CBasePlayer::m_tbdPrev offset shifted" );
static_assert( offsetof( CBasePlayer, m_pTank ) == 1172, "CBasePlayer::m_pTank offset shifted" );
static_assert( offsetof( CBasePlayer, m_iHideHUD ) == 1208, "CBasePlayer::m_iHideHUD offset shifted" );
static_assert( offsetof( CBasePlayer, m_iFOV ) == 1216, "CBasePlayer::m_iFOV offset shifted" );

// --- CBasePlayerItem ---
static_assert( offsetof( CBasePlayerItem, m_pPlayer ) == 128, "CBasePlayerItem::m_pPlayer offset shifted" );
static_assert( offsetof( CBasePlayerItem, m_pNext ) == 132, "CBasePlayerItem::m_pNext offset shifted" );
static_assert( offsetof( CBasePlayerItem, m_iId ) == 136, "CBasePlayerItem::m_iId offset shifted" );

// --- CBasePlayerWeapon ---
static_assert( offsetof( CBasePlayerWeapon, m_flNextPrimaryAttack ) == 156, "CBasePlayerWeapon::m_flNextPrimaryAttack offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_flNextSecondaryAttack ) == 160, "CBasePlayerWeapon::m_flNextSecondaryAttack offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_flTimeWeaponIdle ) == 164, "CBasePlayerWeapon::m_flTimeWeaponIdle offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_iPrimaryAmmoType ) == 168, "CBasePlayerWeapon::m_iPrimaryAmmoType offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_iSecondaryAmmoType ) == 172, "CBasePlayerWeapon::m_iSecondaryAmmoType offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_iClip ) == 176, "CBasePlayerWeapon::m_iClip offset shifted" );
static_assert( offsetof( CBasePlayerWeapon, m_iDefaultAmmo ) == 192, "CBasePlayerWeapon::m_iDefaultAmmo offset shifted" );

// --- Weapon Subclasses ---
static_assert( offsetof( CGauss, m_fInAttack ) == 92, "CGauss::m_fInAttack offset shifted" );
static_assert( offsetof( CGauss, m_fPrimaryFire ) == 220, "CGauss::m_fPrimaryFire offset shifted" );
static_assert( offsetof( CEgon, m_fireState ) == 96, "CEgon::m_fireState offset shifted" );
static_assert( offsetof( CEgon, m_flAmmoUseTime ) == 204, "CEgon::m_flAmmoUseTime offset shifted" );
static_assert( offsetof( CRpg, m_fSpotActive ) == 208, "CRpg::m_fSpotActive offset shifted" );
static_assert( offsetof( CRpg, m_cActiveRockets ) == 212, "CRpg::m_cActiveRockets offset shifted" );
static_assert( offsetof( CSatchel, m_chargeReady ) == 204, "CSatchel::m_chargeReady offset shifted" );
static_assert( offsetof( CShotgun, m_flNextReload ) == 208, "CShotgun::m_flNextReload offset shifted" );
static_assert( offsetof( CShotgun, m_fInSpecialReload ) == 152, "CShotgun::m_fInSpecialReload offset shifted" );
static_assert( offsetof( CShotgun, m_flPumpTime ) == 148, "CShotgun::m_flPumpTime offset shifted" );
static_assert( offsetof( CWeaponBox, m_rgAmmo ) == 252, "CWeaponBox::m_rgAmmo offset shifted" );
static_assert( offsetof( CWeaponBox, m_rgiszAmmo ) == 124, "CWeaponBox::m_rgiszAmmo offset shifted" );
static_assert( offsetof( CWeaponBox, m_rgpPlayerItems ) == 100, "CWeaponBox::m_rgpPlayerItems offset shifted" );
static_assert( offsetof( CWeaponBox, m_cAmmoTypes ) == 380, "CWeaponBox::m_cAmmoTypes offset shifted" );

// --- Weapon Projectiles ---
static_assert( offsetof( CHornet, m_flStopAttack ) == 684, "CHornet::m_flStopAttack offset shifted" );
static_assert( offsetof( CHornet, m_iHornetType ) == 688, "CHornet::m_iHornetType offset shifted" );
static_assert( offsetof( CHornet, m_flFlySpeed ) == 692, "CHornet::m_flFlySpeed offset shifted" );
static_assert( offsetof( CRpgRocket, m_flIgniteTime ) == 692, "CRpgRocket::m_flIgniteTime offset shifted" );
static_assert( offsetof( CRpgRocket, m_hLauncher ) == 696, "CRpgRocket::m_hLauncher offset shifted" );

// --- Entity Base Classes ---
static_assert( offsetof( CBaseEntity, m_pGoalEnt ) == 8, "CBaseEntity::m_pGoalEnt offset shifted" );
static_assert( offsetof( CBaseEntity, m_pfnThink ) == 16, "CBaseEntity::m_pfnThink offset shifted" );
static_assert( offsetof( CBaseEntity, m_pfnTouch ) == 24, "CBaseEntity::m_pfnTouch offset shifted" );
static_assert( offsetof( CBaseEntity, m_pfnUse ) == 32, "CBaseEntity::m_pfnUse offset shifted" );
static_assert( offsetof( CBaseEntity, m_pfnBlocked ) == 40, "CBaseEntity::m_pfnBlocked offset shifted" );
static_assert( offsetof( CBaseDelay, m_flDelay ) == 100, "CBaseDelay::m_flDelay offset shifted" );
static_assert( offsetof( CBaseDelay, m_iszKillTarget ) == 104, "CBaseDelay::m_iszKillTarget offset shifted" );
static_assert( offsetof( CBaseAnimating, m_flFrameRate ) == 108, "CBaseAnimating::m_flFrameRate offset shifted" );
static_assert( offsetof( CBaseAnimating, m_flGroundSpeed ) == 112, "CBaseAnimating::m_flGroundSpeed offset shifted" );
static_assert( offsetof( CBaseAnimating, m_flLastEventCheck ) == 116, "CBaseAnimating::m_flLastEventCheck offset shifted" );
static_assert( offsetof( CBaseAnimating, m_fSequenceFinished ) == 120, "CBaseAnimating::m_fSequenceFinished offset shifted" );
static_assert( offsetof( CBaseAnimating, m_fSequenceLoops ) == 124, "CBaseAnimating::m_fSequenceLoops offset shifted" );
static_assert( offsetof( CBaseToggle, m_toggle_state ) == 128, "CBaseToggle::m_toggle_state offset shifted" );
static_assert( offsetof( CBaseToggle, m_flActivateFinished ) == 132, "CBaseToggle::m_flActivateFinished offset shifted" );
static_assert( offsetof( CBaseToggle, m_flMoveDistance ) == 136, "CBaseToggle::m_flMoveDistance offset shifted" );
static_assert( offsetof( CBaseToggle, m_flWait ) == 140, "CBaseToggle::m_flWait offset shifted" );
static_assert( offsetof( CBaseToggle, m_flLip ) == 144, "CBaseToggle::m_flLip offset shifted" );
static_assert( offsetof( CBaseToggle, m_flTWidth ) == 148, "CBaseToggle::m_flTWidth offset shifted" );
static_assert( offsetof( CBaseToggle, m_flTLength ) == 152, "CBaseToggle::m_flTLength offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecPosition1 ) == 156, "CBaseToggle::m_vecPosition1 offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecPosition2 ) == 168, "CBaseToggle::m_vecPosition2 offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecAngle1 ) == 180, "CBaseToggle::m_vecAngle1 offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecAngle2 ) == 192, "CBaseToggle::m_vecAngle2 offset shifted" );
static_assert( offsetof( CBaseToggle, m_cTriggersLeft ) == 204, "CBaseToggle::m_cTriggersLeft offset shifted" );
static_assert( offsetof( CBaseToggle, m_flHeight ) == 208, "CBaseToggle::m_flHeight offset shifted" );
static_assert( offsetof( CBaseToggle, m_hActivator ) == 212, "CBaseToggle::m_hActivator offset shifted" );
static_assert( offsetof( CBaseToggle, m_pfnCallWhenMoveDone ) == 220, "CBaseToggle::m_pfnCallWhenMoveDone offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecFinalDest ) == 228, "CBaseToggle::m_vecFinalDest offset shifted" );
static_assert( offsetof( CBaseToggle, m_vecFinalAngle ) == 240, "CBaseToggle::m_vecFinalAngle offset shifted" );

// --- CBaseWallCharger ---
static_assert( offsetof( CBaseWallCharger, m_flNextCharge ) == 260, "CBaseWallCharger::m_flNextCharge offset shifted" );
static_assert( offsetof( CBaseWallCharger, m_iReactivate ) == 264, "CBaseWallCharger::m_iReactivate offset shifted" );
static_assert( offsetof( CBaseWallCharger, m_iJuice ) == 268, "CBaseWallCharger::m_iJuice offset shifted" );
static_assert( offsetof( CBaseWallCharger, m_iOn ) == 272, "CBaseWallCharger::m_iOn offset shifted" );
static_assert( offsetof( CBaseWallCharger, m_flSoundTime ) == 276, "CBaseWallCharger::m_flSoundTime offset shifted" );
static_assert( sizeof( CWallHealth ) == sizeof( CBaseWallCharger ), "CWallHealth size diverges from CBaseWallCharger" );
static_assert( sizeof( CWallRecharge ) == sizeof( CBaseWallCharger ), "CWallRecharge size diverges from CBaseWallCharger" );

#endif

#include "external/catch2/catch_amalgamated.hpp"
#include "tests/mock_engine.h"

TEST_CASE( "Charger: CWallHealth save/restore canonical chunk identifier and fallback", "[charger][saverestore]" )
{
	ResetMockEngine();

	CWallHealth healthCharger;
	SAVERESTOREDATA data;
	memset( &data, 0, sizeof( data ) );
	CSave saveHelper( &data );
	CRestore restoreHelper( &data );

	// Canonical rule: CWallHealth::Save must serialize under chunk identifier "CWallHealth"
	healthCharger.Save( saveHelper );
	CHECK( g_mockLastSaveChunk == "CWallHealth" );

	// Canonical rule: CWallHealth::Restore must deserialize from "CWallHealth"
	g_mockRestoreAvailableChunk = "CWallHealth";
	int ok = healthCharger.Restore( restoreHelper );
	CHECK( ok == 1 );
	CHECK( g_mockLastRestoreChunk == "CWallHealth" );

	// Fallback rule: CWallHealth::Restore must also accept "CBaseWallCharger" fallback
	g_mockRestoreAvailableChunk = "CBaseWallCharger";
	ok = healthCharger.Restore( restoreHelper );
	CHECK( ok == 1 );
}

TEST_CASE( "Charger: CWallRecharge save/restore canonical chunk identifier (CRecharge) and fallback", "[charger][saverestore]" )
{
	ResetMockEngine();

	CWallRecharge suitCharger;
	SAVERESTOREDATA data;
	memset( &data, 0, sizeof( data ) );
	CSave saveHelper( &data );
	CRestore restoreHelper( &data );

	// Canonical rule: CWallRecharge::Save must serialize under canonical chunk identifier "CRecharge"
	suitCharger.Save( saveHelper );
	CHECK( g_mockLastSaveChunk == "CRecharge" );

	// Canonical rule: CWallRecharge::Restore must deserialize from "CRecharge"
	g_mockRestoreAvailableChunk = "CRecharge";
	int ok = suitCharger.Restore( restoreHelper );
	CHECK( ok == 1 );
	CHECK( g_mockLastRestoreChunk == "CRecharge" );

	// Fallback rule: CWallRecharge::Restore must also accept "CBaseWallCharger" fallback
	g_mockRestoreAvailableChunk = "CBaseWallCharger";
	ok = suitCharger.Restore( restoreHelper );
	CHECK( ok == 1 );
}

TEST_CASE( "Charger: CWallHealth save table contains canonical 5 fields including m_flSoundTime (#145)", "[charger][saverestore]" )
{
	ResetMockEngine();

	CWallHealth healthCharger;
	SAVERESTOREDATA data;
	memset( &data, 0, sizeof( data ) );
	CSave saveHelper( &data );

	healthCharger.Save( saveHelper );

	// Canonical Valve GoldSrc healthkit.cpp defines 5 fields for CWallHealth::m_SaveData
	// 0: m_flNextCharge (FIELD_TIME)
	// 1: m_iReactivate (FIELD_INTEGER)
	// 2: m_iJuice (FIELD_INTEGER)
	// 3: m_iOn (FIELD_INTEGER)
	// 4: m_flSoundTime (FIELD_TIME)
	REQUIRE( g_mockLastSaveFieldCount == 5 );
	REQUIRE( g_mockLastSaveFields != nullptr );
	CHECK( strcmp( g_mockLastSaveFields[0].fieldName, "m_flNextCharge" ) == 0 );
	CHECK( g_mockLastSaveFields[0].fieldType == FIELD_TIME );
	CHECK( strcmp( g_mockLastSaveFields[1].fieldName, "m_iReactivate" ) == 0 );
	CHECK( g_mockLastSaveFields[1].fieldType == FIELD_INTEGER );
	CHECK( strcmp( g_mockLastSaveFields[2].fieldName, "m_iJuice" ) == 0 );
	CHECK( g_mockLastSaveFields[2].fieldType == FIELD_INTEGER );
	CHECK( strcmp( g_mockLastSaveFields[3].fieldName, "m_iOn" ) == 0 );
	CHECK( g_mockLastSaveFields[3].fieldType == FIELD_INTEGER );
	CHECK( strcmp( g_mockLastSaveFields[4].fieldName, "m_flSoundTime" ) == 0 );
	CHECK( g_mockLastSaveFields[4].fieldType == FIELD_TIME );
}
