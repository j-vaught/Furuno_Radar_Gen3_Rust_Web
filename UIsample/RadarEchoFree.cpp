/**
 * @file   RadarEcho.cpp
 *
 * @brief
 * Window"RadarEcho" of DllSample5
 *
 */

#include "RadarConnection.h"
#include "Api.h"
#include "RadarEchoFree.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <sys/stat.h>
#include <sys/types.h>
#include "EchoServer.h"

#ifdef __linux
typedef unsigned char byte;
#endif

wxDEFINE_EVENT(wxEVT_COMMAND_MYTHREAD_UPDATE, wxThreadEvent);

//------------------------------------------------
//  Global variable definition
//------------------------------------------------
bool g_dualMode = false;
bool g_displaymode_changed = false;
int32_t g_targetRadarNo;

wxImage g_GetData0;
wxImage g_GetData1;
int g_EchoNum = 0;
int dispOn = 0;
int writeData = 0; //Default off
bool g_GetData0Done = false;
bool g_GetData1Done = false;
Furuno::INT16 g_range_for_draw0 = 0;
Furuno::INT16 g_range_for_draw1 = 0;
std::mutex g_ringBufferLock0;
std::mutex g_ringBufferLock1;
std::mutex g_echoInfoLock0;
std::mutex g_echoInfoLock1;

static Furuno::INT16 DotX_Table[Define_LINE90DEG][Define_ECHO_RADIUS];
static Furuno::INT16 DotY_Table[Define_LINE90DEG][Define_ECHO_RADIUS];
static Furuno::INT32 scale_for_draw0 = 884;
static Furuno::INT32 scale_for_draw1 = 884;
static Furuno::INT32 DisplayMode[2] = {0, 0};
static Furuno::UINT8 ImageBuf_for_draw[2][Define_DISP_SIZE_X * Define_DISP_SIZE_Y * 3];
static Furuno::UINT8 ImageBuf_for_tmp[2][Define_DISP_SIZE_X * Define_DISP_SIZE_Y * 3];

//------------------------------------------------
//  Initialize member variables
//------------------------------------------------
RingBufferInfo_s RadarEcho::m_ringBufferInfo0{0, 0, 0, 0};
RingBufferInfo_s RadarEcho::m_ringBufferInfo1{0, 0, 0, 0};
unsigned int RadarEcho::m_packetReceiveCount0 = 0;
unsigned int RadarEcho::m_packetReceiveCount1 = 0;
std::array<std::array<uint8_t, MAX_SWEEP_LENGTH>, MAX_SWEEP_STORE> RadarEcho::m_echoSweepBufferForData0;
std::array<std::array<uint8_t, MAX_SWEEP_LENGTH>, MAX_SWEEP_STORE> RadarEcho::m_echoSweepBufferForData1;
std::array<EchoInfo_s, MAX_SWEEP_STORE> RadarEcho::m_echoSweepBufferForAddInfo0;
std::array<EchoInfo_s, MAX_SWEEP_STORE> RadarEcho::m_echoSweepBufferForAddInfo1;

///////////////////////////////////////////////////////////////////////////

