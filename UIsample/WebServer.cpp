// WebServer.cpp

#include "WebServer.h"
#include <boost/beast.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// Define custom event IDs
enum {
    BUTTON1_ID = wxID_HIGHEST + 1,
    BUTTON2_ID,
    BUTTON3_ID
};

WebServer::WebServer(unsigned short port, wxEvtHandler* evtHandler)
    : port_(port), running_(false), evtHandler_(evtHandler) {}

WebServer::~WebServer() {
    stop();
}

void WebServer::start() {
    running_ = true;
    serverThread_ = std::thread(&WebServer::run, this);
}

void WebServer::stop() {
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
}

void WebServer::setOutputGetter(std::function<std::string()> getter) {
    getOutput_ = getter;
}

void WebServer::run() {
    try {
        net::io_context ioc;

        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), port_));
        std::cout << "WebServer: Listening on port " << port_ << "..." << std::endl;

        while (running_) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            std::cout << "WebServer: Accepted a connection." << std::endl;

            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            http::read(socket, buffer, req);
            std::cout << "WebServer: Received request: " << req.target() << std::endl;

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::server, "RadarWebServer");

            if (req.target() == "/") {
                // Serve the main page
                res.set(http::field::content_type, "text/html");
                std::string body = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Radar Web Interface</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        #output { border: 1px solid #ccc; padding: 10px; height: 180px; overflow-y: scroll; background-color: #f9f9f9; }
        button { margin: 5px; padding: 10px 20px; }
    </style>
</head>
<body>
    <h1>Radar Output</h1>
    <div id="output">
        <!-- Output will be placed here -->
    </div>
    <button onclick="location.href='/button1'">View Echo</button>
    <button onclick="location.href='/button2'">View ARPA</button>
    <button onclick="location.href='/button3'">Command Tool</button>
    <script>
        // Function to fetch and update the output
        function updateOutput() {
            fetch('/output')
                .then(response => response.text())
                .then(data => {
                    document.getElementById('output').innerText = data;
                })
                .catch(error => console.error('Error fetching output:', error));
        }

        // Update output every second
        setInterval(updateOutput, 1000);
        // Initial load
        updateOutput();
    </script>
</body>
</html>
)";
                res.body() = body;
                std::cout << "WebServer: Served main page." << std::endl;
            } else if (req.target() == "/output") {
                // Return the current output
                res.set(http::field::content_type, "text/plain");
                if (getOutput_) {
                    std::string output;
                    {
                        std::lock_guard<std::mutex> lock(outputMutex_);
                        output = getOutput_();
                    }
                    res.body() = output;
                    std::cout << "WebServer: Served output." << std::endl;
                } else {
                    res.body() = "No output available";
                    std::cout << "WebServer: No output getter defined." << std::endl;
                }
            } else if (req.target() == "/button1") {
                if (evtHandler_) {
                    std::cout << "WebServer: Posting BUTTON1_ID event." << std::endl;
                    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, BUTTON1_ID);
                    wxPostEvent(evtHandler_, event);
                }
                res.set(http::field::content_type, "text/plain");
                res.body() = "Button 1 pressed";
            } else if (req.target() == "/button2") {
                if (evtHandler_) {
                    std::cout << "WebServer: Posting BUTTON2_ID event." << std::endl;
                    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, BUTTON2_ID);
                    wxPostEvent(evtHandler_, event);
                }
                res.set(http::field::content_type, "text/plain");
                res.body() = "Button 2 pressed";
            } else if (req.target() == "/button3") {
                if (evtHandler_) {
                    std::cout << "WebServer: Posting BUTTON3_ID event." << std::endl;
                    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, BUTTON3_ID);
                    wxPostEvent(evtHandler_, event);
                }
                res.set(http::field::content_type, "text/plain");
                res.body() = "Button 3 pressed";
            } else {
                res.result(http::status::not_found);
                res.set(http::field::content_type, "text/html");
                res.body() = R"(
<!DOCTYPE html>
<html>
<head>
    <title>404 Not Found</title>
</head>
<body>
    <h1>404 Not Found</h1>
    <p>The requested resource was not found on this server.</p>
    <a href="/">Back to Home</a>
</body>
</html>
)";
                std::cout << "WebServer: 404 Not Found for " << req.target() << std::endl;
            }

            res.prepare_payload();
            http::write(socket, res);
            std::cout << "WebServer: Response sent." << std::endl;
        }
    } catch (std::exception& e) {
        std::cerr << "WebServer exception: " << e.what() << std::endl;
    }
}
