// sample/web_server.cpp

#ifdef USE_BOOST_WEB_INTERFACE

//--------------------------------------
// Includes
//--------------------------------------
#include <boost/asio.hpp>
#include <thread>
#include <string>
#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <atomic>
#include <filesystem>
#include <condition_variable>
#include <iomanip>
#include <cctype>


namespace Furuno {
    #include "nndll.h"
    #include <lasterror.h>
}

struct RangeGainStep {
    uint16_t range;
    uint16_t gain;
    uint16_t repeat;
};

constexpr int REPEAT = 10; // Default repeat count for each step in the pattern
extern std::atomic<bool> patternActive;
// i need to add a new function that goes through range, gain, and also rezboost(0-3) with interp on or off (8 total runs)

extern void RunRangeGainPattern(const std::vector<RangeGainStep>& pattern, int numRepeats = -1);
extern void StopPattern();
extern void RunRangeGainResolutionRampPattern(const std::vector<uint16_t>& ranges, uint16_t gainStart, uint16_t gainEnd, uint16_t repeatCount);

namespace fs = std::filesystem;

//--------------------------------------
// Function Declarations
//--------------------------------------
static void HttpSession(boost::asio::ip::tcp::socket socket);


//--------------------------------------
// Helper Functions
//--------------------------------------
static std::string LoadFileContent(const std::string &filepath) {
    std::ifstream ifs(filepath);
    if (!ifs) {
        return std::string("<h2>Error</h2><p>Cannot open file: " + filepath + "</p>");
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

// Parse query parameters from a URL into a map
static std::map<std::string, std::string> ParseQueryParams(const std::string &path) {
    std::map<std::string, std::string> params;
    auto questionPos = path.find('?');
    if (questionPos == std::string::npos) {
        return params;
    }
    std::string query = path.substr(questionPos + 1);
    size_t start = 0;
    for (;;) {
        auto ampPos = query.find('&', start);
        std::string param = query.substr(start, (ampPos == std::string::npos ? std::string::npos : ampPos - start));
        auto eqPos = param.find('=');
        if (eqPos != std::string::npos) {
            auto key = param.substr(0, eqPos);
            auto val = param.substr(eqPos + 1);
            params[key] = val;
        }
        if (ampPos == std::string::npos) break;
        start = ampPos + 1;
    }
    return params;
}

// Write a minimal HTTP response to the socket
static void WriteHttpResponse(boost::asio::ip::tcp::socket &socket,
                              const std::string &body,
                              const std::string &contentType = "text/html")
{
    std::string response;
    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Connection: close\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    response += body;

    boost::system::error_code ec;
    boost::asio::write(socket, boost::asio::buffer(response), ec);
}

// Find the newest PNG file in a directory
static std::string FindNewestPNGFile(const std::string &directory) {
    DIR* dp = opendir(directory.c_str());
    if (!dp) {
        return std::string();
    }
    std::string newestFile;
    time_t newestMtime = 0;

    struct dirent* de;
    while ((de = readdir(dp)) != nullptr) {
        std::string fname = de->d_name;
        if (fname.size() >= 4 && fname.substr(fname.size() - 4) == ".png") {
            std::string fullp = directory + "/" + fname;
            struct stat st;
            if (stat(fullp.c_str(), &st) == 0) {
                if (st.st_mtime > newestMtime) {
                    newestMtime = st.st_mtime;
                    newestFile = fname;
                }
            }
        }
    }
    closedir(dp);
    return newestFile;
}

// URL-decode helper
static std::string UrlDecode(const std::string& str) {
    std::string ret;
    char ch;
    int i, hex;
    for (i = 0; i < (int)str.length(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < (int)str.length()
             && std::isxdigit(str[i+1]) && std::isxdigit(str[i+2])) {
                std::istringstream iss(str.substr(i+1,2));
                iss >> std::hex >> hex;
                ch = static_cast<char>(hex);
                ret += ch;
                i += 2;
            } else {
                // malformed % escape, just copy
                ret += '%';
            }
        } else if (str[i] == '+') {
            ret += ' ';
        } else {
            ret += str[i];
        }
    }
    return ret;
}

// Pattern generator helper
static std::vector<RangeGainStep> GenerateRampPattern(const std::vector<uint16_t>& ranges, uint16_t gainStart, uint16_t gainEnd, uint16_t repeatCount) {
    std::vector<RangeGainStep> pattern;
    for (auto range : ranges) {
        for (uint16_t gain = gainStart; gain <= gainEnd; ++gain) {
            pattern.push_back(RangeGainStep{range, gain, repeatCount});
        }
    }
    // add a print here so i can see if it made it here
    std::cout << "[PatternControl] Generated ramp pattern with " << pattern.size() << " steps.\n";
    return pattern;
}


//--------------------------------------
// Main Web Server Loop
//--------------------------------------
static void RunHttpServer(unsigned short port) {
    try {
        boost::asio::io_context ioc;
        boost::asio::ip::tcp::acceptor acceptor{
            ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)
        };

        std::cout << "[WebServer] Listening on port " << port << "...\n";

        for (;;) {
            boost::asio::ip::tcp::socket socket{ioc};
            acceptor.accept(socket); // blocking accept
            HttpSession(std::move(socket));
        }
    } catch (const std::exception &e) {
        std::cerr << "[WebServer] Exception: " << e.what() << "\n";
    }
}