RadarEcho::RadarEcho(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &pos, const wxSize &size, long style) : wxFrame(parent, id, title, pos, size, style)
{
	wxInitAllImageHandlers();
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);

	wxBoxSizer *bSizer1;
	bSizer1 = new wxBoxSizer(wxHORIZONTAL);

	wxBoxSizer *bSizer2;
	bSizer2 = new wxBoxSizer(wxVERTICAL);

	wxGridSizer *gSizer1;
	gSizer1 = new wxGridSizer(6, 2, 0, 0);

	m_staticText1 = new wxStaticText(this, wxID_ANY, wxT("Radar No"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText1->Wrap(-1);
	gSizer1->Add(m_staticText1, 0, wxALL, 5);

	m_comboBox1 = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0);
	gSizer1->Add(m_comboBox1, 0, wxALL, 5);

	m_staticText2 = new wxStaticText(this, wxID_ANY, wxT("Display Echo"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText2->Wrap(-1);
	gSizer1->Add(m_staticText2, 0, wxALL, 5);

	m_comboBox2 = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0);
	gSizer1->Add(m_comboBox2, 0, wxALL, 5);
	m_comboBox2->Append(wxT("OFF"));
	m_comboBox2->Append(wxT("ON"));

	m_staticText3 = new wxStaticText(this, wxID_ANY, wxT("Write Data to File"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText3->Wrap(-1);
	gSizer1->Add(m_staticText3, 0, wxALL, 5);

	m_comboBox3 = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0);
	gSizer1->Add(m_comboBox3, 0, wxALL, 5);
	m_comboBox3->Append(wxT("OFF"));
	m_comboBox3->Append(wxT("ON"));

	m_staticText4 = new wxStaticText(this, wxID_ANY, wxT("Set sweep"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText4->Wrap(-1);
	gSizer1->Add(m_staticText4, 0, wxALL, 5);

	m_comboBox4 = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0);
	gSizer1->Add(m_comboBox4, 0, wxALL, 5);
	m_comboBox4->Append(wxT("2048"));
	m_comboBox4->Append(wxT("4096"));
	m_comboBox4->Append(wxT("8192"));

	m_staticText5 = new wxStaticText(this, wxID_ANY, wxT("Display Mode"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText5->Wrap(-1);
	gSizer1->Add(m_staticText5, 0, wxALL, 5);

	m_comboBox5 = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, 0);
	gSizer1->Add(m_comboBox5, 0, wxALL, 5);
	m_comboBox5->Append(wxT("NORTH UP"));
	m_comboBox5->Append(wxT("HEAD UP"));
	bSizer2->Add(gSizer1, 0, wxEXPAND, 5);

	bSizer1->Add(bSizer2, 0, wxEXPAND, 5);

	canvas1 = new RadarEchoCanvas(this);
	canvas1->GetRadarEchoUI(this);
	bSizer1->Add(canvas1, 1, wxALL | wxEXPAND, 5);

	this->SetSizer(bSizer1);
	this->Layout();

	this->Centre(wxBOTH);

	m_comboBox1->Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &RadarEcho::m_comboBox1OnCombobox, this);
	m_comboBox2->Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &RadarEcho::m_comboBox2OnCombobox, this);
	m_comboBox3->Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &RadarEcho::m_comboBox3OnCombobox, this);
	m_comboBox4->Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &RadarEcho::m_comboBox4OnCombobox, this);
	m_comboBox5->Bind(wxEVT_COMMAND_COMBOBOX_SELECTED, &RadarEcho::m_comboBox5OnCombobox, this);
	Bind(wxEVT_COMMAND_MYTHREAD_UPDATE, &RadarEcho::OnThreadUpdate, this);
	Bind(wxEVT_CLOSE_WINDOW, &RadarEcho::OnClose, this);

	MakeDotTable();
	// runs data collection and sweep settings on startup
	Furuno::RmSetEchoSweepTotal(2);		 // 2 is 8192, 1 is 4096, 0 is 2048
	Furuno::RmcSetTxStby(0, 2, 0, 0, 0); // radar #, operation mode(2 is on), other stuff.
	// Create EchoServer instance
	echoServer = new EchoServer(8081); // Use desired port number

	// Provide image getter function
	echoServer->setImageGetter([this]() -> std::string {
		// Save the current radar echo image to a file
		std::string imagePath = "radar_echo.png";
		{
			std::lock_guard<std::mutex> lock(imageMutex_);
			g_GetData0.SaveFile(imagePath, wxBITMAP_TYPE_PNG);
		}
		return imagePath;
	});

	// Provide command handler function
	echoServer->setCommandHandler([this](const std::string& command, const std::string& value) {
		if (command == "display_echo") {
			// Toggle Display Echo
			dispOn = (dispOn == 1) ? 0 : 1;
		} else if (command == "write_to_file") {
			// Toggle Write to File
			writeData = (writeData == 1) ? 0 : 1;
		} else if (command == "set_sweep_length") {
			// Set Sweep Length
			int sweepValue = std::stoi(value);
			Furuno::RmSetEchoSweepTotal(static_cast<short>(sweepValue));
		} else if (command == "display_mode") {
			// Set Display Mode
			if (value == "north_up")
				DisplayMode[g_targetRadarNo] = 1;
			else if (value == "head_up")
				DisplayMode[g_targetRadarNo] = 0;
		}
	});
	// Inside RadarEcho constructor, after setting command handler
	echoServer->setStatusGetter([this]() -> std::pair<bool, bool> {
		return {dispOn == 1, writeData == 1};
	});
	// Start the server
	echoServer->start();


}

