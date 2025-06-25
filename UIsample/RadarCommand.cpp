/**
 * @file   RadarCommand.cpp
 *
 * @brief
 * Window"RadarCommand" of DllSample5
 *
 */

#include "RadarConnection.h"
#include "Api.h"
#include "RadarCommand.h"
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>


//------------------------------------------------
//  Global variable definition
//------------------------------------------------
wxString g_wx_SelectedIndex;

///////////////////////////////////////////////////////////////////////////

RadarCommand::RadarCommand(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style)
{
    this->SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer* bSizer1;
    bSizer1 = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* bSizer2;
    bSizer2 = new wxBoxSizer(wxHORIZONTAL);

    m_staticText1 = new wxStaticText(this, wxID_ANY, wxT("Radar Number"), 
                                    wxDefaultPosition, wxDefaultSize, 0);
    m_staticText1->Wrap(-1);
    bSizer2->Add(m_staticText1, 0, wxALL, 5);

    m_comboBox1 = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, 
                                 wxSize(-1, 30), 0, NULL, 0);
    bSizer2->Add(m_comboBox1, 0, wxALL, 5);

    m_button1 = new wxButton(this, wxID_ANY, wxT("Get all"), wxDefaultPosition, 
                             wxSize(-1, 30), 0);
    bSizer2->Add(m_button1, 0, wxALL, 5);

    m_button2 = new wxButton(this, wxID_ANY, wxT("Clear"), wxDefaultPosition, 
                             wxSize(-1, 30), 0);
    bSizer2->Add(m_button2, 0, wxALL, 5);

    bSizer1->Add(bSizer2, 0, wxEXPAND, 5);

    m_grid1 = new wxGrid(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 500), 0);

    // Grid setup code
    m_grid1->CreateGrid(NUM_OF_RadarFunctions, 14);
    m_grid1->EnableEditing(true);
    m_grid1->EnableGridLines(true);
    m_grid1->EnableDragGridSize(false);
    m_grid1->SetMargins(0, 0);

    m_grid1->EnableDragColMove(false);
    m_grid1->EnableDragColSize(true);
    m_grid1->SetColLabelSize(30);
    m_grid1->SetColLabelValue(0, wxT("No"));
    m_grid1->SetColLabelValue(1, wxT("Name"));
    m_grid1->SetColLabelValue(2, wxEmptyString);
    m_grid1->SetColLabelValue(3, wxEmptyString);
    m_grid1->SetColLabelValue(4, wxT("Param1"));
    m_grid1->SetColLabelValue(5, wxT("Param2"));
    m_grid1->SetColLabelValue(6, wxT("Param3"));
    m_grid1->SetColLabelValue(7, wxT("Param4"));
    m_grid1->SetColLabelValue(8, wxT("Param5"));
    m_grid1->SetColLabelValue(9, wxT("Param6"));
    m_grid1->SetColLabelValue(10, wxT("Param7"));
    m_grid1->SetColLabelValue(11, wxT("Param8"));
    m_grid1->SetColLabelValue(12, wxT("Param9"));
    m_grid1->SetColLabelValue(13, wxT("Param10"));

    m_grid1->EnableDragRowSize(true);
    m_grid1->SetRowLabelAlignment(wxALIGN_CENTER, wxALIGN_CENTER);
    m_grid1->SetDefaultCellAlignment(wxALIGN_LEFT, wxALIGN_TOP);
    bSizer1->Add(m_grid1, 1, wxEXPAND | wxALL, 5);

    // Initialize the information pane
    m_infoText = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, 
                                wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
    m_infoText->SetMinSize(wxSize(-1, 200)); // Set a minimum height
    m_infoText->SetValue("Select a command or parameter to see more information here.");
    bSizer1->Add(m_infoText, 0, wxEXPAND | wxALL, 5);

    this->SetSizerAndFit(bSizer1);
    this->Layout();

    this->Centre(wxBOTH);

    // Event bindings
    Bind(wxEVT_GRID_CELL_CHANGING, &RadarCommand::OnCellValueChanging, this);

    m_button1->Bind(wxEVT_BUTTON, &RadarCommand::m_button1OnButtonClick, this);
    m_button2->Bind(wxEVT_BUTTON, &RadarCommand::m_button2OnButtonClick, this);
    m_comboBox1->Bind(wxEVT_COMBOBOX, &RadarCommand::m_comboBox1OnCombobox, this);
    m_grid1->Bind(wxEVT_GRID_CELL_LEFT_CLICK, &RadarCommand::m_grid1OnGridCellLeftClick, this);

    // Populate command descriptions
    PopulateCommandDescriptions();

    // Bind the grid cell selection event
    m_grid1->Bind(wxEVT_GRID_SELECT_CELL, &RadarCommand::OnGridCellSelected, this);
// Instantiate CommandServer
commandServer_ = std::make_unique<CommandServer>(serverPort_);

// Set the command handler
commandServer_->setCommandHandler(
    [this](const std::string& action, const std::map<std::string, std::string>& params) -> std::string {
        std::lock_guard<std::mutex> lock(commandMutex_);
        try {
            // Extract radar number
            auto radarNoIt = params.find("radarNo");
            if (radarNoIt == params.end()) {
                return "Error: Missing 'radarNo' parameter.";
            }
            std::string radarNoStr = radarNoIt->second;
            int radarNo = std::stoi(radarNoStr);

            // Extract command name for 'set' and 'get' actions
            std::string commandName;
            if (action == "set" || action == "get") {
                auto cmdIt = params.find("command");
                if (cmdIt == params.end()) {
                    return "Error: Missing 'command' parameter.";
                }
                commandName = cmdIt->second;
            }

            // Handle different actions
            if (action == "set") {
                // Find command index
                int index = getCommandIndex(commandName);
                if (index == -1) {
                    return "Error: Command not found.";
                }

                // Set parameters in the grid
                for (const auto& [paramName, paramValue] : params) {
                    if (paramName.find("Param") != std::string::npos) {
                        int paramIndex = std::stoi(paramName.substr(5)) - 1; // Param1 -> 0
                        // Set the grid cell value for the parameter using wxCallAfter
                        CallAfter([this, index, paramIndex, paramValue, radarNo, commandName]() {
                            m_grid1->SetCellValue(index, 4 + paramIndex, paramValue);
                            SetFunction(radarNo, commandName, index);
                        });
                    }
                }

                return "Set command executed successfully.";
            }
            else if (action == "get") {
                // Find command index
                int index = getCommandIndex(commandName);
                if (index == -1) {
                    return "Error: Command not found.";
                }

                // Execute the Get Function using wxCallAfter
                CallAfter([this, radarNo, commandName, index]() {
                    GetFunction(radarNo, commandName, index, false);
                });

                return "Get command executed successfully.";
            }
            else if (action == "getAll") {
                // Execute the Get All Commands using wxCallAfter
                CallAfter([this]() {
                    GetAllCommands();
                });

                return "Get all commands executed successfully.";
            }
            else {
                return "Error: Unknown action.";
            }
        }
        catch (const std::invalid_argument&) {
            return "Error: Invalid parameter values.";
        }
        catch (const std::out_of_range&) {
            return "Error: Missing required parameters.";
        }
        catch (...) {
            return "Error: An unexpected error occurred.";
        }
    }
);

// Start the CommandServer
commandServer_->start();
}

RadarCommand::~RadarCommand()
{
    m_comboBox1->Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, 
                        &RadarCommand::m_comboBox1OnCombobox, this);
    m_button1->Unbind(wxEVT_COMMAND_BUTTON_CLICKED, 
                      &RadarCommand::m_button1OnButtonClick, this);
    m_button2->Unbind(wxEVT_COMMAND_BUTTON_CLICKED, 
                      &RadarCommand::m_button2OnButtonClick, this);
    m_grid1->Unbind(wxEVT_GRID_CELL_LEFT_CLICK, 
                    &RadarCommand::m_grid1OnGridCellLeftClick, this);
    Unbind(wxEVT_GRID_CELL_CHANGING, &RadarCommand::OnCellValueChanging, this);
    Unbind(wxEVT_GRID_CELL_CHANGED, &RadarCommand::OnCellValueChanged, this);

    delete m_staticText1;
    delete m_comboBox1;
    delete m_button1;
    delete m_button2;
    delete m_grid1;
    delete m_infoText; // Delete the information pane
	if (commandServer_) {
        commandServer_->stop();
    }
}

void RadarCommand::PopulateCommandDescriptions()
{
    commandDescriptions[FREE_CMD] = 
        "FREE_CMD:\n"
        "Description: Executes a free-form command.\n"
        "Parameters: \n"
        " - Param1: UINT8 value.";

    commandDescriptions[CONNECT] = 
        "CONNECT:\n"
        "Description: Establishes a connection to the radar.\n"
        "Parameters: \n"
        " - COM Port: Specifies the COM port number to connect.";

    commandDescriptions[DISPMODE] = 
        "DISPMODE:\n"
        "Description: Sets the display mode of the radar.\n"
        "Parameters: \n"
        " - Mode ID (UINT16): The display mode identifier.\n"
        " - Sub-mode ID (UINT16): The sub-mode identifier.";

    commandDescriptions[RANGE] = 
        "RANGE:\n"
        "Description: Configures the radar's range settings.\n"
        "Parameters: \n"
        " - Range Value 1 (UINT16)\n"
        " - Range Value 2 (UINT16)\n"
        " - Range Value 3 (UINT16).";

    commandDescriptions[GAIN] = 
        "GAIN:\n"
        "Description: Adjusts the radar gain settings.\n"
        "Parameters: \n"
        " - Gain Level (INT32)\n"
        " - Gain Parameter 1 (UINT16)\n"
        " - Gain Parameter 2 (UINT16)\n"
        " - Gain Parameter 3 (UINT16).";

    // For command "ARPA_TARGET_ALLCLEAR"
    commandDescriptions[ARPA_TARGET_ALLCLEAR] = 
         "Description: Cancels all ARPA targets currently being tracked, including both normal and reference targets." 
    ;
}

