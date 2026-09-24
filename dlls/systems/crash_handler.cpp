/***
 *
 *	Half-Life Refactored - Crash Handler and Tracing Subsystem
 *
 *	Implements real-time entity dispatch tracing, circular ring buffer,
 *	Windows Vectored Exception Handling (VEH), callstack backtrace,
 *	and post-mortem MiniDump generation.
 *
 ****/

#include "systems/crash_handler.h"
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/game.h"

#include <time.h>
#include <stdarg.h>

#if defined(_WIN32)
#include <dbghelp.h>
#if defined(_MSC_VER)
#pragma comment(lib, "dbghelp.lib")
#endif
#endif

// Global trace cvars
cvar_t trace_crash     = { "trace_crash", "0" };
cvar_t developer_trace = { "developer_trace", "0" };

// Global singleton instance
CrashHandler g_CrashHandler;

#if defined(_WIN32)
static volatile LONG s_inCrashFilter = 0;

static LONG WINAPI GoldSrcCrashFilter( PEXCEPTION_POINTERS pExceptionInfo )
{
	if ( !pExceptionInfo || !pExceptionInfo->ExceptionRecord )
		return EXCEPTION_CONTINUE_SEARCH;

	DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;

	// Trap fatal memory, illegal instruction, divide by zero, and corruption exceptions
	if ( code != EXCEPTION_ACCESS_VIOLATION &&
	     code != EXCEPTION_ILLEGAL_INSTRUCTION &&
	     code != EXCEPTION_STACK_OVERFLOW &&
	     code != EXCEPTION_DATATYPE_MISALIGNMENT &&
	     code != EXCEPTION_IN_PAGE_ERROR &&
	     code != EXCEPTION_ARRAY_BOUNDS_EXCEEDED &&
	     code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
	     code != EXCEPTION_FLT_DIVIDE_BY_ZERO &&
	     code != (DWORD)0xC0000409 && /* STATUS_STACK_BUFFER_OVERRUN */
	     code != (DWORD)0xC0000374 && /* STATUS_HEAP_CORRUPTION */
	     code != (DWORD)0x40000015 )  /* STATUS_FATAL_APP_EXIT */
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// Recursion guard: prevent secondary faults from cascading
	if ( InterlockedCompareExchange( &s_inCrashFilter, 1, 0 ) != 0 )
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	g_CrashHandler.WriteCrashReport( pExceptionInfo );
	g_CrashHandler.WriteMiniDump( pExceptionInfo );

	// Handlers must return EXCEPTION_CONTINUE_SEARCH so downstream filters / WER continue
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

// Server command callback for engine command registration
static void CrashHandler_ServerCmd( void )
{
	g_CrashHandler.ExecuteCommand( CMD_ARGC(), NULL, NULL, 0 );
}

static const char *SafeGetString( string_t str )
{
	if ( !str )
		return "";

	if ( g_engfuncs.pfnSzFromIndex )
	{
		const char *psz = g_engfuncs.pfnSzFromIndex( str );
		if ( psz && psz[0] )
			return psz;
	}

	if ( gpGlobals && gpGlobals->pStringBase )
	{
		return STRING( str );
	}

	return "";
}

CrashHandler::CrashHandler()
    : m_nTotalEvents( 0 ),
      m_bInitialized( false ),
      m_pTraceFile( NULL )
{
	memset( m_ringBuffer, 0, sizeof( m_ringBuffer ) );
	memset( m_szTraceFilename, 0, sizeof( m_szTraceFilename ) );

#if defined(_WIN32)
	m_pVehHandle = NULL;
	InitializeCriticalSection( &m_csLock );
#endif
}

CrashHandler::~CrashHandler()
{
	Shutdown();
#if defined(_WIN32)
	DeleteCriticalSection( &m_csLock );
#endif
}

void CrashHandler::Init( void )
{
	if ( m_bInitialized )
		return;

#if defined(_WIN32)
	if ( !m_pVehHandle )
	{
		m_pVehHandle = AddVectoredExceptionHandler( 1, GoldSrcCrashFilter );
	}
#endif

	// Register console command via engine if function pointer is available
	if ( g_engfuncs.pfnAddServerCommand )
	{
		g_engfuncs.pfnAddServerCommand( (char *)"trace_log", CrashHandler_ServerCmd );
	}

	m_bInitialized = true;
}

void CrashHandler::Shutdown( void )
{
	StopTraceLog();

#if defined(_WIN32)
	if ( m_pVehHandle )
	{
		RemoveVectoredExceptionHandler( m_pVehHandle );
		m_pVehHandle = NULL;
	}
#endif

	m_bInitialized = false;
}

bool CrashHandler::StartTraceLog( const char *pszFilename )
{
	StopTraceLog();

	const char *pszTarget = ( pszFilename && pszFilename[0] ) ? pszFilename : "trace_execution.log";

	m_pTraceFile = fopen( pszTarget, "w" );
	if ( !m_pTraceFile )
		return false;

	strncpy( m_szTraceFilename, pszTarget, sizeof( m_szTraceFilename ) - 1 );
	m_szTraceFilename[sizeof( m_szTraceFilename ) - 1] = '\0';

	time_t now = time( NULL );
	char timeBuf[64];
	strftime( timeBuf, sizeof( timeBuf ), "%Y-%m-%d %H:%M:%S", localtime( &now ) );

	fprintf( m_pTraceFile, "================================================================================\n" );
	fprintf( m_pTraceFile, "Half-Life Refactored - Execution Trace Log Started at %s\n", timeBuf );
	fprintf( m_pTraceFile, "Map: %s | Engine Time: %.3fs\n",
	         ( gpGlobals && gpGlobals->mapname ) ? SafeGetString( gpGlobals->mapname ) : "unknown",
	         gpGlobals ? gpGlobals->time : 0.0f );
	fprintf( m_pTraceFile, "================================================================================\n" );
	fflush( m_pTraceFile );

	return true;
}

void CrashHandler::StopTraceLog( void )
{
	if ( m_pTraceFile )
	{
		fprintf( m_pTraceFile, "================================================================================\n" );
		fprintf( m_pTraceFile, "Execution Trace Log Closed (Total Events: %ld)\n", m_nTotalEvents );
		fprintf( m_pTraceFile, "================================================================================\n" );
		fflush( m_pTraceFile );
		fclose( m_pTraceFile );
		m_pTraceFile = NULL;
	}
	m_szTraceFilename[0] = '\0';
}

void CrashHandler::FlushTraceLog( void )
{
	if ( m_pTraceFile )
	{
		fflush( m_pTraceFile );
	}
}

int CrashHandler::GetActiveEventCount( void ) const
{
	long total = m_nTotalEvents;
	if ( total < 0 )
		return 0;
	return ( total > RING_BUFFER_SIZE ) ? RING_BUFFER_SIZE : (int)total;
}

bool CrashHandler::GetEvent( int indexFromNewest, TraceEvent *pOutEvent ) const
{
	if ( !pOutEvent || indexFromNewest < 0 )
		return false;

	long total = m_nTotalEvents;
	int active = ( total > RING_BUFFER_SIZE ) ? RING_BUFFER_SIZE : (int)total;

	if ( indexFromNewest >= active )
		return false;

	long targetIndex = total - 1 - indexFromNewest;
	int slot = (int)( (unsigned long)targetIndex % RING_BUFFER_SIZE );

	*pOutEvent = m_ringBuffer[slot];
	return true;
}

void CrashHandler::Reset( void )
{
	m_nTotalEvents = 0;
	memset( m_ringBuffer, 0, sizeof( m_ringBuffer ) );
}

bool CrashHandler::ExtractEntityInfo( edict_t *pent, int &outIndex, char *outClassname, size_t maxClassLen,
                                      char *outTargetname, size_t maxTargetLen, float outOrigin[3] )
{
	outIndex = -1;
	if ( outClassname && maxClassLen > 0 ) outClassname[0] = '\0';
	if ( outTargetname && maxTargetLen > 0 ) outTargetname[0] = '\0';
	if ( outOrigin ) { outOrigin[0] = 0.0f; outOrigin[1] = 0.0f; outOrigin[2] = 0.0f; }

	if ( !pent )
		return false;

	if ( g_engfuncs.pfnIndexOfEdict )
	{
		outIndex = g_engfuncs.pfnIndexOfEdict( pent );
	}

	if ( pent->v.classname && outClassname && maxClassLen > 0 )
	{
		const char *pszClass = SafeGetString( pent->v.classname );
		if ( pszClass && pszClass[0] )
		{
			strncpy( outClassname, pszClass, maxClassLen - 1 );
			outClassname[maxClassLen - 1] = '\0';
		}
	}

	if ( pent->v.targetname && outTargetname && maxTargetLen > 0 )
	{
		const char *pszTarget = SafeGetString( pent->v.targetname );
		if ( pszTarget && pszTarget[0] )
		{
			strncpy( outTargetname, pszTarget, maxTargetLen - 1 );
			outTargetname[maxTargetLen - 1] = '\0';
		}
	}

	if ( outOrigin )
	{
		outOrigin[0] = pent->v.origin.x;
		outOrigin[1] = pent->v.origin.y;
		outOrigin[2] = pent->v.origin.z;
	}

	return true;
}

void CrashHandler::RecordEvent( const TraceEvent &event )
{
	long idx = 0;
#ifdef _WIN32
	idx = InterlockedIncrement( &m_nTotalEvents ) - 1;
#else
	idx = __sync_fetch_and_add( &m_nTotalEvents, 1 );
#endif

	int slot = (int)( (unsigned long)idx % RING_BUFFER_SIZE );
	m_ringBuffer[slot] = event;

	// Check if live file logging should be active
	bool bWriteToFile = ( m_pTraceFile != NULL );
	if ( !bWriteToFile && m_bInitialized )
	{
		if ( ( trace_crash.value > 0.0f ) || ( developer_trace.value > 0.0f ) )
		{
			StartTraceLog( "trace_execution.log" );
			bWriteToFile = ( m_pTraceFile != NULL );
		}
	}

	if ( bWriteToFile && m_pTraceFile )
	{
		char line[512];
		FormatEventString( event, line, sizeof( line ) );
		fputs( line, m_pTraceFile );
		fputc( '\n', m_pTraceFile );
		fflush( m_pTraceFile ); // Immediate unbuffered flush for instant crash survival
	}
}

void CrashHandler::FormatEventString( const TraceEvent &ev, char *outBuffer, size_t bufferSize )
{
	if ( !outBuffer || bufferSize == 0 )
		return;

	switch ( ev.type )
	{
	case TRACE_EVENT_THINK:
		snprintf( outBuffer, bufferSize, "[%8.3fs] THINK        [%3d] %-20s (\"%s\") at (%6.1f, %6.1f, %6.1f) | nextthink=%.3fs",
		          ev.timestamp, ev.entindex, ev.classname, ev.targetname,
		          ev.origin[0], ev.origin[1], ev.origin[2], ev.value1 );
		break;

	case TRACE_EVENT_TOUCH:
		snprintf( outBuffer, bufferSize, "[%8.3fs] TOUCH        [%3d] %-20s (\"%s\") touched by [%3d] %s (\"%s\")",
		          ev.timestamp, ev.entindex, ev.classname, ev.targetname,
		          ev.target_entindex, ev.target_classname, ev.target_targetname );
		break;

	case TRACE_EVENT_USE:
		snprintf( outBuffer, bufferSize, "[%8.3fs] USE          [%3d] %-20s (\"%s\") used by [%3d] %s caller=[%3d] %s | useType=%d val=%.2f",
		          ev.timestamp, ev.entindex, ev.classname, ev.targetname,
		          ev.target_entindex, ev.target_classname, ev.caller_entindex, ev.caller_classname,
		          ev.use_type, ev.value1 );
		break;

	case TRACE_EVENT_BLOCKED:
		snprintf( outBuffer, bufferSize, "[%8.3fs] BLOCKED      [%3d] %-20s (\"%s\") blocked by [%3d] %s",
		          ev.timestamp, ev.entindex, ev.classname, ev.targetname,
		          ev.target_entindex, ev.target_classname );
		break;

	case TRACE_EVENT_FIRE_TARGETS:
		snprintf( outBuffer, bufferSize, "[%8.3fs] FIRE_TARGETS target=\"%s\" caller=[%3d] %s activator=[%3d] %s | useType=%d val=%.2f",
		          ev.timestamp, ev.targetname, ev.caller_entindex, ev.caller_classname,
		          ev.target_entindex, ev.target_classname, ev.use_type, ev.value1 );
		break;

	case TRACE_EVENT_SCHEDULE_CHANGE:
		snprintf( outBuffer, bufferSize, "[%8.3fs] SCHED_CHANGE [%3d] %-20s (\"%s\") -> schedule=\"%s\" (id=%d)",
		          ev.timestamp, ev.entindex, ev.classname, ev.targetname,
		          ev.details, ev.int_val );
		break;

	case TRACE_EVENT_ANIM_EVENT:
		snprintf( outBuffer, bufferSize, "[%8.3fs] ANIM_EVENT   [%3d] %-20s (\"%s\") -> event=%d options=\"%s\"",
		          ev.timestamp, ev.entindex, ev.classname, ev.targetname,
		          ev.int_val, ev.details );
		break;

	case TRACE_EVENT_CUSTOM:
		snprintf( outBuffer, bufferSize, "[%8.3fs] CUSTOM       %s", ev.timestamp, ev.details );
		break;

	default:
		snprintf( outBuffer, bufferSize, "[%8.3fs] EVENT        [%3d] %s", ev.timestamp, ev.entindex, ev.classname );
		break;
	}
}

void CrashHandler::LogThink( edict_t *pent, float flNextThink )
{
	TraceEvent ev;
	memset( &ev, 0, sizeof( ev ) );
	ev.timestamp = gpGlobals ? (double)gpGlobals->time : 0.0;
	ev.type = TRACE_EVENT_THINK;
	ExtractEntityInfo( pent, ev.entindex, ev.classname, sizeof( ev.classname ),
	                   ev.targetname, sizeof( ev.targetname ), ev.origin );
	ev.value1 = flNextThink;

	RecordEvent( ev );
}

void CrashHandler::LogTouch( edict_t *pentTouched, edict_t *pentOther )
{
	TraceEvent ev;
	memset( &ev, 0, sizeof( ev ) );
	ev.timestamp = gpGlobals ? (double)gpGlobals->time : 0.0;
	ev.type = TRACE_EVENT_TOUCH;
	ExtractEntityInfo( pentTouched, ev.entindex, ev.classname, sizeof( ev.classname ),
	                   ev.targetname, sizeof( ev.targetname ), ev.origin );
	ExtractEntityInfo( pentOther, ev.target_entindex, ev.target_classname, sizeof( ev.target_classname ),
	                   ev.target_targetname, sizeof( ev.target_targetname ), NULL );

	RecordEvent( ev );
}

void CrashHandler::LogUse( edict_t *pentUsed, edict_t *pentOther, edict_t *pentActivator, int useType, float value )
{
	TraceEvent ev;
	memset( &ev, 0, sizeof( ev ) );
	ev.timestamp = gpGlobals ? (double)gpGlobals->time : 0.0;
	ev.type = TRACE_EVENT_USE;
	ExtractEntityInfo( pentUsed, ev.entindex, ev.classname, sizeof( ev.classname ),
	                   ev.targetname, sizeof( ev.targetname ), ev.origin );
	ExtractEntityInfo( pentActivator, ev.target_entindex, ev.target_classname, sizeof( ev.target_classname ),
	                   ev.target_targetname, sizeof( ev.target_targetname ), NULL );
	ExtractEntityInfo( pentOther, ev.caller_entindex, ev.caller_classname, sizeof( ev.caller_classname ),
	                   NULL, 0, NULL );
	ev.use_type = useType;
	ev.value1 = value;

	RecordEvent( ev );
}

void CrashHandler::LogBlocked( edict_t *pentBlocked, edict_t *pentOther )
{
	TraceEvent ev;
	memset( &ev, 0, sizeof( ev ) );
	ev.timestamp = gpGlobals ? (double)gpGlobals->time : 0.0;
	ev.type = TRACE_EVENT_BLOCKED;
	ExtractEntityInfo( pentBlocked, ev.entindex, ev.classname, sizeof( ev.classname ),
	                   ev.targetname, sizeof( ev.targetname ), ev.origin );
	ExtractEntityInfo( pentOther, ev.target_entindex, ev.target_classname, sizeof( ev.target_classname ),
	                   ev.target_targetname, sizeof( ev.target_targetname ), NULL );

	RecordEvent( ev );
}

void CrashHandler::LogFireTargets( const char *pszTargetName, edict_t *pentCaller, edict_t *pentActivator, int useType, float value )
{
	TraceEvent ev;
	memset( &ev, 0, sizeof( ev ) );
	ev.timestamp = gpGlobals ? (double)gpGlobals->time : 0.0;
	ev.type = TRACE_EVENT_FIRE_TARGETS;

	if ( pszTargetName )
	{
		strncpy( ev.targetname, pszTargetName, sizeof( ev.targetname ) - 1 );
	}

	ExtractEntityInfo( pentCaller, ev.caller_entindex, ev.caller_classname, sizeof( ev.caller_classname ),
	                   NULL, 0, NULL );
	ExtractEntityInfo( pentActivator, ev.target_entindex, ev.target_classname, sizeof( ev.target_classname ),
	                   NULL, 0, NULL );
	ev.use_type = useType;
	ev.value1 = value;

	RecordEvent( ev );
}

void CrashHandler::LogScheduleChange( edict_t *pent, const char *pszScheduleName, int iScheduleId )
{
	TraceEvent ev;
	memset( &ev, 0, sizeof( ev ) );
	ev.timestamp = gpGlobals ? (double)gpGlobals->time : 0.0;
	ev.type = TRACE_EVENT_SCHEDULE_CHANGE;
	ExtractEntityInfo( pent, ev.entindex, ev.classname, sizeof( ev.classname ),
	                   ev.targetname, sizeof( ev.targetname ), ev.origin );

	if ( pszScheduleName )
	{
		strncpy( ev.details, pszScheduleName, sizeof( ev.details ) - 1 );
	}
	ev.int_val = iScheduleId;

	RecordEvent( ev );
}

void CrashHandler::LogAnimEvent( edict_t *pent, int iEventId, const char *pszOptions )
{
	TraceEvent ev;
	memset( &ev, 0, sizeof( ev ) );
	ev.timestamp = gpGlobals ? (double)gpGlobals->time : 0.0;
	ev.type = TRACE_EVENT_ANIM_EVENT;
	ExtractEntityInfo( pent, ev.entindex, ev.classname, sizeof( ev.classname ),
	                   ev.targetname, sizeof( ev.targetname ), ev.origin );

	ev.int_val = iEventId;
	if ( pszOptions )
	{
		strncpy( ev.details, pszOptions, sizeof( ev.details ) - 1 );
	}

	RecordEvent( ev );
}

void CrashHandler::LogCustom( const char *pszFmt, ... )
{
	TraceEvent ev;
	memset( &ev, 0, sizeof( ev ) );
	ev.timestamp = gpGlobals ? (double)gpGlobals->time : 0.0;
	ev.type = TRACE_EVENT_CUSTOM;

	va_list args;
	va_start( args, pszFmt );
	vsnprintf( ev.details, sizeof( ev.details ), pszFmt, args );
	va_end( args );

	RecordEvent( ev );
}

void CrashHandler::DumpRingBuffer( FILE *pOut, int maxEntries )
{
	if ( !pOut )
		return;

	int activeCount = GetActiveEventCount();
	if ( activeCount <= 0 )
	{
		fprintf( pOut, "(No events recorded in ring buffer)\n" );
		return;
	}

	int toDump = activeCount;
	if ( maxEntries > 0 && toDump > maxEntries )
		toDump = maxEntries;

	fprintf( pOut, "\nRecent Entity Dispatch Events (Last %d events, chronological order):\n", toDump );
	fprintf( pOut, "--------------------------------------------------------------------------------\n" );

	for ( int i = toDump - 1; i >= 0; i-- )
	{
		TraceEvent ev;
		if ( GetEvent( i, &ev ) )
		{
			char line[512];
			FormatEventString( ev, line, sizeof( line ) );
			fprintf( pOut, "%s\n", line );
		}
	}
	fprintf( pOut, "--------------------------------------------------------------------------------\n" );
	fflush( pOut );
}

bool CrashHandler::DumpRingBufferToFile( const char *pszFilename, int maxEntries )
{
	const char *pszTarget = ( pszFilename && pszFilename[0] ) ? pszFilename : "trace_dump.log";
	FILE *pFile = fopen( pszTarget, "w" );
	if ( !pFile )
		return false;

	time_t now = time( NULL );
	char timeBuf[64];
	strftime( timeBuf, sizeof( timeBuf ), "%Y-%m-%d %H:%M:%S", localtime( &now ) );

	fprintf( pFile, "================================================================================\n" );
	fprintf( pFile, "Half-Life Refactored - Trace Ring Buffer Snapshot (%s)\n", timeBuf );
	fprintf( pFile, "Map: %s | Engine Time: %.3fs\n",
	         ( gpGlobals && gpGlobals->mapname ) ? SafeGetString( gpGlobals->mapname ) : "unknown",
	         gpGlobals ? gpGlobals->time : 0.0f );
	fprintf( pFile, "================================================================================\n" );

	DumpRingBuffer( pFile, maxEntries );

	fclose( pFile );
	return true;
}

void CrashHandler::WriteCrashReport( void *pExceptionInfoPtr )
{
	FILE *pFile = fopen( "crash_dump.log", "w" );
	if ( !pFile )
		return;

	setvbuf( pFile, NULL, _IONBF, 0 );

	time_t now = time( NULL );
	char timeBuf[64];
	strftime( timeBuf, sizeof( timeBuf ), "%Y-%m-%d %H:%M:%S", localtime( &now ) );

	fprintf( pFile, "================================================================================\n" );
	fprintf( pFile, "                    HALF-LIFE REFACTORED CRASH REPORT\n" );
	fprintf( pFile, "================================================================================\n" );
	fprintf( pFile, "Timestamp:    %s\n", timeBuf );
	fprintf( pFile, "Map:          %s\n", ( gpGlobals && gpGlobals->mapname ) ? SafeGetString( gpGlobals->mapname ) : "unknown" );
	fprintf( pFile, "Engine Time:  %.3fs\n", gpGlobals ? gpGlobals->time : 0.0f );

#if defined(_WIN32)
	const char *pszCodeName = "UNKNOWN_EXCEPTION";
	DWORD code = 0;
	PEXCEPTION_POINTERS pExceptionInfo = (PEXCEPTION_POINTERS)pExceptionInfoPtr;
	if ( pExceptionInfo && pExceptionInfo->ExceptionRecord )
	{
		PEXCEPTION_RECORD pRec = pExceptionInfo->ExceptionRecord;
		code = pRec->ExceptionCode;
		switch ( code )
		{
		case EXCEPTION_ACCESS_VIOLATION: pszCodeName = "EXCEPTION_ACCESS_VIOLATION"; break;
		case EXCEPTION_ILLEGAL_INSTRUCTION: pszCodeName = "EXCEPTION_ILLEGAL_INSTRUCTION"; break;
		case EXCEPTION_STACK_OVERFLOW: pszCodeName = "EXCEPTION_STACK_OVERFLOW"; break;
		case EXCEPTION_DATATYPE_MISALIGNMENT: pszCodeName = "EXCEPTION_DATATYPE_MISALIGNMENT"; break;
		case EXCEPTION_IN_PAGE_ERROR: pszCodeName = "EXCEPTION_IN_PAGE_ERROR"; break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: pszCodeName = "EXCEPTION_ARRAY_BOUNDS_EXCEEDED"; break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO: pszCodeName = "EXCEPTION_INT_DIVIDE_BY_ZERO"; break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO: pszCodeName = "EXCEPTION_FLT_DIVIDE_BY_ZERO"; break;
		case (DWORD)0xC0000409: pszCodeName = "STATUS_STACK_BUFFER_OVERRUN"; break;
		case (DWORD)0xC0000374: pszCodeName = "STATUS_HEAP_CORRUPTION"; break;
		case (DWORD)0x40000015: pszCodeName = "STATUS_FATAL_APP_EXIT"; break;
		}

		fprintf( pFile, "Exception:    %s (0x%08lX)\n", pszCodeName, code );
		fprintf( pFile, "Fault IP:     0x%p\n", pRec->ExceptionAddress );

		// Check module of fault address
		HMODULE hFaultMod = NULL;
		if ( GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		                         (LPCSTR)pRec->ExceptionAddress, &hFaultMod ) && hFaultMod )
		{
			char fullPath[MAX_PATH];
			if ( GetModuleFileNameA( hFaultMod, fullPath, sizeof( fullPath ) ) )
			{
				const char *pSlash = strrchr( fullPath, '\\' );
				if ( !pSlash ) pSlash = strrchr( fullPath, '/' );
				const char *pszBaseName = pSlash ? pSlash + 1 : fullPath;
				uintptr_t rva = (uintptr_t)pRec->ExceptionAddress - (uintptr_t)hFaultMod;
				fprintf( pFile, "Fault Module: %s (Base: 0x%p, RVA: +0x%zX)\n", pszBaseName, (void *)hFaultMod, rva );
			}
		}

		if ( code == EXCEPTION_ACCESS_VIOLATION && pRec->NumberParameters >= 2 )
		{
			const char *pszAccessType = "Read";
			if ( pRec->ExceptionInformation[0] == 1 )
				pszAccessType = "Write";
			else if ( pRec->ExceptionInformation[0] == 8 )
				pszAccessType = "Execute (DEP violation)";

			fprintf( pFile, "Access Type:  %s violation at memory location 0x%p\n",
			         pszAccessType, (void *)pRec->ExceptionInformation[1] );
		}
	}

	if ( pExceptionInfo && pExceptionInfo->ContextRecord )
	{
		PCONTEXT pCtx = pExceptionInfo->ContextRecord;
		fprintf( pFile, "\nCPU Registers:\n" );
#if defined(_M_IX86)
		fprintf( pFile, "EAX=%08lX  EBX=%08lX  ECX=%08lX  EDX=%08lX\n", pCtx->Eax, pCtx->Ebx, pCtx->Ecx, pCtx->Edx );
		fprintf( pFile, "ESI=%08lX  EDI=%08lX  EBP=%08lX  ESP=%08lX\n", pCtx->Esi, pCtx->Edi, pCtx->Ebp, pCtx->Esp );
		fprintf( pFile, "EIP=%08lX  EFLAGS=%08lX\n", pCtx->Eip, pCtx->EFlags );
#elif defined(_M_X64) || defined(__x86_64__)
		fprintf( pFile, "RAX=%016llX  RBX=%016llX  RCX=%016llX  RDX=%016llX\n", pCtx->Rax, pCtx->Rbx, pCtx->Rcx, pCtx->Rdx );
		fprintf( pFile, "RSI=%016llX  RDI=%016llX  RBP=%016llX  RSP=%016llX\n", pCtx->Rsi, pCtx->Rdi, pCtx->Rbp, pCtx->Rsp );
		fprintf( pFile, "R8 =%016llX  R9 =%016llX  R10=%016llX  R11=%016llX\n", pCtx->R8, pCtx->R9, pCtx->R10, pCtx->R11 );
		fprintf( pFile, "R12=%016llX  R13=%016llX  R14=%016llX  R15=%016llX\n", pCtx->R12, pCtx->R13, pCtx->R14, pCtx->R15 );
		fprintf( pFile, "RIP=%016llX  EFLAGS=%08lX\n", pCtx->Rip, pCtx->EFlags );
#endif
	}

	fflush( pFile );

	// Echo crash header directly to active execution trace log
	if ( m_pTraceFile )
	{
		PEXCEPTION_RECORD pRec = ( pExceptionInfo && pExceptionInfo->ExceptionRecord ) ? pExceptionInfo->ExceptionRecord : NULL;
		fprintf( m_pTraceFile, "\n================================================================================\n" );
		fprintf( m_pTraceFile, "[CRASH DETECTED] %s (0x%08lX) at IP=0x%p\n",
		         pszCodeName,
		         pRec ? pRec->ExceptionCode : code,
		         pRec ? pRec->ExceptionAddress : 0 );
		fprintf( m_pTraceFile, "================================================================================\n" );
		fflush( m_pTraceFile );
	}

	// Stack backtrace
	fprintf( pFile, "\nStack Backtrace:\n" );
	fprintf( pFile, "--------------------------------------------------------------------------------\n" );
	fflush( pFile );

	__try
	{
		HANDLE hProcess = GetCurrentProcess();
		HANDLE hThread = GetCurrentThread();

		SymSetOptions( SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES );
		SymInitialize( hProcess, NULL, TRUE );

		CONTEXT ctxCopy;
		if ( pExceptionInfo && pExceptionInfo->ContextRecord )
			ctxCopy = *pExceptionInfo->ContextRecord;
		else
			RtlCaptureContext( &ctxCopy );

		STACKFRAME64 stackFrame;
		memset( &stackFrame, 0, sizeof( stackFrame ) );

		DWORD machineType = 0;
#if defined(_M_IX86)
		machineType = IMAGE_FILE_MACHINE_I386;
		stackFrame.AddrPC.Offset = ctxCopy.Eip;
		stackFrame.AddrPC.Mode = AddrModeFlat;
		stackFrame.AddrFrame.Offset = ctxCopy.Ebp;
		stackFrame.AddrFrame.Mode = AddrModeFlat;
		stackFrame.AddrStack.Offset = ctxCopy.Esp;
		stackFrame.AddrStack.Mode = AddrModeFlat;
#elif defined(_M_X64) || defined(__x86_64__)
		machineType = IMAGE_FILE_MACHINE_AMD64;
		stackFrame.AddrPC.Offset = ctxCopy.Rip;
		stackFrame.AddrPC.Mode = AddrModeFlat;
		stackFrame.AddrFrame.Offset = ctxCopy.Rbp;
		stackFrame.AddrFrame.Mode = AddrModeFlat;
		stackFrame.AddrStack.Offset = ctxCopy.Rsp;
		stackFrame.AddrStack.Mode = AddrModeFlat;
#endif

		int frameIndex = 0;
		while ( frameIndex < 64 )
		{
			BOOL bSuccess = FALSE;
			if ( machineType != 0 )
			{
				bSuccess = StackWalk64(
				    machineType,
				    hProcess,
				    hThread,
				    &stackFrame,
				    &ctxCopy,
				    NULL,
				    SymFunctionTableAccess64,
				    SymGetModuleBase64,
				    NULL
				);
			}

			if ( !bSuccess || stackFrame.AddrPC.Offset == 0 )
				break;

			DWORD64 addr = stackFrame.AddrPC.Offset;

			char szModName[MAX_PATH] = "unknown_module";
			DWORD64 modBase = SymGetModuleBase64( hProcess, addr );
			if ( modBase != 0 )
			{
				HMODULE hMod = (HMODULE)modBase;
				char fullPath[MAX_PATH];
				if ( GetModuleFileNameA( hMod, fullPath, sizeof( fullPath ) ) )
				{
					const char *pSlash = strrchr( fullPath, '\\' );
					if ( !pSlash ) pSlash = strrchr( fullPath, '/' );
					strncpy( szModName, pSlash ? pSlash + 1 : fullPath, sizeof( szModName ) - 1 );
				}
			}

			DWORD64 rva = ( modBase != 0 ) ? ( addr - modBase ) : addr;

			char symBuffer[sizeof( SYMBOL_INFO ) + 256];
			PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symBuffer;
			pSymbol->SizeOfStruct = sizeof( SYMBOL_INFO );
			pSymbol->MaxNameLen = 255;
			DWORD64 disp64 = 0;
			char szSymbolName[256] = "";
			if ( SymFromAddr( hProcess, addr, &disp64, pSymbol ) )
			{
				if ( disp64 != 0 )
					snprintf( szSymbolName, sizeof( szSymbolName ), "%s+0x%llX", pSymbol->Name, disp64 );
				else
					snprintf( szSymbolName, sizeof( szSymbolName ), "%s", pSymbol->Name );
			}

			IMAGEHLP_LINE64 line;
			line.SizeOfStruct = sizeof( IMAGEHLP_LINE64 );
			DWORD lineDisp = 0;
			char szLineInfo[MAX_PATH] = "";
			if ( SymGetLineFromAddr64( hProcess, addr, &lineDisp, &line ) )
			{
				const char *pSlash = strrchr( line.FileName, '\\' );
				if ( !pSlash ) pSlash = strrchr( line.FileName, '/' );
				snprintf( szLineInfo, sizeof( szLineInfo ), "[%s:%u]", pSlash ? pSlash + 1 : line.FileName, line.LineNumber );
			}

			fprintf( pFile, "#%02d  0x%p in %s+0x%llX", frameIndex, (void *)addr, szModName, rva );
			if ( szSymbolName[0] )
				fprintf( pFile, " (%s)", szSymbolName );
			if ( szLineInfo[0] )
				fprintf( pFile, " %s", szLineInfo );
			fprintf( pFile, "\n" );
			fflush( pFile );

			frameIndex++;
		}
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		fprintf( pFile, "<Stack walk failed due to secondary exception>\n" );
		fflush( pFile );
	}
#endif

	// Dump circular ring buffer events
	DumpRingBuffer( pFile, 50 );

	fprintf( pFile, "================================================================================\n" );
	fprintf( pFile, "End of Crash Report\n" );
	fprintf( pFile, "================================================================================\n" );

	fflush( pFile );
	fclose( pFile );

	// Also flush any open trace log
	FlushTraceLog();
}

