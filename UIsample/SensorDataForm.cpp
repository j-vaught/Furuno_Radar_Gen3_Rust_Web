/**
* @file   SensorDataForm.cpp
*
* @brief
* Window"SensorData" of DllSample5
*
*/

#include "SensorDataForm.h"
#include "RadarConnection.h"
#include "Api.h"

//------------------------------------------------
//  Global variable definition
//------------------------------------------------
Furuno::INT32 radarNo;

wxDEFINE_EVENT(wxEVT_COMMAND_MYTHREAD_UPDATE, wxThreadEvent);

///////////////////////////////////////////////////////////////////////////

SensorDataForm::SensorDataForm(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style)
{
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);

	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer(wxVERTICAL);

	m_listCtrl1 = new wxListCtrl(this, wxNewId(), wxDefaultPosition, wxSize(-1, 500), wxLC_REPORT);
	bSizer1->Add(m_listCtrl1, 1, wxALL | wxEXPAND, 5);

	m_comboBox1 = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0);
	bSizer1->Add(m_comboBox1, 0, wxALL, 5);

	m_comboBox1->Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &SensorDataForm::m_comboBox1OnCombobox, this);
	Bind(wxEVT_COMMAND_MYTHREAD_UPDATE, &SensorDataForm::OnThreadUpdate, this);
	Bind(wxEVT_CLOSE_WINDOW, &SensorDataForm::OnClose, this);


	this->SetSizer(bSizer1);
	this->Layout();

	this->Centre(wxBOTH);
}

SensorDataForm::~SensorDataForm()
{
	m_comboBox1->Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, &SensorDataForm::m_comboBox1OnCombobox, this);
	Unbind(wxEVT_COMMAND_MYTHREAD_UPDATE, &SensorDataForm::OnThreadUpdate, this);
	Unbind(wxEVT_CLOSE_WINDOW, &SensorDataForm::OnClose, this);

	delete m_listCtrl1;
	delete m_comboBox1;
}

void SensorDataForm::InitialSensorDataForm()
{
	AddRadarNoToCombobox(m_comboBox1);

	m_listCtrl1->InsertColumn(0, "No.", wxLIST_FORMAT_LEFT, 50);
	m_listCtrl1->InsertColumn(1, "Distance(nm)", wxLIST_FORMAT_LEFT, 90);
	m_listCtrl1->InsertColumn(2, "Bearing(T)", wxLIST_FORMAT_LEFT, 80);
	m_listCtrl1->InsertColumn(3, "Speed(T)", wxLIST_FORMAT_LEFT, 80);
	m_listCtrl1->InsertColumn(4, "Cource(T)", wxLIST_FORMAT_LEFT, 80);
	m_listCtrl1->InsertColumn(5, "Speed(R)", wxLIST_FORMAT_LEFT, 80);
	m_listCtrl1->InsertColumn(6, "Cource(R)", wxLIST_FORMAT_LEFT, 80);
	m_listCtrl1->InsertColumn(7, "COUNT", wxLIST_FORMAT_LEFT, 80);

	for (int i = 1; i <= ARPATARGET_MAX; i++)
	{
		wxString number = wxString::Format(wxT("%i"), i);
		m_listCtrl1->SetItemData(m_listCtrl1->InsertItem(i, number, 0), i);
	}

	m_comboBox1->SetSelection(0);
	radarNo = m_comboBox1->GetSelection();
}

void SensorDataForm::m_comboBox1OnCombobox(wxCommandEvent& event)
{
	radarNo = m_comboBox1->GetSelection();

	m_listCtrl1->ClearAll();

	m_listCtrl1->InsertColumn(0, "No.", wxLIST_FORMAT_LEFT, 50);
	m_listCtrl1->InsertColumn(1, "Distance(nm)", wxLIST_FORMAT_LEFT, 90);
	m_listCtrl1->InsertColumn(2, "Bearing(T)", wxLIST_FORMAT_LEFT, 80);
	m_listCtrl1->InsertColumn(3, "Speed(T)", wxLIST_FORMAT_LEFT, 80);
	m_listCtrl1->InsertColumn(4, "Cource(T)", wxLIST_FORMAT_LEFT, 80);
	m_listCtrl1->InsertColumn(5, "Speed(R)", wxLIST_FORMAT_LEFT, 80);
	m_listCtrl1->InsertColumn(6, "Cource(R)", wxLIST_FORMAT_LEFT, 80);
	m_listCtrl1->InsertColumn(7, "COUNT", wxLIST_FORMAT_LEFT, 80);

	for (int i = 1; i <= ARPATARGET_MAX; i++)
	{
		wxString number = wxString::Format(wxT("%i"), i);
		m_listCtrl1->SetItemData(m_listCtrl1->InsertItem(i, number, 0), i);
	}
}


void SensorDataForm::DoStartThread()
{
	m_pThread = new SensorDataFormThread(this);

	if (m_pThread->Run() != wxTHREAD_NO_ERROR)
	{
		wxLogError("Can't start thread!");
		delete m_pThread;
		m_pThread = NULL;
	}
}

