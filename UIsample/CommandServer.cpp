// CommandServer.cpp

#include "CommandServer.h"
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <fstream>
#include <sstream>

using json = nlohmann::json;
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// Helper function to read a file into a string
std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open file: " + path);
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    file.close();
    return contents.str();
}

CommandServer::CommandServer(unsigned short port)
    : port_(port), running_(false)
{
    try {
        commandsJson_ = loadCommands("commands.json");
        std::cout << "CommandServer: Commands loaded successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "CommandServer: Error loading commands: " << e.what() << std::endl;
    }
}

CommandServer::~CommandServer() {
    stop();
}

nlohmann::json CommandServer::loadCommands(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("CommandServer: Unable to open " + filename);
    }
    json j;
    file >> j;
    return j;
}

void CommandServer::setCommandHandler(CommandHandler handler) {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    commandHandler_ = handler;
}

void CommandServer::start() {
    running_ = true;
    serverThread_ = std::thread(&CommandServer::run, this);
    std::cout << "CommandServer: Server started on port " << port_ << "." << std::endl;
}

void CommandServer::stop() {
    if (running_) {
        running_ = false;
        // To unblock the acceptor if it's waiting
        try {
            net::io_context ioc;
            tcp::socket socket(ioc);
            socket.connect(tcp::endpoint(net::ip::make_address("127.0.0.1"), port_));
            socket.close();
        } catch (...) {
            // Ignore any exceptions
        }
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        std::cout << "CommandServer: Server stopped." << std::endl;
    }
}

void CommandServer::run() {
    try {
        net::io_context ioc;

        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), port_));
        std::cout << "CommandServer: Listening on port " << port_ << "..." << std::endl;

        while (running_) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            std::cout << "CommandServer: Accepted a connection." << std::endl;

            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            http::read(socket, buffer, req);
            std::cout << "CommandServer: Received request: " << req.target() << std::endl;

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::server, "RadarCommandServer");

            std::string target = req.target().to_string();

            if (req.method() == http::verb::get) {
                if (target == "/") {
                    try {
                        std::string html = read_file("index.html");
                        res.set(http::field::content_type, "text/html");
                        res.body() = html;
                    } catch (const std::exception& e) {
                        res.result(http::status::internal_server_error);
                        res.set(http::field::content_type, "text/plain");
                        res.body() = "Internal Server Error: " + std::string(e.what());
                        std::cerr << "CommandServer: Error serving index.html: " << e.what() << std::endl;
                    }
                }
                else if (target == "/getCommands") {
                    res.set(http::field::content_type, "application/json");
                    res.body() = commandsJson_.dump();
                }
                else {
                    // Handle 404 Not Found for other GET requests
                    res.result(http::status::not_found);
                    res.set(http::field::content_type, "application/json");
                    json errorJson;
                    errorJson["status"] = "error";
                    errorJson["message"] = "Endpoint not found.";
                    res.body() = errorJson.dump();
                    std::cout << "CommandServer: 404 Not Found for " << target << std::endl;
                }
            }
            else if (req.method() == http::verb::post) {
                if (target == "/executeCommand") {
                    try {
                        json requestJson = json::parse(req.body());

                        std::string action = requestJson.at("action").get<std::string>();
                        std::map<std::string, std::string> params;

                        if (requestJson.contains("params") && requestJson["params"].is_object()) {
                            for (auto& el : requestJson["params"].items()) {
                                params[el.key()] = el.value().get<std::string>();
                            }
                        }

                        std::string result;
                        {
                            std::lock_guard<std::mutex> lock(handlerMutex_);
                            if (commandHandler_) {
                                result = commandHandler_(action, params);
                            }
                            else {
                                result = "Error: No command handler set.";
                            }
                        }

                        json responseJson;
                        responseJson["status"] = "success";
                        responseJson["message"] = result;

                        res.set(http::field::content_type, "application/json");
                        res.body() = responseJson.dump();
                    }
                    catch (const std::exception& e) {
                        res.result(http::status::bad_request);
                        res.set(http::field::content_type, "application/json");
                        json errorJson;
                        errorJson["status"] = "error";
                        errorJson["message"] = std::string("Invalid request: ") + e.what();
                        res.body() = errorJson.dump();
                        std::cerr << "CommandServer: Error processing /executeCommand: " << e.what() << std::endl;
                    }
                }
                else {
                    // Handle 404 Not Found for other POST requests
                    res.result(http::status::not_found);
                    res.set(http::field::content_type, "application/json");
                    json errorJson;
                    errorJson["status"] = "error";
                    errorJson["message"] = "Endpoint not found.";
                    res.body() = errorJson.dump();
                    std::cout << "CommandServer: 404 Not Found for " << target << std::endl;
                }
            }
            else {
                // Method Not Allowed
                res.result(http::status::method_not_allowed);
                res.set(http::field::content_type, "application/json");
                json errorJson;
                errorJson["status"] = "error";
                errorJson["message"] = "Method not allowed.";
                res.body() = errorJson.dump();
                std::cout << "CommandServer: 405 Method Not Allowed for " << target << std::endl;
            }

            res.prepare_payload();
            http::write(socket, res);
            std::cout << "CommandServer: Response sent." << std::endl;
        }
    }
    catch (std::exception& e) {
        std::cerr << "CommandServer exception in run(): " << e.what() << std::endl;
    }
}
