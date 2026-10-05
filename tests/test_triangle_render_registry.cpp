#include "catch_amalgamated.hpp"
#include "render/triangle_render_registry.h"
#include <string>
#include <vector>

namespace
{

static int s_transparentCallbackCalls = 0;
static int s_normalCallbackCalls      = 0;
static std::vector<std::string> s_executionOrder;

void MockTransparentCallbackA()
{
	s_transparentCallbackCalls++;
	s_executionOrder.push_back( "TransparentA" );
}

void MockTransparentCallbackB()
{
	s_transparentCallbackCalls++;
	s_executionOrder.push_back( "TransparentB" );
}

void MockNormalCallbackA()
{
	s_normalCallbackCalls++;
	s_executionOrder.push_back( "NormalA" );
}

class MockTransparentHook : public ITransparentTriangleHook
{
  public:
	explicit MockTransparentHook( std::string name = "MockTransparentHook" )
	    : m_name( std::move( name ) ), m_callCount( 0 )
	{
	}

	void RenderTransparentTriangles( void ) override
	{
		m_callCount++;
		s_executionOrder.push_back( m_name );
	}

	std::string m_name;
	int m_callCount;
};

class MockNormalHook : public INormalTriangleHook
{
  public:
	explicit MockNormalHook( std::string name = "MockNormalHook" )
	    : m_name( std::move( name ) ), m_callCount( 0 )
	{
	}

	void RenderNormalTriangles( void ) override
	{
		m_callCount++;
		s_executionOrder.push_back( m_name );
	}

	std::string m_name;
	int m_callCount;
};

} // namespace

TEST_CASE( "TriangleRenderRegistry: Transparent Callback Registration & Dispatch", "[render][tri][transparent]" )
{
	TriangleRenderRegistry::Clear();
	s_transparentCallbackCalls = 0;
	s_executionOrder.clear();

	REQUIRE( !TriangleRenderRegistry::HasTransparentHooks() );

	SECTION( "Dispatch with no callbacks does nothing" )
	{
		TriangleRenderRegistry::DispatchTransparentTriangles();
		CHECK( s_transparentCallbackCalls == 0 );
	}

	SECTION( "Register callback and dispatch" )
	{
		TriangleRenderRegistry::RegisterTransparentCallback( MockTransparentCallbackA, 0, "CallbackA" );
		CHECK( TriangleRenderRegistry::HasTransparentHooks() );

		const auto &renderers = TriangleRenderRegistry::GetTransparentRenderers();
		REQUIRE( renderers.size() == 1 );
		CHECK( renderers[0].pfnCallback == MockTransparentCallbackA );
		CHECK( renderers[0].pHook == nullptr );
		CHECK( renderers[0].iDrawOrder == 0 );
		CHECK( std::string( renderers[0].pszName ) == "CallbackA" );

		TriangleRenderRegistry::DispatchTransparentTriangles();
		CHECK( s_transparentCallbackCalls == 1 );
		REQUIRE( s_executionOrder.size() == 1 );
		CHECK( s_executionOrder[0] == "TransparentA" );
	}

	SECTION( "Register null callback is safely ignored" )
	{
		TriangleRenderRegistry::RegisterTransparentCallback( nullptr );
		CHECK( !TriangleRenderRegistry::HasTransparentHooks() );
		CHECK( TriangleRenderRegistry::GetTransparentRenderers().empty() );
	}
}

