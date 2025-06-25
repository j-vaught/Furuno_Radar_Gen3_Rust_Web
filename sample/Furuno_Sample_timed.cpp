#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <cmath>      // for cos, sin, M_PI
#include <cstdint>
#include <algorithm>  // for std::max
#include "stb_image_write.h"
#include <cstdio>
#include <atomic>


#ifdef USE_BOOST_WEB_INTERFACE
#include "web_server.h"
#endif

namespace Furuno
{
	#include "nndll.h"
	#include <lasterror.h>
	
	// Structures
	struct FurunoPermissionData
	{
		Furuno::INT32 vendor_code_size;
		Furuno::UINT8 vendor_code[64];
		Furuno::INT32 password_size;
		Furuno::UINT8 password[64];
		Furuno::INT32 vendor_id_size;
		Furuno::UINT8 vendor_id[8];
		Furuno::INT32 encrypted_device_info_size;
		Furuno::UINT8 encrypted_device_info[40];
		Furuno::INT32 preset_device_id_size; // Use only iOS
		Furuno::UINT8 preset_device_id[40];	 // Use only iOS
	};
}
// Forward declarations
static void __stdcall RadarEchoDraw(Furuno::INT32 no, Furuno::INT16 status, Furuno::UINT8 *echo, Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 range, Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg);
static void __stdcall RadarCycleFunc(void);
static void __stdcall CommandCallback(Furuno::INT32 id, Furuno::INT32 no);
static void __stdcall EchoTileCallback(Furuno::INT32 radarNo, Furuno::INT16 heading, Furuno::INT16 headingStatus, Furuno::INT16 range, Furuno::INT16 rangeUnit, Furuno::INT16 scale, Furuno::INT16 xPos, Furuno::INT16 yPos, Furuno::INT16 sweepCenterX, Furuno::INT16 sweepCenterY, Furuno::INT16 tileSize, unsigned char *echo);
static void ReturnValueCheck(Furuno::INT32 ret);
static Furuno::INT32 connectionStatus = -1;
static Furuno::INT32 dispOnlyResult = 0;
static int UtilStrtol(const char *nptr, const char **endptr, int base, unsigned char *onum);

// Define a structure to hold the radar echo data
struct RadarEchoData {
	Furuno::INT32 no;
	Furuno::INT16 status;
	Furuno::UINT8 echo[1024];
	Furuno::INT16 sweep_len;
	Furuno::INT16 scale;
	Furuno::INT16 range;
	Furuno::INT16 angle;
	Furuno::INT16 heading;
	Furuno::INT16 h_flg;
	Furuno::UINT16 gain;
};
// Vector to accumulate the radar echo data for a full rotation
std::vector<RadarEchoData> radarEchoDataBuffer;

static Furuno::INT16 previousAngle = -1; // Initialize with an invalid angle

using std::this_thread::sleep_for;

#define VENDOR_TXT_LINE_CHAR_MAX (150)
#define VENDOR_CODE_KEYWORD ("vendor code")
#define LICENSE_FILE_KEYWORD ("license file")
#define KEY_CODE_KEYWORD ("keycode")
#define VENDOR_ID_KEYWORD ("vendor id")
#define DEVICE_ID_KEYWORD ("device id")
bool License(std::string vendorCode, std::string vendorPassword, std::string vendorId, std::string deviceInfo);
bool TargetLicense(std::string filePath);
void HexStringToBytes(const std::string strIn, unsigned char *&array, int &size);
static void P_SensorNo(void);

static void StopExecution(void);
static void StartRadarModule();

