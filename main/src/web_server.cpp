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
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <atomic>
#include <filesystem>
#include <condition_variable>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <utility>
#include <numeric>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <chrono>
#include <optional>
#include <cstdio>


namespace Furuno {
    #include "nndll.h"
    #include <lasterror.h>
}

struct RangeGainStep {
    uint16_t range;
    uint16_t gain;
    uint16_t repeat;
};

struct ControlStep {
    uint16_t range;
    uint16_t gain;
    uint16_t pulse_length;     // S1, S2, M1, M2, M3, L 
    uint16_t rezBoost;         // 0, 1, 2, 3
    uint16_t rotation_speed;   // Lo, Hi, Auto
    uint16_t interpolation;    // 2048, 4096, 8192, 1024, 720
    uint16_t repeat;           // number of sweeps at this setting
};

constexpr int REPEAT = 10; // Default repeat count for each step in the pattern
extern std::atomic<bool> patternActive;

extern void RunRangeGainPattern(const std::vector<RangeGainStep>& pattern, int numRepeats = -1);
extern void StopPattern();
extern void RunPattern(const std::vector<ControlStep>& pattern, int numRepeats = -1);
extern void RunPatternSequence(const std::vector<std::pair<std::vector<ControlStep>, int>>& sequence);
extern std::vector<RangeGainStep> GenerateRampPattern(const std::vector<uint16_t>& ranges, uint16_t gainStart, uint16_t gainEnd, uint16_t repeatCount);
extern std::vector<ControlStep> GeneratePattern(
    const std::vector<uint16_t>& ranges,
    const std::vector<uint16_t>& pulse_lengths,
    const std::vector<uint16_t>& rezBoosts,
    const std::vector<uint16_t>& rotation_speeds,
    const std::vector<uint16_t>& interpolations,
    uint16_t gainStart,
    uint16_t gainEnd,
    uint16_t repeatCount
);

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

static bool IsValidControlStep(const ControlStep& s);
static std::string UrlDecode(const std::string& str);

static std::map<std::string, std::string> ParseFormUrlEncoded(const std::string& body) {
    std::map<std::string, std::string> params;
    size_t start = 0;
    while (start < body.size()) {
        const auto ampPos = body.find('&', start);
        const std::string pair = body.substr(
            start, (ampPos == std::string::npos) ? std::string::npos : ampPos - start);
        if (!pair.empty()) {
            const auto eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                const std::string key = pair.substr(0, eqPos);
                const std::string val = pair.substr(eqPos + 1);
                params[UrlDecode(key)] = UrlDecode(val);
            } else {
                params[UrlDecode(pair)] = "";
            }
        }
        if (ampPos == std::string::npos) {
            break;
        }
        start = ampPos + 1;
    }
    return params;
}

static std::string TrimCopy(const std::string& s) {
    auto itFront = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    auto itBack = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    if (itFront >= itBack) {
        return std::string();
    }
    return std::string(itFront, itBack);
}

static std::vector<std::string> TokenizeStepString(const std::string& raw) {
    std::vector<std::string> tokens;
    std::string current;
    auto flush = [&]() {
        std::string trimmed = TrimCopy(current);
        if (!trimmed.empty()) {
            tokens.push_back(std::move(trimmed));
        }
        current.clear();
    };

    for (char ch : raw) {
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n' || ch == ';') {
            flush();
        } else {
            current.push_back(ch);
        }
    }
    flush();
    return tokens;
}

