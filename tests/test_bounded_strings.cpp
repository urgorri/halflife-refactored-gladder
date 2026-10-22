#include "external/catch2/catch_amalgamated.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "common/port.h"

// Verifies bounded string formatting guarantees (#149)

static void FormatBounded( char *dst, size_t size, const char *fmt, ... )
{
	va_list argptr;
	va_start( argptr, fmt );
	vsnprintf( dst, size, fmt, argptr );
	va_end( argptr );
	dst[size - 1] = '\0';
}

TEST_CASE( "vsnprintf truncates and null-terminates overlong output (#149)", "[strings][security]" )
{
	std::string longInput( 4000, 'A' );
	char buf[1024];
	std::memset( buf, 'Z', sizeof( buf ) );

	FormatBounded( buf, sizeof( buf ), "%s", longInput.c_str() );

	REQUIRE( std::strlen( buf ) == 1023 );
	REQUIRE( buf[1023] == '\0' );
}

TEST_CASE( "snprintf keeps short output unchanged (#149)", "[strings][security]" )
{
	char buf[16];
	snprintf( buf, sizeof( buf ), "%s_%d", "abc", 42 );
	REQUIRE( std::strcmp( buf, "abc_42" ) == 0 );
}

TEST_CASE( "snprintf into a small buffer stays terminated (#149)", "[strings][security]" )
{
	char buf[8];
	snprintf( buf, sizeof( buf ), "%d", 2147483647 );
	REQUIRE( std::strlen( buf ) == 7 );
	REQUIRE( buf[7] == '\0' );
}
