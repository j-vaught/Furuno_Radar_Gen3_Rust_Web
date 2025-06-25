// ────────────────────────────────────────────────────────────────
//  FURUNO NXT DR4SD — Minimal-yet-Complete demo / logger
//
//  * Keeps exactly the same API calls that web_server.cpp expects
//  * Prints clear 1-line runtime statistics every second
//  * Saves one CSV + PNG per antenna rotation
//  * Written to be approachable:  ▸ groups code logically
//                                 ▸ uses RAII helpers where possible
//                                 ▸ has self-describing function names
//
//  BUILD (example):
//      g++ -std=c++17 -O2 -pthread -DUSE_BOOST_WEB_INTERFACE \
//          main.cpp web_server.cpp -lboost_system -lstdc++fs \
//          -o radar_demo
//
//  RUNTIME (example):
//      ./radar_demo vendor.txt 192.168.0.10 192.168.0.50 HOSTPC
//
//  NOTE:  PART 1 of 4  →  paste all parts consecutively into one file
// ────────────────────────────────────────────────────────────────
#define _CRT_SECURE_NO_WARNINGS

// ────────── 1. Standard / system headers ──────────
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

// ────────── 2. 3rd-party / external headers ──────────
#include <boost/lockfree/spsc_queue.hpp>
#include "stb_image_write.h"

#ifdef USE_BOOST_WEB_INTERFACE
    #include "web_server.h"               // our ultra-simple http gateway
#endif

// ────────── 3. Furuno SDK ──────────
namespace Furuno {
    #include "nndll.h"
    #include <lasterror.h>
}