RadarEcho::~RadarEcho()
{
	m_comboBox1->Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, &RadarEcho::m_comboBox1OnCombobox, this);
	m_comboBox2->Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, &RadarEcho::m_comboBox2OnCombobox, this);
	m_comboBox3->Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, &RadarEcho::m_comboBox3OnCombobox, this);
	m_comboBox4->Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, &RadarEcho::m_comboBox4OnCombobox, this);
	m_comboBox5->Unbind(wxEVT_COMMAND_COMBOBOX_SELECTED, &RadarEcho::m_comboBox5OnCombobox, this);
	Unbind(wxEVT_COMMAND_MYTHREAD_UPDATE, &RadarEcho::OnThreadUpdate, this);
	Unbind(wxEVT_CLOSE_WINDOW, &RadarEcho::OnClose, this);
	delete canvas1;
	delete m_staticText1;
	delete m_staticText2;
	delete m_comboBox1;
	delete m_comboBox2;
	delete m_staticText3;
	delete m_comboBox3;
	delete m_staticText4;
	delete m_comboBox4;
	delete m_staticText5;
	delete m_comboBox5;
	Furuno::RmcSetTxStby(0, 1, 0, 0, 0); // radar #, operation mode(1 is stdby), other stuff.
	
	if (echoServer) {
        echoServer->stop();
        delete echoServer;
    }

}

