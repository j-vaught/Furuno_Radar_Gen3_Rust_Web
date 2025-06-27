//sample/web_server.cpp

#ifdef USE_BOOST_WEB_INTERFACE

#include <boost/asio.hpp>
#include <thread>
#include <string>
#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>

namespace Furuno
{
#include "nndll.h"
#include <lasterror.h>
}


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

// Parse query parameters from URL
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

// Build and write a minimal HTTP response
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

//--------------------------------------
// Per-connection HTTP Session
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

        //--------------------------------------
        // 1) Home page 
        //--------------------------------------
        if (path == "/") {
            std::string homePage = LoadFileContent("index.html");
            WriteHttpResponse(socket, homePage, "text/html");
            return;
        }
        if (path == "/styles.css") {
            std::string cssContent = LoadFileContent("styles.css");
            WriteHttpResponse(socket, cssContent, "text/css");
            return;
        }
        
        //--------------------------------------
        // NEW: /get_system_info endpoint
        //--------------------------------------
        if (path == "/get_system_info") {
            // Get local IP address from the socket's local endpoint
            boost::system::error_code ip_ec;
            std::string ip = socket.local_endpoint(ip_ec).address().to_string();
            if (ip_ec) {
                ip = "Unavailable";
            }
            // Placeholder for terminal output. You can modify this to read from a log file or similar.
            std::string terminal = "Terminal output placeholder";
            // Get the newest PNG filename
            std::string newest = FindNewestPNGFile("frames");

            std::ostringstream oss;
            oss << "{\"ip\": \"" << ip << "\", "
                << "\"terminal\": \"" << terminal << "\", "
                << "\"newest_png\": \"" << newest << "\"}";
            WriteHttpResponse(socket, oss.str(), "application/json");
            return;
        }
        
        //--------------------------------------
        // 2) /set_gain
        //--------------------------------------
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

        //--------------------------------------
        // 3) /set_range
        //--------------------------------------
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

        //--------------------------------------
        // 4) /set_pulse_width?radarNo=&pulse=&rangeIndex=&rangeUnit=
        //--------------------------------------
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

        //--------------------------------------
        // 5) /set_near_stc?radarNo=&value=
        //--------------------------------------
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

        //--------------------------------------
        // 6) /set_middle_stc?radarNo=&value=
        //--------------------------------------
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

        //--------------------------------------
        // 7) /set_far_stc?radarNo=&value=
        //--------------------------------------
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

        //--------------------------------------
        // 8) /set_antenna_revolution?radarNo=&rollSpeed=&hiRpm=
        //--------------------------------------
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

        //--------------------------------------
        // 9) /set_acsea?radarNo=&status=&manual=&auto=&mode=
        //--------------------------------------
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

        //--------------------------------------
        // 10) /set_acrain?radarNo=&status=&manual=&auto=&ftcMode=&rainMode=
        //--------------------------------------
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
        //--------------------------------------
        // 4) /toggle_echo?mode=on|off
        //--------------------------------------
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

        //--------------------------------------
        // 5) /download?file=<some_csv>
        //--------------------------------------
        if (path.rfind("/download", 0) == 0) {
            auto params = ParseQueryParams(path);
            std::string fname = params["file"];
            std::string fullpath = std::string("data/") + fname;
            std::ifstream ifs(fullpath, std::ios::binary);
            if (!ifs) {
                std::string resp = "<h2>Error</h2><p>Cannot open file: " + fname + "</p>";
                WriteHttpResponse(socket, resp);
                return;
            }
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

            std::string response;
            response += "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: text/csv\r\n";
            response += "Content-Disposition: attachment; filename=\"" + fname + "\"\r\n";
            response += "Connection: close\r\n";
            response += "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n";
            response += content;

            boost::system::error_code e;
            boost::asio::write(socket, boost::asio::buffer(response), e);
            return;
        }

        //--------------------------------------
        // 6) /get_current?command=ALL&radarNo=0
        //--------------------------------------
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
                // RmcGetRange  — gets range code + unit :contentReference[oaicite:0]{index=0}
                ret = Furuno::RmcGetRange(radar, &i16_1, &u16_1);
                report << "<p>RmcGetRange returned " << ret
                    << " → code=" << i16_1
                    << ", unit=" << u16_1 << "</p>\n";
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
            }

            if (cmd == "ALL" || cmd == "GAIN") {
                // RmcGetCustomGAIN — gets manual+auto gain :contentReference[oaicite:1]{index=1}
                ret = Furuno::RmcGetCustomGAIN(radar, &pic, &u16_1, &u16_2, &u16_3);
                report << "<p>RmcGetCustomGAIN returned " << ret
                    << " → mode=" << u16_1
                    << ", manual=" << u16_2
                    << ", auto=" << u16_3 << "</p>\n";
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
            }

            if (cmd == "ALL" || cmd == "PULSE") {
                // RmcGetPulseWidth — gets pulse width & PRF :contentReference[oaicite:2]{index=2}
                Furuno::INT16 pulse, range;
                Furuno::INT32 picPW;
                ret = Furuno::RmcGetPulseWidth(radar, &pulse, &range, &u16_1, &picPW);
                report << "<p>RmcGetPulseWidth returned " << ret
                    << " → pulse=" << pulse
                    << ", prfUnit=" << u16_1 << "</p>\n";
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
            }

            if (cmd == "ALL" || cmd == "STC") {
                // RmcGetNear/Middle/FarSTC — gets STC curves :contentReference[oaicite:3]{index=3}
                ret = Furuno::RmcGetNearSTC   (radar, &u16_1);
                ret = Furuno::RmcGetMiddleSTC (radar, &u16_2);
                ret = Furuno::RmcGetFarSTC    (radar, &u16_3);
                report << "<p>STC: near="  << u16_1
                    << ", middle="       << u16_2
                    << ", far="          << u16_3 << "</p>\n";
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
            }

            if (cmd == "ALL" || cmd == "ANTREV") {
                // RmcGetAntennaRevolution — roll-speed & high-RPM :contentReference[oaicite:4]{index=4}
                ret = Furuno::RmcGetAntennaRevolution(radar, &u16_1, &u16_2);
                report << "<p>RmcGetAntennaRevolution returned " << ret
                    << " → rollSpeed=" << u16_1
                    << ", hiRpm="      << u16_2 << "</p>\n";
                if (cmd != "ALL") { WriteHttpResponse(socket, report.str()); return; }
            }

            if (cmd == "ALL" || cmd == "ACSEA") {
                // RmcGetACSEA — sea clutter manual/auto/mode :contentReference[oaicite:5]{index=5}
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

            // If we reach here, it was "ALL"
            WriteHttpResponse(socket, report.str());
            return;
        }


        //--------------------------------------
        // 7) /newest_png
        //--------------------------------------
        if (path.rfind("/newest_png", 0) == 0) {
            std::string newest = FindNewestPNGFile("frames");
            if (newest.empty()) {
                std::string resp = "HTTP/1.1 404 Not Found\r\n"
                                   "Content-Type: text/plain\r\n"
                                   "Connection: close\r\n\r\n"
                                   "No PNG found.\r\n";
                boost::system::error_code e;
                boost::asio::write(socket, boost::asio::buffer(resp), e);
                return;
            }
            
            std::ifstream ifs(std::string("frames/") + newest, std::ios::binary);
            
            if (!ifs) {
                std::string resp = "HTTP/1.1 404 Not Found\r\n"
                                   "Content-Type: text/plain\r\n"
                                   "Connection: close\r\n\r\n"
                                   "Cannot open the PNG.\r\n";
                boost::system::error_code e;
                boost::asio::write(socket, boost::asio::buffer(resp), e);
                return;
            }
            std::vector<char> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

            std::string header = "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: image/png\r\n"
                                 "Connection: close\r\n"
                                 "Content-Length: " + std::to_string(buf.size()) + "\r\n\r\n";

            boost::system::error_code e;
            boost::asio::write(socket, boost::asio::buffer(header), e);
            boost::asio::write(socket, boost::asio::buffer(buf.data(), buf.size()), e);
            return;
        }

        // ------------------------------------------------------------
        //  /set_sweep_total?mode=0..4
        // ------------------------------------------------------------
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

        // ------------------------------------------------------------
        //  /set_interpolation?enable=0|1
        // ------------------------------------------------------------
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


        //--------------------------------------
        // Fallback: 404 Not Found
        //--------------------------------------
        {
            std::string resp = "<h2>Not Found</h2><p>Try <a href=\"/\">Home</a></p>";
            WriteHttpResponse(socket, resp);
        }

    } catch(...) {
        // If anything fails, ignore for simplicity
    }
}

//--------------------------------------
// Runs forever, accepting connections
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
// Launch server on background thread
//--------------------------------------
void StartWebServerThread() {
    static std::thread webThread([]{
        RunHttpServer(8080);
    });
    webThread.detach(); // run in background
}

#endif // USE_BOOST_WEB_INTERFACE
