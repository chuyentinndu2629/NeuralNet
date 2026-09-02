#include "trafficfetcher.hpp"
#include "console.hpp"

#include <libwebsockets.h>
#include <string>
#include <vector>
#include <fstream>

// We need some debug about buffers
// #include <iostream>

Console* Fetcher::g_logConsole = nullptr; // Gotta initialize this static variable to nullptr
                                          // cuz just declaring it in the class isnt enough and
                                          // its already UNDEFINED

Fetcher::Fetcher(const std::string& __ais_stream_api_key, Console& con, const bool& nocompression, const fs::path& savedDiscoveryPath) 
: __ais_stream_api_key(__ais_stream_api_key), console(con), nocompression(nocompression), savedDiscoveryPath(savedDiscoveryPath) {

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

Fetcher::~Fetcher() { // TODO: We got save, gotta read them too.
    // Destructor
    // Let me save the discovered set here.
    std::ofstream file(savedDiscoveryPath);
    json discoveredJson;

    for (const std::pair<const unsigned int, ship>& vesselEntry : aisVessels) {
        json currentShipJson;

        currentShipJson["MMSI"]       = vesselEntry.second.mmsi;
        currentShipJson["IMO"]        = vesselEntry.second.imo;
        currentShipJson["AISVersion"] = vesselEntry.second.aisVersion;
        currentShipJson["Name"]       = vesselEntry.second.shipName;
        currentShipJson["CallSign"]   = vesselEntry.second.callSign;
        currentShipJson["Type"]       = vesselEntry.second.shipType;

        currentShipJson["Dimension"]["Length"] = vesselEntry.second.dimension.length;
        currentShipJson["Dimension"]["Beam"]   = vesselEntry.second.dimension.beam;

        currentShipJson["ETA"]["Month"]  = vesselEntry.second.eta.month;
        currentShipJson["ETA"]["Day"]    = vesselEntry.second.eta.day;
        currentShipJson["ETA"]["Hour"]   = vesselEntry.second.eta.hour;
        currentShipJson["ETA"]["Minute"] = vesselEntry.second.eta.minute;

        currentShipJson["Destination"] = vesselEntry.second.dest;

        currentShipJson["Latitude"]             = vesselEntry.second.lat;
        currentShipJson["Longtitude"]           = vesselEntry.second.lon;
        currentShipJson["NavigationalStatus"]   = vesselEntry.second.navStatus;
        currentShipJson["MaximumStaticDrought"] = vesselEntry.second.maximumStaticDrought;

        currentShipJson["ROT"]         = vesselEntry.second.rot;
        currentShipJson["SOG"]         = vesselEntry.second.sog;
        currentShipJson["COG"]         = vesselEntry.second.cog;
        currentShipJson["TrueHeading"] = vesselEntry.second.trueHeading;

        currentShipJson["LastUpdated"] = vesselEntry.second.lastUpdated;

        currentShipJson["PositionAccuracy"] = vesselEntry.second.positionAccuracy;
        currentShipJson["SpecialManoeuvre"] = vesselEntry.second.specialManoeuvre;
        currentShipJson["Raim"]             = vesselEntry.second.raim;
        currentShipJson["FixType"]          = vesselEntry.second.fixType;
        currentShipJson["DTE"]              = vesselEntry.second.dte;

        currentShipJson["ClassB"]["Unit"]           = vesselEntry.second.classB.unit;
        currentShipJson["ClassB"]["Display"]        = vesselEntry.second.classB.display;
        currentShipJson["ClassB"]["DSC"]            = vesselEntry.second.classB.dsc;
        currentShipJson["ClassB"]["Band"]           = vesselEntry.second.classB.band;
        currentShipJson["ClassB"]["Msg22"]          = vesselEntry.second.classB.msg22;
        currentShipJson["ClassB"]["VendorIDName"]   = vesselEntry.second.classB.vendorIDName;
        currentShipJson["ClassB"]["VendorIDModel"]  = vesselEntry.second.classB.vendorIDModel;
        currentShipJson["ClassB"]["vendorIDSerial"] = vesselEntry.second.classB.vendorIDSerial;

        currentShipJson["AssignedMode"] = vesselEntry.second.assignedMode;

        discoveredJson[std::to_string(vesselEntry.first)] = currentShipJson;
    }
    
    if (!file.is_open()) {
        console.log("Cannot open file at " + savedDiscoveryPath.string(), DEBUG_FAIL);
        goto done;
    }
    
    file << discoveredJson.dump();
    console.log("Saved discovered set to " + savedDiscoveryPath.string(), DEBUG_OK);
    file.close();

    done:
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
    stopped.store(false);

    // 1. Setup and Create Context ONCE outside the loop
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    if (!nocompression) info.extensions = extensions;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    context = lws_create_context(&info);
    if (!context) {
        console.log("LWS: run(): Failed to create context. Fatal error.", DEBUG_FAIL);
        return; // Cannot proceed without a context
    }

    while (!stopped.load()) {
        reconnectRequested.store(false);
        resetActivityTimer();

        // 2. Setup Client Connection only
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
            console.log("LWS: run(): Failed to initiate client connection. Retrying in 2 seconds...", DEBUG_FAIL);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        // 3. Event Loop
        while (!stopped.load() && !reconnectRequested.load()) {
            lws_service(context, 250);

            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - lastActivityTime
            ).count();

            if (duration >= MESSAGE_TIMEOUT) {
                console.log("LWS: Timeout! No network activity detected. Triggering reconnect...", DEBUG_WARN);
                reconnectRequested.store(true);
            }
        }

        rxBuffer.clear();

        // NOTE: We no longer destroy the context here! We just loop and reconnect.

        if (!stopped.load()) {
            console.log("LWS: Reconnecting to AIS Stream...", DEBUG_INFO);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // 4. Cleanup context ONCE when the thread permanently stops
    if (context) {
        lws_context_destroy(context);
        context = nullptr;
    }

    console.log("LWS: run(): Stopped permanently, exiting run()");
}

int Fetcher::callback_ws(struct lws *wsi, enum lws_callback_reasons reason,
                                void *user, void *in, size_t len) {
    Fetcher* fetcher = static_cast<Fetcher*>(lws_get_opaque_user_data(wsi));
    if (!fetcher) return 0;

    // Refresh active connection timestamp on valid callbacks
    fetcher->resetActivityTimer();

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            fetcher->console.log("LWS: Connection established (wsi=" + std::to_string((uintptr_t)wsi) + ")");
            lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE: {
            fetcher->rxBuffer.append(static_cast<char*>(in), len);

            if (lws_is_final_fragment(wsi)) {
                try {
                    json data = json::parse(fetcher->rxBuffer);
                    fetcher->processAISData(data);
                } catch (const nlohmann::json::exception& e) {
                    fetcher->console.log(std::string("JSON error: ") + e.what(), DEBUG_FAIL);
                    // Force reconnect on corrupted payload
                    // fetcher->reconnectRequested.store(true);
                    // lws_cancel_service(fetcher->context);
                } catch (const std::exception& e) {
                    fetcher->console.log(std::string("Unexpected error: ") + e.what(), DEBUG_FAIL);
                    // fetcher->reconnectRequested.store(true);
                    // lws_cancel_service(fetcher->context);
                }

                fetcher->rxBuffer.clear();
            }
            break;
        }
            
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            std::string aisPayloadStr = fetcher->aisPayload.dump();
            std::vector<unsigned char> buf(LWS_PRE + aisPayloadStr.size());
            memcpy(buf.data() + LWS_PRE, aisPayloadStr.data(), aisPayloadStr.size());
            lws_write(wsi, buf.data() + LWS_PRE, aisPayloadStr.size(), LWS_WRITE_TEXT);
            break;
        }

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
            std::string errorMsg = in ? (char *)in : "Unknown";
            fetcher->console.log("LWS: callback_ws(): Connection error: " + errorMsg, DEBUG_FAIL);
            fetcher->reconnectRequested.store(true);
            lws_cancel_service(fetcher->context);
            break;
        }

        case LWS_CALLBACK_CLOSED:
            fetcher->console.log("LWS: callback_ws(): Connection closed");
            fetcher->reconnectRequested.store(true);
            break;

        default:
            break;
    }

    return 0;
}

