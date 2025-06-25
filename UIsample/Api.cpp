/**
* @file   Api.cpp
*
* @brief
* Api for DllSample5
*
*/

#include "RadarConnection.h"
#include "Api.h"
#include "RadarEchoFree.h"

Furuno::INT32 g_radarnum_max = 0;

void HexStringToBytes(const std::string strIn, unsigned char* &array, int& size)
{
	std::string str = strIn;

	int length = str.length();

	if (length % 2 == 1)
	{
		str = "0" + str;
		length++;
	}

	array = new unsigned char[length / 2];
	size = length / 2;

	std::stringstream sstr(str);
	for (int i = 0; i < size; i++)
	{
		char ch1 = 0;
		char ch2 = 0;
		sstr >> ch1 >> ch2;
		int dig1 = 0;
		int dig2 = 0;
		if (isdigit(ch1)) dig1 = ch1 - '0';
		else if (ch1 >= 'A' && ch1 <= 'F') dig1 = ch1 - 'A' + 10;
		else if (ch1 >= 'a' && ch1 <= 'f') dig1 = ch1 - 'a' + 10;
		else;
		if (isdigit(ch2)) dig2 = ch2 - '0';
		else if (ch2 >= 'A' && ch2 <= 'F') dig2 = ch2 - 'A' + 10;
		else if (ch2 >= 'a' && ch2 <= 'f') dig2 = ch2 - 'a' + 10;
		else;
		array[i] = (unsigned char)(dig1 * 16 + dig2);
	}
}

