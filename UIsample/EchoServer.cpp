#include "EchoServer.h"
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem> // Requires C++17

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

EchoServer::EchoServer(unsigned short port)
    : port_(port), running_(false) {}

EchoServer::~EchoServer() {
    stop();
}

void EchoServer::start() {
    running_ = true;
    serverThread_ = std::thread(&EchoServer::run, this);
}

void EchoServer::stop() {
    running_ = false;
    // Unblock the acceptor if it's waiting
    try {
        net::io_context ioc;
        tcp::socket socket(ioc);
        socket.connect(tcp::endpoint(net::ip::make_address("127.0.0.1"), port_));
        socket.close();
    } catch (...) {
        // Ignore exceptions
    }
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

void EchoServer::setImageGetter(std::function<std::string()> getter) {
    getImage_ = getter;
}

void EchoServer::setCommandHandler(std::function<void(const std::string&, const std::string&)> handler) {
    handleCommand_ = handler;
}

void EchoServer::setStatusGetter(std::function<std::pair<bool, bool>()> getter) {
    getStatus_ = getter;
}

void EchoServer::run() {
    try {
        net::io_context ioc;

        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), port_));
        std::cout << "EchoServer: Listening on port " << port_ << "..." << std::endl;

        while (running_) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);

            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            http::read(socket, buffer, req);

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::server, "EchoServer");

            if (req.target() == "/") {
    // Serve the enhanced main page with sidebar and modern design
    res.set(http::field::content_type, "text/html");
    std::string body = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>Radar Echo Display</title>
    <style>
        /* Reset default margins and paddings */
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: Arial, sans-serif;
            display: flex;
            height: 100vh;
            overflow: hidden;
        }

        /* Sidebar styles */
        .sidebar {
            width: 250px;
            background-color: #2c3e50;
            color: white;
            transition: transform 0.3s ease;
            transform: translateX(0);
            display: flex;
            flex-direction: column;
        }

        .sidebar.hidden {
            transform: translateX(-250px);
        }

        .sidebar-header {
            padding: 20px;
            background-color: #1a252f;
            text-align: center;
            font-size: 1.2em;
            font-weight: bold;
        }

        .file-list {
            flex: 1;
            overflow-y: auto;
            padding: 10px;
        }

        .file-item {
            padding: 10px;
            border-bottom: 1px solid #34495e;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .file-item a {
            color: #ecf0f1;
            text-decoration: none;
        }

        .file-item a:hover {
            text-decoration: underline;
        }

        .file-size {
            font-size: 0.9em;
            color: #bdc3c7;
        }

        /* Main content styles */
        .main-content {
            flex: 1;
            display: flex;
            flex-direction: column;
            background-color: #ecf0f1;
            padding: 20px;
            overflow: auto;
        }

        .header {
            display: flex;
            align-items: center;
            margin-bottom: 20px;
        }

        .hamburger {
            font-size: 1.5em;
            cursor: pointer;
            margin-right: 20px;
            user-select: none;
        }

        .controls {
            display: flex;
            align-items: center;
            gap: 10px;
            flex-wrap: wrap;
        }

        .controls button, .controls select {
            padding: 10px 20px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 1em;
        }

        .controls button.active {
            background-color: #27ae60;
            color: white;
        }

        .controls button.inactive {
            background-color: #c0392b;
            color: white;
        }

        .controls select {
            background-color: #bdc3c7;
            color: #2c3e50;
        }

        /* Radar image styles */
        .radar-image-container {
            flex: 1;
            display: flex;
            justify-content: center;
            align-items: center;
            background-color: #34495e;
            border-radius: 8px;
            overflow: hidden;
        }

        .radar-image-container img {
            max-width: 100%;
            max-height: 100%;
        }

        /* Files section styles */
        .files-section {
            margin-top: 20px;
            background-color: white;
            padding: 15px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }

        .files-section h2 {
            margin-bottom: 10px;
            color: #2c3e50;
        }

        .files-section button {
            background-color: #2980b9;
            color: white;
        }

        .files-section ul {
            list-style: none;
            margin-top: 10px;
            max-height: 200px;
            overflow-y: auto;
        }

        .files-section li {
            padding: 8px;
            border-bottom: 1px solid #ecf0f1;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .files-section li a {
            color: #2980b9;
            text-decoration: none;
        }

        .files-section li a:hover {
            text-decoration: underline;
        }

        .files-section .file-size {
            font-size: 0.9em;
            color: #7f8c8d;
        }

        /* Responsive adjustments */
        @media (max-width: 768px) {
            .sidebar {
                position: absolute;
                height: 100%;
                z-index: 1000;
            }

            .main-content {
                padding: 10px;
            }
        }
    </style>