static bool ParseControlStepsString(const std::string& rawSteps,
                                    std::vector<ControlStep>& pattern,
                                    int& validSteps,
                                    int& invalidSteps) {
    pattern.clear();
    validSteps = 0;
    invalidSteps = 0;

    const auto tokens = TokenizeStepString(rawSteps);
    int idx = 0;
    for (const auto& token : tokens) {
        std::stringstream itemss(token);
        std::string val;
        std::vector<int> nums;
        while (std::getline(itemss, val, ',')) {
            const std::string trimmed = TrimCopy(val);
            if (trimmed.empty()) {
                continue;
            }
            try {
                nums.push_back(std::stoi(trimmed));
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] stoi failed on '" << trimmed
                          << "' (step #" << idx << "): " << e.what() << "\n";
                ++invalidSteps;
                nums.clear();
                break;
            }
        }

        if (nums.size() == 7) {
            ControlStep cs{
                static_cast<uint16_t>(nums[0]),
                static_cast<uint16_t>(nums[1]),
                static_cast<uint16_t>(nums[2]),
                static_cast<uint16_t>(nums[3]),
                static_cast<uint16_t>(nums[4]),
                static_cast<uint16_t>(nums[5]),
                static_cast<uint16_t>(nums[6])
            };
            if (IsValidControlStep(cs)) {
                pattern.push_back(cs);
                ++validSteps;
            } else {
                ++invalidSteps;
            }
        } else if (!nums.empty()) {
            ++invalidSteps;
            std::cerr << "[WARN]   Invalid step, expected 7 values but got "
                      << nums.size() << " for token '" << token << "'\n";
        }
        ++idx;
    }
    return !pattern.empty();
}

static int DecodeBase64Char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return -2;
    return -1;
}

static bool Base64Decode(const std::string& input, std::string& output) {
    output.clear();
    int val = 0;
    int bits = -8;
    for (unsigned char c : input) {
        if (std::isspace(c)) {
            continue;
        }
        const int decoded = DecodeBase64Char(static_cast<char>(c));
        if (decoded == -1) {
            return false;
        }
        if (decoded == -2) {
            break; // padding reached
        }
        val = (val << 6) + decoded;
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<char>((val >> bits) & 0xFF));
            val &= (1 << bits) - 1;
            bits -= 8;
        }
    }
    return true;
}

static std::optional<double> ReadTemperatureCelsius() {
    const std::vector<std::string> candidates = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/hwmon/hwmon0/temp1_input"
    };
    for (const auto& path : candidates) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) continue;
        double milli = 0.0;
        ifs >> milli;
        if (ifs.fail()) continue;
        return milli / 1000.0;
    }
    return std::nullopt;
}

