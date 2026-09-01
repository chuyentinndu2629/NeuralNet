#pragma once

#include "console.hpp"

#include <string>
#include <libwebsockets.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>

using nlohmann::json;
namespace fs = std::filesystem;

#define MESSAGE_TIMEOUT 10 // in seconds

// AIS exclusive
struct shipDimension {
    unsigned int length, beam; // beam is width
};

struct shipEta {
    int month = 0, day = 0, hour = 0, minute = 0;
};

struct classBData {
    bool unit = false; // signifies if this unit uses SOTDMA (false) or CSTDMA (true).
    bool display = false; // does this unit has a display to read incoming messages and things like that.
    bool dsc = false; // if the transponder is paired with a Digital Selective Calling marine radio
    bool band = false; // if the unit can toggle across the entire marine VHF band
    bool msg22 = false; // tells the station if this unit can accept frequency management commands (message 22)

    // because class B ships are usually not registered with an IMO number, we got these.
    std::string vendorIDName = "";
    std::string vendorIDModel = "";
    std::string vendorIDSerial = "";
};

struct ship {
    unsigned int mmsi = 0; // Contains ALL 9 digits MMSI. MMSI is the number identifying the ship's electronics
    unsigned int imo = 0; // Contain ALL 7 digits IMO. IMO is the number identifying the ship's physical body. Registered by class A ships.
    uint8_t aisVersion = 0; // AIS version field
    std::string shipName = "";
    std::string callSign = "";
    uint8_t shipType; // From 0 -> 99 (100 total) so it will fit in this 256 total data type.

    shipDimension dimension;
    shipEta eta; // Estimated Time of Arrival
    std::string dest; // destination

    double lat = 0, lon = 0; // latitude, longtitude.
    uint8_t navStatus = 15; // navigational status (default to UNDEFINED)
    double maximumStaticDrought = -1; // self explanatory

    int rot = 0; // rate of turn. i dont fucking know how this works. but thats GODOT's problem
    double sog = 0; // speed over ground (knots)
    double cog = 0; // course over ground (degrees)
    int trueHeading = 0; // true heading (degrees)

    unsigned long lastUpdated = 0; // last updated timestamp in UNIX
    
    bool positionAccuracy = 0;
    int specialManoeuvre = 0; // indicator if the vehicle (ship) is currently engaged in a special maneuvering operation 
                              // that is high risk, non standard, or specific.
    bool raim; // receiver autonomous integrity monitoring flag
    uint8_t fixType = 0; // fix type / positioning method type
    bool dte = false; // ready to receive messages (data terminal equipment)

    classBData classB;

    bool assignedMode = false; // an operational state where a competent authority, such as a VTS or shore-side base station, remotely controls a vessel's data transmission intervals and time slots.
};

// ADS-B exclusive
struct aircraft {

};

// Convert timestamps.
static double parseTimestamp(const std::string& s) {
    std::istringstream iss(s);
    std::chrono::sys_time<std::chrono::nanoseconds> tp;
    iss >> std::chrono::parse("%Y-%m-%d %H:%M:%S %z UTC", tp);
    if (iss.fail()) throw std::runtime_error("parse failed: " + s);
    return std::chrono::duration<double>(tp.time_since_epoch()).count();
}

class Fetcher {
    public:
    // Fetcher/Websockets manager for AIS Stream and ADSB.LOL
    Fetcher(const std::string& __ais_stream_api_key, Console& con, const bool& nocompression = false, const fs::path& savedDiscoveryPath = fs::current_path() / "dscv.json");
    ~Fetcher();

    void run(); // Run the main loop
    void stop(); // Stop the fetcher

    /* Called by lws for every event on the connection */
    static int callback_ws(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len);

    private:
    std::string __ais_stream_api_key;
    Console& console;
    json aisPayload;

    // Other flags for setting up and allat
    // e.g. nocompression
    bool nocompression;
    fs::path savedDiscoveryPath;

    // Buffer for websocket receive fragments
    std::string rxBuffer;

    struct lws_context_creation_info info;
    struct lws_client_connect_info ccinfo;
    struct lws_context *context;

    std::atomic<bool> stopped; // atomic cuz its being written by different threads

    // AIS received data processing

    // Vehicles hashmap, containing the vehicles and their respective metadatas
    std::map<unsigned int, ship> aisVessels; // MMSI, ship data

    // This function will process the received data from LWS that we got from AIS Stream.
    // Thank god AIS Stream exists.
    void processAISData(const json& data);

    // I am gonna smuggle CONSOLE into a static void function from LWS because the standard logging
    // is way too ass and cannot be accessed straight from a static function (lws_log_emit)
    static Console* g_logConsole;

    static void lws_log_emit(int level, const char *line);

    // Reconnection logic
    std::atomic<bool> reconnectRequested{false};
    std::chrono::steady_clock::time_point lastActivityTime;
    
    // Helper to refresh activity timer
    void resetActivityTimer() {
        lastActivityTime = std::chrono::steady_clock::now();
    }
};

static const struct lws_protocols protocols[] = {
    {
        "ais-stream",
        Fetcher::callback_ws,
        0, 65536
    },
    LWS_PROTOCOL_LIST_TERM
};

static const struct lws_extension extensions[] = {
    {
        "permessage-deflate",
        lws_extension_callback_pm_deflate, // Compression
        "permessage-deflate; client_no_context_takeover; client_max_window_bits"
    },
    { NULL, NULL, NULL }   // terminator, same idea as LWS_PROTOCOL_LIST_TERM
};