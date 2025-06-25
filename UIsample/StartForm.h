/**
* @file   StartForm.h
*
* @brief
* Window"dll_sample" of DllSample5
*
*/

#ifndef DLLSAMPLE5_STARTFORM_H
#define DLLSAMPLE5_STARTFORM_H
#define FLM_COMPILE_NEW_LICENSE 1


#ifdef __linux
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#endif

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/settings.h>
#include <wx/filepicker.h>
#include <wx/combobox.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/frame.h>

#ifdef __linux
namespace Furuno{
	#include "nndll.h"
}
#endif //__linux

///////////////////////////////////////////////////////////////////////////////
/// Class MyApp
///////////////////////////////////////////////////////////////////////////////
class MyApp : public wxApp
{
public:
	virtual bool OnInit(void);

};

///////////////////////////////////////////////////////////////////////////////
/// Class StartForm
///////////////////////////////////////////////////////////////////////////////
class StartForm : public wxFrame
{

public:
#if FLM_COMPILE_NEW_LICENSE == 1
	StartForm(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("dll_sample"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(600, 400), long style = wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL, const wxString& name = wxT("dll_sample"));
#endif

	~StartForm();

	int  InitialStartForm(void);

protected:

private:
	wxArrayString   m_arrItems;

#if FLM_COMPILE_NEW_LICENSE == 1
	wxStaticText* m_staticText1;
	wxFilePickerCtrl* m_filePicker1;
	wxStaticText* m_staticText2;
	wxFilePickerCtrl* m_filePicker2;
	wxStaticText* m_staticText3;
	wxComboBox* m_comboBox1;
	wxStaticText* m_staticText4;
	wxComboBox* m_comboBox2;
	wxButton* m_button1;	

	void m_filePicker1OnFileChanged(wxFileDirPickerEvent& event); //"Select vendor.txt"
	void m_filePicker2OnFileChanged(wxFileDirPickerEvent& event); //"Select target.dat"
	void m_comboBox1OnCombobox(wxCommandEvent& event); //"Select IP Address"
	void m_comboBox2OnCombobox(wxCommandEvent& event); //"Dual Range Mode"
	void m_button1OnButtonClick(wxCommandEvent& event);
	// Declare the member variable
    bool m_allAutomaticallySelected;

    // Declare the member function
    void ProceedToNextPage();
#endif
};


#endif //DLLSAMPLE5_STARTFORM_H