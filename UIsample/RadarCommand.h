/**
* @file   RadarCommand.h
*
* @brief
* Window"RadarCommand" of DllSample5
*
*/

#ifndef DLLSAMPLE5_RADARCOMMAND_H
#define DLLSAMPLE5_RADARCOMMAND_H
#define FLM_COMPILE_NEW_LICENSE 1

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
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/grid.h>
#include <wx/frame.h>
#include <wx/event.h>


#include <map> // Add this line
#include <unordered_map>
#include <vector>
#include <string>
#include "CommandServer.h" // Include CommandServer header


//------------------------------------------------
//  Macro definition
//------------------------------------------------
#define NUM_OF_RadarFunctions (54)
#define LENGTH_OF_RadarFunctions (40)
#define RMC_FUNCNO_MAX  (33)

#define FREE_CMD "FreeCommand"
#define CONNECT "Connect"
#define DISPMODE "DispMode"
#define RANGE "Range"
#define GAIN "GAIN"
#define ACSEA "ACSEA"
#define ACRAIN "ACRAIN"
#define CUSTOMPICTURE_ALL "CustomPictureAll"
#define CUSTOMPICTURE "CustomPicture"
#define PULSE_WIDTH "PulseWidth"
#define TX_STBY "TxStby"
#define ARPA_SELECT_TARGET "ArpaSelectTarget"
#define ARPA_ACQUIRE_TARGET "ArpaAcquireTarget"
#define ARPA_CANCEL_TARGET "ArpaCancelTarget"
#define ARPA_DISPMODE "ArpaDispMode"
#define ANTENNA_TYPE "AntennaType"
#define KEY_CMD "KeyCommand"
#define GUARD_STATUS "GuardStatus"
#define LOST_ACK "LostAck"
#define ARPA_VECTOR "ArpaVector"
#define ARPA_CPATCPA "ArpaCpaTcpa"
#define TUNE "Tune"
#define TUNE_INDICATOR "TuneIndicator"
#define BLIND_SECTOR "BlindSector"
#define RADAR_POSITION "RadarPosition"
#define ARPA_TARGET_ALLCLEAR "ArpaTargetAllClear"
#define TUNE_ADJUST "TuneAdjust"
#define ARPA_CANCEL_TARGETID "ArpaCancelTargetId"
#define REFERENCE_POINT "ReferencePoint"
#define ALARM_STATUS "AlarmStatus"
#define HEADING "Heading"
#define SELECT_QVDISPLAY "SelectQVDisplay"
#define ATT "ATT"
#define HEADING_ADJUST "HeadingAdjust"
#define TIMING_ADJUST "TimingAdjust"
#define MBS_ADJUST "MBSAdjust"
#define ANTENNA_HEIGHT "AntennaHeight"
#define NEAR_STC "NearSTC"
#define MIDDLE_STC "MiddleSTC"
#define FAR_STC "FarSTC"
#define RING_SUPPRESSION "RingSuppression"
#define ANTENNA_REVOLUTION "AntennaRevolution"
#define ANTENNA_SWITCH "AntennaSwitch"
#define STOPPED_TX "StoppedTx"
#define MONITOR_SELECT "MonitorSelect"
#define ANTENNA_NO "AntennaNo"
#define ON_TIME "OnTime"
#define TX_TIME "TxTime"
#define PM_ADJUST "PmAdjust"
#define DEMO_ECHO "DemoEcho"
#define EAV_WITHOUT_GYRO "EavWithoutGyro"
#define SECONDECHO_REJECT "2ndEchoReject"
#define PM "PM"
#define SART "SART"
#define VERSION_NO "VersionNo"
#define ARPA_TEST "ArpaTest"
#define GUARD_MODE "GuardMode"
#define GUARDZONE1FAN "GuardFan(GuardZone1)"
#define GUARDZONE2FAN "GuardFan(GuardZone2)"
#define GUARD_POLYGON "GuardPolygon"
#define ARPA_WITHOUT_GYRO "ArpaWithoutGyro"
#define SPEED_SELECT "SpeedSelect"
#define DRIFT "Drift"
#define GC10 "GC10"
#define ARPA_CANCEL_STATUS "ArpaCancelStatus"
#define ARPA_PRESET "ArpaPreset"
#define NEXT_ECHO "NextEcho"
#define TRAIL_MODE "TrailMode"
#define TRAIL_CLEAR "TrailClear"
#define ARPA_CANCEL_TARGETNO "ArpaCancelTargetNo"
#define SWEEP_POSITION "SweepPosition"
#define ARPA_ACQUIRE_TARGET_DISPLAYID "ArpaAcquireTargetDisplayId"
#define ARPA_CANCEL_TARGET_DISPLAYID "ArpaCancelTargetDisplayId"
#define GPS_ANTENNA_POSITION "GpsAntennaPosition"
#define CONNING_POSITION "ConningPosition"
#define SELECT_ANTENNA "SelectAntenna"
#define WAKEUP_COUNT "WakeupCount"
#define SHIP_INFO "ShipInfo"
#define AIS_ALARM "AisAlarm"
#define ARPA_ALARM "ArpaAlarm"
#define SELECT_ALARMOUT "SelectAlarmOut"
#define ALARM_ACK "AlarmAck"
#define SELECT_CRT "SelectCrt"
#define SELECT_ALARM_POLARITY "SelectAlarmPolarity"
#define COMBINE_STATUS "CombineStatus"
#define HEADING_SOURCE "HeadingSource"
#define NETWORK_PRIORITY "NetworkPriority"
#define MENUSET "MenuSet"
#define IMOECHO_SWITCH "ImoEchoSwitch"
#define EXT_ALARM_ACK "ExtAlarmAck"
#define PM_ADJUST_END "PmAdjustEnd"
#define SELECT_TTMOUT "SelectTtmOut"
#define ARPA_MAXRANGE "ArpaMaxRange"
#define ARPA_ECHOLEVEL "ArpaEchoLevel"
#define ARPA_ALLCLEAR "ArpaAllClear"
#define ARPA_LANDSIZE "ArpaLandSize"
#define ARPA_ANTENNA "ArpaAntenna"
#define ARPA_ACQCORRE "ArpaAcqCorre"
#define ARPA_WEED "ArpaWeed"
#define ARPA_GATESIZE "ArpaGateSize"
#define ARPA_FILTERRESPONS "ArpaFilterRespons"
#define ARPA_LOSTCOUNT "ArpaLostCount"
#define ARPA_MAXSPEED "ArpaMaxSpeed"
#define ARPA_TIMEVECTOR "ArpaTimeVector"
#define OPERATOR_FITNESS "OperatorFitness"
#define PICTURE_DEFAULT "PictureDefault"
#define RFC_VERSIONNO "RfcVersionNo"
#define COMBINE_DATA "CombineData"
#define INTERSW_INFO "InterswInfo"
#define ARPA_LOSTFILTER_RANGE "ArpaLostFilterRange"
#define ARPA_LOSTFILTER_SPEED "ArpaLostFilterSpeed"
#define STC_RANGE "StcRange"
#define CUSTOM_MEMORY "CustomMemory"
#define BUILDUP_TIME "BuildUpTime"
#define DISPLAYUNIT_INFORMATION "DisplayUnitInformation"
#define ARPA_LOSTFILTERMODE "ArpaLostFilterMode"
#define ASSOCIATION_STATUS "AssociationStatus"
#define ECHO_CLEAR "EchoClear"
#define CANCEL_SELECT_TARGET "CancelSelectTarget"
#define CANCEL_SELECT_LOSTTARGET "CancelSelectLostTarget"
#define CUSTOM_ATFPARAMETER "CustomAtfParameter"
#define TRAIL_PROCESS "TrailProcess"
#define ARPA_REACQUIRE_TARGET "ArpaReAcquireTarget"
#define ALIVE_CHECK "AliveCheck"
#define TX_ECHOSELECT "TxEchoSelect"
#define RX_ECHOSELECT "RxEchoSelect"
#define RS_PRESET "RsPreset"
#define START_SELFMODE "StartSelfMode"
#define JUMMING_ABLE "JummingAble"
#define ICECLASS "IceClass"
#define ATFSETTINGS "AtfSettings"
#define RS_ALLCLEAR "RsAllClear"
#define SSD_TXCHANNEL "SsdTxChannel"
#define HIGH_SENSITIVITY_MODE "HighSensitivityMode"
#define BEARING_RESOLUTION_SELECT "BearingResolutionSelect"
#define ECHO_COLOR_TYPE "EchoColorType"
#define ACCUSHIP "AccuShip"
#define OBSERVATION_MODE "ObservationMode"
#define SART_SIGNAL "SARTSignal"
#define IMAGECOR "ImageCor"
#define NN3COMMAND "NN3Command"
#define MEMORY "Memory"
#define EQUIP_COMP "EquipComp"
#define PULSE_RANGE "PulseRange"
#define MAGNETRON "Magnetron"
#define MAGNETRON_IND "MagnetronInd"
#define ARPA_DETECT "ArpaDetect"
#define ARPA_CATCH "ArpaCatch"
#define ARPA_PURSUIT "ArpaPursuit"
#define RANGE_SELECT "RangeSelect"
#define VIDEO_LEVEL "VideoLevel"