static int ReadVendorTxt(const char *vendortxtpath, char vendorlic[4][VENDOR_TXT_LINE_CHAR_MAX])
{
	FILE *fp = NULL;
	char *ptr = NULL;
	int lineindex = 0, charindex = 0;
	char readline[VENDOR_TXT_LINE_CHAR_MAX] = { '\0' };
	
	if ((fp = fopen(vendortxtpath, "r")) == NULL)
	{
		return -1;
	}
	for (lineindex = 0; lineindex <= 3; lineindex++)
	{
		if (fgets(readline, VENDOR_TXT_LINE_CHAR_MAX, fp) != NULL)
		{
			if (lineindex == 0)
			{
				if (strncmp(readline, VENDOR_CODE_KEYWORD, strlen(VENDOR_CODE_KEYWORD)) != 0)
				{
					fclose(fp);
					return -1;
				}
			}
			else if (lineindex == 1)
			{
				if (strncmp(readline, KEY_CODE_KEYWORD, strlen(KEY_CODE_KEYWORD)) != 0)
				{
					fclose(fp);
					return -1;
				}
			}
			else if (lineindex == 2)
			{
				if (strncmp(readline, VENDOR_ID_KEYWORD, strlen(VENDOR_ID_KEYWORD)) != 0)
				{
					fclose(fp);
					return -1;
				}
			}
			else if (lineindex == 3)
			{
				if (strncmp(readline, DEVICE_ID_KEYWORD, strlen(DEVICE_ID_KEYWORD)) != 0)
				{
					fclose(fp);
					return -1;
				}
			}
			
			ptr = strstr(readline, ":");
			if (ptr != NULL)
			{
				if (strlen(ptr) >= 1)
				{
					ptr++;
					for (charindex = 0; ptr[charindex] != 0 && ptr[charindex] != '\r' && ptr[charindex] != '\n'; charindex++)
					{
						vendorlic[lineindex][charindex] = ptr[charindex];
					}
				}
				else
				{
					fclose(fp);
					return -1;
				}
			}
			else
			{
				fclose(fp);
				return -1;
			}
		}
		else
		{
			fclose(fp);
			return -1;
		}
	}
	fclose(fp);
	return 0;
}


static std::atomic_bool g_stopGainThread{false};
void StartAutoGainThread(int radarNo)
{
    std::thread([radarNo]()
    {
        // Our repeating pattern of gain values:
        static const int pattern[] = {5, 15, 25, 35, 45, 55, 65, 75, 85, 95};
        const size_t patternCount = sizeof(pattern) / sizeof(pattern[0]);

        // Start at pattern index 0
        size_t idx = 0;

        while (!g_stopGainThread.load())
        {
            int manualVal = pattern[idx];

            // Set the radar to MANUAL gain = manualVal (0..100)
            Furuno::INT32 ret = Furuno::RmcSetGAIN(
                radarNo, 
                /*status=*/0,                  // 0=MANUAL
                (Furuno::UINT16)manualVal, 
                /*autoVal=*/0                  // not used in MANUAL mode
            );

            // Print the result
            printf("[AutoGainThread] RmcSetGAIN -> manualVal=%d, ret=%d\n",
                   manualVal, (int)ret);

            // Sleep longer (4s) if we are on the 5 gain (index 0),
            // otherwise sleep for 2 seconds.
            if (idx == 0) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }

            // Advance to the next gain value, loop back after 95
            idx = (idx + 1) % patternCount;
        }
    }).detach(); // Fire-and-forget background thread
}


