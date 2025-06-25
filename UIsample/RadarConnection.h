// RadarConnection.h

/**
* @file   RadarConnection.h
*
* @brief
* Window"Radar" of DllSample5
*
*/

#ifndef DLLSAMPLE5_RADAR_H
#define DLLSAMPLE5_RADAR_H

#include <wx/wxprec.h> 
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/frame.h>
#include <wx/thread.h>
#include <wx/event.h>

#include "WebServer.h" // Include the WebServer header

#ifdef _WIN32
namespace Furuno
{
#include <FECLIB/fectype.h>
}
#endif //_WIN32

#ifdef __linux
namespace Furuno
{
#include "nndll.h"
}
#endif //__linux

#if !wxUSE_THREADS
#error "This sample requires thread support!"
#endif // wxUSE_THREADS

//------------------------------------------------
//  Macro definition
//------------------------------------------------
#define COMBO_MAX_ID (100)
//#define USE_PRIVATE_FILES //for developers
//#define USE_PRIVATE_DEBUGMODE //for developers

//------------------------------------------------
//  External variable declaration
//------------------------------------------------
extern Furuno::UINT8 g_ip_addr[COMBO_MAX_ID];
extern Furuno::UINT8 g_subnet_mask[COMBO_MAX_ID];
extern Furuno::INT32 g_radarnum_max;
extern bool g_selected_dualrange;

//------------------------------------------------
//  Forward declaration
//------------------------------------------------
class Radar;


///////////////////////////////////////////////////////////////////////////////
// /// Class RadarThread
///////////////////////////////////////////////////////////////////////////////
class RadarThread : public wxThread
{
public:

    RadarThread(Radar *handler)
        : wxThread(wxTHREAD_DETACHED)
    {
        m_pHandler = handler;
    }
    ~RadarThread(void);

protected:
    virtual ExitCode Entry(void);
    Radar *m_pHandler;
};

///////////////////////////////////////////////////////////////////////////////
// /// Class Radar
///////////////////////////////////////////////////////////////////////////////
class Radar : public wxFrame
{
public:
    Radar(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Radar"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(760, 330), long style = wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL);

    ~Radar(void);

    RadarThread *m_pThread = NULL;
    wxCriticalSection m_pThreadCS;

    void DoStartThread(void);
    void OnThreadUpdate(wxThreadEvent& event);
    void EnableAndDisableButton(wxThreadEvent& event);

protected:

private:
    wxButton* m_button1;
    wxButton* m_button2;
    wxButton* m_button3;
    wxTextCtrl* m_textCtrl1;

    void m_button1OnButtonClick(wxCommandEvent& event); //"View Echo"
    void m_button2OnButtonClick(wxCommandEvent& event); //"View ARPA"
    void m_button3OnButtonClick(wxCommandEvent& event); //"Command Tool"
    void DisableButton(void);
    void OnClose(wxCloseEvent& event);

    wxArrayString   m_arrItems;

    // Add web server member
    WebServer webServer_;
};

#endif //DLLSAMPLE5_RADAR_H