static double Clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static std::string BuildSystemMetricsJson() {
    // CPU usage per core
    static std::vector<std::pair<unsigned long long, unsigned long long>> lastCpuTotals;
    std::vector<double> coreUsages;
    {
        std::ifstream stat("/proc/stat");
        std::string line;
        while (std::getline(stat, line)) {
            if (line.rfind("cpu", 0) != 0) {
                continue;
            }
            if (line.size() < 4 || !std::isdigit(line[3])) {
                continue; // skip aggregate line
            }
            std::istringstream iss(line);
            std::string label;
            unsigned long long user=0, nice=0, system=0, idle=0, iowait=0, irq=0, softirq=0, steal=0, guest=0, guestnice=0;
            iss >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guestnice;
            unsigned long long idleAll = idle + iowait;
            unsigned long long nonIdle = user + nice + system + irq + softirq + steal;
            unsigned long long total = idleAll + nonIdle;
            size_t index = static_cast<size_t>(std::stoul(label.substr(3)));
            if (lastCpuTotals.size() <= index) {
                lastCpuTotals.resize(index + 1, {0, 0});
            }
            double usage = 0.0;
            auto [prevTotal, prevIdle] = lastCpuTotals[index];
            if (prevTotal != 0 || prevIdle != 0) {
                unsigned long long totald = total - prevTotal;
                unsigned long long idled = idleAll - prevIdle;
                if (totald > 0) {
                    usage = Clamp01(static_cast<double>(totald - idled) / static_cast<double>(totald));
                }
            }
            lastCpuTotals[index] = {total, idleAll};
            if (coreUsages.size() <= index) {
                coreUsages.resize(index + 1, 0.0);
            }
            coreUsages[index] = usage;
        }
    }
    double cpuPercent = 0.0;
    if (!coreUsages.empty()) {
        cpuPercent = std::accumulate(coreUsages.begin(), coreUsages.end(), 0.0) / static_cast<double>(coreUsages.size());
    }

    // Memory usage
    double memPercent = 0.0;
    {
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        double memTotalKb = 0.0;
        double memFreeKb = 0.0;
        double buffersKb = 0.0;
        double cachedKb = 0.0;
        double sreclaimKb = 0.0;
        while (std::getline(meminfo, line)) {
            double value = 0.0;
            if (std::sscanf(line.c_str(), "MemTotal: %lf", &value) == 1) {
                memTotalKb = value;
            } else if (std::sscanf(line.c_str(), "MemFree: %lf", &value) == 1) {
                memFreeKb = value;
            } else if (std::sscanf(line.c_str(), "Buffers: %lf", &value) == 1) {
                buffersKb = value;
            } else if (std::sscanf(line.c_str(), "Cached: %lf", &value) == 1) {
                cachedKb = value;
            } else if (std::sscanf(line.c_str(), "SReclaimable: %lf", &value) == 1) {
                sreclaimKb = value;
            }
        }
        if (memTotalKb > 0.0) {
            double availableKb = memFreeKb + buffersKb + cachedKb + sreclaimKb;
            double usedKb = memTotalKb - availableKb;
            memPercent = Clamp01(usedKb / memTotalKb);
        }
    }
    if (memPercent <= 0.0) {
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            double total = static_cast<double>(info.totalram) * info.mem_unit;
            double free = static_cast<double>(info.freeram) * info.mem_unit;
            double buffers = static_cast<double>(info.bufferram) * info.mem_unit;
            double used = total - (free + buffers);
            if (total > 0.0) {
                memPercent = Clamp01(used / total);
            }
        }
    }

    // Storage usage
    const std::vector<std::string> storageCandidates = {
        "runtime/output/images/unprocessed",
        "runtime/output/images/echo_trail",
        "runtime/output/raw/csv",
        "."
    };
    double storagePercent = 0.0;
    std::string storagePath = ".";
    for (const auto& path : storageCandidates) {
        struct statvfs vfs {};
        if (statvfs(path.c_str(), &vfs) == 0) {
            double total = static_cast<double>(vfs.f_blocks) * vfs.f_frsize;
            double free = static_cast<double>(vfs.f_bfree) * vfs.f_frsize;
            double used = total - free;
            if (total > 0.0) {
                storagePercent = Clamp01(used / total);
                storagePath = path;
                break;
            }
        }
    }

    // Network throughput (MB/s)
    static auto lastSampleTime = std::chrono::steady_clock::now();
    static unsigned long long lastRx = 0;
    static unsigned long long lastTx = 0;

    unsigned long long totalRx = 0;
    unsigned long long totalTx = 0;
    std::ifstream netifs("/proc/net/dev");
    if (netifs.is_open()) {
        std::string line;
        std::getline(netifs, line);
        std::getline(netifs, line);
        while (std::getline(netifs, line)) {
            std::istringstream iss(line);
            std::string iface;
            if (!(iss >> iface)) continue;
            if (iface.back() == ':') iface.pop_back();
            if (iface == "lo") continue;
            unsigned long long rxBytes = 0;
            unsigned long long txBytes = 0;
            if (!(iss >> rxBytes)) continue;
            for (int i = 0; i < 7; ++i) {
                iss >> line;
            }
            if (!(iss >> txBytes)) continue;
            totalRx += rxBytes;
            totalTx += txBytes;
        }
    }

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - lastSampleTime;
    double rxMbps = 0.0;
    double txMbps = 0.0;
    if (elapsed.count() > 0.0 && lastRx != 0 && lastTx != 0) {
        double rxDelta = static_cast<double>(totalRx - lastRx);
        double txDelta = static_cast<double>(totalTx - lastTx);
        rxMbps = (rxDelta / (1024.0 * 1024.0)) / elapsed.count();
        txMbps = (txDelta / (1024.0 * 1024.0)) / elapsed.count();
    }
    lastSampleTime = now;
    lastRx = totalRx;
    lastTx = totalTx;

    auto tempOpt = ReadTemperatureCelsius();

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "{";
    oss << "\"cpu_load\":" << cpuPercent << ",";
    oss << "\"cpu_count\":" << coreUsages.size() << ",";
    oss << "\"cpu_cores\":[";
    for (size_t i = 0; i < coreUsages.size(); ++i) {
        if (i) oss << ',';
        oss << coreUsages[i];
    }
    oss << "],";
    oss << "\"memory_used\":" << memPercent << ",";
    oss << "\"storage_used\":" << storagePercent << ",";
    oss << "\"storage_path\":\"" << storagePath << "\",";
    oss << "\"network_rx_mbps\":" << rxMbps << ",";
    oss << "\"network_tx_mbps\":" << txMbps << ",";
    if (tempOpt.has_value()) {
        oss << "\"temperature_c\":" << *tempOpt;
    } else {
        oss << "\"temperature_c\":null";
    }
    oss << "}";
    return oss.str();
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

