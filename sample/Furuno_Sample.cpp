#define _CRT_SECURE_NO_WARNINGS

// --- Project/local headers ---
#include "stb_image_write.h"
#ifdef USE_BOOST_WEB_INTERFACE
#include "web_server.h"
#endif
namespace Furuno {
	#include "nndll.h"
	#include <lasterror.h>
}

// --- Standard C library headers ---
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <unistd.h>
#include <sys/resource.h>

// --- Standard C++ library headers ---
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstdint>
#include <atomic>
#include <algorithm>
#include <filesystem>

// ───────────────────────────────────────────────────────────────
// Using Declarations & Namespace Aliases
// ───────────────────────────────────────────────────────────────
using std::this_thread::sleep_for;
namespace fs = std::filesystem;

// ───────────────────────────────────────────────────────────────
// Constants & Macros
// ───────────────────────────────────────────────────────────────
#define VENDOR_TXT_LINE_CHAR_MAX (150)
#define VENDOR_CODE_KEYWORD      ("vendor code")
#define KEY_CODE_KEYWORD         ("keycode")
#define VENDOR_ID_KEYWORD        ("vendor id")
#define DEVICE_ID_KEYWORD        ("device id")

// ───────────────────────────────────────────────────────────────
// Performance Counters
// ───────────────────────────────────────────────────────────────
namespace Perf {
    static std::atomic<uint64_t> bytesPerSec{0};
    static std::atomic<uint32_t> linesPerSec{0};
    static std::atomic<uint64_t> lastPrintUs{0};
}

// ───────────────────────────────────────────────────────────────
// Gain Pattern State
// ───────────────────────────────────────────────────────────────
namespace GainControl {
    std::atomic<bool> updateGain = false;
    std::atomic<bool> rotationToggle = false;
    std::atomic<size_t> currentGainIndex = 0;
	const int gainPattern[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,60,70,80,90,99};
    const size_t gainPatternCount = sizeof(gainPattern) / sizeof(gainPattern[0]);
    std::atomic<bool> stopGainControlThread = false;
}

// ───────────────────────────────────────────────────────────────
// Radar Echo Data
// ───────────────────────────────────────────────────────────────
struct RadarEchoData {
    Furuno::INT16  status;
    Furuno::INT16  sweep_len;
    Furuno::INT16  scale;
    Furuno::INT16  range;
    Furuno::INT16  angle;
    Furuno::UINT16 gain;
    Furuno::UINT8  echo[1024];
};
std::vector<RadarEchoData> radarEchoBuffer;
static Furuno::INT16 lastSweepAngle = -1;

// ───────────────────────────────────────────────────────────────
// Global/Module State
// ───────────────────────────────────────────────────────────────
static Furuno::INT32 connectionStatus = -1;

// ───────────────────────────────────────────────────────────────
// Forward Declarations
// ───────────────────────────────────────────────────────────────
static void __stdcall RadarEchoCallback(Furuno::INT32 no, Furuno::INT16 status, Furuno::UINT8 *echo, Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 range, Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg);
static void __stdcall RadarCycleCallback(void);
static void __stdcall CommandCallback(Furuno::INT32 id, Furuno::INT32 no);
static void __stdcall EchoTileCallback(Furuno::INT32 radarNo, Furuno::INT16 heading, Furuno::INT16 headingStatus, Furuno::INT16 range, Furuno::INT16 rangeUnit, Furuno::INT16 scale, Furuno::INT16 xPos, Furuno::INT16 yPos, Furuno::INT16 sweepCenterX, Furuno::INT16 sweepCenterY, Furuno::INT16 tileSize, unsigned char *echo);
static void PrintSensorList(void);
static void StartRadarModule();
static int ReadLicenseFile(const char*, char[4][VENDOR_TXT_LINE_CHAR_MAX]);
void GainControlThread();
void WriteEchoImage(const std::vector<RadarEchoData>&, const std::string&);
void WriteEchoCSV(const std::vector<RadarEchoData>&, const std::string&);


