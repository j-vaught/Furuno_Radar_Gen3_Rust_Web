// CommandServer.h

#ifndef COMMANDSERVER_H
#define COMMANDSERVER_H

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <functional>
#include <string>
#include <mutex>
#include <memory>
#include <map>
#include "json.hpp"

class CommandServer {
public:
    // Type alias for the command handler
    using CommandHandler = std::function<std::string(const std::string& action, const std::map<std::string, std::string>& params)>;

    // Constructor: Initialize with the desired port
    CommandServer(unsigned short port);

    // Destructor: Ensure the server is stopped
    ~CommandServer();

    // Start the server
    void start();

    // Stop the server
    void stop();

    // Set the command handler
    void setCommandHandler(CommandHandler handler);

private:
    // Main server loop
    void run();

    // Load commands from a JSON file
    nlohmann::json loadCommands(const std::string& filename);

    unsigned short port_;
    bool running_;
    std::thread serverThread_;

    CommandHandler commandHandler_;
    std::mutex handlerMutex_;

    // Parsed commands from JSON
    nlohmann::json commandsJson_;
};

#endif // COMMANDSERVER_H