TEST_CASE( "TriangleRenderRegistry: Transparent Hook Interface Registration & Dispatch", "[render][tri][transparent]" )
{
	TriangleRenderRegistry::Clear();
	s_executionOrder.clear();

	MockTransparentHook hook( "HookInstance" );

	TriangleRenderRegistry::RegisterTransparentHook( &hook, 10, "CustomHook" );
	REQUIRE( TriangleRenderRegistry::HasTransparentHooks() );

	const auto *desc = TriangleRenderRegistry::FindTransparentRenderer( "CustomHook" );
	REQUIRE( desc != nullptr );
	CHECK( desc->pHook == &hook );
	CHECK( desc->pfnCallback == nullptr );
	CHECK( desc->iDrawOrder == 10 );

	TriangleRenderRegistry::DispatchTransparentTriangles();
	CHECK( hook.m_callCount == 1 );
	REQUIRE( s_executionOrder.size() == 1 );
	CHECK( s_executionOrder[0] == "HookInstance" );

	SECTION( "Register null hook is safely ignored" )
	{
		TriangleRenderRegistry::RegisterTransparentHook( nullptr );
		CHECK( TriangleRenderRegistry::GetTransparentRenderers().size() == 1 );
	}
}

TEST_CASE( "TriangleRenderRegistry: Draw Order Priority Sorting", "[render][tri][order]" )
{
	TriangleRenderRegistry::Clear();
	s_transparentCallbackCalls = 0;
	s_executionOrder.clear();

	MockTransparentHook hookLate( "LateHook" );       // 100
	MockTransparentHook hookEarly( "EarlyHook" );     // -50
	MockTransparentHook hookMiddle( "MiddleHook" );   // 10

	TriangleRenderRegistry::RegisterTransparentHook( &hookLate, 100, "LateHook" );
	TriangleRenderRegistry::RegisterTransparentCallback( MockTransparentCallbackA, 0, "MiddleA" );
	TriangleRenderRegistry::RegisterTransparentHook( &hookEarly, -50, "EarlyHook" );
	TriangleRenderRegistry::RegisterTransparentHook( &hookMiddle, 10, "MiddleHook" );
	TriangleRenderRegistry::RegisterTransparentCallback( MockTransparentCallbackB, 0, "MiddleB" );

	SECTION( "Dispatch executes in ascending order of drawOrder" )
	{
		TriangleRenderRegistry::DispatchTransparentTriangles();

		REQUIRE( s_executionOrder.size() == 5 );
		CHECK( s_executionOrder[0] == "EarlyHook" );    // -50
		CHECK( s_executionOrder[1] == "TransparentA" ); // 0 (stable preserved)
		CHECK( s_executionOrder[2] == "TransparentB" ); // 0
		CHECK( s_executionOrder[3] == "MiddleHook" );   // 10
		CHECK( s_executionOrder[4] == "LateHook" );     // 100
	}
}

TEST_CASE( "TriangleRenderRegistry: Deduplication and Re-Registration", "[render][tri][dedup]" )
{
	TriangleRenderRegistry::Clear();

	MockTransparentHook hook( "TestHook" );

	TriangleRenderRegistry::RegisterTransparentHook( &hook, 50, "FirstPass" );
	REQUIRE( TriangleRenderRegistry::GetTransparentRenderers().size() == 1 );
	CHECK( TriangleRenderRegistry::GetTransparentRenderers()[0].iDrawOrder == 50 );

	// Re-register the exact same hook with new order and name
	TriangleRenderRegistry::RegisterTransparentHook( &hook, -10, "UpdatedHook" );
	REQUIRE( TriangleRenderRegistry::GetTransparentRenderers().size() == 1 );
	CHECK( TriangleRenderRegistry::GetTransparentRenderers()[0].iDrawOrder == -10 );
	CHECK( std::string( TriangleRenderRegistry::GetTransparentRenderers()[0].pszName ) == "UpdatedHook" );

	// Same for callback
	TriangleRenderRegistry::RegisterTransparentCallback( MockTransparentCallbackA, 20, "CallA" );
	TriangleRenderRegistry::RegisterTransparentCallback( MockTransparentCallbackA, 5, "CallA_Updated" );
	REQUIRE( TriangleRenderRegistry::GetTransparentRenderers().size() == 2 );
	CHECK( TriangleRenderRegistry::FindTransparentRenderer( "CallA_Updated" ) != nullptr );
	CHECK( TriangleRenderRegistry::FindTransparentRenderer( "CallA_Updated" )->iDrawOrder == 5 );
}

