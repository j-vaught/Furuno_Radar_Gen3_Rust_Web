/**
* @file   Api.h
*
* @brief
* Api for DllSample5
*
*/


#ifndef DLLSAMPLE5_API_H
#define DLLSAMPLE5_API_H
#define FLM_COMPILE_NEW_LICENSE 1

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <thread>
#include <chrono>
#include <sstream>

namespace Furuno
{
#ifdef __linux
#include "nndll.h"
#include "lasterror.h"
#endif //__linux
}

//------------------------------------------------
//  External variable declaration
//------------------------------------------------
extern bool g_selected_dualrange;

//------------------------------------------------
//  Macro definition
//------------------------------------------------
#define RADAR_WAITING_TIME (30) 
#define RADAR_HOSTNAME_MAX (2)
#define RADAR_HOSTNAME_LENGTH (9) 

#define RADAR_OK           (0)
#define RADAR_NG           (-1)
#define RADAR_NODATA       (-2)
#define	RADAR_INIT			(-3)
#define	RADAR_PARAM_ERROR	(-4)
#define POLYGON_MAX	(10)
#define ARPATARGET_MAX		(255)
#define RMCGET_LATEST (-1)
#define RMCTEMPOLARY (-2)


namespace Furuno
{
#if FLM_COMPILE_NEW_LICENSE == 1
	//Structures
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
		Furuno::UINT8 preset_device_id[40]; // Use only iOS
	};
#endif

	//Enums
	enum FunctionCode
	{
		Common,         // Common
		Radar,		    // Radar Server
		Sounder,	    // Sounder Server
		Plotter,	    // 
		Mcd,	        //
		Mcs,	        //
		Fax,	        //
		Mcd2,	        //
		Mcs2,	        //
		System,	        // System Server
		Key,	        // Keyboard Server
		Database,       // Database Server
		Fls,			// 
		Capture,		// 
		Simulator,		//
		Relay,			//
		PowerSync,		// 
		Settings,        //
		MaxFunctionCode,
	};

	#ifdef __linux
	typedef enum {
		FLM_ERR_SUCCESS = 0,
		FLM_ERR_ARGINVALID,
		FLM_ERR_FAILURE,
		FLM_ERR_MAX
	} FLM_ERR;
	#endif //__linux
}

typedef enum
{
	NoRadar,
	RadarFound,
	ThreadClosed,
	AbnormalReturnValue,
} RadarError;



//------------------------------------------------
//  Forward declaration
//------------------------------------------------
Furuno::FLM_ERR License(std::string vendorCode, std::string vendorPassword, std::string vendorId, std::string deviceInfo);
bool TargetLicense(std::string filePath);
void HexStringToBytes(const std::string strIn, unsigned char* &array, int& size);
bool StartNetwork(std::string hostname, Furuno::UINT8 *ip_addr, Furuno::UINT8 *subnet);
bool StartRadarModule(void);
RadarError  FindRadar(RadarThread* thread, wxThreadEvent* event, Radar *m_pHandler);
void ConvertStringtoUINT8(Furuno::UINT8 *ip_addr, std::string _IpAdd);
#ifdef USE_PREMIUM_VERSION
bool initNmea0183Module(void);
int __stdcall NMEA_ReadPort1Cbk(char *buf, int size);
#endif //USE_PREMIUM_VERSION
void AddRadarNoToCombobox(wxComboBox* m_comboBox);

//Callbacks
void __stdcall CommandCallback(Furuno::INT32 id, Furuno::INT32 no);
void __stdcall CycleCallback(void);
void __stdcall EchoCallback(Furuno::INT32 radarNo, Furuno::INT16 status, Furuno::UINT8 *echo,
	Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 range,
	Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg);
void __stdcall EchoTileCallback(Furuno::INT32 radarNo, Furuno::INT16 heading, Furuno::INT16 headingStatus,
	Furuno::INT16 range, Furuno::INT16 rangeUnit, Furuno::INT16 scale,
	Furuno::INT16 xPos, Furuno::INT16 yPos, Furuno::INT16 sweepCenterX,
	Furuno::INT16 sweepCenterY, Furuno::INT16 tileSize, unsigned char *echo);
void P_SensorNo(wxThreadEvent* event, Radar *m_pHandler);

#endif //DLLSAMPLE5_API_H