bool RadarEcho::PrepareMakeImageBuf(Furuno::INT32 radarNo, Furuno::INT16 status, Furuno::UINT8 *echo, Furuno::INT16 range, Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg)
{
	// Initialize control flags to manage drawing and data processing states.
	bool draw = false;		 // Flag to indicate whether to draw something on the screen.
	bool clear_flag = false; // Flag to indicate whether to clear the drawing on the screen.
	bool copy_flag = false;	 // Flag to indicate whether to copy data for drawing.
	bool ret = false;		 // A generic return flag, potentially indicating success or failure of a function.

	// Static arrays to store various statuses and counts for processing radar echoes.
	// "Static" means these variables keep their value between function calls.
	static Furuno::INT32 IntpEchoDraw[2] = {1, 1};		 // Flags to indicate if interpolated echo should be drawn, for each radar.
	static Furuno::INT32 RcvEchoDraw[2] = {1, 1};		 // Flags to indicate if received echo should be drawn, for each radar.
	static Furuno::INT32 ReceiveCount[2] = {0, 0};		 // Counts of received echoes, for each radar.
	static Furuno::INT32 ReceiveDraw[2] = {0, 0};		 // Counts to be used for drawing received echoes, for each radar.
	static Furuno::INT32 InterpolationCount[2] = {0, 0}; // Counts of interpolated echoes, for each radar.
	static Furuno::INT32 InterpolationDraw[2] = {0, 0};	 // Counts to be used for drawing interpolated echoes, for each radar.
	static Furuno::INT32 oldRange[2] = {-1, -1};		 // Previous range values for echoes, to detect changes in range.
	static Furuno::INT32 oldAngle[2] = {-1, -1};		 // Previous angle values for echoes, to detect changes in angle.

	// Variables to manage file writing based on radar number.
	static int fileCounter[2] = {0, 0}; // Counter for files created for each radar.

	// Check if the current radar is one of the two we are interested in (0 or 1).
	if (radarNo == 0 || radarNo == 1)
	{
		// If the current angle is less than the old angle, less than 50, and the old angle was greater than 8100,
		// this indicates a reset or wrap-around scenario for the angle measurement.
		if (angle < oldAngle[radarNo] && angle < 50 && oldAngle[radarNo] > 8100)
		{
			// Update drawing counts and reset the receive and interpolation counts for this radar.
			ReceiveDraw[radarNo] = ReceiveCount[radarNo];
			InterpolationDraw[radarNo] = InterpolationCount[radarNo];
			ReceiveCount[radarNo] = 0;
			InterpolationCount[radarNo] = 0;
			copy_flag = true; // Indicate that data is ready to be copied for drawing.
		}

		// If receiving echo drawing is enabled for this radar, increment the receive count and set the draw flag.
		if (RcvEchoDraw[radarNo] == 1)
		{
			draw = true;
			ReceiveCount[radarNo]++;
		}

		// If the status is 4 (a specific condition) and interpolation echo drawing is enabled, do the same for interpolation count.
		if ((status == 4) && (IntpEchoDraw[radarNo] == 1))
		{
			draw = true;
			InterpolationCount[radarNo]++;
		}

		// If the range has changed from the last recorded range, update the range and set the flag to clear the screen.
		if (oldRange[radarNo] != range)
		{
			oldRange[radarNo] = range;
			clear_flag = true;
		}
		// Update the old angle with the current angle.
		oldAngle[radarNo] = angle; // Check if the current display mode for this radar is set to 1.
		if (DisplayMode[radarNo] == 1)
		{
			// If the display mode has not been marked as changed yet, set the flag to clear the screen.
			// This is because a change in display mode likely requires a fresh drawing.
			if (g_displaymode_changed == false)
			{
				clear_flag = true;			  // Indicate that the screen needs to be cleared.
				g_displaymode_changed = true; // Mark that the display mode has been changed.
			}

			// If the heading flag is not zero, adjust the radar's angle by adding the heading.
			// This could be to align the radar data with the current heading of the ship.
			if (h_flg != 0)
			{
				angle = (short)(angle + heading); // Adjust angle based on the heading.
				// If the adjusted angle exceeds the maximum (360 degrees, assuming Define_LINE360DEG represents this),
				// wrap it around by subtracting the maximum. This ensures the angle stays within valid bounds.
				if (angle >= Define_LINE360DEG)
				{
					angle -= Define_LINE360DEG;
				}
			}
		}

		// Check if the current display mode for this radar is set to 0.
		if (DisplayMode[radarNo] == 0)
		{
			// If the display mode was previously changed (i.e., not in its initial state),
			// and we're now setting it back to 0, clear the screen and reset the changed flag.
			// This suggests toggling between two types of display modes, each requiring its own drawing setup.
			if (g_displaymode_changed == true)
			{
				clear_flag = true;			   // Indicate that the screen needs to be cleared due to the mode change.
				g_displaymode_changed = false; // Reset the flag as the display mode is now back to its original state.
			}
		}
		// If the clear flag is set, clear the temporary image buffer for this radar by setting all pixel values to 0.
		if (clear_flag)
		{
			for (int i = 0; i < Define_DISP_SIZE_X * Define_DISP_SIZE_Y; i++)
			{
				ImageBuf_for_tmp[radarNo][i * 3 + 0] = 0; // Clear red component.
				ImageBuf_for_tmp[radarNo][i * 3 + 1] = 0; // Clear green component.
				ImageBuf_for_tmp[radarNo][i * 3 + 2] = 0; // Clear blue component.
			}

			// After clearing, reset the clear flag to indicate that the screen has been cleared.
			clear_flag = false;
		}
		if (draw)
		{
			if (copy_flag)
			{
				memcpy(ImageBuf_for_draw[radarNo], ImageBuf_for_tmp[radarNo], sizeof(ImageBuf_for_tmp[radarNo]));
				memset(ImageBuf_for_tmp[radarNo], 0, sizeof(ImageBuf_for_tmp[radarNo]));
				copy_flag = false;
				ret = true;
			}
			MakeImageBuf(radarNo, echo, angle);

			if (writeData == 1)
			{
				// Static variable to keep track of the previous angle
				static Furuno::INT16 previousAngle[2] = {0, 0};

				// Static variables to keep track of the current file stream and counter
				static std::ofstream outFile[2];
				static int currentFileCounter[2] = {-1, -1};

				// If angle has reset (crossed 8192), increment the file counter and close the current file
				if (angle < previousAngle[radarNo])
				{
					if (outFile[radarNo].is_open())
					{
						outFile[radarNo].close();
					}
					fileCounter[radarNo]++;
				}

				// Update the previous angle
				previousAngle[radarNo] = angle;

				// If the file counter has changed or the file is not open, open a new file
				if (currentFileCounter[radarNo] != fileCounter[radarNo] || !outFile[radarNo].is_open())
				{
					if (outFile[radarNo].is_open())
					{
						outFile[radarNo].close();
					}

					mkdir("data", 0777); // Ensure the "data" directory exists

					std::ostringstream filename;
					filename << "data/" << "radar" << radarNo << "_file" << fileCounter[radarNo] << ".bin";

					outFile[radarNo].open(filename.str(), std::ios::binary | std::ios::app);
					if (!outFile[radarNo])
					{
						std::cerr << "Error opening file for writing: " << filename.str() << std::endl;
						return ret;
					}

					currentFileCounter[radarNo] = fileCounter[radarNo];
				}

				// Write the angle and range variables to the file as UINT16
				Furuno::UINT16 angleToWrite = static_cast<Furuno::UINT16>(angle);
				Furuno::UINT16 rangeToWrite = static_cast<Furuno::UINT16>(range);

				outFile[radarNo].write(reinterpret_cast<const char *>(&angleToWrite), sizeof(angleToWrite));
				outFile[radarNo].write(reinterpret_cast<const char *>(&rangeToWrite), sizeof(rangeToWrite));

				// Write the echo data to the binary file
				outFile[radarNo].write(reinterpret_cast<const char *>(echo), Define_ECHO_RADIUS);
			}
		}
	}

	return (ret);
}