TEST_CASE( "TriangleRenderRegistry: Unregistration Lifecycle & Flag Updates", "[render][tri][lifecycle]" )
{
	TriangleRenderRegistry::Clear();

	MockTransparentHook hookA( "HookA" );
	MockTransparentHook hookB( "HookB" );

	TriangleRenderRegistry::RegisterTransparentHook( &hookA, 0, "HookA" );
	TriangleRenderRegistry::RegisterTransparentHook( &hookB, 1, "HookB" );
	TriangleRenderRegistry::RegisterTransparentCallback( MockTransparentCallbackA, 2, "CallbackA" );

	REQUIRE( TriangleRenderRegistry::HasTransparentHooks() );
	REQUIRE( TriangleRenderRegistry::GetTransparentRenderers().size() == 3 );

	SECTION( "Unregister callback removes it cleanly" )
	{
		CHECK( TriangleRenderRegistry::UnregisterTransparentCallback( MockTransparentCallbackA ) == true );
		CHECK( TriangleRenderRegistry::GetTransparentRenderers().size() == 2 );
		CHECK( TriangleRenderRegistry::FindTransparentRenderer( "CallbackA" ) == nullptr );

		// Unregistering again returns false
		CHECK( TriangleRenderRegistry::UnregisterTransparentCallback( MockTransparentCallbackA ) == false );
		CHECK( TriangleRenderRegistry::UnregisterTransparentCallback( nullptr ) == false );
	}

	SECTION( "Unregister hook removes it cleanly" )
	{
		CHECK( TriangleRenderRegistry::UnregisterTransparentHook( &hookA ) == true );
		CHECK( TriangleRenderRegistry::GetTransparentRenderers().size() == 2 );
		CHECK( TriangleRenderRegistry::FindTransparentRenderer( "HookA" ) == nullptr );

		CHECK( TriangleRenderRegistry::UnregisterTransparentHook( &hookA ) == false );
		CHECK( TriangleRenderRegistry::UnregisterTransparentHook( nullptr ) == false );
	}

	SECTION( "Removing all renderers toggles HasTransparentHooks back to false" )
	{
		TriangleRenderRegistry::UnregisterTransparentHook( &hookA );
		TriangleRenderRegistry::UnregisterTransparentHook( &hookB );
		TriangleRenderRegistry::UnregisterTransparentCallback( MockTransparentCallbackA );

		CHECK( !TriangleRenderRegistry::HasTransparentHooks() );
		CHECK( TriangleRenderRegistry::GetTransparentRenderers().empty() );
	}

	SECTION( "ClearTransparent clears everything and resets state" )
	{
		TriangleRenderRegistry::ClearTransparent();
		CHECK( !TriangleRenderRegistry::HasTransparentHooks() );
		CHECK( TriangleRenderRegistry::GetTransparentRenderers().empty() );
	}
}

TEST_CASE( "TriangleRenderRegistry: Normal Triangle API Lifecycle", "[render][tri][normal]" )
{
	TriangleRenderRegistry::Clear();
	s_normalCallbackCalls = 0;
	s_executionOrder.clear();

	MockNormalHook normHook( "NormHook" );

	REQUIRE( !TriangleRenderRegistry::HasNormalHooks() );

	TriangleRenderRegistry::RegisterNormalCallback( MockNormalCallbackA, 20, "NormCallA" );
	TriangleRenderRegistry::RegisterNormalHook( &normHook, 10, "NormHook" );

	CHECK( TriangleRenderRegistry::HasNormalHooks() );
	REQUIRE( TriangleRenderRegistry::GetNormalRenderers().size() == 2 );

	// Dispatch
	TriangleRenderRegistry::DispatchNormalTriangles();
	CHECK( s_normalCallbackCalls == 1 );
	CHECK( normHook.m_callCount == 1 );
	REQUIRE( s_executionOrder.size() == 2 );
	CHECK( s_executionOrder[0] == "NormHook" );  // order 10
	CHECK( s_executionOrder[1] == "NormalA" );   // order 20

	// Find
	CHECK( TriangleRenderRegistry::FindNormalRenderer( "NormCallA" ) != nullptr );
	CHECK( TriangleRenderRegistry::FindNormalRenderer( "NonExistent" ) == nullptr );
	CHECK( TriangleRenderRegistry::FindNormalRenderer( nullptr ) == nullptr );

	// Unregister
	CHECK( TriangleRenderRegistry::UnregisterNormalCallback( MockNormalCallbackA ) );
	CHECK( TriangleRenderRegistry::UnregisterNormalHook( &normHook ) );
	CHECK( !TriangleRenderRegistry::HasNormalHooks() );
}