//--------------------------------------
// Server Startup Entry Point
//--------------------------------------
void StartWebServerThread() {
    static std::thread webThread([]{
        RunHttpServer(8080);
    });
    webThread.detach(); // run in background
}


// ================================
//  STATIC ASSETS HANDLING
// ================================
static bool HandleStaticAssets(const std::string &path, boost::asio::ip::tcp::socket &socket){
    if (path == "/") {
        WriteHttpResponse(socket, LoadFileContent("index.html"));
        return true;
    }
    if (path.rfind("/styles.css",0)==0) {
        WriteHttpResponse(socket, LoadFileContent("styles.css"), "text/css");
        return true;
    }
    if (path.rfind("/script.js",0)==0) {
        WriteHttpResponse(socket, LoadFileContent("script.js"), "application/javascript");
        return true;
    }
    return false;
}


// ================================
//  UTILITY ENDPOINTS
// ================================
static bool HandleUtilities(const std::string &path, boost::asio::ip::tcp::socket &socket){

    if (path.rfind("/get_system_info", 0) == 0) {
        boost::system::error_code ip_ec;// Get local IP address from the socket's local endpoint
        std::string ip = socket.local_endpoint(ip_ec).address().to_string();
        if (ip_ec) {
            ip = "Unavailable";
        }
        std::string newest = FindNewestPNGFile("frames");

        std::ostringstream oss;
        oss << "{\"ip\": \"" << ip << "\", "
            << "\"newest_png\": \"" << newest << "\"}";
        WriteHttpResponse(socket, oss.str(), "application/json");
        return true;
    }
    if (path.rfind("/newest_png", 0) == 0) {
        std::string newest = FindNewestPNGFile("frames");
        if (newest.empty()) {
            WriteHttpResponse(socket, "No PNG found.\r\n", "text/plain");
            return true;
        }
        std::ifstream ifs("frames/" + newest, std::ios::binary);
        if (!ifs) {
            WriteHttpResponse(socket, "Cannot open the PNG.\r\n", "text/plain");
            return true;
        }
        std::vector<char> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        std::string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: image/png\r\n"
            "Connection: close\r\n"
            "Content-Length: " + std::to_string(buf.size()) + "\r\n\r\n";
        boost::system::error_code e;
        boost::asio::write(socket, boost::asio::buffer(header), e);
        boost::asio::write(socket, boost::asio::buffer(buf.data(), buf.size()), e);
        return true;
    }
    return false;
}