</head>
<body>
    <!-- Sidebar -->
    <div class="sidebar" id="sidebar">
        <div class="sidebar-header">
            Data Files
        </div>
        <div class="file-list" id="fileList">
            <!-- Files will be populated here -->
        </div>
    </div>

    <!-- Main Content -->
    <div class="main-content">
        <div class="header">
            <div class="hamburger" id="hamburger">&#9776;</div>
            <h1>Radar Echo Display</h1>
        </div>
        <div class="controls">
            <button id="displayEchoBtn" class="inactive">Display Echo: OFF</button>
            <button id="writeToFileBtn" class="inactive">Write to File: OFF</button>
            <select id="sweepLengthSelect">
                <option value="0">Sweep Length: 2048</option>
                <option value="1">Sweep Length: 4096</option>
                <option value="2">Sweep Length: 8192</option>
            </select>
            <select id="displayModeSelect">
                <option value="north_up">Display Mode: NORTH UP</option>
                <option value="head_up">Display Mode: HEAD UP</option>
            </select>
        </div>
        <div class="radar-image-container">
            <img id="radarImage" src="/radar_image" alt="Radar Echo" />
        </div>
        <div class="files-section">
            <h2>Data Files</h2>
            <button onclick="loadFiles()">Refresh File List</button>
            <ul id="fileListMain">
                <!-- File list will be populated here -->
            </ul>
        </div>
    </div>

    <script>
        // Sidebar toggle
        const hamburger = document.getElementById('hamburger');
        const sidebar = document.getElementById('sidebar');

        hamburger.addEventListener('click', () => {
            sidebar.classList.toggle('hidden');
        });

        // Radar image refresh
        function refreshImage() {
            var img = document.getElementById('radarImage');
            img.src = '/radar_image?rand=' + Math.random();
        }
        setInterval(refreshImage, 1000); // Refresh every second

        // Send command to server
        function sendCommand(command, value) {
            var xhr = new XMLHttpRequest();
            xhr.open('POST', '/command', true);
            xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            xhr.send('command=' + command + '&value=' + value);
        }

        // Toggle Display Echo
        const displayEchoBtn = document.getElementById('displayEchoBtn');
        displayEchoBtn.addEventListener('click', () => {
            sendCommand('display_echo', 'toggle');
            updateButtonStates();
        });

        // Toggle Write to File
        const writeToFileBtn = document.getElementById('writeToFileBtn');
        writeToFileBtn.addEventListener('click', () => {
            sendCommand('write_to_file', 'toggle');
            updateButtonStates();
        });

        // Set Sweep Length
        const sweepLengthSelect = document.getElementById('sweepLengthSelect');
        sweepLengthSelect.addEventListener('change', () => {
            sendCommand('set_sweep_length', sweepLengthSelect.value);
        });

        // Set Display Mode
        const displayModeSelect = document.getElementById('displayModeSelect');
        displayModeSelect.addEventListener('change', () => {
            sendCommand('display_mode', displayModeSelect.value);
        });

        // Update button states based on server status
        function updateButtonStates() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    // Update Display Echo Button
                    if (data.display_echo) {
                        displayEchoBtn.classList.add('active');
                        displayEchoBtn.classList.remove('inactive');
                        displayEchoBtn.innerText = 'Display Echo: ON';
                    } else {
                        displayEchoBtn.classList.add('inactive');
                        displayEchoBtn.classList.remove('active');
                        displayEchoBtn.innerText = 'Display Echo: OFF';
                    }

                    // Update Write to File Button
                    if (data.write_to_file) {
                        writeToFileBtn.classList.add('active');
                        writeToFileBtn.classList.remove('inactive');
                        writeToFileBtn.innerText = 'Write to File: ON';
                    } else {
                        writeToFileBtn.classList.add('inactive');
                        writeToFileBtn.classList.remove('active');
                        writeToFileBtn.innerText = 'Write to File: OFF';
                    }
                })
                .catch(error => {
                    console.error('Error fetching status:', error);
                });
        }

        // Load and display files with numeric sorting
        function loadFiles() {
            fetch('/files')
                .then(response => response.text())
                .then(data => {
                    const parser = new DOMParser();
                    const doc = parser.parseFromString(data, 'text/html');
                    const fileLinks = doc.querySelectorAll('a');
                    const fileList = document.getElementById('fileListMain');

                    // Convert NodeList to Array for sorting
                    const filesArray = Array.from(fileLinks).map(link => {
                        const fileName = link.textContent;
                        const fileSizeElement = link.parentElement.querySelector('.file-size');
                        const fileSize = fileSizeElement ? fileSizeElement.textContent : '';
                        return { name: fileName, size: fileSize };
                    });

                    // Numeric sorting based on file name
                    filesArray.sort((a, b) => {
                        const numA = parseInt(a.name, 10);
                        const numB = parseInt(b.name, 10);
                        return numA - numB;
                    });

                    // Clear existing list
                    fileList.innerHTML = '';

                    // Populate sorted list
                    filesArray.forEach(file => {
                        const li = document.createElement('li');
                        const a = document.createElement('a');
                        a.href = `/files/${file.name}`;
                        a.textContent = file.name;
                        const span = document.createElement('span');
                        span.classList.add('file-size');
                        span.textContent = file.size;
                        li.appendChild(a);
                        li.appendChild(span);
                        fileList.appendChild(li);
                    });
                })
                .catch(error => {
                    console.error('Error loading files:', error);
                });
        }

        // Initialize on page load
        window.onload = function() {
            updateButtonStates();
            loadFiles();
        };
    </script>
