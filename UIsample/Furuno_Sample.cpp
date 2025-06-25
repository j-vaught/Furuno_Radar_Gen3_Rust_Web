/********************************************************************
* filename : main.c
*
* description : Sample Application for NAVNET DLL Initialization
*      The following processings are performed in this program.
*      1. Preparations to use the Navnet function
*      2. NAVNET DLL Initialization (Connection with Navnet equipment)
*      3. Radar command communication function Initialization
*      4. Radar echo receiving function Initialzation
*
*
* Copyright (C) 2009 Furuno Electric co.,ltd.
*
*
* Usage: a.out [ifname [machinename]]
********************************************************************/
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

#ifdef __linux__ /*__Linux__*/
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#elif defined(WIN32) /*  WIN32  */
#include <sstream>
#include <cstdio>
#include <fstream>
#include <Windows.h>

#else
#error "OS not supported"
#endif

namespace Furuno
{
#ifdef __linux__ /*__Linux__*/
#include "nndll.h"
#endif /*__Linux__*/

#ifdef WIN32 /*  WIN32  */
#include <FECLIB/fectype.h>
#include <radar.h>
#include <flm_api.h>
#include <common.h>
#include <common_error.h>
#include <radar_error.h>
#include <syscall.h>
#include <Logger.h>
#endif /*  WIN32  */

#include <lasterror.h>

#if FLM_COMPILE_NEW_LICENSE == 1
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
#else // TRIAL_LICENSE
	struct FurunoTimePermissionData
	{
		Furuno::INT32 time_code_size;
		Furuno::UINT8 time_code[40];
	};
#endif
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
using std::this_thread::sleep_for;

#if FLM_COMPILE_NEW_LICENSE == 1
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
#endif

static void StopExecution(void);
static void StartRadarModule();

#if FLM_COMPILE_NEW_LICENSE == 1
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

static void PrintVendorGuidance(void)
{
	printf("Please fix vendor.txt as following\n");
	printf(" %s\t: [%s\tstring]\n", VENDOR_CODE_KEYWORD, VENDOR_CODE_KEYWORD);
	printf(" %s\t: [%s\tstring]\n", LICENSE_FILE_KEYWORD, LICENSE_FILE_KEYWORD);
	printf(" %s\t: [%s\tstring]\n", KEY_CODE_KEYWORD, KEY_CODE_KEYWORD);
	printf(" %s\t: [%s\tstring]\n", VENDOR_ID_KEYWORD, VENDOR_ID_KEYWORD);
	printf(" %s\t: [%s\tstring]\n", DEVICE_ID_KEYWORD, DEVICE_ID_KEYWORD);
}
#endif

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

#if FLM_COMPILE_NEW_LICENSE == 1
	char vendorlic[4][VENDOR_TXT_LINE_CHAR_MAX] = { '\0' };
#endif

#if FLM_COMPILE_NEW_LICENSE == 1
	printf("********[Command line arguments check FLM_COMPILE_NEW_LICENSE]***************************\n");
	printf("***	argv[1] [vendor.txt path]	: [%s]\n", argv[1]);
	printf("***	argv[2] [target.dat path]	: [%s]\n", argv[2]);
	printf("***	argv[3] [IPv4 address]		: [%s]\n", argv[3]);
	printf("***	argv[4] [hostname]		: [%s]\n", argv[4]);
	printf("*****************************************************************************************\n");
#else // TRIAL_LICENSE
	printf("********[Command line arguments check TRIAL_LICENSE]*************************************\n");
	printf("***	argv[1] [farauth2.dat path]	: [%s]\n", argv[1]);
	printf("***	argv[2] [IPv4 address]		: [%s]\n", argv[2]);
	printf("***	argv[3] [hostname]		: [%s]\n", argv[3]);
	printf("*****************************************************************************************\n");
#endif