void RadarEcho::MakeDotTable()
{
	short x, y;
	int r, ang;
	double dat;

	for (ang = 0; ang < Define_LINE90DEG; ang++)
	{
		dat = (double)(Define_ANGLE * ang);
		for (r = 0; r < Define_ECHO_RADIUS; r++)
		{
			x = (short)((double)(r * sin((Define_PAI * dat) / 180.0)) + 0.5);
			DotX_Table[ang][r] = x;
		}
	}
	for (ang = 0; ang < Define_LINE90DEG; ang++)
	{
		dat = (double)(Define_ANGLE * ang);
		for (r = 0; r < Define_ECHO_RADIUS; r++)
		{
			y = (short)((double)(r * cos((Define_PAI * dat) / 180.0)) + 0.5);
			DotY_Table[ang][r] = y;
		}
	}
}

bool RadarEcho::MakeImageBuf(Furuno::INT32 radarNo, Furuno::UINT8 *echo, Furuno::INT16 ang)
{
	int r, put_x, put_y;
	int angle;
	int color_flag = 1;
	int num;
	static Furuno::INT32 CircleRadius = Define_ECHO_RADIUS - 10;
	static Furuno::INT32 CenterPosX[2] = {Define_CENTER_X, Define_CENTER_X};
	static Furuno::INT32 CenterPosY[2] = {Define_CENTER_Y, Define_CENTER_Y};

	if ((0 <= ang) && (ang < Define_LINE90DEG))
	{
		angle = ang;
		for (r = 0; r < CircleRadius; r++)
		{
			put_x = CenterPosX[radarNo] + DotX_Table[angle][r];
			put_y = CenterPosY[radarNo] + DotY_Table[angle][r];
			if ((put_x < Define_DISP_SIZE_X) && (put_y < Define_DISP_SIZE_Y))
			{
				num = ((Define_DISP_SIZE_Y - 1 - put_y) * Define_DISP_SIZE_X + put_x) * 3;
				ImageBuf_for_tmp[radarNo][num + color_flag] = echo[r];
			}
			else
			{
				return true;
			}
		}
		return true;
	}

	if ((Define_LINE90DEG <= ang) && (ang < Define_LINE180DEG))
	{
		angle = ang - Define_LINE90DEG;
		for (r = 0; r < CircleRadius; r++)
		{
			put_x = CenterPosX[radarNo] + DotY_Table[angle][r];
			put_y = CenterPosY[radarNo] - DotX_Table[angle][r];
			if ((put_x < Define_DISP_SIZE_X) && (put_y > 0))
			{
				num = ((Define_DISP_SIZE_Y - 1 - put_y) * Define_DISP_SIZE_X + put_x) * 3;
				ImageBuf_for_tmp[radarNo][num + color_flag] = echo[r];
			}
			else
			{
				return true;
			}
		}
		return true;
	}

	if ((Define_LINE180DEG <= ang) && (ang < Define_LINE270DEG))
	{
		angle = ang - Define_LINE180DEG;
		for (r = 0; r < CircleRadius; r++)
		{
			put_x = CenterPosX[radarNo] - DotX_Table[angle][r];
			put_y = CenterPosY[radarNo] - DotY_Table[angle][r];
			if ((put_x > 0) && (put_y > 0))
			{
				num = ((Define_DISP_SIZE_Y - 1 - put_y) * Define_DISP_SIZE_X + put_x) * 3;
				ImageBuf_for_tmp[radarNo][num + color_flag] = echo[r];
			}
			else
			{
				return true;
			}
		}
		return true;
	}

	if ((Define_LINE270DEG <= ang) && (ang < Define_LINE360DEG))
	{
		angle = ang - Define_LINE270DEG;
		for (r = 0; r < CircleRadius; r++)
		{
			put_x = CenterPosX[radarNo] - DotY_Table[angle][r];
			put_y = CenterPosY[radarNo] + DotX_Table[angle][r];
			if ((put_x > 0) && (put_y < Define_DISP_SIZE_Y))
			{
				num = ((Define_DISP_SIZE_Y - 1 - put_y) * Define_DISP_SIZE_X + put_x) * 3;
				ImageBuf_for_tmp[radarNo][num + color_flag] = echo[r];
			}
			else
			{
				return true;
			}
		}
		return true;
	}
	return false;
}

void RadarEcho::m_comboBox1OnCombobox(wxCommandEvent &event) //"Radar No"
{
	g_targetRadarNo = m_comboBox1->GetSelection();
}

void RadarEcho::m_comboBox2OnCombobox(wxCommandEvent &event) //"Show Echo"
{
	dispOn = m_comboBox2->GetSelection();
	// if 1, turn echo on. if 0, turn off. default is off
}

void RadarEcho::m_comboBox3OnCombobox(wxCommandEvent &event) //"write data"
{
	writeData = m_comboBox3->GetSelection();
}