void CrashHandler::WriteMiniDump( void *pExceptionInfoPtr )
{
#if defined(_WIN32)
	PEXCEPTION_POINTERS pExceptionInfo = (PEXCEPTION_POINTERS)pExceptionInfoPtr;

	HANDLE hFile = CreateFileA( "hl_crash.dmp", GENERIC_WRITE, FILE_SHARE_READ, NULL,
	                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		MINIDUMP_EXCEPTION_INFORMATION mei;
		mei.ThreadId = GetCurrentThreadId();
		mei.ExceptionPointers = pExceptionInfo;
		mei.ClientPointers = FALSE;

		MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(
		    MiniDumpNormal |
		    MiniDumpWithDataSegs |
		    MiniDumpWithIndirectlyReferencedMemory |
		    MiniDumpWithProcessThreadData );

		MiniDumpWriteDump(
		    GetCurrentProcess(),
		    GetCurrentProcessId(),
		    hFile,
		    dumpType,
		    pExceptionInfo ? &mei : NULL,
		    NULL,
		    NULL );

		CloseHandle( hFile );
	}
#endif
}

bool CrashHandler::ExecuteCommand( int argc, const char *argv[], char *outBuffer, size_t outBufferSize )
{
	// If argv is NULL, retrieve from engine CMD functions
	const char *pSubCmd = NULL;
	const char *pArg2 = NULL;

	if ( argv && argc >= 2 )
	{
		pSubCmd = argv[1];
		if ( argc >= 3 )
			pArg2 = argv[2];
	}
	else if ( CMD_ARGC() >= 2 )
	{
		pSubCmd = CMD_ARGV( 1 );
		if ( CMD_ARGC() >= 3 )
			pArg2 = CMD_ARGV( 2 );
	}

	char localBuf[512];
	char *pOut = outBuffer ? outBuffer : localBuf;
	size_t outLen = outBuffer ? outBufferSize : sizeof( localBuf );

	if ( !pSubCmd || !pSubCmd[0] || !stricmp( pSubCmd, "status" ) )
	{
		snprintf( pOut, outLen, "trace_log status: active=%s, file=\"%s\", total_events=%ld, ring_buffer_events=%d\n",
		          IsLiveTracingActive() ? "YES" : "NO",
		          IsLiveTracingActive() ? GetTraceLogFilename() : "(none)",
		          m_nTotalEvents,
		          GetActiveEventCount() );
	}
	else if ( !stricmp( pSubCmd, "start" ) )
	{
		const char *pszTarget = ( pArg2 && pArg2[0] ) ? pArg2 : "trace_execution.log";
		if ( StartTraceLog( pszTarget ) )
		{
			snprintf( pOut, outLen, "trace_log: live tracing STARTED to \"%s\"\n", pszTarget );
		}
		else
		{
			snprintf( pOut, outLen, "trace_log: ERROR failed to open trace file \"%s\"\n", pszTarget );
		}
	}
	else if ( !stricmp( pSubCmd, "stop" ) )
	{
		StopTraceLog();
		snprintf( pOut, outLen, "trace_log: live tracing STOPPED\n" );
	}
	else if ( !stricmp( pSubCmd, "flush" ) )
	{
		FlushTraceLog();
		snprintf( pOut, outLen, "trace_log: trace file flushed\n" );
	}
	else if ( !stricmp( pSubCmd, "dump" ) )
	{
		const char *pszTarget = ( pArg2 && pArg2[0] ) ? pArg2 : "trace_dump.log";
		if ( DumpRingBufferToFile( pszTarget, 100 ) )
		{
			snprintf( pOut, outLen, "trace_log: ring buffer dumped successfully to \"%s\"\n", pszTarget );
		}
		else
		{
			snprintf( pOut, outLen, "trace_log: ERROR failed to dump ring buffer to \"%s\"\n", pszTarget );
		}
	}
	else
	{
		snprintf( pOut, outLen, "Usage: trace_log [start <file> | stop | flush | status | dump <file>]\n" );
	}

	// Echo to console if outBuffer was not provided
	if ( !outBuffer )
	{
		if ( g_engfuncs.pfnServerPrint )
			g_engfuncs.pfnServerPrint( localBuf );
		else
			printf( "%s", localBuf );
	}

	return true;
}