bool StartNetwork(std::string hostname, Furuno::UINT8 *ip_addr, Furuno::UINT8 *subnet)
{
	bool init_result = false;

	//List of function codes. I left them all at 0 because that's what happens in the navnet sample app
	unsigned int funcArray[18];
	memset(&funcArray[0], 0, sizeof(unsigned int) * Furuno::MaxFunctionCode);
	wxLogDebug("IP Address : %d:%d:%d:%d", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
	wxLogDebug("Subnet Mask : %d:%d:%d:%d", subnet[0], subnet[1], subnet[2], subnet[3]);

	int ret1 = Furuno::NcSetNetworkIpAddress(ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
	int ret2 = Furuno::NcInitialNavnetCommon((Furuno::UINT8 *)hostname.c_str(), (Furuno::INT32 *)&funcArray[0]);

	wxLogDebug("StartNetwork Init : %i, %i", ret1, ret2);
	std::this_thread::sleep_for(std::chrono::milliseconds(500)); //give FNET some time to start up (is there a way to verify when it has started up?)
	if (ret1 == 0 && ret2 == 0){
		init_result = true;
	}
	else{
		init_result = false;
	}
	return init_result;
}

bool StartRadarModule(void)
{
	bool init_result = false;

	int ret = Furuno::RmSetServerClientMode(0);

	int ret2 = Furuno::RmInitialModule((Furuno::UINT8*)"", NULL);

	int ret3 = Furuno::RmSetCommandCallbackFunc(CommandCallback);
	int ret4 = Furuno::RmSetCycleCallbackFunc(CycleCallback);
	int ret5 = Furuno::RmSetEchoCallbackFunc(EchoCallback);

	int ret6 = Furuno::RmSetEchoInterpolationFunc(1);

	int ret7 = Furuno::RmSetEchoSweepTotal(0);

	int ret8 = Furuno::RmSetTileEchoCallbackFunc(EchoTileCallback);

	wxLogDebug("RadarModule Init : %i, %i, %i, %i, %i, %i, %i, %i", ret, ret2, ret3, ret4, ret5, ret6, ret7, ret8);

	if (ret == 0 && ret2 == 0 && ret3 == 0 && ret4 == 0 && ret5 == 0 && ret6 == 0 && ret7 == 0 && ret8 == 0){
		init_result = true;
	}
	else{
		init_result = false;
	}
	return init_result;
}

RadarError FindRadar(RadarThread* thread, wxThreadEvent* event, Radar *m_pHandler)
{
	Furuno::INT32 count = 0;
	Furuno::UINT8 radarHostNameList[RADAR_HOSTNAME_MAX][RADAR_HOSTNAME_LENGTH];
	Furuno::UINT8 radarHostName[RADAR_HOSTNAME_LENGTH];
	std::string RadarHostMessage = "Find Radar : ";
	Furuno::INT32 ret = RADAR_PARAM_ERROR;
	Furuno::INT32 ret2 = RADAR_PARAM_ERROR;
	Furuno::INT32 ret3 = RADAR_PARAM_ERROR;
	Furuno::INT32 ret4 = RADAR_PARAM_ERROR;
	int i, j;

	for (i = 0; i < RADAR_WAITING_TIME; i++)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		ret = Furuno::RmGetSensorHostCount(&count);
		if (count > 0 && count <= RADAR_HOSTNAME_MAX)
		{
			if (!g_selected_dualrange){
				event->SetString("Dual range mode : OFF\n");
				wxQueueEvent(m_pHandler, event->Clone());
			}
			else{
				event->SetString("Dual range mode : ON\n");
				wxQueueEvent(m_pHandler, event->Clone());
			}

			for (j = 0; j < count; j++){
				ret2 = Furuno::RmGetSensorHostName(j, (Furuno::UINT8*)&radarHostNameList[j]);
				strcpy((char*)radarHostName, (const char*)radarHostNameList[j]);
				RadarHostMessage = RadarHostMessage + std::string((char*)radarHostName) + "\t";
			}
			RadarHostMessage = RadarHostMessage + "\n";
			event->SetString(RadarHostMessage);
			wxQueueEvent(m_pHandler, event->Clone());

			strcpy((char*)radarHostName, (const char*)radarHostNameList[0]);
			RadarHostMessage = "Connecting to " + std::string((char*)radarHostName) + "...";
			event->SetString(RadarHostMessage);
			wxQueueEvent(m_pHandler, event->Clone());
			if (!g_selected_dualrange){ //When display unit connect to one radar and use only one range data
				g_radarnum_max = 0;
				ret3 = Furuno::RmSetRadarSource(0, radarHostName, 0);

				if (ret != RADAR_OK || ret2 != RADAR_OK || ret3 != RADAR_OK){
					wxLogDebug("FindRadar : %i, %i, %i", ret, ret2, ret3);
					return AbnormalReturnValue;
				}
			}
			else{ //When display unit connect to one radar and use dual range
				g_radarnum_max = 1;
				ret3 = Furuno::RmSetRadarSource(0, radarHostName, 0);
				ret4 = Furuno::RmSetRadarSource(1, radarHostName, 1);

				if (ret != RADAR_OK || ret2 != RADAR_OK || ret3 != RADAR_OK || ret4 != RADAR_OK){
					wxLogDebug("FindRadar : %i, %i, %i, %i", ret, ret2, ret3, ret4);
					return AbnormalReturnValue;
				}
			}
			return RadarFound; //find radar
		}
		else if (count > RADAR_HOSTNAME_MAX){
			wxLogDebug("Please edit Api.h and increase RADAR_HOSTNAME_MAX");
			return NoRadar;
		}
		else{
			//
		}

		if (thread->TestDestroy()){
			return ThreadClosed;
		}
	}
	return NoRadar; //no radar
}

//Callbacks
void __stdcall CommandCallback(Furuno::INT32 id, Furuno::INT32 no)
{
	//wxLogDebug("Command callback: %i, %i\n", id, no);
}

void __stdcall CycleCallback(void)
{
	Furuno::UINT32 lastError = FEC_ERROR_OK;

	// get LastError of NavNetLibrary API
	lastError = Furuno::LeGetLastError(Furuno::ERRORTYPE_BACKGROUND);
	if (lastError != FEC_ERROR_OK){
		wxLogDebug("Background Error = %x", lastError);

		// initialize LastError of NavNetLibrary Background 
		Furuno::LeClearLastBackgroundError();
	}
}

void __stdcall EchoCallback(Furuno::INT32 radarNo, Furuno::INT16 status, Furuno::UINT8 *echo,
	Furuno::INT16 sweep_len, Furuno::INT16 scale, Furuno::INT16 range,
	Furuno::INT16 angle, Furuno::INT16 heading, Furuno::INT16 h_flg)
{
	RadarEcho::storeSweep(radarNo, status, echo, sweep_len, scale, range, angle, heading, h_flg);
	//wxLogDebug("Echo callback\n");
}

void __stdcall EchoTileCallback(Furuno::INT32 radarNo, Furuno::INT16 heading, Furuno::INT16 headingStatus,
	Furuno::INT16 range, Furuno::INT16 rangeUnit, Furuno::INT16 scale,
	Furuno::INT16 xPos, Furuno::INT16 yPos, Furuno::INT16 sweepCenterX,
	Furuno::INT16 sweepCenterY, Furuno::INT16 tileSize, unsigned char *echo)
{
	//wxLogDebug("Echo tile callback\n");
}

void ConvertStringtoUINT8(Furuno::UINT8 *ip_addr, std::string _IpAdd)
{
	int i = 0;
	int j = 0;
	std::string tmp = "";
	std::string tmp_IpAdd;
	unsigned long ip;
	for (i = 0; i < _IpAdd.length(); i++){
		tmp_IpAdd = _IpAdd[i];
		if (tmp_IpAdd == "."){
			ip = stoul(tmp);
			ip_addr[j] = (Furuno::UINT8)ip;
			j++;
			tmp = "";
		}
		else if (i == (_IpAdd.length() - 1)){
			tmp = tmp + tmp_IpAdd;
			ip = stoul(tmp);
			ip_addr[j] = (Furuno::UINT8)ip;
		}
		else{
			tmp = tmp + tmp_IpAdd;
		}
	}
}

void AddRadarNoToCombobox(wxComboBox* m_comboBox)
{
	int i = 0;
	for (i = 0; i <= g_radarnum_max; i++){
		m_comboBox->Append(wxString::Format(wxT("%i"), i));
	}
}
#if FLM_COMPILE_NEW_LICENSE == 1
/*  Check for certified devices from [Sensor_list.txt]*/
void P_SensorNo(wxThreadEvent* event, Radar *m_pHandler)
{
	int status;
	int count = 0;
	FILE *fp = NULL;
	char num_ID[200] = { "" }, equipment[200] = { "" }, sensor_name[200] = { "" };

	fp = fopen("..//Sensor_list.txt", "r");
	if (fp == NULL) //[Sensor_list.txt] does not open
	{
		event->SetString("\t\t Missing file [Sensor_list.txt] \n");
		wxQueueEvent(m_pHandler, event->Clone());
		event->SetString("\t\tIgnored ID check \n");
		wxQueueEvent(m_pHandler, event->Clone());
	}
	else
	{
		event->SetString("\t\t Sensors in your target.dat \n");
		wxQueueEvent(m_pHandler, event->Clone());

		//read in order line by line , [ID_No],[whitch one Sensor(Consider indentation)],[Sensor Name]
		while (fscanf(fp, "%s , %[^ , ]  , Sensor : %s", num_ID, equipment, sensor_name) != EOF)
		{
			if (Furuno::FlmCertificateEquipment((const Furuno::UINT8 *)equipment, (Furuno::UINT8 *)num_ID, &status) == 0)
			{
				count++;
				event->SetString("\t\t");
				wxQueueEvent(m_pHandler, event->Clone());

				event->SetString(equipment);
				wxQueueEvent(m_pHandler, event->Clone());

				event->SetString(" \tSensor : ");
				wxQueueEvent(m_pHandler, event->Clone());

				event->SetString(sensor_name);
				wxQueueEvent(m_pHandler, event->Clone());

				event->SetString(" \n");
				wxQueueEvent(m_pHandler, event->Clone());
			}
		}
		if (count == 0) //SensorNo is Nothing
		{
			event->SetString("\t\t Not included in [Sensor_list.txt] \n\t\t Please update [License : target.dat ]]\n");
			wxQueueEvent(m_pHandler, event->Clone());
		}
		fclose(fp);
	}
}
#endif