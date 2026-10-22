/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Network User Messages Registration (SPEC §9.1, §9.2, §12, GitHub Issues #14, #15, #16, #25)
 *
 ****/

#include "gladder_usermsg.h"

#ifdef VALVE_DLL
int gmsgGladderWave = 0;
int gmsgGladderTelemetry = 0;
int gmsgGladderCombo = 0;
int gmsgGladderNotify = 0;
int gmsgGladderSummary = 0;
int gmsgGladderLambda = 0;
int gmsgGladderGridDebug = 0;
int gmsgGladderClear = 0;

REGISTER_USER_MSG( GladWave, GLADDER_MSG_WAVE_SIZE, &gmsgGladderWave );
REGISTER_USER_MSG( GladTelem, GLADDER_MSG_TELEMETRY_SIZE, &gmsgGladderTelemetry );
REGISTER_USER_MSG( GladCombo, GLADDER_MSG_COMBO_SIZE, &gmsgGladderCombo );
REGISTER_USER_MSG( GladNotify, GLADDER_MSG_NOTIFY_SIZE, &gmsgGladderNotify );
REGISTER_USER_MSG( GladSumm, GLADDER_MSG_SUMMARY_SIZE, &gmsgGladderSummary );
REGISTER_USER_MSG( GladLambda, GLADDER_MSG_LAMBDA_SIZE, &gmsgGladderLambda );
REGISTER_USER_MSG( GladGrid, GLADDER_MSG_GRID_DEBUG_SIZE, &gmsgGladderGridDebug );
REGISTER_USER_MSG( GladClear, GLADDER_MSG_CLEAR_SIZE, &gmsgGladderClear );
#endif
