/***
 *
 *	Studio Model Dead Player Angle Latching Interface
 *	Prevents dead player corpse models from rotating with view angles.
 *
 ****/

#pragma once

#ifndef STUDIO_DEATH_ANGLES_H
#define STUDIO_DEATH_ANGLES_H

#ifdef __cplusplus
extern "C" {
#endif

void StudioResetDeadPlayerAngles( void );
int StudioGetDeadPlayerAngles( int playerIndex, float *outAngles );
void StudioLatchDeadPlayerAngles( int playerIndex, const float *angles );

#ifdef __cplusplus
}
#endif

#endif // STUDIO_DEATH_ANGLES_H
