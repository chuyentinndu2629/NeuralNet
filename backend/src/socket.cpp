#include "socket.h"

#include <format>
#include <filesystem>
#include <fstream>

using asio::ip::tcp;

SocketHost::SocketHost(Console& con) : console(con), acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::make_address(ADDR), PORT)), socket_(io_context_), signals_(io_context_, SIGINT, SIGTERM) {
    console.log("Initializing socket host at socket " + std::string(ADDR) + ":" + std::to_string(PORT) + "...");

    // Start listening for signals
    handle_signals();

    wait_for_client(); // Start waiting for a client to connect
}

SocketHost::~SocketHost() {
    console.log("Shutting down socket host...");
    // Detatch console? Nahh its fine
    // Let me close the socket
    if (socket_.is_open()) {
        socket_.close();
    }

    if (acceptor_.is_open()) {
        acceptor_.close();
    }
}

void SocketHost::handle_signals() {
    signals_.async_wait([this](std::error_code ec, int signal_number) {
        if (!ec) {
            // std::stringstream ss;
            // ss << "Recieved signal " << std::format();
            console.log("Recieved signal " + std::format("{:#x}", signal_number));

            // This safely stops the event loop, causing io_context_.run() to return.f
            io_context_.stop();
        }
    });
}

void SocketHost::run() {
    io_context_.run(); // Start the ASIO event loop
}

void SocketHost::wait_for_client() {
    console.log("Waiting for a new client to connect...");

    // Accept exactly ONE connection because I aint working my ass off on a multi client server 
    // when each would have to call in here and get back value of a neural network calculation. That would fucking sucks
    acceptor_.async_accept(socket_, [this](std::error_code ec) {
        if (!ec) {
            console.log("Client connected from " + socket_.remote_endpoint().address().to_string() + ":" + std::to_string(socket_.remote_endpoint().port()));
            do_read(); // Start reading from the client
        } else {
            console.log("Error accepting client connection: " + ec.message(), DEBUG_FAIL);
            // Clean up the closed socket
            socket_.close();
            // Start listening for a new client
            wait_for_client();
        }
    });
}

void SocketHost::do_read() {
    socket_.async_read_some(asio::buffer(data_, max_length), [this](std::error_code ec, std::size_t length) {
        if (!ec) {
            console.log("Received data from client: " + std::string(data_, length));
            std::string ret = handle_request(std::string(data_, length));

            do_write(ret); // Echo the data back to the client
        } else {
            if (ec != asio::error::eof) console.log("Error reading from client: " + ec.message(), DEBUG_FAIL);
            else console.log("EOF Recieved. Connection severed.", DEBUG_WARN);
            // Clean up the closed socket
            socket_.close();
            // Start listening for a new client
            wait_for_client();
        }
    });
}

void SocketHost::do_write(std::string msg) {
    // Make sure the data aint gonna be destroyed when its out of scope, so that big ass msgs wont disappear when async_write is
    // trying to stream the data.

    // This basically move the buffer (stealing its pointer) without copying the whole payload
    auto data_ptr = std::make_shared<std::string>(std::move(msg + "\nEND\n"));
    
    asio::async_write(socket_, asio::buffer(*data_ptr), 
        [this, data_ptr](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                do_read();
            } else {
                console.log("Error writing to client: " + ec.message(), DEBUG_FAIL);
                socket_.close();
                wait_for_client();
            }
        });
}


std::string SocketHost::handle_request(std::string req) {
    if (req == "query:GEODATA") {
        std::ifstream worldData(std::filesystem::current_path() / WORLD_GEOJSON_PATH);

        if (!worldData.is_open()) {
            return "null";
        }

        std::stringstream ss;
        ss << worldData.rdbuf();

        return ss.str();
    } else {
        return "{ \"type\":\"TestReturn\" }";
    }
}