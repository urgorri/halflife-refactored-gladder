/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Network User Messages Definition (SPEC §9.1, §9.2, §12, GitHub Issues #14, #15, #16, #25)
 *
 ****/

#pragma once

#ifndef GLADDER_USERMSG_H
#define GLADDER_USERMSG_H

#ifdef VALVE_DLL
#include "core/user_message_registry.h"

extern int gmsgGladderWave;
extern int gmsgGladderTelemetry;
extern int gmsgGladderCombo;
extern int gmsgGladderNotify;
extern int gmsgGladderSummary;
extern int gmsgGladderLambda;
extern int gmsgGladderGridDebug;
#endif

// Message names (must be <= 11 characters in GoldSrc engine REG_USER_MSG)
#define GLADDER_MSG_WAVE_NAME "GladWave"
#define GLADDER_MSG_TELEMETRY_NAME "GladTelem"
#define GLADDER_MSG_COMBO_NAME "GladCombo"
#define GLADDER_MSG_NOTIFY_NAME "GladNotify"
#define GLADDER_MSG_SUMMARY_NAME "GladSumm"
#define GLADDER_MSG_LAMBDA_NAME "GladLambda"
#define GLADDER_MSG_GRID_DEBUG_NAME "GladGrid"

// Fixed sizes for network messages (-1 means variable, or exact byte count)
#define GLADDER_MSG_GRID_DEBUG_SIZE -1

// Fixed sizes for network messages (-1 means variable, or exact byte count)
// GladderWave wire format (7 bytes total):
//   WRITE_BYTE  iState                  (1 byte)
//   WRITE_SHORT iWaveNumber             (2 bytes)
//   WRITE_COORD flSessionTimeRemaining  (2 bytes — GoldSrc WRITE_COORD is a scaled short)
//   WRITE_COORD flLapTime               (2 bytes)
#define GLADDER_MSG_WAVE_SIZE 7

// GladderTelemetry wire format (12 bytes total):
//   WRITE_COORD flFastestLap    (2 bytes)
//   WRITE_COORD flSlowestLap    (2 bytes)
//   WRITE_COORD flAverageLap    (2 bytes)
//   WRITE_SHORT iCompletedLaps  (2 bytes)
//   WRITE_SHORT iFrags          (2 bytes)
//   WRITE_SHORT iCollectibles   (2 bytes)
#define GLADDER_MSG_TELEMETRY_SIZE 12

// GladderCombo wire format (2 bytes total):
//   WRITE_BYTE  iComboMultiplier (1 byte: 1..255)
//   WRITE_BYTE  iDecayFraction255(1 byte: 0..255 normalized decay fraction)
#define GLADDER_MSG_COMBO_SIZE 2

// GladderWaveNotify wire format (3 bytes total):
//   WRITE_BYTE  iType            (1 byte: 0 = STARTED, 1 = COMPLETED)
//   WRITE_BYTE  iWaveNumber      (1 byte: 1..255)
//   WRITE_BYTE  iMutatorType     (1 byte: GladderMutatorType)
#define GLADDER_MSG_NOTIFY_SIZE 3

// GladderLambda wire format (6 bytes total, SPEC §7.3, Issue #92):
//   WRITE_LONG  iScoreBonus       (4 bytes)
//   WRITE_SHORT iCollectibleCount (2 bytes)
#define GLADDER_MSG_LAMBDA_SIZE 6

// GladderSummary wire format (Variable size: -1):
//   WRITE_BYTE  bSurvived        (1 byte: 0 = KIA, 1 = Survived)
//   WRITE_SHORT iWavesCompleted  (2 bytes)
//   WRITE_LONG  iFinalScore      (4 bytes)
//   WRITE_CHAR  cRankGrade       (1 byte: 'S', 'A', 'B', 'C', 'D', 'F')
//   WRITE_SHORT iMaxComboStreak  (2 bytes)
//   WRITE_SHORT iTotalFrags      (2 bytes)
//   WRITE_SHORT iCollectibles    (2 bytes)
//   WRITE_COORD flTotalMatchTime (2 bytes)
//   WRITE_COORD flAverageLapTime (2 bytes)
//   WRITE_COORD flFastestLapTime (2 bytes)
//   WRITE_COORD flSlowestLapTime (2 bytes)
//   WRITE_COORD flTotalDamage    (2 bytes)
//   WRITE_LONG  iBaseCombatPoints(4 bytes)
//   WRITE_LONG  iComboBonusPoints(4 bytes)
//   WRITE_LONG  iWaveBonusPoints (4 bytes)
//   WRITE_LONG  iLambdaBonusPoints(4 bytes)
//   WRITE_LONG  iSurvivalBonusPoints(4 bytes)
//   WRITE_LONG  iDamagePenalty   (4 bytes)
//   WRITE_BYTE  iSpeciesCount    (1 byte: N)
//   Repeated N times:
//     WRITE_STRING szSpeciesName (null-terminated string)
//     WRITE_SHORT  iKills        (2 bytes)
#define GLADDER_MSG_SUMMARY_SIZE -1

#endif // GLADDER_USERMSG_H
