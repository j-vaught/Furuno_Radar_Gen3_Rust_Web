/**
* @file   RadarEcho.h
*
* @brief
* Window"RadarEcho" of DllSample5
*
*/

#ifndef DLLSAMPLE5_RADARECHO_H
#define DLLSAMPLE5_RADARECHO_H
#define FLM_COMPILE_NEW_LICENSE 1

#include <mutex>
#include <array>
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <sys/stat.h>
#include <sys/types.h>


#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif


#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/combobox.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/frame.h>
#include <wx/rawbmp.h>
#include <wx/thread.h>
#include <wx/event.h>
#include "EchoServer.h"

namespace Furuno { 
#include "nndll.h" 
}


//------------------------------------------------
//  External variable declaration
//------------------------------------------------
extern bool g_selected_dualrange;

//------------------------------------------------
//  Macro definition
//------------------------------------------------
//#define DEBUG_MODE
#define RENDER_FPS (30.0)
#define Define_ECHO_RADIUS  (1024) 
#define Define_DISP_SIZE_X  (Define_ECHO_RADIUS * 2)
#define Define_DISP_SIZE_Y  (Define_ECHO_RADIUS * 2)
#define Define_CENTER_X  (Define_DISP_SIZE_X / 2) 
#define Define_CENTER_Y  (Define_DISP_SIZE_Y / 2) 
#define Define_LINE360DEG  (8192)	
#define Define_LINE270DEG  (6144)	
#define Define_LINE180DEG  (4096)
#define Define_LINE90DEG  (2048)
#define Define_ANGLE  (0.0439453125) 
#define Define_PAI  (3.1415926535) 
#define WINDOW_WIDTH (750) 
#define WINDOW_HEIGHT (750) 
#define MAX_SWEEP_LENGTH (1024)
#define MAX_SWEEP_STORE (2048)
#define MARGIN_OF_IMAGE (50) 


//Structures
struct RingBufferInfo_s {
	RingBufferInfo_s(Furuno::UINT16 p_write, Furuno::UINT16 p_preprocess, Furuno::UINT16 p_read, Furuno::UINT16 s_data) :
		writePoint{ p_write }, preprocessPoint{ p_preprocess }, readPoint{ p_read }, dataSize{ s_data }{}

	Furuno::UINT16 writePoint;
	Furuno::UINT16 preprocessPoint;
	Furuno::UINT16 readPoint;
	Furuno::UINT16 dataSize;
};

struct EchoInfo_s { 
	Furuno::INT16 status;
	Furuno::INT16 sweepLen;
	Furuno::INT16 scale;
	Furuno::INT16 range;
	Furuno::INT16 angle;
	Furuno::INT16 heading;
	Furuno::INT16 headingFlg;
	bool isStartOfTurn;
};

struct RenderingInformation_s {
	int16_t Range;
	int16_t scale;
	int16_t sweepLength;
	int16_t headingFlag;
	bool isNewTurn;
	int16_t turnStartIndex;
	bool doScaling;
	double scalingFactor;
	bool canRThetaScaling;
};


//Enums
typedef enum setEchoDataStatus_e {
	RdlError_OK = 0,
	RdlError_InvalidArgument,
}setEchoDataStatus_t;

typedef enum readEchoDataStatus_e {
	OK,
	NoSweep, 
	InvalidArgument,
}readEchoDataStatus_t;


//------------------------------------------------
//  Forward declaration
//------------------------------------------------
class RadarEcho;
class RadarEchoCanvas;
Furuno::UINT16 Calculate_Remainder(Furuno::UINT16 dividend, Furuno::UINT16 divisor);
void radarRangeNumberToNM(Furuno::INT16 range, double *nmrange, double *dradius);


///////////////////////////////////////////////////////////////////////////////
/// Class SensorDataFormThread
///////////////////////////////////////////////////////////////////////////////
class RadarEchoThread : public wxThread
{
public:

	RadarEchoThread(RadarEcho *handler)
		: wxThread(wxTHREAD_DETACHED)
	{
		m_pHandler = handler;
	}
	~RadarEchoThread();

protected:
	virtual ExitCode Entry(void);
	RadarEcho *m_pHandler;
};

///////////////////////////////////////////////////////////////////////////////
/// Class RadarEcho
///////////////////////////////////////////////////////////////////////////////
class RadarEcho : public wxFrame
{
public:

