/**
* @file   SensorDataForm.h
*
* @brief
* Window"SensorData" of DllSample5
*
*/

#ifndef DLLSAMPLE5_SENSORDATAFORM_H
#define DLLSAMPLE5_SENSORDATAFORM_H
#define FLM_COMPILE_NEW_LICENSE 1

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/listctrl.h>
#include <wx/gdicmn.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/combobox.h>
#include <wx/sizer.h>
#include <wx/frame.h>
#include <wx/thread.h>
#include <wx/event.h>

#if !wxUSE_THREADS
#error "This sample requires thread support!"
#endif // wxUSE_THREADS

//------------------------------------------------
//  Forward declaration
//------------------------------------------------
class SensorDataForm;


///////////////////////////////////////////////////////////////////////////////
/// Class SensorDataFormThread
///////////////////////////////////////////////////////////////////////////////
class SensorDataFormThread : public wxThread
{
public:

	SensorDataFormThread(SensorDataForm *handler)
		: wxThread(wxTHREAD_DETACHED)
	{
		m_pHandler = handler;
	}
	~SensorDataFormThread();

protected:
	virtual ExitCode Entry(void);
	SensorDataForm *m_pHandler;
};

///////////////////////////////////////////////////////////////////////////////
/// Class SensorDataForm
///////////////////////////////////////////////////////////////////////////////
class SensorDataForm : public wxFrame
{
public:

	SensorDataForm(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("SensorData"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(650, 600), long style = wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL);

	~SensorDataForm();

	void InitialSensorDataForm(void);
	void DoStartThread(void);
	void OnThreadCompletion(wxThreadEvent& event);
	void OnThreadUpdate(wxThreadEvent& event);

	SensorDataFormThread *m_pThread = NULL;
	wxCriticalSection m_pThreadCS;

	typedef struct{
		int TargetStatus; //Target found : 0, Target Lost : 1, Target Deleted : 2
		int Cells0Value;
		double Cells1Value;
		double Cells2Value;
		double Cells3Value;
		double Cells4Value;
		double Cells5Value;
		double Cells6Value;
		short Cells7Value;
	}GetARPAData;

protected:

private:
	wxListCtrl* m_listCtrl1;
	wxComboBox* m_comboBox1;

	void OnClose(wxCloseEvent& event);
	void m_comboBox1OnCombobox(wxCommandEvent& event);
};

#endif //DLLSAMPLE5_SENSORDATAFORM_H
