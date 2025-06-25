#ifndef __INCLUDE_NNDLL_H
#define __INCLUDE_NNDLL_H (1)

#include "fectype.h"

#ifndef __stdcall
#	define __stdcall
#endif

#ifndef DECLSPEC
#	define DECLSPEC
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ENTRY_OK            (0)
#define ENTRY_DLL_ERR       (-1)
#define ENTRY_OPEN_LOG_ERR  (-2)
#define ENTRY_FUNC_ERR      (-3)
#define ENTRY_SO_NAME_ERR	(-4)

int fec_entry_function( const char *pSoFileName );
void    InitialITRON( void );

#define NC_OK           (0)
#define NC_NG           (-1)
#define NC_NODATA       (-2)
#define NC_INIT_ERROR   (-3)
#define NC_PARAM_ERROR  (-4)
#define NC_SEND_ERROR   (-5)

#define RM_OK           (0)
#define RM_NG           (-1)
#define RM_NODATA       (-2)
#define RM_INIT   		(-3)
#define RM_PARAM_ERROR  (-4)
#define RM_SEND_ERROR   (-5)
#define RM_CERT_ERROR   (-9)

#define RM_INIT_ERROR RM_INIT

#define	MAX_COMMAND_ID				(256)
#define EQUIPMENT_NAME_RADAR		"Radar"
#define EQUIPMENT_NAME_ETR			"Etr"

// IRADAR
#define	IRAD_CONNECT					(0)		//	60
#define	IRAD_DISPMODE					(1)		//	61
#define	IRAD_RANGE						(2)		//	62
#define	IRAD_GAIN						(3)		//	63
#define	IRAD_ACSEA						(4)		//	64
#define	IRAD_ACRAIN						(5)		//	65
#define	IRAD_CUSTOMFUNCTIONALL			(6)		//	66
#define	IRAD_CUSTOMFUNCTION				(7)		//	67
#define	IRAD_PULSEWIDTH					(8)		//	68
#define	IRAD_TXSTBY						(9)		//	69
#define	IRAD_SELECTTARGET				(10)	//	6A
#define	IRAD_ACQTARGET					(11)	//	6B
#define	IRAD_CANCELTARGET				(12)	//	6C
#define	IRAD_ARPADISPMODE				(13)	//	6D
#define	IRAD_ANTENNATYPE				(14)	//	6E
#define	IRAD_KEYCOMMAND					(15)	//	6F
#define	IRAD_GUARDSTATUS				(16)	//	70
#define	IRAD_GUARDAREA					(17)	//	71
#define	IRAD_LOSTACK					(18)	//	72
#define	IRAD_ARPAVECTOR					(19)	//	73
#define	IRAD_ARPACPATCPA				(20)	//	74
#define	IRAD_TUNE						(21)	//	75
#define	IRAD_TUNEINDICATOR				(22)	//	76
#define	IRAD_BLIND						(23)	//	77
#define	IRAD_RADARPOS					(24)	//	78
#define	IRAD_ARPA_TARGET_ALLCLEAR		(25)	//	79
#define	IRAD_TUNEADJUST					(26)	//	7A
#define	IRAD_CANCELTARGETID				(27)	//	7B
#define	IRAD_REFERENCEPOINT				(28)	//	7C
#define	IRAD_ALARM						(29)	//	7D
#define	IRAD_HEADING					(30)	//	7E
#define	IRAD_SELECTQVDISPLAY			(31)	//	7F
#define	IRAD_ATT						(32)	//	80
#define	IRAD_HDADJ						(33)	//	81
#define	IRAD_TIMINGADJ					(34)	//	82
#define	IRAD_MBSADJ						(35)	//	83
#define	IRAD_ANTHEIGHT					(36)	//	84
#define	IRAD_NEARSTC					(37)	//	85
#define	IRAD_MIDSTC						(38)	//	86
#define	IRAD_FARSTC						(39)	//	87
#define	IRAD_RINGSUP					(40)	//	88
#define	IRAD_ANTREVOLUTION				(41)	//	89
#define	IRAD_ANTSW						(42)	//	8A
#define	IRAD_STOPEDTX					(43)	//	8B
#define	IRAD_MONITORSELECT				(44)	//	8C
#define	IRAD_ANTNO						(45)	//	8D
#define	IRAD_ONTIME						(46)	//	8E
#define	IRAD_TXTIME						(47)	//	8F
#define	IRAD_PMADJ						(48)	//	90
#define	IRAD_DEMOECHO					(49)	//	91
#define	IRAD_EAVGYRO					(50)	//	92
#define	IRAD_2NDECHOREJ					(51)	//	93
#define	IRAD_IMOPM						(52)	//	94
#define	IRAD_SART						(53)	//	95
#define	IRAD_VERSIONNO					(54)	//	96
#define	IRAD_STARTARPATEST				(55)	//	97
#define	IRAD_GUARD_MODE					(56)	//	98
#define	IRAD_GUARD_FAN1					(57)	//	99
#define	IRAD_GUARD_FAN2					(58)	//	9A
#define	IRAD_POLYGON					(59)	//	9B
#define	IRAD_ARPAGYRO					(60)	//	9C
#define	IRAD_SPEED						(61)	//	9D
#define	IRAD_SETDRIFT					(62)	//	9E
#define	IRAD_GC10						(63)	//	9F
#define	IRAD_CANCELSTATUS				(64)	//	A0
#define	IRAD_ARPAPRESET					(65)	//	A1
#define	IRAD_NEXTECHO					(66)	//	A2
#define	IRAD_TRAILMODE					(67)	//	A3
#define	IRAD_TRAILCLEAR					(68)	//	A4
#define	IRAD_CANCELTARGETNO				(69)	//	A5
#define	IRAD_SWEEPPOS					(70)	//	A6
#define	IRAD_ACQTARGETNOIBS				(71)	//	A7
#define	IRAD_CANCELTARGETNOIBS			(72)	//	A8
#define	IRAD_GPSANTPOS					(73)	//	A9
#define	IRAD_CONNINGPOS					(74)	//	AA
#define	IRAD_SELECTANTENNA				(75)	//	AB
#define	IRAD_WAKEUPCOUNT				(76)	//	AC
#define	IRAD_SHIPINFO					(77)	//	AD
#define	IRAD_AISALARM					(78)	//	AE
#define	IRAD_ARPAALARM					(79)	//	AF
#define	IRAD_SELECTALARMOUT				(80)	//	B0
#define	IRAD_ALARMACK					(81)	//	B1
#define	IRAD_SELECTCRT					(82)	//	B2
#define	IRAD_SELECTALARMPOLARITY		(83)	//	B3
#define	IRAD_COMBINESTATUS				(84)	//	B4
#define	IRAD_HEADINGSOURCE				(85)	//	B5
#define	IRAD_NETWORKPRIO				(86)	//	B6
#define	IRAD_MENUSET					(87)	//	B7
#define	IRAD_IMOECHOSW					(88)	//	B8
#define	IRAD_EXTALARMCK					(89)	//	B9
#define	IRAD_PMADJEND					(90)	//	BA
#define	IRAD_SELECTTTMOUT				(91)	//	BB
#define	IRAD_ARPA_MAX_RANGE				(92)	//	BC
#define	IRAD_ECHOLEVEL					(93)	//	BD
#define	IRAD_ARPA_ALLCLEAR				(94)	//	BE
#define	IRAD_LAND_SIZE					(95)	//	BF
#define	IRAD_ARPA_ANT					(96)	//	C0
#define	IRAD_ARPA_ACQ_CORRE				(97)	//	C1
#define	IRAD_ARPA_WEED					(98)	//	C2
#define	IRAD_ARPA_GATE_SIZE				(99)	//	C3
#define	IRAD_ARPA_FILTER_RESPONS		(100)	//	C4
#define	IRAD_ARPA_LOST_COUNT			(101)	//	C5
#define	IRAD_ARPA_MAX_SPEED				(102)	//	C6
#define	IRAD_ARPA_TIME_VECT				(103)	//	C7
#define	IRAD_OPERATOR_FITNESS			(104)	//	C8
#define	IRAD_PICTURE_DEFAULT			(105)	//	C9
#define	IRAD_RFC_VERSION_NO				(106)	//	CA
#define	IRAD_COMBINE_DATA				(107)	//	CB
#define	IRAD_DUMMY_19					(108)	//	CC
#define	IRAD_DUMMY_20					(109)	//	CD
#define	IRAD_DUMMY_21					(110)	//	CE
#define	IRAD_INTERSW_INFO				(111)	//	CF
#define	IRAD_ARPA_LOSTFILTER_RANGE		(112)	//	D0
#define	IRAD_ARPA_LOSTFILTER_SPEED		(113)	//	D1
#define	IRAD_STC_RANGE					(114)	//	D2
#define	IRAD_CUSTOM_ACTION				(115)	//	D3
#define	IRAD_BUILD_UP_TIME				(116)	//	D4
#define	IRAD_DISPUNIT_INFO				(117)	//	D5
#define	IRAD_DUMMY_29					(118)	//	D6
#define	IRAD_ARPA_LOSTFILTER_MODE		(119)	//	D7
#define	IRAD_ASSOCIATION_STATUS			(121)	//	D8
#define	IRAD_ECHO_CLEAR					(120)	//	D9
#define	IRAD_CANCELSELECTTARGET			(122)	//	DA
#define	IRAD_CANCELSELECTLOSTTARGET		(123)	//	DB
#define	IRAD_DUMMY_35					(124)	//	DC
#define	IRAD_DUMMY_36					(125)	//	DD
#define	IRAD_DUMMY_37					(126)	//	DE
#define	IRAD_DUMMY_38					(127)	//	DF
#define	IRAD_ATFPARM					(128)	//	E0
#define	IRAD_TRAILPROCESS				(129)	//	E1
#define	IRAD_REACQTARGET				(130)	//	E2
#define	IRAD_ALIVECHECK					(131)	//	E3
#define	IRAD_TXECHOSEL					(132)	//	E4
#define	IRAD_RXECHOSEL					(133)	//	E5
#define	IRAD_RSPRESET					(134)	//	E6
#define	IRAD_STARTSELFMODE				(135)	//	E7
#define	IRAD_JUMMINGABLE				(136)	//	E8
#define	IRAD_ICECLASS					(137)	//	E9
#define	IRAD_ATFSETTING					(138)	//	EA
#define	IRAD_RSALLCLEAR					(139)	//	EB
#define	IRAD_SSDTXCHANNEL				(140)	//	EC
#define	IRAD_HIGHSENSITIVITYMODE		(141)	//	ED
#define	IRAD_BEARINGRESOLUTIONSELECT	(142)	//	EE
#define	IRAD_ECHOCOLORTYPE				(143)	//	EF
#define	IRAD_ACCUSHIP					(144)	//	F0
#define	IRAD_OBSERVATION_MODE			(145)	//	F1
#define	IRAD_SART_SIGNAL				(146)	//	F2
#define	IRAD_DUMMY_58					(147)	//	F3
#define	IRAD_IMAGECOR					(148)	//	F4
#define	IRAD_NN3COMMAND					(149)	//	F5
#define	IRAD_SETMEMORY					(150)	//	F6
#define	IRAD_EQUIPCOMP					(151)	//	F7
#define	IRAD_PULSERANGE					(152)	//	F8
#define	IRAD_MAGNETRON					(153)	//	F9
#define	IRAD_MAGNETRONIND				(154)	//	FA
#define	IRAD_ARPDETECT					(155)	//	FB
#define	IRAD_ARPCATCH					(156)	//	FC
#define	IRAD_ARPPURSUIT					(157)	//	FD
#define	IRAD_RANGESELECT				(158)	//	FE
#define	IRAD_VIDEOLEVEL					(159)	//	FF

#define	USE_DOLLAR_ARPA
#define	FNET_MAX_SHAREGROUP (10)	// Maximum number of sharing groups

typedef enum FnetShareGroup_e {
	kFnetShareGroup_Unknown = -1,
	kFnetShareGroup_GlobalNN3 = 0,
	kFnetShareGroup_Global,
	kFnetShareGroup_Product,
	kFnetShareGroup_Max = FNET_MAX_SHAREGROUP
}FnetShareGroup_t;

typedef struct FnetShareGroupList_s {
	UINT64 group_id[kFnetShareGroup_Max];
}FnetShareGroupList_t;

#define	FNET_HOSTNAME_MAX	(8)
typedef struct FnetHostname_s {
	char name[FNET_HOSTNAME_MAX+1];
}FnetHostname_t;
#define FnetHostname_INITIALIZER {0}

enum{
	FNET_LOG_TYPE_EXCEPTION=1,
	FNET_LOG_TYPE_SYSTEM,
	FNET_LOG_TYPE_DATA,
	FNET_LOG_TYPE_DEBUG,
	FNET_LOG_TYPE_MAX,
};

