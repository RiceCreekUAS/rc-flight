//
// FILE: imu_fgfs.hxx
// DESCRIPTION: aquire live sensor data from a live running instance
// of Flightgear
//

#ifndef _AURA_IMU_FGFS_HXX
#define _AURA_IMU_FGFS_HXX


#include "python/pyprops.hxx"

#include "include/globaldefs.h"


// function prototypes
bool fgfs_imu_init( string output_path, pyPropertyNode *config );
bool fgfs_imu_update();
void fgfs_imu_close();

bool fgfs_airdata_init( string output_path );
bool fgfs_airdata_update();


#endif // _AURA_IMU_FGFS_HXX