	// Command line arguments
#if FLM_COMPILE_NEW_LICENSE == 1
	if (argc != 5)
	{
		printf("Help:\n");
		printf("Please run [vendor.txt path] [target.dat path] [IPv4 address, for example(xxx.xxx.xxx.xxx)] [hostname,for example PC1]\n");

#else // TRIAL_LICENSE
	if (argc != 4)
	{
		printf("Help:\n");
		printf("Please run [farauth2.dat path] [IPv4 address, for example(xxx.xxx.xxx.xxx)] [hostname,for example PC1]\n");
#endif
		StopExecution();
		return -1;
	}
	// Hostname
#if FLM_COMPILE_NEW_LICENSE == 1
	if (strlen(argv[4]) > sizeof(hostName) - 1)
	{
#else // TRIAL_LICENSE
	if (strlen(argv[3]) > sizeof(hostName) - 1)
	{
#endif
		printf("Help:\n");
		printf("Please change shorter hostname (Less than 8)\n");
		StopExecution();
		return -1;
	}
	else
	{
		memset(hostName, 0, sizeof(hostName));
#if FLM_COMPILE_NEW_LICENSE == 1
		strncpy((char *)hostName, argv[4], (sizeof(hostName) - 1));
	}
#else // TRIAL_LICENSE
		strncpy((char *)hostName, argv[3], (sizeof(hostName) - 1));
	}
#endif
	printf("HostName will be set to %s\n", hostName);
	sleep_for(std::chrono::milliseconds(500));
	printf("\nStart sample code!\n");

#ifdef __linux__ /*__Linux__*/
	// Entry functions
	ret = Furuno::fec_entry_function("libNAVNETDLL.so");
	switch (ret)
	{
	case ENTRY_OK:
		printf("Entry function  : OK\n");
		break;
	case ENTRY_SO_NAME_ERR:
		printf("So file name is error\n");
		break;
	case ENTRY_DLL_ERR:
		printf("Open SO file error\n");
		break;
	case ENTRY_FUNC_ERR:
		printf("Entry function have problem\n");
		break;
	default:
		break;
	}
	if (ret != ENTRY_OK)
	{
		printf("Entry function failed and return now\n");
		return -1;
	}
#endif /*__Linux__*/

	// Initial
	sleep_for(std::chrono::milliseconds(500));
	Furuno::InitialITRON();
	printf("InitialITRON \t: OK\n");

#ifdef __linux__ /*__Linux__*/
	/* FLM_certification */
	Furuno::FecError error = Furuno::InitError;
#endif /*__Linux__*/

	printf("License      \t: ");
	sleep_for(std::chrono::milliseconds(500));
#if FLM_COMPILE_NEW_LICENSE == 1
	/* Authentication with the official license file */
	if (ReadVendorTxt(argv[1], vendorlic) != 0)
	{
		printf("Help:\n");
		printf("vendor.txt is broken, NOT OK\n");
		PrintVendorGuidance();
		StopExecution();
		return -1;
	}
	/* initialize LastError of NavNetLibrary API */
	Furuno::LeClearLastAPIError();

#ifdef __linux__ /*__Linux__*/
	setenv("FEC_NAVNETDLL_TARGET", argv[2], 1);
	error = Furuno::Initialize((Furuno::UINT8 *)vendorlic[0], (Furuno::UINT8 *)vendorlic[1], (Furuno::UINT8 *)vendorlic[2], (Furuno::UINT8 *)vendorlic[3], ip_addr); /* vendor, password, vendorid, deviceinfo -> FLM_info check */

	/* get LastError of NavNetLibrary API */
	lastError = Furuno::LeGetLastError(Furuno::ERRORTYPE_API);

	if (error == Furuno::None)
	{
		printf("Flm_certification : OK\n");
	}
	else
	{
		printf("Flm_certification : failed\n");
		printf("Error = %x\n", lastError);
		StopExecution();
		return -1;
	}
#endif /*__Linux__*/

#ifdef WIN32 /*  WIN32  */
	/* Authentication with the official license file */
	/* "vendor code","keycode","vendor id","device id" */
	if (License(vendorlic[0], vendorlic[1], vendorlic[2], vendorlic[3]))
	{
		printf("OK\n");
	}
	else
	{
		printf("NOT OK\n");
		/* get LastError of NavNetLibrary API */
		lastError = Furuno::LeGetLastError(Furuno::ERRORTYPE_API);
		printf("Error = %x\n", lastError);
		StopExecution();
		return -1;
	}

	printf("Target License  : ");
	/* initialize LastError of NavNetLibrary API */
	Furuno::LeClearLastAPIError();
	sleep_for(std::chrono::milliseconds(500));
	if (TargetLicense(argv[2]))
	{
		printf("OK\n");
	}
	else
	{
		printf("NOT OK\n");
		/* get LastError of NavNetLibrary API */
		lastError = Furuno::LeGetLastError(Furuno::ERRORTYPE_API);
		printf("Error = %x\n", lastError);
		printf("target.dat is broken, NOT OK\n");
		StopExecution();
		return -1;
	}
#endif		 /*  WIN32  */
#else		 // TRIAL_LICENSE
#ifdef WIN32 /*  WIN32  */
	/* Authentication with the trial license file */
	Furuno::INT32 status = 0;
	Furuno::FurunoTimePermissionData data;
	Furuno::INT32 flm_err = Furuno::FLM_ERR_FAILURE;

	memset(&data, 0, sizeof(data));
	FILE *fp = fopen(argv[1], "rb");
	if (fp == NULL)
	{
		printf("Can not open farauth2.dat\n");
		StopExecution();
		return -1;
	}
	data.time_code_size = fread(data.time_code, sizeof(unsigned char), sizeof(data.time_code) / sizeof(data.time_code[0]), fp);
	fclose(fp);

	if (data.time_code_size != 40)
	{
		printf("The data of farauth2.dat is not correct or can not read farauth2.dat correctly!\n");
		StopExecution();
		return -1;
	}
#endif		 /*  WIN32  */
	/* initialize LastError of NavNetLibrary API */
	Furuno::LeClearLastAPIError();

#ifdef __linux__ /*__Linux__*/
	setenv("FEC_NAVNETDLL_TARGET", argv[1], 1);
	error = Furuno::Initialize_Trial();
#endif			 /*__Linux__*/

#ifdef WIN32 /*  WIN32  */
	flm_err = Furuno::FlmInitial(&status, (Furuno::UINT8 *)&data, sizeof(data), Furuno::FLM_TYPE_TIME_1);
#endif		 /*  WIN32  */

	/* get LastError of NavNetLibrary API */
	lastError = Furuno::LeGetLastError(Furuno::ERRORTYPE_API);

#ifdef __linux__ /*__Linux__*/
	if (error == Furuno::None)
	{
		printf("Flm_certification : OK\n");
	}
	else
	{
		printf("Flm_certification : failed\n");
		printf("Error = %x\n", lastError);
		StopExecution();
		return -1;
	}
	/* If you want to switch the license to the official version,
	Please enable the following processing instead of Initialize_trial(). */
#endif			 /*__Linux__*/
#ifdef WIN32	 /*  WIN32  */
	if (flm_err == Furuno::FLM_ERR_SUCCESS)
	{
		if (status != Furuno::FLM_STATE_SUCCESS)
		{
			printf("NOT OK\n");
			printf("Error = %x\n", lastError);
			StopExecution();
			return -1;
		}
		else
		{
			printf("OK\n");
		}
	}
	else
	{
		printf("NOT OK\n");
		printf("Error = %x\n", lastError);
		StopExecution();
		return -1;
	}
#endif			 /*  WIN32  */
#endif

	// Get IP address
	printf("IP address   \t: ");
	sleep_for(std::chrono::milliseconds(500));
#if FLM_COMPILE_NEW_LICENSE == 1
	strncpy(ip_buf, argv[3], sizeof(ip_buf));
#else // TRIAL_LICENSE
	strncpy(ip_buf, argv[2], sizeof(ip_buf));
#endif

	for (int i = 0; i < sizeof(ip_buf); i++)
	{
		if (ip_buf[i] == '.')
		{
			d_count++;
		}
	}
	if (d_count != 3)
	{
		printf("Please change IPv4 address(for dots)\n");
		printf("            for example : [xxx.xxx.xxx.xxx]\n");
		printf(" Your input IPv4 address: [%s]\n", argv[3]);
		StopExecution();
		return -1;
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
	if (ret1 != NC_OK)
	{
		printf("Help:\n");
		printf("NcSetNetworkIpAddress() NG!      Please change IPv4 address \n");
		StopExecution();
		return -1;
	}

	printf("\n");
	Furuno::NcGetModuleVersionNo(versionNoForFnetModule); // both enter No.1[NcGetModuleVersionNo]
	printf("Fnet Module Version No.    : ver.%s\n", versionNoForFnetModule);
	sleep_for(std::chrono::milliseconds(500));

	ret = Furuno::NcInitialNavnetCommon(hostName, fcode);
	if (ret == 0)
	{
		printf("NcInitialNavnetCommon      : OK\n");
	}
	else
	{
		printf("NcInitialNavnetCommon      : NG\n");
		StopExecution();
		return -1;
	}

	Furuno::NcInitialNmea183Server();		//[extras] It initializes the NMEA0183 LAN functionality.
	Furuno::NcInitialNmea0183PortStatus(1); //[extras] Nothing API specification

#if FLM_COMPILE_NEW_LICENSE == 1
	printf("\t------------------------------------------------------------------\t\n");
	P_SensorNo();
	printf("\t------------------------------------------------------------------\t\n");
#endif

	printf("Init Radar Module...\n");
	sleep_for(std::chrono::milliseconds(500));
	StartRadarModule();

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
	else
	{
		printf("Which sensor do you connect?\n");
		for (i = 0; i < radarHostCount; i++)
		{
			printf("%d : \"%s\"\n", i, radarHostNameList[i]);
		}
		while (1)
		{
			printf("Input sensor No. = ");
			ret = scanf("%ld", (long int *)&sensorNo);
			if ((0 <= sensorNo) && (sensorNo < radarHostCount))
			{
				strncpy((char *)radarHostName, (const char *)radarHostNameList[sensorNo], sizeof(radarHostName) - 1);
				break;
			}
		}
	}

	printf("Do you use Dual Range Mode? (0=No/1=Yes):");
	ret = scanf("%d", &dualRangeMode);
	printf("Connecting to \"%s\"\n", radarHostName);

	if (dualRangeMode)
	{
		Furuno::RmSetRadarSource(0, radarHostName, 0);
		Furuno::RmSetRadarSource(1, radarHostName, 1);
	}
	else
	{
		Furuno::RmSetRadarSource(0, radarHostName, 0);
	}
	sleep_for(std::chrono::milliseconds(500));

	// Connect to Radar Sensor
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

				if (ret == 0)
				{
					printf("RmSetCommandCallbackFunc : OK\n");
				}
				else
				{
					printf("RmSetCommandCallbackFunc : failed\n");
					return -1;
				}
			}

			if (strcmp("x", (const char *)command) == 0)
			{
				dispOnlyResult = 0;
				ret = Furuno::RmSetCommandCallbackFunc(NULL);
				if (ret == 0)
				{
					printf("RmSetCommandCallbackFunc : OK\n");
				}
				else
				{
					printf("RmSetCommandCallbackFunc : failed\n");
					return -1;
				}
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

			if (strcmp("00", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: freecommandID(32 characters) = ");
				ret = scanf("%s", freeID);
				printf("Input value: freecommandTEXT(128 characters) = ");
				ret = scanf("%s", freetext);
				ret = Furuno::RmcSetFreeCommand(radarNo, freeID, freetext);
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

			if (strcmp("1", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=HeadingUp-Relative/1=HeadingUp-TB/2=CourseUp-Relative/3=NorthUp-Relative/4=NorthUp-TrueMotion/5=CourseUp-TrueMotion/6=SternUp(for FAR-3000)) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: direction when status is CourseUp-Relative(0 - 3599) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetDispMode(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
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

			if (strcmp("6", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Picture Number(0 - 12) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				// This command can't set dataTable.
				// If you want to set dataTable, please use RmcSetCustomPicture().
				ret = Furuno::RmcSetCustomPictureAll(radarNo, lvalue1, dateTable);
				ReturnValueCheck(ret);
			}

			if (strcmp("7", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Picture Number(0 - 12) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: Function Number(0 - 26) = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				printf("Input value: Function data(different depending on funcNo) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetCustomPicture(radarNo, lvalue1, lvalue2, value1);
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

			if (strcmp("10", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: distance from center position X(1=0.00001nm) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: distance from center position Y(1=0.00001nm) = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				printf("Input value: Target Number(1 - 255) = ");
				ret = scanf("%ld", (long int *)&lvalue3);
				ret = Furuno::RmcSetArpaSelectTarget(radarNo, lvalue1, lvalue2, lvalue3);
				ReturnValueCheck(ret);
			}

			if (strcmp("11", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: distance from center position X(1=0.00001nm) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: distance from center position Y(1=0.00001nm) = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				printf("Input value: mode(0=NORMAL/1=REFERENCE) = ");
				ret = scanf("%ld", (long int *)&lvalue3);
				ret = Furuno::RmcSetArpaAcquireTarget(radarNo, lvalue1, lvalue2, lvalue3);
				ReturnValueCheck(ret);
			}

			if (strcmp("12", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: distance from center position X(1=0.00001nm) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: distance from center position Y(1=0.00001nm) = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				ret = Furuno::RmcSetArpaCancelTarget(radarNo, lvalue1, lvalue2);
				ReturnValueCheck(ret);
			}

			if (strcmp("13", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: mode(0=MODE0/1=MODE1/3=MODE2/3=MODE3/4=MODE4) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: vector display of reference speed(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: maximum number of auto acquisition(0 - ARPA_TARGET_MAX) (DRS: 0 to 30) (FAR-14x7: 0 to 100) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: maximum number of manual acquisition(0 - ARPA_TARGET_MAX) (DRS: 0 to 30) (FAR-14x7: 0 to 100) = ");
				ret = scanf("%d", (int *)&value5);
				printf("Input value: acquisition mode(0=Relative/1=True) for FAR-14x7 = ");
				ret = scanf("%d", (int *)&value6);
				ret = Furuno::RmcSetArpaDispMode(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, (Furuno::UINT16)value4, (Furuno::UINT16)value5, (Furuno::UINT16)value6);
				ReturnValueCheck(ret);
			}

			if (strcmp("14", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: type(0=6/1=12/2=25UP/3=25DOWN/4=50/5=30UP/6=30DOWN/7=60/8=S-SSD/9=X-SSD) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: antenna position(0=FORE/1=MAIN TOP/2=MAIN 2nd/3=MAIN 3rd/4=AFT/5=PORT/6=STAR) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: transmission power(kw) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: antenna length(cm) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: updown(0=UP/1=DOWN) = ");
				ret = scanf("%d", (int *)&value5);
				printf("Input value: antenna type(0=radome/1=open/3=EG/3=other) = ");
				ret = scanf("%d", (int *)&value6);
				printf("Input value: device type(0=magnetron/1=solid state) = ");
				ret = scanf("%d", (int *)&value7);
				ret = Furuno::RmcSetAntennaType(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, (Furuno::UINT16)value4, (Furuno::UINT16)value5, (Furuno::UINT16)value6, (Furuno::UINT16)value7);
				ReturnValueCheck(ret);
			}

			if (strcmp("15", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Key Command(100 characters) = ");
				ret = scanf("%s", keyCom);
				ret = Furuno::RmcSetKeyCommand(radarNo, keyCom);
				ReturnValueCheck(ret);
			}

			if (strcmp("16", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: number of polygon point(0 - 10) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: status of GZ1(0=none/1=select/2=set/3=work/4=off/5=out) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: status of GZ2(0=none/1=select/2=set/3=work/4=off/5=out) = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetGuardStatus(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3);
				ReturnValueCheck(ret);
			}

			if (strcmp("18", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				ret = Furuno::RmcSetLostAck(radarNo);
				ReturnValueCheck(ret);
			}

			if (strcmp("19", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: vector reference(0=relative/1=true) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: vector time(0=30sec/1=1min/2=3min/3=6min/4=12min/5=15min/6=30min) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetArpaVector(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("20", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: dangerous alarm SW(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: CPA setting(0=OFF/1=0.5nm/2=1nm/3=2nm/4=3nm/5=5nm/6=6nm) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: TCPA setting(0=30sec/1=1min/2=2min/3=3min/4=4min/5=5min/6=6min/7=12min) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: detail CPA(1/10000NM) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: detail TCPA(1-3600sec) = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				ret = Furuno::RmcSetArpaCpaTcpa(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, lvalue1, lvalue2);
				ReturnValueCheck(ret);
			}

			if (strcmp("21", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mode(0=MANUAL/1=AUTO) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: tune value(0 - 4095) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetTune(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("22", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: indicator(0 - 100) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: tuning status(0=OK/1=NG) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetTuneIndicator(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("23", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: starting direction0(0 - 359) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: starting direction1(0 - 359) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: sector0 to be set(0 - 180) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: sector1 to be set(0 - 180) = ");
				ret = scanf("%d", (int *)&value5);
				ret = Furuno::RmcSetBlindSector(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, (Furuno::UINT16)value4, (Furuno::UINT16)value5);
				ReturnValueCheck(ret);
			}

			if (strcmp("24", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: monitor number(0=MAIN/1=SUB) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: the distance from the bow(m) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: the distance from the port(m) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetRadarPosition(radarNo, lvalue1, value1, value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("25", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				ret = Furuno::RmcSetArpaTargetAllClear(radarNo);
				ReturnValueCheck(ret);
			}

			if (strcmp("26", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetTuneAdjust(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("27", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Target Number(0 - 255) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				int aaa = Furuno::RmcSetArpaCancelTargetId(radarNo, lvalue1);
				ret = Furuno::RmcSetArpaCancelTargetId(radarNo, lvalue1);
				ReturnValueCheck(ret);
				printf("RmcSet ERROR=%d", aaa);
			}

			if (strcmp("28", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=antenna/1=conning) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetReferencePoint(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("29", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: alarm status bit(0 - 0xFFFF) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: mtr status bit(0 - 0xFFFFFFFF) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: pm status bit(0 - 0xFFFFFFFF) = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				printf("Input value: rfc status bit(0 - 0xFFFFFFFF) = ");
				ret = scanf("%ld", (long int *)&lvalue3);
				ret = Furuno::RmcSetAlarmStatus(radarNo, (Furuno::UINT16)value1, (Furuno::UINT32)lvalue1, (Furuno::UINT32)lvalue2, (Furuno::UINT32)lvalue3);
				ReturnValueCheck(ret);
			}

			if (strcmp("30", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mode(0=AD-10/1=Serial/2=CAN) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: heading(0-3599 1/10deg) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetHeading(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("31", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: QV display(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetSelectQVDisplay(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("32", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: monitor number(0=MAIN/1=SUB) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: status(0=AUTO/1=MANUAL) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: adjustment value(FAR-21x7/FAR-15x8/FAR-3000:0-64, FAR-14x7:0-99, Others:Not use) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetATT(radarNo, lvalue1, (Furuno::UINT16)value1, value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("33", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: monitor number(0=MAIN/1=SUB) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: adjustment value(0 - 3599) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: fine adjustment value(-100 - 100) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetHeadingAdjust(radarNo, lvalue1, (Furuno::UINT16)value1, (Furuno::INT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("34", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: monitor number(0=MAIN/1=SUB) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: mode(0=MANUAL/1=AUTO) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: adjustment value(0 - 4095) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: adjustment value when mode is auto = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: offset value(0 - 100) = ");
				ret = scanf("%d", (int *)&value4);
				ret = Furuno::RmcSetTimingAdjust(radarNo, lvalue1, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, (Furuno::UINT16)value4);
				ReturnValueCheck(ret);
			}

			if (strcmp("35", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mbs adjustment value(0 - 255) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: pulse width(0=S1/1=S2/2=M1/3=M2/4=M3/5=L) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetMBSAdjust(radarNo, (Furuno::UINT16)value1, value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("36", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: monitor number(0=MAIN/1=SUB) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: antenna height(0=5m or less than/1=7.5m/2=10m/3=15m/4=20/5=25m/6=30m/7=35m/8=40m/9=45m/10=50m or more than) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: antenna height(value of antenna height(m)) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetAntennaHeight(radarNo, lvalue1, (Furuno::INT16)value1, (Furuno::UINT16)value2);
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

			if (strcmp("42", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: antenna power sw(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetAntennaSwitch(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("43", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: transmit status(0=STBY/1=TX) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetStoppedTx(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("44", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: monitor number(0=MAIN/1=SUB) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetMonitorSelect(radarNo, value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("45", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: antenna number(0 - 3) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetAntennaNo(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("46", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: operation time(sec) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				ret = Furuno::RmcSetOnTime(radarNo, (Furuno::UINT32)lvalue1);
				ReturnValueCheck(ret);
			}

			if (strcmp("47", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: transmission time(sec) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				ret = Furuno::RmcSetTxTime(radarNo, (Furuno::UINT32)lvalue1);
				ReturnValueCheck(ret);
			}

			if (strcmp("48", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: gain(0 - 255) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: position(x10000) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: adjust mode(0=AUTO/1=MANUAL) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetPmAdjust(radarNo, (Furuno::UINT16)value1, lvalue1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("49", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: demo echo(0=OFF/1=EG/2=TT TEST/3=PC) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetDemoEcho(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("50", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: select(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetEavWithoutGyro(radarNo, (Furuno::UINT16)value1);
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

			if (strcmp("52", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: PM status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: the number of arc(0 - 6) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetPM(radarNo, (Furuno::UINT16)value1, value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("53", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: SART status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetSART(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("54", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: program number of radar application(20 characters) = ");
				ret = scanf("%s", bvalue1);
				printf("Input value: program number of radar starter(20 characters) = ");
				ret = scanf("%s", bvalue2);
				printf("Input value: program number of radar booter(20 characters) = ");
				ret = scanf("%s", bvalue3);
				printf("Input value: program number of FPGA(20 characters) = ");
				ret = scanf("%s", bvalue4);
				printf("Input value: program number of NMEA application(20 characters) = ");
				ret = scanf("%s", bvalue5);
				printf("Input value: program number of NMEA starter(20 characters) = ");
				ret = scanf("%s", bvalue6);
				printf("Input value: program number of NMEA booter(20 characters) = ");
				ret = scanf("%s", bvalue7);
				ret = Furuno::RmcSetVersionNo(radarNo, bvalue1, bvalue2, bvalue3, bvalue4, bvalue5, bvalue6, bvalue7);
				ReturnValueCheck(ret);
			}

			if (strcmp("55", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaTest(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("56", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: zoneNo(0=GuardZone1/1=GuardZone2) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: Fan Stab(0=Heading/1=North) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: Polygon Stab(0=OFF/1=Ground/2=Heading/3=North) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: Around Check Area(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetGuardMode(radarNo, lvalue1, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3);
				ReturnValueCheck(ret);
			}

			if (strcmp("57", (const char *)command) == 0)
			{
				printf("Input value: zone No.(0=ZONE0/1=ZONE1) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: start dist(1=0.000001nm) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: start dir(0 - 3599) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: end dist(1=0.00001nm) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: end dir(0 - 3599) = ");
				ret = scanf("%d", (int *)&value5);
				ret = Furuno::RmcSetGuardFan(0, (Furuno::INT32)value1, (Furuno::INT32)value2, (Furuno::INT32)value3, (Furuno::INT32)value4, (Furuno::INT32)value5);
				ReturnValueCheck(ret);
			}

			if (strcmp("58", (const char *)command) == 0)
			{
				printf("Input value: zone No.(0=ZONE0/1=ZONE1) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: start dist(1=0.000001nm) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: start dir(0 - 3599) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: end dist(1=0.00001nm) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: end dir(0 - 3599) = ");
				ret = scanf("%d", (int *)&value5);
				ret = Furuno::RmcSetGuardFan(1, (Furuno::INT32)value1, (Furuno::INT32)value2, (Furuno::INT32)value3, (Furuno::INT32)value4, (Furuno::INT32)value5);
				ReturnValueCheck(ret);
			}

			if (strcmp("59", (const char *)command) == 0)
			{
				for (i = 0; i < 10; i++)
				{
					dist[i] = 0;
					dir[i] = 0;
					lat[i] = 0;
					lon[i] = 0;
				}
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: zoneNo(0=GuardZone1/1=GuardZone2) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: the total number of polygons(3 - 10) = ");
				ret = scanf("%d", (int *)&value1);
				for (i = 0; i < value1; i++)
				{
					printf("Input value: starting distant[%d](1/10000 nm) = ", i);
					ret = scanf("%d", (int *)&dist[i]);
					printf("Input value: starting direction[%d](1/10 deg) = ", i);
					ret = scanf("%d", (int *)&dir[i]);
					printf("Input value: latitude[%d](1/10000 min) = ", i);
					ret = scanf("%d", (int *)&lat[i]);
					printf("Input value: longitude[%d](1/10000 min) = ", i);
					ret = scanf("%d", (int *)&lon[i]);
				}
				ret = Furuno::RmcSetGuardPolygon(radarNo, lvalue1, (Furuno::UINT16)value1, dist, dir, lat, lon);
				ReturnValueCheck(ret);
			}

			if (strcmp("60", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: select(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaWithoutGyro(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("61", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mode(0=LOG,BT/1=LOG,WT/2=GPS/3=MANUAL/4=REF) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: speed(0 - 999) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetSpeedSelect(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("62", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mode(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: set(0 - 3599) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: drift(0 - 199) = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetDrift(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3);
				ReturnValueCheck(ret);
			}

			if (strcmp("63", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: offset(0 - 3599) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetGC10(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("64", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Target Number(0 - 255) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				ret = Furuno::RmcSetArpaCancelStatus(radarNo, lvalue1);
				ReturnValueCheck(ret);
			}

			if (strcmp("65", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: data number(0 - 99) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: adjustment value(0 - 9999) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaPreset(radarNo, lvalue1, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("67", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mode(0=relative/1=true) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: grad(0=single/1=multi) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: narrow(0=off/1=on) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: level(0=1/1=2/2=3/3=4) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: range(0=off/1=on) = ");
				ret = scanf("%d", (int *)&value5);
				printf("Input value: copy(0=off/1=on) = ");
				ret = scanf("%d", (int *)&value6);
				printf("Input value: os(0=off/1=on) = ");
				ret = scanf("%d", (int *)&value7);
				printf("Input value: trail time(0 - 9999) = ");
				ret = scanf("%d", (int *)&value8);
				ret = Furuno::RmcSetTrailMode(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, (Furuno::UINT16)value4, (Furuno::UINT16)value5, (Furuno::UINT16)value6, (Furuno::UINT16)value7, (Furuno::UINT16)value8);
				ReturnValueCheck(ret);
			}

			if (strcmp("68", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: range(0 - 21) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetTrailClear(radarNo);
				ReturnValueCheck(ret);
			}

			if (strcmp("69", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Target Number(0 - 255) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				ret = Furuno::RmcSetArpaCancelTargetNo(radarNo, lvalue1);
				ReturnValueCheck(ret);
			}

			if (strcmp("70", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: offset x(pixel) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: offset y(pixel) = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				ret = Furuno::RmcSetSweepPosition(radarNo, lvalue1, lvalue2);
				ReturnValueCheck(ret);
			}

			if (strcmp("71", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: the distance from center X(1/10000nm) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: the distance from center Y(1/10000nm) = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				printf("Input value: Mark Type(bit0-13:Mark shape, bit14:Guard zone entry flag at start of acquisition, bit15:Guard zone alert approval flag) = ");
				ret = scanf("%ld", (long int *)&lvalue3);
				printf("Input value: Target ID(1 - 200) = ");
				ret = scanf("%ld", (long int *)&lvalue4);
				ret = Furuno::RmcSetArpaAcquireTargetDisplayId(radarNo, lvalue1, lvalue2, lvalue3, lvalue4);
				ReturnValueCheck(ret);
			}

			if (strcmp("72", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Target Number(0 - 255) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				ret = Furuno::RmcSetArpaCancelTargetDisplayId(radarNo, lvalue1);
				ReturnValueCheck(ret);
			}

			if (strcmp("73", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: antenna number(0/1) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: distance from bow(m) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: distance from port(m) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetGpsAntennaPosition(radarNo, lvalue1, value1, value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("74", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: distance from bow(m) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: distance from port(m) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetConningPosition(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("75", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: antenna number(0/1) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				ret = Furuno::RmcSetSelectAntenna(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("76", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: countdown number(sec) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetWakeupCount(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("77", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: ship length(m) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: ship width(m) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetShipInfo(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("78", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0 - 0x0F) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetAisAlarm(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("79", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0 - 0x0F) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaAlarm(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("80", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: channel(0 - 3) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: code = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: sw(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetSelectAlarmOut(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3);
				ReturnValueCheck(ret);
			}

			if (strcmp("81", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: alarm number defined by INS = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: alarm instance = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetAlarmAck(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT8)'A');
				ReturnValueCheck(ret);
			}

			if (strcmp("82", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: TODO = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetSelectCrt(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("83", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: channel(0 - 3) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: sw(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetSelectAlarmPolarity(radarNo, lvalue1, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("84", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF 1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: antenna number(0 - 3) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetCombineStatus(radarNo, (Furuno::UINT16)value1, value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("85", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: source(0=AD10 1=Serial) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetHeadingSource(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("86", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetNetworkPriority(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("87", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: menu number = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: menu data = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetMenuSet(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("88", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetImoEchoSwitch(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("89", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetExtAlarmAck(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("90", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=during the idol/1=during the adjustment) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetPmAdjustEnd(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("91", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: TODO = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetSelectTtmOut(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("92", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: maxRange(0=24nm/1=32nm) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaMaxRange(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("93", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: echo level(1 - 31) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaEchoLevel(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("94", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				ret = Furuno::RmcSetArpaAllClear(radarNo);
				ReturnValueCheck(ret);
			}

			if (strcmp("95", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: size(100 - 3000) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaLandSize(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("96", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: the kind of the antennas(0 - 14) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaAntenna(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("97", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: correction value(3 - 10) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaAcqCorre(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("98", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: correction value(1 - 5) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaWeed(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("99", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: gate size(0=S/1=M/2=L/3=LL) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaGateSize(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("100", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: filter response(0=1/1=2/2=3/3=4) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaFilterRespons(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("101", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: lost count(1 - 30) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaLostCount(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("102", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: max speed(40 - 150 kn) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaMaxSpeed(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("103", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: adjustment value(0 - 100 sec) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: adjustment value(0 - 40 scan) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetArpaTimeVector(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("104", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				ret = Furuno::RmcSetOperatorFitness(radarNo);
				ReturnValueCheck(ret);
			}

			if (strcmp("105", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				ret = Furuno::RmcSetPictureDefault(radarNo);
				ReturnValueCheck(ret);
			}

			if (strcmp("106", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: program number of radar application(20 characters) = ");
				ret = scanf("%s", bvalue1);
				ret = Furuno::RmcSetRfcVersionNo(radarNo, bvalue1);
				ReturnValueCheck(ret);
			}

			if (strcmp("107", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mode(TODO) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: start_angle(TODO) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: angle(TODO) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: start_length(TODO) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: length(TODO) = ");
				ret = scanf("%d", (int *)&value5);
				ret = Furuno::RmcSetCombineData(radarNo, value1, value2, value3, value4, value5);
				ReturnValueCheck(ret);
			}

			if (strcmp("111", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				for (i = 0; i < 8; i++)
				{
					printf("Input value: data array[%d] = ", i);
					ret = scanf("%d", (int *)&interswTable[i]);
				}
				ret = Furuno::RmcSetInterswInfo(radarNo, interswTable);
				ReturnValueCheck(ret);
			}

			if (strcmp("112", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: max distance(nm) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: max distance(unit is depends on current setting) = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				ret = Furuno::RmcSetArpaLostFilterRange(radarNo, (Furuno::UINT16)value1, lvalue1, lvalue2);
				ReturnValueCheck(ret);
			}

			if (strcmp("113", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: min speed(1/10kn) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				ret = Furuno::RmcSetArpaLostFilterSpeed(radarNo, (Furuno::UINT16)value1, lvalue1);
				ReturnValueCheck(ret);
			}

			if (strcmp("114", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: STC range = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetStcRange(radarNo, value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("115", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: picture settings(0=None/1=Save/2=Load/3=Default) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetCustomMemory(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("116", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: buildup time(sec) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				ret = Furuno::RmcSetBuildUpTime(radarNo, lvalue1);
				ReturnValueCheck(ret);
			}

			if (strcmp("117", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: antenna number(0 - 3) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: master(0=Slave/1=Master) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: hostname(8 characters) = ");
				ret = scanf("%s", dispname);
				ret = Furuno::RmcSetDisplayUnitInformation(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, dispname);
				ReturnValueCheck(ret);
			}

			if (strcmp("119", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mode(0=OFF/1=FILTER/2=ALL) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetArpaLostFilterMode(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("121", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=OFF/1=TTM/2=TTD) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetAssociationStatus(radarNo, value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("120", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				ret = Furuno::RmcSetEchoClear(radarNo);
				ReturnValueCheck(ret);
			}

			if (strcmp("122", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: start target ID(-1, 1 - 200, REF_ID(251,252,253)) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: end target ID(-1, 1 - 200, REF_ID(251,252,253)) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetCancelSelectTarget(radarNo, value1, value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("123", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: start target ID(-1, 1 - 200, REF_ID(251,252,253)) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: end target ID(-1, 1 - 200, REF_ID(251,252,253)) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetCancelSelectLostTarget(radarNo, value1, value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("128", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Picture Number(0 - 12) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: ATF status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: Gain value(0 - 100) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: Sea value(-5 - +5) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: Rain value(-5 - +5) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: High sensitive mode(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value5);
				printf("Input value: ATF mode(1=CALM/2=SEA/3=RAIN) = ");
				ret = scanf("%d", (int *)&value6);
				ret = Furuno::RmcSetCustomAtfParameter(radarNo, lvalue1, (Furuno::UINT16)value1, (Furuno::UINT16)value2, value3, value4, (Furuno::UINT16)value5, (Furuno::UINT16)value6);
				ReturnValueCheck(ret);
			}

			if (strcmp("129", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Selection mode(0=MODEI(Dead reckoning)/1=MODEC) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetTrailProcess(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("130", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Target Number(0 - 255) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: Target id(1 - 200, ref id(251 - 253)) = ");
				ret = scanf("%ld", (long int *)&lvalue2);
				printf("Input value: X position(Unit: 1/10000nm, Based on True North) = ");
				ret = scanf("%ld", (long int *)&lvalue3);
				printf("Input value: Y position(Unit: 1/10000nm, Based on True North) = ");
				ret = scanf("%ld", (long int *)&lvalue4);
				printf("Input value: Target speed(Unit: 1/10kn) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: Target course(Unit: 1/10deg, Based on True North) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: Status for the handover of target(16bit) = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetArpaReAcquireTarget(radarNo, lvalue1, lvalue2, lvalue3, lvalue4, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3);
				ReturnValueCheck(ret);
			}

			if (strcmp("131", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				ret = Furuno::RmcSetAliveCheck(radarNo);
				ReturnValueCheck(ret);
			}

			if (strcmp("132", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Output setting status(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetTxEchoSelect(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("133", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Echo input selector(0=own master-R/1=own slave-R/2=other master-R/3=other slave-R) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: ip_addr[0] = ");
				ret = scanf("%d", (int *)&ip_addr[0]);
				printf("Input value: ip_addr[1] = ");
				ret = scanf("%d", (int *)&ip_addr[1]);
				printf("Input value: ip_addr[2] = ");
				ret = scanf("%d", (int *)&ip_addr[2]);
				printf("Input value: ip_addr[3] = ");
				ret = scanf("%d", (int *)&ip_addr[3]);
				ret = Furuno::RmcSetRxEchoSelect(radarNo, (Furuno::UINT16)value1, ip_addr);
				ReturnValueCheck(ret);
			}

			if (strcmp("134", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Data Number(0 - 99) = ");
				ret = scanf("%ld", (long int *)&lvalue1);
				printf("Input value: Sets value(0 - 9999) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetRsPreset(radarNo, lvalue1, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("135", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				ret = Furuno::RmcSetStartSelfMode(radarNo);
				ReturnValueCheck(ret);
			}

			if (strcmp("136", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Jamming mode(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetJummingAble(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("137", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: IceClass setting mode(0=OFF/1=ON)(Used in antenna rotation start/stop control) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: IceClass setting mode(0=OFF/1=ON)(Used in ICING PREVENTION mode identification) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetIceClass(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("138", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: ATF adjust mode(0=OFF/1=ON) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: Signal enhancement level(1 - 3) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: Cut level(0 - 8) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: Starting direction(0 - 359) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: Sector range(0 - 180) = ");
				ret = scanf("%d", (int *)&value5);
				ret = Furuno::RmcSetAtfSettings(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, (Furuno::UINT16)value4, (Furuno::UINT16)value5);
				ReturnValueCheck(ret);
			}

			if (strcmp("139", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				ret = Furuno::RmcSetRsAllClear(radarNo);
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

			if (strcmp("141", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mode OFF/1/2/3(=0/1/2/3) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetHighSensitivityMode(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("142", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mode OFF/Low/Middle/High(=0/1/2/3) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetBearingResolutionSelect(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("143", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: mode  normal display/Echo discriminant display/Speed map display(=0/1/2) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: Rain display setting in Echo discriminant display OFF/ON(=0/1) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: Auto Video Auto Setting OFF/ON(=0/1) = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetEchoColorType(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3);
				ReturnValueCheck(ret);
			}

			if (strcmp("144", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: select OFF/ON(=0/1) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetAccuShip(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("145", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Observation mode setting(0=Normal/1=ICE/2=Wave) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetObservationMode(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("146", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: SART signal received(0=not received/1=received) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetSARTSignal(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("148", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: TODO = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetImageCor(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("149", (const char *)command) == 0)
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

			if (strcmp("150", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: setting status(0=factory/1=serviceman/2=user) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: memory number(0 - 100) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetMemory(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("151", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=none/1=setting/2=finished) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetEquipComp(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("152", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: min(0=S1/1=S2/2=M1/3=M2/4=M3/5=L) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: max(0=S1/1=S2/2=M1/3=M2/4=M3/5=L) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetPulseRange(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
				ReturnValueCheck(ret);
			}

			if (strcmp("153", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: magne(0 - 100) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: status(0=start/1=adjusting/2=finished) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: result(0=OK/1=NG) = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetMagnetron(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3);
				ReturnValueCheck(ret);
			}

			if (strcmp("154", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: indicator(0 - 100) = ");
				ret = scanf("%d", (int *)&value1);
				ret = Furuno::RmcSetMagnetronInd(radarNo, (Furuno::UINT16)value1);
				ReturnValueCheck(ret);
			}

			if (strcmp("155", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: processing range(1 - 64)NM = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: echo detect level(1 - 31) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: echo detect size(1 - 10) = ");
				ret = scanf("%d", (int *)&value3);
				ret = Furuno::RmcSetArpaDetect(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3);
				ReturnValueCheck(ret);
			}

			if (strcmp("156", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: detect land size(100 - 2000) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: Auto Acq. Correlation count(1 - 10) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: Auto Acq. Erasure count(1 - 10) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: gate size(1 - 10) = ");
				ret = scanf("%d", (int *)&value4);
				ret = Furuno::RmcSetArpaCatch(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, (Furuno::UINT16)value4);
				ReturnValueCheck(ret);
			}

			if (strcmp("157", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: Gate Size(1 - 10) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: Filter Responsiveness(1 - 10) = ");
				ret = scanf("%d", (int *)&value2);
				printf("Input value: Lost Count(1 - 20) = ");
				ret = scanf("%d", (int *)&value3);
				printf("Input value: Max Tracking Speed(80 - 200KTS) = ");
				ret = scanf("%d", (int *)&value4);
				printf("Input value: Tracking mode(1 - 4) = ");
				ret = scanf("%d", (int *)&value5);
				printf("Input value: Vector Start Time/Scan sec/scan(=0/1) = ");
				ret = scanf("%d", (int *)&value6);
				printf("Input value: Time Value/Scan Value(8 - 100) = ");
				ret = scanf("%d", (int *)&value7);
				ret = Furuno::RmcSetArpaPursuit(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2, (Furuno::UINT16)value3, (Furuno::UINT16)value4, (Furuno::UINT16)value5, (Furuno::UINT16)value6, (Furuno::UINT16)value7);
				ReturnValueCheck(ret);
			}

			if (strcmp("158", (const char *)command) == 0)
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

			if (strcmp("159", (const char *)command) == 0)
			{
				printf("Input value: radar No.(0=RADAR0/1=RADAR1) = ");
				ret = scanf("%ld", (long int *)&radarNo);
				printf("Input value: status(0=manual/1=auto) = ");
				ret = scanf("%d", (int *)&value1);
				printf("Input value: manual value(0 - 100) = ");
				ret = scanf("%d", (int *)&value2);
				ret = Furuno::RmcSetVideoLevel(radarNo, (Furuno::UINT16)value1, (Furuno::UINT16)value2);
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

/* Radar echo callback function */
static void __stdcall RadarEchoDraw(Furuno::INT32 no, Furuno::INT16 status, Furuno::UINT8 *echo, Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 range, Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg)
{
	// Radar number(0 - 1)
	printf("Echo callback: radarNo=%ld, ", (long int)no);

	// type of echo data (0=data none, 1=raw echo)
	printf("status=%d\n", status);

	// UINT8 *echo : storing pointer of a sweep of echo data
	// It was too much, so I omitted.

	// sweep length
	printf("sweep length=%d, ", sweep_len);

	// scale (echo data position corresponding to present range)
	printf("scale=%d, ", scale);

	// range scale (0 - 24)
	printf("range=%d, ", range);

	// relative antenna angle (0 - 8191)
	printf("angle=%d, ", angle);

	// heading correction value (relative angle of heading) (0 - 8191)
	printf("heading=%d, ", heading);

	// Heading Status
	if (h_flg == 0)
	{
		printf("Heading Status=FALSE");
	}
	else
	{
		printf("Heading Status=TRUE");
	}
	printf("\n");
}

/* Radar echo tile callback function */
static void __stdcall EchoTileCallback(Furuno::INT32 radarNo, Furuno::INT16 heading, Furuno::INT16 headingStatus, Furuno::INT16 range, Furuno::INT16 rangeUnit, Furuno::INT16 scale, Furuno::INT16 xPos, Furuno::INT16 yPos, Furuno::INT16 sweepCenterX, Furuno::INT16 sweepCenterY, Furuno::INT16 tileSize, unsigned char *echo)
{
	// Radar number(0 - 1)
	printf("Echo callback: radarNo=%ld, ", (long int)radarNo);

	// heading
	printf("heading=%d\n", heading);

	// type of echo data (0=data none, 1=raw echo)
	printf("headingStatus=%d\n", headingStatus);

	// range scale
	printf("range=%d, ", range);

	// rangeUnit
	printf("rangeUnit=%d, ", rangeUnit);

	// xPos
	printf("xPos=%d, ", xPos);

	// yPos
	printf("yPos=%d, ", yPos);

	// sweepCenterX
	printf("sweepCenterX=%d, ", sweepCenterX);

	// sweepCenterY
	printf("sweepCenterY=%d, ", sweepCenterY);

	// tileSize
	printf("tileSize=%d, ", tileSize);

	// printf("Echo tile callback\n");
}

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
#ifdef WIN32 /*  WIN32  */
#if FLM_COMPILE_NEW_LICENSE == 1
bool License(std::string vendorCode, std::string vendorPassword, std::string vendorId, std::string deviceInfo)
{
	int err_cnt = 0;

	if (vendorCode == "vendor code")
	{
		err_cnt++;
	}
	if (vendorPassword == "keycode")
	{
		err_cnt++;
	}
	if (vendorId == "vendor id")
	{
		err_cnt++;
	}
	if (deviceInfo == "device id")
	{
		err_cnt++;
	}
	if (err_cnt != 0)
	{
		return 0;
	}

	// convert password string to bytes
	unsigned char *convertedPassword = NULL;
	int convertedPasswordSize = 0;
	HexStringToBytes(vendorPassword, convertedPassword, convertedPasswordSize);

	unsigned char *convertedDeviceInfo = NULL;
	int convertedDeviceInfoSize = 0;
	HexStringToBytes(deviceInfo, convertedDeviceInfo, convertedDeviceInfoSize);

	Furuno::INT32 status;
	Furuno::FurunoPermissionData data;

	memset(&data, 0, sizeof(data));

	// Copy vendor code to struct
	data.vendor_code_size = vendorCode.size();

	strncpy((char *)&data.vendor_code[0], vendorCode.c_str(), 64);

	memcpy(data.password, convertedPassword, convertedPasswordSize);
	data.password_size = convertedPasswordSize;

	data.vendor_id_size = vendorId.size();
	strncpy((char *)&data.vendor_id[0], vendorId.c_str(), 8);

	memcpy(data.encrypted_device_info, convertedDeviceInfo, convertedDeviceInfoSize);
	data.encrypted_device_info_size = convertedDeviceInfoSize;

	// validate
	Furuno::FlmInitial(&status, (Furuno::UINT8 *)&data, sizeof(data), Furuno::FLM_TYPE_NEW_LICENSE);

	// Cleanup temporary password
	if (convertedPassword)
	{
		delete convertedPassword;
		convertedPassword = NULL;
	}
	if (convertedDeviceInfo)
	{
		delete convertedDeviceInfo;
		convertedDeviceInfo = NULL;
	}

	return status == Furuno::FLM_STATE_SUCCESS;
}
bool TargetLicense(std::string filePath)
{
	int flm_err = 0;
	std::ifstream ifs(filePath);
	std::string str;

	if (ifs.fail())
	{
		return 0;
	}

	while (getline(ifs, str))
	{
		// delete CR or LF
		if (str.size() >= 1)
		{
			if (str[str.size() - 1] == '\r' || str[str.size() - 1] == '\n')
			{
				str.erase(str.size() - 1);
			}
		}
		// delete CR or LF
		if (str.size() >= 1)
		{
			if (str[str.size() - 1] == '\r' || str[str.size() - 1] == '\n')
			{
				str.erase(str.size() - 1);
			}
		}

		// convert password string to bytes
		unsigned char *convertedTargetInfo = NULL;
		int convertedTargetInfoSize = str.size();
		HexStringToBytes(str.c_str(), convertedTargetInfo, convertedTargetInfoSize);

		flm_err = Furuno::FlmPreparation((const Furuno::UINT8 *)convertedTargetInfo, convertedTargetInfoSize);
		if (convertedTargetInfo)
		{
			delete convertedTargetInfo;
			convertedTargetInfo = NULL;
		}

		if (flm_err != 0)
		{
			break;
		}
	}
	return flm_err == 0;
}
#endif
#endif /*  WIN32  */

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
#ifdef WIN32 /*  WIN32  */
#if FLM_COMPILE_NEW_LICENSE == 1
void HexStringToBytes(const std::string strIn, unsigned char *&array, int &size)
{
	std::string str = strIn;
	int length = str.length();

	if (length % 2 == 1)
	{
		str = "0" + str;
		length++;
	}

	array = new unsigned char[length / 2];
	size = length / 2;

	std::stringstream sstr(str);
	for (int i = 0; i < size; i++)
	{
		char ch1, ch2;
		sstr >> ch1 >> ch2;
		int dig1 = 0, dig2 = 0;
		if (isdigit(ch1))
			dig1 = ch1 - '0';
		else if (ch1 >= 'A' && ch1 <= 'F')
			dig1 = ch1 - 'A' + 10;
		else if (ch1 >= 'a' && ch1 <= 'f')
			dig1 = ch1 - 'a' + 10;
		if (isdigit(ch2))
			dig2 = ch2 - '0';
		else if (ch2 >= 'A' && ch2 <= 'F')
			dig2 = ch2 - 'A' + 10;
		else if (ch2 >= 'a' && ch2 <= 'f')
			dig2 = ch2 - 'a' + 10;
		array[i] = dig1 * 16 + dig2;
	}
}
#endif
#endif /*  WIN32  */

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

#if FLM_COMPILE_NEW_LICENSE == 1
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
#endif