// Returns true if step values are within allowed ranges
static bool IsValidControlStep(const ControlStep& s) {
    bool ok = true;
    if (s.range > 21)         { std::cerr << "[ERR] range " << s.range << " out of range 0-21\n"; ok = false; }
    if (s.gain > 99)          { std::cerr << "[ERR] gain " << s.gain << " out of range 0-99\n"; ok = false; }
    if (s.pulse_length > 5)   { std::cerr << "[ERR] pulse_length " << s.pulse_length << " out of range 0-5\n"; ok = false; }
    if (s.rezBoost > 3)       { std::cerr << "[ERR] rezBoost " << s.rezBoost << " out of range 0-3\n"; ok = false; }
    if (s.rotation_speed > 2) { std::cerr << "[ERR] rotation_speed " << s.rotation_speed << " out of range 0-2\n"; ok = false; }
    if (s.interpolation > 5)  { std::cerr << "[ERR] interpolation " << s.interpolation << " out of range 0-5\n"; ok = false; }
    // repeat: no explicit check
    return ok;
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
    if (path == "/" || path == "/index" || path == "/index.html") {
        WriteHttpResponse(socket, LoadFileContent("web/index.html"));
        return true;
    }
    if (path == "/radar" || path == "/radar.html") {
        WriteHttpResponse(socket, LoadFileContent("web/radar.html"));
        return true;
    }
    if (path == "/settings" || path == "/settings.html") {
        WriteHttpResponse(socket, LoadFileContent("web/settings.html"));
        return true;
    }
    if (path == "/status" || path == "/status.html") {
        WriteHttpResponse(socket, LoadFileContent("web/status.html"));
        return true;
    }
    if (path.rfind("/styles.css",0)==0) {
        WriteHttpResponse(socket, LoadFileContent("web/styles.css"), "text/css");
        return true;
    }
    if (path.rfind("/script.js",0)==0) {
        WriteHttpResponse(socket, LoadFileContent("web/script.js"), "application/javascript");
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
        std::string newest = FindNewestPNGFile("runtime/output/images/unprocessed");

        std::ostringstream oss;
        oss << "{\"ip\": \"" << ip << "\", "
            << "\"newest_png\": \"" << newest << "\"}";
        WriteHttpResponse(socket, oss.str(), "application/json");
        return true;
    }
    if (path.rfind("/system_metrics", 0) == 0) {
        WriteHttpResponse(socket, BuildSystemMetricsJson(), "application/json");
        return true;
    }
    if (path.rfind("/newest_png", 0) == 0) {
        std::string newest = FindNewestPNGFile("runtime/output/images/unprocessed");
        if (newest.empty()) {
            WriteHttpResponse(socket, "No PNG found.\r\n", "text/plain");
            return true;
        }
        std::ifstream ifs("runtime/output/images/unprocessed/" + newest, std::ios::binary);
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
    if (path.rfind("/newest_detection", 0) == 0) {
        std::string det_path = "runtime/output/images/yolo_detect/latest.png";
        std::ifstream ifs(det_path, std::ios::binary);
        if (!ifs) {
            WriteHttpResponse(socket, "No detection image yet.\r\n", "text/plain");
            return true;
        }
        std::vector<char> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        std::string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: image/png\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n"
            "Content-Length: " + std::to_string(buf.size()) + "\r\n\r\n";
        boost::system::error_code e;
        boost::asio::write(socket, boost::asio::buffer(header), e);
        boost::asio::write(socket, boost::asio::buffer(buf.data(), buf.size()), e);
        return true;
    }
    if (path.rfind("/newest_trail", 0) == 0) {
        std::string newest = FindNewestPNGFile("runtime/output/images/echo_trail");
        if (newest.empty()) {
            WriteHttpResponse(socket, "No trail image yet.\r\n", "text/plain");
            return true;
        }
        std::ifstream ifs("runtime/output/images/echo_trail/" + newest, std::ios::binary);
        if (!ifs) {
            WriteHttpResponse(socket, "Cannot open trail PNG.\r\n", "text/plain");
            return true;
        }
        std::vector<char> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        std::string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: image/png\r\n"
            "Cache-Control: no-cache\r\n"
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
            //run this 5 times if it fails to set the STC to 0.
    int ret1, ret2, ret3;
    for (int attempt = 1; attempt <= 5; ++attempt) {
        ret1  = Furuno::RmcSetNearSTC(   0, static_cast<Furuno::UINT16>(0));
        ret2 = Furuno::RmcSetMiddleSTC( 0, static_cast<Furuno::UINT16>(0));
        ret3 = Furuno::RmcSetFarSTC(    0, static_cast<Furuno::UINT16>(0));

        if (ret1 == 0 && ret2 == 0 && ret3 == 0) {
            // Success!
            std::cout << "STC values set to 0 on attempt #" << attempt << std::endl;
            break;
        }

        std::cerr << "Attempt #" << attempt 
                << " failed: Near=" << ret1 
                << ", Middle=" << ret2 
                << ", Far=" << ret3 
                << std::endl;

        if (attempt < 5) {
            // Optional back-off before retrying
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    if (ret != 0 || ret2 != 0 || ret3 != 0) {
        std::cerr << "Failed to set STC values after " 
                << 5 << " attempts." << std::endl;
        return -1;
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
static bool HandlePatternControl(const std::string &method,
                                 const std::string &path,
                                 const std::string &body,
                                 boost::asio::ip::tcp::socket &socket){
    if (path.rfind("/run_pattern_queue", 0) == 0) {
        if (method != "POST") {
            WriteHttpResponse(socket,
                              "<h2>Error</h2><p>Use POST to submit pattern queues.</p>");
            return true;
        }
        if (patternActive.load()) {
            WriteHttpResponse(socket,
                              "<h2>Busy</h2><p>A pattern is already running.</p>");
            return true;
        }

        auto form = ParseFormUrlEncoded(body);
        if (!form.count("payload")) {
            WriteHttpResponse(socket, "<h2>Error</h2><p>No payload supplied.</p>");
            return true;
        }

        const std::string payload = form["payload"];
        if (payload.empty()) {
            WriteHttpResponse(socket, "<h2>Error</h2><p>Empty queue payload.</p>");
            return true;
        }

        std::vector<std::pair<std::vector<ControlStep>, int>> queue;
        int totalValid = 0;
        int totalInvalid = 0;
        std::stringstream ss(payload);
        std::string item;
        int jobIndex = 0;

        while (std::getline(ss, item, ',')) {
            std::string trimmed = TrimCopy(item);
            if (trimmed.empty()) {
                continue;
            }
            const auto colon = trimmed.find(':');
            if (colon == std::string::npos) {
                ++totalInvalid;
                continue;
            }
            int repeats = -1;
            try {
                repeats = std::stoi(trimmed.substr(0, colon));
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Failed to parse queue repeat: " << e.what() << "\n";
                ++totalInvalid;
                continue;
            }

            std::string decodedSteps;
            if (!Base64Decode(trimmed.substr(colon + 1), decodedSteps)) {
                std::cerr << "[ERROR] Failed to base64 decode queue item #" << jobIndex << "\n";
                ++totalInvalid;
                continue;
            }

            std::vector<ControlStep> pattern;
            int valid = 0;
            int invalid = 0;
            if (ParseControlStepsString(decodedSteps, pattern, valid, invalid)) {
                queue.emplace_back(std::move(pattern), repeats);
                totalValid += valid;
                totalInvalid += invalid;
            } else {
                totalInvalid += std::max(1, invalid);
            }
            ++jobIndex;
        }

        if (queue.empty()) {
            WriteHttpResponse(socket,
                              "<h2>Error</h2><p>No valid patterns found in queue.</p>");
            return true;
        }

        std::ostringstream resp;
        resp << "<h2>Pattern queue started</h2>"
             << "<p>Jobs queued: " << queue.size() << "</p>"
             << "<p>Accepted steps: " << totalValid << "</p>";
        if (totalInvalid > 0) {
            resp << "<p style='color:red'>Ignored / invalid steps: "
                 << totalInvalid << "</p>";
        }

        std::thread([sequence = std::move(queue)]() mutable {
            RunPatternSequence(sequence);
        }).detach();

        WriteHttpResponse(socket, resp.str());
        return true;
    }

    if (path.rfind("/run_pattern", 0) == 0) {
        if (method != "GET") {
            WriteHttpResponse(socket,
                              "<h2>Error</h2><p>Use GET parameters to start a pattern.</p>");
            return true;
        }
        if (patternActive.load()) {
            WriteHttpResponse(socket,
                              "<h2>Busy</h2><p>A pattern is already running.</p>");
            return true;
        }

        auto params = ParseQueryParams(path);
        if (!params.count("steps")) {
            WriteHttpResponse(socket, "<h2>Error</h2><p>No steps parameter supplied.</p>");
            return true;
        }

        int repeats = -1;
        if (params.count("repeat")) {
            try {
                repeats = std::stoi(params["repeat"]);
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Failed to parse repeat: " << e.what() << "\n";
            }
        }

        const std::string steps = UrlDecode(params["steps"]);
        std::vector<ControlStep> pattern;
        int validSteps = 0;
        int invalidSteps = 0;
        if (!ParseControlStepsString(steps, pattern, validSteps, invalidSteps)) {
            WriteHttpResponse(socket, "<h2>Error</h2><p>No valid steps.</p>");
            return true;
        }

        std::ostringstream resp;
        resp << "<h2>Pattern started</h2>"
             << "<p>Accepted steps: " << validSteps << "</p>";
        if (invalidSteps > 0) {
            resp << "<p style='color:red'>Rejected invalid steps: "
                 << invalidSteps << "</p>";
        }

        std::thread([pattern = std::move(pattern), repeats]() mutable {
            RunPattern(pattern, repeats);
        }).detach();

        WriteHttpResponse(socket, resp.str());
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
        // Calibration: Ranges = [21, 0, 1, 2, 3], Gains 0-99, Repeat 5 times
        Furuno::RmSetEchoInterpolationFunc(0); // Disable interpolation
        Furuno::RmcSetACSEA(0, 0, 0, 0, 0); // Disable A/C SEA
        Furuno::RmcSetACRAIN(0, 0, 0, 0, 0, 0); // Disable A/C RAIN

        auto pattern = GenerateRampPattern({21, 0, 1, 2, 3}, 0, 99, 5); //note that 21-2 use the S1 pulse, 3 uses the S2 pulse
        int repeats = 1; // One cycle of the whole pattern

        int totalTime = pattern.size() * repeats; // in seconds

        std::thread([pattern, repeats]() { RunRangeGainPattern(pattern, repeats); }).detach(); // Run in detached thread so HTTP handler returns immediately

        std::cout << "[DEBUG] Calibration pattern started with " << pattern.size() << " steps.\n";

        std::ostringstream oss;
        oss << "<h2>Calibration sequencing started</h2>\n"
            << "<p>Total steps: " << pattern.size() << "</p>\n"
            << "<p>Expected time: " << totalTime << " seconds</p>\n"
            << "<p>Note: This will take a while, please wait...</p>\n";
        WriteHttpResponse(socket, oss.str());
        return true;
    }
    if (path.rfind("/begin_calib_full", 0) == 0) {
        // Calibration: Ranges = [2,3,4,5], Gains 0-99, Repeat 5 times
        Furuno::RmcSetACSEA(0, 0, 0, 0, 0); // Disable A/C SEA
        Furuno::RmcSetACRAIN(0, 0, 0, 0, 0, 0); // Disable A/C RAIN

        auto pattern = GeneratePattern(
            {2,3,4,5}, //ranges
            {0,1,2,3,4,5}, //pulse lengths
            {0,1,2,3}, //Rezboosts
            {0,1,2}, //rotation speeds
            {5,4,3,0,1,2}, //interpolations
            0, // gain start
            99, // gain end
            5 // repeat count
        );
        /*
        Codes are in order as used above: 
            pulse_length;     // S1, S2, M1, M2, M3, L 
            rezBoost;         // Off, Lo, Med, Hi
            rotation_speed;   // Lo, Hi, Auto
            interpolation;    // OFF, 2048, 4096, 8192, 1024, 720
        */
        int repeats = 1; // One cycle of the whole pattern
        int totalTime = pattern.size() * repeats; // in seconds

        std::thread([pattern, repeats]() { RunPattern(pattern, repeats); }).detach();// Run in detached thread so HTTP handler returns immediately
        std::cout << "[DEBUG] Calibration pattern started with " << pattern.size() << " steps.\n";
        std::ostringstream oss;
        oss << "<h2>Calibration sequencing started</h2>\n"
            << "<p>Total steps: " << pattern.size() << "</p>\n"
            << "<p>Expected time: " << totalTime << " seconds</p>\n"
            << "<p>Note: This will take a while, please wait...</p>\n";
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
        bool wantsJson = params.count("format") && params["format"] == "json";

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

            if (wantsJson) {
                std::ostringstream js;
                js << "{"
                << "\"near\":"   << u16_1 << ","
                << "\"middle\":" << u16_2 << ","
                << "\"far\":"    << u16_3 << "}";
                WriteHttpResponse(socket, js.str(), "application/json");
                return true;
            }

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

        std::map<std::string, std::string> headers;
        std::string headerLine;
        size_t contentLength = 0;
        while (std::getline(requestStream, headerLine)) {
            if (headerLine == "\r") {
                break;
            }
            if (!headerLine.empty() && headerLine.back() == '\r') {
                headerLine.pop_back();
            }
            auto colonPos = headerLine.find(':');
            if (colonPos == std::string::npos) {
                continue;
            }
            std::string key = headerLine.substr(0, colonPos);
            std::string value = headerLine.substr(colonPos + 1);
            key = TrimCopy(key);
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            value = TrimCopy(value);
            headers[key] = value;
        }

        if (headers.count("content-length")) {
            try {
                contentLength = static_cast<size_t>(std::stoul(headers["content-length"]));
            } catch (const std::exception&) {
                contentLength = 0;
            }
        }

        std::string body;
        if (contentLength > 0) {
            body.resize(contentLength);
            requestStream.read(&body[0], static_cast<std::streamsize>(contentLength));
            std::streamsize readCount = requestStream.gcount();
            if (readCount < static_cast<std::streamsize>(contentLength)) {
                boost::asio::read(socket,
                                  boost::asio::buffer(&body[static_cast<size_t>(readCount)],
                                                      contentLength - static_cast<size_t>(readCount)));
            }
        }

        if (HandlePatternControl(method, path, body, socket))   return;

        if (method != "GET") {
            WriteHttpResponse(socket, "<h1>Only GET supported on this endpoint</h1>");
            return;
        }

        if (HandleStaticAssets(path, socket))     return;
        if (HandleUtilities(path, socket))        return;
        if (HandleRadarSetters(path, socket))     return;
        if (HandleRadarGetters(path, socket))     return;
        // fallback 404
        WriteHttpResponse(socket, "<h2>Not Found</h2><p>Home: <a href=\"/\">/</a></p>");
    } catch(...) {
        // ignore individual errors
    }
}

#endif // USE_BOOST_WEB_INTERFACE