void SensorDataForm::OnThreadUpdate(wxThreadEvent& event)
{
	int i = 1;
	int item_max = 7;
	wxListItem NewItem;
	GetARPAData GetData = event.GetPayload<GetARPAData>();

	if (GetData.TargetStatus == 0) // If the target is been tracking 
	{
		this->m_listCtrl1->SetItem(GetData.Cells0Value - 1, 1, wxString::Format(wxT("%g"), GetData.Cells1Value));
		this->m_listCtrl1->SetItem(GetData.Cells0Value - 1, 2, wxString::Format(wxT("%g"), GetData.Cells2Value));
		this->m_listCtrl1->SetItem(GetData.Cells0Value - 1, 3, wxString::Format(wxT("%g"), GetData.Cells3Value));
		this->m_listCtrl1->SetItem(GetData.Cells0Value - 1, 4, wxString::Format(wxT("%g"), GetData.Cells4Value));
		this->m_listCtrl1->SetItem(GetData.Cells0Value - 1, 5, wxString::Format(wxT("%g"), GetData.Cells5Value));
		this->m_listCtrl1->SetItem(GetData.Cells0Value - 1, 6, wxString::Format(wxT("%g"), GetData.Cells6Value));
		this->m_listCtrl1->SetItem(GetData.Cells0Value - 1, 7, wxString::Format(wxT("%i"), GetData.Cells7Value));
	
		NewItem.SetTextColour(wxColor(0, 0, 0));
		NewItem.SetId(GetData.Cells0Value - 1);
		this->m_listCtrl1->SetItem(NewItem);
	}

	else if (GetData.TargetStatus == 1) // If the target is lost
	{
		NewItem.SetText("Lost");
		NewItem.SetTextColour(wxColor(255, 0, 0));
		NewItem.SetId(GetData.Cells0Value - 1);
		for (i = 1; i <= item_max; i++){
			NewItem.SetColumn(i);
			this->m_listCtrl1->SetItem(NewItem);
		}
	}

	else if (GetData.TargetStatus == 2) // If the target is deleted
	{
		NewItem.SetText("Deleted");
		NewItem.SetTextColour(wxColor(128, 128, 128));
		NewItem.SetId(GetData.Cells0Value - 1);
		for (i = 1; i <= item_max; i++){
			NewItem.SetColumn(i);
			this->m_listCtrl1->SetItem(NewItem);
		}
		
	}
	else
	{

	}
	this->m_listCtrl1->Refresh();

}

wxThread::ExitCode SensorDataFormThread::Entry()
{
	wxThreadEvent* event2 = new wxThreadEvent(wxEVT_COMMAND_MYTHREAD_UPDATE);

	Furuno::ARPA arpa;
	Furuno::INT32 ret = RADAR_NG;
	SensorDataForm::GetARPAData SendData = { 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0 };

	while (!TestDestroy())
	{
		for (int i = 1; i <= ARPATARGET_MAX; i++)
		{
			ret = Furuno::RmaGetArpaInfo(radarNo, i, &arpa);
			if (ret == RADAR_OK)
			{
				if (arpa.status1 & (1 << 5))
				{
					SendData.TargetStatus = 1;
					SendData.Cells0Value = i;
					event2->SetPayload(SendData);
					Furuno::RmcSetArpaCancelTargetId(radarNo, i);
				}

				else
				{
					SendData.TargetStatus = 0;
					SendData.Cells0Value = i;
					SendData.Cells1Value = (double)arpa.ccrp_dist / 10000.0;
					SendData.Cells2Value = (double)arpa.ccrp_bear / 10.0;
					SendData.Cells3Value = (double)arpa.t_speed / 10.0;
					SendData.Cells4Value = (double)arpa.t_course / 10.0;
					SendData.Cells5Value = (double)arpa.r_speed / 10.0;
					SendData.Cells6Value = (double)arpa.r_course / 10.0;
					SendData.Cells7Value = arpa.count;
					event2->SetPayload(SendData);
					
				}
				wxQueueEvent(m_pHandler, event2->Clone());
			}

			if (ret == RADAR_NODATA)
			{
				if (arpa.status1 & (1 << 2)) 
				{
					SendData.TargetStatus = 2;
					SendData.Cells0Value = i;
					event2->SetPayload(SendData);
					wxQueueEvent(m_pHandler, event2->Clone());
				}			
			}
		}		
		wxThread::Sleep(100);
	}

	return (wxThread::ExitCode)0;     // success
}

SensorDataFormThread::~SensorDataFormThread()
{
	wxCriticalSectionLocker enter(m_pHandler->m_pThreadCS);

	m_pHandler->m_pThread = NULL;

}

void SensorDataForm::OnClose(wxCloseEvent& event)
{

	{
		wxCriticalSectionLocker enter(m_pThreadCS);

		if (m_pThread)        
		{
			wxMessageOutputDebug().Printf("MYFRAME: deleting thread");

			if (m_pThread->Delete() != wxTHREAD_NO_ERROR)
				wxLogError("Can't delete the thread!");
		}
	}       

	for (;;)
	{
		{ 
			wxCriticalSectionLocker enter(m_pThreadCS);
			if (!m_pThread) break;
		}

		wxThread::This()->Sleep(1);
	}

	Destroy();
}