void Fetcher::stop() {
    stopped.store(true);
    if (context) {
        lws_cancel_service(context);
    }
}

void Fetcher::processAISData(const json& data) {
    if (data["MessageType"] == "SubscriptionConfirmation") {
        // This has NO metadata.
        // And also, this is just a confirmation that the connection has been established (for real this time) sucessfully.
        if (data["Message"]["CompressionEnabled"].get<bool>() == false) {
            console.log("Compression is not enabled, confirmed from endpoint. Starting from Sep 2026, uncompressed connections will be subject to per-user bandwidth limits, and messages exceeding those limits will be dropped (by AIS Stream). Data usage may increase.", DEBUG_WARN);
        } else {
            console.log("Compression enabled and confirmed from endpoint.", DEBUG_OK);
        }
    } else if (data["MessageType"] == "UnknownMessage") {
        console.log("Message cannot be processed at endpoint. Error: " + data["Message"]["Error"].get<std::string>(), DEBUG_FAIL);
    } else {
        // If we are talking about a station right now, we ignore it, that's for the vessels, we ARE NOT a vessel
        if (data["MessageType"] == "BaseStationReport") {
            return;
        }

        // As long as it ISNT SubscriptionConfirmation. SubscriptionConfirmation DOES NOT have any metadata.
        // Therefore, it is USELESS except confirming that the connection has been made

        // Now, MetaData EXISTS, therefore, I will update the metadata to the map containing these things' data
        unsigned int mmsi = data["MetaData"]["MMSI"].get<unsigned int>();
        console.log("Received " + data["MessageType"].get<std::string>() + " from " + std::to_string(mmsi));

        ship currentShip;
        currentShip.mmsi = mmsi;

        if (aisVessels.count(mmsi)) {
            currentShip = aisVessels[mmsi];
        } else {
            console.log("New ship discovered: " + data["MetaData"]["ShipName"].get<std::string>() + " (" + std::to_string(mmsi) + ")", DEBUG_OK);
        }

        currentShip.lat = data["MetaData"]["latitude"].get<double>();
        currentShip.lon = data["MetaData"]["longitude"].get<double>();

        // We use this since 
        currentShip.lastUpdated = static_cast<unsigned long>(parseTimestamp(data["MetaData"]["time_utc"].get<std::string>()));

        // Now to the main course
        json message = data["Message"][data["MessageType"]];
        if (!message["Valid"].get<bool>()) return; // Invalid data, skip processing

        if (data["MessageType"] == "PositionReport") {
            currentShip.mmsi = message["UserID"].get<unsigned int>();

            currentShip.navStatus = message["NavigationalStatus"];
            currentShip.rot = message["RateOfTurn"];
            currentShip.sog = message["Sog"];
            currentShip.cog = message["Cog"];
            currentShip.trueHeading = message["TrueHeading"];

            currentShip.specialManoeuvre = message["SpecialManoeuvreIndicator"];
            currentShip.raim = message["Raim"];

            currentShip.lon = message["Longitude"];
            currentShip.lat = message["Latitude"];

            currentShip.positionAccuracy = message["PositionAccuracy"];
            // currentShip.fixType = message["FixType"]

        } else if (data["MessageType"] == "StandardSearchAndRescueAircraftReport") {
            // TODO: When integrating ADS-B, add this thing's functionality too.
        } else if (data["MessageType"] == "StandardClassBPositionReport") {
            // Standard position report for Class B shipborne mobile equipment to be used instead of Messages 1, 2, 3 (https://www.navcen.uscg.gov/ais-messages)
            currentShip.mmsi = message["UserID"];

            currentShip.navStatus = 15;
            currentShip.rot = 0;
            currentShip.sog = message["Sog"];
            currentShip.cog = message["Cog"];
            currentShip.trueHeading = message["TrueHeading"];

            // currentShip.specialManoeuvre = 0;
            currentShip.raim = message["Raim"];

            currentShip.lon = message["Longitude"];
            currentShip.lat = message["Latitude"];

            currentShip.positionAccuracy = message["PositionAccuracy"];

            // Class B exclusive data reports. If a ship with a specific MMSI do this and "ExtendedClassBPositionReport". It IS definitely using
            // class B shipborne mobile equipment (which is lightweight as hell (on the power side of things))
            currentShip.classB.unit = message["ClassBUnit"];
            currentShip.classB.display = message["ClassBDisplay"];
            currentShip.classB.dsc = message["ClassBDsc"];
            currentShip.classB.band = message["ClassBBand"];
            currentShip.classB.msg22 = message["ClassBMsg22"];

            // Now onto assignedmode flag
            currentShip.assignedMode = message["AssignedMode"];
        } else if (data["MessageType"] == "ExtendedClassBPositionReport") {
            // Extended position report for Class B shipborne mobile equipment
            currentShip.mmsi = message["UserID"];

            currentShip.navStatus = 15;
            currentShip.rot = 0;
            currentShip.sog = message["Sog"];
            currentShip.cog = message["Cog"];
            currentShip.trueHeading = message["TrueHeading"];

            // currentShip.specialManoeuvre = 0;
            currentShip.raim = message["Raim"];

            currentShip.lon = message["Longitude"];
            currentShip.lat = message["Latitude"];

            currentShip.positionAccuracy = message["PositionAccuracy"];

            // Extended class B data reports, these and the above over at StandardClassBPositionReport are NOT the same
            currentShip.shipName = message["Name"];
            currentShip.shipType = message["Type"];
            currentShip.fixType = message["FixType"];
            currentShip.dte = message["Dte"];

            // Dimensions will fallback to 0 if a key is MISSIN'
            currentShip.dimension.length = message["Dimension"].value("A", 0) + message["Dimension"].value("B", 0);
            currentShip.dimension.beam   = message["Dimension"].value("C", 0) + message["Dimension"].value("D", 0);

            currentShip.assignedMode = message["AssignedMode"];
        } else if (data["MessageType"] == "LongRangeAisBroadcastMessage") {
            // Class A and Class B "SO" shipborne mobile equipment outside base station coverage
            currentShip.mmsi = message["UserID"];

            currentShip.navStatus = message["NavigationalStatus"];
            currentShip.sog = message["Sog"];
            currentShip.cog = message["Cog"];

            currentShip.raim = message["Raim"];

            currentShip.lon = message["Longitude"];
            currentShip.lat = message["Latitude"];

            currentShip.positionAccuracy = message["PositionAccuracy"];

            if (message["PositionLatency"].get<bool>()) {
                console.log("Ship with MMSI " + std::to_string(currentShip.mmsi) + " has position latency over long range AIS message.", DEBUG_WARN);
            }
        } else if (data["MessageType"] == "ShipStaticData") {
            currentShip.mmsi = message["UserID"];

            currentShip.aisVersion = message["AisVersion"];
            currentShip.imo = message["ImoNumber"];
            currentShip.callSign = message["CallSign"];
            currentShip.shipName = message["Name"];
            currentShip.shipType = message["Type"];
            
            currentShip.fixType = message["FixType"];

            currentShip.dimension.length = message["Dimension"].value("A", 0) + message["Dimension"].value("B", 0);
            currentShip.dimension.beam   = message["Dimension"].value("C", 0) + message["Dimension"].value("D", 0);

            currentShip.eta.month  = message["Eta"]["Month"];
            currentShip.eta.day    = message["Eta"]["Day"];
            currentShip.eta.hour   = message["Eta"]["Hour"];
            currentShip.eta.minute = message["Eta"]["Minute"];

            currentShip.maximumStaticDrought = message["MaximumStaticDraught"];
            currentShip.dest = message["Destination"];
            currentShip.dte = message["Dte"];
        } else if (data["MessageType"] == "StaticDataReport") {
            currentShip.mmsi = message["UserID"];

            std::string targetReport;
            if (message["PartNumber"]) targetReport = "ReportB";
            else                       targetReport = "ReportA";

            if (!message[targetReport]["Valid"].get<bool>()) return;

            currentShip.shipType = message[targetReport]["ShipType"];
            currentShip.callSign = message[targetReport]["CallSign"];

            currentShip.fixType = message[targetReport]["FixType"];

            currentShip.classB.vendorIDName   = message[targetReport]["VendorIDName"];
            currentShip.classB.vendorIDModel  = message[targetReport]["VenderIDModel"];
            currentShip.classB.vendorIDSerial = message[targetReport]["VenderIDSerial"];

            currentShip.dimension.length = message[targetReport]["Dimension"].value("A", 0) + message[targetReport]["Dimension"].value("B", 0);
            currentShip.dimension.beam   = message[targetReport]["Dimension"].value("C", 0) + message[targetReport]["Dimension"].value("D", 0);
        } else {
            console.log("Received unknown message type: " + data["MessageType"].get<std::string>(), DEBUG_WARN);
        }

        // Saving into the discovered map
        aisVessels[mmsi] = currentShip;
    }
}