// ================================
//  RADAR SETTERS
// ================================
static bool HandleRadarSetters(const std::string &path, boost::asio::ip::tcp::socket &socket) {
    auto p = ParseQueryParams(path);
    if (path.rfind("/set_gain", 0) == 0) {
        auto params = ParseQueryParams(path);
        int radarNo     = std::stoi(params["radarNo"]);
        int status      = std::stoi(params["status"]);
        int manualValue = std::stoi(params["manualValue"]);
        int autoValue   = std::stoi(params["autoValue"]);

        // Example call to your backend
        auto ret = Furuno::RmcSetGAIN(
            radarNo,
            static_cast<Furuno::UINT16>(status),
            static_cast<Furuno::UINT16>(manualValue),
            static_cast<Furuno::UINT16>(autoValue)
        );
        std::string resp = "<h2>Set Gain</h2><p>RmcSetGAIN returned: "
            + std::to_string(ret) + "</p>";
        WriteHttpResponse(socket, resp);
        return true;
    }
    if (path.rfind("/set_range", 0) == 0) {
        auto params = ParseQueryParams(path);
        int radarNo    = std::stoi(params["radarNo"]);
        int rangeIndex = std::stoi(params["rangeIndex"]);
        int rangeUnit  = std::stoi(params["rangeUnit"]);

        auto ret = Furuno::RmcSetRange_v2(
            radarNo,
            static_cast<Furuno::UINT16>(rangeIndex),
            static_cast<Furuno::UINT16>(rangeUnit),
            0 // metricValue
        );
        std::string resp = "<h2>Set Range</h2><p>RmcSetRange_v2 returned: "
            + std::to_string(ret) + "</p>";
        WriteHttpResponse(socket, resp);
        return true;
    }
    if (path.rfind("/set_pulse_width", 0) == 0) {
        auto p = ParseQueryParams(path);
        int radarNo    = std::stoi(p["radarNo"]);
        int pulse      = std::stoi(p["pulse"]);       // 0=S1…5=L
        int rangeIndex = std::stoi(p["rangeIndex"]);  // 0–24
        int rangeUnit  = std::stoi(p["rangeUnit"]);   // 0=NM,1=KM,2=SM,3=KYD
        int ret = Furuno::RmcSetPulseWidth(
            radarNo,
            static_cast<Furuno::INT16>(pulse),
            static_cast<Furuno::INT16>(rangeIndex),
            static_cast<Furuno::UINT16>(rangeUnit),
            0);
        WriteHttpResponse(socket,
            "<h2>Set Pulse Width</h2><p>RmcSetPulseWidth returned: "
            + std::to_string(ret) + "</p>");
        return true;
    }
    if (path.rfind("/set_near_stc", 0) == 0) {
        auto p = ParseQueryParams(path);
        int radarNo = std::stoi(p["radarNo"]);
        int val     = std::stoi(p["value"]);  // 0=2µs,1=2.5µs…4=4.2µs
        int ret = Furuno::RmcSetNearSTC(
            radarNo,
            static_cast<Furuno::UINT16>(val)
        );
        WriteHttpResponse(socket,
            "<h2>Set Near STC</h2><p>RmcSetNearSTC returned: "
            + std::to_string(ret) + "</p>");
        return true;
    }
    if (path.rfind("/set_middle_stc", 0) == 0) {
        auto p = ParseQueryParams(path);
        int radarNo = std::stoi(p["radarNo"]);
        int val     = std::stoi(p["value"]);  // 0…3
        int ret = Furuno::RmcSetMiddleSTC(
            radarNo,
            static_cast<Furuno::UINT16>(val)
        );
        WriteHttpResponse(socket,
            "<h2>Set Middle STC</h2><p>RmcSetMiddleSTC returned: "
            + std::to_string(ret) + "</p>");
        return true;
    }
    if (path.rfind("/set_far_stc", 0) == 0) {
        auto p = ParseQueryParams(path);
        int radarNo = std::stoi(p["radarNo"]);
        int val     = std::stoi(p["value"]);  // 0…2
        int ret = Furuno::RmcSetFarSTC(
            radarNo,
            static_cast<Furuno::UINT16>(val)
        );
        WriteHttpResponse(socket,
            "<h2>Set Far STC</h2><p>RmcSetFarSTC returned: "
            + std::to_string(ret) + "</p>");
        return true;
    }
    if (path.rfind("/set_antenna_revolution", 0) == 0) {
        auto p = ParseQueryParams(path);
        int radarNo   = std::stoi(p["radarNo"]);
        int rollSpeed = std::stoi(p["rollSpeed"]); // 0=LO,1=HI,2=AUTO
        int hiRpm     = std::stoi(p["hiRpm"]);     // 0=OFF,1=ON
        int ret = Furuno::RmcSetAntennaRevolution(
            radarNo,
            static_cast<Furuno::UINT16>(rollSpeed),
            static_cast<Furuno::UINT16>(hiRpm)
        );
        WriteHttpResponse(socket,
            "<h2>Set Antenna Revolution</h2><p>RmcSetAntennaRevolution returned: "
            + std::to_string(ret) + "</p>");
        return true;
    }
    if (path.rfind("/set_acsea", 0) == 0) {
        auto p = ParseQueryParams(path);
        int radarNo = std::stoi(p["radarNo"]);
        int st      = std::stoi(p["status"]); // 0=MANUAL,1=AUTO
        int man     = std::stoi(p["manual"]); 
        int aut     = std::stoi(p["auto"]);
        int mode    = std::stoi(p["mode"]);   // 0=Advanced,1=Coastal
        int ret = Furuno::RmcSetACSEA(
            radarNo,
            static_cast<Furuno::UINT16>(st),
            static_cast<Furuno::UINT16>(man),
            static_cast<Furuno::UINT16>(aut),
            static_cast<Furuno::UINT16>(mode)
        );
        WriteHttpResponse(socket,
            "<h2>Set A/C SEA</h2><p>RmcSetACSEA returned: "
            + std::to_string(ret) + "</p>");
        return true;
    }
    if (path.rfind("/set_acrain", 0) == 0) {
        auto p = ParseQueryParams(path);
        int radarNo = std::stoi(p["radarNo"]);
        int st      = std::stoi(p["status"]);   // 0=MANUAL,1=AUTO
        int man     = std::stoi(p["manual"]);
        int aut     = std::stoi(p["auto"]);
        int ftc     = std::stoi(p["ftcMode"]);  // 0=OFF,1=FTC-1,2=FTC-2
        int rain    = std::stoi(p["rainMode"]); // 0=Calm,1=Moderate,2=Rough
        int ret = Furuno::RmcSetACRAIN(
            radarNo,
            static_cast<Furuno::UINT16>(st),
            static_cast<Furuno::UINT16>(man),
            static_cast<Furuno::UINT16>(aut),
            static_cast<Furuno::UINT16>(ftc),
            static_cast<Furuno::UINT16>(rain)
        );
        WriteHttpResponse(socket,
            "<h2>Set A/C RAIN</h2><p>RmcSetACRAIN returned: "
            + std::to_string(ret) + "</p>");
        return true;
    }
    if (path.rfind("/toggle_echo", 0) == 0) {
        auto params = ParseQueryParams(path);
        int radarNo = std::stoi(params["radarNo"]);
        std::string mode = params["mode"]; // "on" or "off"
        
        Furuno::INT32 ret = 0;
        if (mode == "on") {
            ret = Furuno::RmcSetTxStby(
                radarNo,
                (Furuno::UINT16)2,  // TX=2
                (Furuno::UINT16)0,
                (Furuno::UINT32)0,
                (Furuno::UINT32)0
            );
        } else {
            ret = Furuno::RmcSetTxStby(
                radarNo,
                (Furuno::UINT16)1,  // STBY=1
                (Furuno::UINT16)0,
                (Furuno::UINT32)0,
                (Furuno::UINT32)0
            );
        }
        std::string resp = "<h2>Toggle Echo</h2><p>RmcSetTxStby returned: "
            + std::to_string(ret) + "</p>";
        WriteHttpResponse(socket, resp);
        return true;
    }
    if (path.rfind("/set_sweep_total", 0) == 0) {
        auto p   = ParseQueryParams(path);
        int mode = std::stoi(p["mode"]);             // 0-4

        // RAW Furuno call – no wrapper
        int ret = Furuno::RmSetEchoSweepTotal(static_cast<Furuno::INT16>(mode));

        std::string body = "<h2>Set Sweep Total</h2><p>mode = "
                        + std::to_string(mode) + " → API returned "
                        + std::to_string(ret)  + "</p>";
        WriteHttpResponse(socket, body);
        return true;
    }
    if (path.rfind("/set_interpolation", 0) == 0) {
        auto p     = ParseQueryParams(path);
        int enable = std::stoi(p["enable"]);         // 0 or 1

        // RAW Furuno call – no wrapper
        int ret = Furuno::RmSetEchoInterpolationFunc(
                    static_cast<Furuno::INT16>(enable));

        std::string body = "<h2>Set Interpolation</h2><p>enable = "
                        + std::to_string(enable) + " → API returned "
                        + std::to_string(ret)    + "</p>";
        WriteHttpResponse(socket, body);
        return true;
    }
    if (path.rfind("/set_bearing_resolution", 0) == 0) {
        auto params  = ParseQueryParams(path);
        int  radarNo = std::stoi(params["radarNo"]);

        static const char* modeNames[] = { "Off", "Low", "Med", "High" };

        // 1) Get current
        Furuno::UINT16 currentMode = 0;
        int retGet = Furuno::RmcGetBearingResolutionSelect(
            static_cast<Furuno::UINT16>(radarNo),
            &currentMode
        );

        // 2) Decide new mode (0–3)
        int requested = params.count("mode")
        ? std::clamp(std::stoi(params["mode"]), 0, 3)
        : 3;
        Furuno::UINT16 newMode = static_cast<Furuno::UINT16>(requested);

        // 3) Set it
        int retSet = Furuno::RmcSetBearingResolutionSelect(
            static_cast<Furuno::UINT16>(radarNo),
            newMode
        );

        // 4) Build friendly response
        std::ostringstream resp;
        resp << "<h2>RezBoost (Bearing Resolution)</h2>\n";

        if (retGet == 0) {
            resp << "<p><b>Read Current:</b> " 
                << currentMode << " (" << modeNames[currentMode] << ") — OK</p>\n";
        } else {
            resp << "<p><b>Read Current:</b> Error code " 
                << retGet << "</p>\n";
        }

        if (retSet == 0) {
            resp << "<p><b>Set New:</b> " 
                << newMode << " (" << modeNames[newMode] << ") — OK</p>\n";
        } else {
            resp << "<p><b>Set New:</b> Error code " 
                << retSet << "</p>\n";
        }

        WriteHttpResponse(socket, resp.str());
        return true;
    }

    return false;
}


