#include "math.h"
#include "rapidjson/document.h"
#include "time.h"
#include "util/sg_path.h"

#include "gps_gpsd.h"

#include <iostream>
using std::cout;
using std::endl;

// connect to gpsd
void gpsd_t::connect() {
    // make sure it's closed
    gpsd_sock.close();

    if ( verbose ) {
	printf("Attempting to connect to gpsd @ %s:%d ... ",
	       host.c_str(), port);
    }

    if ( ! gpsd_sock.open( true ) ) {
	if ( verbose ) {
	    printf("error opening gpsd socket\n");
	}
	return;
    }
    
    if (gpsd_sock.connect( host.c_str(), port ) < 0) {
	if ( verbose ) {
	    printf("error connecting to gpsd\n");
	}
	return;
    }

    gpsd_sock.setBlocking( false );

    socket_connected = true;

    send_init();

    if ( verbose ) {
	printf("success!\n");
    }
}

// send our configured init strings to configure gpsd the way we prefer
void gpsd_t::send_init() {
    if ( !socket_connected ) {
	return;
    }

    if ( init_string != "" ) {
	if ( verbose ) {
	    printf("sending to gpsd: %s\n", init_string.c_str());
	}
	if ( gpsd_sock.send( init_string.c_str(), init_string.length() ) < 0 ) {
	    socket_connected = false;
	}
    }

    last_init_time = get_Time();
}

void gpsd_t::init( PropertyNode *config ) {
    if ( config->hasChild("port") ) {
	port = config->getInt("port");
    }
    if ( config->hasChild("host") ) {
	host = config->getString("host");
    }
    if ( config->hasChild("init_string") ) {
	init_string = config->getString("init_string");
    }
    bool primary = false;
    if ( config->hasChild("primary") ) {
        primary = config->getBool("primary");
    }
    string output_path = get_next_path("/sensors", "gps", primary);
    gps_node = PropertyNode(output_path.c_str(), true);
    string raw_path = get_next_path("/sensors", "gps_raw", true);
    raw_node = PropertyNode(raw_path.c_str(), true);
    ephem_node = raw_node.getChild("ephemeris", true);
}

bool gpsd_t::parse_message(const string message) {
    //printf("parse: %s\n", message.c_str());
    rapidjson::Document d;
    d.Parse(message.c_str());
    if ( !d.HasMember("class") ) {
        return false;
    }
    string msg_class = d["class"].GetString();
    if ( msg_class == "VERSION" ) {
        printf("gpsd: %s\n", message.c_str());
    } else if ( msg_class == "TPV" ) {
        // time, pos, vel
        if ( d.HasMember("time") ) {
            string time_str = d["time"].GetString();
            struct tm t;
            char* ptr = strptime(time_str.c_str(), "%Y-%m-%dT%H:%M:%S", &t);
            // printf("hour: %d min: %d sec: %d\n", t.tm_hour, t.tm_min, t.tm_sec);
            if ( ptr == nullptr ) {
                printf("gpsd: unable to parse time string = %s\n",
                       time_str.c_str());
            } else {
                double t2 = timegm(&t); // UTC
                if ( *ptr ) {
                    double fraction = atof(ptr);
                    // printf("fraction: %f\n", fraction);
                    t2 += fraction;
                }
                gps_node.setDouble( "unix_time_sec", t2 );
                gps_node.setDouble( "timestamp", get_Time() );
                // printf("%s %f\n", time_str.c_str(), t2);
            }
        }
        if ( d.HasMember("leapseconds") ) {
            leapseconds = d["leapseconds"].GetDouble();
            gps_node.setDouble( "leapseconds", leapseconds );
        }
        if ( d.HasMember("lat") ) {
            gps_node.setDouble( "latitude_deg", d["lat"].GetDouble() );
        }
        if ( d.HasMember("lon") ) {
            gps_node.setDouble( "longitude_deg", d["lon"].GetDouble() );
        }
        if ( d.HasMember("alt") ) {
            gps_node.setDouble( "altitude_m", d["alt"].GetFloat() );
        }
        float course_deg = 0.0;
        if ( d.HasMember("track") ) {
            course_deg = d["track"].GetFloat();
        }
        float speed_mps = 0.0;
        if ( d.HasMember("speed") ) {
            speed_mps = d["speed"].GetFloat();
        }
        float angle_rad = (90.0 - course_deg) * M_PI/180.0;
        gps_node.setDouble( "vn_mps", sin(angle_rad) * speed_mps );
        gps_node.setDouble( "ve_mps", cos(angle_rad) * speed_mps );
        if ( d.HasMember("climb") ) {
            gps_node.setDouble( "vd_mps", -d["climb"].GetFloat() );
        }
        if ( d.HasMember("mode") ) {
            gps_node.setInt( "fixType", d["mode"].GetInt() );
        }
    } else if ( msg_class == "SKY" ) {
        if ( d.HasMember("satellites") ) {
            int num_sats = 0;
            const rapidjson::Value& sats = d["satellites"];
            if ( sats.IsArray() ) {
                for (rapidjson::SizeType i = 0; i < sats.Size(); i++) {
                    if ( sats[i].HasMember("used") ) {
                        if ( sats[i]["used"].GetBool() ) {
                            num_sats++;
                        }
                    }
                }
            }
            gps_node.setInt( "satellites", num_sats );
        }
    } else {
        printf("gpsd: unhandled class = %s\n", msg_class.c_str());
        printf("parse: %s\n", message.c_str());
    }
    return true;
}

bool gpsd_t::process_buffer() {
    if ( json_buffer.length() <= 2 ) {
        return false;
    }
    //printf("buffer len: %lu\n", json_buffer.length());
    int start = 0;
    int end = 0;
    //printf("gpsd buffer: %s\n", json_buffer.c_str());
    int level = 0;
    for ( unsigned int i = start; i < json_buffer.length(); i++ ) {
        if ( json_buffer[i] == '{' ) {
            //printf("  found '{' @ %d\n", i); 
            if ( level == 0 ) {
                start = i;
            }
            level++;
        }
        if ( json_buffer[i] == '}' ) {
            //printf("  found '}' @ %d\n", i);
            level--;
            if ( level == 0 ) {
                end = i;
                parse_message(json_buffer.substr(start, end-start+1));
                json_buffer.erase(0, end+1);
                break;
            }
        }
    }
    // if ( level > 0 ) {
    //     printf("gpsd: incomplete json: %s\n", json_buffer.c_str());
    // }
    
    // try to ensure some sort of sanity so the buffer can't
    // completely run away if something bad is happening.
    if ( json_buffer.length() > 16384 ) {
        json_buffer = "";
        return true;
    }
    return false;
}

float gpsd_t::read() {
    bool gps_data_valid = false;

    if ( !socket_connected ) {
	connect();
    }

    const int buf_size = 256;
    char buf[buf_size];
    int result;
    while ( (result = gpsd_sock.recv( buf, buf_size-1 )) > 0 ) {
	buf[result] = 0;
        json_buffer += buf;
    }
    if ( errno != EAGAIN ) {
	if ( verbose ) {
	    perror("gpsd_sock.recv()");
	}
	socket_connected = false;
    }

    process_buffer();
    
    // If more than 5 seconds has elapsed without seeing new data and
    // our last init attempt was more than 5 seconds ago, try
    // resending the init sequence.
    double gps_timestamp = gps_node.getDouble("timestamp");
    if ( get_Time() > gps_timestamp + 5 && get_Time() > last_init_time + 5 ) {
	send_init();
    }

    return gps_data_valid;
}

void gpsd_t::close() {
    gpsd_sock.close();
    socket_connected = false;
}
