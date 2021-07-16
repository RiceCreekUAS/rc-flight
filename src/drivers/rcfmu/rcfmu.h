//
// FILE: rcfmu.h
//
// DESCRIPTION: interact with rcfmu (AP_HAL/ChibiOS/Ardupilot based)
// flight controller
//

#pragma once

#include <eigen3/Eigen/Core>
using namespace Eigen;

#include <pyprops.h>

#include "drivers/driver.h"
#include "include/globaldefs.h" /* fixme, get rid of? */
#include "util/butter.h"
#include "util/linearfit.h"
#include "util/lowpass.h"

#include "serial_link2.h"
#include "rcfmu_messages.h"

class rcfmu_t: public driver_t {
    
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    rcfmu_t() {}
    ~rcfmu_t() {}
    void init( pyPropertyNode *config );
    float read();
    void process() {}
    void write();
    void close();
    void command(const char *cmd);

private:
    pyPropertyNode aura4_config;
    pyPropertyNode aura4_node;
    pyPropertyNode airdata_node;
    pyPropertyNode ekf_node;
    pyPropertyNode gps_node;
    pyPropertyNode imu_node;
    pyPropertyNode pilot_node;
    pyPropertyNode power_node;
    pyPropertyNode act_node;
    pyPropertyNode status_node;
    
    string device_name = "/dev/ttyS4";
    int baud = 500000;
    SerialLink2 serial;
    bool configuration_sent = false;
    int last_ack_id = 0;
    int last_ack_subid = 0;
    uint32_t skipped_frames = 0;
    uint32_t airdata_packet_counter = 0;
    uint32_t ekf_packet_counter = 0;
    uint32_t gps_packet_counter = 0;
    uint32_t imu_packet_counter = 0;
    uint32_t pilot_packet_counter = 0;

    bool airspeed_inited = false;
    double airspeed_zero_start_time = 0.0;
    float pitot_calibrate = 1.0;
    // 2nd order filter, 100hz sample rate expected, 3rd field is
    // cutoff freq.  higher freq value == noisier, a value near 1 hz
    // should work well for airspeed.
    ButterworthFilter pitot_filter = ButterworthFilter(2, 100, 0.8);
    double pitot_sum = 0.0;
    int pitot_count = 0;
    float pitot_offset = 0.0;
    LowPassFilter pitot_filt = LowPassFilter(0.2);
    
    double imu_timestamp = 0.0;
    uint32_t last_imu_millis = 0;
    LinearFitFilter imu_offset = LinearFitFilter(200.0, 0.01);

    string pilot_mapping[rcfmu_message::sbus_channels]; // channel->name mapping
    
    int battery_cells = 4;
    LowPassFilter avionics_vcc_filt = LowPassFilter(2.0);
    LowPassFilter int_main_vcc_filt = LowPassFilter(2.0);
    LowPassFilter ext_main_vcc_filt = LowPassFilter(2.0);

    bool first_status_message = false;
    
    void info( const char* format, ... );
    void hard_fail( const char*format, ... );
    
    bool open( pyPropertyNode *config );
    void init_airdata( pyPropertyNode *config );
    void init_ekf( pyPropertyNode *config );
    void init_gps( pyPropertyNode *config );
    void init_imu( pyPropertyNode *config );
    void init_pilot( pyPropertyNode *config );
    void init_actuators( pyPropertyNode *config );

    bool parse( uint8_t pkt_id, uint16_t pkt_len, uint8_t *payload );
    bool send_config();
    bool write_config_message(int id, uint8_t *payload, int len);
    bool write_command_zero_gyros();
    bool write_command_cycle_inceptors();
    bool write_command_reset_ekf();
    bool wait_for_ack(uint8_t id);

    bool update_airdata( rcfmu_message::airdata_t *airdata );
    bool update_ekf( rcfmu_message::ekf_t *ekf );
    bool update_gps( rcfmu_message::gps_t *gps );
    bool update_imu( rcfmu_message::imu_t *imu );
    bool update_pilot( rcfmu_message::pilot_t *pilot );
    
    void airdata_zero_airspeed();
};
