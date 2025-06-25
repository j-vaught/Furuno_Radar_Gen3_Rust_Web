// RadarConnection.cpp

#include "RadarConnection.h"       // Header for Radar class definition
#include "Api.h"                   // Header for API functions and definitions
#include "RadarEchoFree.h"         // Header for RadarEcho class
#include "SensorDataForm.h"        // Header for SensorDataForm class
#include "RadarCommand.h"          // Header for RadarCommand class
#include <stdlib.h>                // Standard library for general utilities
#include <iostream>                // Include for std::cout, used for console output

// Define custom wxWidgets events for thread communication
wxDEFINE_EVENT(wxEVT_COMMAND_MYTHREAD_UPDATE, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_COMMAND_MYTHREAD_RECEIVERADAR, wxThreadEvent);

// Define custom event IDs for buttons to avoid conflicts with predefined IDs
enum {
    BUTTON1_ID = wxID_HIGHEST + 1, // Unique ID for Button 1 ("View Echo")
    BUTTON2_ID,                    // Unique ID for Button 2 ("View ARPA")
    BUTTON3_ID                     // Unique ID for Button 3 ("Command Tool")
};

///////////////////////////////////////////////////////////////////////////////

// Constructor for the Radar class, which is a wxFrame (a top-level window)
Radar::Radar(wxWindow *parent, wxWindowID id, const wxString &title, 
             const wxPoint &pos, const wxSize &size, long style)
    : wxFrame(parent, id, title, pos, size, style), // Initialize the base wxFrame
      webServer_(8080, this)  // Initialize the web server on port 8080, passing 'this' as handler
{
    // Set minimum and maximum size hints for the window (using default sizes)
    this->SetSizeHints(wxDefaultSize, wxDefaultSize);

    // Create a vertical box sizer to manage layout vertically
    wxBoxSizer *bSizer1;
    bSizer1 = new wxBoxSizer(wxVERTICAL);

    // Create a multi-line text control for displaying text output
    m_textCtrl1 = new wxTextCtrl(this, wxID_ANY, wxEmptyString, 
                                 wxDefaultPosition, wxSize(-1, 180), wxTE_MULTILINE);
    // Add the text control to the vertical sizer with border and expansion
    bSizer1->Add(m_textCtrl1, 0, wxALL | wxEXPAND, 5);

    // Create a horizontal box sizer for arranging buttons horizontally
    wxBoxSizer *bSizer2;
    bSizer2 = new wxBoxSizer(wxHORIZONTAL);

    // Create Button 1 ("View Echo") with its unique ID
    m_button1 = new wxButton(this, BUTTON1_ID, wxT("View Echo"), 
                             wxDefaultPosition, wxDefaultSize, 0);
    // Add Button 1 to the horizontal sizer with border and vertical alignment
    bSizer2->Add(m_button1, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    // Create Button 2 ("View ARPA") with its unique ID
    m_button2 = new wxButton(this, BUTTON2_ID, wxT("View ARPA"), 
                             wxDefaultPosition, wxDefaultSize, 0);
    // Add Button 2 to the horizontal sizer with border and vertical alignment
    bSizer2->Add(m_button2, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    // Add a stretchable space to push the next button to the right
    bSizer2->Add(0, 0, 1, wxEXPAND, 5);

    // Create Button 3 ("Command Tool") with its unique ID
    m_button3 = new wxButton(this, BUTTON3_ID, wxT("Command Tool"), 
                             wxDefaultPosition, wxDefaultSize, 0);
    // Add Button 3 to the horizontal sizer with border and vertical alignment
    bSizer2->Add(m_button3, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    // Add the horizontal sizer containing buttons to the main vertical sizer
    bSizer1->Add(bSizer2, 1, wxEXPAND, 5);

    // Set the main sizer for the frame and perform layout
    this->SetSizer(bSizer1);
    this->Layout();

    // Center the window on the screen both horizontally and vertically
    this->Centre(wxBOTH);

    // Bind button click events to their respective handler methods using custom IDs
    Bind(wxEVT_COMMAND_BUTTON_CLICKED, &Radar::m_button1OnButtonClick, this, BUTTON1_ID);
    Bind(wxEVT_COMMAND_BUTTON_CLICKED, &Radar::m_button2OnButtonClick, this, BUTTON2_ID);
    Bind(wxEVT_COMMAND_BUTTON_CLICKED, &Radar::m_button3OnButtonClick, this, BUTTON3_ID);

    // Bind custom thread events to handler methods
    Bind(wxEVT_COMMAND_MYTHREAD_RECEIVERADAR, &Radar::EnableAndDisableButton, this);
    Bind(wxEVT_COMMAND_MYTHREAD_UPDATE, &Radar::OnThreadUpdate, this);

    // Bind the window close event to the OnClose handler
    Bind(wxEVT_CLOSE_WINDOW, &Radar::OnClose, this);

    // Initially disable all buttons until certain conditions are met
    DisableButton();

    // Start the embedded web server
    webServer_.start();

    // Set a callback for the web server to retrieve output from the text control
    webServer_.setOutputGetter([this]() {
        wxCriticalSectionLocker locker(m_pThreadCS); // Ensure thread safety
        wxString output = m_textCtrl1->GetValue();    // Get current text
        return std::string(output.mb_str());          // Convert to std::string
    });
}

// Destructor for the Radar class, responsible for cleanup
Radar::~Radar()
{
    // Unbind all previously bound events to prevent dangling references
    m_button1->Unbind(wxEVT_COMMAND_BUTTON_CLICKED, &Radar::m_button1OnButtonClick, this);
    m_button2->Unbind(wxEVT_COMMAND_BUTTON_CLICKED, &Radar::m_button2OnButtonClick, this);
    m_button3->Unbind(wxEVT_COMMAND_BUTTON_CLICKED, &Radar::m_button3OnButtonClick, this);
    Unbind(wxEVT_COMMAND_MYTHREAD_RECEIVERADAR, &Radar::EnableAndDisableButton, this);
    Unbind(wxEVT_COMMAND_MYTHREAD_UPDATE, &Radar::OnThreadUpdate, this);
    Unbind(wxEVT_CLOSE_WINDOW, &Radar::OnClose, this);

    // **Do NOT manually delete the controls**
    // The wxWidgets framework manages the lifetime of child controls
    // delete m_button1;
    // delete m_button2;
    // delete m_button3;
    // delete m_textCtrl1;

    // Stop the embedded web server gracefully
    webServer_.stop();
}

// Event handler for Button 1 ("View Echo") click event
void Radar::m_button1OnButtonClick(wxCommandEvent &event)
{
    // Output a debug message to the console indicating the button was clicked
    std::cout << "Radar: m_button1OnButtonClick called." << std::endl;

    // Create a new RadarEcho window (frame3) without a parent
    RadarEcho *frame3 = new RadarEcho(NULL);

    // Show the RadarEcho window
    frame3->Show(true);

    // Start a thread associated with the RadarEcho window for background processing
    frame3->DoStartThread();
}

// Event handler for Button 2 ("View ARPA") click event
void Radar::m_button2OnButtonClick(wxCommandEvent &event)
{
    // Output a debug message to the console indicating the button was clicked
    std::cout << "Radar: m_button2OnButtonClick called." << std::endl;

    // Create a new SensorDataForm window (frame4) without a parent
    SensorDataForm *frame4 = new SensorDataForm(NULL);

    // Show the SensorDataForm window
    frame4->Show(true);

    // Initialize sensor data within the SensorDataForm window
    frame4->InitialSensorDataForm();

    // Start a thread associated with the SensorDataForm window for background processing
    frame4->DoStartThread();
}

// Event handler for Button 3 ("Command Tool") click event
void Radar::m_button3OnButtonClick(wxCommandEvent &event)
{
    // Output a debug message to the console indicating the button was clicked
    std::cout << "Radar: m_button3OnButtonClick called." << std::endl;

    // Create a new RadarCommand window (frame5) without a parent
    RadarCommand *frame5 = new RadarCommand(NULL);

    // Show the RadarCommand window
    frame5->Show(true);

    // Initialize radar commands within the RadarCommand window
    frame5->InitialRadarCommand();
}

// Method to disable all three buttons, preventing user interaction
void Radar::DisableButton()
{
    m_button1->Disable(); // Disable Button 1 ("View Echo")
    m_button2->Disable(); // Disable Button 2 ("View ARPA")
    m_button3->Disable(); // Disable Button 3 ("Command Tool")
}

// Method to start a background thread associated with the Radar class
void Radar::DoStartThread()
{
    // Create a new RadarThread object, passing 'this' as the handler
    m_pThread = new RadarThread(this);

    // Attempt to run the thread
    if (m_pThread->Run() != wxTHREAD_NO_ERROR)
    {
        // If thread creation fails, log an error message
        wxLogError("Can't start thread!");

        // Clean up by deleting the thread object
        delete m_pThread;
        m_pThread = NULL;
    }
}

// Event handler for thread update events, appending received text to the text control
void Radar::OnThreadUpdate(wxThreadEvent &event)
{
    this->m_textCtrl1->AppendText(event.GetString());
}

// Event handler to enable or disable buttons based on radar connection status
void Radar::EnableAndDisableButton(wxThreadEvent &event)
{
    int connection_status = event.GetInt(); // Retrieve connection status from the event

    if (connection_status == 0)
    {
        // If connection is successful (status 0), enable all buttons
        m_button1->Enable();
        m_button2->Enable();
        m_button3->Enable();

        // Open the RadarEcho window and start its associated thread
        RadarEcho *frame3 = new RadarEcho(NULL);
        frame3->Show(true);
        frame3->DoStartThread();

        // Optionally, open RadarCommand window if needed (commented out)
        // RadarCommand *frame5 = new RadarCommand(NULL);
        // frame5->Show(true);
        // frame5->InitialRadarCommand();
    }
    else if (connection_status == -1)
    {
        // If connection is lost or invalid (status -1), disable all buttons
        DisableButton();
    }
    else
    {
        // Handle other connection statuses if necessary (currently empty)
    }
}

// Implementation of the RadarThread's entry point, running in a separate thread
wxThread::ExitCode RadarThread::Entry()
{
    // Create two custom thread events for updates and radar reception
    wxThreadEvent *event = new wxThreadEvent(wxEVT_COMMAND_MYTHREAD_UPDATE);
    wxThreadEvent *ReceiveRadarevent = new wxThreadEvent(wxEVT_COMMAND_MYTHREAD_RECEIVERADAR);

    // Main loop of the thread, running until the thread is requested to terminate
    while (!TestDestroy())
    {
        // Initialize connection status variables
        Furuno::INT32 connectionStatus0 = 0;
        Furuno::INT32 connectionStatus1 = 0;
        Furuno::INT32 ret = RADAR_NG;
        static Furuno::INT32 connectionStatus_previous0 = 0;
        static Furuno::INT32 connectionStatus_previous1 = 0;

        // Send a message indicating network initialization
        event->SetString("Init network...");
        wxQueueEvent(m_pHandler, event->Clone());

        // Attempt to start the network with specified parameters
        if (StartNetwork("RDLAPP", g_ip_addr, g_subnet_mask))
        {
            // If network initialization is successful, notify the user
            event->SetString("Done\n");
            wxQueueEvent(m_pHandler, event->Clone());
        }
        else
        {
            // If network initialization fails, log an error and exit the loop
            wxLogError("Failed to initialize network!\nPlease exit and retry.");
            break;
        }

#if FLM_COMPILE_NEW_LICENSE == 1
        // If compiling with new license, send separator lines and handle sensor numbers
        event->SetString("\t\t--------------------------------------------------------\n");
        wxQueueEvent(m_pHandler, event->Clone());

        P_SensorNo(event, m_pHandler); // Handle sensor numbers (function implementation not shown)

        event->SetString("\t\t--------------------------------------------------------\n");
        wxQueueEvent(m_pHandler, event->Clone());
#endif

        // Initialize the NMEA0183 packet transmission module
        event->SetString("Init NMEA0183 Packet transmission module...");
        wxQueueEvent(m_pHandler, event->Clone());
        Furuno::NcInitialNmea183Server(); // Initialize NMEA server
        event->SetString("Done\n");
        wxQueueEvent(m_pHandler, event->Clone());

        // Initialize the radar module
        event->SetString("Init radar module...");
        wxQueueEvent(m_pHandler, event->Clone());
        if (StartRadarModule())
        {
            // If radar module initialization is successful, notify the user
            event->SetString("Done\n");
            wxQueueEvent(m_pHandler, event->Clone());
        }
        else
        {
            // If radar module initialization fails, log an error and exit the loop
            wxLogError("Failed to initialize radar module!\nPlease exit and retry.");
            break;
        }

        // Initialize the ARPA module
        event->SetString("Init ARPA module...");
        wxQueueEvent(m_pHandler, event->Clone());
        if (Furuno::RmaInitialArpaModule() == 0)
        {
            // If ARPA module initialization is successful, notify the user
            event->SetString("Done\n");
        }
        else
        {
            // If ARPA module initialization fails, notify the user
            event->SetString("false\n");
        }
        wxQueueEvent(m_pHandler, event->Clone());

#ifndef USE_PRIVATE_DEBUGMODE
        // Attempt to find and validate the radar connection
        RadarError radar_error = FindRadar(this, event, m_pHandler);

        if (radar_error == RadarFound)
        {
            // If radar is found, wait briefly before entering the monitoring loop
            wxThread::Sleep(4500);
            for (;;)
            {
                if (!g_selected_dualrange)
                {
                    // If not in dual-range mode, check connection status for radar 0
                    ret = Furuno::RmGetConnectionEx(&connectionStatus0, 0);

                    if (connectionStatus_previous0 != connectionStatus0)
                    {
                        // Log the change in connection status
                        wxLogDebug("connectionStatus_previous == %d, connectionStatus == %d", 
                                   connectionStatus_previous0, connectionStatus0);
                        if (connectionStatus_previous0 == 0 && connectionStatus0 == 1 && (ret == RADAR_OK))
                        {
                            // Connection established
                            event->SetString("Connected! ");
                            wxQueueEvent(m_pHandler, event->Clone());
                            ReceiveRadarevent->SetInt(0); // Status 0 indicates connected
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else if (connectionStatus_previous0 == 1 && connectionStatus0 == 2 && (ret == RADAR_OK))
                        {
                            // Connection lost
                            wxLogError("Disconnected! ");
                            ReceiveRadarevent->SetInt(-1); // Status -1 indicates disconnected
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else if (connectionStatus_previous0 == 2 && connectionStatus0 == 1 && (ret == RADAR_OK))
                        {
                            // Reconnected after disconnection
                            wxLogError("Reconnected! ");
                            ReceiveRadarevent->SetInt(0); // Status 0 indicates connected
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else if ((connectionStatus_previous0 == 1 && connectionStatus0 == 0) || 
                                 (connectionStatus_previous0 == 0 && connectionStatus0 == 2) || 
                                 (connectionStatus_previous0 == 2 && connectionStatus0 == 0))
                        {
                            // Handle unexpected connection status transitions
                            wxLogError("An impossible situation occurred! ");
                            ReceiveRadarevent->SetInt(0); // Default to connected
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else
                        {
                            // Handle other status transitions if necessary
                        }
                        // Update the previous connection status
                        connectionStatus_previous0 = connectionStatus0;
                    }
                }
                else
                {
                    // If in dual-range mode, check connection statuses for both radars 0 and 1
                    ret = Furuno::RmGetConnectionEx(&connectionStatus0, 0);
                    ret = Furuno::RmGetConnectionEx(&connectionStatus1, 1);

                    // Check and handle connection status changes for radar 0
                    if (connectionStatus_previous0 != connectionStatus0)
                    {
                        wxLogDebug("connectionStatus_previous0 == %d, connectionStatus0 == %d", 
                                   connectionStatus_previous0, connectionStatus0);
                        if (connectionStatus_previous0 == 0 && connectionStatus0 == 1 && (ret == RADAR_OK))
                        {
                            // Radar 0 connected
                            event->SetString("Connected!(radarno 0) ");
                            wxQueueEvent(m_pHandler, event->Clone());
                            ReceiveRadarevent->SetInt(0);
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else if (connectionStatus_previous0 == 1 && connectionStatus0 == 2 && (ret == RADAR_OK))
                        {
                            // Radar 0 disconnected
                            wxLogError("Disconnected!(radarno 0) ");
                            ReceiveRadarevent->SetInt(-1);
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else if (connectionStatus_previous0 == 2 && connectionStatus0 == 1 && (ret == RADAR_OK))
                        {
                            // Radar 0 reconnected
                            wxLogError("Reconnected!(radarno 0) ");
                            ReceiveRadarevent->SetInt(0);
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else if ((connectionStatus_previous0 == 1 && connectionStatus0 == 0) || 
                                 (connectionStatus_previous0 == 0 && connectionStatus0 == 2) || 
                                 (connectionStatus_previous0 == 2 && connectionStatus0 == 0))
                        {
                            // Handle unexpected connection status transitions for radar 0
                            wxLogError("An impossible situation occurred!(radarno 0) ");
                            ReceiveRadarevent->SetInt(0);
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else
                        {
                            // Handle other status transitions if necessary
                        }
                        // Update the previous connection status for radar 0
                        connectionStatus_previous0 = connectionStatus0;
                    }

                    // Check and handle connection status changes for radar 1
                    if (connectionStatus_previous1 != connectionStatus1)
                    {
                        wxLogDebug("connectionStatus_previous1 == %d, connectionStatus1 == %d", 
                                   connectionStatus_previous1, connectionStatus1);
                        if (connectionStatus_previous1 == 0 && connectionStatus1 == 1 && (ret == RADAR_OK))
                        {
                            // Radar 1 connected
                            event->SetString("Connected!(radarno 1) ");
                            wxQueueEvent(m_pHandler, event->Clone());
                            ReceiveRadarevent->SetInt(0);
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else if (connectionStatus_previous1 == 1 && connectionStatus1 == 2 && (ret == RADAR_OK))
                        {
                            // Radar 1 disconnected
                            wxLogError("Disconnected!(radarno 1) ");
                            ReceiveRadarevent->SetInt(-1);
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else if (connectionStatus_previous1 == 2 && connectionStatus1 == 1 && (ret == RADAR_OK))
                        {
                            // Radar 1 reconnected
                            wxLogError("Reconnected!(radarno 1) ");
                            ReceiveRadarevent->SetInt(0);
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else if ((connectionStatus_previous1 == 1 && connectionStatus1 == 0) || 
                                 (connectionStatus_previous1 == 0 && connectionStatus1 == 2) || 
                                 (connectionStatus_previous1 == 2 && connectionStatus1 == 0))
                        {
                            // Handle unexpected connection status transitions for radar 1
                            wxLogError("An impossible situation occurred!(radarno 1) ");
                            ReceiveRadarevent->SetInt(0);
                            wxQueueEvent(m_pHandler, ReceiveRadarevent->Clone());
                        }
                        else
                        {
                            // Handle other status transitions if necessary
                        }
                        // Update the previous connection status for radar 1
                        connectionStatus_previous1 = connectionStatus1;
                    }
                }

                // Check if the thread has been requested to terminate
                if (this->TestDestroy())
                {
                    break; // Exit the loop if termination is requested
                }

                // Sleep for 1 second before the next iteration to reduce CPU usage
                wxThread::Sleep(1000);
            }
        }
        else if (radar_error == NoRadar)
        {
            // If no radar is found, notify the user and exit the loop
            event->SetString("No radar\n");
            wxQueueEvent(m_pHandler, event->Clone());
            event->SetString("Please check your connection to the radar, exit and retry.\n");
            wxQueueEvent(m_pHandler, event->Clone());
            break;
        }
        else if (radar_error == AbnormalReturnValue)
        {
            // If an abnormal return value is encountered, notify the user and exit the loop
            event->SetString("Return values are abnormal.\n");
            wxQueueEvent(m_pHandler, event->Clone());
            event->SetString("Please check a log, exit and retry.\n");
            wxQueueEvent(m_pHandler, event->Clone());
            break;
        }
        else
        {
            // Handle other radar errors if necessary
            break;
        }
#endif //! USE_PRIVATE_DEBUGMODE
    }

    // Clean up dynamically allocated thread events to prevent memory leaks
    delete event;
    delete ReceiveRadarevent;

    // Return success exit code
    return (wxThread::ExitCode)0;
}

// Destructor for RadarThread class, responsible for cleaning up thread resources
RadarThread::~RadarThread()
{
    // Lock the critical section to safely modify shared data
    wxCriticalSectionLocker enter(m_pHandler->m_pThreadCS);

    // Set the thread pointer in the handler to NULL, indicating no active thread
    m_pHandler->m_pThread = NULL;
}

// Event handler for the window close event
void Radar::OnClose(wxCloseEvent &event)
{
    {
        // Lock the critical section to safely access thread-related data
        wxCriticalSectionLocker enter(m_pThreadCS);

        if (m_pThread)
        {
            // If a thread is running, log a debug message indicating thread deletion
            wxMessageOutputDebug().Printf("MYFRAME: deleting thread");

            // Attempt to delete the thread gracefully
            if (m_pThread->Delete() != wxTHREAD_NO_ERROR)
                wxLogError("Can't delete the thread!"); // Log an error if deletion fails
        }
    }

    // Wait until the thread has been safely deleted
    for (;;)
    {
        {
            // Lock the critical section to check the thread pointer
            wxCriticalSectionLocker enter(m_pThreadCS);
            if (!m_pThread)
                break; // Exit the loop if no thread is active
        }

        // Sleep for 1 millisecond before checking again to prevent busy-waiting
        wxThread::This()->Sleep(1);
    }

    // Destroy the window, effectively closing the application
    Destroy();
}
