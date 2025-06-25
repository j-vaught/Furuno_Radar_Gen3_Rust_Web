/**
* @file   StartForm.cpp
*
* @brief
* Window"dll_sample" of DllSample5
*
*/

#include "StartForm.h"
#include "RadarConnection.h"
#include "Api.h"
#include <string>
#include <wx/filename.h>
#include <wx/stdpaths.h>

//------------------------------------------------
//  Global variable definition
//------------------------------------------------
Furuno::UINT8 g_ip_addr[COMBO_MAX_ID];
Furuno::UINT8 g_subnet_mask[COMBO_MAX_ID];
std::string g_SubnetMask;
char g_IpAdd[COMBO_MAX_ID][INET_ADDRSTRLEN] = {};
wxString g_wx_LicensePath;
#if FLM_COMPILE_NEW_LICENSE == 1
wxString g_wx_TargetPath;
#endif
wxString g_wx_IpAdd;
bool g_selected_dualrange = false;

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit ()
{
	StartForm *frame1 = new StartForm(NULL);
	frame1->Show(true);
	frame1->InitialStartForm();
	return true;
}

///////////////////////////////////////////////////////////////////////////

StartForm::StartForm (wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style, const wxString& name) : wxFrame(parent, id, title, pos, size, style, name)
{
#if FLM_COMPILE_NEW_LICENSE == 1
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);

	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer(wxVERTICAL);

	m_staticText1 = new wxStaticText(this, wxID_ANY, wxT("Select vendor.txt"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText1->Wrap(-1);
	bSizer1->Add(m_staticText1, 1, wxALL, 5);

	m_filePicker1 = new wxFilePickerCtrl(this, wxID_ANY, wxEmptyString, wxT("Select vendor.txt"), wxT("*.txt"), wxDefaultPosition, wxSize(500, -1), wxFLP_DEFAULT_STYLE);
	bSizer1->Add(m_filePicker1, 1, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

	m_staticText2 = new wxStaticText(this, wxID_ANY, wxT("Select target.dat"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText2->Wrap(-1);
	bSizer1->Add(m_staticText2, 1, wxALL, 5);

	m_filePicker2 = new wxFilePickerCtrl(this, wxID_ANY, wxEmptyString, wxT("Select target.dat"), wxT("*.dat"), wxDefaultPosition, wxSize(500, -1), wxFLP_DEFAULT_STYLE);
	bSizer1->Add(m_filePicker2, 1, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

	m_staticText3 = new wxStaticText(this, wxID_ANY, wxT("Select IP Address"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText3->Wrap(-1);
	bSizer1->Add(m_staticText3, 1, wxALL, 5);

	m_comboBox1 = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(500, -1), 0, NULL, 0);
	bSizer1->Add(m_comboBox1, 1, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

	m_staticText4 = new wxStaticText(this, wxID_ANY, wxT("Do you use Dual Range Mode?\n(Please read FAQ in the manual to check which models support the dual range function.)"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText4->Wrap(-1);
	bSizer1->Add(m_staticText4, 0, wxALL, 5);

	m_comboBox2 = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0);
	m_comboBox2->Append(wxT("No"));
	m_comboBox2->Append(wxT("Yes"));
	m_comboBox2->SetSelection(0);
	bSizer1->Add(m_comboBox2, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

	m_button1 = new wxButton(this, wxID_ANY, wxT("Init"), wxDefaultPosition, wxDefaultSize, 0);
	bSizer1->Add(m_button1, 1, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

	this->SetSizer(bSizer1);
	this->Layout();

	this->Centre(wxBOTH);

	m_filePicker1->Bind(wxEVT_COMMAND_FILEPICKER_CHANGED, &StartForm::m_filePicker1OnFileChanged, this);
	m_filePicker2->Bind(wxEVT_COMMAND_FILEPICKER_CHANGED, &StartForm::m_filePicker2OnFileChanged, this);
	m_comboBox1->Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &StartForm::m_comboBox1OnCombobox, this);
	m_comboBox2->Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &StartForm::m_comboBox2OnCombobox, this);
	m_button1->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &StartForm::m_button1OnButtonClick, this);

    // Initialize flag to track if all selections were made automatically
    m_allAutomaticallySelected = true;

    // Get the application's directory
    wxString appPath = wxStandardPaths::Get().GetExecutablePath();
    wxFileName appFileName(appPath);
    wxString appDir = appFileName.GetPath();

// After obtaining appDir
wxLogDebug("Application Directory: %s", appDir);

// Check for vendor.txt
wxFileName vendorFileName(appDir, "vendor.txt");
wxLogDebug("Checking for vendor.txt at: %s", vendorFileName.GetFullPath());
if (vendorFileName.Exists()) {
    m_filePicker1->SetPath(vendorFileName.GetFullPath());
    g_wx_LicensePath = vendorFileName.GetFullPath();
    wxLogDebug("vendor.txt found and set.");
} else {
    m_allAutomaticallySelected = false;
    wxLogDebug("vendor.txt not found.");
}

// Check for target.dat
wxFileName targetFileName(appDir, "target.dat");
wxLogDebug("Checking for target.dat at: %s", targetFileName.GetFullPath());
if (targetFileName.Exists()) {
    m_filePicker2->SetPath(targetFileName.GetFullPath());
    g_wx_TargetPath = targetFileName.GetFullPath();
    wxLogDebug("target.dat found and set.");
} else {
    m_allAutomaticallySelected = false;
    wxLogDebug("target.dat not found.");
}


#endif
}


StartForm::~StartForm ()
{
#if FLM_COMPILE_NEW_LICENSE == 1
	m_filePicker1->Unbind(wxEVT_COMMAND_FILEPICKER_CHANGED, &StartForm::m_filePicker1OnFileChanged, this);
	m_filePicker2->Unbind(wxEVT_COMMAND_FILEPICKER_CHANGED, &StartForm::m_filePicker2OnFileChanged, this);
	m_comboBox1->Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, &StartForm::m_comboBox1OnCombobox, this);
	m_comboBox2->Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, &StartForm::m_comboBox2OnCombobox, this);
	m_button1->Unbind(wxEVT_COMMAND_BUTTON_CLICKED, &StartForm::m_button1OnButtonClick, this);

	delete m_staticText1;
	delete m_filePicker1;
	delete m_staticText2;
	delete m_filePicker2;
	delete m_staticText3;
	delete m_comboBox1;
	delete m_button1;
#endif
}

#if __linux
int StartForm::InitialStartForm ()
{
    struct ifaddrs * ifAddrStruct = NULL;
    struct ifaddrs * ifa = NULL;
    void * tmpAddrPtr = NULL;
    char addressBuffer[INET_ADDRSTRLEN];
    char maskBuffer[INET_ADDRSTRLEN];
    int cnt = 0;

    getifaddrs(&ifAddrStruct);

    int indexOfDesiredIP = -1;

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }

        if (ifa->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;		
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

            tmpAddrPtr = &((struct sockaddr_in *)(ifa->ifa_netmask))->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, maskBuffer, INET_ADDRSTRLEN);

            wxString mystring(addressBuffer);
            this->m_comboBox1->Append(mystring);

            // Debug: Log each found IP address
            wxLogDebug("Found IP address: %s", mystring);

            // Check for desired IP address
            if (mystring == wxT("172.31.1.100")) {
                indexOfDesiredIP = cnt;
                // Debug: Desired IP found
                wxLogDebug("Desired IP 172.31.1.100 found at index: %d", cnt);
            }

            if (cnt < COMBO_MAX_ID){
                strcpy(g_IpAdd[cnt], maskBuffer);
            }
            cnt++;
        }
    }

    if (ifAddrStruct != NULL) {
        freeifaddrs(ifAddrStruct);
    }

    // Debug: Log whether the desired IP was found and the selection status
    if (indexOfDesiredIP != -1) {
        m_comboBox1->SetSelection(indexOfDesiredIP);
        g_wx_IpAdd = m_comboBox1->GetStringSelection();
        int selected_combo_id = m_comboBox1->GetSelection();
        if (selected_combo_id < COMBO_MAX_ID){
            g_SubnetMask = g_IpAdd[selected_combo_id];
        }
        wxLogDebug("Desired IP 172.31.1.100 was found and selected at index: %d", indexOfDesiredIP);
    } else {
        m_allAutomaticallySelected = false;
        wxLogDebug("Desired IP 172.31.1.100 was not found among the available IP addresses.");
    }

    // Entry functions
    int ret;
    char libName[] = "libNAVNETDLL.so";
    ret = Furuno::fec_entry_function(libName);
    switch (ret){
    case ENTRY_OK:
        printf("Entry function : OK\n");
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

    if (ret != ENTRY_OK){
        printf("Entry function failed and return now\n");
        return(-1);
    }

    // If all selections were made automatically, proceed to next page
    if (m_allAutomaticallySelected) {
        ProceedToNextPage();
    }

    return 0;
}
#endif


#if FLM_COMPILE_NEW_LICENSE == 1
void StartForm::m_filePicker1OnFileChanged (wxFileDirPickerEvent& event) //"Select vendor.txt"
{
    m_allAutomaticallySelected = false; // User made a change
	g_wx_LicensePath = m_filePicker1->GetPath();
}

void StartForm::m_filePicker2OnFileChanged (wxFileDirPickerEvent& event) //"Select target.dat"
{
    m_allAutomaticallySelected = false; // User made a change
	g_wx_TargetPath = m_filePicker2->GetPath();
}
#endif

void StartForm::m_comboBox1OnCombobox (wxCommandEvent& event) //"Select IP Address"
{
    m_allAutomaticallySelected = false; // User made a change
	int selected_combo_id = 0;
	g_wx_IpAdd = m_comboBox1->GetStringSelection();
	g_wx_IpAdd.ToStdString();
	selected_combo_id = m_comboBox1->GetSelection();
	if (selected_combo_id < COMBO_MAX_ID){
		g_SubnetMask = g_IpAdd[selected_combo_id];
	}

}

void StartForm::m_comboBox2OnCombobox(wxCommandEvent& event) //"Dual Range Mode"
{
    m_allAutomaticallySelected = false; // User made a change
	if (m_comboBox2->GetSelection() == 0){
		g_selected_dualrange = false;
	}
	else{
		g_selected_dualrange = true;
	}	
}

void StartForm::m_button1OnButtonClick (wxCommandEvent& event)
{
    ProceedToNextPage();
}

void StartForm::ProceedToNextPage()
{
	Furuno::UINT32 lastError = FEC_ERROR_OK;

#if FLM_COMPILE_NEW_LICENSE == 1
	std::string LicensePath;
	std::string TargetPath;
	std::string _IpAdd;

	LicensePath = g_wx_LicensePath.ToStdString();
	TargetPath = g_wx_TargetPath.ToStdString();
	_IpAdd = g_wx_IpAdd;

	std::string _vendor;
	std::string _password;
	std::string _id;
	std::string _device;
	std::ifstream ifs(LicensePath);

	std::string ErrorMessage;
	bool RadarOpen = true;

	// Convert std::string to Furuno::UINT8
	if (_IpAdd.empty()) {
		ErrorMessage = ErrorMessage + "IP Address is not selected!\n";
		RadarOpen = false;
	}
	else{
		ConvertStringtoUINT8(g_ip_addr, _IpAdd);
		ConvertStringtoUINT8(g_subnet_mask, g_SubnetMask);
	}
	
	// Get information from License.txt
	if (ifs.fail()) {
		ErrorMessage = ErrorMessage + "License.txt is not selected!\n";
		RadarOpen = false;
	}
	else{
		getline(ifs, _vendor);
		getline(ifs, _password);
		getline(ifs, _id);
		getline(ifs, _device);

		_vendor = _vendor.substr(_vendor.find(":") + 1);
		_password = _password.substr(_password.find(":") + 1);
		_id = _id.substr(_id.find(":") + 1);
		_device = _device.substr(_device.find(":") + 1);

		wxLogDebug("vendor code: %s", _vendor);
		wxLogDebug("keycode: %s", _password);
		wxLogDebug("vendor id: %s", _id);
		wxLogDebug("device id: %s", _device);
	}

	if (TargetPath.empty()){
		ErrorMessage = ErrorMessage + "target.dat is not selected!\n";
		RadarOpen = false;
	}
#endif

	// Initialize Itron
	Furuno::InitialITRON();

#if __linux
	if (RadarOpen){
		Furuno::FecError error = Furuno::InitError;
#if FLM_COMPILE_NEW_LICENSE == 1
		if (_vendor[_vendor.size()-1] == '\r') _vendor.erase(_vendor.size()-1);
		if (_password[_password.size()-1] == '\r') _password.erase(_password.size()-1);
		if (_id[_id.size()-1] == '\r') _id.erase(_id.size()-1);
		if (_device[_device.size()-1] == '\r') _device.erase(_device.size()-1);

		// License & Target License
		setenv("FEC_NAVNETDLL_TARGET", TargetPath.c_str(), 1);

		// Initialize LastError of NavNetLibrary API
		Furuno::LeClearLastAPIError();

		error = Furuno::Initialize((Furuno::UINT8 *)_vendor.c_str(),
			(Furuno::UINT8 *)_password.c_str(),
			(Furuno::UINT8 *)_id.c_str(),
			(Furuno::UINT8 *)_device.c_str(),
			g_ip_addr);

		// Get LastError of NavNetLibrary API
		lastError = Furuno::LeGetLastError(Furuno::ERRORTYPE_API);

		if (error != Furuno::None)
		{
			ErrorMessage = ErrorMessage + "Flm_certification  Failed!\n";
			wxLogDebug("Furuno::Initialize error : %d", error);
			wxLogDebug("Error = %x", lastError);
			RadarOpen = false;
		}	
#endif //FLM_COMPILE_NEW_LICENSE
	}
#endif //__linux

	if (RadarOpen)
	{
		Radar *frame2 = new Radar(NULL);
		frame2->Show(true);
		frame2->DoStartThread();
		Close();
	}
	else
	{
		ErrorMessage = ErrorMessage + "\nPlease exit and retry.\n";
		wxString wx_ErrorMessage(ErrorMessage);
		wxLogError(wx_ErrorMessage);
		Close();
	}
}