// ────────────────────────────────────────────────────────────────
//  SECTION A — compile-time constants & helpers
// ────────────────────────────────────────────────────────────────
namespace {

using Clock          = std::chrono::steady_clock;
using Microseconds   = std::chrono::microseconds;
using namespace std::chrono_literals;
namespace fs         = std::filesystem;

// Every radar echo line is ≤1024 bytes (always true on DR4SD)
constexpr int MAX_ECHO_SAMPLES = 1024;

// Gain pattern (manual values) we cycle through each rotation
constexpr int GAIN_PATTERN[] = {
      1,  5, 10, 15, 20, 25, 30, 35, 40, 45,
     50, 55, 60, 65, 70, 75, 80, 85, 90, 95
};
constexpr size_t GAIN_COUNT = sizeof(GAIN_PATTERN)/sizeof(GAIN_PATTERN[0]);

// ────────── runtime statistics ──────────
std::atomic<uint64_t> g_bytesThisSec   {0};
std::atomic<uint32_t> g_linesThisSec   {0};
Clock::time_point     g_statLastPrint  = Clock::now();

// ────────── concurrency flags ──────────
std::atomic<bool>     g_requestGainUpdate {false};
std::atomic<size_t>   g_gainIdx{0};

// ────────── misc forward declarations ──────────
void radarEchoCallback(Furuno::INT32 no, Furuno::INT16 status,
                       Furuno::UINT8* echo, Furuno::INT16 sweep_len,
                       Furuno::INT16 scale,  Furuno::INT16 range,
                       Furuno::INT16 angle,  Furuno::INT16 heading,
                       Furuno::INT16 h_flg);

void cycleCallback();
void commandCallback  (Furuno::INT32 id, Furuno::INT32 no);
void tileCallback     (Furuno::INT32,    Furuno::INT16,  Furuno::INT16,
                       Furuno::INT16,    Furuno::INT16,  Furuno::INT16,
                       Furuno::INT16,    Furuno::INT16,  Furuno::INT16,
                       Furuno::INT16,    Furuno::INT16,  Furuno::INT16,
                       unsigned char*);

void startRadarModule();
void prettyPrintReturn(const char* api, Furuno::INT32 ret);

// ────────────────────────────────────────────────────────────────
//  SECTION B — data containers
// ────────────────────────────────────────────────────────────────
struct EchoLine {
    Furuno::INT16  sweep_len {};
    Furuno::INT16  angle     {};   // 0-8196
    Furuno::UINT16 gain      {};
    std::array<uint8_t,MAX_ECHO_SAMPLES> samples{};
};

struct RotationBuffer {
    std::vector<EchoLine> lines;
    void clear() { lines.clear(); lines.shrink_to_fit(); }
};

// One buffer per antenna revolution
RotationBuffer g_rotation;

// Track previous spoke to detect a wrap-around
Furuno::INT16 g_prevAngle = -1;

// ────────────────────────────────────────────────────────────────
//  SECTION C — utility functions
// ────────────────────────────────────────────────────────────────
std::string nowYmdHmsMs()
{
    using namespace std::chrono;
    const auto now  = system_clock::now();
    const auto t    = system_clock::to_time_t(now);
    const auto ms   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_r(&t, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S")
        << '_' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();              // e.g. 20250615_143812_057
}

// Save current rotation to disk (CSV + PNG)
// --------------------------------------------------
void flushRotationToDisk()
{
    if (g_rotation.lines.empty()) return;

    const std::string stamp   = nowYmdHmsMs();
    const fs::path csvPath    = fs::path("data")   / (stamp + ".csv");
    const fs::path pngPath    = fs::path("frames") / (stamp + ".png");

    // 1) CSV
    {
        std::ofstream csv(csvPath);
        csv << "angle,gain";
        for (int i=0;i<MAX_ECHO_SAMPLES;++i) csv << ",s" << i;
        csv << '\n';

        for (const auto& l: g_rotation.lines) {
            csv << l.angle << ',' << l.gain;
            for (int i=0;i<l.sweep_len; ++i) csv << ',' << int(l.samples[i]);
            csv << '\n';
        }
    }

    // 2) PNG (quick polar plot, no axes)
    const int radius  = g_rotation.lines.front().sweep_len;
    const int dim     = radius*2;
    std::vector<uint8_t> img(dim*dim*4, 0);
    for (auto& l : g_rotation.lines) {
        double rad = (l.angle/8196.0)*2*M_PI;
        for (int r=0;r<l.sweep_len;++r) {
            int x = int(radius + r*std::cos(rad));
            int y = int(radius + r*std::sin(rad));
            if (x<0||x>=dim||y<0||y>=dim) continue;
            int idx = (y*dim + x)*4;
            img[idx+0]=img[idx+1]=img[idx+2]=l.samples[r]; img[idx+3]=255;
        }
    }
    stbi_write_png(pngPath.string().c_str(), dim, dim, 4, img.data(), dim*4);

    g_rotation.clear();
}

// ────────────────────────────────────────────────────────────────
//  SECTION D — runtime statistic printer
// ────────────────────────────────────────────────────────────────
void maybePrintStats(Furuno::INT16 currentSweepLen)
{
    using namespace std::chrono;
    auto now = Clock::now();
    if (now - g_statLastPrint < 1s) return;

    struct rusage ru{};
    getrusage(RUSAGE_SELF, &ru);

    double kb   = g_bytesThisSec.load() / 1024.0;
    printf("sweep_len=%4d  lines=%5u  kB=%6.1f  CPU(u|s)=%.1f|%.1f\n",
        currentSweepLen,
        g_linesThisSec.load(),
        kb,
        ru.ru_utime.tv_sec + ru.ru_utime.tv_usec/1e6,
        ru.ru_stime.tv_sec + ru.ru_stime.tv_usec/1e6);

    g_bytesThisSec = g_linesThisSec = 0;
    g_statLastPrint = now;
}

// ────────────────────────────────────────────────────────────────
//  SECTION E — Gain-ramp background thread
// ────────────────────────────────────────────────────────────────
void gainRampThread()
{
    while (true) {
        if (g_requestGainUpdate.exchange(false)) {
            size_t idx = g_gainIdx.fetch_add(1) % GAIN_COUNT;
            Furuno::UINT16 val = static_cast<Furuno::UINT16>(GAIN_PATTERN[idx]);
            Furuno::RmcSetGAIN(0, 0, val, 0);
        }
        std::this_thread::sleep_for(10ms);
    }
}

// ────────────────────────────────────────────────────────────────
//  SECTION F — radar callbacks
// ────────────────────────────────────────────────────────────────
void radarEchoCallback(Furuno::INT32    /*no*/,
                       Furuno::INT16    status,
                       Furuno::UINT8*   echo,
                       Furuno::INT16    sweep_len,
                       Furuno::INT16    /*scale*/,
                       Furuno::INT16    /*range*/,
                       Furuno::INT16    angle,
                       Furuno::INT16    /*heading*/,
                       Furuno::INT16    /*h_flg*/)
{
    // runtime stats
    g_bytesThisSec += sweep_len;
    g_linesThisSec += 1;
    maybePrintStats(sweep_len);

    // ask DLL for gain value of THIS spoke
    Furuno::INT32   dummy=-2;
    Furuno::UINT16  mode=0, gain=0, autoVal=0;
    Furuno::RmcGetCustomGAIN(0, &dummy, &mode, &gain, &autoVal);

    // store spoke
    EchoLine line;
    line.sweep_len = sweep_len;
    line.angle     = angle;
    line.gain      = gain;
    std::memcpy(line.samples.data(), echo, sweep_len);
    g_rotation.lines.push_back(std::move(line));

    // detect wrap-around (angle decreases)
    if (g_prevAngle!=-1 && angle < g_prevAngle) {
        flushRotationToDisk();
        g_requestGainUpdate = true;          // advance gain
    }
    g_prevAngle = angle;
}

void cycleCallback() {
    Furuno::INT32 connected{};
    Furuno::RmGetConnection(&connected);
    static Furuno::INT32 last = -1;
    if (connected!=last) {
        printf(connected ? "Connected!\n" : "Disconnected!\n");
        last = connected;
    }
}

void commandCallback(Furuno::INT32 id, Furuno::INT32 no){
    std::printf("Cmd #%d from radar %d\n", id, no);
}
void tileCallback(...) {}   // not used

// ────────────────────────────────────────────────────────────────
//  SECTION G — radar module bootstrap
// ────────────────────────────────────────────────────────────────
void startRadarModule()
{
    Furuno::UINT8 ver[16];
    Furuno::RmGetModuleVersionNo(ver);
    printf("Radar DLL   version: %s\n", ver);

    prettyPrintReturn("RmSetServerClientMode",
        Furuno::RmSetServerClientMode(0));
    prettyPrintReturn("RmInitialModule",
        Furuno::RmInitialModule(reinterpret_cast<Furuno::UINT8*>(""), commandCallback));
    prettyPrintReturn("RmaInitialArpaModule",
        Furuno::RmaInitialArpaModule());
    prettyPrintReturn("RmSetEchoCallbackFunc",
        Furuno::RmSetEchoCallbackFunc(radarEchoCallback));
    prettyPrintReturn("RmSetCycleCallbackFunc",
        Furuno::RmSetCycleCallbackFunc(cycleCallback));
    prettyPrintReturn("RmSetEchoInterpolationFunc",
        Furuno::RmSetEchoInterpolationFunc(1));
    prettyPrintReturn("RmSetEchoSweepTotal",
        Furuno::RmSetEchoSweepTotal(0));        // 2048
    prettyPrintReturn("RmSetTileEchoCallbackFunc",
        Furuno::RmSetTileEchoCallbackFunc(tileCallback));
}

// helper
void prettyPrintReturn(const char* api, Furuno::INT32 ret)
{
    static const char* tbl[] = {
        "OK", "NG", "NODATA", "INIT", "PARAM", "SEND", "—", "WTC_GZOFF",
        "WTC_OUTPROC", "CERT_ERR"
    };
    const char* s = "???";
    int idx = -ret;
    if (idx>=0 && idx<int(std::size(tbl))) s = tbl[idx];
    std::printf("%-30s : %s (%d)\n", api, s, ret);
}

} // anonymous namespace

// ────────────────────────────────────────────────────────────────
//  PART 2 of 4 —  helpers, Furuno initialisation, main()
// ────────────────────────────────────────────────────────────────
namespace {

// ────────── tiny helper: parse “A.B.C.D” into 4 bytes ──────────
bool parseIPv4(const char* txt, Furuno::UINT8 ip[4])
{
    int a,b,c,d;
    if (std::sscanf(txt,"%d.%d.%d.%d",&a,&b,&c,&d)!=4) return false;
    if ((a|b|c|d)&~0xFF) return false;
    ip[0]=a; ip[1]=b; ip[2]=c; ip[3]=d;
    return true;
}

// ────────── read simple vendor.txt (4 lines, “key: value”) ─────
bool readVendorTxt(const char* path, char out[4][150])
{
    constexpr const char* EXPECT[4] = {
        "vendor code", "keycode", "vendor id", "device id"
    };
    std::ifstream f(path);
    if (!f) return false;

    std::string line;
    for (int i=0;i<4;++i) {
        if (!std::getline(f,line)) return false;
        auto pos = line.find(':');
        if (pos==std::string::npos)   return false;
        auto key = line.substr(0,pos);
        for (auto&ch:key) ch=std::tolower(ch);
        if (key.find(EXPECT[i])!=0)   return false;     // wrong order / key

        std::string val = line.substr(pos+1);
        val.erase(0,val.find_first_not_of(" \t\r\n"));
        if (val.empty()) return false;
        std::strncpy(out[i], val.c_str(), 149);
    }
    return true;
}

// Pretty fatal-error helper
[[noreturn]] void die(const char* msg)
{
    std::fprintf(stderr,"FATAL: %s\n",msg);
    std::exit(1);
}

} // anonymous namespace


// ────────────────────────────────────────────────────────────────
//  MAIN
// ────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    if (argc!=5) {
        std::printf("\nUsage: %s vendor.txt RADAR_IP HOST_IP HOST_NAME\n\n", argv[0]);
        return 0;
    }

    fs::create_directories("data");
    fs::create_directories("frames");

    // 1) Read vendor.txt
    char lic[4][150]={{0}};
    if (!readVendorTxt(argv[1], lic))
        die("Invalid vendor.txt");

    // 2) Parse target radar sensor IP
    Furuno::UINT8 radarIp[4];
    if (!parseIPv4(argv[2], radarIp))
        die("Bad RADAR_IP address");

    // 3) Parse host name (≤8 chars)
    Furuno::UINT8 hostName[9]={0};
    std::strncpy( reinterpret_cast<char*>(hostName), argv[4], 8 );

    // 4) Load NAVNET DLL
    if (Furuno::fec_entry_function("libNAVNETDLL.so")!=ENTRY_OK)
        die("fec_entry_function() failed");

    Furuno::InitialITRON();

    // 5) Initialise Furuno runtime
    if (Furuno::Initialize(
            reinterpret_cast<Furuno::UINT8*>(lic[0]),
            reinterpret_cast<Furuno::UINT8*>(lic[1]),
            reinterpret_cast<Furuno::UINT8*>(lic[2]),
            reinterpret_cast<Furuno::UINT8*>(lic[3]),
            radarIp) != Furuno::None)
    {
        die("Furuno::Initialize() failed");
    }

    // 6) Configure network IP for DLL
    Furuno::NcSetNetworkIpAddress(radarIp[0],radarIp[1],radarIp[2],radarIp[3]);
    Furuno::NcInitialNavnetCommon(hostName,nullptr);

    // 7) Bring radar module up
    startRadarModule();

    // 8) Fire gain-ramp helper thread
    std::thread(gainRampThread).detach();