// ================================
//  RADAR PATTERN RAMP CONTROL
// ================================
static bool HandlePatternControl(const std::string &path, boost::asio::ip::tcp::socket &socket){
    auto params = ParseQueryParams(path);
    if (path.rfind("/run_pattern", 0) == 0) {
        std::cerr << "[DEBUG] Received path: " << path << "\n";
        auto params = ParseQueryParams(path);
        for (const auto& kv : params) {
            std::cerr << "[DEBUG] Param: '" << kv.first << "' = '" << kv.second << "'\n";
        }

        if (!params.count("steps")) {
            std::cerr << "[ERROR] No 'steps' parameter supplied\n";
            WriteHttpResponse(socket, "<h2>Error</h2><p>No steps parameter supplied.</p>");
            return true;
        }

        // Decode steps
        std::string rawSteps = params["steps"];
        std::string steps = UrlDecode(rawSteps);
        std::cerr << "[DEBUG] Decoded steps = '" << steps << "'\n";

        std::vector<RangeGainStep> pattern;
        // parse repeats as before...
        int repeats = -1;
        if (params.count("repeat")) {
            try {
                repeats = std::stoi(params["repeat"]);
                std::cerr << "[DEBUG] Parsed repeats = " << repeats << "\n";
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Failed to parse repeat: " << e.what() << "\n";
            }
        }

        // Now split on ';'
        std::stringstream ss(steps);
        std::string stepItem;
        int idx = 0;
        while (std::getline(ss, stepItem, ';')) {
            std::cerr << "[DEBUG] Step #" << idx++ << ": raw='" << stepItem << "'\n";
            std::stringstream itemss(stepItem);
            std::string val;
            std::vector<int> nums;
            int valIdx = 0;
            while (std::getline(itemss, val, ',')) {
                try {
                    int n = std::stoi(val);
                    nums.push_back(n);
                    std::cerr << "[DEBUG]   Value #" << valIdx++ << " = " << n << "\n";
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR]   stoi failed on '" << val << "': " << e.what() << "\n";
                }
            }
            if (nums.size() == 3) {
                pattern.push_back(RangeGainStep{
                    static_cast<uint16_t>(nums[0]),
                    static_cast<uint16_t>(nums[1]),
                    static_cast<uint16_t>(nums[2])
                });
                std::cerr << "[DEBUG]   -> Added RangeGainStep("
                        << nums[0] << "," << nums[1] << "," << nums[2] << ")\n";
            } else {
                std::cerr << "[WARN]   Invalid step, expected 3 values but got "
                        << nums.size() << "\n";
            }
        }

        if (pattern.empty()) {
            std::cerr << "[ERROR] No valid steps parsed, pattern is empty\n";
            WriteHttpResponse(socket, "<h2>Error</h2><p>No valid steps.</p>");
            return true;
        }

        // Run in detached thread
        std::thread([pattern, repeats]() {
            RunRangeGainPattern(pattern, repeats);
        }).detach();
        WriteHttpResponse(socket, "<h2>Pattern started</h2>");
        return true;
    }

    if (path.rfind("/stop_pattern", 0) == 0) {
        if (!patternActive) {
            WriteHttpResponse(socket, "<h2>Pattern was not running.</h2>");
            return true;
        }
        StopPattern();
        WriteHttpResponse(socket, "<h2>Pattern stopped.</h2>");
        return true;
    }
    if (path.rfind("/begin_calibration", 0) == 0) {
        // Calibration: Ranges = [21, 0, 1, 2, 3], Gains 0-99, Repeat 10
        //need to set display and interpolation to 8196 and 0 respectively
        Furuno::RmSetEchoSweepTotal(2); // Set sweeps to 8196
        Furuno::RmSetEchoInterpolationFunc(0); // Disable interpolation
        //need to set acsea and acrain to all 0 
        Furuno::RmcSetACSEA(0, 0, 0, 0, 0); // Disable A/C SEA
        Furuno::RmcSetACRAIN(0, 0, 0, 0, 0, 0); // Disable A/C RAIN

        auto pattern = GenerateRampPattern({21, 0, 1, 2, 3}, 0, 99, 5);
        int repeats = 1; // One cycle of the whole pattern

        //add a thing that calulcate totlal time expected to complete the calibrations. 
        int totalTime = pattern.size() * repeats; // in seconds


        // Run in detached thread so HTTP handler returns immediately
        std::thread([pattern, repeats]() {
            RunRangeGainPattern(pattern, repeats);
        }).detach();
        // add a debug print here. did it make it here? if so was a pattern generated?
        std::cout << "[DEBUG] Calibration pattern started with " << pattern.size() << " steps.\n";
        // add the total time to the response
        std::ostringstream oss;
        oss << "<h2>Calibration sequencing started</h2>\n"
            << "<p>Total steps: " << pattern.size() << "</p>\n"
            << "<p>Expected time: " << totalTime << " seconds</p>\n"
            << "<p>Note: This will take a while, please wait...</p>\n";
        WriteHttpResponse(socket, oss.str());
        return true;
    }
    // NEW: /run_ramp_pattern  (hardcoded ramp)
    if (path.rfind("/run_ramp_pattern", 0) == 0) {
        // Your preferred ramp settings:
        int gainStart = 0;
        int gainEnd = 99;
        int repeat = 5;

        // Launch pattern in background
        std::thread([gainStart, gainEnd, repeat]() {
            RunRangeGainResolutionRampPattern({21,0,1,2,3}, gainStart, gainEnd, repeat);
        }).detach();

        std::ostringstream oss;
        oss << "<h2>Ramp/Resolution Pattern started</h2>\n";
        oss << "<br>Gain: " << gainStart << " to " << gainEnd
            << "<br>Repeat: " << repeat << "</p>";
        WriteHttpResponse(socket, oss.str());
        return true;
    }

    return false;
}