void RadarEcho::m_comboBox4OnCombobox(wxCommandEvent &event) //"Set sweep"
{
	Furuno::RmSetEchoSweepTotal((short)m_comboBox4->GetSelection());
}

void RadarEcho::m_comboBox5OnCombobox(wxCommandEvent &event) //"Display Mode"
{
	int wx_DisplayMode = m_comboBox5->GetSelection();
	if (g_targetRadarNo == 0 || g_targetRadarNo == 1)
	{
		if (wx_DisplayMode == 0)
		{
			DisplayMode[g_targetRadarNo] = 1; //"NORTH UP"
		}
		else
		{
			DisplayMode[g_targetRadarNo] = 0; //"HEAD UP"
		}
	}
}

void RadarEcho::AddRadarNoToCombobox(wxComboBox* comboBox)
{
    comboBox->Append(wxT("0"));
    comboBox->Append(wxT("1"));
}

// ----------------------------------------------------------------------------
// RadarEchoCanvas
// ----------------------------------------------------------------------------

RadarEchoCanvas::RadarEchoCanvas(RadarEcho *parent)
	: wxPanel(parent)
{

	owner = parent;

	Bind(wxEVT_PAINT, &RadarEchoCanvas::OnPaint, this);
}

RadarEchoCanvas::~RadarEchoCanvas()
{
	Unbind(wxEVT_PAINT, &RadarEchoCanvas::OnPaint, this);
}

void RadarEchoCanvas::OnPaint(wxPaintEvent &WXUNUSED(event))
{

	// Get the device context for drawing
	wxPaintDC dc(this);

	if (dispOn != 1)
	{
		// Fill the background with black color
		dc.SetBackground(*wxBLACK_BRUSH);
		dc.Clear();

		// Draw a simple message to indicate no drawing
		dc.SetTextForeground(*wxWHITE);
		dc.DrawText(wxT("Echo Display OFF. To view, set to ON"), 10, 10);
	}
	else if(dispOn == 1)
	{
		int i = 1;
		int circle_center_x = WINDOW_WIDTH / 2;
		int circle_center_y = WINDOW_HEIGHT / 2;
		;
		double NMRANGE = 0.0;
		double DRADIUS = 0.0;
		double scale_of_image0 = 0.0;
		double scale_of_image1 = 0.0;

		if (InitialOnPaint == true)
		{
			m_radarui->setCurrentSettingToComponent();
			InitialOnPaint = false;
		}

		wxPaintDC dc(this);

		if (g_GetData0Done == true)
		{
			{
				std::lock_guard<std::mutex> lock(g_echoInfoLock0);
				radarRangeNumberToNM(g_range_for_draw0, &NMRANGE, &DRADIUS);
				scale_of_image0 = (double)(WINDOW_WIDTH) / (2 * (scale_for_draw0 + MARGIN_OF_IMAGE));

				dc.DrawBitmap(g_GetData0, 0, 0, false);

				dc.SetPen(wxcyan);
				dc.DrawLine(circle_center_x, circle_center_y, circle_center_x, (circle_center_y - (scale_for_draw0 * scale_of_image0)));

				dc.SetPen(wxgreen);
				dc.SetBrush(*wxTRANSPARENT_BRUSH);
				for (i = 1; i <= (int)(NMRANGE / DRADIUS); i++)
				{
					dc.DrawCircle(circle_center_x, circle_center_y, i * (scale_for_draw0 * scale_of_image0) / (NMRANGE / DRADIUS));
				}
			}
		}
	}
}

void RadarEcho::DoStartThread()
{
	m_pThread = new RadarEchoThread(this);

	if (m_pThread->Run() != wxTHREAD_NO_ERROR)
	{
		wxLogError("Can't start thread!");
		delete m_pThread;
		m_pThread = NULL;
	}
}

void RadarEcho::OnThreadUpdate(wxThreadEvent &event)
{
    g_EchoNum = event.GetInt();

    if (g_EchoNum == 0) {
        g_GetData0 = event.GetPayload<wxImage>();
        g_GetData0Done = true;

        // Save image for web server
        {
            std::lock_guard<std::mutex> lock(imageMutex_);
            if (!g_GetData0.SaveFile("radar_echo.png", wxBITMAP_TYPE_PNG)) {
                wxLogError("Failed to save radar_echo.png");
            }
        }
    }
    else if (g_EchoNum == 1) {
        g_GetData1 = event.GetPayload<wxImage>();
        g_GetData1Done = true;
    }
    else {
        wxLogDebug("Neither radar number is 0 nor 1");
    }

    Refresh();
}


