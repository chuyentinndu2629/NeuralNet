#include "console.h"
#include <iostream>  // To be normal is to be blessed
#include <chrono>  // Time
#include <format>  // Format the time
#include <mutex>

// Win32 exclusive: Enabling UTF-8
#ifdef _WIN32
#include <windows.h>
#endif

Console::Console(bool verbosity) {
#ifdef _WIN32
    // Set the console output code page to UTF-8 on Windows
    SetConsoleOutputCP(CP_UTF8);
#endif

    verbose = verbosity;

    worker_ = std::thread([this] { consumerLoop(); });
}

Console::~Console() {
    running_.store(false);
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void Console::setVerbosity(bool verbosity) {
    verbose = verbosity;
}

void Console::log(std::string message, uint8_t state, bool forced) {
    // Either the console is initialized with verbosity = true, or the forced parameter is turned to true and bypasses the verbosity limitation.
    // Your call.
    if (verbose || forced) {
        std::string color = "";
        std::string label = "";

        // This is stupid.
        switch (state) {
            case DEBUG_INFO:
                color = "\x1b[30;47m";
                label = "INFO";
                break;
            case DEBUG_FAIL:
                color = "\x1b[37;41m";
                label = "FAIL";
                break;
            case DEBUG_WARN:
                color = "\x1b[30;43m";
                label = "WARN";
                break;
            case DEBUG_OK:
                color = "\x1b[30;42m";
                label = " OK ";
                break;
        }

        // Get current time
        auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        // Formats directly to UTC
        std::string ts = std::format("{:%Y-%m-%d %H:%M:%S}", now);
        std::string full = "\x1b[2;49;3m" + ts + " \x1b[0m" + color + " " + label + " \x1b[0m " + message + "\n";
        // std::cout << "\x1b[2;49;3m" << ts << " \x1b[0m" << color << " " << label << " \x1b[0m " << message << '\n';

        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(full));
    }

    cv_.notify_one();
}

void Console::consumerLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || !running_; });

        while (!queue_.empty()) {
            std::string line = std::move(queue_.front());
            queue_.pop();
            lock.unlock();
            std::cout << line << std::flush;   // actual I/O happens here, off the hot path
            lock.lock();
        }

        if (!running_ && queue_.empty()) break;  // drain fully before exiting
    }
}

void Console::print(std::string message) {
    // Parse the message first. To find any markings.
    std::string currentString = "";

    std::string formattingTags = "";
    bool formatting = false;
    for (size_t i = 0; i < message.size(); i++) {
        if (message[i] == '[') {
            formatting = true;
        } else if (message[i] == ']') {
            formatting = false;
            // Let's print out all the current strings according to the current formatting tags
            // std::cout << currentString;

            if      (formattingTags == "b")  currentString += "\x1b[1m";
            else if (formattingTags == "i")  currentString += "\x1b[3m";
            else if (formattingTags == "bi") currentString += "\x1b[3;1m";
            else if (formattingTags == "/")  currentString += "\x1b[0m";

            // else if (formattingTags == "bi") std::cout << "\x1b[3;1m" << currentString << "\x1b[0m";

            // formattingTags = "";
            // currentString = "";
        } else {
            if (formatting) formattingTags += message[i];
            else {
                // Add in the current string
                currentString += message[i];
            }
        }
    }

    currentString += "\x1b[0m";

    // if (!currentString.empty()) std::cout << currentString << "\x1b[0m";

    // std::cout.flush();

    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(currentString));
    
    cv_.notify_one();
}