TEST_CASE( "TriangleRenderRegistry: Clear resets both transparent and normal", "[render][tri][clear]" )
{
	TriangleRenderRegistry::Clear();

	MockTransparentHook transHook( "Trans" );
	MockNormalHook normHook( "Norm" );

	TriangleRenderRegistry::RegisterTransparentHook( &transHook );
	TriangleRenderRegistry::RegisterNormalHook( &normHook );

	CHECK( TriangleRenderRegistry::HasTransparentHooks() );
	CHECK( TriangleRenderRegistry::HasNormalHooks() );

	TriangleRenderRegistry::Clear();

	CHECK( !TriangleRenderRegistry::HasTransparentHooks() );
	CHECK( !TriangleRenderRegistry::HasNormalHooks() );
	CHECK( TriangleRenderRegistry::GetTransparentRenderers().empty() );
	CHECK( TriangleRenderRegistry::GetNormalRenderers().empty() );
}

TEST_CASE( "TriangleRenderRegistry: Overloaded Register / Unregister Methods", "[render][tri][overload]" )
{
	TriangleRenderRegistry::Clear();

	MockTransparentHook transHook( "TransOverload" );
	MockNormalHook normHook( "NormOverload" );

	// Transparent overloads
	TriangleRenderRegistry::RegisterTransparentRenderer( MockTransparentCallbackA, -5, "FuncOverload" );
	TriangleRenderRegistry::RegisterTransparentRenderer( &transHook, 15, "HookOverload" );

	CHECK( TriangleRenderRegistry::HasTransparentHooks() );
	REQUIRE( TriangleRenderRegistry::GetTransparentRenderers().size() == 2 );
	CHECK( TriangleRenderRegistry::FindTransparentRenderer( "FuncOverload" ) != nullptr );
	CHECK( TriangleRenderRegistry::FindTransparentRenderer( "HookOverload" ) != nullptr );

	CHECK( TriangleRenderRegistry::UnregisterTransparentRenderer( MockTransparentCallbackA ) == true );
	CHECK( TriangleRenderRegistry::UnregisterTransparentRenderer( &transHook ) == true );
	CHECK( !TriangleRenderRegistry::HasTransparentHooks() );

	// Normal overloads
	TriangleRenderRegistry::RegisterNormalRenderer( MockNormalCallbackA, -5, "NormFuncOverload" );
	TriangleRenderRegistry::RegisterNormalRenderer( &normHook, 15, "NormHookOverload" );

	CHECK( TriangleRenderRegistry::HasNormalHooks() );
	REQUIRE( TriangleRenderRegistry::GetNormalRenderers().size() == 2 );
	CHECK( TriangleRenderRegistry::FindNormalRenderer( "NormFuncOverload" ) != nullptr );
	CHECK( TriangleRenderRegistry::FindNormalRenderer( "NormHookOverload" ) != nullptr );

	CHECK( TriangleRenderRegistry::UnregisterNormalRenderer( MockNormalCallbackA ) == true );
	CHECK( TriangleRenderRegistry::UnregisterNormalRenderer( &normHook ) == true );
	CHECK( !TriangleRenderRegistry::HasNormalHooks() );
}

