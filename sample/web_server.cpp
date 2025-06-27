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

namespace Furuno {
    #include "nndll.h"
    #include <lasterror.h>
}

namespace fs = std::filesystem;

//--------------------------------------
// Gain-sweep globals
//--------------------------------------
static std::vector<int>        g_gainSeq;
static std::atomic<bool>       g_sweepActive{false};
static std::thread             g_sweepThread;
static std::mutex              g_dirMutex; 

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

static void WaitForNextSweep(const std::string& lastPng)
{
    while (g_sweepActive)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::string newest = FindNewestPNGFile("frames");
        if (!newest.empty() && newest != lastPng) break;
    }
}

static void GainSweepWorker(std::vector<int> seq, int radarNo)
{
    size_t idx = 0;
    std::string lastPng = FindNewestPNGFile("frames");

    while (g_sweepActive)
    {
        int gain = seq[idx];

        // set gain (manual mode)
        Furuno::RmcSetGAIN(
            radarNo,
            /*status*/      static_cast<Furuno::UINT16>(0),   // 0 = manual
            /*manualValue*/ static_cast<Furuno::UINT16>(gain),
            /*autoValue*/   static_cast<Furuno::UINT16>(0)
        );

        // wait until next full rotation is captured
        WaitForNextSweep(lastPng);
        lastPng = FindNewestPNGFile("frames");

        // next gain (wrap-around)
        idx = (idx + 1) % seq.size();
    }
}

static void StartGainSweep(const std::vector<int>& seq, int radarNo)
{
    if (g_sweepActive) return;         // already running
    g_gainSeq  = seq;
    g_sweepActive = true;
    g_sweepThread =
        std::thread(GainSweepWorker, g_gainSeq, radarNo);
    g_sweepThread.detach();
}

static void StopGainSweep()
{
    g_sweepActive = false;
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

        // ================================
        //  STATIC ASSETS
        // ================================
        if (path == "/") {
            std::string homePage = LoadFileContent("index.html");
            WriteHttpResponse(socket, homePage, "text/html");
            return;
        }
        if (path.rfind("/styles.css", 0) == 0) {
            std::string cssContent = LoadFileContent("styles.css");
            WriteHttpResponse(socket, cssContent, "text/css");
            return;
        }
        if (path.rfind("/script.js", 0) == 0) {
            std::string jsContent = LoadFileContent("script.js");
            WriteHttpResponse(socket, jsContent, "application/javascript");
            return;
        }

        // ================================
        //  UTILITY ENDPOINTS
        // ================================
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
            return;
        }
        if (path.rfind("/newest_png", 0) == 0) {
            std::string newest = FindNewestPNGFile("frames");
            if (newest.empty()) {
                WriteHttpResponse(socket, "No PNG found.\r\n", "text/plain");
                return;
            }
            std::ifstream ifs("frames/" + newest, std::ios::binary);
            if (!ifs) {
                WriteHttpResponse(socket, "Cannot open the PNG.\r\n", "text/plain");
                return;
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
            return;
        }


        // ================================
        //  RADAR SETTERS
        // ================================
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
            return;
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
            return;
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
            return;
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
            return;
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
            return;
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
            return;
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
            return;
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
            return;
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
            return;
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
            return;
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
            return;
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
            return;
        }
        if (path.rfind("/start_gain_sweep", 0) == 0) {
            auto p = ParseQueryParams(path);
            std::string seqStr = p["seq"];          // "1,3,5,7,9,11"
            int radarNo = p.count("radarNo") ? std::stoi(p["radarNo"]) : 0;

            std::vector<int> seq;
            std::stringstream ss(seqStr);
            std::string token;
            while (std::getline(ss, token, ',')) {
                if (!token.empty()) seq.push_back(std::stoi(token));
            }
            if (seq.empty()) {
                WriteHttpResponse(socket, "<h2>Error</h2><p>Empty sequence</p>");
                return;
            }
            StartGainSweep(seq, radarNo);
            WriteHttpResponse(socket,
                "<h2>Gain sweep started</h2>"
                "<p>Sequence: " + seqStr + "</p>");
            return;
        }
        if (path.rfind("/stop_gain_sweep", 0) == 0) {
            StopGainSweep();
            WriteHttpResponse(socket, "<h2>Gain sweep stopped</h2>");
            return;
        }

        // ================================
        //  RADAR GETTERS
        // ================================
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
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
            }

            if (cmd == "ALL" || cmd == "GAIN") {
                // RmcGetCustomGAIN — gets manual+auto gain
                ret = Furuno::RmcGetCustomGAIN(radar, &pic, &u16_1, &u16_2, &u16_3);
                report << "<p>RmcGetCustomGAIN returned " << ret
                    << " → mode=" << u16_1
                    << ", manual=" << u16_2
                    << ", auto=" << u16_3 << "</p>\n";
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
            }

            if (cmd == "ALL" || cmd == "PULSE") {
                // RmcGetPulseWidth — gets pulse width & PRF
                Furuno::INT16 pulse, range;
                Furuno::INT32 picPW;
                ret = Furuno::RmcGetPulseWidth(radar, &pulse, &range, &u16_1, &picPW);
                report << "<p>RmcGetPulseWidth returned " << ret
                    << " → pulse=" << pulse
                    << ", prfUnit=" << u16_1 << "</p>\n";
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
            }

            if (cmd == "ALL" || cmd == "STC") {
                // RmcGetNear/Middle/FarSTC — gets STC curves
                ret = Furuno::RmcGetNearSTC   (radar, &u16_1);
                ret = Furuno::RmcGetMiddleSTC (radar, &u16_2);
                ret = Furuno::RmcGetFarSTC    (radar, &u16_3);
                report << "<p>STC: near="  << u16_1
                    << ", middle="       << u16_2
                    << ", far="          << u16_3 << "</p>\n";
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
            }

            if (cmd == "ALL" || cmd == "ANTREV") {
                // RmcGetAntennaRevolution — roll-speed & high-RPM
                ret = Furuno::RmcGetAntennaRevolution(radar, &u16_1, &u16_2);
                report << "<p>RmcGetAntennaRevolution returned " << ret
                    << " → rollSpeed=" << u16_1
                    << ", hiRpm="      << u16_2 << "</p>\n";
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
            }

            if (cmd == "ALL" || cmd == "ACSEA") {
                // RmcGetACSEA — sea clutter manual/auto/mode
                ret = Furuno::RmcGetACSEA(radar, &u16_1, &u16_2, &u16_3, &u16_4);
                report << "<p>RmcGetACSEA returned " << ret
                    << " → status=" << u16_1
                    << ", manual="   << u16_2
                    << ", auto="     << u16_3
                    << ", mode="     << u16_4 << "</p>\n";
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
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
                if (cmd != "ALL") {WriteHttpResponse(socket, report.str()); return; }
            }

            // If we reach here, teh command was "ALL"
            WriteHttpResponse(socket, report.str());
            return;
        }


        // ================================
        //  FALLBACK: 404 NOT FOUND
        // ================================
        {
            std::string resp = "<h2>Not Found</h2><p>Try <a href=\"/\">Home</a></p>";
            WriteHttpResponse(socket, resp);
        }

    } catch(...) {
        // If anything fails... ignore and keep moving forward lol
    }
}

#endif // USE_BOOST_WEB_INTERFACE