#ifdef USE_BOOST_WEB_INTERFACE
    std::puts("HTTP server on :8080 (web_server.cpp)");
    StartWebServerThread();
#endif

    // 9) Main thread just sleeps; callbacks do the work
    while (true) std::this_thread::sleep_for(1s);
}
// ────────────────────────────────────────────────────────────────
//  PART 3 of 4 — callback plumbing, gain-ramp helper thread
// ────────────────────────────────────────────────────────────────
using namespace std::chrono_literals;

/*───────────────────  GLOBAL STATE  ───────────────────*/
static std::vector<RadarEchoData> g_echoBuf;          // one full rotation
static std::mutex                g_echoMx;            // guard above

static std::atomic<uint64_t>     g_bytes{0};
static std::atomic<uint32_t>     g_lines{0};
static uint64_t                  g_lastPrintUs=0;

/*── gain pattern we’ll sweep through every rotation ──*/
constexpr int  kGainPattern[] =
    {  3, 10, 20, 35, 50, 65, 80, 95 };
constexpr size_t kGainCount = std::size(kGainPattern);
static std::atomic<size_t>  g_gainIdx{0};
static std::atomic<bool>    g_kickGain{false};

/*───────────────────  GAIN-RAMP THREAD  ───────────────────*/
void gainRampThread()
{
    while (true)
    {
        if (g_kickGain.exchange(false))               // fired by RadarEchoDraw
        {
            size_t idx   = g_gainIdx.load();
            int    value = kGainPattern[idx];

            Furuno::RmcSetGAIN(0, /*MANUAL*/0,
                               static_cast<Furuno::UINT16>(value),
                               /*auto*/0);

            g_gainIdx = (idx+1) % kGainCount;
        }
        std::this_thread::sleep_for(10ms);
    }
}

