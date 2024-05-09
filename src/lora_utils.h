#ifndef LORA_UTILS_H_
#define LORA_UTILS_H_

#include <Arduino.h>

struct ReceivedLoRaPacket {
    String  text;
    int     rssi;
    float   snr;
    int     freqError;
};

namespace LoRa_Utils {

    void setFlag();
    void changeFreq();
    void setup();
    void sendNewPacket(const String &newPacket);
    ReceivedLoRaPacket receivePacket();

}

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
}

#endif