// NAVNETDLL COMMON
DECLSPEC	void	__stdcall	NcEntryCommonUdpRxFunction( INT32 code, void (__stdcall *p)( INT8* , UINT32, INT32 ) );
DECLSPEC	INT32	__stdcall	NcGetCommonUdpCepID( void );
DECLSPEC	UINT32	__stdcall	NcMakeBroadcastAddress( void );
DECLSPEC	UINT32	__stdcall	NcMakeBroadcastAddressWifi( void );
DECLSPEC	UINT32	__stdcall	NcMakeBroadcastAddressWiredClassC( void );
DECLSPEC	INT32	__stdcall	NcSetNetworkIpAddress( UINT8 ip1, UINT8 ip2, UINT8 ip3, UINT8 ip4 );
DECLSPEC	INT32	__stdcall	NcGetNetworkIpAddress( UINT8 *ip1, UINT8 *ip2, UINT8 *ip3, UINT8 *ip4 );
DECLSPEC	INT32	__stdcall	NcSetNetworkSubnetMask( UINT8 subnet1, UINT8 subnet2, UINT8 subnet3, UINT8 subnet4 );
DECLSPEC	INT32	__stdcall	NcGetNetworkSubnetMask( UINT8 *subnet1, UINT8 *subnet2, UINT8 *subnet3, UINT8 *subnet4 );
DECLSPEC	INT32	__stdcall	NcSetNetworkDefaultGateway( UINT8 gateway1, UINT8 gateway2, UINT8 gateway3, UINT8 gateway4 );
DECLSPEC	INT32	__stdcall	NcGetNetworkDefaultGateway( UINT8 *gateway1, UINT8 *gateway2, UINT8 *gateway3, UINT8 *gateway4 );
DECLSPEC	INT32	__stdcall	NcSetNetworkBasePortNo( UINT32 portno );
DECLSPEC	INT32	__stdcall	NcGetNetworkBasePortNo( UINT32 *portno );
DECLSPEC	INT32	__stdcall	NcSetHostName( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcGetHostName( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcInitialNavnetCommon( UINT8 *hname, INT32 *fcode );
DECLSPEC	INT32	__stdcall	NcBasicInitializationForModules(  UINT8 ip0, UINT8 ip1, UINT8 ip2, UINT8 ip3, FnetHostname_t* h );
DECLSPEC	INT32	__stdcall	NcEndNavnetCommon( INT32 mode );
DECLSPEC	UINT16	__stdcall	NcGetFnetPortNo( void );
DECLSPEC	INT32	__stdcall	NcGetMaster( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcGetMasterEx( UINT8 *hname, UINT64 group );
DECLSPEC	INT32	__stdcall	NcGetFnetMaster( UINT8 *hname, /*(FnetShareGroup_t)*/ INT32 group );
DECLSPEC	INT32	__stdcall	NcGetClientIP( INT32 fc, INT32 no, UINT8 *ip );
DECLSPEC	INT32	__stdcall	NcGetClientHostname( INT32 fc, INT32 no, UINT8* hname );
DECLSPEC	INT32	__stdcall	NcGetConnectionHost( UINT8 *host, INT32 max, INT32 fc, INT32 d );
DECLSPEC	INT32	__stdcall	NcWatchAlarmFnetServerDown( UINT8 *serverName, INT32 fc );
DECLSPEC	INT32	__stdcall	NcWatchAlarmFnetClientDown( INT32 num, INT32 fc );
DECLSPEC	INT32	__stdcall	NcGetTargetHostName( UINT32 ip, UINT8 *host );
DECLSPEC	UINT32	__stdcall	NcHostNameToIpAddr( UINT8 *host );
DECLSPEC	void	__stdcall	NcEntryCallbackRxFuncCode( void (__stdcall *func)( UINT32, const INT8*, UINT16 ) );
DECLSPEC	INT32	__stdcall	NcSetRestartCallbackFunc( INT32 (__stdcall *func)( INT32 status ) );
DECLSPEC	INT32	__stdcall	NcSetSleepMode( INT32 mode );
DECLSPEC	INT32	__stdcall	NcSetSleepCallbackFunc( INT32 (__stdcall *func)( INT32 status ) );
DECLSPEC	INT32	__stdcall	NcSetShareGroups( const FnetShareGroupList_t* groups );
DECLSPEC	INT32	__stdcall	NcSendPutFnetStartRequest( void );	// 2007.10.05
DECLSPEC	INT32	__stdcall	NcEntryFnetCallback( void (__stdcall *func)(void) );
DECLSPEC	INT32	__stdcall	NcEntryFnetShareHostListUpdatedCallback ( void (__stdcall *func)(void) );
DECLSPEC	void	__stdcall	NcWakeUpFnetTask( void );
DECLSPEC	void	__stdcall	NcKillAllTask( void );
DECLSPEC	INT32	__stdcall	NcGetModuleVersionNo( UINT8 *version );
DECLSPEC	INT32	__stdcall	NcSendChangeIpAddress( char *hname, UINT8 *ipaddr );
DECLSPEC	INT32	__stdcall	NcSendChangeHostName( char *hname, char *ch_hname );
DECLSPEC	INT32	__stdcall	NcSendChangeGatewayAddress( char *hname, UINT8 *pt );
DECLSPEC	INT32	__stdcall	NcSendChangeSubnetMask( char *hname, UINT8 *pt );
DECLSPEC	INT32	__stdcall	NcSetChangeIpAddressCallbackFunc( INT32 (__stdcall *func)( UINT8* buf ) );
DECLSPEC	INT32	__stdcall	NcSetChangeHostNameCallbackFunc( INT32 (__stdcall *func)( UINT8* buf ) );
DECLSPEC	INT32	__stdcall	NcSetChangeGatewayAddressCallbackFunc( INT32 (__stdcall *func)( UINT8* buf ) );
DECLSPEC	INT32	__stdcall	NcSetChangeSubnetMaskCallbackFunc( INT32 (__stdcall *func)( UINT8* buf ) );
DECLSPEC	INT32	__stdcall	NcSendRestart( UINT8 *hname, INT32 status );
DECLSPEC	INT32	__stdcall	NcDeleteHostTableByIP( UINT32 ip );
DECLSPEC	INT32	__stdcall	NcDeleteHostTableByHostName( UINT8 *hostname );
DECLSPEC	INT32	__stdcall	NcGetAliveFlag( INT32 *num, UINT32 *aliveTbl );
DECLSPEC	void 	__stdcall	NcSetFecDebugLogEnable( INT32 enable );
DECLSPEC	INT32	__stdcall	NcGetFecDebugLogEnable();
DECLSPEC	INT32	__stdcall	NcSetNetworkIpAddressWifi( UINT8 ip1, UINT8 ip2, UINT8 ip3, UINT8 ip4 );
DECLSPEC	INT32	__stdcall	NcGetNetworkIpAddressWifi( UINT8 *ip1, UINT8 *ip2, UINT8 *ip3, UINT8 *ip4 );
DECLSPEC	INT32	__stdcall	NcSetNetworkSubnetMaskWifi( UINT8 subnet1, UINT8 subnet2, UINT8 subnet3, UINT8 subnet4 );
DECLSPEC	INT32	__stdcall	NcGetNetworkSubnetMaskWifi( UINT8 *subnet1, UINT8 *subnet2, UINT8 *subnet3, UINT8 *subnet4 );
DECLSPEC	INT32	__stdcall	NcSetNetworkDefaultGatewayWifi( UINT8 gateway1, UINT8 gateway2, UINT8 gateway3, UINT8 gateway4 );
DECLSPEC	INT32	__stdcall	NcGetNetworkDefaultGatewayWifi( UINT8 *gateway1, UINT8 *gateway2, UINT8 *gateway3, UINT8 *gateway4 );
DECLSPEC	INT32	__stdcall	NcSetNetworkIpAddressWiredClassC( UINT8 ip1, UINT8 ip2, UINT8 ip3, UINT8 ip4 );
DECLSPEC	INT32	__stdcall	NcGetNetworkIpAddressWiredClassC( UINT8 *ip1, UINT8 *ip2, UINT8 *ip3, UINT8 *ip4 );
DECLSPEC	INT32	__stdcall	NcSetNetworkSubnetMaskWiredClassC( UINT8 subnet1, UINT8 subnet2, UINT8 subnet3, UINT8 subnet4 );
DECLSPEC	INT32	__stdcall	NcGetNetworkSubnetMaskWiredClassC( UINT8 *subnet1, UINT8 *subnet2, UINT8 *subnet3, UINT8 *subnet4 );
DECLSPEC	INT32	__stdcall	NcSetNetworkDefaultGatewayWiredClassC( UINT8 gateway1, UINT8 gateway2, UINT8 gateway3, UINT8 gateway4 );
DECLSPEC	INT32	__stdcall	NcGetNetworkDefaultGatewayWiredClassC( UINT8 *gateway1, UINT8 *gateway2, UINT8 *gateway3, UINT8 *gateway4 );
DECLSPEC	void	__stdcall	NcSuspendNetwork( void );
DECLSPEC	void	__stdcall	NcResumeNetwork( void );
DECLSPEC	INT32	__stdcall	NcSetIpAddress( UINT8 ip1, UINT8 ip2, UINT8 ip3, UINT8 ip4);
DECLSPEC	UINT8*	__stdcall	NcGetIpAddress( void );
DECLSPEC	INT32	__stdcall	NcSetSubnetMask( UINT8 *p );
DECLSPEC	UINT8*	__stdcall	NcGetSubnetMask( void );
DECLSPEC	INT32	__stdcall	NcSetBasePortNo( UINT32 portno );
DECLSPEC	UINT32	__stdcall	NcGetBasePortNo( void );
DECLSPEC	INT32	__stdcall	NcSetGatewayMask( UINT8 *p );
DECLSPEC	UINT8*	__stdcall	NcGetGatewayMask( void );

// RADAR
DECLSPEC	INT32	__stdcall	NcSetRadarTxEchoAddress( INT32 txType );
DECLSPEC	INT32	__stdcall	NcInitialRadarServer( INT32 evt_no );
DECLSPEC	INT32	__stdcall	NcInitialRadarClient( INT32 evt_no );
DECLSPEC	INT32	__stdcall	NcEndRadarClient( void );
DECLSPEC	INT32	__stdcall	NcConnectRadarServer( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcConnectRadarServerEx( UINT8 *hname, INT32 sensorNo );
DECLSPEC	INT32	__stdcall	NcUnconnectRadarServer( void );
DECLSPEC	INT32	__stdcall	NcUnconnectRadarServerEx( INT32 sensorNo );
DECLSPEC	INT32	__stdcall	NcSetRadarEchoTxTriggerCallbackFunc( INT32 (__stdcall *func)( UINT8 *header, INT32 headersize ) );
DECLSPEC	INT32	__stdcall	NcRadarEchoTxTrigger( void );
DECLSPEC	INT32	__stdcall	NcSendRadarEchoData( UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcSendRadarCommand( UINT8 *buf );
DECLSPEC	INT32	__stdcall	NcSendRadarCommandEx( INT32 sensorNo, UINT8 *buf );
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForRadar( INT32 num );
DECLSPEC	INT32	__stdcall	NcGetPassForRadarEx( INT32 num, INT32 sensorNo );
#endif
DECLSPEC	INT32	__stdcall	NcIsRadarServer( void );
DECLSPEC	INT32	__stdcall	NcGetRadarFunctionCode( INT32 *fc );
DECLSPEC	INT32	__stdcall	NcSetRadarEchoCallbackFunc( INT32 (__stdcall *func)( UINT8 *rcv_data, INT32 size, INT32 sensorNo ) );
DECLSPEC	INT32	__stdcall	NcSetRadarTileEchoCallbackFunc( INT32 (__stdcall *func)( UINT8 *rcv_data, INT32 size, INT32 sensorNo ) );
DECLSPEC	INT32	__stdcall	NcSetRadarConnectionFunc( INT32 (__stdcall *func)( INT32 status ) );
DECLSPEC	UINT8*	__stdcall	NcGetRadarSourceName( void );
DECLSPEC	UINT32	__stdcall	NcGetRadarIpAddress( void );
DECLSPEC	UINT32	__stdcall	NcGetRadarIpAddressEx( INT32 sensorNo );
DECLSPEC	INT32	__stdcall	NcReadRadarData( INT32 sensorNo, INT32 num, UINT8 *buf, INT32 size );

// ETR
DECLSPEC	INT32	__stdcall	NcInitialEtrServer( INT32 evt_no );
DECLSPEC	INT32	__stdcall	NcInitialEtrClient( INT32 evt_no );
DECLSPEC	INT32	__stdcall	NcConnectEtrServer( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcConnectEtrServerEx( UINT8 *hname, INT32 sensorNo );
DECLSPEC	INT32	__stdcall	NcSetEtrEchoTxTriggerCallbackFunc( INT32 (__stdcall *func)( UINT8 *header, INT32 headersize ) );
DECLSPEC	INT32	__stdcall	NcEtrEchoTxTrigger( void );
DECLSPEC	INT32	__stdcall	NcSendEtrEchoData( UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcSendEtrCommand( UINT8 *buf );
DECLSPEC	INT32	__stdcall	NcSendEtrCommandEx( INT32 sensorNo, UINT8 *buf );
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForEtr( INT32 num );
#endif
DECLSPEC	INT32	__stdcall	NcIsEtrServer( void );
DECLSPEC	INT32	__stdcall	NcGetEtrFunctionCode( INT32 *fc );
DECLSPEC	UINT32	__stdcall	NcGetEtrIpAddress ( void );
DECLSPEC	UINT32	__stdcall	NcGetEtrIpAddressEx ( INT32 sensorNo );
DECLSPEC	INT32	__stdcall	NcSetEtrEchoCallbackFunc( INT32 (__stdcall *func)( UINT8 *rcv_data, INT32 size, INT32 sensorNo ) );
DECLSPEC	INT32	__stdcall	NcSetEtrConnectionFunc( INT32 (__stdcall *func)( INT32 status ) );
DECLSPEC	UINT8*	__stdcall	NcGetEtrSourceName( void );
DECLSPEC	INT32	__stdcall	NcReadEtrData( INT32 sensorNo, INT32 num, UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcSetThroughEtrEchoWiredClassC( INT32 through );
DECLSPEC	INT32	__stdcall	NcGetThroughEtrEchoWiredClassC( void );
DECLSPEC	INT32	__stdcall	NcSetThroughEtrEchoWifi( INT32 through );
DECLSPEC	INT32	__stdcall	NcGetThroughEtrEchoWifi( void );

// NEMA0183
DECLSPEC	INT32	__stdcall	NcInitialNmea183Server( void );
#if NO_CHECK_SENSOR_ID
DECLSPEC	INT32	__stdcall	NcInitialNmea183Server2( void );
#endif
DECLSPEC	INT32	__stdcall	NcInitialNmea0183PortStatus( INT32 STATUS );
DECLSPEC	INT32	__stdcall	NcPutNmeaData( UINT8 *_buffer, INT32 n, UINT32 ipaddr );
DECLSPEC	INT32	__stdcall	NcNetworkWriteNMEA0183( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkReadNMEA0183( INT32 no, UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNmea0183InitializeUnicast( void );
DECLSPEC	INT32	__stdcall	NcNmea0183WriteUnicast( INT32 no, const UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNmea0183GetUnicastAddress( INT16* pStatus, UINT32* pAddress );
DECLSPEC	INT32	__stdcall	NcNmea0183SetUnicastAddress( UINT32 address );
DECLSPEC	INT32	__stdcall	NcReadTargetDataForNMEA0183( INT32 sensorNo, UINT8 *buf, INT32 size );

// NEMA0183SP
DECLSPEC	INT32	__stdcall	NcInitialNmea183SPServer( void );
DECLSPEC	INT32	__stdcall	NcInitialNmea0183SPPortStatus( INT32 STATUS );
DECLSPEC	INT32	__stdcall	NcPutNmeaSPData( UINT8 *_buffer, INT32 n, UINT32 ipaddr );
DECLSPEC	INT32	__stdcall	NcNetworkWriteNMEA0183SP( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkReadNMEA0183SP( INT32 no, UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNmea0183SPInitializeUnicast( void );
DECLSPEC	INT32	__stdcall	NcNmea0183SPWriteUnicast( INT32 no, const UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNmea0183SPGetUnicastAddress( INT16* pStatus, UINT32* pAddress );
DECLSPEC	INT32	__stdcall	NcNmea0183SPSetUnicastAddress( UINT32 address );
DECLSPEC	INT32	__stdcall	NcReadTargetDataForNMEA0183SP( INT32 sensorNo, UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNmea0183SPTryConnect( void );

// NEMA0183SPWIFI
DECLSPEC	INT32	__stdcall	NcInitialNmea183SPWIFIServer( void );
DECLSPEC	INT32	__stdcall	NcInitialNmea0183SPWIFIPortStatus( INT32 STATUS );
DECLSPEC	INT32	__stdcall	NcPutNmeaSPWIFIData( UINT8 *_buffer, INT32 n, UINT32 ipaddr );
DECLSPEC	INT32	__stdcall	NcNetworkWriteNMEA0183SPWIFI( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkReadNMEA0183SPWIFI( INT32 no, UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNmea0183SPWIFIInitializeUnicast( void );
DECLSPEC	INT32	__stdcall	NcNmea0183SPWIFIWriteUnicast( INT32 no, const UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNmea0183SPWIFIGetUnicastAddress( INT16* pStatus, UINT32* pAddress );
DECLSPEC	INT32	__stdcall	NcNmea0183SPWIFISetUnicastAddress( UINT32 address );
DECLSPEC	INT32	__stdcall	NcReadTargetDataForNMEA0183SPWIFI( INT32 sensorNo, UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNmea0183SPWIFITryConnect( void );

// COMMONSP
DECLSPEC	INT32	__stdcall	NcInitialCommonSPServer( void );
DECLSPEC	INT32	__stdcall	NcInitialCommonSPPortStatus( INT32 STATUS );
DECLSPEC	INT32	__stdcall	NcPutCommonSPData( UINT8 *_buffer, INT32 n, UINT32 ipaddr );
DECLSPEC	INT32	__stdcall	NcNetworkWriteCommonSP( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkReadCommonSP( INT32 no, UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcCommonSPInitializeUnicast( void );
DECLSPEC	INT32	__stdcall	NcCommonSPWriteUnicast( INT32 no, const UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcCommonSPGetUnicastAddress( INT16* pStatus, UINT32* pAddress );
DECLSPEC	INT32	__stdcall	NcCommonSPSetUnicastAddress( UINT32 address );
DECLSPEC	INT32	__stdcall	NcReadTargetDataForCommonSP( INT32 sensorNo, UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcCommonSPTryConnect( void );
DECLSPEC	INT32	__stdcall	NcCommonSpTxDepthRange( UINT8 unit );

// COMMONSP Wifi
DECLSPEC	INT32	__stdcall	NcInitialCommonSPWIFIServer( void );
DECLSPEC	INT32	__stdcall	NcInitialCommonSPWIFIPortStatus( INT32 STATUS );
DECLSPEC	INT32	__stdcall	NcPutCommonSPWIFIData( UINT8 *_buffer, INT32 n, UINT32 ipaddr );
DECLSPEC	INT32	__stdcall	NcNetworkWriteCommonSPWIFI( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkReadCommonSPWIFI( INT32 no, UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcCommonSPWIFIInitializeUnicast( void );
DECLSPEC	INT32	__stdcall	NcCommonSPWIFIWriteUnicast( INT32 no, const UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcCommonSPWIFIGetUnicastAddress( INT16* pStatus, UINT32* pAddress );
DECLSPEC	INT32	__stdcall	NcCommonSPWIFISetUnicastAddress( UINT32 address );
DECLSPEC	INT32	__stdcall	NcReadTargetDataForCommonSPWIFI( INT32 sensorNo, UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcCommonSPWIFITryConnect( void );
DECLSPEC	INT32	__stdcall	NcCommonSpWifiTxDepthRange( UINT8 unit );

// ARPA
DECLSPEC	INT32	__stdcall	NcPutArpaData( UINT8 *_buffer, INT32 n, UINT32 ipaddr );
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForNmeaPortARPA( void );
DECLSPEC	INT32	__stdcall	NcGetPassForNmeaSPPortARPA( void );
#endif

// NEMAxxxxF
DECLSPEC	INT32	__stdcall	NcInitialNmea183FHeaveServer( void );
DECLSPEC	INT32	__stdcall	NcInitialNmea183FHeadingServer( void );
DECLSPEC	INT32	__stdcall	NcInitialNmea183FRollPitchServer( void );
DECLSPEC	INT32	__stdcall	NcInitialNmea183FWindServer( void );
DECLSPEC	void	__stdcall	NcFuncThroughNmea183FHeave( UINT32 reference, UINT32 ltime, UINT8 buf[] , INT32 len );
DECLSPEC	void	__stdcall	NcFuncThroughNmea183FHeading( UINT32 reference, UINT32 ltime, UINT8 buf[] , INT32 len );
DECLSPEC	void	__stdcall	NcFuncThroughNmea183FRollPitch( UINT32 reference, UINT32 ltime, UINT8 buf[] , INT32 len );
DECLSPEC	void	__stdcall	NcFuncThroughNmea183FWind( UINT32 reference, UINT32 ltime, UINT8 buf[] , INT32 len );
DECLSPEC	INT8*	__stdcall	NcGetNetRingName0183FHeave( void );
DECLSPEC	INT8*	__stdcall	NcGetNetRingName0183FHeading( void );
DECLSPEC	INT8*	__stdcall	NcGetNetRingName0183FRollPitch( void );
DECLSPEC	INT8*	__stdcall	NcGetNetRingName0183FWind( void );
DECLSPEC	void	__stdcall	NcNmea0183FHeading( UINT8 buf[] , INT32 len, UINT32 ipaddr );
DECLSPEC	void	__stdcall	NcNmea0183FRollPitch( UINT8 buf[] , INT32 len, UINT32 ipaddr );
DECLSPEC	void	__stdcall	NcNmea0183FWind( UINT8 buf[] , INT32 len, UINT32 ipaddr );
DECLSPEC	void	__stdcall	NcNmea0183FHeave( UINT8 buf[] , INT32 len, UINT32 ipaddr );
DECLSPEC	void	__stdcall	NcNmea0183F( UINT8 buf[] , INT32 len, UINT32 ipaddr );
DECLSPEC	INT32	__stdcall	NcNetworkWriteNMEA0183FHeading( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkReadNMEA0183FHeading( INT32 no, UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkWriteNMEA0183FHeave( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkReadNMEA0183FHeave( INT32 no, UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkWriteNMEA0183FRollPitch( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkReadNMEA0183FRollPitch( INT32 no, UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkWriteNMEA0183FWind( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkReadNMEA0183FWind( INT32 no, UINT8* buf, INT32 size );

// NMEA2000
DECLSPEC	INT32	__stdcall	NcInitialNmea2000FEngineServer( void );
DECLSPEC	void	__stdcall	NcFuncThroughNmea2000FEngine( UINT32 reference, UINT32 ltime, UINT8 buf[], INT32 len );
DECLSPEC	INT8*	__stdcall	NcGetNetRingName2000FEngine( void );
DECLSPEC	void	__stdcall	NcNmea2000FEngine( UINT8 buf[] , INT32 len, UINT32 ipaddr );
DECLSPEC	void	__stdcall	NcNmea2000F( INT8 buf[] , INT32 len, UINT32 ipaddr );
DECLSPEC	INT32	__stdcall	NcNetworkWriteNMEA2000FEngine( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcNetworkReadNMEA2000FEngine( INT32 no, UINT8* buf, INT32 size );

// TARGET
DECLSPEC	INT32	__stdcall	NcInitialTargetInfo( void );
#if NO_CHECK_SENSOR_ID
DECLSPEC	INT32	__stdcall	NcInitialTargetInfo2( void );
#endif
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForTarget( void );
DECLSPEC	INT32	__stdcall	NcGetPassForTargetEx( INT32 sensorNo );
#endif
DECLSPEC	INT32	__stdcall	NcSendTargetData( UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcReadTargetData( INT32 sensorNo, UINT8 *buf, INT32 size );

// SYSTEM
DECLSPEC	INT32	__stdcall	NcInitialSystemServer( INT32 evt_no );
DECLSPEC	INT32	__stdcall	NcGetSystemServer( void );
DECLSPEC	INT32	__stdcall	NcConnectSystemServer( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcSendSystemCommand( UINT8 *buf );
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForSystem( INT32 num );
#endif
DECLSPEC	INT32	__stdcall	NcGetSystemConnectionStatus( void );
DECLSPEC	INT32	__stdcall	NcGetSystemFunctionCode( INT32 *fc );
DECLSPEC	INT32	__stdcall	NcReadSystemData( INT32 num, UINT8 *buf, INT32 size );

// DATABASE
DECLSPEC	INT32	__stdcall	NcInitialDataBaseServer( INT32 evt_no );
DECLSPEC	INT32	__stdcall	NcGetDataBaseServer( void );
DECLSPEC	INT32	__stdcall	NcSendDataBaseCommand( UINT8 *buf );
DECLSPEC	INT32	__stdcall	NcSendDataBaseCommandSingle( INT32 n, UINT8 *buf );
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForDataBase( INT32 num );
#endif
DECLSPEC	INT32	__stdcall	NcConnectDataBaseServer( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcGetDataBaseConnectionStatus( void );
DECLSPEC	INT32	__stdcall	NcGetDatabaseFunctionCode( INT32 *fc );
DECLSPEC	INT32	__stdcall	NcReadDatabaseData( INT32 num, UINT8 *buf, INT32 size );

// SETTINGS - @todo: DELETEME
DECLSPEC	INT32	__stdcall	NcInitialSettingsServer( INT32 evt_no );
DECLSPEC	INT32	__stdcall	NcGetSettingsServer( void );
DECLSPEC	INT32	__stdcall	NcConnectSettingsServer( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcSendSettingsCommand( UINT8 *buf );
DECLSPEC	INT32	__stdcall	NcSendSettingsCommandSingle( INT32 n, UINT8 *buf );
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForSettings( INT32 num );
#endif
DECLSPEC	INT32	__stdcall	NcGetSettingsConnectionStatus( void );
DECLSPEC	INT32	__stdcall	NcGetSettingsFunctionCode( INT32 *fc );
DECLSPEC	INT32	__stdcall	NcReadSettingsData( INT32 num, UINT8 *buf, INT32 size );

// MXCMD - @todo: DELETEME
DECLSPEC	INT32	__stdcall	NcInitialMxCmdServer( INT32 evt_no );
DECLSPEC	INT32	__stdcall	NcGetIdForMxCmdClient( INT32 num );
DECLSPEC	INT32	__stdcall	NcGetMxCmdServer( void );
DECLSPEC	INT32	__stdcall	NcConnectMxCmdServer( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcSendMxCmdCommand( UINT8 *buf );
DECLSPEC	INT32	__stdcall	NcSendMxCmdCommandSingle( INT32 n, UINT8 *buf );
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForMxCmd( INT32 num );
#endif
DECLSPEC	INT32	__stdcall	NcGetMxCmdConnectionStatus( void );
DECLSPEC	INT32	__stdcall	NcGetMxCmdFunctionCode( INT32 *fc );
DECLSPEC	INT32	__stdcall	NcReadMxCmdData( INT32 num, UINT8 *buf, INT32 size );
DECLSPEC	void	__stdcall	NcWaitMxCmdData();
DECLSPEC	INT32	__stdcall	NcEndMxCmdFunc( void );

// FILESHARING - @todo: DELETEME
DECLSPEC	INT32	__stdcall	NcInitialFileSharingServer( INT32 evt_no );
DECLSPEC	INT32	__stdcall	NcGetFileSharingServer( void );
DECLSPEC	INT32	__stdcall	NcConnectFileSharingServer( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcSendFileSharingCommand( UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcSendFileSharingCommandSingle( INT32 n, UINT8 *buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcGetIdForFileSharingClient( INT32 num );
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForFileSharing( INT32 num );
#endif
DECLSPEC	INT32	__stdcall	NcGetFileSharingConnectionStatus( void );
DECLSPEC	INT32	__stdcall	NcGetFileSharingFunctionCode( INT32 *fc );
DECLSPEC	INT32	__stdcall	NcReadFileSharingData( INT32 num, UINT8 *buf, INT32 size );
DECLSPEC	void	__stdcall	NcWaitFileSharingData();

// KEYBOARD
DECLSPEC	INT32	__stdcall	NcInitialKeyServer( INT32 evt_no );
DECLSPEC	INT32	__stdcall	NcConnectKeyServer( UINT8 *hname );
DECLSPEC	INT32	__stdcall	NcSendKeyCommand( INT32 dch, UINT8 *buf );
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForKey( INT32 num );
#endif
DECLSPEC	INT32	__stdcall	NcGetKeyServer( void );
DECLSPEC	INT32	__stdcall	NcGetKeyFunctionCode( INT32 *fc );
DECLSPEC	UINT8*	__stdcall	NcGetKeySourceName( void );
DECLSPEC	INT32	__stdcall	NcReadKeyData( INT32 num, UINT8 *buf, INT32 size );

// MC
DECLSPEC	int		__stdcall	NcInitialMcServer( char *addr );
DECLSPEC	int		__stdcall	NcCancelMcServer( int no );
DECLSPEC	int		__stdcall	NcConnectMcServer( int no, char *hname );
DECLSPEC	int		__stdcall	NcGetExtMCstatus( int num, int drive, int sw );
DECLSPEC	int		__stdcall	NcSetMcConditionFlag( int drv_num );
DECLSPEC	int		__stdcall	NcSetCardInfo( int drv_num );
DECLSPEC	int		__stdcall	NcGetOtherDriveCount( short *cnt );
DECLSPEC	int		__stdcall	NcGetOtherDriveHostName( short no, char *name );
DECLSPEC	int		__stdcall	NcGetMcFunctionCode( int no, int *fcd, int *fcs );

// RELAY
DECLSPEC	INT32	__stdcall	NcInitRelay( void (__stdcall *func)(INT32, INT32, INT32) );
DECLSPEC	INT32	__stdcall	NcStartRelayConnection( UINT8* hname, const UINT8* uniqueID, void (__stdcall *cbStart)(void) );
DECLSPEC	void	__stdcall	NcStopRelayConnection( char* hname, const UINT8* uniqueID );
#ifdef	NO_FNET_RING
DECLSPEC	INT32	__stdcall	NcGetPassForRelay( INT32 num );
#endif
DECLSPEC	INT32	__stdcall	NcGetRelayFunctionCode( INT32 *fc );
DECLSPEC	INT32	__stdcall	NcGetRelayServer( void );
DECLSPEC	INT32	__stdcall	NcGetRelayConnectionStatus( void );
DECLSPEC	INT32	__stdcall	NcSendRelayData( UINT8* uniqueID, INT8 *buf, INT32 length );
DECLSPEC	INT32	__stdcall	NcEntryTxFunc( void (__stdcall *func)(INT32 com, INT8* buff, INT32 length) );

// TCPIP
DECLSPEC	INT32	__stdcall	NcInitialTcpIp( UINT8 *openName );
DECLSPEC	INT32	__stdcall	NcOpenTcpIp( UINT8 *name, INT32 a, INT32 b );
DECLSPEC	INT32	__stdcall	NcCloseTcpIp( INT32 no );
DECLSPEC	INT32	__stdcall	NcReadTcpIp( INT32 no, UINT8 *to, INT32 size );
DECLSPEC	INT32	__stdcall	NcReadTcpIpWithoutCache( INT32 no, UINT8 *to, INT32 size );
DECLSPEC	INT32	__stdcall	NcWriteTcpIp( INT32 no, UINT8 *from, INT32 size );
DECLSPEC	INT32	__stdcall	NcSetTcpIpStatus( INT32 no, INT32 fc, INT32 *ptr );
DECLSPEC	INT32	__stdcall	NcGetTcpIpStatus( INT32 no, INT32 fc, INT32 *ptr );

// CIF
DECLSPEC	INT32	__stdcall	NcInitialCifServer( void );
DECLSPEC	INT32	__stdcall	NcCifWrite( INT32 no, const UINT8* buf, INT32 size );
DECLSPEC	INT32	__stdcall	NcCifRead( INT32 no, UINT8* buf, INT32 size );

// GMM
DECLSPEC	int		__stdcall	NcGmmGetFunctionCode( int* code );
DECLSPEC	int		__stdcall	NcGmmServerInitialize( void );
DECLSPEC	int		__stdcall	NcGmmServerWrite( int index, UINT8* pData, UINT32 size, UINT32 mode );
DECLSPEC	int		__stdcall	NcGmmServerEntryFunctionReceivePacket( void (__stdcall *pFunc)( UINT8, const UINT8*, UINT32 ) );
DECLSPEC	int		__stdcall	NcGmmClientInitialize( void );
DECLSPEC	int		__stdcall	NcGmmClientWrite( int index, UINT8* pData, UINT32 size, UINT32 mode );
DECLSPEC	int		__stdcall	NcGmmClientEntryFunctionReceivePacket( void (__stdcall *pFunc)( UINT8, const UINT8*, UINT32 ) );
DECLSPEC	int		__stdcall	NcGmmClientConnectServer( const char* hostname, int index );

// LOG
DECLSPEC	INT32	__stdcall	NcInitialLogOutput( INT32 status, UINT8 *appName );
DECLSPEC	INT32	__stdcall	NcOutputLogData( INT32 logType, INT32 funcType, INT32 level, UINT8 *data );

// MCU
DECLSPEC	INT32	__stdcall	NcInitialMcu( INT32 is_server );
DECLSPEC	INT32	__stdcall	NcConnectMcuServer( void );
DECLSPEC	INT32	__stdcall	NcSendMcuCommand( UINT8 *buf );

// NAVNETDLL RADAR

typedef struct{
	UINT16	status1;
	UINT16	status2;
	INT16	count;
	INT32	ccrp_dist;
	INT16	ccrp_bear;
	INT32	ant_dist;
	INT16	ant_bear;
	INT16	t_speed;
	INT16	t_course;
	INT16	r_speed;
	INT16	r_course;
	INT32	cpa;
	INT32	tcpa;
	INT32	bcr;
	INT32	bct;
	INT16	antno;
} ARPA;

typedef	struct{
	UINT16	status1;
	UINT16	status2;
	INT16	count;
	INT32	ccrp_x;
   	INT32	ccrp_y;
	INT32	t_vx;
	INT32	t_vy;
	INT32	r_vx;
	INT32	r_vy;
	INT32	ant_x;
   	INT32	ant_y;
	INT16	antno;
} ARPA_TARGET;

typedef	struct{
	INT32			cpa;
	INT32			tcpa;
	INT32			bcr;
	INT32			bct;
	INT16			antno;
} ARPA_JUDGE;

typedef	struct{
	INT16			echo[32];	
	INT16			min_w;		
	UINT16			scan_time;	
	INT16			fe_status;	
	INT16			antno;
} ARPA_ANALYSE_DATA;

#pragma pack(push,8)
typedef struct RadarSweepMetadata_s {
	INT16 angle;
	UINT16 heading;
	INT16 headingFlag;
	INT16 radarNo;
	INT16 range;
	INT16 scale;
	INT16 sweepLength;
	INT16 status;
}RadarSweepMetadata;
#pragma pack(pop)

typedef enum
{
    None,
    LicenseError,
    InitError,
} FecError;

// FLM
DECLSPEC	FecError	__stdcall	Initialize( UINT8* vendor, UINT8* password, UINT8* id, UINT8* device, UINT8* IpAdd );
DECLSPEC	FecError	__stdcall	Initialize_Trial( void );
DECLSPEC	FecError	__stdcall	FlmCertificateEquipment (const UINT8 *equipment_str, UINT8 *product_str, INT32 *status);

// RADAR COMMON
DECLSPEC	INT32	__stdcall	RmSetServerClientMode( INT32 mode );
DECLSPEC	INT32	__stdcall	RmGetServerClientMode( INT32 *mode );
DECLSPEC	INT32	__stdcall	RmInitialModule( UINT8 *hname, void(__stdcall *func)(INT32 cmdId, INT32 radarNo) );
DECLSPEC	INT32	__stdcall	RmEndModule( INT32 param );
DECLSPEC	INT32	__stdcall	RmSetCommandCallbackFunc( void(__stdcall *func)(INT32 cmdId, INT32 radarNo) );
DECLSPEC	INT32	__stdcall	RmGetModuleVersionNo( UINT8 *version );
DECLSPEC	INT32	__stdcall	RmSetCommandSetting( INT32 cmdId, INT32 status );
DECLSPEC	INT32	__stdcall	RmGetCommandSetting( INT32 cmdId, INT32 *status );
DECLSPEC	INT32	__stdcall	RmSetCycleCallbackTick( INT32 tick );
DECLSPEC	INT32	__stdcall	RmSetCycleCallbackFunc( void(__stdcall *func)(void) );
DECLSPEC	INT32	__stdcall	RmSetFreeCommandCallbackFunc( void(__stdcall *func)( INT32 radarNo, UINT8 *freeCmdId, UINT8 *text) );
DECLSPEC	INT32	__stdcall	RmSetGetFreeCommandDataCallbackFunc( INT32(__stdcall *func)( INT32 radarNo, UINT8 *freeCmdId, UINT8 *text) );
DECLSPEC	INT32	__stdcall	RmGetConnection( INT32 *status );
DECLSPEC	INT32	__stdcall	RmGetConnectionEx( INT32 *status, INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmGetAliveFlag( INT32 *num, UINT32 *aliveTbl );
DECLSPEC	INT32	__stdcall	RmSetRadarSource( INT32 radarNo, UINT8 *hostname, INT32 rangeNo );
DECLSPEC	INT32	__stdcall	RmGetRadarSource( INT32 radarNo, UINT8 *hostname, INT32 *rangeNo );
DECLSPEC	INT32	__stdcall	RmChangeHost( UINT8 *hostname );
DECLSPEC	INT32	__stdcall	RmGetHost( UINT8 *hostname );
DECLSPEC	INT32	__stdcall	RmGetSensorHostCount( INT32 *cnt );
DECLSPEC	INT32	__stdcall	RmGetSensorHostName( INT32 no, UINT8 *name );
DECLSPEC	void	__stdcall	RmInitializeImoEcho( void );
DECLSPEC	INT32	__stdcall	DecodeImoEchoFormat( UINT8 buf[], INT32 size, INT32 sensorNo);
DECLSPEC	INT32	__stdcall	RmSetEchoCallbackFunc( void(__stdcall *func)( INT32 radarNo, INT16 status, UINT8 *echo, INT16 sweep_len, INT16 scale, INT16 range, INT16 angle, INT16 heading, INT16 h_flg) );
DECLSPEC	INT32	__stdcall	RmSetEchoInterpolationFunc( INT32 status );
DECLSPEC	INT32	__stdcall	RmSetInterpolationSweepNum( INT32 num );
DECLSPEC	INT32	__stdcall	RmSetEchoSweepTotal( INT32 status );
DECLSPEC	INT32	__stdcall	RmSetLowLevelEcho( INT32 status );
DECLSPEC	INT32	__stdcall	RmSetTileEchoCallbackFunc( void(__stdcall *func)( INT32 radarNo, INT16 heading, INT16 headingStatus, INT16 range, INT16 rangeUnit, INT16 scale, INT16 xPos, INT16 yPos, INT16 sweepCenterX, INT16 sweepCenterY, INT16 tileSize, unsigned char *echo) );
DECLSPEC	INT32	__stdcall	RmSetTxEchoFormat( INT32 echoType, INT32 encType, INT32 txType );
DECLSPEC	INT32	__stdcall	RmGetTxEchoFormat( INT32 *echoType, INT32 *encType, INT32 *txType );
DECLSPEC	INT32	__stdcall	RmTransmitEcho( INT32 radarNo, UINT8 *echo, INT16 sweepLength, INT16 scale, INT16 range, INT16 rangeUnit, INT16 angle, INT16 heading, INT16 h_flg );
DECLSPEC	INT32	__stdcall	RmSetGzAlarmCallbackFunc(void(__stdcall *func)(INT32 type, INT32 result));
DECLSPEC	INT32	__stdcall	RmResetGzResult ( INT16 gzNo );
DECLSPEC	INT32	__stdcall	RmSetGzTargetZone( INT16 gzNo, INT16 onoff, INT32 start_dist, INT16 start_angle, INT32 end_dist, INT16 end_angle );
DECLSPEC	INT32	__stdcall	RmSetGzDetectLevel( INT16 gzNo, INT16 level );
DECLSPEC	INT32	__stdcall	RmSetGzDetectSize( INT16 gzNo, INT16 size );
DECLSPEC	INT32	__stdcall	RmSetGzSizeTbl( INT16 gzNo, UINT8 *tbl );
DECLSPEC	INT32	__stdcall	RmSetGzJudgeType( INT16 gzNo, INT16 type );
DECLSPEC	INT32	__stdcall	RmSetGzJudgeCount( INT16 gzNo, INT16 count );
DECLSPEC	INT32	__stdcall	RmGetGzTargetZone( INT16 gzNo, INT16 *onoff, INT32 *start_dist, INT16 *start_angle, INT32 *end_dist, INT16 *end_angle );
DECLSPEC	INT32	__stdcall	RmGetGzDetectLevel( INT16 gzNo, INT16 *level );
DECLSPEC	INT32	__stdcall	RmGetGzDetectSize( INT16 gzNo, INT16 *size );
DECLSPEC	INT32	__stdcall	RmGetGzJudgeType( INT16 gzNo, INT16 *type );
DECLSPEC	INT32	__stdcall	RmGetGzJudgeCount( INT16 gzNo, INT16 *count );
DECLSPEC	INT32	__stdcall	RmGetGzJudgeResult( INT16 gzNo, INT16 radarNo, INT16 *result );
DECLSPEC	INT32	__stdcall	RmGetGzDetectResult( INT16 gzNo, INT16 radarNo, INT16 *detect );
DECLSPEC	INT32	__stdcall	RmGetGzJudgeOutOfRange( INT16 gzNo, INT16 radarNo, INT16 *status );
DECLSPEC	INT32	__stdcall	RmGetGuardZoneStatus( INT32 gzNo, INT32 *status );
DECLSPEC	INT32	__stdcall	RmResetGuardZoneStatus( INT16 gzNo );

// RADAR SET
DECLSPEC	INT32	__stdcall	RmcSetFreeCommand( INT32 radarNo, UINT8 *id, UINT8 *text );
DECLSPEC	INT32	__stdcall	RmcSetConnect( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetDispMode( INT32 radarNo, UINT16 status, UINT16 dir );
DECLSPEC	INT32	__stdcall	RmcSetRange( INT32 radarNo, UINT16 range, UINT16 unit );
DECLSPEC	INT32	__stdcall	RmcSetRange_v2(INT32 radarNo, UINT16 range, UINT16 unit, UINT16 unitShort);
DECLSPEC	INT32	__stdcall	RmcSetGAIN( INT32 radarNo, UINT16 mode, UINT16 manualData, UINT16 autoData );
DECLSPEC	INT32	__stdcall	RmcSetCustomGAIN( INT32 radarNo, INT32 pictureNo, UINT16 mode, UINT16 manualData, UINT16 autoData );
DECLSPEC	INT32	__stdcall	RmcSetACSEA( INT32 radarNo, UINT16 mode, UINT16 manualData, UINT16 autoData, UINT16 autoMode );
DECLSPEC	INT32	__stdcall	RmcSetCustomACSEA( INT32 radarNo, INT32 pictureNo, UINT16 mode, UINT16 manualData, UINT16 autoData, UINT16 autoMode );
DECLSPEC	INT32	__stdcall	RmcSetACRAIN( INT32 radarNo, UINT16 mode, UINT16 manualData, UINT16 autoData, UINT16 ftcMode, UINT16 autoRainMode );
DECLSPEC	INT32	__stdcall	RmcSetCustomACRAIN( INT32 radarNo, INT32 pictureNo, UINT16 mode, UINT16 manualData, UINT16 autoData, UINT16 ftcMode, UINT16 autoRainMode );
DECLSPEC	INT32	__stdcall	RmcSetCustomPictureAll( INT32 radarNo, INT32 pictureNo, INT16 dateTable[] );
DECLSPEC	INT32	__stdcall	RmcSetCustomPicture( INT32 radarNo, INT32 pictureNo, INT32 funcNo, INT16 data );
DECLSPEC	INT32	__stdcall	RmcSetPulseWidth( INT32 radarNo, INT16 pulse, INT16 range, UINT16 unit, INT32 pictureNo );
DECLSPEC	INT32	__stdcall	RmcSetTxStby( INT32 radarNo, UINT16 mode, UINT16 wman, UINT32 sendTime, UINT32 stopTime );
DECLSPEC	INT32	__stdcall	RmcSetArpaSelectTarget( INT32 radarNo, INT32 x, INT32 y, INT32 targetNo );
DECLSPEC	INT32	__stdcall	RmcSetArpaAcquireTarget( INT32 radarNo, INT32 x, INT32 y, INT32 mode );
DECLSPEC	INT32	__stdcall	RmcSetArpaCancelTarget( INT32 radarNo, INT32 x, INT32 y );
DECLSPEC	INT32	__stdcall	RmcSetArpaDispMode( INT32 radarNo, UINT16 status, UINT16 mode, UINT16 vector, UINT16 autop, UINT16 manualp, UINT16 trackingBase );
DECLSPEC	INT32	__stdcall	RmcSetAntennaType( INT32 radarNo, UINT16 type, UINT16 pos, UINT16 output, UINT16 length, UINT16 updown, UINT16 atype, UINT16 deviceType );
DECLSPEC	INT32	__stdcall	RmcSetKeyCommand( INT32 radarNo, UINT8 *command );
DECLSPEC	INT32	__stdcall	RmcSetGuardStatus( INT32 radarNo, UINT16 count, UINT16 status0, UINT16 status1 );
DECLSPEC	INT32	__stdcall	RmcSetLostAck( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcSetArpaVector( INT32 radarNo, UINT16 mode, UINT16 time );
DECLSPEC	INT32	__stdcall	RmcSetArpaCpaTcpa( INT32 radarNo, UINT16 sw, UINT16 cpa, UINT16 tcpa, INT32 d_cpa, INT32 d_tcpa );
DECLSPEC	INT32	__stdcall	RmcSetTune( INT32 radarNo, UINT16 mode, UINT16 manualData );
DECLSPEC	INT32	__stdcall	RmcSetTuneIndicator( INT32 radarNo, UINT16 value, UINT16 errFlag);
DECLSPEC	INT32	__stdcall	RmcSetBlindSector( INT32 radarNo, UINT16 status, UINT16 startDir0, UINT16	startDir1, UINT16 sector0, UINT16 sector1);
DECLSPEC	INT32	__stdcall	RmcSetRadarPosition( INT32 radarNo, INT32 monitorNo, INT16 top, INT16 left);
DECLSPEC	INT32	__stdcall	RmcSetArpaTargetAllClear( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcSetTuneAdjust( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetArpaCancelTargetId( INT32 radarNo, INT32 targetNo );
DECLSPEC	INT32	__stdcall	RmcSetReferencePoint( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetAlarmStatus( INT32 radarNo, UINT16 status, UINT32 mtr_status, UINT32 pm_status, UINT32 rfc_status );
DECLSPEC	INT32	__stdcall	RmcSetHeading( INT32 radarNo, UINT16 mode, UINT16 heading );
DECLSPEC	INT32	__stdcall	RmcSetSelectQVDisplay( INT32 radarNo, UINT16 sw );
DECLSPEC	INT32	__stdcall	RmcSetATT( INT32 radarNo, INT32 monitorNo, UINT16 status, INT16 value );
DECLSPEC	INT32	__stdcall	RmcSetHeadingAdjust( INT32 radarNo, INT32 monitorNo, UINT16 angle, INT16 offset );
DECLSPEC	INT32	__stdcall	RmcSetTimingAdjust( INT32 radarNo, INT32 monitorNo, UINT16 mode, UINT16 timing, UINT16 autoData, UINT16 offset );
DECLSPEC	INT32	__stdcall	RmcSetMBSAdjust( INT32 radarNo, UINT16 mbs, INT16 pulse );
DECLSPEC	INT32	__stdcall	RmcSetAntennaHeight( INT32 radarNo, INT32 monitorNo, UINT16 height, UINT16 height2 );
DECLSPEC	INT32	__stdcall	RmcSetNearSTC( INT32 radarNo, UINT16 curve );
DECLSPEC	INT32	__stdcall	RmcSetMiddleSTC( INT32 radarNo, UINT16 curve );
DECLSPEC	INT32	__stdcall	RmcSetFarSTC( INT32 radarNo, UINT16 curve );
DECLSPEC	INT32	__stdcall	RmcSetRingSuppression( INT32 radarNo, UINT16 data );
DECLSPEC	INT32	__stdcall	RmcSetAntennaRevolution( INT32 radarNo, UINT16 speed, UINT16 highRotationMode );
DECLSPEC	INT32	__stdcall	RmcSetAntennaSwitch( INT32 radarNo, UINT16 sw );
DECLSPEC	INT32	__stdcall	RmcSetStoppedTx( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetMonitorSelect( INT32 radarNo, INT32 monitor );
DECLSPEC	INT32	__stdcall	RmcSetAntennaNo( INT32 radarNo, UINT16 ant );
DECLSPEC	INT32	__stdcall	RmcSetOnTime( INT32 radarNo, UINT32 time );
DECLSPEC	INT32	__stdcall	RmcSetTxTime( INT32 radarNo, UINT32 time );
DECLSPEC	INT32	__stdcall	RmcSetPmAdjust( INT32 radarNo, UINT16 gain, INT32 position, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetDemoEcho( INT32 radarNo, UINT16 echo );
DECLSPEC	INT32	__stdcall	RmcSetEavWithoutGyro( INT32 radarNo, UINT16 select );
DECLSPEC	INT32	__stdcall	RmcSet2ndEchoReject( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetPM( INT32 radarNo, UINT16 status, INT16 arc );
DECLSPEC	INT32	__stdcall	RmcSetSART( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetVersionNo( INT32 radarNo, UINT8 *app, UINT8 *start, UINT8 *boot, UINT8 *fpga, UINT8 *napp, UINT8 *nstart, UINT8 *nboot );
DECLSPEC	INT32	__stdcall	RmcSetArpaTest( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetGuardMode( INT32 radarNo, INT32 zoneNo, UINT16 fanStab, UINT16 polygonStab, UINT16 aroundCheckArea );
DECLSPEC	INT32	__stdcall	RmcSetGuardFan( INT32 radarNo, INT32 zoneNo, INT32 startDist, INT32 startDir, INT32 endDist, INT32 endDir );
DECLSPEC	INT32	__stdcall	RmcSetGuardPolygon( INT32 radarNo, INT32 zoneNo, UINT16 total, INT32 dist[], INT32 dir[], INT32 lat[], INT32 lon[] );
DECLSPEC	INT32	__stdcall	RmcSetArpaWithoutGyro( INT32 radarNo, UINT16 select );
DECLSPEC	INT32	__stdcall	RmcSetSpeedSelect( INT32 radarNo, UINT16 mode, UINT16 speed );
DECLSPEC	INT32	__stdcall	RmcSetDrift( INT32 radarNo, UINT16 mode, UINT16 set, UINT16 drift );
DECLSPEC	INT32	__stdcall	RmcSetGC10( INT32 radarNo, UINT16 offset );
DECLSPEC	INT32	__stdcall	RmcSetArpaCancelStatus( INT32 radarNo, INT32 targetNo );
DECLSPEC	INT32	__stdcall	RmcSetArpaPreset( INT32 radarNo, INT32 dataNo, UINT16 data );
DECLSPEC	INT32	__stdcall	RmcSetNextEcho(INT32 radarNo, UINT16 sw);
DECLSPEC	INT32	__stdcall	RmcSetTrailMode( INT32 radarNo, UINT16 mode, UINT16 grad, UINT16 narrow, UINT16 level, UINT16 range, UINT16 copy, UINT16 os, UINT16 time );
DECLSPEC	INT32	__stdcall	RmcSetTrailClear( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcSetArpaCancelTargetNo( INT32 radarNo, INT32 targetNo );
DECLSPEC	INT32	__stdcall	RmcSetSweepPosition( INT32 radarNo, INT32 x, INT32 y );
DECLSPEC	INT32	__stdcall	RmcSetArpaAcquireTargetDisplayId( INT32 radarNo, INT32 x, INT32 y, INT32 targetNo, INT32 id );
DECLSPEC	INT32	__stdcall	RmcSetArpaCancelTargetDisplayId( INT32 radarNo, INT32 targetNo );
DECLSPEC	INT32	__stdcall	RmcSetGpsAntennaPosition( INT32 radarNo, INT32 antennaNo, INT16 bow, INT16 port );
DECLSPEC	INT32	__stdcall	RmcSetConningPosition( INT32 radarNo, UINT16 bow, UINT16 port );
DECLSPEC	INT32	__stdcall	RmcSetSelectAntenna( INT32 radarNo, UINT16 no );
DECLSPEC	INT32	__stdcall	RmcSetWakeupCount( INT32 radarNo, UINT16 count );
DECLSPEC	INT32	__stdcall	RmcSetShipInfo( INT32 radarNo, UINT16 length, UINT16 width );
DECLSPEC	INT32	__stdcall	RmcSetAisAlarm( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetArpaAlarm( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetSelectAlarmOut( INT32 radarNo, UINT16 ch, UINT16 code, UINT16 sw );
DECLSPEC	INT32	__stdcall	RmcSetAlarmAck( INT32 radarNo, UINT16 identifier, UINT16 instance, UINT8 command );
DECLSPEC	INT32	__stdcall	RmcSetSelectCrt( INT32 radarNo, UINT16 crt );
DECLSPEC	INT32	__stdcall	RmcSetSelectAlarmPolarity( INT32 radarNo, INT32 channel, UINT16 sw );
DECLSPEC	INT32	__stdcall	RmcSetCombineStatus( INT32 radarNo, UINT16 status, INT16 antno );
DECLSPEC	INT32	__stdcall	RmcSetHeadingSource( INT32 radarNo, UINT16 source );
DECLSPEC	INT32	__stdcall	RmcSetNetworkPriority( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetMenuSet( INT32 radarNo, UINT16 menuNo, UINT16 data );
DECLSPEC	INT32	__stdcall	RmcSetImoEchoSwitch( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetExtAlarmAck( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetPmAdjustEnd( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetSelectTtmOut( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetArpaMaxRange( INT32 radarNo, UINT16 maxRange );
DECLSPEC	INT32	__stdcall	RmcSetArpaEchoLevel( INT32 radarNo, UINT16 level );
DECLSPEC	INT32	__stdcall	RmcSetArpaAllClear( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcSetArpaLandSize( INT32 radarNo, UINT16 size );
DECLSPEC	INT32	__stdcall	RmcSetArpaAntenna( INT32 radarNo, UINT16 kind );
DECLSPEC	INT32	__stdcall	RmcSetArpaAcqCorre( INT32 radarNo, UINT16 scan );
DECLSPEC	INT32	__stdcall	RmcSetArpaWeed( INT32 radarNo, UINT16 weed );
DECLSPEC	INT32	__stdcall	RmcSetArpaGateSize( INT32 radarNo, UINT16 size );
DECLSPEC	INT32	__stdcall	RmcSetArpaFilterRespons( INT32 radarNo, UINT16 res );
DECLSPEC	INT32	__stdcall	RmcSetArpaLostCount( INT32 radarNo, UINT16 count );
DECLSPEC	INT32	__stdcall	RmcSetArpaMaxSpeed( INT32 radarNo, UINT16 speed );
DECLSPEC	INT32	__stdcall	RmcSetArpaTimeVector( INT32 radarNo, UINT16 mode, UINT16 count );
DECLSPEC	INT32	__stdcall	RmcSetOperatorFitness( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcSetPictureDefault( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcSetRfcVersionNo( INT32 radarNo, UINT8 *version );
DECLSPEC	INT32	__stdcall	RmcSetCombineData( INT32 radarNo, INT16 mode, INT16 start_angle, INT16 angle, INT16 start_length, INT16 length );
DECLSPEC	INT32	__stdcall	RmcSetInterswInfo( INT32 radarNo, UINT16 interswTable[] );
DECLSPEC	INT32	__stdcall	RmcSetArpaLostFilterRange( INT32 radarNo, UINT16 status, INT32 rangeNM, INT32 range );
DECLSPEC	INT32	__stdcall	RmcSetArpaLostFilterSpeed( INT32 radarNo, UINT16 status, INT32 speed );
DECLSPEC	INT32	__stdcall	RmcSetStcRange( INT32 radarNo, INT16 range );
DECLSPEC	INT32	__stdcall	RmcSetCustomMemory( INT32 radarNo, UINT16 action );
DECLSPEC	INT32	__stdcall	RmcSetBuildUpTime( INT32 radarNo, INT32 buildUpTime );
DECLSPEC	INT32	__stdcall	RmcSetDisplayUnitInformation( INT32 radarNo, UINT16 antennaNo, UINT16 master, UINT8 *hostname );
DECLSPEC	INT32	__stdcall	RmcSetArpaLostFilterMode( INT32 radarNo, UINT16 mode );
DECLSPEC	INT32	__stdcall	RmcSetAssociationStatus( INT32 radarNo, INT32 status );
DECLSPEC	INT32	__stdcall	RmcSetEchoClear( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcSetCancelSelectTarget( INT32 radarNo, INT16 startTargetID, INT16 endTargetID );
DECLSPEC	INT32	__stdcall	RmcSetCancelSelectLostTarget( INT32 radarNo, INT16 startTargetID, INT16 endTargetID );
DECLSPEC	INT32	__stdcall	RmcSetCustomAtfParameter(INT32 radarNo, INT32 pictureNo, UINT16 sw, UINT16 gainData, INT16 seaData, INT16 rainData, UINT16 hGain, UINT16 mode);
DECLSPEC	INT32	__stdcall	RmcSetTrailProcess( INT32 radarNo, UINT16 mode );
DECLSPEC	INT32	__stdcall	RmcSetArpaReAcquireTarget( INT32 radarNo, INT32 targetNo, INT32 targetId, INT32 x, INT32 y, UINT16 speed, UINT16 course, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetAliveCheck( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcSetTxEchoSelect( INT32 radarNo, UINT16 select );
DECLSPEC	INT32	__stdcall	RmcSetRxEchoSelect( INT32 radarNo, UINT16 select, UINT8 *ip );
DECLSPEC	INT32	__stdcall	RmcSetRsPreset( INT32 radarNo, INT32 dataNo, UINT16 data );
DECLSPEC	INT32	__stdcall	RmcSetStartSelfMode( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcSetJummingAble( INT32 radarNo, UINT16 mode );
DECLSPEC	INT32	__stdcall	RmcSetIceClass( INT32 radarNo, UINT16 mode, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetAtfSettings( INT32 radarNo, UINT16 adjust, UINT16 highGain, UINT16 cutLevel, UINT16 startDir, UINT16 sector );
DECLSPEC	INT32	__stdcall	RmcSetRsAllClear( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcSetSsdTxChannel( INT32 radarNo, UINT16 channel );
DECLSPEC	INT32	__stdcall	RmcSetHighSensitivityMode( INT32 radarNo, UINT16 mode );
DECLSPEC	INT32	__stdcall	RmcSetBearingResolutionSelect( INT32 radarNo, UINT16 mode );
DECLSPEC	INT32	__stdcall	RmcSetEchoColorType( INT32 radarNo, UINT16 mode, UINT16 sw, UINT16 autoPicture );
DECLSPEC	INT32	__stdcall	RmcSetAccuShip( INT32 radarNo, UINT16 sw );
DECLSPEC	INT32	__stdcall	RmcSetObservationMode( INT32 radarNo, UINT16 mode );
DECLSPEC	INT32	__stdcall	RmcSetSARTSignal( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetImageCor(INT32 radarNo, UINT16 dummy);
DECLSPEC	INT32	__stdcall	RmcSetNN3Command( INT32 radarNo, INT32 sub, UINT32 param1, UINT16 param2, UINT16 param3, UINT16 param4 );
DECLSPEC	INT32	__stdcall	RmcSetMemory( INT32 radarNo, UINT16 status, UINT16 memory );
DECLSPEC	INT32	__stdcall	RmcSetEquipComp( INT32 radarNo, UINT16 status );
DECLSPEC	INT32	__stdcall	RmcSetPulseRange( INT32 radarNo, UINT16 min, UINT16 max );
DECLSPEC	INT32	__stdcall	RmcSetMagnetron( INT32 radarNo, UINT16 magne, UINT16 status, UINT16 result );
DECLSPEC	INT32	__stdcall	RmcSetMagnetronInd( INT32 radarNo, UINT16 ind );
DECLSPEC	INT32	__stdcall	RmcSetArpaDetect( INT32 radarNo, UINT16 range, UINT16 level, UINT16 size );
DECLSPEC	INT32	__stdcall	RmcSetArpaCatch( INT32 radarNo, UINT16 landsize, UINT16 correl, UINT16 cancel, UINT16 gate );
DECLSPEC	INT32	__stdcall	RmcSetArpaPursuit( INT32 radarNo, UINT16 gate, UINT16 filter, UINT16 lost, UINT16 speed, UINT16 mode, UINT16 unit, UINT16 data );
DECLSPEC	INT32	__stdcall	RmcSetRangeSelect( INT32 radarNo, INT32 unit, UINT16 range[] );
DECLSPEC	INT32	__stdcall	RmcSetVideoLevel( INT32 radarNo, UINT16 status, UINT16 data );

// RADAR GET
DECLSPEC	INT32	__stdcall	RmcGetConnect( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetDispMode( INT32 radarNo, UINT16 *status, UINT16 *dir );
DECLSPEC	INT32	__stdcall	RmcGetRange( INT32 radarNo, INT16 *range, UINT16 *unit );
DECLSPEC	INT32	__stdcall	RmcGetRange_v2(INT32 radarNo, INT16 *range, UINT16 *unit, UINT16 *unitShort);
DECLSPEC	INT32	__stdcall	RmcGetGAIN( INT32 radarNo, UINT16 *mode, UINT16 *manualData, UINT16 *autoData );
DECLSPEC	INT32	__stdcall	RmcGetCustomGAIN( INT32 radarNo, INT32 *pictureNo, UINT16 *mode, UINT16 *manualData, UINT16 *autoData );
DECLSPEC	INT32	__stdcall	RmcGetACSEA( INT32 radarNo, UINT16 *mode, UINT16 *manualData, UINT16 *autoData, UINT16 *autoMode );
DECLSPEC	INT32	__stdcall	RmcGetCustomACSEA( INT32 radarNo, INT32 *pictureNo, UINT16 *mode, UINT16 *manualData, UINT16 *autoData, UINT16 *autoMode );
DECLSPEC	INT32	__stdcall	RmcGetACRAIN( INT32 radarNo, UINT16 *mode, UINT16 *manualData, UINT16 *autoData, UINT16 *ftcMode, UINT16 *autoRainMode );
DECLSPEC	INT32	__stdcall	RmcGetCustomACRAIN( INT32 radarNo, INT32 *pictureNo, UINT16 *mode, UINT16 *manualData, UINT16 *autoData, UINT16 *ftcMode, UINT16 *autoRainMode );
DECLSPEC	INT32	__stdcall	RmcGetCustomPictureAll( INT32 radarNo, INT32 *pictureNo, INT16 *dataTable );
DECLSPEC	INT32	__stdcall	RmcGetCustomPicture( INT32 radarNo, INT32 *pictureNo, INT32 *funcNo, INT16 *data );
DECLSPEC	INT32	__stdcall	RmcGetPulseWidth( INT32 radarNo, INT16 *pulse, INT16 *range, UINT16 *unit, INT32 *pictureNo );
DECLSPEC	INT32	__stdcall	RmcGetTxStby( INT32 radarNo, UINT16 *mode, UINT16 *wman, UINT32 *sendTime, UINT32 *stopTime );
DECLSPEC	INT32	__stdcall	RmcGetArpaSelectTarget( INT32 radarNo, INT32 *x, INT32 *y, INT32 *targetNo );
DECLSPEC	INT32	__stdcall	RmcGetArpaAcquireTarget( INT32 radarNo, INT32 *x, INT32 *y, INT32 *mode );
DECLSPEC	INT32	__stdcall	RmcGetArpaCancelTarget( INT32 radarNo, INT32 *x, INT32 *y );
DECLSPEC	INT32	__stdcall	RmcGetArpaDispMode( INT32 radarNo, UINT16 *status, UINT16 *mode, UINT16 *vector, UINT16 *autop, UINT16 *manualp, UINT16 *trackingBase );
DECLSPEC	INT32	__stdcall	RmcGetAntennaType( INT32 radarNo, UINT16 *model, UINT16 *posn, UINT16 *output, UINT16 *length, UINT16 *updown, UINT16 *atype, UINT16 *deviceType );
DECLSPEC	INT32	__stdcall	RmcGetKeyCommand( INT32 radarNo, UINT8 *command );
DECLSPEC	INT32	__stdcall	RmcGetGuardStatus( INT32 radarNo, UINT16 *count, UINT16 *status0, UINT16 *status1 );
DECLSPEC	INT32	__stdcall	RmcGetArpaVector( INT32 radarNo, UINT16 *mode, UINT16 *time );
DECLSPEC	INT32	__stdcall	RmcGetArpaCpaTcpa( INT32 radarNo, UINT16 *sw, UINT16 *cpa, UINT16 *tcpa, INT32 *d_cpa, INT32 *d_tcpa );
DECLSPEC	INT32	__stdcall	RmcGetTune( INT32 radarNo, UINT16 *mode, UINT16 *manualData );
DECLSPEC	INT32	__stdcall	RmcGetTuneIndicator( INT32 radarNo, UINT16 *value, UINT16 *errFlag );
DECLSPEC	INT32	__stdcall	RmcGetBlindSector( INT32 radarNo, UINT16 *status, UINT16 *startDir0, UINT16 *startDir1, UINT16 *sector0, UINT16 *sector1 );
DECLSPEC	INT32	__stdcall	RmcGetRadarPosition( INT32 radarNo, INT32 *monitorNo, INT16 *top, INT16 *left );
DECLSPEC	INT32	__stdcall	RmcGetTuneAdjust( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetArpaCancelTargetId( INT32 radarNo, INT32 *targetNo );
DECLSPEC	INT32	__stdcall	RmcGetReferencePoint( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetAlarmStatus( INT32 radarNo, UINT16 *status, UINT32 *mtr_status, UINT32 *pm_status, UINT32 *rfc_status );
DECLSPEC	INT32	__stdcall	RmcGetHeading( INT32 radarNo, UINT16 *mode, UINT16 *heading );
DECLSPEC	INT32	__stdcall	RmcGetSelectQVDisplay( INT32 radarNo, UINT16 *sw );
DECLSPEC	INT32	__stdcall	RmcGetATT( INT32 radarNo, INT32 *monitorNo, UINT16 *status, INT16 *value );
DECLSPEC	INT32	__stdcall	RmcGetHeadingAdjust( INT32 radarNo, INT32 *monitorNo, UINT16 *angle, INT16 *offset );
DECLSPEC	INT32	__stdcall	RmcGetTimingAdjust( INT32 radarNo, INT32 *monitorNo, UINT16 *mode, UINT16 *timing, UINT16 *autoData, UINT16 *offset );
DECLSPEC	INT32	__stdcall	RmcGetMBSAdjust( INT32 radarNo, UINT16 *mbs, INT16 *pulse );
DECLSPEC	INT32	__stdcall	RmcGetAntennaHeight( INT32 radarNo, INT32 *monitorNo, UINT16 *height, UINT16 *height2 );
DECLSPEC	INT32	__stdcall	RmcGetNearSTC( INT32 radarNo, UINT16 *curve );
DECLSPEC	INT32	__stdcall	RmcGetMiddleSTC( INT32 radarNo, UINT16 *curve );
DECLSPEC	INT32	__stdcall	RmcGetFarSTC( INT32 radarNo, UINT16 *curve );
DECLSPEC	INT32	__stdcall	RmcGetRingSuppression( INT32 radarNo, UINT16 *data );
DECLSPEC	INT32	__stdcall	RmcGetAntennaRevolution( INT32 radarNo, UINT16 *speed, UINT16 *highRotationMode );
DECLSPEC	INT32	__stdcall	RmcGetAntennaSwitch( INT32 radarNo, UINT16 *sw );
DECLSPEC	INT32	__stdcall	RmcGetStoppedTx( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetMonitorSelect( INT32 radarNo, INT32 *monitor );
DECLSPEC	INT32	__stdcall	RmcGetAntennaNo( INT32 radarNo, UINT16 *no );
DECLSPEC	INT32	__stdcall	RmcGetOnTime( INT32 radarNo, UINT32 *time );
DECLSPEC	INT32	__stdcall	RmcGetTxTime( INT32 radarNo, UINT32 *time );
DECLSPEC	INT32	__stdcall	RmcGetPmAdjust( INT32 radarNo, UINT16 *gain, INT32 *position, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetDemoEcho( INT32 radarNo, UINT16 *echo );
DECLSPEC	INT32	__stdcall	RmcGetEavWithoutGyro( INT32 radarNo, UINT16 *select );
DECLSPEC	INT32	__stdcall	RmcGet2ndEchoReject( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetPM( INT32 radarNo, UINT16 *status, INT16 *arc );
DECLSPEC	INT32	__stdcall	RmcGetSART( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetVersionNo( INT32 radarNo, UINT8 *app, UINT8 *start, UINT8 *boot, UINT8 *fpga, UINT8 *napp, UINT8 *nstart, UINT8 *nboot );
DECLSPEC	INT32	__stdcall	RmcGetArpaTest( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetGuardMode( INT32 radarNo, INT32 *zoneNo, UINT16 *fanStab, UINT16 *polygonStab, UINT16 *aroundCheckArea );
DECLSPEC	INT32	__stdcall	RmcGetGuardFan( INT32 radarNo, INT32 *zoneNo, INT32 *startDist, INT32 *startDir, INT32 *endDist, INT32 *endDir );
DECLSPEC	INT32	__stdcall	RmcGetGuardPolygon( INT32 radarNo, INT32 *zoneNo, UINT16 *total, INT32 dist[], INT32 dir[], INT32 lat[], INT32 lon[] );
DECLSPEC	INT32	__stdcall	RmcGetArpaWithoutGyro( INT32 radarNo, UINT16 *select );
DECLSPEC	INT32	__stdcall	RmcGetSpeedSelect( INT32 radarNo, UINT16 *mode, UINT16 *speed );
DECLSPEC	INT32	__stdcall	RmcGetDrift( INT32 radarNo, UINT16 *mode, UINT16 *set, UINT16 *drift );
DECLSPEC	INT32	__stdcall	RmcGetGC10( INT32 radarNo, UINT16 *offset );
DECLSPEC	INT32	__stdcall	RmcGetArpaCancelStatus( INT32 radarNo, INT32 *targetNo );
DECLSPEC	INT32	__stdcall	RmcGetArpaPreset( INT32 radarNo, INT32 *dataNo, UINT16 *data );
DECLSPEC	INT32	__stdcall	RmcGetNextEcho(INT32 radarNo, UINT16 *sw);
DECLSPEC	INT32	__stdcall	RmcGetTrailMode( INT32 radarNo, UINT16 *mode, UINT16 *grad, UINT16 *narrow, UINT16 *level, UINT16 *range, UINT16 *copy, UINT16 *os, UINT16 *time );
DECLSPEC	INT32	__stdcall	RmcGetArpaCancelTargetNo( INT32 radarNo, INT32 *targetNo );
DECLSPEC	INT32	__stdcall	RmcGetSweepPosition( INT32 radarNo, INT32 *x, INT32 *y );
DECLSPEC	INT32	__stdcall	RmcGetArpaAcquireTargetDisplayId( INT32 radarNo, INT32 *x, INT32 *y, INT32 *targetNo, INT32 *id );
DECLSPEC	INT32	__stdcall	RmcGetArpaCancelTargetDisplayId( INT32 radarNo, INT32 *targetNo );
DECLSPEC	INT32	__stdcall	RmcGetGpsAntennaPosition( INT32 radarNo, INT32 *antennaNo, INT16 *bow, INT16 *port );
DECLSPEC	INT32	__stdcall	RmcGetConningPosition( INT32 radarNo, UINT16 *bow, UINT16 *port );
DECLSPEC	INT32	__stdcall	RmcGetSelectAntenna( INT32 radarNo, UINT16 *no );
DECLSPEC	INT32	__stdcall	RmcGetWakeupCount( INT32 radarNo, UINT16 *count );
DECLSPEC	INT32	__stdcall	RmcGetShipInfo( INT32 radarNo, UINT16 *length, UINT16 *width );
DECLSPEC	INT32	__stdcall	RmcGetAisAlarm( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetArpaAlarm( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetSelectAlarmOut( INT32 radarNo, UINT16 *ch, UINT16 *code, UINT16 *sw );
DECLSPEC	INT32	__stdcall	RmcGetAlarmAck( INT32 radarNo, UINT16 *identifier, UINT16 *instance, UINT8 *command );
DECLSPEC	INT32	__stdcall	RmcGetSelectCrt( INT32 radarNo, UINT16 *crt );
DECLSPEC	INT32	__stdcall	RmcGetSelectAlarmPolarity( INT32 radarNo, INT32 *channelNo, UINT16 *sw );
DECLSPEC	INT32	__stdcall	RmcGetCombineStatus( INT32 radarNo, UINT16 *status, INT16 *antno );
DECLSPEC	INT32	__stdcall	RmcGetHeadingSource( INT32 radarNo, UINT16 *source );
DECLSPEC	INT32	__stdcall	RmcGetNetworkPriority( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetMenuSet( INT32 radarNo, UINT16 *menuNo, UINT16 *data );
DECLSPEC	INT32	__stdcall	RmcGetImoEchoSwitch( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetExtAlarmAck( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetPmAdjustEnd( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetSelectTtmOut( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetArpaMaxRange( INT32 radarNo, UINT16 *maxRange );
DECLSPEC	INT32	__stdcall	RmcGetArpaEchoLevel( INT32 radarNo, UINT16 *level );
DECLSPEC	INT32	__stdcall	RmcGetArpaLandSize( INT32 radarNo, UINT16 *size );
DECLSPEC	INT32	__stdcall	RmcGetArpaAntenna( INT32 radarNo, UINT16 *kind );
DECLSPEC	INT32	__stdcall	RmcGetArpaAcqCorre( INT32 radarNo, UINT16 *scan );
DECLSPEC	INT32	__stdcall	RmcGetArpaWeed( INT32 radarNo, UINT16 *weed );
DECLSPEC	INT32	__stdcall	RmcGetArpaGateSize( INT32 radarNo, UINT16 *size );
DECLSPEC	INT32	__stdcall	RmcGetArpaFilterRespons( INT32 radarNo, UINT16 *res );
DECLSPEC	INT32	__stdcall	RmcGetArpaLostCount( INT32 radarNo, UINT16 *count );
DECLSPEC	INT32	__stdcall	RmcGetArpaMaxSpeed( INT32 radarNo, UINT16 *speed );
DECLSPEC	INT32	__stdcall	RmcGetArpaTimeVector( INT32 radarNo, UINT16 *mode, UINT16 *count );
DECLSPEC	INT32	__stdcall	RmcGetRfcVersionNo( INT32 radarNo, UINT8 *version );
DECLSPEC	INT32	__stdcall	RmcGetCombineData( INT32 radarNo, INT16 *mode, INT16 *start_angle, INT16 *angle, INT16 *start_length, INT16 *length );
DECLSPEC	INT32	__stdcall	RmcGetInterswInfo( INT32 radarNo, UINT16 interswTable[] );
DECLSPEC	INT32	__stdcall	RmcGetArpaLostFilterRange( INT32 radarNo, UINT16 *status, INT32 *rangeNM, INT32 *range );
DECLSPEC	INT32	__stdcall	RmcGetArpaLostFilterSpeed( INT32 radarNo, UINT16 *status, INT32 *speed );
DECLSPEC	INT32	__stdcall	RmcGetStcRange( INT32 radarNo, INT16 *range );
DECLSPEC	INT32	__stdcall	RmcGetCustomMemory( INT32 radarNo, UINT16 *action );
DECLSPEC	INT32	__stdcall	RmcGetBuildUpTime( INT32 radarNo, INT32 *buildUpTime );
DECLSPEC	INT32	__stdcall	RmcGetDisplayUnitInformation( INT32 radarNo, UINT16 *antNo, UINT16 *master, UINT8 *hostname );
DECLSPEC	INT32	__stdcall	RmcGetArpaLostFilterMode( INT32 radarNo, UINT16 *mode );
DECLSPEC	INT32	__stdcall	RmcGetAssociationStatus( INT32 radarNo, INT32 *status );
DECLSPEC	INT32	__stdcall	RmcGetCancelSelectTarget( INT32 radarNo, INT16 *startTargetID, INT16 *endTargetID );
DECLSPEC	INT32	__stdcall	RmcGetCancelSelectLostTarget( INT32 radarNo, INT16 *startTargetID, INT16 *endTargetID );
DECLSPEC	INT32	__stdcall	RmcGetCustomAtfParameter(INT32 radarNo, INT32 *pictureNo, UINT16 *sw, UINT16 *gainData, INT16 *seaData, INT16 *rainData, UINT16 *hGain, UINT16 *mode);
DECLSPEC	INT32	__stdcall	RmcGetTrailProcess( INT32 radarNo, UINT16 *mode );
DECLSPEC	INT32	__stdcall	RmcGetArpaReAcquireTarget( INT32 radarNo, INT32 *targetNo, INT32 *targetId, INT32 *x, INT32 *y, UINT16 *speed, UINT16 *course, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetTxEchoSelect( INT32 radarNo, UINT16 *select );
DECLSPEC	INT32	__stdcall	RmcGetRxEchoSelect( INT32 radarNo, UINT16 *select, UINT8 *ip );
DECLSPEC	INT32	__stdcall	RmcGetRsPreset( INT32 radarNo, INT32 *dataNo, UINT16 *data );
DECLSPEC	INT32	__stdcall	RmcGetJummingAble( INT32 radarNo, UINT16 *mode );
DECLSPEC	INT32	__stdcall	RmcGetIceClass( INT32 radarNo, UINT16 *mode, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetAtfSettings( INT32 radarNo, UINT16 *adjust, UINT16 *highGain, UINT16 *cutLevel, UINT16 *startDir, UINT16 *sector );
DECLSPEC	INT32	__stdcall	RmcGetSsdTxChannel( INT32 radarNo, UINT16 *channel );
DECLSPEC	INT32	__stdcall	RmcGetHighSensitivityMode( INT32 radarNo, UINT16 *mode );
DECLSPEC	INT32	__stdcall	RmcGetBearingResolutionSelect( INT32 radarNo, UINT16 *mode );
DECLSPEC	INT32	__stdcall	RmcGetEchoColorType( INT32 radarNo, UINT16 *mode, UINT16 *sw, UINT16 *autoPicture );
DECLSPEC	INT32	__stdcall	RmcGetAccuShip( INT32 radarNo, UINT16 *sw );
DECLSPEC	INT32	__stdcall	RmcGetObservationMode( INT32 radarNo, UINT16 *mode );
DECLSPEC	INT32	__stdcall	RmcGetSARTSignal( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetImageCor(INT32 radarNo, UINT16 *dummy);
DECLSPEC	INT32	__stdcall	RmcGetNN3Command( INT32 radarNo, INT32 *sub, UINT32 *param1, UINT16 *param2, UINT16 *param3, UINT16 *param4 );
DECLSPEC	INT32	__stdcall	RmcGetMemory( INT32 radarNo, UINT16 *status, UINT16 *memory );
DECLSPEC	INT32	__stdcall	RmcGetEquipComp( INT32 radarNo, UINT16 *status );
DECLSPEC	INT32	__stdcall	RmcGetPulseRange( INT32 radarNo, UINT16 *min, UINT16 *max );
DECLSPEC	INT32	__stdcall	RmcGetMagnetron( INT32 radarNo, UINT16 *magne, UINT16 *status, UINT16 *result );
DECLSPEC	INT32	__stdcall	RmcGetMagnetronInd( INT32 radarNo, UINT16 *ind );
DECLSPEC	INT32	__stdcall	RmcGetArpaDetect( INT32 radarNo, UINT16 *range, UINT16 *level, UINT16 *size );
DECLSPEC	INT32	__stdcall	RmcGetArpaCatch( INT32 radarNo, UINT16 *landsize, UINT16 *correl, UINT16 *cancel, UINT16 *gate );
DECLSPEC	INT32	__stdcall	RmcGetArpaPursuit( INT32 radarNo, UINT16 *gate, UINT16 *filter, UINT16 *lost, UINT16 *speed, UINT16 *mode, UINT16 *unit, UINT16 *data );
DECLSPEC	INT32	__stdcall	RmcGetRangeSelect( INT32 radarNo, INT32 *unit, UINT16 range[] );
DECLSPEC	INT32	__stdcall	RmcGetVideoLevel( INT32 radarNo, UINT16 *status, UINT16 *data );

// RADAR REQ
DECLSPEC	INT32	__stdcall	RmcReqFreeCommand( INT32 radarNo, UINT8 *id );
DECLSPEC	INT32	__stdcall	RmcReqConnect( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqDispMode( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqRange( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqGAIN( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqCustomGAIN( INT32 radarNo, INT32 pictureNo );
DECLSPEC	INT32	__stdcall	RmcReqACSEA( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqCustomACSEA( INT32 radarNo, INT32 pictureNo );
DECLSPEC	INT32	__stdcall	RmcReqACRAIN( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqCustomACRAIN( INT32 radarNo, INT32 pictureNo );
DECLSPEC	INT32	__stdcall	RmcReqCustomPictureAll( INT32 radarNo, INT32 pictureNo );
DECLSPEC	INT32	__stdcall	RmcReqCustomPicture( INT32 radarNo, INT32 pictureNo, INT32 funcNo );
DECLSPEC	INT32	__stdcall	RmcReqPulseWidth( INT32 radarNo, UINT16 range, UINT16 unit, INT32 pictureNo );
DECLSPEC	INT32	__stdcall	RmcReqTxStby( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaDispMode( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAntennaType( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqGuardStatus( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaVector( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaCpaTcpa( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqTune( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqTuneIndicator( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqBlindSector( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqRadarPosition( INT32 radarNo, INT32 monitorNo );
DECLSPEC	INT32	__stdcall	RmcReqTuneAdjust( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqReferencePoint( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAlarmStatus( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqHeading( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSelectQVDisplay( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqATT( INT32 radarNo, INT32 monitorNo );
DECLSPEC	INT32	__stdcall	RmcReqHeadingAdjust( INT32 radarNo, INT32 monitorNo );
DECLSPEC	INT32	__stdcall	RmcReqTimingAdjust( INT32 radarNo, INT32 monitorNo );
DECLSPEC	INT32	__stdcall	RmcReqMBSAdjust( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAntennaHeight( INT32 radarNo, INT32 monitorNo );
DECLSPEC	INT32	__stdcall	RmcReqNearSTC( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqMiddleSTC( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqFarSTC( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqRingSuppression( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAntennaRevolution( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAntennaSwitch( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqStoppedTx( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqMonitorSelect( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAntennaNo( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqOnTime( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqTxTime( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqPmAdjust( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqDemoEcho( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqEavWithoutGyro( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReq2ndEchoReject( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqPM( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSART( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqVersionNo( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaTest( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqGuardMode( INT32 radarNo, INT32 zoneNo );
DECLSPEC	INT32	__stdcall	RmcReqGuardFan( INT32 radarNo, INT32 zoneNo );
DECLSPEC	INT32	__stdcall	RmcReqGuardPolygon( INT32 radarNo, INT32 zoneNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaWithoutGyro( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSpeedSelect( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqDrift( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqGC10( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaCancelStatus( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaPreset( INT32 radarNo, INT32 dataNo );
DECLSPEC	INT32	__stdcall	RmcReqNextEcho(INT32 radarNo);
DECLSPEC	INT32	__stdcall	RmcReqTrailMode( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSweepPosition( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqGpsAntennaPosition( INT32 radarNo, INT32 antennaNo );
DECLSPEC	INT32	__stdcall	RmcReqConningPosition( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSelectAntenna( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqWakeupCount( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqShipInfo( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAisAlarm( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaAlarm( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSelectAlarmOut( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAlarmAck( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSelectCrt( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSelectAlarmPolarity( INT32 radarNo, INT32 channel );
DECLSPEC	INT32	__stdcall	RmcReqCombineStatus( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqHeadingSource( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqNetworkPriority( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqMenuSet( INT32 radarNo, UINT16 menu_no );
DECLSPEC	INT32	__stdcall	RmcReqImoEchoSwitch( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqPmAdjustEnd( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSelectTtmOut( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaMaxRange( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaEchoLevel( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaLandSize( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaAntenna( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaAcqCorre( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaWeed( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaGateSize( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaFilterRespons( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaLostCount( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaMaxSpeed( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaTimeVector( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqRfcVersionNo( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqCombineData( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqInterswInfo( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaLostFilterRange( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaLostFilterSpeed( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqStcRange( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqBuildUpTime( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqDisplayUnitInformation( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaLostFilterMode( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAssociationStatus( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqCancelSelectTarget( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqCancelSelectLostTarget( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqCustomAtfParameter(INT32 radarNo, INT32 pictureNo);
DECLSPEC	INT32	__stdcall	RmcReqTrailProcess( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaReAcquireTarget( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqTxEchoSelect( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqRxEchoSelect( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqRsPreset( INT32 radarNo, INT32 dataNo);
DECLSPEC	INT32	__stdcall	RmcReqJummingAble( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqIceClass( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAtfSettings( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSsdTxChannel( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqHighSensitivityMode( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqBearingResolutionSelect( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqEchoColorType( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqAccuShip( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqObservationMode( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqSARTSignal( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqImageCor(INT32 radarNo);
DECLSPEC	INT32	__stdcall	RmcReqNN3Command( INT32 radarNo, INT32 sub );
DECLSPEC	INT32	__stdcall	RmcReqMemory( INT32 radarNo, UINT16 status, UINT16 memory );
DECLSPEC	INT32	__stdcall	RmcReqEquipComp( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqPulseRange( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqMagnetron( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqMagnetronInd( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaDetect( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaCatch( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqArpaPursuit( INT32 radarNo );
DECLSPEC	INT32	__stdcall	RmcReqRangeSelect( INT32 radarNo, INT32 unit );
DECLSPEC	INT32	__stdcall	RmcReqVideoLevel( INT32 radarNo );

/* */
DECLSPEC	INT32	__stdcall	RmSetImoCommandCallbackFunc( void(__stdcall *func)(UINT8* data) );
DECLSPEC	INT32	__stdcall	RmaInitialArpaModule( void );
DECLSPEC	INT32	__stdcall	RmaSetArpaCallbackFunc( void(__stdcall *func)( INT32 radarNo, INT32 targetNo, INT64 timestamp) );
DECLSPEC	INT32	__stdcall	RmaGetTargetCpaTcpa( INT32 radarNo, UINT16 id, INT16 *cpa_tcpa );
DECLSPEC	INT32	__stdcall	RmaGetTargetCpaTcpaAll( INT32 radarNo, INT16 *cpa_tcpa );
DECLSPEC	INT32	__stdcall	RmaGetTargetProximity( INT32 radarNo, UINT16 id, INT16 *prox );
DECLSPEC	INT32	__stdcall	RmaGetArpaInfo( INT32 radarNo, INT32 targetNo, ARPA *arpaInfo );
DECLSPEC	INT32	__stdcall	RmaGetNextArpaInfo( INT32 radarNo, INT32 *targetNo, ARPA *arpaInfo, INT32 *receiveTick );
DECLSPEC	INT32	__stdcall	RmaDeleteArpaInfo( INT32 radarNo, INT32 targetNo );
DECLSPEC	INT32	__stdcall	RmaGetTargetInfo( INT32 radarNo, INT32 tno, ARPA_TARGET *targetInfo );
DECLSPEC	INT32	__stdcall	RmaGetNextTargetInfo( INT32 radarNo, INT32 *targetNo, ARPA_TARGET *targetInfo, INT32 *receiveTick );
DECLSPEC	INT32	__stdcall	RmaGetJudgeInfo( INT32 radarNo, INT32 tno, ARPA_JUDGE *judgeInfo );
DECLSPEC	INT32	__stdcall	RmSetRadarRxCommandType( INT32 type );
DECLSPEC	INT32	__stdcall	RmGetRadarRxCommandType( INT32 *type );
DECLSPEC	INT32	__stdcall	RmSetRadarTxCommandType( INT32 type );
DECLSPEC	INT32	__stdcall	RmGetRadarTxCommandType( INT32 *type );
DECLSPEC	INT32	__stdcall	RmSetRadarCommandType( INT32 type );
DECLSPEC	INT32	__stdcall	RmGetRadarCommandType( INT32 *type );

#ifdef __cplusplus
}
#endif

#endif