void RadarCommand::InitializeCommandParameters()
{
    // For command "FREE_CMD"
    commandParameters["FreeCommand"] = {
        { "Param1", "UINT8", "Specifies the value for the free command." }
    };

// For command "CONNECT"
    commandParameters["Connect"] = {
        { "Param1", "UINT16", "A UINT16 value representing the COM port number. Typical values are between 1 and 65535, with common COM ports being 1 for local and higher numbers for remote connections." }
    };

    // For command "DISPMODE"
    commandParameters["DispMode"] = {
        { "Param1", "UINT16", "Sets how the radar screen is oriented: \n"
                              "0 = Heading Up (Relative to the ship's direction): The ship's heading is at the top. \n"
                              "1 = Heading Up (True North): The ship's heading is still at the top, but objects are placed relative to true north. \n"
                              "2 = Course Up (Ship's movement direction): The ship's course (where it's moving) is at the top. Use Param2 to set the course direction. \n"
                              "3 = North Up (Relative): North is always at the top, with the ship moving relative to its surroundings. \n"
                              "4 = North Up (True Motion): North is always at the top, and the ship moves according to its actual movement. \n"
                              "5 = Course Up (True Motion): The ship's movement direction is at the top, and it moves according to its actual movement. \n"
                              "6 = Stern Up (Back of the ship at the top): The back of the ship is at the top, only available on some radar systems." },
        { "Param2", "UINT16", "Direction in tenths of a degree (0 to 3599). Used only when Param1 is 2 (Course Up). If Param1 is not 2, this is ignored." }
    };

    // For command "RANGE"
    commandParameters["Range"] = {
        { "Param1", "UINT16", "This sets how far the radar can see. Use different range codes like 9 for 6 Nautical Miles or 52 for 50 meters. Options are 0-24, and 50-54 for short range radars. Choose the range that fits your needs." },
        { "Param2", "UINT16", "The unit for regular range codes. 0 = Nautical Miles (NM), 1 = Kilometers (KM), 2 = Statute Miles (SM), 3 = Kiloyards (KYD). This is ignored for short-range codes (like 50 to 54)." },
        { "Param3", "UINT16", "The unit for short-range codes. 0 = Meters (m), 1 = Yards (yd). This is used when you're setting very short ranges (codes 50 to 54). If you're using regular ranges, this is ignored." }
    };

	// For command "GAIN"
    commandParameters["GAIN"] = {
        { "Param1", "INT32", "An INT32 value representing the picture number (0 to RMC_PICTURENO_MAX-1). Enter -2 for temporary[new] data." },
        { "Param2", "UINT16", "A UINT16 value representing the GAIN mode: 0 for manual adjustment, 1 for automatic adjustment. Note: based on this value, certain parameters will be ignored." },
        { "Param3", "UINT16", "A UINT16 value for manual gain sensitivity. This is used only when Param2 (mode) is set to 0 (MANUAL). If mode is AUTO (1), this parameter is ignored. Enter a value between 0 and 100, where 0 means minimum sensitivity and 100 means maximum sensitivity." },
        { "Param4", "UINT16", "A UINT16 value for semi-automatic gain sensitivity. This is used only when Param2 (mode) is set to 1 (AUTO). If mode is MANUAL (0), this parameter is ignored. Enter a value between 0 and 100, where 0 is minimum auto-adjustment and 100 is maximum." }
    };

    // For command "ACSEA"
    commandParameters["ACSEA"] = {
        { "Param1", "INT32", "The picture number for the radar view you're adjusting (0 to RMC_PICTURENO_MAX-1). Use -2 for temporary[new] settings." },
        { "Param2", "UINT16", "The mode for anti-clutter sea adjustment: 0 = Manual, 1 = Auto. In manual mode, Param3 is used. In auto mode, Param4 and Param5 are used." },
        { "Param3", "UINT16", "The manual clutter reduction setting (0% to 100%). Only used when Param2 is set to 0 (Manual). If Auto mode is used, this is ignored." },
        { "Param4", "UINT16", "The automatic clutter reduction setting (0% to 100%). Only used when Param2 is set to 1 (Auto). If Manual mode is used, this is ignored." },
        { "Param5", "UINT16", "The type of automatic mode to use: 0 = Advanced (offshore), 1 = Coastal (near coast), 2 = Channel (narrow waterways)." }
    };

	// For command "ACRAIN"
    commandParameters["ACRAIN"] = {
        { "Param1", "INT32", "The picture number for the radar view you're adjusting (0 to RMC_PICTURENO_MAX-1). Use -2 for temporary[new] settings." },
        { "Param2", "UINT16", "The mode for anti-clutter rain adjustment: 0 = Manual, 1 = Auto. In manual mode, Param3 is used. In auto mode, Param4, Param5, and Param6 are used." },
        { "Param3", "UINT16", "The manual rain clutter reduction setting (0% to 100%). Only used when Param2 is set to 0 (Manual). If Auto mode is used, this is ignored." },
        { "Param4", "UINT16", "The automatic rain clutter reduction setting (0% to 100%). Only used when Param2 is set to 1 (Auto). If Manual mode is used, this is ignored." },
        { "Param5", "UINT16", "FTC Mode (Fast Time Constant) for filtering rain clutter: 0 = OFF, 1 = FTC-1, 2 = FTC-2. \n"
                               "// FTC-1 is basic filtering for moderate rain, while FTC-2 is stronger filtering for heavier rain." },
        { "Param6", "UINT16", "The type of automatic mode to use based on rain conditions: 0 = Calm, 1 = Moderate, 2 = Rough." }
    };
	
    // For command "CUSTOMPICTURE_ALL"
    commandParameters[CUSTOMPICTURE_ALL] = {
        { "Param1", "INT32", "The picture number to set custom data for (0 to RMC_PICTURENO_MAX-1). This selects which radar display you want to change." },
        { "Param2", "INT16[]", "An array of function data that controls different aspects of the radar picture. Each index in the array represents a function number and its respective setting. Available function numbers and values are:\n"
            " 0: INT REJECT - 0 (OFF), 1, 2, 3\n"
            " 1: ECHO STRETCH - 0 (OFF), 1, 2, 3\n"
            " 2: ECHO AVERAGE - 0 (OFF), 1, 2, 3\n"
            " 3: NOISE REJECT - 0 (OFF), 1 (ON)\n"
            " 4: AUTO SEA - 0 (OFF), 1 (ON)\n"
            " 5: AUTO RAIN - 0 (OFF), 1, 2, 3, 4\n"
            " 6: VIDEO CONTRAST - 0-15 (4 and 5 not used)\n"
            " 7: Pulse Width (≤ 0.5nm) - 0 (S1), 1 (S2)\n"
            " 8: Pulse Width (0.75nm) - 0 (S1), 1 (S2), 2 (M1)\n"
            " 9: Pulse Width (1.5nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2)\n"
            "10: Pulse Width (3nm) - 0 (S2), 1 (M1), 2 (M2), 3 (M3), 4 (S1)\n"
            "11: Pulse Width (6nm) - 0 (M1), 1 (M2), 2 (M3), 3 (L)\n"
            "12: Pulse Width (12nm) - 3 (M1), 0 (M2), 1 (M3), 2 (L)\n"
            "13: SEA CONDITION - 1 to 5\n"
            "14: STC ANT HEIGHT - 0 (5m), 1 (7.5m), 2 (10m), ... 10 (50m or more)\n"
            "15: LOW LEVEL ECHO - 0 to 8\n"
            "16: Pulse Width (S-band, 0.75nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "17: Pulse Width (S-band, 1.5nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "18: Pulse Width (S-band, 3nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "19: Pulse Width (S-band, 6nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "20: Pulse Width (S-band, 12nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "21: Wiper - 0 (OFF), 1, 2\n"
            "22: S/C RATIO - -10 to 10\n"
            "23: NEAR STC - 0 (2), 1 (2.5), 2 (3), 3 (3.5), 4 (4.2)\n"
            "24: LOW LEVEL ECHO - 0 to 8\n"
            "25: TT ECHO LEVEL - 0 to 31\n"
            "26: Pulse Width (> 24nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "27: Target Analyzer - 0 (OFF), 1 (ON)\n"
            "28: Hatching - 0 (OFF), 1 (ON)\n"
            "29: Echo Color Adjustment Level - 0 to 30\n"
            "30: Near-field STC Inflection Point - 1 to 50 (10x value)\n"
            "31: Near-field STC Multiplier - 20 to 90 (10x value)\n"
            "32: Wanted Wave Removal - 0 (OFF), 1 (ON)\n"
        }
    };

	// For command "CUSTOMPICTURE"
    commandParameters[CUSTOMPICTURE] = {
        { "Param1", "INT32", "The picture number to adjust (0 to RMC_PICTURENO_MAX-1). Picture 0 preserves the current custom settings, and pictures 1 to 11 allow you to save custom settings for future use." },
        { "Param2", "INT32", "The function number to adjust. Available functions are:\n"
            " 0: INT REJECT - 0 (OFF), 1, 2, 3\n"
            " 1: ECHO STRETCH - 0 (OFF), 1, 2, 3\n"
            " 2: ECHO AVERAGE - 0 (OFF), 1, 2, 3\n"
            " 3: NOISE REJECT - 0 (OFF), 1 (ON)\n"
            " 4: AUTO SEA - 0 (OFF), 1 (ON)\n"
            " 5: AUTO RAIN - 0 (OFF), 1, 2, 3, 4\n"
            " 6: VIDEO CONTRAST - 0-15 (4 and 5 not used)\n"
            " 7: Pulse Width (≤ 0.5nm) - 0 (S1), 1 (S2)\n"
            " 8: Pulse Width (0.75nm) - 0 (S1), 1 (S2), 2 (M1)\n"
            " 9: Pulse Width (1.5nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2)\n"
            "10: Pulse Width (3nm) - 0 (S2), 1 (M1), 2 (M2), 3 (M3), 4 (S1)\n"
            "11: Pulse Width (6nm) - 0 (M1), 1 (M2), 2 (M3), 3 (L)\n"
            "12: Pulse Width (12nm) - 3 (M1), 0 (M2), 1 (M3), 2 (L)\n"
            "13: SEA CONDITION - 1 to 5\n"
            "14: STC ANT HEIGHT - 0 (5m), 1 (7.5m), 2 (10m), ... 10 (50m or more)\n"
            "15: LOW LEVEL ECHO - 0 to 8\n"
            "16: Pulse Width (S-band, 0.75nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "17: Pulse Width (S-band, 1.5nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "18: Pulse Width (S-band, 3nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "19: Pulse Width (S-band, 6nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "20: Pulse Width (S-band, 12nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "21: Wiper - 0 (OFF), 1, 2\n"
            "22: S/C RATIO - -10 to 10\n"
            "23: NEAR STC - 0 (2), 1 (2.5), 2 (3), 3 (3.5), 4 (4.2)\n"
            "24: LOW LEVEL ECHO - 0 to 8\n"
            "25: TT ECHO LEVEL - 0 to 31\n"
            "26: Pulse Width (> 24nm) - 0 (S1), 1 (S2), 2 (M1), 3 (M2), 4 (M3), 5 (L)\n"
            "27: Target Analyzer - 0 (OFF), 1 (ON)\n"
            "28: Hatching - 0 (OFF), 1 (ON)\n"
            "29: Echo Color Adjustment Level - 0 to 30\n"
            "30: Near-field STC Inflection Point - 1 to 50 (10x value)\n"
            "31: Near-field STC Multiplier - 20 to 90 (10x value)\n"
            "32: Wanted Wave Removal - 0 (OFF), 1 (ON)\n"
        },
        { "Param3", "INT16", "The value to set for the selected function. This value depends on the function number." }
    };

    // For command "PULSE_WIDTH"
    commandParameters[PULSE_WIDTH] = {
        { "Param1", "INT16", "The pulse width value to set for the radar signal (0 to 7). Pulse width controls how long the radar sends a signal, impacting resolution and detection range." },
        { "Param2", "INT16", "The radar's range setting (0 to RMC_RANGE_MAX-1). This sets how far the radar can detect objects." },
        { "Param3", "UINT16", "The unit of measurement for the range: \n"
            " 0 = Nautical Miles (NM)\n"
            " 1 = Kilometers (KM)\n"
            " 2 = Statute Miles (SM)\n"
            " 3 = Kiloyards (KYD)" },
        { "Param4", "INT32", "The picture number for the radar display (0 to RMC_PICTURENO_MAX-1). Use -2 for temporary[new] settings. When using a DRS radar sensor, set this to 0." }
    };

	// For command "TX_STBY"
    commandParameters[TX_STBY] = {
        { "Param1", "UINT16", "Transmission status: \n"
            " 0 = Pre-heating (warming up)\n"
            " 1 = Standby (STBY, ready but not transmitting)\n"
            " 2 = Transmission (TX, actively transmitting)." },
        { "Param2", "UINT16", "Watchman mode: \n"
            " 0 = OFF (disabled)\n"
            " 1 = ON (enabled, switches between TX and STBY automatically after set times)." },
        { "Param3", "UINT32", "Send time (in seconds). Specifies how long the radar stays in transmission (TX) mode before switching to standby (STBY). Use 0 to keep it in TX indefinitely. This applies when Watchman mode is ON." },
        { "Param4", "UINT32", "Stop time (in seconds). Specifies how long the radar stays in standby (STBY) before switching back to transmission (TX). This applies when Watchman mode is ON." }
    };

    // For command "ARPA_SELECT_TARGET"
    commandParameters[ARPA_SELECT_TARGET] = {
        { "Param1", "INT32", "X-coordinate of the target in 1/10000th of a Nautical Mile (NM) relative to the radar center. Positive values = East, negative values = West (North reference)." },
        { "Param2", "INT32", "Y-coordinate of the target in 1/10000th of a Nautical Mile (NM) relative to the radar center. Positive values = North, negative values = South (North reference)." },
        { "Param3", "INT32", "The ARPA target number to select (1 to ARPA_TARGET_MAX). Each target is identified by its number, which the radar tracks." }
    };

	// For command "ARPA_ACQUIRE_TARGET"
    commandParameters[ARPA_ACQUIRE_TARGET] = {
        { "Param1", "INT32", "X-coordinate of the target in 1/10000th of a Nautical Mile (NM) relative to your ship. Positive values = East, negative values = West (True North reference)." },
        { "Param2", "INT32", "Y-coordinate of the target in 1/10000th of a Nautical Mile (NM) relative to your ship. Positive values = North, negative values = South (True North reference)." },
        { "Param3", "INT32", "Acquisition mode: \n"
            " 0 = Normal Target Acquisition (used for moving targets).\n"
            " 1 = Reference Target Acquisition (used for fixed targets to establish reference speed).\n"
            " For DRS radar sensors, set this parameter to 0 (Normal Target Acquisition)." }
    };

    // For command "ARPA_CANCEL_TARGET"
    commandParameters[ARPA_CANCEL_TARGET] = {
        { "Param1", "INT32", "X-coordinate of the target in 1/10000th of a Nautical Mile (NM) relative to your ship. Positive values = East, negative values = West (True North reference)." },
        { "Param2", "INT32", "Y-coordinate of the target in 1/10000th of a Nautical Mile (NM) relative to your ship. Positive values = North, negative values = South (True North reference)." }
    };

	// For command "ARPA_DISPMODE"
    commandParameters[ARPA_DISPMODE] = {
        { "Param1", "UINT16", "Not in use (reserved for future use)." },
        { "Param2", "UINT16", "Mode for target acquisition: \n"
            " 0 = Manual 100 (full manual acquisition)\n"
            " 1 = Auto 25 (25% automatic acquisitions)\n"
            " 2 = Auto 50 (50% automatic acquisitions)\n"
            " 3 = Auto 75 (75% automatic acquisitions)\n"
            " 4 = Auto 100 (full automatic acquisition)." },
        { "Param3", "UINT16", "Vector display for reference speed: \n"
            " 0 = OFF (no vector displayed)\n"
            " 1 = ON (vector displayed)." },
        { "Param4", "UINT16", "Maximum number of targets for automatic acquisition (1 to ARPA_TARGET_MAX)." },
        { "Param5", "UINT16", "Maximum number of targets for manual acquisition (1 to ARPA_TARGET_MAX)." },
        { "Param6", "UINT16", "Tracking base mode: \n"
            " 0 = Relative (tracks target movement relative to your ship)\n"
            " 1 = True (tracks target movement in relation to fixed geographical points)." }
    };

    // For command "ANTENNA_TYPE"
    commandParameters[ANTENNA_TYPE] = {
        { "Param1", "UINT16", "Antenna type: \n"
            " 0 = 6 (6-foot antenna)\n"
            " 1 = 12 (12-foot antenna)\n"
            " 2 = 25UP (25-foot antenna in up position)\n"
            " 3 = 25DOWN (25-foot antenna in down position)\n"
            " 4 = 50 (50-foot antenna)\n"
            " 5 = 30UP (30-foot antenna in up position)\n"
            " 6 = 30DOWN (30-foot antenna in down position)\n"
            " 7 = 60 (60-foot antenna)\n"
            " 8 = S-SSD (S-band SSD antenna)\n"
            " 9 = X-SSD (X-band SSD antenna)\n"
            " 10 = X-NXT (X-band NXT antenna)." },
        { "Param2", "UINT16", "Antenna position: \n"
            " 0 = FORE (front of ship)\n"
            " 1 = MAIN TOP (main top position)\n"
            " 2 = MAIN 2nd (second main mast position)\n"
            " 3 = MAIN 3rd (third main mast position)\n"
            " 4 = AFT (back of ship)\n"
            " 5 = PORT (left side of ship)\n"
            " 6 = STAR (right side of ship)." },
        { "Param3", "UINT16", "Transmission power in kilowatts (kW)." },
        { "Param4", "UINT16", "Antenna length in centimeters (cm)." },
        { "Param5", "UINT16", "Antenna position: \n"
            " 0 = UP (raised)\n"
            " 1 = DOWN (lowered)." },
        { "Param6", "UINT16", "Antenna type design: \n"
            " 0 = Radome\n"
            " 1 = Open\n"
            " 2 = EG\n"
            " 3 = Other." },
        { "Param7", "UINT16", "Device type: \n"
            " 0 = Magnetron (traditional)\n"
            " 1 = Solid State (modern)." }
    };

    // For command "KEY_CMD"
    commandParameters[KEY_CMD] = {
        { "Param1", "UINT8*", "Pointer to the key command to be sent remotely. The key command triggers a function or action on the radar as if a key was pressed on the radar console." }
    };
// For command "GUARD_STATUS"
    commandParameters[GUARD_STATUS] = {
        { "Param1", "UINT16", "The number of polygon points for the guard zone (1 to RMC_POLYGON_MAX)." },
        { "Param2", "UINT16", "Status of Guard Zone 1 (GZ1): \n"
            " 0 = None (no zone)\n"
            " 1 = Select (zone selected)\n"
            " 2 = Set (zone configured, not active)\n"
            " 3 = Work (zone actively monitoring)\n"
            " 4 = Off (zone turned off)\n"
            " 5 = Out (targets detected outside the zone)." },
        { "Param3", "UINT16", "Status of Guard Zone 2 (GZ2), with the same options as GZ1:\n"
            " 0 = None\n"
            " 1 = Select\n"
            " 2 = Set\n"
            " 3 = Work\n"
            " 4 = Off\n"
            " 5 = Out." }
    };
// For command "LOST_ACK"
    commandParameters[LOST_ACK] = {
        { "Description", "Acknowledges and deletes ARPA targets with a lost status. This command ensures the radar display remains focused on active targets by clearing lost targets from memory." }
    };

    // For command "ARPA_VECTOR"
    commandParameters[ARPA_VECTOR] = {
        { "Param1", "UINT16", "Vector reference mode:\n"
            " 0 = Relative (relative to your ship's movement)\n"
            " 1 = True (referenced to a fixed geographical point)." },
        { "Param2", "UINT16", "Vector time duration:\n"
            " 0 = 30 sec\n"
            " 1 = 1 min\n"
            " 2 = 3 min\n"
            " 3 = 6 min\n"
            " 4 = 12 min\n"
            " 5 = 15 min\n"
            " 6 = 30 min" }
    };
    // For command "ARPA_CPATCPA"
    commandParameters[ARPA_CPATCPA] = {
        { "Param1", "UINT16", "Enable or disable the collision alarm:\n 0 = OFF (no alarm)\n 1 = ON (alarm triggers if CPA/TCPA limits are exceeded)." },
        { "Param2", "UINT16", "Closest Point of Approach (CPA) setting:\n"
            " 0 = OFF\n"
            " 1 = 0.5 NM\n"
            " 2 = 1 NM\n"
            " 3 = 2 NM\n"
            " 4 = 3 NM\n"
            " 5 = 5 NM\n"
            " 6 = 6 NM" },
        { "Param3", "UINT16", "Time to Closest Point of Approach (TCPA) setting:\n"
            " 0 = 30 sec\n"
            " 1 = 1 min\n"
            " 2 = 2 min\n"
            " 3 = 3 min\n"
            " 4 = 4 min\n"
            " 5 = 5 min\n"
            " 6 = 6 min\n"
            " 7 = 12 min" },
        { "Param4", "INT32", "Detailed CPA value in 1/10000 NM for precise monitoring." },
        { "Param5", "INT32", "Detailed TCPA value in seconds (1 to 3600) for precise timing." }
    };
    // For command "TUNE"
    commandParameters[TUNE] = {
        { "Param1", "UINT16", "Tuning mode:\n"
            " 0 = MANUAL (requires manual tuning input)\n"
            " 1 = AUTO (automatic tuning by radar system)." },
        { "Param2", "UINT16", "Manual tuning data (only used in manual mode). Adjust this value to fine-tune the radar signal for optimal reception. The valid range depends on the radar model." }
    };
    // For command "TUNE_INDICATOR"
    commandParameters[TUNE_INDICATOR] = {
        { "Param1", "UINT16", "Indicator value representing the radar's tuning level (0 to 100%). Higher values indicate better tuning." },
        { "Param2", "UINT16", "Tuning status:\n"
            " 0 = OK (no issues with tuning)\n"
            " 1 = NG (tuning error detected)." }
    };
    // For command "BLIND_SECTOR"
    commandParameters[BLIND_SECTOR] = {
        { "Param1", "UINT16", "Status:\n 0 = OFF (disable blind sector)\n 1 = ON (enable blind sector)." },
        { "Param2", "UINT16", "Starting direction of first blind sector (0 to 359 degrees)." },
        { "Param3", "UINT16", "Starting direction of second blind sector (0 to 359 degrees)." },
        { "Param4", "UINT16", "Angular width of the first blind sector from the starting direction (0 to 360 degrees)." },
        { "Param5", "UINT16", "Angular width of the second blind sector from the starting direction (0 to 360 degrees)." }
    };
    // For command "RADAR_POSITION"
    commandParameters[RADAR_POSITION] = {
        { "Param1", "INT32", "Monitor number:\n 0 = MAIN (primary monitor)\n 1 = SUB (secondary monitor)." },
        { "Param2", "INT16", "Distance from the bow in meters (position along the ship's length)." },
        { "Param3", "INT16", "Distance from the port side in meters (position along the ship's width)." }
    };
    // For command "TUNE_ADJUST"
    commandParameters[TUNE_ADJUST] = {
        { "Param1", "UINT16", "Status:\n 0 = OFF (disable tune adjustment)\n 1 = ON (activate tune adjustment to initialize tuning data)." }
    };
    // For command "ARPA_CANCEL_TARGETID"
    commandParameters[ARPA_CANCEL_TARGETID] = {
        { "Param1", "INT32", "ID of the target to be canceled (1 to ARPA_TARGET_MAX)." }
    };
    // For command "REFERENCE_POINT"
    commandParameters[REFERENCE_POINT] = {
        { "Param1", "UINT16", "Select the reference point for radar calculations:\n"
            " 0 = Antenna (data relative to radar antenna location)\n"
            " 1 = Conning (data aligned with the ship's control center)." }
    };
    // For command "ALARM_STATUS"
    commandParameters[ALARM_STATUS] = {
        { "Param1", "UINT16", "General alarm status (bit field):\n"
            " Bit0 = HD (Heading data)\n"
            " Bit1 = BP (Bearing pulse)\n"
            " Bit2 = TRIG (Trigger status)\n"
            " Bit3 = VIDEO (Video status)\n"
            " Bit4 = GYRO (Gyro error)\n"
            " Bit5 = MTR-DRV Unit COM Error\n"
            " Bit6 = PM Unit COM Error\n"
            " Bit7 = RF-Converter Unit COM Error\n"
            " Bit8 = MD Type Error\n"
            " Bit9 = PM Type Error\n"
            " Bit10 = TUNE_GATE_S_STATUS\n"
            " Bit11 = TX_HV\n"
            " Bits12-15 = Reserved" },
        { "Param2", "UINT32", "Motor drive unit alarm status (bit field):\n"
            " Bit0 = ROM error\n"
            " Bit1 = RAM error\n"
            " Bit2 = Circuit temperature error\n"
            " Bit3 = Motor current error\n"
            " Bit4 = Motor voltage error\n"
            " Bit5 = 12V PSU error\n"
            " Bit6 = Hall sensor error\n"
            " Bit7 = Antenna lock error\n"
            " Bit9 = Braking resistor error\n"
            " Bit10 = Overload error\n"
            " Bits8, 11-31 = Reserved" },
        { "Param3", "UINT32", "Power management alarm status (bit field):\n"
            " Bit0 = ROM error\n"
            " Bit1 = RAM error\n"
            " Bit2 = 12V PSU error\n"
            " Bit3 = PLL locking error\n"
            " Bits4-31 = Reserved" },
        { "Param4", "UINT32", "RF converter board alarm status (bit field):\n"
            " Bit0 = ROM error\n"
            " Bit1 = RAM error\n"
            " Bit2 = 6V PSU error\n"
            " Bit3 = 48V PSU error\n"
            " Bit4 = PLL (IF) locking error\n"
            " Bit5 = PLL (RF) locking error\n"
            " Bit6 = RFC output signal error\n"
            " Bit7 = RFC input signal error\n"
            " Bit8 = HPA output signal error\n"
            " Bit9 = HPA peak current error\n"
            " Bit10 = Circuit temperature error\n"
            " Bit11 = Antenna VSWR error\n"
            " Bit12-14 = Number of device failures (0b000 to 0b111)\n"
            " Bits15-31 = Reserved" }
    };

    // For command "HEADING"
    commandParameters[HEADING] = {
        { "Param1", "UINT16", "Communication mode:\n"
            " 0 = AD-10 (Analog/Digital)\n"
            " 1 = Serial (e.g., NMEA 0183)\n"
            " 2 = CAN (Controller Area Network)." },
        { "Param2", "UINT16", "Heading value in tenths of a degree (0 to 3599).\n"
            " Example: 900 = 90.0° (East), 1805 = 180.5° (South)." }
    };
    // For command "SELECT_QVDISPLAY"
    commandParameters[SELECT_QVDISPLAY] = {
        { "Param1", "UINT16", "Quick View (QV) display switch:\n"
            " 0 = OFF (disable QV display mode)\n"
            " 1 = ON (enable QV display mode)." }
    };
    // For command "ATT"
    commandParameters[ATT] = {
        { "Param1", "INT32", "Monitor number:\n 0 = MAIN (primary monitor)\n 1 = SUB (secondary monitor)." },
        { "Param2", "UINT16", "Adjustment mode:\n 0 = AUTO (automatic adjustment)\n 1 = MANUAL (manual adjustment required)." },
        { "Param3", "INT16", "Adjustment value (only used in manual mode):\n"
            " - FAR-21x7 / FAR-15x8 / FAR-3000: 0 - 64\n"
            " - FAR-14x7: 0 - 99\n"
            " - Others: Not used." }
    };
    // For command "HEADING_ADJUST"
    commandParameters[HEADING_ADJUST] = {
        { "Param1", "INT32", "Monitor number:\n 0 = MAIN (primary monitor)." },
        { "Param2", "UINT16", "Heading adjustment value (in 1/10th of a degree):\n"
            " Range: 0 to 3599 (e.g., 90 = 9.0°, 3599 = 359.9°)." },
        { "Param3", "INT16", "Fine offset adjustment (in 1/10th of a degree). Typically set to 0 for no offset." }
    };
    // For command "TIMING_ADJUST"
    commandParameters[TIMING_ADJUST] = {
        { "Param1", "INT32", "Monitor number:\n 0 = MAIN (primary monitor)\n 1 = SUB (secondary monitor)." },
        { "Param2", "UINT16", "Adjustment mode:\n 0 = MANUAL (requires manual timing input)\n 1 = AUTO (timing adjusted automatically)." },
        { "Param3", "UINT16", "Manual timing adjustment value (0 to 4095)." },
        { "Param4", "UINT16", "Auto adjustment value (used only when in AUTO mode)." },
        { "Param5", "UINT16", "Offset adjustment value (0 to 100) for fine-tuning." }
    };
    // For command "MBS_ADJUST"
    commandParameters[MBS_ADJUST] = {
        { "Param1", "UINT16", "MBS adjustment value (0 to 255). Higher values provide stronger suppression of the main bang." },
        { "Param2", "INT16", "Pulse width (currently not used)." }
    };
    // For command "ANTENNA_HEIGHT"
    commandParameters[ANTENNA_HEIGHT] = {
        { "Param1", "INT32", "Monitor number:\n 0 = MAIN (primary monitor)." },
        { "Param2", "UINT16", "Predefined antenna height category:\n"
            " 0 = 5 meters or less\n"
            " 1 = 7.5 meters\n"
            " 2 = 10 meters\n"
            " 3 = 15 meters\n"
            " 4 = 20 meters\n"
            " 5 = 25 meters\n"
            " 6 = 30 meters\n"
            " 7 = 35 meters\n"
            " 8 = 40 meters\n"
            " 9 = 45 meters\n"
            " 10 = 50 meters or more." },
        { "Param3", "UINT16", "Exact antenna height in meters for precise calibration." }
    };
    // For command "NEAR_STC"
    commandParameters[NEAR_STC] = {
        { "Param1", "UINT16", "STC(Sensitivity Time Control) curve setting (0 to 5):\n"
            " 0 = 2 (light suppression)\n"
            " 1 = 2.5\n"
            " 2 = 3\n"
            " 3 = 3.5\n"
            " 4 = 4.2\n"
            " 5 = 5.4 (strong suppression)." }
    };
	    // For command "MIDDLE_STC"
    commandParameters[MIDDLE_STC] = {
        { "Param1", "UINT16", "STC curve setting for middle range (0 to 3):\n"
            " 0 = 3 (light suppression)\n"
            " 1 = 4 (moderate suppression)\n"
            " 2 = 5 (strong suppression)\n"
            " 3 = 6 (very strong suppression)." }
    };
    // For command "FAR_STC"
    commandParameters[FAR_STC] = {
        { "Param1", "UINT16", "STC curve setting for far range (0 to 3):\n"
            " 0 = 6 (light suppression)\n"
            " 1 = 7 (moderate suppression)\n"
            " 2 = 8 (strong suppression)\n"
            " 3 = 3 (minimal suppression for calm conditions)." }
    };
    // For command "RING_SUPPRESSION"
    commandParameters[RING_SUPPRESSION] = {
        { "Param1", "UINT16", "Ring suppression adjustment (0 to 255):\n"
            " - Lower values provide minimal suppression\n"
            " - Higher values provide stronger suppression (target: rings disappear within 0.125 NM)." }
    };
    // For command "ANTENNA_REVOLUTION"
	commandParameters[ANTENNA_REVOLUTION] = {
		{ 
			"Param1", 
			"UINT16", 
			"Antenna speed:\n"
			" 0 = Lowest Speed (from documentation)\n"
			" 1 = High Speed (from testing)\n"
			" 2 = AUTO (auto-adjust based on pulse width)\n"
			" 3 = Medium Speed (from testing)\n"
			" 4 = High Speed (from testing)" 
		},
		{ 
			"Param2", 
			"UINT16", 
			"High rotation mode (currently not used):\n"
			" 0 = OFF." 
		}
	};

    // For command "ANTENNA_SWITCH"
    commandParameters[ANTENNA_SWITCH] = {
        { "Param1", "UINT16", "Antenna power switch:\n"
            " 0 = OFF (stop antenna)\n"
            " 1 = ON (start antenna)." }
    };
    // For command "STOPPED_TX"
    commandParameters[STOPPED_TX] = {
        { "Param1", "UINT16", "Transmission status (does not affect rotation):\n"
            " 0 = STBY (stop transmissions, standby mode)\n"
            " 1 = TX (enable transmissions)." }
    };
// For command "MONITOR_SELECT"
    commandParameters[MONITOR_SELECT] = {
        { "Param1", "INT32", "Monitor selection:\n"
            " 0 = MAIN (primary monitor)\n"
            " 1 = SUB (secondary monitor)." }
    };
// For command "ANTENNA_NO"
    commandParameters[ANTENNA_NO] = {
        { "Param1", "UINT16", "Antenna number (0 to 3):\n"
            " 0 = Antenna 0\n"
            " 1 = Antenna 1\n"
            " 2 = Antenna 2\n"
            " 3 = Antenna 3." }
    };
    // For command "ON_TIME"
    commandParameters[ON_TIME] = {
        { "Param1", "UINT32", "Operation time in seconds." }
    };
    // For command "TX_TIME"
    commandParameters[TX_TIME] = {
        { "Param1", "UINT32", "Transmission time in seconds." }
    };
    // For command "PM_ADJUST"
    commandParameters[PM_ADJUST] = {
        { "Param1", "UINT16", "Gain level (0 to 255). Higher values increase monitoring sensitivity." },
        { "Param2", "INT32", "Position value (x10,000) used for calibration." },
        { "Param3", "UINT16", "Adjustment mode:\n 0 = AUTO (automatic adjustment)\n 1 = MANUAL (manual adjustment)." }
    };
    // For command "DEMO_ECHO"
    commandParameters[DEMO_ECHO] = {
        { "Param1", "UINT16", "Demo echo mode:\n"
            " 0 = OFF (disable demo mode)\n"
            " 1 = EG (general echo generation)\n"
            " 2 = TT TEST (simulate target tracking)\n"
            " 3 = PC (PC-based radar simulation)\n"
            " 4 = EG-4000 (advanced echo generation for EG-4000 systems)." }
    };
    // For command "EAV_WITHOUT_GYRO"
    commandParameters[EAV_WITHOUT_GYRO] = {
        { "Param1", "UINT16", "Echo Average Processing (EAV) setting:\n"
            " 0 = OFF (disable echo averaging)\n"
            " 1 = ON (enable echo averaging)." }
    };
    // For command "SECONDECHO_REJECT"
    commandParameters[SECONDECHO_REJECT] = {
        { "Param1", "UINT16", "Second Echo Rejection status:\n"
            " 0 = OFF (disable second echo rejection)\n"
            " 1 = ON (enable second echo rejection to reduce false echoes)." }
    };
    // For command "PM"
    commandParameters[PM] = {
        { "Param1", "UINT16", "PM status:\n"
            " 0 = OFF (disable performance monitoring)\n"
            " 1 = ON (enable performance monitoring)." },
        { "Param2", "INT16", "Number of arcs (0 to 6) to monitor." }
    };
    // For command "SART"
    commandParameters[SART] = {
        { "Param1", "UINT16", "SART status:\n"
            " 0 = OFF (disable SART mode)\n"
            " 1 = ON (enable SART mode to detect distress signals)." }
    };
    // For command "VERSION_NO"
    commandParameters[VERSION_NO] = {
        { "Param1", "UINT8*", "Program number of radar application." },
        { "Param2", "UINT8*", "Program number of radar starter." },
        { "Param3", "UINT8*", "Program number of radar booter." },
        { "Param4", "UINT8*", "Program number of FPGA (Field-Programmable Gate Array)." },
        { "Param5", "UINT8*", "Program number of NMEA application." },
        { "Param6", "UINT8*", "Program number of NMEA starter." },
        { "Param7", "UINT8*", "Program number of NMEA booter." }
    };
    // For command "ARPA_TEST"
    commandParameters[ARPA_TEST] = {
        { "Param1", "UINT16", "ARPA test mode status:\n"
            " 0 = OFF (disable ARPA test mode)\n"
            " 1 = ON (enable ARPA test mode for diagnostics)." }
    };
// For command "GUARD_MODE"
    commandParameters[GUARD_MODE] = {
        { "Param1", "INT32", "Zone number:\n 0 = GuardZone1\n 1 = GuardZone2" },
        { "Param2", "UINT16", "Fan Stabilization:\n 0 = Heading (aligned to ship’s bow)\n 1 = North (aligned to true north)" },
        { "Param3", "UINT16", "Polygon Stabilization:\n"
            " 0 = OFF\n"
            " 1 = Ground (land reference)\n"
            " 2 = Heading (aligned to ship’s bow)\n"
            " 3 = North (aligned to true north)." },
        { "Param4", "UINT16", "Around Check Area:\n 0 = OFF (disable)\n 1 = ON (enable full 360-degree monitoring)." }
    };
    // For command "GUARD_FAN"
    commandParameters[GUARDZONE1FAN] = {
        { "Param1", "INT32", "Zone number:\n 0 = GuardZone1\n 1 = GuardZone2" },
        { "Param2", "INT32", "Starting distance (in 1/10,000 NM)." },
        { "Param3", "INT32", "Starting direction (0 to 3599, in 1/10 degrees)." },
        { "Param4", "INT32", "Ending distance (in 1/10,000 NM)." },
        { "Param5", "INT32", "Ending direction (0 to 3599, in 1/10 degrees)." }
    };
    // For command "GUARD_POLYGON"
    commandParameters[GUARD_POLYGON] = {
        { "Param1", "INT32", "Zone number:\n 0 = GuardZone1\n 1 = GuardZone2" },
        { "Param2", "UINT16", "Total number of polygon points (3 to 10)." },
        { "Param3", "INT32[]", "Distances (1/10,000 NM) for each polygon point." },
        { "Param4", "INT32[]", "Directions (0 to 3599 in 1/10 degrees) for each point." },
        { "Param5", "INT32[]", "Latitudes (1/10,000 minutes) for each point." },
        { "Param6", "INT32[]", "Longitudes (1/10,000 minutes) for each point." }
    };
    // For command "ARPA_WITHOUT_GYRO"
    commandParameters[ARPA_WITHOUT_GYRO] = {
        { "Param1", "UINT16", "ARPA without gyro mode:\n"
            " 0 = OFF (disable ARPA without gyro)\n"
            " 1 = ON (enable ARPA to function without gyro input)." }
    };
    // For command "SPEED_SELECT"
    commandParameters[SPEED_SELECT] = {
        { "Param1", "UINT16", "Speed source mode:\n"
            " 0 = LOG, BT (Bottom Track mode)\n"
            " 1 = LOG, WT (Water Track mode)\n"
            " 2 = GPS (speed from GPS input)\n"
            " 3 = MANUAL (user-entered speed)\n"
            " 4 = REF (reference speed from other system)." },
        { "Param2", "UINT16", "Speed value (0-999).\nUnit depends on system configuration (e.g., knots)." }
    };
    // For command "DRIFT"
    commandParameters[DRIFT] = {
        { "Param1", "UINT16", "Drift mode:\n 0 = OFF (disable drift correction)\n 1 = ON (enable drift correction)." },
        { "Param2", "UINT16", "Set (current’s direction in 1/10 degrees, 0-3599)." },
        { "Param3", "UINT16", "Drift speed (current speed in knots, 0-199)." }
    };
    // For command "GC10"
    commandParameters[GC10] = {
        { "Param1", "UINT16", "Heading offset value in 1/10 degrees (0 to 3599).\n"
            "Example: 900 = 90° (eastward alignment)." }
    };
    // For command "ARPA_CANCEL_STATUS"
    commandParameters[ARPA_CANCEL_STATUS] = {
        { "Param1", "INT32", "The target number to cancel tracking for (1 to ARPA_TARGET_MAX). This stops ARPA tracking of the specified target." }
    };
    // For command "ARPA_PRESET"
    commandParameters[ARPA_PRESET] = {
        { "Param1", "INT32", "The data number to adjust (0 to 99). Each number corresponds to a specific ARPA setting." },
        { "Param2", "UINT16", "The adjustment value for the selected setting (0 to 9999). Higher values increase the effect of the adjustment." }
    };
    // For command "NEXT_ECHO"
    commandParameters[NEXT_ECHO] = {
        { "Param1", "UINT16", "Controls the next screen update: 0 = Prepare for next update, 1 = Mark current data as readout complete." }
    };
    // For command "TRAIL_MODE"
    commandParameters[TRAIL_MODE] = {
        { "Param1", "UINT16", "Trail mode: 0 = Relative to radar movement, 1 = True movement." },
        { "Param2", "UINT16", "Trail gradation: 0 = Single trail, 1 = Multi-segmented trail." },
        { "Param3", "UINT16", "Narrow trails: 0 = Off, 1 = On (slim trail)." },
        { "Param4", "UINT16", "Trail level: 0 = Level 1 (basic) to 3 = Level 4 (most detailed)." },
        { "Param5", "UINT16", "Range connection: 0 = Off, 1 = On (trails linked to range)." },
        { "Param6", "UINT16", "Trail copy: 0 = Off, 1 = On (trails copied onto radar echoes)." },
        { "Param7", "UINT16", "Own ship trail: 0 = Off, 1 = On without cutout, 2 = On with cutout." },
        { "Param8", "UINT16", "Trail time (0 - 9999 seconds): Sets how long the trail stays visible." }
    };
    // For command "ARPA_CANCEL_TARGET_NO"
    commandParameters[ARPA_CANCEL_TARGETNO] = {
        { "Param1", "INT32", "The target number to cancel tracking for (1 to ARPA_TARGET_MAX). This stops the ARPA tracking for the specified target." }
    };
    // For command "SWEEP_POSITION"
    commandParameters[SWEEP_POSITION] = {
        { "Param1", "INT32", "The X offset in pixels. Positive values shift the sweep to the right, and negative values shift it to the left." },
        { "Param2", "INT32", "The Y offset in pixels. Positive values shift the sweep down, and negative values shift it up." }
    };
    // For command "ARPA_ACQUIRE_TARGET_DISPLAY_ID"
    commandParameters[ARPA_ACQUIRE_TARGET_DISPLAYID] = {
        { "Param1", "INT32", "The X coordinate (1/10000 NM) from the radar's center. Positive values move east, negative values move west." },
        { "Param2", "INT32", "The Y coordinate (1/10000 NM) from the radar's center. Positive values move north, negative values move south." },
        { "Param3", "INT32", "The marker type: \n"
                             "- Bits 0-13: Marker shape\n"
                             "- Bit 14: Guard zone entry flag (0 = Off, 1 = On)\n"
                             "- Bit 15: Guard zone alert approval flag (0 = Off, 1 = On)" },
        { "Param4", "INT32", "The ID of the target (0, 1-200, or 251-253 for special cases)." }
    };	
    // For command "ARPA_CANCEL_TARGET_DISPLAY_ID"
    commandParameters[ARPA_CANCEL_TARGET_DISPLAYID] = {
        { "Param1", "INT32", "The ID of the ARPA target to stop tracking (1 to ARPA_TARGET_MAX)." }
    };
    // For command "GPS_ANTENNA_POSITION"
    commandParameters[GPS_ANTENNA_POSITION] = {
        { "Param1", "INT32", "The GPS antenna number: 0 = First antenna, 1 = Second antenna." },
        { "Param2", "INT16", "The distance from the bow (in meters). Positive values place the antenna further towards the stern." },
        { "Param3", "INT16", "The distance from the port side (in meters). Positive values place the antenna closer to the starboard (right side)." }
    };
    // For command "CONNING_POSITION"
    commandParameters[CONNING_POSITION] = {
        { "Param1", "UINT16", "The distance from the bow (in meters). Positive values place the conning position further toward the stern." },
        { "Param2", "UINT16", "The distance from the port side (in meters). Positive values place the conning position closer to the starboard side." }
    };
    // For command "SELECT_ANTENNA"
    commandParameters[SELECT_ANTENNA] = {
        { "Param1", "UINT16", "The antenna number to select: 0 = First antenna, 1 = Second antenna." }
    };
    // For command "WAKEUP_COUNT"
    commandParameters[WAKEUP_COUNT] = {
        { "Param1", "UINT16", "The countdown time in seconds for preheating the magnetron." }
    };
    // For command "SHIP_INFO"
    commandParameters[SHIP_INFO] = {
        { "Param1", "UINT16", "The length of the ship in meters." },
        { "Param2", "UINT16", "The width of the ship in meters." }
    };
    // For command "AIS_ALARM"
    commandParameters[AIS_ALARM] = {
        { "Param1", "UINT16", "AIS alarm status. \n"
                              "Bit 2 = AIS Guard Zone \n"
                              "Bit 3 = AIS Lost Target \n"
                              "Bit 4 = AIS CPA Limit \n"
                              "Bit 5 = AIS Target Full" }
    };
    // For command "ARPA_ALARM"
    commandParameters[ARPA_ALARM] = {
        { "Param1", "UINT16", "ARPA alarm status. \n"
                              "Bit 2 = ARPA Guard Zone Alarm \n"
                              "Bit 3 = ARPA Lost Target Alarm \n"
                              "Bit 4 = ARPA CPA Limit Alarm \n"
                              "Bit 5 = Auto Acquisition Full Alarm \n"
                              "Bit 6 = Manual Acquisition Full Alarm \n"
                              "Bit 7 = System Error Alarm" }
    };
    // For command "SELECT_ALARM_OUT"
    commandParameters[SELECT_ALARMOUT] = {
        { "Param1", "UINT16", "The alarm output channel to configure (0 to 3)." },
        { "Param2", "UINT16", "The code for the type of alarm." },
        { "Param3", "UINT16", "Turn the alarm ON or OFF: 0 = OFF, 1 = ON." }
    };
    // For command "ALARM_ACK"
    commandParameters[ALARM_ACK] = {
        { "Param1", "UINT16", "The alarm number (identifier) as defined by the INS." },
        { "Param2", "UINT16", "The instance of the alarm to acknowledge." },
        { "Param3", "UINT8", "The command to acknowledge the alarm. Must always be 'A'." }
    };
    // For command "SELECT_CRT"
    commandParameters[SELECT_CRT] = {
        { "Param1", "UINT16", "Select the display mode or size for the radar. The exact modes depend on the radar's configuration." }
    };
    // For command "SELECT_ALARM_POLARITY"
    commandParameters[SELECT_ALARM_POLARITY] = {
        { "Param1", "INT32", "Select the alarm channel (0 to 3)." },
        { "Param2", "UINT16", "Set the polarity: 0 = OFF (inactive), 1 = ON (active)." }
    };
    // For command "COMBINE_STATUS"
    commandParameters[COMBINE_STATUS] = {
        { "Param1", "UINT16", "Set the combine status: 0 = OFF (disabled), 1 = ON (enabled)." },
        { "Param2", "INT16", "Select the antenna number (0 to 3)." }
    };
    // For command "HEADING_SOURCE"
    commandParameters[HEADING_SOURCE] = {
        { "Param1", "UINT16", "Select the heading source: 0 = AD10 protocol, 1 = Serial connection." }
    };
    // For command "NETWORK_PRIORITY"
    commandParameters[NETWORK_PRIORITY] = {
        { "Param1", "UINT16", "Set the network priority status: 0 = OFF (no priority), 1 = ON (high priority)." }
    };
    // For command "MENU_SET"
    commandParameters[MENUSET] = {
        { "Param1", "UINT16", "The menu number to set the preset configuration." },
        { "Param2", "UINT16", "The data value to apply for the selected menu number." }
    };
    // For command "IMO_ECHO_SWITCH"
    commandParameters[IMOECHO_SWITCH] = {
        { "Param1", "UINT16", "Set the IMO echo output: 0 = OFF (disable sharing), 1 = ON (enable sharing)." }
    };
// For command "EXT_ALARM_ACK"
    commandParameters[EXT_ALARM_ACK] = {
        { "Param1", "UINT16", "Acknowledge the external alarm: 0 = OFF (not acknowledged), 1 = ON (acknowledged)." }
    };
    // For command "PM_ADJUST_END"
    commandParameters[PM_ADJUST_END] = {
        { "Param1", "UINT16", "Set PM adjustment status: 0 = Idle (not adjusting), 1 = Adjusting (in progress)." }
    };
    // For command "SELECT_TTM_OUT"
    commandParameters[SELECT_TTMOUT] = {
        { "Param1", "UINT16", "Set TTM output status: 0 = OFF (disable), 1 = ON (enable)." }
    };
    // For command "ARPA_MAX_RANGE"
    commandParameters[ARPA_MAXRANGE] = {
        { "Param1", "UINT16", "Set the maximum ARPA tracking range: 0 = 24 NM, 1 = 32 NM, 2 = 48 NM." }
    };
    // For command "ARPA_ECHO_LEVEL"
    commandParameters[ARPA_ECHOLEVEL] = {
        { "Param1", "UINT16", "Set the echo detection level (1 to 31). Lower values reduce noise, higher values increase sensitivity." }
    };
    // For command "ARPA_LAND_SIZE"
    commandParameters[ARPA_LANDSIZE] = {
        { "Param1", "UINT16", "Set the minimum size of echoes to be considered a landmass (100 to 3000 meters, in increments of 100 meters)." }
    };
    // For command "ARPA_ANTENNA"
    commandParameters[ARPA_ANTENNA] = {
        { "Param1", "UINT16", "Set the antenna model: 15 = XN12A, 16 = XN13A, 19 = XN10A." }
    };
    // For command "ARPA_ACQ_CORRE"
    commandParameters[ARPA_ACQCORRE] = {
        { "Param1", "UINT16", "Set the number of consecutive echoes (3 to 10) required for automatic acquisition of a target." }
    };
    // For command "ARPA_WEED"
    commandParameters[ARPA_WEED] = {
        { "Param1", "UINT16", "Set the weed-out value (1 to 5). Determines how many consecutive missed echoes are allowed before canceling target tracking." }
    };
    // For command "ARPA_GATE_SIZE"
    commandParameters[ARPA_GATESIZE] = {
        { "Param1", "UINT16", "Set the ARPA gate size: 0 = S (Small), 1 = M (Medium), 2 = L (Large), 3 = LL (Extra Large)." }
    };
    // For command "ARPA_FILTER_RESPONS"
    commandParameters[ARPA_FILTERRESPONS] = {
        { "Param1", "UINT16", "Set the ARPA filter response: 0 = Fast (Level 1), 1 = Moderate (Level 2), 2 = Stable (Level 3), 3 = Most Stable (Level 4)." }
    };
    // For command "ARPA_LOST_COUNT"
    commandParameters[ARPA_LOSTCOUNT] = {
        { "Param1", "UINT16", "Set the lost count threshold (1 to 30). Lower values detect lost targets faster; higher values offer more stability." }
    };
    // For command "ARPA_MAX_SPEED"
    commandParameters[ARPA_MAXSPEED] = {
        { "Param1", "UINT16", "Set the maximum tracking speed (40 to 150 knots). Lower values track slow vessels; higher values track fast-moving targets." }
    };
    // For command "ARPA_TIME_VECTOR"
    commandParameters[ARPA_TIMEVECTOR] = {
        { "Param1", "UINT16", "Select the mode: 0 = Time Mode (seconds), 1 = Scan Mode (antenna scans)." },
        { "Param2", "UINT16", "Set the delay: 0-100 seconds if in Time Mode, 0-40 scans if in Scan Mode." }
    };
    // For command "RFC_VERSION_NO"
    commandParameters[RFC_VERSIONNO] = {
        { "Param1", "UINT8*", "The RFC version number (up to 20 ASCII characters)." }
    };	
    // For command "COMBINEDATA"
    commandParameters[COMBINE_DATA] = {
        { "Param1", "INT16", "Mode for combining radar data (TODO: Specific modes need to be defined)." },
        { "Param2", "INT16", "The starting angle for the radar sweep or data collection area, in tenths of a degree (0 to 3599)." },
        { "Param3", "INT16", "The angle range for the sweep, in tenths of a degree. This defines how wide the sweep will be." },
        { "Param4", "INT16", "The starting distance or radius from which the radar begins collecting data." },
        { "Param5", "INT16", "The range of the data collection area, starting from the initial length." }
    };
    // For command "INTERSWINFO"
    commandParameters[INTERSW_INFO] = {
        { "Param1", "UINT16[]", "An array of 8 values representing the inter-switch configuration. Each value sets a specific switch or input." }
    };
    // For command "ARPALOSTFILTERRANGE"
    commandParameters[ARPA_LOSTFILTER_RANGE] = {
        { "Param1", "UINT16", "Enables or disables the ARPA lost filter: 0 = OFF, 1 = ON." },
        { "Param2", "INT32", "The maximum distance for tracking lost targets in nautical miles (NM)." },
        { "Param3", "INT32", "The maximum distance for tracking lost targets in the radar's current unit (e.g., KM)." }
    };
    // For command "ARPALOSTFILTERSPEED"
    commandParameters[ARPA_LOSTFILTER_SPEED] = {
        { "Param1", "UINT16", "Enables or disables the ARPA lost filter: 0 = OFF, 1 = ON." },
        { "Param2", "INT32", "The minimum speed of targets to track, in 1/10 knots. Example: 50 = 5.0 knots." }
    };
    // For command "STCRANGE"
    commandParameters[STC_RANGE] = {
        { "Param1", "INT16", "The range for Sensitivity Time Control (STC). A higher value extends the clutter reduction range." }
    };
    // For command "CUSTOMMEMORY"
    commandParameters[CUSTOM_MEMORY] = {
        { "Param1", "UINT16", "The action to perform on the picture settings: 0 = Save, 1 = Load, 2 = Default (reset to factory settings)." }
    };
    // For command "BUILDUPTIME"
    commandParameters[BUILDUP_TIME] = {
        { "Param1", "INT32", "The build-up time in seconds. This sets how long trails remain visible on the radar screen." }
    };
    // For command "DISPLAYUNITINFO"
    commandParameters[DISPLAYUNIT_INFORMATION] = {
        { "Param1", "UINT16", "The antenna number to use (0 to 3)." },
        { "Param2", "UINT16", "Sets whether the unit is a master or a slave: 0 = Slave, 1 = Master." },
        { "Param3", "UINT8*", "The hostname of the radar unit (up to 8 characters)." }
    };
    // For command "ARPALOSTFILTERMODE"
    commandParameters[ARPA_LOSTFILTERMODE] = {
        { "Param1", "UINT16", "The mode for the ARPA lost filter: 0 = OFF, 1 = FILTER, 2 = ALL." }
    };
    // For command "ASSOCIATIONSTATUS"
    commandParameters[ASSOCIATION_STATUS] = {
        { "Param1", "INT32", "The association status: 0 = OFF, 1 = TTM (Tracked Target Message), 2 = TTD (Target Data Display)." }
    };
     // For command "CANCELSELECTTARGET"
    commandParameters[CANCEL_SELECT_TARGET] = {
        { "Param1", "INT16", "The starting target ID to cancel (-1 for all, 1-200 for specific targets, 251-253 for reference IDs)." },
        { "Param2", "INT16", "The ending target ID to cancel. Use the same valid range as the start target ID." }
    };
    // For command "CANCELSELECTLOSTTARGET"
    commandParameters[CANCEL_SELECT_LOSTTARGET] = {
        { "Param1", "INT16", "The starting lost target ID to cancel (-1 for all, 1-200 for specific targets, 251-253 for reference IDs)." },
        { "Param2", "INT16", "The ending lost target ID to cancel. Use the same valid range as the start target ID." }
    };
    // For command "CUSTOMATFPARAMETER"
    commandParameters[CUSTOM_ATFPARAMETER] = {
        { "Param1",  "INT32", "The radar picture number to adjust (0 to RMC_PICTURENO_MAX-1). Use -2 for temporary data." },
        { "Param2", "UINT16", "ATF status: 0 = OFF, 1 = ON." },
        { "Param3", "UINT16", "Gain value (0 to 100). Higher values increase radar sensitivity." },
        { "Param4", "INT16", "Sea clutter adjustment value (currently fixed at 0)." },
        { "Param5", "INT16", "Rain clutter adjustment value (currently fixed at 0)." },
        { "Param6", "UINT16", "High sensitivity mode: 0 = OFF, 1 = ON." },
        { "Param7", "UINT16", "ATF mode (currently fixed at 0)." }
    };
    // For command "TRAILPROCESS"
    commandParameters[TRAIL_PROCESS] = {
        { "Param1", "UINT16", "The trail process mode: 0 = MODEI (Dead Reckoning), 1 = MODEC (Uses Navigation Input)." }
    };
    // For command "ARPAREREACQUIRETARGET"
    commandParameters[ARPA_REACQUIRE_TARGET] = {
        { "Param1", "INT32", "The target number (1 to ARPA_TARGET_MAX)." },
        { "Param2", "INT32", "The target ID (1 to 200, or 251 to 253 for reference)." },
        { "Param3", "INT32", "The X-coordinate of the target (in 1/10,000 NM from true north)." },
        { "Param4", "INT32", "The Y-coordinate of the target (in 1/10,000 NM from true north)." },
        { "Param5", "UINT16", "The speed of the target (in 1/10 knots)." },
        { "Param6", "UINT16", "The course of the target (in 1/10 degrees from true north)." },
        { "Param7", "UINT16", "A 16-bit status for the target handover process." }
    };
    // For command "TXECHOSELECT"
    commandParameters[TX_ECHOSELECT] = {
        { "Param1", "UINT16", "Controls radar echo transmission: 0 = OFF, 1 = ON." }
    };
    // For command "RXECHOSELECT"
    commandParameters[RX_ECHOSELECT] = {
        { "Param1", "UINT16", "Selects the radar echo input: 0 = Own Master, 1 = Own Slave, 2 = Other Master, 3 = Other Slave." },
        { "Param2", "UINT8*", "The IP address of the radar unit to receive echo data from." }
    };
    // For command "RSPRESET"
    commandParameters[RS_PRESET] = {
        { "Param1", "INT32", "The preset data number to set (0 to 99)." },
        { "Param2", "UINT16", "The preset value to assign (0 to 9999)." }
    };
    // For command "JUMMINGABLE"
    commandParameters[JUMMING_ABLE] = {
        { "Param1", "UINT16", "Enable or disable jamming mode: 0 = OFF, 1 = ON." }
    };
    // For command "ICECLASS"
    commandParameters[ICECLASS] = {
        { "Param1", "UINT16", "Enable or disable IceClass mode for antenna rotation control: 0 = OFF, 1 = ON." },
        { "Param2", "UINT16", "Enable or disable Icing Prevention mode: 0 = OFF, 1 = ON." }
    };
    // For command "ATFSETTINGS"
    commandParameters[ATFSETTINGS] = {
        { "Param1", "UINT16", "Enable or disable ATF adjustment: 0 = OFF, 1 = ON." },
        { "Param2", "UINT16", "Signal enhancement level (1 = Low, 2 = Medium, 3 = High)." },
        { "Param3", "UINT16", "Cut level (0 to 8). Higher values apply stronger filtering." },
        { "Param4", "UINT16", "Starting direction for signal enhancement (0 to 359 degrees)." },
        { "Param5", "UINT16", "Sector range to apply enhancement (0 to 180 degrees)." }
    };
    // For command "SSDTXCHANNEL"
    commandParameters[SSD_TXCHANNEL] = {
        { "Param1", "UINT16", "The transmission channel: 0 = Auto, 1 = Channel 1, 2 = Channel 2, 3 = Channel 3." }
    };
    // For command "HIGHSENSITIVITYMODE"
    commandParameters[HIGH_SENSITIVITY_MODE] = {
        { "Param1", "UINT16", "Sets the high sensitivity mode: 0 = OFF, 1 = Low, 2 = Medium, 3 = High." }
    };
    // For command "BEARINGRESOLUTIONSELECT"
    commandParameters[BEARING_RESOLUTION_SELECT] = {
        { "Param1", "UINT16", "Selects the bearing resolution mode: 0 = OFF, 1 = Enhanced 1, 2 = Enhanced 2, 3 = Enhanced 3." }
    };
    // For command "ECHOCOLORTYPE"
    commandParameters[ECHO_COLOR_TYPE] = {
        { "Param1", "UINT16", "Display mode: 0 = Normal, 1 = Target Analyzer, 2 = Velocity Map." },
        { "Param2", "UINT16", "Rain display setting in Target Analyzer mode: 0 = OFF, 1 = ON." },
        { "Param3", "UINT16", "Auto video setting: 0 = OFF, 1 = ON." }
    };
    // For command "ACCUSHIP"
    commandParameters[ACCUSHIP] = {
        { "Param1", "UINT16", "Select ACCU-SHIP mode: 0 = OFF, 1 = ON." }
    };
    // For command "OBSERVATIONMODE"
    commandParameters[OBSERVATION_MODE] = {
        { "Param1", "UINT16", "Selects the observation mode: 0 = Normal, 1 = ICE, 2 = Wave, 3 = Full-TT." }
    };
    // For command "SARTSIGNAL"
    commandParameters[SART_SIGNAL] = {
        { "Param1", "UINT16", "SART signal status: 0 = Not Received, 1 = Received." }
    };
    // For command "NN3COMMAND"
    commandParameters[NN3COMMAND] = {
        { "Param1", "INT32", "The sub-command for the NN3 operation." },
        { "Param2", "UINT32", "The first parameter for customization (32-bit)." },
        { "Param3", "UINT16", "The second parameter (16-bit)." },
        { "Param4", "UINT16", "The third parameter (16-bit)." },
        { "Param5", "UINT16", "The fourth parameter (16-bit)." }
    };
    // For command "MEMORY"
    commandParameters[MEMORY] = {
        { "Param1", "UINT16", "The setting status: 0 = Factory reset." },
        { "Param2", "UINT16", "Memory number (currently set to 0)." }
    };
    // For command "EQUIPCOMP"
    commandParameters[EQUIP_COMP] = {
        { "Param1", "UINT16", "Setup status: 0 = None, 1 = Setting, 2 = Finished." }
    };
    // For command "PULSERANGE"
    commandParameters[PULSE_RANGE] = {
        { "Param1", "UINT16", "Minimum pulse length: 0 = S1, 1 = S2, 2 = M1, 3 = M2, 4 = M3, 5 = L." },
        { "Param2", "UINT16", "Maximum pulse length: 0 = S1, 1 = S2, 2 = M1, 3 = M2, 4 = M3, 5 = L." }
    };
    // For command "MAGNETRON"
    commandParameters[MAGNETRON] = {
        { "Param1", "UINT16", "Magnetron adjustment value (0 to 100)." },
        { "Param2", "UINT16", "Magnetron status: 0 = Start, 1 = Adjusting, 2 = Finished." },
        { "Param3", "UINT16", "Result of the operation: 0 = OK, 1 = NG (Not Good)." }
    };
    // For command "MAGNETRONIND"
    commandParameters[MAGNETRON_IND] = {
        { "Param1", "UINT16", "Magnetron indicator value (0 to 100)." }
    };
    // For command "ARPADETECT"
    commandParameters[ARPA_DETECT] = {
        { "Param1", "UINT16", "Processing range (1 to 64 NM)." },
        { "Param2", "UINT16", "Echo detect level (1 to 31, higher values for more sensitivity)." },
        { "Param3", "UINT16", "Echo detect size (1 to 10, larger values focus on larger objects)." }
    };
    // For command "ARPACATCH"
    commandParameters[ARPA_CATCH] = {
        { "Param1", "UINT16", "Detect land size (100 to 2000)." },
        { "Param2", "UINT16", "Auto acquisition correlation count (1 to 10)." },
        { "Param3", "UINT16", "Auto acquisition erasure count (1 to 10)." },
        { "Param4", "UINT16", "Gate size for tracking targets (1 to 10)." }
    };
    // For command "RANGESELECT"
    commandParameters[RANGE_SELECT] = {
        { "Param1", "INT32", "Unit of measurement: 0 = NM, 1 = KM, 2 = SM, 3 = KYD." },
        { "Param2", "UINT16[]", "Array of range codes (0 to RMC_RANGE_MAX-1)." }
    };
    // For command "VIDEOLEVEL"
    commandParameters[VIDEO_LEVEL] = {
        { "Param1", "UINT16", "Video level mode: 0 = Manual, 1 = Auto." },
        { "Param2", "UINT16", "Manual video level value (0 to 100)." }
    };
	
    // Add entries for other commands and parameters as needed...
}


///////////////////////////////////////////////////////////////////////////

void RadarCommand::OnGridCellSelected(wxGridEvent& event)
{
    int row = event.GetRow();
    int col = event.GetCol();

    // Get the command name from column 1
    std::string selectedCommand = m_grid1->GetCellValue(row, 1).ToStdString();

    if (col == 1) // Name column
    {
        // Existing code for displaying command description...
        auto it = commandDescriptions.find(selectedCommand);
        if (it != commandDescriptions.end())
        {
            m_infoText->SetValue(it->second);
        }
        else
        {
            m_infoText->SetValue("No detailed information available for this command.");
        }
    }
    else if (col >= 4 && col <= 13) // Parameter columns (Param1 to Param10)
    {
        // Calculate parameter index (0-based)
        int paramIndex = col - 4; // Column 4 corresponds to Param1

        // Find the command in the map
        auto cmdIt = commandParameters.find(selectedCommand);
        if (cmdIt != commandParameters.end())
        {
            const std::vector<CmdParamInfo>& params = cmdIt->second;
            if (paramIndex < params.size())
            {
                const CmdParamInfo& paramInfo = params[paramIndex];
                m_infoText->SetValue("Command: " + selectedCommand + "\n" +
                                     "Parameter: " + paramInfo.name + "\n" +
                                     "Type: " + paramInfo.type + "\n" +
                                     paramInfo.description);
            }
            else
            {
                m_infoText->SetValue("No information available for this parameter.");
            }
        }
        else
        {
            m_infoText->SetValue("No detailed information available for this command.");
        }
    }
    else
    {
        // Default message for other columns
        m_infoText->SetValue("Select a command or parameter to see more information here.");
    }

    event.Skip(); // Allow other handlers to process the event
}



void RadarCommand::m_comboBox1OnCombobox(wxCommandEvent& event) //"Radar No"
{
	AllClearFunction();
	g_wx_SelectedIndex = m_comboBox1->GetStringSelection();
}

void RadarCommand::m_button1OnButtonClick (wxCommandEvent& event) //"Get all"
{
	int row;
	Furuno::INT32 radarNo = (Furuno::INT32)(atoi(g_wx_SelectedIndex.c_str()));

	for (row = 0; row < NUM_OF_RadarFunctions; row++) {
		std::string command = m_grid1->GetCellValue(row, 1).ToStdString();
		GetFunction(radarNo, command, row, true);
	}
	m_grid1->Refresh();
}

void RadarCommand::m_button2OnButtonClick (wxCommandEvent& event) //"Clear"
{
	AllClearFunction();
}

// Programmatic "Get All" method
void RadarCommand::GetAllCommands()
{
    int radarNo = std::stoi(m_comboBox1->GetValue().ToStdString());
    CallAfter([=]() {
        for (int i = 0; i < NUM_OF_RadarFunctions; ++i) {
            std::string command = m_grid1->GetCellValue(i, 1).ToStdString();
            GetFunction(radarNo, command, i, true);
        }
    });
}

void RadarCommand::m_grid1OnGridCellLeftClick (wxGridEvent& event)
{
	int row = event.GetRow();
	Furuno::INT32 radarNo = (Furuno::INT32)(atoi(g_wx_SelectedIndex.c_str()));

	if (event.GetCol() == 2){
		std::string command = m_grid1->GetCellValue(row, 1).ToStdString();
		SetFunction(radarNo, command, row);
	}
	if (event.GetCol() == 3){
		std::string command = m_grid1->GetCellValue(row, 1).ToStdString();
		GetFunction(radarNo, command, row, false);
	}
	m_grid1->Refresh();
	event.Skip();
}

void RadarCommand::OnCellValueChanging(wxGridEvent& event)
{
	event.Skip();
}

void RadarCommand::OnCellValueChanged(wxGridEvent& event)
{
	event.Skip();
}

void RadarCommand::InitialRadarCommand()
{
    AddRadarNoToCombobox(m_comboBox1);
    m_comboBox1->SetSelection(0);
    g_wx_SelectedIndex = m_comboBox1->GetStringSelection();

    InitialRadarCommandGrid();
    InitializeCommandParameters(); // Add this line
}


void RadarCommand::InitialRadarCommandGrid()
{
	int i;
	static char _RadarFunctions[][NUM_OF_RadarFunctions][LENGTH_OF_RadarFunctions] =
	{
		{ RANGE, "COM2" },
		{ GAIN, "COM3" },
		{ ACSEA, "COM4" },
		{ ACRAIN, "COM5" },
		{ CUSTOMPICTURE_ALL, "COM6" },
		{ CUSTOMPICTURE, "COM7" },
		{ PULSE_WIDTH, "COM8" },
		{ TX_STBY, "COM9" },
		{ ANTENNA_TYPE, "COM14" },
		{ GUARD_STATUS, "COM16" },
		{ TUNE, "COM21" },
		{ BLIND_SECTOR, "COM23" },
		{ RADAR_POSITION, "COM24" },
		{ ALARM_STATUS, "COM29" },
		{ SELECT_QVDISPLAY, "COM31" },
		{ ATT, "COM32" },
		{ HEADING_ADJUST, "COM33" },
		{ TIMING_ADJUST, "COM34" },
		{ MBS_ADJUST, "COM35" },
		{ ANTENNA_HEIGHT, "COM36" },
		{ NEAR_STC, "COM37" },
		{ MIDDLE_STC, "COM38" },
		{ FAR_STC, "COM39" },
		{ RING_SUPPRESSION, "COM40" },
		{ ANTENNA_REVOLUTION, "COM41" },
		{ ANTENNA_SWITCH, "COM42" },
		{ STOPPED_TX, "COM43" },
		{ MONITOR_SELECT, "COM44" },
		{ ANTENNA_NO, "COM45" },
		{ ON_TIME, "COM46" },
		{ TX_TIME, "COM47" },
		{ PM_ADJUST, "COM48" },
		{ DEMO_ECHO, "COM49" },
		{ EAV_WITHOUT_GYRO, "COM50" },
		{ SECONDECHO_REJECT, "COM51" },
		{ PM, "COM52" },
		{ SART, "COM53" },
		{ GUARD_MODE, "COM56" },
		{ GUARDZONE1FAN, "COM57" },
		{ GUARDZONE2FAN, "COM58" },
		{ SPEED_SELECT, "COM61" },
		{ DRIFT, "COM62" },
		{ GC10, "COM63" },
		{ CONNING_POSITION, "COM74" },
		{ WAKEUP_COUNT, "COM76" },
		{ IMOECHO_SWITCH, "COM88" },
		{ CUSTOM_ATFPARAMETER, "COM128" },
		{ TRAIL_PROCESS, "COM129" },
		{ TX_ECHOSELECT, "COM132" },
		{ RX_ECHOSELECT, "COM133" },
		{ JUMMING_ABLE, "COM136" },
		{ ICECLASS, "COM137" },
		{ ATFSETTINGS, "COM138" },
		{ SSD_TXCHANNEL, "COM140" }
	};

	for (i = 0; i < NUM_OF_RadarFunctions; i++){
		m_grid1->SetCellValue(i, 0, wxString::FromUTF8(_RadarFunctions[i][1]));
		m_grid1->SetCellValue(i, 1, wxString::FromUTF8(_RadarFunctions[i][0]));
		m_grid1->SetCellValue(i, 2, "Set");
		m_grid1->SetCellValue(i, 3, "Get");

		m_grid1->SetCellBackgroundColour(i, 0, *wxWHITE);
		m_grid1->SetCellBackgroundColour(i, 1, *wxWHITE);
		m_grid1->SetCellBackgroundColour(i, 2, wxgrey);
		m_grid1->SetCellBackgroundColour(i, 3, wxgrey);

		m_grid1->SetCellAlignment(i, 2, wxALIGN_CENTER, wxALIGN_CENTRE);
		m_grid1->SetCellAlignment(i, 3, wxALIGN_CENTER, wxALIGN_CENTRE);

		m_grid1->SetReadOnly(i, 0);
		m_grid1->SetReadOnly(i, 1);
		m_grid1->SetReadOnly(i, 2);
		m_grid1->SetReadOnly(i, 3);
	}

	m_grid1->AutoSizeColumn(1, true);

}

// Returns the index of a command in the grid
int RadarCommand::getCommandIndex(const std::string& commandName) const
{
    for (int i = 0; i < NUM_OF_RadarFunctions; ++i) {
        if (m_grid1->GetCellValue(i, 1).ToStdString() == commandName) {
            return i;
        }
    }
    return -1; // Command not found
}

// Returns a list of command names
std::vector<std::string> RadarCommand::getCommandList() const
{
    std::vector<std::string> cmds;
    for (int i = 0; i < NUM_OF_RadarFunctions; ++i) {
        cmds.push_back(m_grid1->GetCellValue(i, 1).ToStdString());
    }
    return cmds;
}

// Returns a list of radar numbers (from the combo box)
std::vector<std::string> RadarCommand::getRadarNumbers() const
{
    std::vector<std::string> radarNos;
    for (unsigned int i = 0; i < m_comboBox1->GetCount(); ++i) {
        radarNos.push_back(m_comboBox1->GetString(i).ToStdString());
    }
    return radarNos;
}

void RadarCommand::SetFunction(Furuno::INT32 radarNo, std::string command, int index)
{
	bool isInvalidCommand = false;
	bool isServerModeCommand = false;

	Furuno::UINT8 uc1[32] = { 0 }, uc2[128] = { 0 };
	Furuno::INT16 s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0;
	Furuno::UINT16 us1 = 0, us2 = 0, us3 = 0, us4 = 0, us5 = 0, us6 = 0, us7 = 0, us8 = 0;
	Furuno::INT32 l1 = 0, l2 = 0, l3 = 0, l4 = 0;
	Furuno::UINT32 ul1 = 0, ul2 = 0;
	std::string str1 = ""; 
	int _argIndex = 0;
	Furuno::UINT8 keyCom[100] = {};
	Furuno::UINT8 bvalue1[21] = {};
	Furuno::INT16 dataTable[] = { 2, 0, 0, 0, 0, 0, 3, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 4 };
	Furuno::UINT16	range[24];
	Furuno::INT32 dist[POLYGON_MAX], dir[POLYGON_MAX], lat[POLYGON_MAX], lon[POLYGON_MAX];
	Furuno::INT32 ret = RADAR_NODATA; 

		if (command == RANGE){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			ret = Furuno::RmcSetRange_v2(radarNo, us1, us2, us3);
		}
		else if (command == GAIN){
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
            SetArgument(index, &us3, &_argIndex);
			ret = Furuno::RmcSetCustomGAIN(radarNo, l1, us1, us2, us3);
		}
		else if (command == ACSEA){
            SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			SetArgument(index, &us4, &_argIndex);
			ret = Furuno::RmcSetCustomACSEA(radarNo, l1, us1, us2, us3, us4);
		}
		else if (command == ACRAIN){
            SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			SetArgument(index, &us4, &_argIndex);
			SetArgument(index, &us5, &_argIndex);
			ret = Furuno::RmcSetCustomACRAIN(radarNo, l1, us1, us2, us3, us4, us5);
		}
		else if (command == CUSTOMPICTURE_ALL){
			SetArgument(index, &l1, &_argIndex);
			Furuno::RmcReqCustomPictureAll(radarNo, l1);
			ret = Furuno::RmcSetCustomPictureAll(radarNo, l1, dataTable);
		}
		else if (command == CUSTOMPICTURE){
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &l2, &_argIndex);
			SetArgument(index, &s1, &_argIndex);
			ret = Furuno::RmcSetCustomPicture(radarNo, l1, l2, s1);
		}
		else if (command == PULSE_WIDTH){		
			SetArgument(index, &s1, &_argIndex);
			SetArgument(index, &s2, &_argIndex);
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &l1, &_argIndex);
			ret = Furuno::RmcSetPulseWidth(radarNo, s1, s2, us1, l1);
		}
		else if (command == TX_STBY){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &ul1, &_argIndex);
			SetArgument(index, &ul2, &_argIndex);
			ret = Furuno::RmcSetTxStby(radarNo, us1, us2, ul1, ul2);
		}
		else if (command == ANTENNA_TYPE){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			SetArgument(index, &us4, &_argIndex);
			SetArgument(index, &us5, &_argIndex);
			SetArgument(index, &us6, &_argIndex);
			SetArgument(index, &us7, &_argIndex);
			ret = Furuno::RmcSetAntennaType(radarNo, us1, us2, us3, us4, us5, us6, us7);
		}
		else if (command == GUARD_STATUS){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			ret = Furuno::RmcSetGuardStatus(radarNo, us1, us2, us3);
		}
		else if (command == TUNE){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			ret = Furuno::RmcSetTune(radarNo, us1, us2);
		}
		else if (command == BLIND_SECTOR){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			SetArgument(index, &us4, &_argIndex);
			SetArgument(index, &us5, &_argIndex);
			ret = Furuno::RmcSetBlindSector(radarNo, us1, us2, us3, us4, us5);
		}
		else if (command == RADAR_POSITION){
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &s1, &_argIndex);
			SetArgument(index, &s2, &_argIndex);
			ret = Furuno::RmcSetRadarPosition(radarNo, l1, s1, s2);
		}
		else if (command == ALARM_STATUS){
			isServerModeCommand = true;
		}
		else if (command == SELECT_QVDISPLAY){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetSelectQVDisplay(radarNo, us1);
		}
		else if (command == ATT){
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &s1, &_argIndex);
			ret = Furuno::RmcSetATT(radarNo, l1, us1, s1);
		}
		else if (command == HEADING_ADJUST){
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &s1, &_argIndex);
			ret = Furuno::RmcSetHeadingAdjust(radarNo, l1, us1, s1);
		}
		else if (command == TIMING_ADJUST){
			SetArgument(index, &l1, &_argIndex); 
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			SetArgument(index, &us4, &_argIndex);
			ret = Furuno::RmcSetTimingAdjust(radarNo, l1, us1, us2, us3, us4);
		}
		else if (command == MBS_ADJUST){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &s1, &_argIndex);
			ret = Furuno::RmcSetMBSAdjust(radarNo, us1, s1);
		}
		else if (command == ANTENNA_HEIGHT){
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			ret = Furuno::RmcSetAntennaHeight(radarNo, l1, us1, us2);
		}
		else if (command == NEAR_STC){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetNearSTC(radarNo, us1);
		}
		else if (command == MIDDLE_STC){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetMiddleSTC(radarNo, us1);
		}
		else if (command == FAR_STC){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetFarSTC(radarNo, us1);
		}
		else if (command == RING_SUPPRESSION){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetRingSuppression(radarNo, us1);
		}
		else if (command == ANTENNA_REVOLUTION){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			ret = Furuno::RmcSetAntennaRevolution(radarNo, us1, us2);
		}
		else if (command == ANTENNA_SWITCH){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetAntennaSwitch(radarNo, us1);
		}
		else if (command == STOPPED_TX){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetStoppedTx(radarNo, us1);
		}
		else if (command == MONITOR_SELECT){
			SetArgument(index, &l1, &_argIndex);
			ret = Furuno::RmcSetMonitorSelect(radarNo, l1);
		}
		else if (command == ANTENNA_NO){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetAntennaNo(radarNo, us1);
		}
		else if (command == ON_TIME){
			SetArgument(index, &ul1, &_argIndex);
			ret = Furuno::RmcSetOnTime(radarNo, ul1);
		}
		else if (command == TX_TIME){
			SetArgument(index, &ul1, &_argIndex);
			ret = Furuno::RmcSetTxTime(radarNo, ul1);
		}
		else if (command == PM_ADJUST){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			ret = Furuno::RmcSetPmAdjust(radarNo, us1, l1, us2);
		}
		else if (command == DEMO_ECHO){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetDemoEcho(radarNo, us1);
		}
		else if (command == EAV_WITHOUT_GYRO){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetEavWithoutGyro(radarNo, us1);
		}
		else if (command == SECONDECHO_REJECT){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSet2ndEchoReject(radarNo, us1);
		}
		else if (command == PM){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			ret = Furuno::RmcSetPM(radarNo, us1, us2);
		}
		else if (command == SART){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetSART(radarNo, us1);
		}
		else if (command == GUARD_MODE){
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			ret = Furuno::RmcSetGuardMode(radarNo, l1, us1, us2, us3);
		}
		else if (command == GUARDZONE1FAN){
			_argIndex++;
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &l2, &_argIndex);
			SetArgument(index, &l3, &_argIndex);
			SetArgument(index, &l4, &_argIndex);
			ret = Furuno::RmcSetGuardFan(radarNo, 0, l1, l2, l3, l4);
		}
		else if (command == GUARDZONE2FAN){ 
			_argIndex++;
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &l2, &_argIndex);
			SetArgument(index, &l3, &_argIndex);
			SetArgument(index, &l4, &_argIndex);
			ret = Furuno::RmcSetGuardFan(radarNo, 1, l1, l2, l3, l4);
		}
		else if (command == SPEED_SELECT){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			ret = Furuno::RmcSetSpeedSelect(radarNo, us1, us2);
		}
		else if (command == DRIFT){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			ret = Furuno::RmcSetDrift(radarNo, us1, us2, us3);
		}
		else if (command == GC10){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetGC10(radarNo, us1);
		}
		else if (command == CONNING_POSITION){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			ret = Furuno::RmcSetConningPosition(radarNo, us1, us2);
		}
		else if (command == WAKEUP_COUNT){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetWakeupCount(radarNo, us1);
		}		
		else if (command == IMOECHO_SWITCH){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetImoEchoSwitch(radarNo, us1);
		}
		else if (command == CUSTOM_ATFPARAMETER){
			SetArgument(index, &l1, &_argIndex);
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &s1, &_argIndex);
			SetArgument(index, &s2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			SetArgument(index, &us4, &_argIndex);
			ret = Furuno::RmcSetCustomAtfParameter(radarNo, l1, us1, us2, s1, s2, us3, us4);
		}
		else if (command == TRAIL_PROCESS){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetTrailProcess(radarNo, us1);
		}
		else if (command == TX_ECHOSELECT){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetTxEchoSelect(radarNo, us1);
		}
		else if (command == RX_ECHOSELECT){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &str1, &_argIndex);
			{
				Furuno::UINT8* data = (Furuno::UINT8*)str1.c_str();
				ret = Furuno::RmcSetRxEchoSelect(radarNo, us1, data);
			}
		}
		else if (command == JUMMING_ABLE){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetJummingAble(radarNo, us1);
		}
		else if (command == ICECLASS){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			ret = Furuno::RmcSetIceClass(radarNo, us1, us2);
		}
		else if (command == ATFSETTINGS){
			SetArgument(index, &us1, &_argIndex);
			SetArgument(index, &us2, &_argIndex);
			SetArgument(index, &us3, &_argIndex);
			SetArgument(index, &us4, &_argIndex);
			SetArgument(index, &us5, &_argIndex);
			ret = Furuno::RmcSetAtfSettings(radarNo, us1, us2, us3, us4, us5);
		}
		else if (command == SSD_TXCHANNEL){
			SetArgument(index, &us1, &_argIndex);
			ret = Furuno::RmcSetSsdTxChannel(radarNo, us1);
		}
		
	if (ret != RADAR_OK) {
		m_grid1->SetCellBackgroundColour(index, 0, wxred);
		m_grid1->SetCellBackgroundColour(index, 1, wxred);
	}
	else {
		m_grid1->SetCellBackgroundColour(index, 0, wxblue);
		m_grid1->SetCellBackgroundColour(index, 1, wxblue);
		m_grid1->SetCellBackgroundColour(index, 2, wxgreen);
		m_grid1->SetCellBackgroundColour(index, 3, wxgreen);
	}

	if (isInvalidCommand) {
		wxLogError("This command can not be used.");
	}
	if (isServerModeCommand) {
		wxLogError("This command is only used in server mode.");
	}
}

