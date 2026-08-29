#pragma once

#include "console.hpp"

#include <string>
#include <libwebsockets.h>
#include <nlohmann/json.hpp>
#include <chrono>

using nlohmann::json;

// AIS exclusive
struct ship {
    unsigned int mmsi = -1; // Contains ALL 9 digits MMSI
    std::string shipName = "";

    double lat = 0, lon = 0; // latitude, longtitude.
    uint8_t navStatus = 15; // navigational status (default to UNDEFINED)

    int rot = 0; // rate of turn. i dont fucking know how this works. but thats GODOT's problem
    double sog = 0; // speed over ground (knots)
    double cog = 0; // course over ground (degrees)
    int trueHeading = 0; // true heading (degrees)

    unsigned long lastUpdated = 0; // last updated timestamp in UNIX
    
    bool positionAccuracy = 0;
    int specialManoeuvre = 0; // indicator if the vehicle (ship) is currently engaged in a special maneuvering operation 
                              // that is high risk, non standard, or specific.
    bool raim; // Receiver autonomous integrity monitoring flag
    uint8_t fixType = 0; // Fix type / Positioning method type
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
    Fetcher(const std::string& __ais_stream_api_key, Console& con, const bool& nocompression = false);
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

    // Buffer for websocket receive fragments
    std::string rxBuffer;

    struct lws_context_creation_info info;
    struct lws_client_connect_info ccinfo;
    struct lws_context *context;

    bool stopped = false;

    // AIS received data processing

    // Vehicles hashmap, containing the vehicles and their respective metadatas
    std::map<unsigned int, ship> aisVehicles; // MMSI, ship data

    // This function will process the received data from LWS that we got from AIS Stream.
    // Thank god AIS Stream exists.
    void processAISData(const json& data);

    // I am gonna smuggle CONSOLE into a static void function from LWS because the standard logging
    // is way too ass and cannot be accessed straight from a static function (lws_log_emit)
    static Console* g_logConsole;

    static void lws_log_emit(int level, const char *line);
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