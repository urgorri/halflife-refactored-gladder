#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// Forward declarations for Half-Life engine types
struct edict_s;
typedef struct edict_s edict_t;

enum TraceEventType
{
	TRACE_EVENT_NONE = 0,
	TRACE_EVENT_THINK,
	TRACE_EVENT_TOUCH,
	TRACE_EVENT_USE,
	TRACE_EVENT_BLOCKED,
	TRACE_EVENT_FIRE_TARGETS,
	TRACE_EVENT_SCHEDULE_CHANGE,
	TRACE_EVENT_ANIM_EVENT,
	TRACE_EVENT_CUSTOM
};

struct TraceEvent
{
	double timestamp;
	TraceEventType type;
	int entindex;
	char classname[48];
	char targetname[48];
	float origin[3];

	// Target / Other / Blocker entity
	int target_entindex;
	char target_classname[48];
	char target_targetname[48];

	// Caller / Activator entity
	int caller_entindex;
	char caller_classname[48];

	// Contextual fields
	int use_type;
	float value1;
	float value2;
	int int_val;
	char details[80];
};

class CrashHandler
{
  public:
	static const int RING_BUFFER_SIZE = 1024;

	CrashHandler();
	~CrashHandler();

	// Lifecycle
	void Init( void );
	void Shutdown( void );
	bool IsInitialized( void ) const { return m_bInitialized; }

	// Live file tracing
	bool StartTraceLog( const char *pszFilename );
	void StopTraceLog( void );
	void FlushTraceLog( void );
	bool IsLiveTracingActive( void ) const { return m_pTraceFile != NULL; }
	const char *GetTraceLogFilename( void ) const { return m_szTraceFilename; }

	// Dispatch instrumentation logging
	void LogThink( edict_t *pent, float flNextThink );
	void LogTouch( edict_t *pentTouched, edict_t *pentOther );
	void LogUse( edict_t *pentUsed, edict_t *pentOther, edict_t *pentActivator, int useType, float value );
	void LogBlocked( edict_t *pentBlocked, edict_t *pentOther );
	void LogFireTargets( const char *pszTargetName, edict_t *pentCaller, edict_t *pentActivator, int useType, float value );
	void LogScheduleChange( edict_t *pent, const char *pszScheduleName, int iScheduleId = 0 );
	void LogAnimEvent( edict_t *pent, int iEventId, const char *pszOptions );
	void LogCustom( const char *pszFmt, ... );

	// Ring buffer inspection
	int GetTotalEventsLogged( void ) const { return m_nTotalEvents; }
	int GetRingBufferSize( void ) const { return RING_BUFFER_SIZE; }
	int GetActiveEventCount( void ) const;
	bool GetEvent( int indexFromNewest, TraceEvent *pOutEvent ) const;
	void Reset( void );

	// Output & post-mortem dumps
	void DumpRingBuffer( FILE *pOut, int maxEntries = 50 );
	bool DumpRingBufferToFile( const char *pszFilename, int maxEntries = 100 );
	void WriteCrashReport( void *pExceptionInfoPtr );
	void WriteMiniDump( void *pExceptionInfoPtr );

	// Console command processing (returns response message in outBuffer)
	bool ExecuteCommand( int argc, const char *argv[], char *outBuffer, size_t outBufferSize );

  private:
	void RecordEvent( const TraceEvent &event );
	static void FormatEventString( const TraceEvent &ev, char *outBuffer, size_t bufferSize );
	static bool ExtractEntityInfo( edict_t *pent, int &outIndex, char *outClassname, size_t maxClassLen,
	                               char *outTargetname, size_t maxTargetLen, float outOrigin[3] );

	TraceEvent m_ringBuffer[RING_BUFFER_SIZE];
	volatile long m_nTotalEvents;
	bool m_bInitialized;

	FILE *m_pTraceFile;
	char m_szTraceFilename[260];

#ifdef _WIN32
	void *m_pVehHandle;
	CRITICAL_SECTION m_csLock;
#endif
};

extern CrashHandler g_CrashHandler;

#endif // CRASH_HANDLER_H