/*───────────────────  SAVE HELPERS  ───────────────────*/
void saveEchoAsCSV(const fs::path& p, const std::vector<RadarEchoData>& buf)
{
    std::ofstream f(p); if (!f) return;
    f << "angle,gain,";                               // header
    for (int i=0;i<1024;++i) f<<"e"<<i<<(i==1023?'\n':',');
    for (auto&row:buf)
    {
        f<<row.angle<<','<<row.gain<<',';
        f.write(reinterpret_cast<const char*>(row.echo), row.sweep_len);
        f<<'\n';
    }
}

void saveEchoAsPNG(const fs::path& p, const std::vector<RadarEchoData>& buf);
/* full implementation left unchanged — see part 4 */

/*───────────────────  RADAR ECHO CALLBACK  ───────────────────*/
static Furuno::INT16 g_prevAngle=-1;

void __stdcall RadarEchoDraw(Furuno::INT32 /*no*/, Furuno::INT16 /*status*/,
                             Furuno::UINT8* echo,  Furuno::INT16 sweepLen,
                             Furuno::INT16 /*scale*/, Furuno::INT16 /*range*/,
                             Furuno::INT16 angle,   Furuno::INT16 /*hdg*/,
                             Furuno::INT16 /*hFlg*/)
{
    g_bytes += sweepLen;
    g_lines += 1;

    auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>
                 (steady_clock::now().time_since_epoch()).count();
    if (nowUs - g_lastPrintUs > 1'000'000) {
        std::printf("sweep_len=%d  lines=%u  kB=%.1f\n",
                    sweepLen, g_lines.load(), g_bytes.load()/1024.0);
        g_bytes       = 0;
        g_lines       = 0;
        g_lastPrintUs = nowUs;
    }

    // read current manual-gain
    Furuno::INT32 dummy=-2;
    Furuno::UINT16 mode=0, manual=0, autoG=0;
    Furuno::RmcGetCustomGAIN(0,&dummy,&mode,&manual,&autoG);

    // save this sweep in RAM
    RadarEchoData row{};
    row.angle      = angle;
    row.sweep_len  = sweepLen;
    row.gain       = manual;
    std::memcpy(row.echo, echo, sweepLen);

    {
        std::lock_guard lg(g_echoMx);
        g_echoBuf.emplace_back(row);
    }

    // rotation finished? (Furuno angles wrap 0-8196)
    if (g_prevAngle!=-1 && angle<g_prevAngle)
    {
        std::vector<RadarEchoData> local;
        {
            std::lock_guard lg(g_echoMx);
            local.swap(g_echoBuf);
        }

        auto ts = std::chrono::system_clock::now();
        auto   t = std::chrono::system_clock::to_time_t(ts);
        std::tm tm; localtime_r(&t,&tm);
        char stamp[32];
        std::strftime(stamp,sizeof(stamp),"%Y%m%d_%H%M%S",&tm);

        std::ostringstream oss; oss<<stamp<<'.'
            << std::setw(3)<<std::setfill('0')
            << (std::chrono::duration_cast<std::chrono::milliseconds>
                (ts.time_since_epoch()).count()%1000);

        fs::path csv = fs::path("data")   /(oss.str()+".csv");
        fs::path png = fs::path("frames") /(oss.str()+".png");

        saveEchoAsCSV(csv, local);
        saveEchoAsPNG(png, local);

        g_kickGain = true;                        // trigger next gain step
    }
    g_prevAngle = angle;
}

/*───────────────────  SIMPLE CYCLE CALLBACK  ───────────────────*/
void __stdcall RadarCycleFunc()
{
    Furuno::INT32 conn=0;  Furuno::RmGetConnection(&conn);
    static int prev=-1;
    if (conn!=prev) {
        std::puts(conn?"[Radar] connected":"[Radar] disconnected");
        prev=conn;
    }
}

/*───────────────────  RADAR MODULE STARTUP  ───────────────────*/
void startRadarModule()
{
    Furuno::RmSetServerClientMode(0);
    Furuno::RmInitialModule(reinterpret_cast<Furuno::UINT8*>(""), CommandCallback);
    Furuno::RmaInitialArpaModule();

    Furuno::RmSetEchoCallbackFunc (RadarEchoDraw);
    Furuno::RmSetTileEchoCallbackFunc(nullptr);
    Furuno::RmSetCycleCallbackFunc(RadarCycleFunc);

    Furuno::RmSetEchoInterpolationFunc(1);   // default ON
    Furuno::RmSetEchoSweepTotal      (0);   // 2048 sweeps
    std::puts("[Radar] module started");
}