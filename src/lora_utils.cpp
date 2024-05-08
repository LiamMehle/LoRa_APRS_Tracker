#include <optional>
#include <logger.h>
#include <SPI.h>
#include "notification_utils.h"
#include "configuration.h"
#include "pins_config.h"
#include "msg_utils.h"
#include "display.h"
#ifdef HAS_SX127X
#include <LoRa.h>
#endif
#ifdef HAS_SX126X
#include <RadioLib.h>
#endif


extern logging::Logger  logger;
extern Configuration    Config;
extern LoraType         *currentLoRaType;
extern uint8_t          loraIndex;
extern int              loraIndexSize;


#if defined(TTGO_T_Beam_V1_0_SX1268) || defined(ESP32_DIY_1W_LoRa_GPS) || defined(OE5HWN_MeshCom)
SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
bool transmissionFlag = true;
bool enableInterrupt = true;
#endif
#if defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
auto radio_module = Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
SX1262 radio = SX1262(&radio_module);
bool transmissionFlag = true;
bool enableInterrupt = true;
#endif

namespace LoRa_Utils {

    void setFlag() {
        #ifdef HAS_SX126X
        transmissionFlag = true;
        #endif
    }

    void changeFreq() {
        if(loraIndex >= (loraIndexSize - 1)) {
            loraIndex = 0;
        } else {
            loraIndex++;
        }
        currentLoRaType = &Config.loraTypes[loraIndex];
        #ifdef HAS_SX126X
        float freq = (float)currentLoRaType->frequency/1000000;
        radio.setFrequency(freq);
        radio.setSpreadingFactor(currentLoRaType->spreadingFactor);
        float signalBandwidth = currentLoRaType->signalBandwidth/1000;
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);
        #if defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
        radio.setOutputPower(currentLoRaType->power + 2); // values available: 10, 17, 22 --> if 20 in tracker_conf.json it will be updated to 22.
        radio.setCurrentLimit(140);
        #endif
        #if defined(ESP32_DIY_1W_LoRa_GPS) || defined(OE5HWN_MeshCom)
        radio.setOutputPower(currentLoRaType->power);
        radio.setCurrentLimit(140);     // still needs to be validated
        #endif
        #endif
        #ifdef HAS_SX127X
        LoRa.setFrequency(currentLoRaType->frequency);
        LoRa.setSpreadingFactor(currentLoRaType->spreadingFactor);
        LoRa.setSignalBandwidth(currentLoRaType->signalBandwidth);
        LoRa.setCodingRate4(currentLoRaType->codingRate4);
        LoRa.setTxPower(currentLoRaType->power);
        #endif
        String loraCountryFreq;
        switch (loraIndex) {
            case 0:
                loraCountryFreq = "EU/WORLD";
                break;
            case 1:
                loraCountryFreq = "POLAND";
                break;
            case 2:
                loraCountryFreq = "UK";
                break;
        }
        String currentLoRainfo = "LoRa " + loraCountryFreq + " / Freq: " + String(currentLoRaType->frequency)  + " / SF:" + String(currentLoRaType->spreadingFactor) + " / CR: " + String(currentLoRaType->codingRate4);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", currentLoRainfo.c_str());
        show_display({"LORA FREQ>", "", "CHANGED TO: " + loraCountryFreq, "", "", ""}, 2000);
    }

    void setup() {
        #ifdef HAS_SX126X
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set SPI pins!");
        SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
        float freq = (float)currentLoRaType->frequency/1000000;
        int state = radio.begin(freq);
        if (state == RADIOLIB_ERR_NONE) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Initializing SX126X");
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
            while (true);
        }
        radio.setDio1Action(setFlag);
        radio.setSpreadingFactor(currentLoRaType->spreadingFactor);
        float signalBandwidth = currentLoRaType->signalBandwidth/1000;
        radio.setBandwidth(signalBandwidth);
        radio.setCodingRate(currentLoRaType->codingRate4);
        radio.setCRC(true);
        #if defined(ESP32_DIY_1W_LoRa_GPS) || defined(OE5HWN_MeshCom)
        radio.setRfSwitchPins(RADIO_RXEN, RADIO_TXEN);
        #endif
        #if defined(TTGO_T_Beam_V1_0_SX1268) || defined(TTGO_T_Beam_V1_2_SX1262) || defined(TTGO_T_Beam_S3_SUPREME_V3) || defined(HELTEC_V3_GPS) || defined(HELTEC_WIRELESS_TRACKER) || defined(TTGO_T_DECK_GPS)
        state = radio.setOutputPower(currentLoRaType->power + 2); // values available: 10, 17, 22 --> if 20 in tracker_conf.json it will be updated to 22.
        radio.setCurrentLimit(140);
        #endif
        #if defined(ESP32_DIY_1W_LoRa_GPS) || defined(OE5HWN_MeshCom)
        state = radio.setOutputPower(currentLoRaType->power); // max value 20 (when 20dB in setup 30dB in output as 400M30S has Low Noise Amp)
        radio.setCurrentLimit(140); // still needs to be validated
        #endif
        if (state == RADIOLIB_ERR_NONE) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
            while (true);
        }
        #endif
        #ifdef HAS_SX127X
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "Set SPI pins!");
        SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
        LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
        long freq = currentLoRaType->frequency;
        if (!LoRa.begin(freq)) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa", "Starting LoRa failed!");
            show_display({"ERROR", "Starting LoRa failed!"});
            while (true) {
                delay(1000);
            }
        }
        LoRa.setSpreadingFactor(currentLoRaType->spreadingFactor);
        LoRa.setSignalBandwidth(currentLoRaType->signalBandwidth);
        LoRa.setCodingRate4(currentLoRaType->codingRate4);
        LoRa.enableCrc();
        LoRa.setTxPower(currentLoRaType->power);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", "LoRa init done!");
        String currentLoRainfo = "LoRa Freq: " + String(currentLoRaType->frequency)  + " / SF:" + String(currentLoRaType->spreadingFactor) + " / CR: " + String(currentLoRaType->codingRate4);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa", currentLoRainfo.c_str());
        #endif
    }

    void sendNewPacket(const String &newPacket) {
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Tx","---> %s", newPacket.c_str());
        /*logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "LoRa","Send data: %s", newPacket.c_str());
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "LoRa","Send data: %s", newPacket.c_str());
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "LoRa","Send data: %s", newPacket.c_str());*/

        if (Config.ptt.active) {
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? LOW : HIGH);
            delay(Config.ptt.preDelay);
        }
        if (Config.notification.ledTx) digitalWrite(Config.notification.ledTxPin, HIGH);
        if (Config.notification.buzzerActive && Config.notification.txBeep) NOTIFICATION_Utils::beaconTxBeep();
        #ifdef HAS_SX126X
        int state = radio.transmit("\x3c\xff\x01" + newPacket);
        if (state == RADIOLIB_ERR_NONE) {
            //Serial.println(F("success!"));
        } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
            Serial.println(F("too long!"));
        } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
            Serial.println(F("timeout!"));
        } else {
            Serial.print(F("failed, code "));
            Serial.println(state);
        }
        #endif
        #ifdef HAS_SX127X
        LoRa.beginPacket();
        LoRa.write('<');
        LoRa.write(0xFF);
        LoRa.write(0x01);
        LoRa.write((const uint8_t *)newPacket.c_str(), newPacket.length());
        LoRa.endPacket();
        #endif
        if (Config.notification.ledTx) digitalWrite(Config.notification.ledTxPin, LOW);
        if (Config.ptt.active) {
            delay(Config.ptt.postDelay);
            digitalWrite(Config.ptt.io_pin, Config.ptt.reverse ? HIGH : LOW);
        }
        #ifdef HAS_TFT
        cleanTFT();
        #endif
    }

    ReceivedLoRaPacket receivePacket() {
        ReceivedLoRaPacket receivedLoraPacket;
        String packet = "";
        #ifdef HAS_SX127X
        int packetSize = LoRa.parsePacket();
        if (packetSize) {
            while (LoRa.available()) {
                int inChar = LoRa.read();
                packet += (char)inChar;
            }
            receivedLoraPacket.text       = packet;
            receivedLoraPacket.rssi       = LoRa.packetRssi();
            receivedLoraPacket.snr        = LoRa.packetSnr();
            receivedLoraPacket.freqError  = LoRa.packetFrequencyError();
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Rx", "---> %s", packet.c_str());
        }
        #endif
        #ifdef HAS_SX126X
        if (transmissionFlag) {
            transmissionFlag = false;
            radio.startReceive();
            int state = radio.readData(packet);
            if (state == RADIOLIB_ERR_NONE) {
                if(!packet.isEmpty()) {
                    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "LoRa Rx","---> %s", packet.c_str());
                }
                receivedLoraPacket.text       = packet;
                receivedLoraPacket.rssi       = radio.getRSSI();
                receivedLoraPacket.snr        = radio.getSNR();
                receivedLoraPacket.freqError  = radio.getFrequencyError();
            } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
                // timeout occurred while waiting for a packet
            } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
                Serial.println(F("CRC error!"));
            } else {
                Serial.print(F("failed, code "));
                Serial.println(state);
            }
        }
        #endif
        return receivedLoraPacket;
    }
}

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
        auto const parsed_lng = parse_coord(info.latitude, 'E', 'W');

        auto const buffer_offset = snprintf(buffer, buffer_size,
            ";%-9s%c"                  // name and status
            "%02hhd%02hhd.%02hhd%c"    // latitude
            "%c"                       // overlay
            "%03hhd%02hhd.%02hhd%c"    // longtitude
            "%c"                       // symbol
            "%s",                      // comment
            info.name.c_str(),
            info.is_live ? '*' : '_',  // is not killed?
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
        if (buffer_offset == sizeof(buffer))
            return TooLong;
        show_display({"[TX]", buffer});
        LoRa_Utils::sendNewPacket(buffer);
        return Success;
    }
}