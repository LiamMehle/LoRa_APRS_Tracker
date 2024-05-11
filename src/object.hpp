#pragma once
#include <vector>
#include <cstdint>
#include <WString.h>
#include "TinyGPS++.h"
#include "display.h"
#include "configuration.h"

extern Beacon* currentBeacon;
extern TinyGPSPlus gps;

namespace APRS {
    // base structure of a packet
    struct PacketMetadata {
        String from;
        // >
        String to;
        std::vector<String> path; // comma sepatrated
        // :
        // String data;
    };
    // suplementary information to add at the end of a bare aprs packet
    struct Object {
        String name;
        String comment;
        double latitude;
        double longtitude;
        bool is_live;
        uint16_t course;
        uint16_t speed;
        uint8_t hour, minute, second;
        char overlay;
        char symbol;
    };
    enum PublishObjectStatus {
        Success = 0,
        TooLong
    };

    PublishObjectStatus publish_object(PacketMetadata const aprs_metadata, Object const object_info);

    namespace object {
        void place();
        void remove_last();
        void remove_all();
        void retransmit_all();
    }
}
