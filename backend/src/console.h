#pragma once
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>

// Values for debugging. Uses with `log()` in `Console` class.
#define DEBUG_INFO 0x0F
#define DEBUG_FAIL 0x1F
#define DEBUG_WARN 0x2F
#define DEBUG_OK   0x3F

/*
This class is the central console output of the whole C++ project right here.
*/
class Console {
    public:
    // Initialize the console. Verbosity is whether or not would `log()` be able to output logs.
    // There's nothing here to detatch so...
    Console(bool verbosity = true);
    ~Console();

    // Prevent copying to prevent double-freeing resources
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

    // Change verbosity at runtime.
    void setVerbosity(bool verbosity = true);

    /* 
     * Log debugging texts. `state` dictates what kind of debugging message this is, whether that is an information dump, an error or a warning. `forced` makes this function ignores the underlying verbosity.
     * There are multiple states, which can be called through their defined names:
     *
     * - `DEBUG_INFO`: Prints information dump
     * - `DEBUG_FAIL`: Prints error
     * - `DEBUG_WARN`: Prints warning
     * - `DEBUG_OK`: Prints successful notices.
     *
     * *Output of this command ends in `\n`
     */
    void log(std::string message, uint8_t state = DEBUG_INFO, bool forced = false);

    /* A central printing command. This will act as a replacement for normal std::cout.
     * Output of this command DOES NOT end in `\n`
     * There are markings that can be used. 
     * 
     * Currently supported markings:
     * [b]bold[/], [i]italic[/], [bi]bold&italic[/]
     *
     * Basically, first come [tag], then ends with [/]
     */
    void print(std::string message);

    bool verbose;

    private:
    void consumerLoop(); // Looping the queues

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::string> queue_;
    std::atomic<bool> running_{true};
    std::thread worker_;
};