// ================================
//  RADAR GETTERS
// ================================
static bool HandleRadarGetters(const std::string &path, boost::asio::ip::tcp::socket &socket){
    if (path.rfind("/get_current", 0) == 0) {
        auto params  = ParseQueryParams(path);
        std::string cmd   = params["command"];
        int         radar = std::stoi(params["radarNo"]);

        // Reusable dummy vars
        Furuno::INT16  i16_1 = 0;
        Furuno::UINT16 u16_1 = 0, u16_2 = 0, u16_3 = 0, u16_4 = 0, u16_5=0;
        Furuno::INT32  pic   = -2;
        int            ret;

        std::ostringstream report;
        report << "<h2>Current Setting (" << cmd << ")</h2>\n"
            << "<p>RadarNo=" << radar << "</p>\n";

        if (cmd == "ALL" || cmd == "RANGE") {
            // RmcGetRange  — gets range code + unit
            ret = Furuno::RmcGetRange(radar, &i16_1, &u16_1);
            report << "<p>RmcGetRange returned " << ret
                << " → code=" << i16_1
                << ", unit=" << u16_1 << "</p>\n";
            if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return true; }
        }

        if (cmd == "ALL" || cmd == "GAIN") {
            // RmcGetCustomGAIN — gets manual+auto gain
            ret = Furuno::RmcGetCustomGAIN(radar, &pic, &u16_1, &u16_2, &u16_3);
            report << "<p>RmcGetCustomGAIN returned " << ret
                << " → mode=" << u16_1
                << ", manual=" << u16_2
                << ", auto=" << u16_3 << "</p>\n";
            if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return true; }
        }

        if (cmd == "ALL" || cmd == "PULSE") {
            // RmcGetPulseWidth — gets pulse width & PRF
            Furuno::INT16 pulse, range;
            Furuno::INT32 picPW;
            ret = Furuno::RmcGetPulseWidth(radar, &pulse, &range, &u16_1, &picPW);
            report << "<p>RmcGetPulseWidth returned " << ret
                << " → pulse=" << pulse
                << ", prfUnit=" << u16_1 << "</p>\n";
            if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return true; }
        }

        if (cmd == "ALL" || cmd == "STC") {
            // RmcGetNear/Middle/FarSTC — gets STC curves
            ret = Furuno::RmcGetNearSTC   (radar, &u16_1);
            ret = Furuno::RmcGetMiddleSTC (radar, &u16_2);
            ret = Furuno::RmcGetFarSTC    (radar, &u16_3);
            report << "<p>STC: near="  << u16_1
                << ", middle="       << u16_2
                << ", far="          << u16_3 << "</p>\n";
            if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return true; }
        }

        if (cmd == "ALL" || cmd == "ANTREV") {
            // RmcGetAntennaRevolution — roll-speed & high-RPM
            ret = Furuno::RmcGetAntennaRevolution(radar, &u16_1, &u16_2);
            report << "<p>RmcGetAntennaRevolution returned " << ret
                << " → rollSpeed=" << u16_1
                << ", hiRpm="      << u16_2 << "</p>\n";
            if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return true; }
        }

        if (cmd == "ALL" || cmd == "ACSEA") {
            // RmcGetACSEA — sea clutter manual/auto/mode
            ret = Furuno::RmcGetACSEA(radar, &u16_1, &u16_2, &u16_3, &u16_4);
            report << "<p>RmcGetACSEA returned " << ret
                << " → status=" << u16_1
                << ", manual="   << u16_2
                << ", auto="     << u16_3
                << ", mode="     << u16_4 << "</p>\n";
            if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return true; }
        }

        if (cmd == "ALL" || cmd == "ACRAIN") {
            ret = Furuno::RmcGetACRAIN(radar,&u16_1, &u16_2, &u16_3, &u16_4, &u16_5);
            report << "<p>RmcGetACRAIN returned " << ret
                << " → status="     << u16_1
                << ", manual="      << u16_2
                << ", auto="        << u16_3
                << ", ftcMode="     << u16_4
                << ", rainMode="    << u16_5
                << "</p>\n";
            if (cmd != "ALL") {WriteHttpResponse(socket, report.str()); return true; }
        }

        // If we reach here, the command was "ALL"
        WriteHttpResponse(socket, report.str());
        return true;
    }
    return false;
}


