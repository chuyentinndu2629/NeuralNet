#pragma once

#include "console.h"

#include <string>
#include <libwebsockets.h>
#include <nlohmann/json.hpp>

using nlohmann::json;

class Fetcher {
    public:
    // Fetcher/Websockets manager for AIS Stream and ADSB.LOL
    Fetcher(const std::string& __ais_stream_api_key, Console& con);
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

    // Buffer for websocket receive fragments
    std::string rxBuffer;

    struct lws_context_creation_info info;
    struct lws_client_connect_info ccinfo;
    struct lws_context *context;

    bool stopped = false;

    // I am gonna smuggle CONSOLE into a static void function from LWS because the standard logging
    // is way too ass
    static Console* g_logConsole;

    static void lws_log_emit(int level, const char *line);
};

static const struct lws_protocols protocols[] = {
    {
        "ais-stream",
        Fetcher::callback_ws,
        0, 128
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