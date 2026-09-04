#include "trafficfetcher.hpp"
#include "console.hpp"

#include <libwebsockets.h>
#include <mutex>
#include <string>
#include <unordered_set>
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
    aisPayload["BoundingBoxes"] = json::parse("[[[-90, -180], [90, 180]]]");

    console.log("Loaded JSON payload: " + aisPayload.dump(), DEBUG_OK);

    loadDiscoveredData();

    // Reserving for 10^6 ships (even tho there are only 10^5 ships active, some overhead is always the best)
    pendingUpdates.reserve(1e6);

    rxBuffer.clear();
}

Fetcher::~Fetcher() { // TODO: We got save, gotta read them too.
    // Destructor
    saveDiscoveredData();

    console.log("Destructed Fetcher", DEBUG_OK);
}

void Fetcher::saveDiscoveredData() {
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
        currentShipJson["MaximumStaticDraught"] = vesselEntry.second.maximumStaticDraught;

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

        discoveredJson["AIS"][std::to_string(vesselEntry.first)] = currentShipJson;
    }
    
    if (!file.is_open()) {
        console.log("Cannot open file at " + savedDiscoveryPath.string(), DEBUG_FAIL);
        return;
    }
    
    file << discoveredJson.dump();
    console.log("Saved discovered set to " + savedDiscoveryPath.string(), DEBUG_OK);
    file.close();
}

