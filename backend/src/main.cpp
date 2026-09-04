#include "console.hpp"
#include "socket.hpp"
#include "trafficfetcher.hpp"
#include "apikeys.hpp"
// #include "renderer.h"

#include "cpr/response.h"
#include <strings.h>
#include <cpr/cpr.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
using nlohmann::json;
// using asio::ip::tcp;

Console console;
fs::path dirPath;

#include "consts.hpp"

bool fetchMap() {
    cpr::Session session;

    // Define a callback function or lambda to print progress
    // Track the last time a progress update was printed
    auto lastUpdate = std::chrono::steady_clock::now();

    // Using a lambda to print progress since I am NOT downloading anything else at init.
    session.SetOption(cpr::ProgressCallback([&](cpr::cpr_off_t downloadTotal, 
                                                cpr::cpr_off_t downloadNow, 
                                                cpr::cpr_off_t uploadTotal, 
                                                cpr::cpr_off_t uploadNow, 
                                                intptr_t userdata) -> bool {
        if (downloadTotal > 0) {
            auto now = std::chrono::steady_clock::now();
            bool isComplete = (downloadNow == downloadTotal);
            
            // Update at most once every 100ms (10 FPS), or when the transfer completes
            if (isComplete || std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() >= 100) {
                double percentage = (static_cast<double>(downloadNow) / downloadTotal) * 100.0;
                
                std::stringstream output;
                output << "\rProgress: " << percentage << "% (" << downloadNow << "/" << downloadTotal << " bytes)" << std::flush;
                console.print(output.str());
                
                lastUpdate = now;
            }
        }
        return true;
    }));
    
    if (fs::exists(dirPath / LOCAL_PBF_PATH)) {
        console.log("PBF file already exists. Skipping...");
    } else if (PBF_DISABLED) {
        console.log("PBF file disabled in this build. Redefine `PBF_DISABLED` to change it", DEBUG_WARN);
    } else {
        console.print("Pulling PBF file from defined URL...\n");
        console.log(std::string("Pulling PBF file from ") + PBF_URL);
        std::ofstream file(dirPath / LOCAL_PBF_PATH);

        session.SetUrl(PBF_URL);
        cpr::Response response = session.Download(file);

        console.print("\n");
        if (response.status_code != 200) {
            // We are GONERS
            console.log("Failure fetching PBF file.", DEBUG_FAIL);
            return false;
        }
    }

    if (fs::exists(dirPath / WORLD_GEOJSON_PATH)) {
        console.log("GeoJson file already exists. Skipping...");
    } else {
        console.print("Pulling GeoJson file from defined URL...\n");

        console.log(std::string("Pulling GeoJson file metadata from ") + PHYS_VECT_URL);
        std::ofstream file2(dirPath / WORLD_GEOJSON_PATH);
    
        session.SetUrl(PHYS_VECT_URL);
        cpr::Response response2 = session.Get();
        if (response2.status_code != 200) {
            console.log("\nFailure pulling GeoJson file metadata from GitHub", DEBUG_FAIL);
            return false;
        }
        console.print("\n");
        
        json metadata = json::parse(response2.text);
        console.log("Pulling GeoJson file from " + metadata["download_url"].get<std::string>());
        session.SetUrl(metadata["download_url"].get<std::string>());

        cpr::Response response3 = session.Download(file2);
        if (response2.status_code != 200) {
            console.log("\nFailure pulling GeoJson file from provided download URL in metadata.", DEBUG_FAIL);
            return false;
        }
        console.print("\n");
    }

    return true;
}

int main(int argc, char *argv[]) {
    console.setVerbosity(false); // Initialize console with verbosity off by default
    dirPath = std::filesystem::current_path().string();

    // std::cout << "Hi\n";
    // std::cin.get();

    // bool nomon = false;
    bool nocompression = false;

    // Parse the arguments at runtime
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
            console.print("There are a multitude of different arguments you can use with this little tool.\nThat includes:\n-h / --help : Display this help screen\n-v / --verbose : Display verbose logging\n-nc / --nocompression : Disable compression in LWS (used in AIS fetching)");

            return 0; // Just displaying the help screen
        } 
        
        else if (arg == "--verbose" || arg == "-v") console.setVerbosity(true);

        // else if (arg == "--nomon" || arg == "-n") nomon = true;

        else if (arg == "--nocompression" || arg == "--nc") nocompression = true;

        else {
            // This is not recognized.
            console.log(arg + " is not recognized as a valid argument. Use the '--help' argument to list all possible arguments.", DEBUG_FAIL, true);
        }
    }

    console.print("Welcome to [bi]NeuralNet[/]!\n");

    // Fetch the PBF file down.
    if (!fetchMap()) {
        return -2;
    } else {
        console.log("Maps data are fetched!", DEBUG_OK);
    }

    
    // std::thread serverThread([&server]() {
    //     // Lambda shyts
    //     server.run();
    // });

    Fetcher fetcher(AIS_STREAM_KEY, console, nocompression, dirPath / SAVED_DISCOVERY_PATH);

    std::thread fetcherThread([&fetcher]() {
        // Lambda shyts
        fetcher.run();
    });

    SocketHost server(console, fetcher);

    // Blocks here until the client connects, sends messages, and eventually disconnects.
    server.run();
    
    fetcher.stop(); // Stop the fetcher when the server stops running

    if (fetcherThread.joinable()) fetcherThread.join(); // Make sure the server object's destructor is executed properly before being destroyed   

    // ~Renderer destructor will automatically run here.
    return 0;
}