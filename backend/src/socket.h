#pragma once

#include "console.h"
#include "consts.h"

#include <asio.hpp>

using asio::ip::tcp;

class SocketHost : public std::enable_shared_from_this<SocketHost> {
public:
    SocketHost(Console& con);
    ~SocketHost(); // Destructor

    void run(); // Start the event loop (ASIO). Stop when the client disconnects.
    
private:
    // Async operation handlers
    void wait_for_client();
    void do_read();
    void do_write(std::string msg);

    // Setup the signal handler (SIGINT, SIGTERM)
    void handle_signals();

    // Processing some things related to the main thing here.
    std::string handle_request(std::string req);

    Console& console;

    // ASIO members (REQUIRED)
    asio::io_context io_context_;  // The main I/O context for asynchronous operations
    asio::ip::tcp::acceptor acceptor_;  // Accepts incoming connections (TCP)
    asio::ip::tcp::socket socket_;  // Represents the connected client socket

    // Add a signal_set member
    asio::signal_set signals_;

    // Buffer for reading the client data
    enum { max_length = BUFFER_SIZE };
    char data_[max_length];  // Buffer to hold incoming data
};