</body>
</html>
)HTML";
    res.body() = body;
}

            else if (req.target().starts_with("/radar_image")) {
                // Serve the radar image
                if (getImage_) {
                    std::string imagePath = getImage_();
                    if (!imagePath.empty()) {
                        std::ifstream imageFile(imagePath, std::ios::binary);
                        if (imageFile) {
                            res.set(http::field::content_type, "image/png");
                            std::ostringstream ss;
                            ss << imageFile.rdbuf();
                            res.body() = ss.str();
                        }
                        else {
                            res.result(http::status::not_found);
                            res.set(http::field::content_type, "text/plain");
                            res.body() = "Image not found";
                        }
                    }
                    else {
                        res.result(http::status::internal_server_error);
                        res.set(http::field::content_type, "text/plain");
                        res.body() = "Image path is empty";
                    }
                }
                else {
                    res.result(http::status::internal_server_error);
                    res.set(http::field::content_type, "text/plain");
                    res.body() = "Image getter not defined";
                }
            }
            else if (req.target() == "/command") {
                // Handle commands
                if (req.method() == http::verb::post) {
                    std::string body = req.body();
                    // Parse body to get command and value
                    std::string command, value;
                    size_t pos1 = body.find("command=");
                    size_t pos2 = body.find("&value=");
                    if (pos1 != std::string::npos && pos2 != std::string::npos) {
                        command = body.substr(pos1 + 8, pos2 - (pos1 + 8));
                        value = body.substr(pos2 + 7);
                    }
                    if (handleCommand_) {
                        handleCommand_(command, value);
                    }
                    res.set(http::field::content_type, "text/plain");
                    res.body() = "Command received";
                }
                else {
                    res.result(http::status::method_not_allowed);
                    res.set(http::field::content_type, "text/plain");
                    res.body() = "Only POST method allowed";
                }
            }
            else if (req.target() == "/status") {
                // Return current status as JSON
                if (getStatus_) {
                    std::pair<bool, bool> status = getStatus_();
                    std::ostringstream json;
                    json << "{ \"display_echo\": " << (status.first ? "true" : "false") 
                         << ", \"write_to_file\": " << (status.second ? "true" : "false") << " }";
                    res.set(http::field::content_type, "application/json");
                    res.body() = json.str();
                }
                else {
                    res.result(http::status::internal_server_error);
                    res.set(http::field::content_type, "text/plain");
                    res.body() = "Status getter not defined";
                }
            }
            else if (req.target() == "/files") {
                // List files in 'data' folder
                std::string dataDir = "data";
                std::vector<std::string> files;
                
                // Use C++17 filesystem
                try {
                    for (const auto & entry : std::filesystem::directory_iterator(dataDir)) {
                        if (entry.is_regular_file()) {
                            files.emplace_back(entry.path().filename().string());
                        }
                    }
                }
                catch (std::filesystem::filesystem_error& e) {
                    res.result(http::status::internal_server_error);
                    res.set(http::field::content_type, "text/plain");
                    res.body() = "Error accessing 'data' directory";
                    http::write(socket, res);
                    continue;
                }

                // Build HTML list
                std::ostringstream html;
                html << "<!DOCTYPE html><html><head><title>Data Files</title></head><body>";
                html << "<h1>Data Files</h1><ul>";
                for (const auto & file : files) {
                    html << "<li><a href=\"/files/" << file << "\">" << file << "</a></li>";
                }
                html << "</ul></body></html>";

                res.set(http::field::content_type, "text/html");
                res.body() = html.str();
            }
            else if (req.target().find("/files/") == 0) {
                // Serve individual file
                std::string filePath = std::string(req.target().substr(7));  // Remove "/files/"
                std::string fullPath = "data/" + filePath;

                std::ifstream file(fullPath, std::ios::binary);
                if (file) {
                    // Determine the content type based on file extension, assuming binary
                    res.set(http::field::content_type, "application/octet-stream");
                    res.set(http::field::content_disposition, "attachment; filename=\"" + filePath + "\"");
                    std::ostringstream ss;
                    ss << file.rdbuf();
                    res.body() = ss.str();
                }
                else {
                    res.result(http::status::not_found);
                    res.set(http::field::content_type, "text/plain");
                    res.body() = "File not found";
                }
            }
            else {
                // Handle 404 Not Found
                res.result(http::status::not_found);
                res.set(http::field::content_type, "text/plain");
                res.body() = "Not found";
            }

            res.prepare_payload();
            http::write(socket, res);
        }
    }
    catch (std::exception& e) {
        std::cerr << "EchoServer exception: " << e.what() << std::endl;
    }
}