void RadarEcho::DoPauseThread()
{
	wxCriticalSectionLocker enter(m_pThreadCS);

	if (m_pThread)
	{
		if (m_pThread->Pause() != wxTHREAD_NO_ERROR)
			wxLogError("Can't pause the thread!");
	}
}

wxThread::ExitCode RadarEchoThread::Entry()
{
	wxThreadEvent *event3 = new wxThreadEvent(wxEVT_COMMAND_MYTHREAD_UPDATE);

	readEchoDataStatus_t textureUploadStatus;
	std::array<Furuno::UINT8, MAX_SWEEP_LENGTH> echo_read;
	EchoInfo_s echoInfo_read;

	Furuno::INT32 radarNo = 0;
	Furuno::INT16 status = 0;
	Furuno::UINT8 echo[MAX_SWEEP_LENGTH];
	Furuno::INT16 sweep_len = 0;
	Furuno::INT16 scale = 0;
	Furuno::INT16 range = 0;
	Furuno::INT16 angle = 0;
	Furuno::INT16 heading = 0;
	Furuno::INT16 h_flg = 0;

	int i = 0;

	while (!TestDestroy())
	{
		radarNo = 0;
		textureUploadStatus = readEchoDataStatus_t::NoSweep;
		textureUploadStatus = RadarEcho::readEchoData(radarNo, echo_read, echoInfo_read);

		if (textureUploadStatus == readEchoDataStatus_t::OK)
		{

			status = echoInfo_read.status;
			sweep_len = echoInfo_read.sweepLen;
			scale = echoInfo_read.scale;
			range = echoInfo_read.range;
			angle = echoInfo_read.angle;
			heading = echoInfo_read.heading;
			h_flg = echoInfo_read.headingFlg;

			memset(echo, 0, sizeof(echo));

			for (i = 0; i < scale; i++)
			{
				echo[i] = (Furuno::UINT8)echo_read[i];
			}
			bool ret = RadarEcho::PrepareMakeImageBuf(radarNo, status, echo, range, angle, heading, h_flg);

			if (ret)
			{
				wxImage image0(Define_DISP_SIZE_X, Define_DISP_SIZE_Y, (unsigned char *)ImageBuf_for_draw[0], true);

				wxRect ClipRect0;
				ClipRect0.x = Define_ECHO_RADIUS - (scale + MARGIN_OF_IMAGE);
				ClipRect0.y = Define_ECHO_RADIUS - (scale + MARGIN_OF_IMAGE);
				ClipRect0.width = 2 * (scale + MARGIN_OF_IMAGE);
				ClipRect0.height = 2 * (scale + MARGIN_OF_IMAGE);
				wxImage image0_converted = image0.GetSubImage(ClipRect0);
				image0_converted.Rescale(WINDOW_WIDTH, WINDOW_HEIGHT);

				{
					std::lock_guard<std::mutex> lock(g_echoInfoLock0);
					g_range_for_draw0 = range;
					scale_for_draw0 = scale;
				}

				event3->SetInt(0);
				event3->SetPayload(image0_converted);
				wxQueueEvent(m_pHandler, event3->Clone());
			}
		}
	}

	return (wxThread::ExitCode)0; // success
}

RadarEchoThread::~RadarEchoThread()
{
	wxCriticalSectionLocker enter(m_pHandler->m_pThreadCS);
	m_pHandler->m_pThread = NULL;
}

void RadarEcho::OnClose(wxCloseEvent &event)
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
			if (!m_pThread)
				break;
		}

		wxThread::This()->Sleep(1);
	}

	Destroy();
}

void RadarEchoCanvas::GetRadarEchoUI(RadarEcho *radarechoui)
{
	m_radarui = radarechoui;
}

void RadarEcho::setCurrentSettingToComponent()
{

	AddRadarNoToCombobox(m_comboBox1);
	m_comboBox1->SetSelection(0);
	g_targetRadarNo = m_comboBox1->GetSelection();

	if (g_targetRadarNo == 0 || g_targetRadarNo == 1)
	{
		if (DisplayMode[g_targetRadarNo] == 1)
		{
			m_comboBox5->SetSelection(0);
		}
		else
		{
			m_comboBox5->SetSelection(1);
		}
	}
}

Furuno::UINT16 Calculate_Remainder(Furuno::UINT16 dividend, Furuno::UINT16 divisor)
{

	if (dividend >= divisor)
	{
		dividend = (Furuno::UINT16)(dividend - divisor);
	}
	return dividend;
}

