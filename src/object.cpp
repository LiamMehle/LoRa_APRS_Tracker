#include "configuration.h"
#include "object.hpp"
#include "lora_utils.h"

extern Configuration    Config;

namespace APRS {
    // writes the beginning of an APRS packet into provided buffer ending with ':'. After that comes the payload.
    static
    size_t write_aprs_metadata(char buffer[], size_t const buffer_size, PacketMetadata const info) {
        size_t buffer_offset = snprintf(buffer, buffer_size, "%s>%s", info.from.c_str(), info.to.c_str());
        for (auto const& path : info.path)
            buffer_offset += snprintf(
                buffer+buffer_offset,         // write to end of string
                buffer_size-buffer_offset, // no more than available space
                ",%s", path.c_str());         // ",<callsign>"

        if (buffer_size - 2 <= buffer_offset) return buffer_offset;

        buffer[buffer_offset++] = ':';
        buffer[buffer_offset] = 0;            // null-terminate
        return buffer_offset;
    }
    static
    size_t write_aprs_object_payload(char buffer[], size_t const buffer_size, Object const info) {
        struct CoordSpec {
            uint8_t degrees;
            uint8_t minutes;
            uint8_t fractional_minutes;
            char    cardinal_direction;
        };
        auto parse_coord = [](double const x,
                        char const positive_char,
                        char const negative_char) -> struct CoordSpec {
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wstrict-aliasing"
            uint64_t const x_as_int = *(uint64_t*)(&x);
            bool     const is_negative = (x_as_int >> 63) != 0;  // get last bit
            uint64_t const abs_x_as_int = x_as_int & ~(1ull<<63);
            double   const absolute_x = *(double*)&abs_x_as_int;
            #pragma GCC diagnostic pop
            uint64_t constexpr scaling_factor = 6000;
            uint32_t const scaled_absolute_x = static_cast<uint32_t>(absolute_x * scaling_factor);
            uint8_t  const degrees    = static_cast<uint8_t>(scaled_absolute_x/scaling_factor);
            uint8_t  const minutes    = static_cast<uint8_t>(scaled_absolute_x/(scaling_factor/60)%60);
            uint8_t  const fractional_minutes = static_cast<uint8_t>(scaled_absolute_x/(scaling_factor/6000)%100);
            return CoordSpec{
                degrees,
                minutes,
                fractional_minutes,
                is_negative ? negative_char : positive_char};
        };

        auto const parsed_lat   = parse_coord(info.latitude, 'N', 'S');
        auto const parsed_lng = parse_coord(info.longtitude, 'E', 'W');

        auto const buffer_offset = snprintf(buffer, buffer_size,
            ";%-9s%c"                  // name and status
            "%02hhd%02hhd%02hhdz"      // time
            "%02hhd%02hhd.%02hhd%c"    // latitude
            "%c"                       // overlay
            "%03hhd%02hhd.%02hhd%c"    // longtitude
            "%c"                       // symbol
            "%s",                      // comment
            info.name.c_str(),
            info.is_live ? '*' : '_',  // is not killed?
            info.hour, info.minute, info.second,
            parsed_lat.degrees, parsed_lat.minutes, parsed_lat.fractional_minutes, parsed_lat.cardinal_direction,
            info.overlay,
            parsed_lng.degrees, parsed_lng.minutes, parsed_lng.fractional_minutes, parsed_lng.cardinal_direction,
            info.symbol,
            info.comment.c_str());
        return buffer_offset;
    }
    PublishObjectStatus publish_object(PacketMetadata const aprs_metadata, Object const object_info) {
        char buffer[256];
        auto buffer_offset = write_aprs_metadata(buffer, sizeof(buffer), aprs_metadata);
        buffer_offset += write_aprs_object_payload(buffer+buffer_offset, sizeof(buffer)-buffer_offset, object_info);
        if (buffer_offset == sizeof(buffer)) {
            show_display("[ERROR]", "buffer overrun: aborted", 0);
            return TooLong;
        }
        show_display("[TX]", buffer, 0);
        LoRa_Utils::sendNewPacket(buffer);
        return Success;
    }
    namespace object {
        std::vector<APRS::Object> registered_objects = std::vector<APRS::Object>();
        char const* const object_basename = "OBJECT";

        void place() {
            char object_name[10];
            snprintf(object_name, sizeof(object_name), "%6s%03d", object_basename, registered_objects.size());
            APRS::Object object {
                    .name = object_name,
                    .comment = "",
                    .latitude = gps.location.lat(),
                    .longtitude = gps.location.lng(),
                    .is_live = true,
                    .hour = gps.time.hour(),
                    .minute = gps.time.minute(),
                    .second = gps.time.second(),
                    .overlay = '/',
                    .symbol = '['
            };
            APRS::publish_object({
                    .from = currentBeacon->callsign,
                    .to = "APRS",
                    .path = {Config.path}
                }, object);
            registered_objects.emplace_back(object);
        }
        void remove_last() {
            if (registered_objects.empty())
                return;

            auto& object = registered_objects.back();
            object.is_live = false;
            APRS::publish_object({
                    .from = currentBeacon->callsign,
                    .to = "APRS",
                    .path = {Config.path}
                }, object);
            registered_objects.pop_back();
        }
        void remove_all() {
            for (auto object : registered_objects) {
                object.is_live = false;
                APRS::publish_object({
                    .from = currentBeacon->callsign,
                    .to = "APRS",
                    .path = {Config.path}
                }, object);
                usleep(100000);
            }
            registered_objects.clear();
        }
        void retransmit_all() {
            for (auto const object : registered_objects) {
                APRS::publish_object({
                    .from = currentBeacon->callsign,
                    .to = "APRS",
                    .path = {Config.path}
                }, object);
                usleep(100000);
            }
        }
    }
}