//--------------------------------------
// HTTP Request Routing & Handling
// One connection, one request (called per client)
//--------------------------------------
static void HttpSession(boost::asio::ip::tcp::socket socket) {
    try {
        boost::asio::streambuf buffer;
        boost::system::error_code ec;
        boost::asio::read_until(socket, buffer, "\r\n\r\n", ec);

        std::istream requestStream(&buffer);
        std::string requestLine;
        if (!std::getline(requestStream, requestLine)) {
            return;
        }
        auto firstSpacePos  = requestLine.find(' ');
        auto secondSpacePos = requestLine.find(' ', firstSpacePos + 1);
        if (firstSpacePos == std::string::npos || secondSpacePos == std::string::npos) {
            return;
        }

        std::string method = requestLine.substr(0, firstSpacePos);
        std::string path   = requestLine.substr(firstSpacePos + 1, secondSpacePos - firstSpacePos - 1);

        if (method != "GET") {
            WriteHttpResponse(socket, "<h1>Only GET supported</h1>");
            return;
        }

        if (HandleStaticAssets(path, socket))     return;
        if (HandleUtilities(path, socket))        return;
        if (HandleRadarSetters(path, socket))     return;
        if (HandlePatternControl(path, socket))   return;
        if (HandleRadarGetters(path, socket))     return;
        // fallback 404
        WriteHttpResponse(socket, "<h2>Not Found</h2><p>Home: <a href=\"/\">/</a></p>");
    } catch(...) {
        // ignore individual errors
    }
}

#endif // USE_BOOST_WEB_INTERFACE