void RadarCommand::GetFunction (Furuno::INT32 radarNo, std::string command, int index, bool isAll)
{
	bool isInvalidCommand = false;
	bool isServerModeCommand = false;

	Furuno::UINT8 version[21];
	Furuno::UINT8 uc11;
	Furuno::INT16 s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0;
	Furuno::UINT16 us1 = 0, us2 = 0, us3 = 0, us4 = 0, us5 = 0, us6 = 0, us7 = 0, us8 = 0;
	Furuno::INT32 l1 = 0, l2 = 0, l3 = 0, l4 = 0, l5 = 0;
	Furuno::UINT32 ul1 = 0, ul2 = 0, ul3 = 0; 
	int _argIndex = 0; 
	Furuno::UINT8 keyCom[100] = {};
	Furuno::UINT8 ip_addr[21] = {};
	Furuno::INT16 dataTable[RMC_FUNCNO_MAX];
	Furuno::UINT16	range[25];
	Furuno::INT32 dist[POLYGON_MAX], dir[POLYGON_MAX], lat[POLYGON_MAX], lon[POLYGON_MAX];
	Furuno::INT32 ret = RADAR_NODATA;
	std::string   setArg;   // Will store something like "us1, us2, l1", etc.
		if (command == RANGE){
			ret = Furuno::RmcGetRange_v2(radarNo, &s1, &us1, &us2);
			setArg = "s1, us1, us2";
		}
		else if (command == GAIN){
			l1 = -2;
			ret = Furuno::RmcGetCustomGAIN(radarNo, &l1, &us1, &us2, &us3);
            setArg = "l1, us1, us2, us3";
		}
		else if (command == ACSEA){
			l1 = -2;
			ret = Furuno::RmcGetCustomACSEA(radarNo, &l1, &us1, &us2, &us3, &us4);
            setArg = "l1, us1, us2, us3, us4";
		}
		else if (command == ACRAIN){
			l1 = -2;
			ret = Furuno::RmcGetCustomACRAIN(radarNo, &l1, &us1, &us2, &us3, &us4, &us5);
            setArg = "l1, us1, us2, us3, us4, us5";
		}
		else if (command == CUSTOMPICTURE_ALL){ 
			l1 = -1;
			ret = Furuno::RmcGetCustomPictureAll(radarNo, &l1, dataTable);
			ShowArgument(index, l1, &_argIndex);
			ShowArgument(index, dataTable[0], &_argIndex);
		}
		else if (command == CUSTOMPICTURE){
			ret = Furuno::RmcGetCustomPicture(radarNo, &l1, &l2, &s1);
			setArg = "l1, l2, s1";
		}
		else if (command == PULSE_WIDTH){
			s2 = RMCGET_LATEST;
			us1 = 0;
			l1 = RMCTEMPOLARY;
			ret = Furuno::RmcGetPulseWidth(radarNo, &s1, &s2, &us1, &l1);
			setArg = "s1, s2, us1, l1";
		}
		else if (command == TX_STBY){
			ret = Furuno::RmcGetTxStby(radarNo, &us1, &us2, &ul1, &ul2);
			setArg = "us1, us2, ul1, ul2";
		}
		else if (command == ANTENNA_TYPE){
			ret = Furuno::RmcGetAntennaType(radarNo, &us1, &us2, &us3, &us4, &us5, &us6, &us7);
			setArg = "us1, us2, us3, us4, us5, us6, us7";
		}
		else if (command == GUARD_STATUS){
			ret = Furuno::RmcGetGuardStatus(radarNo, &us1, &us2, &us3);
			setArg = "us1, us2, us3";
		}
		else if (command == TUNE){
			ret = Furuno::RmcGetTune(radarNo, &us1, &us2);
			setArg = "us1, us2";
		}
		else if (command == BLIND_SECTOR){
			ret = Furuno::RmcGetBlindSector(radarNo, &us1, &us2, &us3, &us4, &us5);
			setArg = "us1, us2, us3, us4, us5";
		}
		else if (command == RADAR_POSITION){
			ret = Furuno::RmcGetRadarPosition(radarNo, &l1, &s1, &s2);
			setArg = "l1, s1, s2";
		}
		else if (command == ALARM_STATUS){
			ret = Furuno::RmcGetAlarmStatus(radarNo, &us1, &ul1, &ul2, &ul3);
			setArg = "us1, ul1, ul2, ul3";
		}
		else if (command == SELECT_QVDISPLAY){
			ret = Furuno::RmcGetSelectQVDisplay(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == ATT){
			ret = Furuno::RmcGetATT(radarNo, &l1, &us1, &s1);
			setArg = "l1, us1, s1";
		}
		else if (command == HEADING_ADJUST){
			ret = Furuno::RmcGetHeadingAdjust(radarNo, &l1, &us1, &s1);
			setArg = "l1, us1, s1";
		}
		else if (command == TIMING_ADJUST){
			ret = Furuno::RmcGetTimingAdjust(radarNo, &l1, &us1, &us2, &us3, &us4);
			setArg = "l1, us1, us2, us3, us4";
		}
		else if (command == MBS_ADJUST){
			ret = Furuno::RmcGetMBSAdjust(radarNo, &us1, &s1);
            setArg = "us1, s1";
		}
		else if (command == ANTENNA_HEIGHT){
			ret = Furuno::RmcGetAntennaHeight(radarNo, &l1, &us1, &us2);
			setArg = "l1, us1, us2";
		}
		else if (command == NEAR_STC){
			ret = Furuno::RmcGetNearSTC(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == MIDDLE_STC){
			ret = Furuno::RmcGetMiddleSTC(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == FAR_STC){
			ret = Furuno::RmcGetFarSTC(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == RING_SUPPRESSION){
			ret = Furuno::RmcGetRingSuppression(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == ANTENNA_REVOLUTION){
			ret = Furuno::RmcGetAntennaRevolution(radarNo, &us1, &us2);
			setArg = "us1, us2";
		}
		else if (command == ANTENNA_SWITCH){
			ret = Furuno::RmcGetAntennaSwitch(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == STOPPED_TX){
			ret = Furuno::RmcGetStoppedTx(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == MONITOR_SELECT){
			ret = Furuno::RmcGetMonitorSelect(radarNo, &l1);
			setArg = "l1";
		}
		else if (command == ANTENNA_NO){
			ret = Furuno::RmcGetAntennaNo(radarNo, &us1);
            setArg = "us1";
		}
		else if (command == ON_TIME){
			ret = Furuno::RmcGetOnTime(radarNo, &ul1);
			setArg = "ul1";
		}
		else if (command == TX_TIME){
			ret = Furuno::RmcGetTxTime(radarNo, &ul1);
			setArg = "ul1";
		}
		else if (command == PM_ADJUST){
			ret = Furuno::RmcGetPmAdjust(radarNo, &us1, &l1, &us2);
			setArg = "us1, l1, us2";
		}
		else if (command == DEMO_ECHO){
			ret = Furuno::RmcGetDemoEcho(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == EAV_WITHOUT_GYRO){
			ret = Furuno::RmcGetEavWithoutGyro(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == SECONDECHO_REJECT){
			ret = Furuno::RmcGet2ndEchoReject(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == PM){
			ret = Furuno::RmcGetPM(radarNo, &us1, &s1);
			setArg = "us1, s1";
		}
		else if (command == SART){
			ret = Furuno::RmcGetSART(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == GUARD_MODE){
			l1 = RMCGET_LATEST;
			ret = Furuno::RmcGetGuardMode(radarNo, &l1, &us1, &us2, &us3);
			setArg = "l1, us1, us2, us3";
		}
		else if (command == GUARDZONE1FAN){
			l1 = 0; //zoneNo
			ret = Furuno::RmcGetGuardFan(radarNo, &l1, &l2, &l3, &l4, &l5);
			setArg = "l1, l2, l3, l4, l5";
		}
		else if (command == GUARDZONE2FAN){
			l1 = 1; //zoneNo
			ret = Furuno::RmcGetGuardFan(radarNo, &l1, &l2, &l3, &l4, &l5);
			setArg = "l1, l2, l3, l4, l5";
		}
		else if (command == SPEED_SELECT){
			ret = Furuno::RmcGetSpeedSelect(radarNo, &us1, &us2);
			setArg = "us1, us2";
		}
		else if (command == DRIFT){
			ret = Furuno::RmcGetDrift(radarNo, &us1, &us2, &us3);
			setArg = "us1, us2, us3";
		}
		else if (command == GC10){
			ret = Furuno::RmcGetGC10(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == CONNING_POSITION){
			ret = Furuno::RmcGetConningPosition(radarNo, &us1, &us2);
			setArg = "us1, us2";
		}
		else if (command == WAKEUP_COUNT){
			ret = Furuno::RmcGetWakeupCount(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == IMOECHO_SWITCH){
			ret = Furuno::RmcGetImoEchoSwitch(radarNo, &us1);
            setArg = "us1";
		}
		else if (command == CUSTOM_ATFPARAMETER){
			ret = Furuno::RmcGetCustomAtfParameter(radarNo, &l1, &us1, &us2, &s1, &s2, &us3, &us4);
			setArg = "l1, us1, us2, s1, s2, us3, us4";
		}
		else if (command == TRAIL_PROCESS){
			ret = Furuno::RmcGetTrailProcess(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == TX_ECHOSELECT){
			ret = Furuno::RmcGetTxEchoSelect(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == RX_ECHOSELECT){
			ret = Furuno::RmcGetRxEchoSelect(radarNo, &us1, ip_addr);
			ShowArgument(index, us1, &_argIndex);
			ShowArgument(index, ip_addr, &_argIndex);
		}
		else if (command == JUMMING_ABLE){
			ret = Furuno::RmcGetJummingAble(radarNo, &us1);
			setArg = "us1";
		}
		else if (command == ICECLASS){
			ret = Furuno::RmcGetIceClass(radarNo, &us1, &us2);
			setArg = "us1, us2";
		}
		else if (command == ATFSETTINGS){
			ret = Furuno::RmcGetAtfSettings(radarNo, &us1, &us2, &us3, &us4, &us5);
			setArg = "us1, us2, us3, us4, us5";
		}
		else if (command == SSD_TXCHANNEL){
			ret = Furuno::RmcGetSsdTxChannel(radarNo, &us1);
			setArg = "us1";
		}


	//------------------------------------------------------
	// 3. Parse the setArg string & call ShowArgument() for
	//    each variable name in the order they appear
	//------------------------------------------------------
	
	// (a) Helper to split by comma
	auto tokenize = [](const std::string &str, char delim) {
		std::vector<std::string> tokens;
		std::istringstream iss(str);
		std::string token;
		while (std::getline(iss, token, delim))
		{
			while (!token.empty() && std::isspace((unsigned char)token.front()))
				token.erase(token.begin());
			while (!token.empty() && std::isspace((unsigned char)token.back()))
				token.pop_back();

			tokens.push_back(token);
		}
		return tokens;
	};

	// Split setArg (e.g. "us1, us2, us3, us4, us5") by comma
	auto tokens = tokenize(setArg, ',');

	// (b) For each token, map to the correct variable
	for (auto &token : tokens)
	{
		if      (token == "s1")  ShowArgument(index, s1,  &_argIndex);
		else if (token == "s2")  ShowArgument(index, s2,  &_argIndex);
		else if (token == "s3")  ShowArgument(index, s3,  &_argIndex);
		else if (token == "s4")  ShowArgument(index, s4,  &_argIndex);
		else if (token == "s5")  ShowArgument(index, s5,  &_argIndex);

		else if (token == "us1") ShowArgument(index, us1, &_argIndex);
		else if (token == "us2") ShowArgument(index, us2, &_argIndex);
		else if (token == "us3") ShowArgument(index, us3, &_argIndex);
		else if (token == "us4") ShowArgument(index, us4, &_argIndex);
		else if (token == "us5") ShowArgument(index, us5, &_argIndex);
		else if (token == "us6") ShowArgument(index, us6, &_argIndex);
		else if (token == "us7") ShowArgument(index, us7, &_argIndex);
		else if (token == "us8") ShowArgument(index, us8, &_argIndex);

		else if (token == "l1")  ShowArgument(index, l1,  &_argIndex);
		else if (token == "l2")  ShowArgument(index, l2,  &_argIndex);
		else if (token == "l3")  ShowArgument(index, l3,  &_argIndex);
		else if (token == "l4")  ShowArgument(index, l4,  &_argIndex);
		else if (token == "l5")  ShowArgument(index, l5,  &_argIndex);

		else if (token == "ul1") ShowArgument(index, ul1, &_argIndex);
		else if (token == "ul2") ShowArgument(index, ul2, &_argIndex);
		else if (token == "ul3") ShowArgument(index, ul3, &_argIndex);
	}

	if (ret != RADAR_OK) {
		m_grid1->SetCellBackgroundColour(index, 0, wxred);
		m_grid1->SetCellBackgroundColour(index, 1, wxred);
	}
	else {
		m_grid1->SetCellBackgroundColour(index, 0, wxblue);
		m_grid1->SetCellBackgroundColour(index, 1, wxblue);
		m_grid1->SetCellBackgroundColour(index, 2, wxgreen);
		m_grid1->SetCellBackgroundColour(index, 3, wxgreen);
	}

	if (!isAll && isInvalidCommand) {
		wxLogError("This command can not be used.");
	}
	if (!isAll && isServerModeCommand)
	{
		wxLogError("This command is only used in server mode.");
	}
}

void RadarCommand::SetArgument(int index, Furuno::UINT8 *value, int *_argIndex)
{
	std::string _value = m_grid1->GetCellValue(index, (4 + *_argIndex)).ToStdString();
	int _value_size = _value.size();

	if (!_value.empty()){
			strncpy((char*)value, _value.c_str(), _value_size);
	}
	(*_argIndex)++;
}

void RadarCommand::SetArgument(int index, Furuno::INT16 *value, int *_argIndex)
{
	Furuno::INT16 value_copy = 0;
	std::string _value = m_grid1->GetCellValue(index, (4 + *_argIndex)).ToStdString();

	if (!_value.empty()){
			std::sscanf(_value.c_str(), "%hd", &value_copy);
			*value = value_copy;
	}
	(*_argIndex)++;
}

void RadarCommand::SetArgument(int index, Furuno::UINT16 *value, int *_argIndex)
{
	Furuno::UINT16 value_copy = 0;
	std::string _value = m_grid1->GetCellValue(index, (4 + *_argIndex)).ToStdString();

	if (!_value.empty()){
		std::sscanf(_value.c_str(), "%hd", &value_copy);
		*value = value_copy;
	}
	(*_argIndex)++;
}

void RadarCommand::SetArgument(int index, Furuno::INT32 *value, int *_argIndex)
{
	Furuno::INT32 value_copy = 0;
	std::string _value = m_grid1->GetCellValue(index, (4 + *_argIndex)).ToStdString();

	if (!_value.empty()){
			std::sscanf(_value.c_str(), "%d", &value_copy);
			*value = value_copy;
	}
	(*_argIndex)++;
}

void RadarCommand::SetArgument(int index, Furuno::UINT32 *value, int *_argIndex)
{
	Furuno::UINT32 value_copy = 0;
	std::string _value = m_grid1->GetCellValue(index, (4 + *_argIndex)).ToStdString();

	if (!_value.empty()){
			std::sscanf(_value.c_str(), "%d", &value_copy);
			*value = value_copy;
	}
	(*_argIndex)++;
}

void RadarCommand::SetArgument(int index, std::string *value, int *_argIndex)
{
	std::string _value = m_grid1->GetCellValue(index, (4 + *_argIndex)).ToStdString();
	if (!_value.empty())
	{
			*value = _value;
	}
	(*_argIndex)++;
}

void RadarCommand::ShowArgument(int index, Furuno::UINT8 value, int *_argIndex)
{
	wxString wx_value(std::to_string(value));
	m_grid1->SetCellValue(index, (4 + *_argIndex), wx_value);
	(*_argIndex)++;
}

void RadarCommand::ShowArgument(int index, Furuno::UINT8 *value, int *_argIndex)
{
	wxString wx_value = wxString::FromUTF8((char*)value);
	m_grid1->SetCellValue(index, (4 + *_argIndex), wx_value);
	(*_argIndex)++;
}

void RadarCommand::ShowArgument(int index, Furuno::INT16 value, int *_argIndex)
{
	wxString wx_value(std::to_string(value));
	m_grid1->SetCellValue(index, (4 + *_argIndex), wx_value);
	(*_argIndex)++;
}

void RadarCommand::ShowArgument(int index, Furuno::UINT16 value, int *_argIndex)
{
	wxString wx_value(std::to_string(value));
	m_grid1->SetCellValue(index, (4 + *_argIndex), wx_value);
	(*_argIndex)++;
}

void RadarCommand::ShowArgument(int index, Furuno::INT32 value, int *_argIndex)
{
	wxString wx_value(std::to_string(value));
	m_grid1->SetCellValue(index, (4 + *_argIndex), wx_value);
	(*_argIndex)++;
}

void RadarCommand::ShowArgument(int index, Furuno::UINT32 value, int *_argIndex)
{
	wxString wx_value(std::to_string(value));
	m_grid1->SetCellValue(index, (4 + *_argIndex), wx_value);
	(*_argIndex)++;
}

void RadarCommand::ShowArgument(int index, std::string value, int *_argIndex)
{
	wxString wx_value(value);
	m_grid1->SetCellValue(index, (4 + *_argIndex), wx_value);
	(*_argIndex)++;
}

void RadarCommand::AllClearFunction()
{
	m_grid1->ClearGrid();
	for (int i = 0; i < 1; i++){
		m_grid1->SetCellBackgroundColour(i, 0, wxNullColour);
		m_grid1->SetCellBackgroundColour(i, 1, wxNullColour);
	}
	InitialRadarCommandGrid();
}
