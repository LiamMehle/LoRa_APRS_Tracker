#include <TinyGPS++.h>
#include <esp_bt.h>
#include "bluetooth_utils.h"
#include "configuration.h"
#include "KISS_TO_TNC2.h"
#include "lora_utils.h"
#include "display.h"
#include "logger.h"
#include "object.hpp"

extern Configuration    Config;
extern BluetoothSerial  SerialBT;
extern logging::Logger  logger;
extern TinyGPSPlus      gps;
extern bool             bluetoothConnected;
extern bool             bluetoothActive;

extern int myBeaconsIndex;
extern Beacon *currentBeacon;
extern char object_overlay;
extern char object_symbol;
extern char object_name[10];
extern char callsign[10];

// possible optimization: prefix tree lookup and generation at comp time
static
void process_command(char* cmd) {
    using Func = void (*)(char const*);
    struct Mapping { char cmd[16]; Func f; };
    Mapping commands[] = {
        {{"cs"        }, [](char const* const parameters){ currentBeacon->callsign = String(parameters); strcpy(callsign, parameters); }},
        {{"symbol"    }, [](char const* const parameters){ currentBeacon->symbol = String(parameters);                                 }},
        {{"obj"       }, [](char const* const parameters){ APRS::object::place();                                                      }},
        {{"objrep"    }, [](char const* const parameters){ APRS::object::retransmit_all();                                             }},
        {{"objpop"    }, [](char const* const parameters){ APRS::object::remove_last();                                                }},
        {{"objclear"  }, [](char const* const parameters){ APRS::object::remove_all();                                                 }},
        {{"objname"   }, [](char const* const parameters){ snprintf(object_name, sizeof(object_name), "%9s", parameters); show_display("object name", object_name); sleep(1); }},
        {{"objsymbol" }, [](char const* const parameters){ object_symbol  = *parameters; char object_symbol_str[3] = {object_overlay, object_symbol, 0}; show_display("obj sym", object_symbol_str); sleep(1); }},
        {{"objoverlay"}, [](char const* const parameters){ object_overlay = *parameters; char object_symbol_str[3] = {object_overlay, object_symbol, 0}; show_display("obj sym", object_symbol_str); sleep(1); }},
    };
    char action[8];
    int parameter_offset;
    int const parse_status = sscanf(cmd, " %8[^ ] %n", action, &parameter_offset);
    if (parse_status == 0)
        return;
    
    char* parameters = cmd + parameter_offset;

    for (char* p=action; *p != 0; p++)
        *p = isupper(*p) ? tolower(*p) : *p;
    for (char* p=parameters; *p != 0; p++)
        *p = islower(*p) ? toupper(*p) : *p;
    for (auto command : commands) {
        if (strcmp(command.cmd, action) == 0) {
            command.f(parameters);
        }
    }
}

namespace BLUETOOTH_Utils {
    String serialReceived;
    bool shouldSendToLoRa = false;
    bool useKiss = false;

    void setup() {
        if (!bluetoothActive) {
            btStop();
            esp_bt_controller_disable();
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Main", "BT controller disabled");
            return;
        }

        serialReceived.reserve(255);

        SerialBT.register_callback(BLUETOOTH_Utils::bluetoothCallback);
        SerialBT.onData(BLUETOOTH_Utils::getData); // callback instead of while to avoid RX buffer limit when NMEA data received

        uint8_t dmac[6];
        esp_efuse_mac_get_default(dmac);
        char ourId[5];
        snprintf(ourId, sizeof(ourId), "%02x%02x", dmac[4], dmac[5]);

        if (!SerialBT.begin(String("LoRa Tracker " + String(ourId)))) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Bluetooth", "Starting Bluetooth failed!");
            show_display("ERROR", "Starting Bluetooth failed!");
            while(true) {
                delay(1000);
            }
        }
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Bluetooth", "Bluetooth init done!");
    }

    void bluetoothCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
        if (event == ESP_SPP_SRV_OPEN_EVT) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Bluetooth", "Client connected !");
            bluetoothConnected = true;
            useKiss = false;
        } else if (event == ESP_SPP_CLOSE_EVT) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Bluetooth", "Client disconnected !");
            bluetoothConnected = false;
        } else {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Bluetooth", "Status: %d", event);
        }
    }

    void getData(const uint8_t *buffer, size_t size) {
        if (size == 0) {
            return;
        }
        shouldSendToLoRa = false;
        serialReceived.clear();
        bool isNmea = buffer[0] == '$';
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "bluetooth", "Received buffer size %d. Nmea=%d. %s", size, isNmea, buffer);

        for (int i = 0; i < size; i++) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "bluetooth", "[%d/%d] %x -> %c", i + 1, size, buffer[i], buffer[i]);
        }
        for (int i = 0; i < size; i++) {
            char c = (char) buffer[i];
            if (isNmea) {
                gps.encode(c);
            } else {
                serialReceived += c;
            }
        }
        // Test if we have to send frame
        isNmea = serialReceived.indexOf("$G") != -1 || serialReceived.indexOf("$B") != -1;
        if (isNmea) useKiss = false;
        if (isNmea || serialReceived.isEmpty()) return;
        if (validateKISSFrame(serialReceived)) {
            bool dataFrame;
            String decodeKiss = decode_kiss(serialReceived, dataFrame);
            serialReceived.clear();
            serialReceived += decodeKiss;
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "bluetooth", "It's a kiss frame. dataFrame: %d", dataFrame);
            useKiss = true;
        } else {
            useKiss = false;
        }
        if (validateTNC2Frame(serialReceived)) {
            shouldSendToLoRa = true;
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "bluetooth", "Data received should be transmitted to RF => %s", serialReceived.c_str());
        }
    }

    void sendToLoRa() {
        if (!shouldSendToLoRa || serialReceived.length() == 0) {
            return;
        }
        char address[10] = {0};
        char content[257] = {0};
        sscanf(serialReceived.c_str(), "%*[^:]::%9[^ :] :%256[^{]", address, content);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "cmd", "%s: %s", address, content);
        if (strcmp(address, "SYSTEM") == 0) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "cmd", "match, command=\"%s\"", content);
            process_command(content);
            shouldSendToLoRa = false;
            serialReceived.clear();
            return;
        }

        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BT TX", "%s", serialReceived.c_str());
        show_display("BT Tx >>", "", serialReceived, 1000);
        LoRa_Utils::sendNewPacket(serialReceived);
        shouldSendToLoRa = false;
    }

    void sendPacket(const String& packet) {
        if (bluetoothActive && !packet.isEmpty()) {
            if (useKiss) {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BT RX Kiss", "%s", serialReceived.c_str());
                SerialBT.println(encode_kiss(packet));
            } else {
                logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BT RX TNC2", "%s", serialReceived.c_str());
                SerialBT.println(packet);
            }
        }
    }
  
}