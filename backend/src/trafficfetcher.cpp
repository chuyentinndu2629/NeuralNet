#include "trafficfetcher.hpp"

#include <libwebsockets.h>
#include <vector>

Console* Fetcher::g_logConsole = nullptr; // Gotta initialize this static variable to nullptr
                                          // cuz just declaring it in the class isnt enough and
                                          // its already UNDEFINED

Fetcher::Fetcher(const std::string& __ais_stream_api_key, Console& con, const bool& nocompression) 
: __ais_stream_api_key(__ais_stream_api_key), console(con), nocompression(nocompression) {

    g_logConsole = &console;

    ccinfo.opaque_user_data = this;

    lws_set_log_level(LLL_ERR | LLL_WARN | LLL_USER, Fetcher::lws_log_emit);

    console.log("Initialized Fetcher", DEBUG_OK);

    aisPayload = json::object();
    aisPayload["APIKey"] = __ais_stream_api_key;
    aisPayload["BoundingBoxes"] = json::parse("[[[23.4, 102.1], [5.5, 115.5]]]");

    console.log("Loaded JSON payload: " + aisPayload.dump(), DEBUG_OK);

    rxBuffer.clear();
}

Fetcher::~Fetcher() {
    // Destructor
    console.log("Destructed Fetcher", DEBUG_OK);
}

void Fetcher::lws_log_emit(int level, const char *line) {
    if (!g_logConsole) return; // If the console is not set, just return

    std::string logLine(line);
    if (!logLine.empty() && logLine.back() == '\n') logLine.pop_back();
    g_logConsole->log("LWS: " + logLine, DEBUG_INFO);
}

void Fetcher::run() {
    console.log("Running Fetcher");
    stopped = false;

    console.log("LWS: run(): Creating context");
    // Set up connections
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    if (!nocompression) info.extensions = extensions; // Enable some extensions
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT; // Enable SSL globally

    context = lws_create_context(&info);
    if (!context) {
        console.log("LWS: run(): Failed to create context", DEBUG_FAIL);
        return;
    }

    console.log("LWS: run(): Creating client connection");
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.opaque_user_data = this;
    ccinfo.address = "stream.aisstream.io";
    ccinfo.port = 443;
    ccinfo.path = "/v0/stream";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = protocols[0].name;
    ccinfo.ssl_connection = LCCSCF_USE_SSL;

    if (!lws_client_connect_via_info(&ccinfo)) {
        console.log("LWS: run(): Failed to create client connection", DEBUG_FAIL);
        lws_context_destroy(context);
        return;
    }

    while (!stopped) {
        lws_service(context, 1000); /* Pump the event loop, 1 seconds timeout */
    }

    lws_context_destroy(context);
    console.log("LWS: run(): Context destroyed, exiting run()");
}

int Fetcher::callback_ws(struct lws *wsi, enum lws_callback_reasons reason,
                                void *user, void *in, size_t len) {
    Fetcher* fetcher = static_cast<Fetcher*>(lws_get_opaque_user_data(wsi));
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            // Connection has been established
            fetcher->console.log("LWS: callback_ws(): Connection established. Requesting writable");
            lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE: {
            // Data has been received
            // fetcher->console.log("LWS: callback_ws(): Data received: " + std::string(static_cast<char*>(in), len));
            fetcher->rxBuffer.append(static_cast<char*>(in), len);

            if (lws_is_final_fragment(wsi)) {
                // fetcher->console.log("LWS: callback_ws(): Data received: " + fetcher->rxBuffer);
                
                json data = json::parse(fetcher->rxBuffer);

                fetcher->rxBuffer.clear();
                fetcher->processAISData(data);
            }
            break;
        }
            
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            // Writable
            std::string aisPayloadStr = fetcher->aisPayload.dump();
            std::vector<unsigned char> buf(LWS_PRE + aisPayloadStr.size());

            memcpy(buf.data() + LWS_PRE, aisPayloadStr.data(), aisPayloadStr.size());

            lws_write(wsi, buf.data() + LWS_PRE, aisPayloadStr.size(), LWS_WRITE_TEXT);
            break;
        }

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
            // Connection error
            std::string errorMsg = in ? (char *)in : "Unknown";

            fetcher->console.log("LWS: callback_ws(): Connection error: " + errorMsg, DEBUG_FAIL);
            break;
        }

        case LWS_CALLBACK_CLOSED:
            // Connection closed
            fetcher->console.log("LWS: callback_ws(): Connection closed");
            break;

        default:
            break;
    }

    return 0;
}

void Fetcher::stop() {
    stopped = true;
}

void Fetcher::processAISData(const json& data) {
    if (data["MessageType"] == "SubscriptionConfirmation") {
        // This has NO metadata.
        // And also, this is just a confirmation that the connection has been established (for real this time) sucessfully.
        if (data["Message"]["CompressionEnabled"].get<bool>() == false) {
            console.log("Compression is not enabled, confirmed from endpoint. Data usage may increase.", DEBUG_WARN);
        } else {
            console.log("Compression enabled and confirmed from endpoint.", DEBUG_OK);
        }
    } else {
        // As long as it ISNT SubscriptionConfirmation. SubscriptionConfirmation DOES NOT have any metadata.
        // Therefore, it is USELESS except confirming that the connection has been made

        // Now, MetaData EXISTS, therefore, I will update the metadata to the map containing these things' data
        unsigned int mmsi = data["MetaData"]["MMSI"].get<unsigned int>();
        ship currentShip;
        currentShip.mmsi = mmsi;

        if (aisVehicles.count(mmsi)) {
            currentShip = aisVehicles[mmsi];

            console.log("New ship discovered: " + data["MetaData"]["ShipName"].get<std::string>());
        }

        currentShip.lat = data["MetaData"]["latitude"].get<double>();
        currentShip.lon = data["MetaData"]["longitude"].get<double>();

        currentShip.lastUpdated = static_cast<unsigned long>(parseTimestamp(data["MetaData"]["time_utc"].get<std::string>()));
    }
}