#include "extdll.h"
#include "util.h"
#include "util_debug.h"
#include "cbase.h"

#ifdef DEBUG
edict_t *DBG_EntOfVars( const entvars_t *pev )
{
	if ( pev->pContainingEntity != NULL )
		return pev->pContainingEntity;
	ALERT( at_console, "entvars_t pContainingEntity is NULL, calling into engine" );
	edict_t *pent = ( *g_engfuncs.pfnFindEntityByVars )( (entvars_t *)pev );
	if ( pent == NULL )
		ALERT( at_console, "DAMN!  Even the engine couldn't FindEntityByVars!" );
	( (entvars_t *)pev )->pContainingEntity = pent;
	return pent;
}
#endif // DEBUG

#ifdef DEBUG
void DBG_AssertFunction(
    BOOL fExpr,
    const char *szExpr,
    const char *szFile,
    int szLine,
    const char *szMessage )
{
	if ( fExpr )
		return;
	char szOut[512];
	if ( szMessage != NULL )
		snprintf( szOut, sizeof( szOut ), "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage );
	else
		snprintf( szOut, sizeof( szOut ), "ASSERT FAILED:\n %s \n(%s@%d)", szExpr, szFile, szLine );
	ALERT( at_console, szOut );
}
#endif // DEBUG

// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf( char *fmt, ... )
{
	va_list argptr;
	static char string[1024];

	va_start( argptr, fmt );
	vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );
	string[sizeof( string ) - 1] = '\0';

	// Print to server console
	ALERT( at_logged, "%s", string );
}
