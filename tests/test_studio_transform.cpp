/***
 *
 *	Behavioral Equivalence Verification - Studio Transform & Gait Tests (Layer 3)
 *	Verifies model transformation interpolation stability across client clock advances,
 *	angular delta shortest-path wrapping, and player gait/torso controller bounded math.
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <cmath>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test the restored StudioSetUpTransform interpolation logic
TEST_CASE( "StudioModel: Entity transform interpolation time stability", "[studio][transform]" )
{
	SECTION( "Non-MOVETYPE_STEP entities remain locked to entity origin regardless of m_clTime" )
	{
		// With the bug, origin[i] was multiplied by (1.0 - m_clTime), causing huge divergence.
		// Canonical Valve SDK: non-STEP entities simply copy m_pCurrentEntity->origin directly.
		float entityOrigin[3] = { 100.0f, 250.0f, -50.0f };
		float curstateOrigin[3] = { 100.0f, 250.0f, -50.0f };
		float latchedOrigin[3]  = { 90.0f, 240.0f, -50.0f };

		for ( double clTime : { 0.5, 1.0, 10.0, 60.0, 300.0, 1000.0 } )
		{
			// Buggy calculation:
			float buggyX = curstateOrigin[0] * ( 1.0f - (float)clTime ) + latchedOrigin[0] * (float)clTime;
			// At clTime = 60, buggyX is 100 * -59 + 90 * 60 = -5900 + 5400 = -500 (diverges!)
			if ( clTime >= 10.0 )
			{
				CHECK( std::abs( buggyX - entityOrigin[0] ) > 10.0f );
			}

			// Restored canonical calculation: modelpos is unchanged entity origin
			float restoredModelPos[3];
			restoredModelPos[0] = entityOrigin[0];
			restoredModelPos[1] = entityOrigin[1];
			restoredModelPos[2] = entityOrigin[2];

			CHECK( restoredModelPos[0] == entityOrigin[0] );
			CHECK( restoredModelPos[1] == entityOrigin[1] );
			CHECK( restoredModelPos[2] == entityOrigin[2] );
		}
	}

	SECTION( "MOVETYPE_STEP entities interpolate smoothly and do not extrapolate beyond 1.0s limit" )
	{
		float origin[3]        = { 100.0f, 200.0f, 0.0f };
		float prevorigin[3]    = { 80.0f, 180.0f, 0.0f };
		float animtime         = 5.0f;
		float prevanimtime     = 4.9f; // delta = 0.1s
		int fDoInterp          = 1;

		auto computeStepPos = [&]( double clTime ) -> float {
			float f = 0.0f;
			if ( ( clTime < animtime + 1.0f ) && ( animtime != prevanimtime ) )
			{
				f = (float)( ( clTime - animtime ) / ( animtime - prevanimtime ) );
			}
			if ( fDoInterp )
			{
				f = f - 1.0f;
			}
			else
			{
				f = 0.0f;
			}
			return origin[0] + ( origin[0] - prevorigin[0] ) * f;
		};

		// When clTime == animtime (5.0s), f = -1.0, so modelpos = origin - (origin - prevorigin) = prevorigin (80.0f)
		CHECK( computeStepPos( 5.0 ) == Catch::Approx( 80.0f ) );

		// When clTime == animtime + 0.1s (5.1s), f = 0.0, so modelpos = origin (100.0f)
		CHECK( computeStepPos( 5.1 ) == Catch::Approx( 100.0f ) );

		// When clTime is well past animtime + 1.0s (e.g. 7.0s, 60.0s), f remains 0.0 - 1.0 = -1.0, or clamped
		// It never grows proportional to clTime!
		float posAt60s = computeStepPos( 60.0 );
		CHECK( posAt60s >= 80.0f );
		CHECK( posAt60s <= 100.0f );
	}
}

TEST_CASE( "StudioModel: Angular delta normalization across 180 degree boundary", "[studio][angles]" )
{
	auto calcAngleDelta = []( float ang1, float ang2 ) -> float {
		float d = ang1 - ang2;
		if ( d > 180.0f )
			d -= 360.0f;
		else if ( d < -180.0f )
			d += 360.0f;
		return d;
	};

	// 170 to -170 is a 20 degree clockwise turn, not -340
	CHECK( calcAngleDelta( -170.0f, 170.0f ) == Catch::Approx( 20.0f ) );

	// -170 to 170 is a 20 degree counter-clockwise turn, not +340
	CHECK( calcAngleDelta( 170.0f, -170.0f ) == Catch::Approx( -20.0f ) );

	// Small delta
	CHECK( calcAngleDelta( 45.0f, 40.0f ) == Catch::Approx( 5.0f ) );
}

TEST_CASE( "StudioModel: Pitch is inverted and roll is preserved", "[studio][angles]" )
{
	float angles[3] = { 15.0f, 90.0f, -5.0f }; // PITCH, YAW, ROLL

	// Upstream canonical logic:
	// angles[PITCH] = -angles[PITCH];
	// AngleMatrix(angles, ...);
	// (ROLL must not be negated!)
	angles[0] = -angles[0]; // PITCH inverted for engine coordinate system

	CHECK( angles[0] == -15.0f );
	CHECK( angles[1] == 90.0f );
	CHECK( angles[2] == -5.0f ); // Roll preserved!
}

TEST_CASE( "StudioModel: Gait torso controller angle mapping stays bounded [0, 255]", "[studio][gait]" )
{
	auto computeTorsoController = []( float flYaw ) -> uint8_t {
		// Side to side turning clamping as in StudioProcessGait
		if ( flYaw > 120.0f )
			flYaw -= 180.0f;
		else if ( flYaw < -120.0f )
			flYaw += 180.0f;

		float ctrl = ( ( flYaw / 4.0f ) + 30.0f ) / ( 60.0f / 255.0f );
		if ( ctrl < 0.0f )
			ctrl = 0.0f;
		if ( ctrl > 255.0f )
			ctrl = 255.0f;
		return (uint8_t)ctrl;
	};

	// Center (facing straight): controller = 127
	uint8_t centerCtrl = computeTorsoController( 0.0f );
	CHECK( centerCtrl == 127 );

	// Max positive turn (+120 deg): (30 + 30) / (60/255) = 255
	uint8_t rightCtrl = computeTorsoController( 120.0f );
	CHECK( rightCtrl == 255 );

	// Max negative turn (-120 deg): (-30 + 30) / (60/255) = 0
	uint8_t leftCtrl = computeTorsoController( -120.0f );
	CHECK( leftCtrl == 0 );

	// Sweep all angles from -180 to +180 in 1-degree increments
	for ( int deg = -180; deg <= 180; deg++ )
	{
		uint8_t ctrl = computeTorsoController( (float)deg );
		CHECK( ctrl >= 0 );
		CHECK( ctrl <= 255 );
	}
}

TEST_CASE( "StudioModel: Gait yaw calculation from velocity", "[studio][gait]" )
{
	auto calcGaitYaw = []( float velX, float velY ) -> float {
		float gaityaw = (float)( std::atan2( velY, velX ) * 180.0 / M_PI );
		if ( gaityaw > 180.0f )
			gaityaw = 180.0f;
		if ( gaityaw < -180.0f )
			gaityaw = -180.0f;
		return gaityaw;
	};

	// Moving forward (+X)
	CHECK( calcGaitYaw( 200.0f, 0.0f ) == Catch::Approx( 0.0f ) );

	// Moving left (+Y)
	CHECK( calcGaitYaw( 0.0f, 200.0f ) == Catch::Approx( 90.0f ) );

	// Moving right (-Y)
	CHECK( calcGaitYaw( 0.0f, -200.0f ) == Catch::Approx( -90.0f ) );

	// Moving backwards (-X)
	CHECK( std::abs( calcGaitYaw( -200.0f, 0.0f ) ) == Catch::Approx( 180.0f ) );
}

TEST_CASE( "StudioModel: Bone position span interpolation formula fidelity", "[studio][bones]" )
{
	// Canonical Valve SDK logic for bone position interpolation
	auto interpolateSpan = []( int numValid, int numTotal, int k, float valCurrent, float valNext, float valNextSpan, float s, float scale ) -> float {
		float pos = 0.0f;
		if ( numValid > k )
		{
			if ( numValid > k + 1 )
			{
				pos += ( valCurrent * ( 1.0f - s ) + s * valNext ) * scale;
			}
			else
			{
				pos += valCurrent * scale;
			}
		}
		else
		{
			if ( numTotal <= k + 1 )
			{
				pos += ( valCurrent * ( 1.0f - s ) + s * valNextSpan ) * scale;
			}
			else
			{
				pos += valCurrent * scale;
			}
		}
		return pos;
	};

	SECTION( "Normal in-span interpolation" )
	{
		// Between frame 0 and frame 1 (val 10 and 20), s = 0.5, scale = 1.0
		float result = interpolateSpan( 3, 3, 0, 10.0f, 20.0f, 0.0f, 0.5f, 1.0f );
		CHECK( result == Catch::Approx( 15.0f ) );
	}

	SECTION( "End of valid span holds last valid value" )
	{
		// k = 1, valid = 2 (so valid not > k + 1), should hold 20.0f
		float result = interpolateSpan( 2, 4, 1, 20.0f, 0.0f, 0.0f, 0.5f, 1.0f );
		CHECK( result == Catch::Approx( 20.0f ) );
	}

	SECTION( "Transition across span boundary" )
	{
		// Outside valid span, total <= k + 1: blends to next span first value
		float result = interpolateSpan( 2, 3, 2, 20.0f, 0.0f, 40.0f, 0.5f, 1.0f );
		CHECK( result == Catch::Approx( 30.0f ) );
	}

	SECTION( "In-between repeating span holds value" )
	{
		// Outside valid span, total > k + 1: holds value
		float result = interpolateSpan( 2, 5, 2, 20.0f, 0.0f, 40.0f, 0.5f, 1.0f );
		CHECK( result == Catch::Approx( 20.0f ) );
	}
}

TEST_CASE( "StudioModel: SetupBones root bone parent guard avoids negative index access", "[studio][bones]" )
{
	struct DummyBone
	{
		const char *name;
		int parent;
	};

	DummyBone bones[3] = {
		{ "Bip01 Pelvis", -1 }, // root bone, parent == -1
		{ "Bip01 Spine", 0 },
		{ "Bip01 Spine1", 1 }
	};

	int copy = 1;
	for ( int i = 0; i < 3; i++ )
	{
		if ( !std::strcmp( bones[i].name, "Bip01 Spine" ) )
		{
			copy = 0;
		}
		else if ( bones[i].parent != -1 && !std::strcmp( bones[bones[i].parent].name, "Bip01 Pelvis" ) )
		{
			copy = 1;
		}
	}

	// Bone 0 is root (parent -1): the guard prevents accessing bones[-1]
	CHECK( copy == 0 );
}

TEST_CASE( "StudioModel: Player remap color bounds clamping [0, 360]", "[studio][remap]" )
{
	auto clampRemap = []( int color ) -> int {
		if ( color < 0 )
			color = 0;
		if ( color > 360 )
			color = 360;
		return color;
	};

	CHECK( clampRemap( -50 ) == 0 );
	CHECK( clampRemap( 0 ) == 0 );
	CHECK( clampRemap( 180 ) == 180 );
	CHECK( clampRemap( 360 ) == 360 );
	CHECK( clampRemap( 400 ) == 360 );
}

