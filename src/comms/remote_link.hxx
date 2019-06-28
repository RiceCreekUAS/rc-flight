#pragma once

#include <pyprops.hxx>

// import a python module and call it's init() and update() routines
// requires imported python modules to follow some basic rules to play
// nice.  (see examples in the code for now.)

#include <pymodule.hxx>

#include <stdint.h>
#include <string>
#include <vector>

class pyModuleRemoteLink: public pyModuleBase {

public:
    
    // constructor / destructor
    pyModuleRemoteLink();
    ~pyModuleRemoteLink() {}

    // bool open();
    void send_message( uint8_t *buf, int size );
    void send_message( int id, uint8_t *buf, int len );
    bool command();
    bool flush_serial();
    bool decode_fcs_update( const char *buf );

private:

    bool remote_link_on;
};