///////////////////////////////////////////////////////////////////////////////
/// Class RadarCommand
// Struct to hold command descriptions and their parameters
struct CommandInfo {
    std::string description;
    std::map<std::string, std::string> parameterDescriptions; // paramName -> description
};

// Struct to hold parameter descriptions
struct ParameterInfo {
    std::string paramName;
    std::string description;
};
///////////////////////////////////////////////////////////////////////////////

/// Command parameter information structure
struct CmdParamInfo {
    std::string name;
    std::string type;
    std::string description;
};

class RadarCommand : public wxFrame
{
public:
    RadarCommand(wxWindow* parent = nullptr, wxWindowID id = wxID_ANY, const wxString& title = wxT("RadarCommand"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(900, 600), long style = wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL);

    ~RadarCommand();

    void InitialRadarCommand(); // Initializes the radar command interface
    void InitializeCommandParameters(); // Initializes command parameters

    // Expose necessary functions for CommandServer
    int getCommandIndex(const std::string& commandName) const;
    std::vector<std::string> getCommandList() const;
    std::vector<std::string> getRadarNumbers() const;

	void SetFunction(Furuno::INT32 radarNo, std::string command, int index);
	void GetFunction(Furuno::INT32 radarNo, std::string command, int index, bool isAll);

    // Expose the grid and combo box for parameter access
    wxGrid* m_grid1;
    wxComboBox* m_comboBox1;

    // Event handlers
    void m_button1OnButtonClick(wxCommandEvent& event);
    void m_button2OnButtonClick(wxCommandEvent& event); // "Clear"

	void GetAllCommands(); // Programmatic "Get All"


protected:
    // Event handlers...
    void OnGridCellSelected(wxGridEvent& event);
    void m_comboBox1OnCombobox(wxCommandEvent& event); // "Radar No"
    void m_grid1OnGridCellLeftClick(wxGridEvent& event);
    void OnCellValueChanging(wxGridEvent& event);
    void OnCellValueChanged(wxGridEvent& event);

private:
    wxStaticText* m_staticText1;
    wxButton* m_button1;
    wxButton* m_button2;

    // New member variable for information pane
    wxTextCtrl* m_infoText;

    // New member variable for command descriptions
    std::map<std::string, std::string> commandDescriptions;
    std::unordered_map<std::string, std::vector<CmdParamInfo>> commandParameters;

    // Helper functions
    void PopulateCommandDescriptions();
    void InitialRadarCommandGrid();
    void AllClearFunction();

    // Color definitions
    wxColor wxgrey = wxColour(wxT("LIGHT GREY"));
    wxColor wxred = wxColour(wxT("MEDIUM VIOLET RED"));
    wxColor wxblue = wxColour(wxT("MEDIUM TURQUOISE"));
    wxColor wxgreen = wxColour(wxT("GREEN"));

    // Argument handling functions
    void SetArgument(int index, Furuno::UINT8* value, int* _argIndex);
    void SetArgument(int index, Furuno::INT16* value, int* _argIndex);
    void SetArgument(int index, Furuno::UINT16* value, int* _argIndex);
    void SetArgument(int index, Furuno::INT32* value, int* _argIndex);
    void SetArgument(int index, Furuno::UINT32* value, int* _argIndex);
    void SetArgument(int index, std::string* value, int* _argIndex);
    void ShowArgument(int index, Furuno::UINT8 value, int* _argIndex);
    void ShowArgument(int index, Furuno::UINT8* value, int* _argIndex);
    void ShowArgument(int index, Furuno::INT16 value, int* _argIndex);
    void ShowArgument(int index, Furuno::UINT16 value, int* _argIndex);
    void ShowArgument(int index, Furuno::INT32 value, int* _argIndex);
    void ShowArgument(int index, Furuno::UINT32 value, int* _argIndex);
    void ShowArgument(int index, const std::string& value, int* _argIndex);
    void ShowArgument(int index, const std::string value, int* _argIndex);

	    // CommandServer instance
    std::unique_ptr<CommandServer> commandServer_; // Smart pointer for automatic memory management
    unsigned short serverPort_ = 8082;            // Default port (adjust as needed)
    std::mutex commandMutex_;                     // Mutex for thread safety


};

#endif //DLLSAMPLE5_RADARCOMMAND_H