// ========== ENTRY POINT ==========
int main(int argc, char *argv[]) {
    if (argc < 5) return 1;
    fs::create_directories("data");
    fs::create_directories("frames");

    Furuno::UINT8 ipAddress[4] = { 0 };
    Furuno::UINT8 hostName[9] = { 0 };
    Furuno::UINT8 radarHostName[9] = { 0 };
    Furuno::UINT8 radarHostNameList[4][9] = { 0 };
    Furuno::INT32 radarHostCount = 0;
    char vendorlic[4][VENDOR_TXT_LINE_CHAR_MAX] = { '\0' };

    setvbuf(stdout, NULL, _IONBF, 0);
    strncpy((char *)hostName, argv[4], sizeof(hostName) - 1);

    if (ReadLicenseFile(argv[1], vendorlic) != 0) return -1;

    Furuno::fec_entry_function("libNAVNETDLL.so");
    Furuno::InitialITRON();
    Furuno::LeClearLastAPIError();
    setenv("FEC_NAVNETDLL_TARGET", argv[2], 1);

    if (Furuno::Initialize((Furuno::UINT8 *)vendorlic[0], (Furuno::UINT8 *)vendorlic[1], (Furuno::UINT8 *)vendorlic[2], (Furuno::UINT8 *)vendorlic[3], ipAddress) != Furuno::None)
        return -1;

    // Parse IP address
    std::istringstream ipstream(argv[3]);
    std::string byte; int idx = 0;
    while (std::getline(ipstream, byte, '.') && idx < 4)
        ipAddress[idx++] = static_cast<Furuno::UINT8>(std::stoi(byte));
    if (idx != 4) return -1;

    Furuno::NcSetNetworkIpAddress(ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);
    Furuno::NcInitialNavnetCommon(hostName, nullptr);
    Furuno::NcInitialNmea183Server();
    Furuno::NcInitialNmea0183PortStatus(1);
    PrintSensorList();

    StartRadarModule();

    #ifdef USE_BOOST_WEB_INTERFACE
    StartWebServerThread();
    #endif

    // --- Find the Radar Sensor ---
    for (int attempts = 0; attempts < 60; ++attempts) {
        Furuno::RmGetSensorHostCount(&radarHostCount);
        if (radarHostCount > 0) {
            Furuno::RmGetSensorHostName(0, radarHostNameList[0]);
            strncpy((char *)radarHostName, (const char *)radarHostNameList[0], sizeof(radarHostName) - 1);
			radarHostName[sizeof(radarHostName) - 1] = '\0';
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (radarHostCount == 0)
        return -1;

    Furuno::RmSetRadarSource(0, radarHostName, 0);
    std::thread gainThread(GainControlThread);
    gainThread.detach();

    // --- Block forever ---
    while (true) std::this_thread::sleep_for(std::chrono::hours(24));
    return 0;
}


// ========== CORE RUNTIME ==========
void GainControlThread() {
	while (!GainControl::stopGainControlThread.load()) {
		if (GainControl::updateGain.load()) {
			GainControl::updateGain = false;
			
			size_t idx = GainControl::currentGainIndex.load();
			int manualVal = GainControl::gainPattern[idx];
			
			Furuno::RmcSetGAIN(0, 0, (Furuno::UINT16)manualVal, 0);
			
			// Move to next index for next rotation
			GainControl::currentGainIndex = (idx + 1) % GainControl::gainPatternCount;
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

static void StartRadarModule() {
    Furuno::UINT8 versionNoForRadarModule[16];
    Furuno::RmGetModuleVersionNo(versionNoForRadarModule);
    printf("Radar Module Version No.   : ver.%s\n", versionNoForRadarModule);

    int ret1 = Furuno::RmSetServerClientMode(0);
    int ret2 = Furuno::RmInitialModule((Furuno::UINT8 *)"", CommandCallback);
    int ret3 = Furuno::RmaInitialArpaModule();
    int ret4 = Furuno::RmSetEchoCallbackFunc(RadarEchoCallback);
    int ret5 = Furuno::RmSetCycleCallbackFunc(RadarCycleCallback);
    int ret6 = Furuno::RmSetEchoInterpolationFunc(1);
    int ret7 = Furuno::RmSetEchoSweepTotal(0);
    int ret8 = Furuno::RmSetTileEchoCallbackFunc(EchoTileCallback);

    printf("RmSetServerClientMode      : %d\n", ret1);
    printf("RmInitialModule            : %d\n", ret2);
    printf("RmaInitialArpaModule       : %d\n", ret3);
    printf("RmSetEchoCallbackFunc      : %d\n", ret4);
    printf("RmSetCycleCallbackFunc     : %d\n", ret5);
    printf("RmSetEchoInterpolationFunc : %d\n", ret6);
    printf("RmSetEchoSweepTotal        : %d\n", ret7);
    printf("RmSetTileEchoCallbackFunc  : %d\n", ret8);

    if (ret1 || ret2 || ret3 || ret4 || ret5 || ret6 || ret7 || ret8) exit(1);

    printf("StartRadarModule : OK!\n");
}
	

// ========== RADAR CALLBACKS ==========
static void __stdcall RadarEchoCallback(
    Furuno::INT32 no, Furuno::INT16 status, Furuno::UINT8 *echo,
    Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 range,
    Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg)
{
    // Perf counters (update atomics only once)
    Perf::bytesPerSec += sweep_len;
    Perf::linesPerSec++;

    // Print stats every second
    uint64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    if (nowUs - Perf::lastPrintUs > 1'000'000) {
        struct rusage ru {};
        getrusage(RUSAGE_SELF, &ru);
        printf("sweep_len=%4d  lines=%5u  kB=%6.1f  CPU(u|s)=%.1f|%.1f\n",
            sweep_len,
            Perf::linesPerSec.load(),
            Perf::bytesPerSec.load() / 1024.0,
            ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6,
            ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6);
        Perf::bytesPerSec = Perf::linesPerSec = 0;
        Perf::lastPrintUs = nowUs;
    }

    // Custom gain for this pulse
    Furuno::UINT16 gain = 0;
    Furuno::RmcGetCustomGAIN(no, nullptr, nullptr, &gain, nullptr);

    // Store radar echo data
    radarEchoBuffer.push_back({status, sweep_len, scale, range, angle, gain, {}});
    memcpy(radarEchoBuffer.back().echo, echo, sweep_len);

    // Full rotation check
    if (lastSweepAngle != -1 && (angle < lastSweepAngle || angle > 8196)) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);

        char timeStr[20];
        std::strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", std::localtime(&now_c));

        std::ostringstream filename;
        filename << timeStr << "_" << std::setw(3) << std::setfill('0') << ms.count();

        std::string base = filename.str();
        fs::path csvPath = fs::path("data") / (base + ".csv");
        fs::path pngPath = fs::path("frames") / (base + ".csv.png");

        WriteEchoCSV(radarEchoBuffer, csvPath.string());
        WriteEchoImage(radarEchoBuffer, pngPath.string());

        // Trigger gain update every other rotation
        if (!GainControl::rotationToggle.exchange(!GainControl::rotationToggle, std::memory_order_acq_rel))
            GainControl::updateGain.store(true, std::memory_order_release);

        radarEchoBuffer.clear();
        lastSweepAngle = -1;
    } else {
        lastSweepAngle = angle;
    }
}

static void __stdcall RadarCycleCallback(void) {
	Furuno::INT32 connect = 0;
	Furuno::RmGetConnection(&connect);

	if (connectionStatus != connect) {
		printf(connect ? "Connected!\n" : "Disconnected!\n");
		connectionStatus = connect;
	}

	Furuno::UINT32 lastError = Furuno::LeGetLastError(Furuno::ERRORTYPE_BACKGROUND);
	if (lastError != FEC_ERROR_OK) {
		printf("Background Error = %x\n", lastError);
		Furuno::LeClearLastBackgroundError();
	}
}
	
static void __stdcall EchoTileCallback(Furuno::INT32 radarNo, Furuno::INT16 heading, Furuno::INT16 headingStatus, Furuno::INT16 range, Furuno::INT16 rangeUnit, Furuno::INT16 scale, Furuno::INT16 xPos, Furuno::INT16 yPos, Furuno::INT16 sweepCenterX, Furuno::INT16 sweepCenterY, Furuno::INT16 tileSize, unsigned char *echo){}

void __stdcall CommandCallback(Furuno::INT32 id, Furuno::INT32 no) {}


// ========== FILE/DATA PROCESSING ==========
static int ReadLicenseFile(const char *path, char vendorlic[4][VENDOR_TXT_LINE_CHAR_MAX]) {
    const char* keywords[4] = { VENDOR_CODE_KEYWORD, KEY_CODE_KEYWORD, VENDOR_ID_KEYWORD, DEVICE_ID_KEYWORD };
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    for (int i = 0; i < 4; ++i) {
        char line[VENDOR_TXT_LINE_CHAR_MAX] = {0};
        if (!fgets(line, sizeof(line), fp)) { fclose(fp); return -1; }
        size_t len = strlen(keywords[i]);

        if (strncmp(line, keywords[i], len) != 0) { fclose(fp); return -1; }
        char *val = strchr(line, ':');

        if (!val || !*(++val)) { fclose(fp); return -1; }
        while (*val == ' ' || *val == '\t') ++val;
        int j = 0;

        while (*val && *val != '\n' && *val != '\r' && j < VENDOR_TXT_LINE_CHAR_MAX - 1)
            vendorlic[i][j++] = *val++;
        vendorlic[i][j] = 0; // null terminate
    }
    fclose(fp);
    return 0;
}

void WriteEchoImage(const std::vector<RadarEchoData>& dataBuffer, const std::string& filename) {
    if (dataBuffer.empty()) return;

    // Find max sweep length
    int maxRadius = 0;
    for (const auto& e : dataBuffer) if (e.sweep_len > maxRadius) maxRadius = e.sweep_len;
    if (maxRadius <= 0) return;

    int w = 2 * maxRadius, h = 2 * maxRadius, cx = maxRadius, cy = maxRadius;
    std::vector<uint8_t> px(w * h * 4, 0);
    for (int i = 0; i < w * h; ++i) px[i * 4 + 3] = 255; // alpha

    // Draw echoes
    constexpr double TWO_PI = 2.0 * M_PI;
    for (const auto& entry : dataBuffer) {
        double angleRad = (entry.angle / 8196.0) * TWO_PI;
        for (int r = 0; r < entry.sweep_len; ++r) {
            int x = int(std::round(cx + r * std::cos(angleRad)));
            int y = int(std::round(cy + r * std::sin(angleRad)));
            if (x < 0 || x >= w || y < 0 || y >= h) continue;
            int idx = (y * w + x) * 4;
            px[idx] = px[idx + 1] = px[idx + 2] = entry.echo[r];
        }
    }

    // Detect gain change angles
    int lastGain = -1, firstAngle = -1;
    std::vector<int> gainChangeAngles;
    for (const auto& entry : dataBuffer) {
        if (firstAngle < 0) firstAngle = entry.angle;
        if (lastGain < 0) lastGain = entry.gain;
        else if (entry.gain != lastGain) gainChangeAngles.push_back(entry.angle), lastGain = entry.gain;
    }

    // Draw lines for gain changes
    auto drawLine = [&](double angleRad, uint8_t r, uint8_t g, uint8_t b) {
        for (int d = 0; d < maxRadius; ++d) {
            int x = int(std::round(cx + d * std::cos(angleRad)));
            int y = int(std::round(cy + d * std::sin(angleRad)));
            if (x < 0 || x >= w || y < 0 || y >= h) continue;
            int idx = (y * w + x) * 4;
            px[idx] = r; px[idx + 1] = g; px[idx + 2] = b;
        }
    };
    if (gainChangeAngles.empty() && firstAngle >= 0) {
        drawLine((firstAngle / 8196.0) * TWO_PI, 0, 255, 0);
    } else {
        for (int angle : gainChangeAngles)
            drawLine((angle / 8196.0) * TWO_PI, 255, 0, 0);
    }

    if (!stbi_write_png(filename.c_str(), w, h, 4, px.data(), w * 4))
        fprintf(stderr, "Failed to write PNG: %s\n", filename.c_str());
}

void WriteEchoCSV(const std::vector<RadarEchoData>& dataBuffer, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }
    file << "Status,Scale,Range,Gain,Angle,EchoValues\n";
    for (const auto& d : dataBuffer) {
        file << d.status << ',' << d.scale << ',' << d.range << ',' << d.gain << ',' << d.angle;
        for (int j = 0; j < d.sweep_len; ++j) file << ',' << int(d.echo[j]);
        file << '\n';
    }
}

// ========== UTILITIES ==========

static void PrintSensorList() {
	int status, count = 0;
	FILE *fp = fopen("../Sensor_list.txt", "r");
	char num_ID[200], equipment[200], sensor_name[200];

	if (!fp) {
		puts("\t\t Missing file [Sensor_list.txt]");
		puts("\t\t Ignored ID check");
		return;
	}

	puts("\t\t Sensors in your target.dat");
	while (fscanf(fp, "%s , %[^ , ]  , Sensor : %s", num_ID, equipment, sensor_name) != EOF) {
		if (Furuno::FlmCertificateEquipment((Furuno::UINT8 *)equipment, (Furuno::UINT8 *)num_ID, &status) == 0) {
			printf("\t\t %s\tSensor : %s \n", equipment, sensor_name);
			count++;
		}
	}
	if (count == 0)
		puts("\t\t Not included in [Sensor_list.txt] \r\n\t\t Please update [License : target.dat ]\r\n");
	fclose(fp);
}
				