void Fetcher::loadDiscoveredData() {
    std::ifstream file(savedDiscoveryPath);
    json discoveredJson;
    
    if (!file.is_open()) {
        console.log("Cannot open file at " + savedDiscoveryPath.string(), DEBUG_FAIL);
        return;
    }

    discoveredJson = json::parse(file);

    for (const auto& shipEntry : discoveredJson["AIS"].items()) {
        ship currentShip;

        currentShip.mmsi       = shipEntry.value()["MMSI"];
        currentShip.imo        = shipEntry.value()["IMO"];
        currentShip.aisVersion = shipEntry.value()["AISVersion"];
        currentShip.shipName   = shipEntry.value()["Name"];
        currentShip.callSign   = shipEntry.value()["CallSign"];
        currentShip.shipType   = shipEntry.value()["Type"];

        currentShip.dimension.length = shipEntry.value()["Dimension"]["Length"];
        currentShip.dimension.beam   = shipEntry.value()["Dimension"]["Beam"];

        currentShip.eta.month  = shipEntry.value()["ETA"]["Month"];
        currentShip.eta.day    = shipEntry.value()["ETA"]["Day"];
        currentShip.eta.hour   = shipEntry.value()["ETA"]["Hour"];
        currentShip.eta.minute = shipEntry.value()["ETA"]["Minute"];

        currentShip.dest = shipEntry.value()["Destination"];

        currentShip.lat                  = shipEntry.value()["Latitude"];
        currentShip.lon                  = shipEntry.value()["Longtitude"];
        currentShip.navStatus            = shipEntry.value()["NavigationalStatus"];
        currentShip.maximumStaticDraught = shipEntry.value()["MaximumStaticDraught"];

        currentShip.rot         = shipEntry.value()["ROT"];
        currentShip.sog         = shipEntry.value()["SOG"];
        currentShip.cog         = shipEntry.value()["COG"];
        currentShip.trueHeading = shipEntry.value()["TrueHeading"];

        currentShip.lastUpdated = shipEntry.value()["LastUpdated"];

        currentShip.positionAccuracy = shipEntry.value()["PositionAccuracy"];
        currentShip.specialManoeuvre = shipEntry.value()["SpecialManoeuvre"];
        currentShip.raim             = shipEntry.value()["Raim"];
        currentShip.fixType          = shipEntry.value()["FixType"];
        currentShip.dte              = shipEntry.value()["DTE"];

        currentShip.classB.unit           = shipEntry.value()["ClassB"]["Unit"];
        currentShip.classB.display        = shipEntry.value()["ClassB"]["Display"];
        currentShip.classB.dsc            = shipEntry.value()["ClassB"]["DSC"];
        currentShip.classB.band           = shipEntry.value()["ClassB"]["Band"];
        currentShip.classB.msg22          = shipEntry.value()["ClassB"]["Msg22"];
        currentShip.classB.vendorIDName   = shipEntry.value()["ClassB"]["VendorIDName"];
        currentShip.classB.vendorIDModel  = shipEntry.value()["ClassB"]["VendorIDModel"];
        currentShip.classB.vendorIDSerial = shipEntry.value()["ClassB"]["vendorIDSerial"];

        currentShip.assignedMode = shipEntry.value()["AssignedMode"];

        aisVessels[std::stoi(shipEntry.key())] = currentShip;
        
        markPending(currentShip.mmsi);
    }

    console.log("Parsed last session's discovered data.", DEBUG_OK);
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
                    fetcher->console.log(std::string("LWS: Received data JSON parsing error: ") + e.what() + ". Given string: " + fetcher->rxBuffer, DEBUG_FAIL);
                    // Force reconnect on corrupted payload
                    // fetcher->reconnectRequested.store(true);
                    // lws_cancel_service(fetcher->context);
                } catch (const std::exception& e) {
                    fetcher->console.log(std::string("LWS: Received data JSON parsing unexpected error: ") + e.what(), DEBUG_FAIL);
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
            console.log("AIS[LWS]: Compression is not enabled, confirmed from endpoint. Starting from Sep 2026, uncompressed connections will be subject to per-user bandwidth limits, and messages exceeding those limits will be dropped (by AIS Stream). Data usage may increase.", DEBUG_WARN);
        } else {
            console.log("AIS[LWS]: Compression enabled and confirmed from endpoint.", DEBUG_OK);
        }
    } else if (data["MessageType"] == "UnknownMessage") {
        console.log("AIS[LWS]: Message cannot be processed at endpoint. Error: " + data["Message"]["Error"].get<std::string>(), DEBUG_FAIL);
    } else {
        // If we are talking about a station right now, we ignore it, that's for the vessels, we ARE NOT a vessel
        if (data["MessageType"] == "BaseStationReport") {
            return;
        }

        // As long as it ISNT SubscriptionConfirmation. SubscriptionConfirmation DOES NOT have any metadata.
        // Therefore, it is USELESS except confirming that the connection has been made

        // Now, MetaData EXISTS, therefore, I will update the metadata to the map containing these things' data
        unsigned int mmsi = data["MetaData"]["MMSI"].get<unsigned int>();
        console.log("AIS[LWS]: Received " + data["MessageType"].get<std::string>() + " from " + std::to_string(mmsi));

        ship currentShip;
        currentShip.mmsi = mmsi;

        if (aisVessels.count(mmsi)) {
            currentShip = aisVessels[mmsi];
        } else {
            // console.log("New ship discovered: " + data["MetaData"]["ShipName"].get<std::string>() + " (" + std::to_string(mmsi) + ")", DEBUG_OK);
        }

        currentShip.lat = data["MetaData"]["latitude"].get<double>();
        currentShip.lon = data["MetaData"]["longitude"].get<double>();

        // We use this since 
        currentShip.lastUpdated = static_cast<unsigned long>(parseTimestamp(data["MetaData"]["time_utc"].get<std::string>()));

        pendingUpdates.insert(mmsi);

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
            if (!message["Dimension"]["A"].is_null() && !message["Dimension"]["B"].is_null()) 
                currentShip.dimension.length = message["Dimension"]["A"].get<unsigned int>() + message["Dimension"]["B"].get<unsigned int>();
            if (!message["Dimension"]["C"].is_null() && !message["Dimension"]["D"].is_null())
                currentShip.dimension.beam   = message["Dimension"]["C"].get<unsigned int>() + message["Dimension"]["D"].get<unsigned int>();

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

            if (!message["Dimension"]["A"].is_null() && !message["Dimension"]["B"].is_null()) 
                currentShip.dimension.length = message["Dimension"]["A"].get<unsigned int>() + message["Dimension"]["B"].get<unsigned int>();
            if (!message["Dimension"]["C"].is_null() && !message["Dimension"]["D"].is_null())
                currentShip.dimension.beam   = message["Dimension"]["C"].get<unsigned int>() + message["Dimension"]["D"].get<unsigned int>();

            currentShip.eta.month  = message["Eta"]["Month"];
            currentShip.eta.day    = message["Eta"]["Day"];
            currentShip.eta.hour   = message["Eta"]["Hour"];
            currentShip.eta.minute = message["Eta"]["Minute"];

            currentShip.maximumStaticDraught = message["MaximumStaticDraught"];
            currentShip.dest = message["Destination"];
            currentShip.dte = message["Dte"];
        } else if (data["MessageType"] == "StaticDataReport") {
            currentShip.mmsi = message["UserID"];

            std::string targetReport;
            if (message["PartNumber"]) {
                targetReport = "ReportB";
                
                if (!message[targetReport]["Valid"].get<bool>()) return;

                if (!message[targetReport]["ShipType"].is_null()) currentShip.shipType = message[targetReport]["ShipType"];
                if (!message[targetReport]["CallSign"].is_null()) currentShip.callSign = message[targetReport]["CallSign"];

                if (!message[targetReport]["FixType"].is_null()) currentShip.fixType = message[targetReport]["FixType"];

                if (!message[targetReport]["VendorIDName"].is_null())   currentShip.classB.vendorIDName   = message[targetReport]["VendorIDName"];
                if (!message[targetReport]["VenderIDModel"].is_null())  currentShip.classB.vendorIDModel  = message[targetReport]["VenderIDModel"];
                if (!message[targetReport]["VenderIDSerial"].is_null()) currentShip.classB.vendorIDSerial = message[targetReport]["VenderIDSerial"];

                if (!message[targetReport]["Dimension"]["A"].is_null() && !message[targetReport]["Dimension"]["B"].is_null()) 
                    currentShip.dimension.length = message[targetReport]["Dimension"]["A"].get<unsigned int>() + message[targetReport]["Dimension"]["B"].get<unsigned int>();
                if (!message[targetReport]["Dimension"]["C"].is_null() && !message[targetReport]["Dimension"]["D"].is_null())
                    currentShip.dimension.beam   = message[targetReport]["Dimension"]["C"].get<unsigned int>() + message[targetReport]["Dimension"]["D"].get<unsigned int>();
            } else {
                targetReport = "ReportA";

                if (!message[targetReport]["Valid"].get<bool>()) return;

                if (!message[targetReport]["Name"].is_null()) currentShip.shipName = message[targetReport]["Name"];
            }
        } else {
            console.log("AIS[LWS]: Received unknown message type: " + data["MessageType"].get<std::string>(), DEBUG_WARN);
        }

        // Saving into the discovered map
        aisVessels[mmsi] = currentShip;
    }
}

void Fetcher::markPending(const unsigned int& mmsi) {
    std::lock_guard<std::mutex> lock(pendingUpdatesMutex);
    pendingUpdates.insert(mmsi);
}

json Fetcher::getPending() {
    std::unique_lock<std::mutex> lock(pendingUpdatesMutex);

    // lock.lock(); unique_lock acts the same as std::lock_guard by default it seems? Tho unique_lock gives me more leeway
    // to modify it whenever I want with its lock() and unlock() methods.
    std::vector<unsigned int> pendings;

    pendings.reserve(1e6);
    std::unordered_set<unsigned int>::iterator it = pendingUpdates.begin();
    while (it != pendingUpdates.end()) {
        pendings.push_back(std::move(pendingUpdates.extract(it++).value()));
    }

    lock.unlock();

    // We got the vector now. We just need to create a json for it.
    json returnData;
    for (const unsigned int& mmsi : pendings) {
        json data;
        // data["MMSI"] = mmsi;
        data["Lon"] = aisVessels[mmsi].lon;
        data["Lat"] = aisVessels[mmsi].lat;

        returnData[std::to_string(mmsi)] = data;
    }

    return returnData;
}