setEchoDataStatus_t RadarEcho::storeSweep(Furuno::INT32 radarno, Furuno::INT16 status, Furuno::UINT8 *echo, Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 range, Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg)
{
	if (echo == nullptr)
	{
		return RdlError_InvalidArgument;
	}
	setEchoDataStatus_t ret = RdlError_OK;

	static const unsigned int RECEIVE_COUNT = 50;

	if (radarno == 0)
	{
		if (m_packetReceiveCount0 < RECEIVE_COUNT)
		{
			m_packetReceiveCount0++;
		}
		else
		{
			writeEchoData(radarno, status, echo, sweep_len, scale, range, angle, heading, h_flg);
		}
	}
	else
	{
		ret = RdlError_InvalidArgument;
	}

	return ret;
}

bool RadarEcho::writeEchoData(Furuno::INT32 radarno, Furuno::INT16 status, Furuno::UINT8 *echo, Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 range, Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg)
{
	bool ret = true;

	if (radarno == 0)
	{
		{
			std::lock_guard<std::mutex> lock(g_ringBufferLock0);

			if (Calculate_Remainder((Furuno::UINT16)(m_ringBufferInfo0.writePoint + 1), MAX_SWEEP_STORE) == m_ringBufferInfo0.readPoint)
			{
				ret = false;
			}

			else
			{
				m_echoSweepBufferForData0[m_ringBufferInfo0.writePoint].fill(0);
				memcpy(&(m_echoSweepBufferForData0[m_ringBufferInfo0.writePoint]), echo, sweep_len);

				m_echoSweepBufferForAddInfo0[m_ringBufferInfo0.writePoint] = EchoInfo_s{status, sweep_len, scale, range, angle, heading, h_flg, false};
				m_ringBufferInfo0.writePoint = Calculate_Remainder((Furuno::UINT16)(m_ringBufferInfo0.writePoint + 1), MAX_SWEEP_STORE);

				ret = true;
			}
		}
	}
	else
	{
		ret = false;
	}
	return ret;
}

readEchoDataStatus_t RadarEcho::readEchoData(Furuno::INT32 radarno, std::array<uint8_t, MAX_SWEEP_LENGTH> &outputEcho, EchoInfo_s &outputEchoInfo)
{
	readEchoDataStatus_t ret = readEchoDataStatus_t::OK;

	if (radarno == 0)
	{
		{
			std::lock_guard<std::mutex> lock(g_ringBufferLock0);

			if (m_ringBufferInfo0.readPoint == m_ringBufferInfo0.writePoint)
			{
				ret = readEchoDataStatus_t::NoSweep;
			}
			else
			{
				outputEcho = m_echoSweepBufferForData0[m_ringBufferInfo0.readPoint];
				outputEchoInfo = m_echoSweepBufferForAddInfo0[m_ringBufferInfo0.readPoint];

				m_ringBufferInfo0.readPoint = Calculate_Remainder((Furuno::UINT16)(m_ringBufferInfo0.readPoint + 1), MAX_SWEEP_STORE);
			}
		}
	}
	else
	{
		ret = readEchoDataStatus_t::InvalidArgument;
	}

	return ret;
}

void radarRangeNumberToNM(Furuno::INT16 range, double *nmrange, double *dradius) // List of Range code (radar.html)
{
	switch (range)
	{
	case 0:
		*nmrange = 0.125;
		*dradius = 0.025;
		break;
	case 1:
		*nmrange = 0.25;
		*dradius = 0.05;
		break;
	case 2:
		*nmrange = 0.5;
		*dradius = 0.1;
		break;
	case 3:
		*nmrange = 0.75;
		*dradius = 0.25;
		break;
	case 4:
		*nmrange = 1;
		*dradius = 0.25;
		break;
	case 5:
		*nmrange = 1.5;
		*dradius = 0.25;
		break;
	case 6:
		*nmrange = 2;
		*dradius = 0.5;
		break;
	case 7:
		*nmrange = 3;
		*dradius = 0.5;
		break;
	case 8:
		*nmrange = 4;
		*dradius = 1;
		break;
	case 9:
		*nmrange = 6;
		*dradius = 1;
		break;
	case 10:
		*nmrange = 8;
		*dradius = 2;
		break;
	case 11:
		*nmrange = 12;
		*dradius = 2;
		break;
	case 12:
		*nmrange = 16;
		*dradius = 4;
		break;
	case 13:
		*nmrange = 24;
		*dradius = 4;
		break;
	case 14:
		*nmrange = 32;
		*dradius = 8;
		break;
	case 15:
		*nmrange = 48;
		*dradius = 8;
		break;
	case 17:
		*nmrange = 96;
		*dradius = 16;
		break;
	default:
		*nmrange = 0;
		*dradius = 0;
		break;
	}
}