	RadarEcho(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("RadarEcho"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(1000, 770), long style = wxCAPTION | wxCLOSE_BOX | wxTAB_TRAVERSAL);
	~RadarEcho();
	static bool PrepareMakeImageBuf(Furuno::INT32 radarNo, Furuno::INT16 status, Furuno::UINT8* echo, Furuno::INT16 range, Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg);
	void AddRadarNoToCombobox(wxComboBox* comboBox); // Declaration of the new function
	void setCurrentSettingToComponent(void);
	static void BitmapUpdate(wxNativePixelData &data0, wxNativePixelData &data1);
	static void CopyImageBuf(void);
	RadarEchoCanvas *canvas1;

	static setEchoDataStatus_t storeSweep(Furuno::INT32 radarno, Furuno::INT16 status, Furuno::UINT8* echo, Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 Range, Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg);
	static readEchoDataStatus_t readEchoData(Furuno::INT32 radarno, std::array<uint8_t, MAX_SWEEP_LENGTH> & rThetaTextureData, EchoInfo_s & outputEchoInfo);

	void DoStartThread(void);
	void OnThreadUpdate(wxThreadEvent& event);
	RadarEchoThread *m_pThread = NULL;
	wxCriticalSection m_pThreadCS;
	void OnClose(wxCloseEvent& event);
	void DoPauseThread();
protected:

private:
	wxStaticText* m_staticText1;
	wxStaticText* m_staticText2;
	wxComboBox* m_comboBox1;
	wxComboBox* m_comboBox2;
	wxStaticText* m_staticText3;
	wxComboBox* m_comboBox3;
	wxStaticText* m_staticText4;
	wxComboBox* m_comboBox4;
	wxStaticText* m_staticText5;
	wxComboBox* m_comboBox5;

	void m_comboBox1OnCombobox(wxCommandEvent& event); //"Radar No"
	void m_comboBox2OnCombobox(wxCommandEvent& event); //"Dual Display"
	void m_comboBox3OnCombobox(wxCommandEvent& event); //"Interpolation"
	void m_comboBox4OnCombobox(wxCommandEvent& event); //"Set sweep"
	void m_comboBox5OnCombobox(wxCommandEvent& event); //"Display Mode"	
	
	static bool MakeImageBuf(Furuno::INT32 radarNo, Furuno::UINT8* echo, Furuno::INT16 ang);
	static int ChangeColor(Furuno::UINT8 color);
	
	static void MakeDotTable(void);

	
	static bool writeEchoData(Furuno::INT32 radarno, Furuno::INT16 status, Furuno::UINT8* echo, Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 Range, Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg);

	static unsigned int m_packetReceiveCount0;
	static unsigned int m_packetReceiveCount1;

	static std::array<std::array<uint8_t, MAX_SWEEP_LENGTH>, MAX_SWEEP_STORE> m_echoSweepBufferForData0;
	static std::array<std::array<uint8_t, MAX_SWEEP_LENGTH>, MAX_SWEEP_STORE> m_echoSweepBufferForData1;

	static std::array<EchoInfo_s, MAX_SWEEP_STORE> m_echoSweepBufferForAddInfo0;
	static std::array<EchoInfo_s, MAX_SWEEP_STORE> m_echoSweepBufferForAddInfo1;

private:
	static RingBufferInfo_s m_ringBufferInfo0;
	static RingBufferInfo_s m_ringBufferInfo1;
	EchoServer* echoServer;
    std::mutex imageMutex_;

};

///////////////////////////////////////////////////////////////////////////////
/// Class RadarEchoCanvas
///////////////////////////////////////////////////////////////////////////////
class RadarEchoCanvas : public wxPanel
{
public:
	RadarEchoCanvas(RadarEcho *parent);
	
	~RadarEchoCanvas();
	
	RadarEcho *m_radarui = NULL;

	void OnPaint(wxPaintEvent& event);
	void GetRadarEchoUI(RadarEcho *radarechoui);	

protected:

private:
	RadarEcho *owner;
	bool InitialOnPaint = true;
	wxColor wxgreen = wxColour(17, 255, 17);
	wxColor wxcyan = wxColour(0, 255, 255);
};

#endif //DLLSAMPLE5_RADARECHO_H