int main(int argc, char *argv[]) // argc is count inputs , argv[] is input charactor
{
	Furuno::INT32 fcode[3] = { 0 };
	int ret, i;
	int dualRangeMode = 0;
	Furuno::UINT8 ip_addr[4] = { 0, 0, 0, 0 };
	int d_count = 0;
	char *ip1, *ip2, *ip3, *ip4, ip_buf[16], *ipbufPoint;
	Furuno::UINT8 hostName[8 + 1];			  // HOST NAME for PC
	Furuno::UINT8 radarHostName[8 + 1] = { 0 }; // HOST NAME for Radar Sensor
	Furuno::INT32 radarHostCount = 0;
	Furuno::UINT8 radarHostNameList[4][8 + 1] = { 0 };
	Furuno::UINT8 radarNotFindCount = 0;
	long int sensorNo;
	Furuno::UINT8 versionNoForFnetModule[16];
	Furuno::UINT8 command[32];
	long int radarNo;
	int value1, value2, value3, value4, value5, value6, value7, value8;
	long int lvalue1, lvalue2, lvalue3, lvalue4;
	Furuno::UINT8 bvalue1[21], bvalue2[21], bvalue3[21], bvalue4[21], bvalue5[21], bvalue6[21], bvalue7[21];
	Furuno::UINT8 freeID[32] = { 0 }, freetext[128] = { 0 }, keyCom[100] = { 0 };
	Furuno::INT32 dist[10], dir[10], lat[10], lon[10];
	Furuno::INT16 dateTable[27];
	Furuno::UINT16 interswTable[8];
	Furuno::UINT8 dispname[8 + 1];
	Furuno::UINT16 rangeTable[25];
	Furuno::UINT32 lastError = FEC_ERROR_OK;
	
	setvbuf(stdout, NULL, _IONBF, 0);
	char vendorlic[4][VENDOR_TXT_LINE_CHAR_MAX] = { '\0' };
	
	printf("***	argv[3] [IPv4 address]		: [%s]\n", argv[3]);
	printf("***	argv[4] [hostname]		: [%s]\n", argv[4]);
	
	// Hostname
	if (strlen(argv[4]) <= sizeof(hostName) - 1)
	{
		memset(hostName, 0, sizeof(hostName));
		strncpy((char *)hostName, argv[4], (sizeof(hostName) - 1));
	}
	
	sleep_for(std::chrono::milliseconds(500));
	printf("\nStart sample code!\n");
	
	// Entry functions
	ret = Furuno::fec_entry_function("libNAVNETDLL.so");
	switch (ret)
	{
		case ENTRY_OK:
		printf("%d", ret);
		break;
	}
	
	// Initial
	sleep_for(std::chrono::milliseconds(500));
	Furuno::InitialITRON();
	Furuno::FecError error = Furuno::InitError;
	sleep_for(std::chrono::milliseconds(500));
	ret = ReadVendorTxt(argv[1], vendorlic) != 0;
	Furuno::LeClearLastAPIError();
	
	setenv("FEC_NAVNETDLL_TARGET", argv[2], 1);
	error = Furuno::Initialize((Furuno::UINT8 *)vendorlic[0], (Furuno::UINT8 *)vendorlic[1], (Furuno::UINT8 *)vendorlic[2], (Furuno::UINT8 *)vendorlic[3], ip_addr); /* vendor, password, vendorid, deviceinfo -> FLM_info check */
	lastError = Furuno::LeGetLastError(Furuno::ERRORTYPE_API);
	
	if (error != Furuno::None)
	{
		printf("Error = %x\n", lastError);
		StopExecution();
		return -1;
	}
	sleep_for(std::chrono::milliseconds(500));
	strncpy(ip_buf, argv[3], sizeof(ip_buf));
	for (int i = 0; i < sizeof(ip_buf); i++)
	{
		if (ip_buf[i] == '.')
		{
			d_count++;
		}
	}
	ip1 = ip_buf;
	ipbufPoint = strchr(ip1, '.');
	
	*ipbufPoint = 0;
	ip2 = ipbufPoint + 1;
	ipbufPoint = strchr(ip2, '.');
	
	*ipbufPoint = 0;
	ip3 = ipbufPoint + 1;
	ipbufPoint = strchr(ip3, '.');
	
	*ipbufPoint = 0;
	ip4 = ipbufPoint + 1;
	
	if ((UtilStrtol(ip1, NULL, 10, &ip_addr[0]) != 0) || (UtilStrtol(ip2, NULL, 10, &ip_addr[1]) != 0) || (UtilStrtol(ip3, NULL, 10, &ip_addr[2]) != 0) || (UtilStrtol(ip4, NULL, 10, &ip_addr[3]) != 0))
	{
		printf("Please change IP address to correct numeric value\n");
		StopExecution();
		return -1;
	}
	
	printf("%d.%d.%d.%d \n", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
	int ret1 = Furuno::NcSetNetworkIpAddress(ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
	Furuno::NcGetModuleVersionNo(versionNoForFnetModule); // both enter No.1[NcGetModuleVersionNo]
	printf("Fnet Module Version No.    : ver.%s\n", versionNoForFnetModule);
	sleep_for(std::chrono::milliseconds(500));
	
	ret = Furuno::NcInitialNavnetCommon(hostName, fcode);
	Furuno::NcInitialNmea183Server();		//[extras] It initializes the NMEA0183 LAN functionality.
	Furuno::NcInitialNmea0183PortStatus(1); //[extras] Nothing API specification
	printf("\t------------------------------------------------------------------\t\n");
	P_SensorNo();
	
	printf("Init Radar Module...\n");
	sleep_for(std::chrono::milliseconds(500));
	StartRadarModule();
	#ifdef USE_BOOST_WEB_INTERFACE
	printf("Starting local web server on port 8080...\n");
	StartWebServerThread();
	#endif
	
	// Find the Radar Sensor
	printf("\nSearching the Radar Sensor");
	sleep_for(std::chrono::milliseconds(500));
	while (1)
	{
		Furuno::RmGetSensorHostCount(&radarHostCount);
		if (radarHostCount > 0)
		{
			for (i = 0; i < radarHostCount; i++)
			{
				Furuno::RmGetSensorHostName(i, radarHostNameList[i]);
				printf("radarHostNameList[%d] : %s\n", i, radarHostNameList[i]);
			}
			printf("\tRadar Sensor were found.\n");
			break;
		}
		if (radarNotFindCount == 60)
		{
			printf("\nEtr sensor could not find.\n");
			printf("Please retry.\n");
			getchar();
			StopExecution();
			return -1;
		}
		radarNotFindCount++;
		printf(".");
		sleep_for(std::chrono::milliseconds(500));
	}
	printf("\n");
	if (radarHostCount == 1)
	{
		strncpy((char *)radarHostName, (const char *)radarHostNameList[0], sizeof(radarHostName) - 1);
	}
	
	Furuno::RmSetRadarSource(0, radarHostName, 0);

	    // -----------------------------------------------------------------------
    // >>>> Start the auto-gain incrementing thread on radarNo=0 <<<<
    StartAutoGainThread(0);
    // -----------------------------------------------------------------------

	sleep_for(std::chrono::milliseconds(500));
	while (1)
	{
		if (connectionStatus == 1)
		{
			printf("Input Radar command Number(0 - 159) or GetRadarCommand( w=ON / x=OFF ) or GetRadarEcho:TxStby( y=ON / z=OFF ):");
			ret = scanf("%s", command);
			
			// Change [else if]-> [if]  (Error avoidance No.C1061)
			if (strcmp("w", (const char *)command) == 0)
			{
				dispOnlyResult = 0;
				ret = Furuno::RmSetCommandCallbackFunc(CommandCallback);
			}
			
			if (strcmp("x", (const char *)command) == 0)
			{
				dispOnlyResult = 0;
				ret = Furuno::RmSetCommandCallbackFunc(NULL);
			}
			
			if (strcmp("y", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				value1 = 2; //	(0=WARMUP/1=STDBY/2=TX)
				ret = Furuno::RmcSetTxStby(radarNo, (Furuno::UINT16)value1, 0, 0, 0);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("z", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				value1 = 1; //	(0=WARMUP/1=STDBY/2=TX)
				ret = Furuno::RmcSetTxStby(radarNo, (Furuno::UINT16)value1, 0, 0, 0);
				ReturnValueCheck(ret);
			}
			if (strcmp("0", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=not connecting/1=connecting) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetConnect(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}
			
			
			if (strcmp("2", (const char *)command) == 0)
			{  
				// This API RmcSetRange is an old function, Please use new API RmcSetRange_v2 as the old API will not be updated in the future.
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: range(0 - 24)/(50 - 54) ");
				ret = scanf("%d", (int *)&value1);
				if (value1 >= 0 && value1 <= 24){
					printf("Input value: unit(0=NM/1=KM/2=SM/3=Kyd) = ");
					ret = scanf("%d", (int *)&value2);
					ret = Furuno::RmcSetRange_v2(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, 0);
					ReturnValueCheck(ret);
				}
				else if (value1 >= 50 && value1 <= 54){
					printf("Input value: unit(0=m/1=yd) = ");
					ret = scanf("%d", (int *)&value2);
					ret = Furuno::RmcSetRange_v2(radarNo, (Furuno::UINT16)value1, 0, (Furuno::UINT16)value2);
					ReturnValueCheck(ret);
				}else{
					printf("the value of range is not valid!\n");
				}
			}
			
			if (strcmp("3", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=MANUAL/1=AUTO) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: manual value(0 - 100) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: auto value(0 - 100) = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetGAIN(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("4", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=MANUAL/1=AUTO) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: manual value(0 - 100) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: auto value(0 - 100) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: auto mode(0=Advanced/1=Coastal) = ");
				ret = scanf("%d", (int *)&value4);
				ret = Furuno::RmcSetACSEA(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, (Furuno::UINT16)value4);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("5", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=MANUAL/1=AUTO) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: manual value(0 - 100) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: auto value(0 - 100) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: ftcMode(0=OFF/1=FTC-1/2=FTC-2) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: autoRainMode(0=Calm/1=Moderate/2=Rough) = ");
				ret = scanf("%d", (int *)&value5);
				ret = Furuno::RmcSetACRAIN(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, (Furuno::UINT16)value4, (Furuno::UINT16)value5);
				ReturnValueCheck(ret);
			}
			
			
			
			if (strcmp("8", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: pulse(0=S1/1=S2/2=M1/3=M2/4=M3/5=L) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: range(0 - 21) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: unit(0=NM/1=KM/3=SM/4=Kyd) = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetPulseWidth(radarNo, (Furuno::INT16)value1, (Furuno::INT16)value2, (Furuno::UINT16)value3, 0);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("9", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=WARMUP/1=STDBY/2=TX) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetTxStby(radarNo, (Furuno::UINT16)value1, 0, 0, 0);
				ReturnValueCheck(ret);
			}
			
			
			if (strcmp("37", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: STC curve(0=2/1=2.5/2=3/3=3.5/4=4.2) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetNearSTC(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("38", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: STC curve(0=3/1=4/2=5/3=6) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetMiddleSTC(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("39", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: STC curve(0=6/1=7/2=8) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetFarSTC(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("40", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: adjustment value(0 - 255) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetRingSuppression(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("41", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: rolling speed(0=LO/1=HI/2=AUTO) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: antenna high rotation mode(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetAntennaRevolution(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("51", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSet2ndEchoReject(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("140", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Transmission channel number(1 - 2) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetSsdTxChannel(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("149", (const char *)command) == 0)//NEEDED fpor radar echo
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: sub command = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: parameter1 = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				printf("Input value: parameter2 = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: parameter3 = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: parameter4 = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetNN3Command(radarNo, lvalue1, lvalue2, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3);
				ReturnValueCheck(ret);
			}
			
			if (strcmp("158", (const char *)command) == 0)//NEEDED fpor radar echo
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: unit(0=NM/1=KM/2=SM/3=KYD) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				for (i = 0; i < 24; i++)
				{
					printf("Input value: select range code[%d] = ", i);
					ret = scanf("%d", (int *)&rangeTable[i]);
				}
				ret = Furuno::RmcSetRangeSelect(radarNo, lvalue1, rangeTable);
				ReturnValueCheck(ret);
			}
		}
		sleep_for(std::chrono::milliseconds(500));
	}
	
	return 0;
}

void ReturnValueCheck(Furuno::INT32 ret)
{
	if (ret == 0)
	{
		printf("RM_OK(0)\n");
	}
	else if (ret == -1)
	{
		printf("RM_NG(-1)\n");
	}
	else if (ret == -2)
	{
		printf("RM_NODATA(-2)\n");
	}
	else if (ret == -3)
	{
		printf("RM_INIT(-3)\n");
	}
	else if (ret == -4)
	{
		printf("RM_PARAM_ERROR(-4)\n");
	}
	else if (ret == -5)
	{
		printf("RM_SEND_ERROR(-5)\n");
	}
	else if (ret == -7)
	{
		printf("RM_WTC_GZOFF(-7)\n");
	}
	else if (ret == -8)
	{
		printf("RM_WTC_OUTPROC(-8)\n");
	}
	else if (ret == -9)
	{
		printf("RM_CERT_ERROR(-9)\n");
	}
	else
	{
		printf("OTHER_ERROR\n");
	}
}

#if WSTRICTALIASING
static struct sockaddr_in *ToSockaddrIn(void *p)
{
	return (struct sockaddr_in *)p;
}
#endif

void __stdcall CommandCallback(Furuno::INT32 id, Furuno::INT32 no)
{
	printf("Command callback: (comID:%d\t,RadarNo:%d)\n", id, no);
}




static void DrawLineFromCenter(
    std::vector<uint8_t>& pixels,
    int imgWidth,
    int imgHeight,
    int centerX,
    int centerY,
    double angleRad,
    int maxRadius,
    uint8_t red,
    uint8_t green,
    uint8_t blue)
{
    for(int r = 0; r < maxRadius; ++r)
    {
        int x = static_cast<int>(std::round(centerX + r * std::cos(angleRad)));
        int y = static_cast<int>(std::round(centerY + r * std::sin(angleRad)));
        // Make sure we don't go out of image bounds
        if (x < 0 || x >= imgWidth || y < 0 || y >= imgHeight)
            continue;

        // Calculate pixel index and set the color
        int idx = (y * imgWidth + x) * 4;
        pixels[idx + 0] = red;
        pixels[idx + 1] = green;
        pixels[idx + 2] = blue;
        pixels[idx + 3] = 255; // Alpha
    }
}

void SaveRadarEchoAsPNG(const std::vector<RadarEchoData>& dataBuffer, const std::string& filename)
{
    if (dataBuffer.empty()) {
        return;
    }

    // Find maximum radius among all sweeps
    int maxRadius = 0;
    for (auto &entry : dataBuffer) {
        if (entry.sweep_len > maxRadius) {
            maxRadius = entry.sweep_len;
        }
    }
    if (maxRadius <= 0) {
        return;
    }

    const int imgWidth  = 2 * maxRadius;
    const int imgHeight = 2 * maxRadius;

    // Prepare an RGBA buffer (4 bytes per pixel)
    std::vector<uint8_t> pixels(imgWidth * imgHeight * 4, 0);

    // Set all alpha channels to 255 (fully opaque)
    for (int i = 0; i < imgWidth * imgHeight; i++) {
        pixels[i * 4 + 3] = 255;  // alpha
    }

    const double TWO_PI = 2.0 * M_PI;
    const int centerX = maxRadius;
    const int centerY = maxRadius;

    // First, draw the radar echo data
    for (auto &entry : dataBuffer) {
        // Convert the angle from the 0..8196 range to radians [0..2PI)
        double angleRaw = static_cast<double>(entry.angle);
        double angleRad = (angleRaw / 8196.0) * TWO_PI;

        for (int r = 0; r < entry.sweep_len; r++) {
            uint8_t val = entry.echo[r];
            double xD = static_cast<double>(centerX) + r * std::cos(angleRad);
            double yD = static_cast<double>(centerY) + r * std::sin(angleRad);
            int x = static_cast<int>(std::round(xD));
            int y = static_cast<int>(std::round(yD));

            // Bounds check
            if (x < 0 || x >= imgWidth || y < 0 || y >= imgHeight) {
                continue;
            }

            int idx = (y * imgWidth + x) * 4;
            // For simplicity, use echo value for R/G/B (grayish pixel)
            pixels[idx + 0] = val;
            pixels[idx + 1] = val;
            pixels[idx + 2] = val;
            pixels[idx + 3] = 255; // alpha
        }
    }

    //
    // 2) After drawing the echoes, figure out whether the gain changed:
    //
    int lastGain = -1;
    int firstAngle = -1;
    std::vector<int> gainChangeAngles; // angles where gain changed

    for (size_t i = 0; i < dataBuffer.size(); ++i)
    {
        const RadarEchoData& entry = dataBuffer[i];
        if (firstAngle < 0) {
            firstAngle = entry.angle;  // store first angle we encounter
        }
        if (lastGain < 0) {
            // first time, just remember the gain
            lastGain = entry.gain;
        } else {
            // if gain changed compared to the previous pulse
            if (entry.gain != lastGain) {
                gainChangeAngles.push_back(entry.angle);
            }
            lastGain = entry.gain;
        }
    }

    // 3) Draw lines:
    //    - If gainChangeAngles is empty => no change => draw green line at firstAngle
    //    - Otherwise => draw red line(s) at each angle where the gain changed
    if (gainChangeAngles.empty()) {
        // No gain change => draw a green line at firstAngle
        if (firstAngle >= 0) {
            double angleRad = (static_cast<double>(firstAngle) / 8196.0) * TWO_PI;
            // Draw line in green
            DrawLineFromCenter(pixels, imgWidth, imgHeight,
                               centerX, centerY, angleRad, maxRadius,
                               /*red=*/0, /*green=*/255, /*blue=*/0);
        }
    } else {
        // We have at least one gain change; draw each as a red line
        for (int angle : gainChangeAngles) {
            double angleRad = (static_cast<double>(angle) / 8196.0) * TWO_PI;
            // Draw line in red
            DrawLineFromCenter(pixels, imgWidth, imgHeight,
                               centerX, centerY, angleRad, maxRadius,
                               /*red=*/255, /*green=*/0, /*blue=*/0);
        }
    }

    // Finally, write out the PNG with the lines rendered
    int strideInBytes = imgWidth * 4;
    if (!stbi_write_png(filename.c_str(), imgWidth, imgHeight, 4, pixels.data(), strideInBytes)) {
        fprintf(stderr, "Failed to write PNG: %s\n", filename.c_str());
    }
}


// Function to save the accumulated data to a CSV file
void SaveRadarEchoDataToCSV(const std::vector<RadarEchoData>& dataBuffer, const std::string& filename) {
	std::ofstream file(filename);
	if (!file.is_open()) {
		std::cerr << "Error opening file for writing: " << filename << std::endl;
		return;
	}
	
	// Write CSV header: Gain first, then the other desired fields.
	file << "Status,SweepLength,Scale,Range,Gain,Angle,EchoValues\n";
	
	// Write the data rows
	for (size_t i = 0; i < dataBuffer.size(); ++i) {
		const RadarEchoData& data = dataBuffer[i];
		file << data.status << "," << data.sweep_len << "," 
		<< data.scale << "," << data.range << "," << data.gain << "," << data.angle;
		// Write each echo value
		for (int j = 0; j < data.sweep_len; ++j) {
			file << "," << static_cast<int>(data.echo[j]);
		}
		file << "\n";
	}
	file.close();
}


// Modified RadarEchoDraw function
static void __stdcall RadarEchoDraw(Furuno::INT32 no, Furuno::INT16 status, Furuno::UINT8 *echo, 
	Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 range, 
	Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg) {
		// Call the custom gain function for every pulse
		Furuno::INT32 l1 = -2;
		Furuno::UINT16 us1 = 0, gain = 0, us3 = 0;
		int retCustom = Furuno::RmcGetCustomGAIN(no, &l1, &us1, &gain, &us3);
		// us2 now holds the custom gain for this pulse
		
		// Accumulate the radar echo data and store the gain
		RadarEchoData data;
		data.status = status;
		data.sweep_len = sweep_len;
		data.scale = scale;
		data.range = range;
		data.angle = angle;
		data.gain = gain;  // store the gain measurement
		
		memcpy(data.echo, echo, sweep_len);
		radarEchoDataBuffer.push_back(data);
		

		// Check if the current angle indicates a full rotation.
    if (previousAngle != -1 && (angle < previousAngle || angle > 8196)) {
        // Get the current time with high resolution
        auto now = std::chrono::system_clock::now();
        // Extract milliseconds from the current time
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()) % 1000;
        // Convert to time_t to use with std::localtime
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm *localTime = std::localtime(&now_c);

        char timeStr[20];
        std::strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", localTime);

        // Build the filename with milliseconds included
        std::ostringstream filename;
        filename << timeStr << "_" << std::setfill('0') << std::setw(3) << ms.count() << ".csv";

        // Save CSV and PNG files with the new time-based name
        SaveRadarEchoDataToCSV(radarEchoDataBuffer, filename.str());
        SaveRadarEchoAsPNG(radarEchoDataBuffer, filename.str() + ".png");

        // Clear the data for the next rotation
        radarEchoDataBuffer.clear();
        previousAngle = -1;
    }
    else {
        // Update previousAngle for the next callback
        previousAngle = angle;
    }
	}
	
	
	/* Radar echo tile callback function */
	static void __stdcall EchoTileCallback(Furuno::INT32 radarNo, Furuno::INT16 heading, Furuno::INT16 headingStatus, Furuno::INT16 range, Furuno::INT16 rangeUnit, Furuno::INT16 scale, Furuno::INT16 xPos, Furuno::INT16 yPos, Furuno::INT16 sweepCenterX, Furuno::INT16 sweepCenterY, Furuno::INT16 tileSize, unsigned char *echo)
	{}
	
	static void __stdcall RadarCycleFunc(void)
	{
		Furuno::INT32 connect = 0;
		Furuno::UINT32 lastError = FEC_ERROR_OK;
		
		Furuno::RmGetConnection(&connect);
		if (connectionStatus != connect)
		{
			if ((connect == 0) && (connectionStatus == 1))
			{
				printf("Disconnected!\n");
			}
			else if (connect)
			{
				printf("Connected!\n");
			}
			connectionStatus = connect;
		}
		
		/* get LastError of NavNetLibrary Background */
		lastError = Furuno::LeGetLastError(Furuno::ERRORTYPE_BACKGROUND);
		if (lastError != FEC_ERROR_OK)
		{
			printf("Background Error = %x\n", lastError);
			
			/* initialize LastError of NavNetLibrary Background */
			Furuno::LeClearLastBackgroundError();
		}
	}
	
	const char *RMErrorString(int ret)
	{
		switch (ret)
		{
			case RM_OK:
			return ("RM_OK");
			break;
			case RM_NG:
			return ("RM_NG");
			break;
			case RM_NODATA:
			return ("RM_NODATA");
			break;
			case RM_INIT:
			return ("RM_INIT");
			break;
			case RM_PARAM_ERROR:
			return ("RM_PARAM_ERROR");
			break;
			case RM_SEND_ERROR:
			return ("RM_SEND_ERROR");
			break;
			case RM_CERT_ERROR:
			return ("RM_CERT_ERROR");
			break;
			default:
			return ("UNKNOWN");
		}
	}
	
	static void StartRadarModule()
	{
		Furuno::UINT8 versionNoForRadarModule[16];
		
		Furuno::RmGetModuleVersionNo(versionNoForRadarModule);
		printf("Radar Module Version No.   : ver.%s\n", versionNoForRadarModule);
		
		// Initialization for Radar Module
		int ret1 = Furuno::RmSetServerClientMode(0);
		int ret2 = Furuno::RmInitialModule((Furuno::UINT8 *)"", CommandCallback);
		int ret3 = Furuno::RmaInitialArpaModule();				   // Initializing the ARPA Module
		int ret4 = Furuno::RmSetEchoCallbackFunc(RadarEchoDraw);   // register the callback for Radar echo
		int ret5 = Furuno::RmSetCycleCallbackFunc(RadarCycleFunc); // register the callback for cycle function
		int ret6 = Furuno::RmSetEchoInterpolationFunc(1);
		int ret7 = Furuno::RmSetEchoSweepTotal(0);
		int ret8 = Furuno::RmSetTileEchoCallbackFunc(EchoTileCallback);
		
		printf("RmSetServerClientMode      : %s\n", RMErrorString(ret1));
		printf("RmInitialModule            : %s\n", RMErrorString(ret2));
		printf("RmaInitialArpaModule       : %s\n", RMErrorString(ret3));
		printf("RmSetEchoCallbackFunc      : %s\n", RMErrorString(ret4));
		printf("RmSetCycleCallbackFunc     : %s\n", RMErrorString(ret5));
		printf("RmSetEchoInterpolationFunc : %s\n", RMErrorString(ret6));
		printf("RmSetEchoSweepTotal        : %s\n", RMErrorString(ret7));
		printf("RmSetTileEchoCallbackFunc  : %s\n", RMErrorString(ret8));
		
		if (!((ret1 == RM_OK) && (ret2 == RM_OK) && (ret3 == RM_OK) && (ret4 == RM_OK) && (ret5 == RM_OK) && (ret6 == RM_OK) && (ret7 == RM_OK) && (ret8 == RM_OK)))
		{
			printf("StartRadarModule           : NG!\n");
			StopExecution();
			exit(0);
		}
		else
		{
			printf("StartRadarModule           : OK!\n");
		}
	}
	
	static void StopExecution()
	{
		printf("\n\tPlease Enter key, End of execution.\n");
		getchar();
	}
	
	/*  Convert string to number [Within a range of IP addresses]  */
	static int UtilStrtol(const char *nptr, const char **endptr, int base, unsigned char *onum)
	{
		long ulvalue = 0UL;
		char *tmp_endptr = NULL;
		
		if (nptr == NULL)
		{
			return -1;
		}
		
		ulvalue = strtol(nptr, &tmp_endptr, base);
		
		if (endptr != NULL)
		{
			*endptr = tmp_endptr;
		}
		
		if (tmp_endptr == nptr)
		{
			return -1;
		}
		if (ulvalue < 0 || ulvalue > 255)
		{
			return -1;
		}
		*onum = ulvalue;
		return 0;
	}
	
	/*  Check for certified devices from [Sensor_list.txt]*/
	static void P_SensorNo()
	{
		int status;
		int count = 0;
		FILE *fp = NULL;
		char num_ID[200] = { "" }, equipment[200] = { "" }, sensor_name[200] = { "" };
		fp = fopen("..//Sensor_list.txt", "r");
		
		if (fp == NULL) //[Sensor_list.txt] does not open
		{
			printf("\t\t Missing file [Sensor_list.txt] \n");
			printf("\t\t Ignored ID check \n");
		}
		else
		{
			printf("\t\t Sensors in your target.dat \n");
			
			//read in order line by line , [ID_No],[whitch one Sensor(Consider indentation)],[Sensor Name]
			while (fscanf(fp, "%s , %[^ , ]  , Sensor : %s", num_ID, equipment, sensor_name) != EOF)
			{
				if (Furuno::FlmCertificateEquipment((const Furuno::UINT8 *)equipment, (Furuno::UINT8 *)num_ID, &status) == 0)
				{
					printf("\t\t %s\tSensor : %s \n", equipment, sensor_name);
					count++;
				}
			}
			if (count == 0) //SensorNo is Nothing
			{
				printf("\t\t Not included in [Sensor_list.txt] \r\n\t\t Please update [License : target.dat ]\r\n");
			}
			fclose(fp);
		}
	}
