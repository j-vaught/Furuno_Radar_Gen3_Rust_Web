# NAVNETDLL Symbol Classification

Generated: 2026-03-06

## Binary Overview

| Property | Value |
|----------|-------|
| File | `furuno/lib/libNAVNETDLL.so` |
| Format | ELF 64-bit LSB shared object, ARM aarch64 |
| Version | 01.95.0000 |
| Stripped | No (all symbols intact) |
| Size | 1,329,608 bytes |
| Build ID | `59c00597cbd95a517d29e11fb2cbf732d6722812` |
| Defined functions | 6,102 |
| Imported functions | 164 (all glibc) |

## Module Breakdown

| Module | Prefix | Count | Relevant | Description |
|--------|--------|-------|----------|-------------|
| Radar Cmd Set | `RmcSet*` | 154 | YES | Send control commands to radar |
| Radar Cmd Get | `RmcGet*` | 143 | YES | Read radar state |
| Radar Cmd Req | `RmcReq*` | 133 | YES | Request specific data from radar |
| Radar Module | `Rm*` | 67 | YES | Echo pipeline, callbacks, guard zones |
| Radar Internal | `rmSub*/rmMake*/rmConv*` | 934 | IMPL | Internal command serialization/parsing |
| ARPA | `Rma*` | 13 | YES | Automatic Radar Plotting Aid |
| NAVNET Common | `Nc*` | 310 | YES | Network stack, servers, NMEA, TCP |
| Echo Sounder Cmd | `Esc*` | 724 | NO | Echo sounder commands (not radar) |
| Echo Sounder Mod | `Es*` | 158 | NO | Echo sounder module (not radar) |
| FNET Network | `nn*` | 188 | IMPL | Internal FNET protocol |
| Component | `cpnt*` | 96 | IMPL | Internal component management |
| ITRON RTOS | `acre_*/c_*` | 62 | IMPL | Userspace ITRON shim |
| Initialization | `Initial*` | 52 | IMPL | Module initialization |
| FNET Service | `Fnet*` | 56 | IMPL | FNET service layer |
| Socket Internal | `tcp*/udp*` | 70 | IMPL | Socket operations |
| FLM Licensing | `Flm*` | 46 | MAYBE | License management |
| FVOS | `Fvos*` | 44 | IMPL | Possibly virtual OS layer |
| FEC Core | `Fec*` | 24 | IMPL | Core FEC framework |
| Crypto | `Blowfish*/bf*` | 14 | IMPL | Blowfish encryption |
| SHA | `sha*/SHA*` | 8 | IMPL | SHA hashing |
| Error API | `Le*` | 6 | YES | Last error management |
| Change/Convert | `chg*/change*` | 76 | IMPL | Unit conversions |
| Guard Zone | `Gz*` | 36 | IMPL | Guard zone internals |
| Other | misc | 992 | IMPL | Misc internal functions |

**Legend:** YES = public API to reimplement, IMPL = internal (must be reimplemented as part of the internals), NO = not needed for radar, MAYBE = depends on licensing approach

---

## IRAD Command ID Map

160 command IDs (0-159) map radar functions to wire protocol bytes. Each ID corresponds to a `RmcSet*`, `RmcGet*`, and usually a `RmcReq*` function.

| ID | Define | Set Function | Get Function |
|----|--------|-------------|-------------|
| 0 | `IRAD_CONNECT` | `RmcSetConnect` | `RmcGetConnect` |
| 1 | `IRAD_DISPMODE` | `RmcSetArpaDispMode` | `RmcGetArpaDispMode` |
| 2 | `IRAD_RANGE` | `RmcSetArpaLostFilterRange` | `RmcGetArpaLostFilterRange` |
| 3 | `IRAD_GAIN` | `RmcSetCustomGAIN` | `RmcGetCustomGAIN` |
| 4 | `IRAD_ACSEA` | `RmcSetACSEA` | `RmcGetACSEA` |
| 5 | `IRAD_ACRAIN` | `RmcSetACRAIN` | `RmcGetACRAIN` |
| 6 | `IRAD_CUSTOMFUNCTIONALL` | `—` | `—` |
| 7 | `IRAD_CUSTOMFUNCTION` | `—` | `—` |
| 8 | `IRAD_PULSEWIDTH` | `RmcSetPulseWidth` | `RmcGetPulseWidth` |
| 9 | `IRAD_TXSTBY` | `RmcSetTxStby` | `RmcGetTxStby` |
| 10 | `IRAD_SELECTTARGET` | `RmcSetArpaSelectTarget` | `RmcGetArpaSelectTarget` |
| 11 | `IRAD_ACQTARGET` | `—` | `—` |
| 12 | `IRAD_CANCELTARGET` | `RmcSetArpaCancelTarget` | `RmcGetArpaCancelTarget` |
| 13 | `IRAD_ARPADISPMODE` | `RmcSetArpaDispMode` | `RmcGetArpaDispMode` |
| 14 | `IRAD_ANTENNATYPE` | `RmcSetAntennaType` | `RmcGetAntennaType` |
| 15 | `IRAD_KEYCOMMAND` | `RmcSetKeyCommand` | `RmcGetKeyCommand` |
| 16 | `IRAD_GUARDSTATUS` | `RmcSetGuardStatus` | `RmcGetGuardStatus` |
| 17 | `IRAD_GUARDAREA` | `—` | `—` |
| 18 | `IRAD_LOSTACK` | `RmcSetLostAck` | `—` |
| 19 | `IRAD_ARPAVECTOR` | `RmcSetArpaVector` | `RmcGetArpaVector` |
| 20 | `IRAD_ARPACPATCPA` | `RmcSetArpaCpaTcpa` | `RmcGetArpaCpaTcpa` |
| 21 | `IRAD_TUNE` | `RmcSetTune` | `RmcGetTune` |
| 22 | `IRAD_TUNEINDICATOR` | `RmcSetTuneIndicator` | `RmcGetTuneIndicator` |
| 23 | `IRAD_BLIND` | `RmcSetBlindSector` | `RmcGetBlindSector` |
| 24 | `IRAD_RADARPOS` | `RmcSetRadarPosition` | `RmcGetRadarPosition` |
| 25 | `IRAD_ARPA_TARGET_ALLCLEAR` | `—` | `—` |
| 26 | `IRAD_TUNEADJUST` | `RmcSetTuneAdjust` | `RmcGetTuneAdjust` |
| 27 | `IRAD_CANCELTARGETID` | `RmcSetArpaCancelTargetId` | `RmcGetArpaCancelTargetId` |
| 28 | `IRAD_REFERENCEPOINT` | `RmcSetReferencePoint` | `RmcGetReferencePoint` |
| 29 | `IRAD_ALARM` | `RmcSetAisAlarm` | `RmcGetAisAlarm` |
| 30 | `IRAD_HEADING` | `RmcSetHeading` | `RmcGetHeading` |
| 31 | `IRAD_SELECTQVDISPLAY` | `RmcSetSelectQVDisplay` | `RmcGetSelectQVDisplay` |
| 32 | `IRAD_ATT` | `RmcSetATT` | `RmcGetATT` |
| 33 | `IRAD_HDADJ` | `—` | `—` |
| 34 | `IRAD_TIMINGADJ` | `RmcSetTimingAdjust` | `RmcGetTimingAdjust` |
| 35 | `IRAD_MBSADJ` | `RmcSetMBSAdjust` | `RmcGetMBSAdjust` |
| 36 | `IRAD_ANTHEIGHT` | `—` | `—` |
| 37 | `IRAD_NEARSTC` | `RmcSetNearSTC` | `RmcGetNearSTC` |
| 38 | `IRAD_MIDSTC` | `—` | `—` |
| 39 | `IRAD_FARSTC` | `RmcSetFarSTC` | `RmcGetFarSTC` |
| 40 | `IRAD_RINGSUP` | `RmcSetRingSuppression` | `RmcGetRingSuppression` |
| 41 | `IRAD_ANTREVOLUTION` | `—` | `—` |
| 42 | `IRAD_ANTSW` | `—` | `—` |
| 43 | `IRAD_STOPEDTX` | `—` | `—` |
| 44 | `IRAD_MONITORSELECT` | `RmcSetMonitorSelect` | `RmcGetMonitorSelect` |
| 45 | `IRAD_ANTNO` | `—` | `—` |
| 46 | `IRAD_ONTIME` | `RmcSetOnTime` | `RmcGetOnTime` |
| 47 | `IRAD_TXTIME` | `RmcSetTxTime` | `RmcGetTxTime` |
| 48 | `IRAD_PMADJ` | `RmcSetPmAdjust` | `RmcGetPmAdjust` |
| 49 | `IRAD_DEMOECHO` | `RmcSetDemoEcho` | `RmcGetDemoEcho` |
| 50 | `IRAD_EAVGYRO` | `—` | `—` |
| 51 | `IRAD_2NDECHOREJ` | `RmcSet2ndEchoReject` | `RmcGet2ndEchoReject` |
| 52 | `IRAD_IMOPM` | `—` | `—` |
| 53 | `IRAD_SART` | `RmcSetSART` | `RmcGetSART` |
| 54 | `IRAD_VERSIONNO` | `RmcSetRfcVersionNo` | `RmcGetRfcVersionNo` |
| 55 | `IRAD_STARTARPATEST` | `—` | `—` |
| 56 | `IRAD_GUARD_MODE` | `—` | `—` |
| 57 | `IRAD_GUARD_FAN1` | `—` | `—` |
| 58 | `IRAD_GUARD_FAN2` | `—` | `—` |
| 59 | `IRAD_POLYGON` | `RmcSetGuardPolygon` | `RmcGetGuardPolygon` |
| 60 | `IRAD_ARPAGYRO` | `—` | `—` |
| 61 | `IRAD_SPEED` | `RmcSetArpaLostFilterSpeed` | `RmcGetArpaLostFilterSpeed` |
| 62 | `IRAD_SETDRIFT` | `RmcSetDrift` | `—` |
| 63 | `IRAD_GC10` | `RmcSetGC10` | `RmcGetGC10` |
| 64 | `IRAD_CANCELSTATUS` | `RmcSetArpaCancelStatus` | `RmcGetArpaCancelStatus` |
| 65 | `IRAD_ARPAPRESET` | `RmcSetArpaPreset` | `RmcGetArpaPreset` |
| 66 | `IRAD_NEXTECHO` | `RmcSetNextEcho` | `RmcGetNextEcho` |
| 67 | `IRAD_TRAILMODE` | `RmcSetTrailMode` | `RmcGetTrailMode` |
| 68 | `IRAD_TRAILCLEAR` | `RmcSetTrailClear` | `—` |
| 69 | `IRAD_CANCELTARGETNO` | `RmcSetArpaCancelTargetNo` | `RmcGetArpaCancelTargetNo` |
| 70 | `IRAD_SWEEPPOS` | `RmcSetSweepPosition` | `RmcGetSweepPosition` |
| 71 | `IRAD_ACQTARGETNOIBS` | `—` | `—` |
| 72 | `IRAD_CANCELTARGETNOIBS` | `—` | `—` |
| 73 | `IRAD_GPSANTPOS` | `—` | `—` |
| 74 | `IRAD_CONNINGPOS` | `RmcSetConningPosition` | `RmcGetConningPosition` |
| 75 | `IRAD_SELECTANTENNA` | `RmcSetSelectAntenna` | `RmcGetSelectAntenna` |
| 76 | `IRAD_WAKEUPCOUNT` | `RmcSetWakeupCount` | `RmcGetWakeupCount` |
| 77 | `IRAD_SHIPINFO` | `RmcSetShipInfo` | `RmcGetShipInfo` |
| 78 | `IRAD_AISALARM` | `RmcSetAisAlarm` | `RmcGetAisAlarm` |
| 79 | `IRAD_ARPAALARM` | `RmcSetArpaAlarm` | `RmcGetArpaAlarm` |
| 80 | `IRAD_SELECTALARMOUT` | `RmcSetSelectAlarmOut` | `RmcGetSelectAlarmOut` |
| 81 | `IRAD_ALARMACK` | `RmcSetAlarmAck` | `RmcGetAlarmAck` |
| 82 | `IRAD_SELECTCRT` | `RmcSetSelectCrt` | `RmcGetSelectCrt` |
| 83 | `IRAD_SELECTALARMPOLARITY` | `RmcSetSelectAlarmPolarity` | `RmcGetSelectAlarmPolarity` |
| 84 | `IRAD_COMBINESTATUS` | `RmcSetCombineStatus` | `RmcGetCombineStatus` |
| 85 | `IRAD_HEADINGSOURCE` | `RmcSetHeadingSource` | `RmcGetHeadingSource` |
| 86 | `IRAD_NETWORKPRIO` | `RmcSetNetworkPriority` | `RmcGetNetworkPriority` |
| 87 | `IRAD_MENUSET` | `RmcSetMenuSet` | `RmcGetMenuSet` |
| 88 | `IRAD_IMOECHOSW` | `RmcSetImoEchoSwitch` | `RmcGetImoEchoSwitch` |
| 89 | `IRAD_EXTALARMCK` | `—` | `—` |
| 90 | `IRAD_PMADJEND` | `—` | `—` |
| 91 | `IRAD_SELECTTTMOUT` | `RmcSetSelectTtmOut` | `RmcGetSelectTtmOut` |
| 92 | `IRAD_ARPA_MAX_RANGE` | `—` | `—` |
| 93 | `IRAD_ECHOLEVEL` | `RmcSetArpaEchoLevel` | `RmcGetArpaEchoLevel` |
| 94 | `IRAD_ARPA_ALLCLEAR` | `—` | `—` |
| 95 | `IRAD_LAND_SIZE` | `—` | `—` |
| 96 | `IRAD_ARPA_ANT` | `—` | `—` |
| 97 | `IRAD_ARPA_ACQ_CORRE` | `—` | `—` |
| 98 | `IRAD_ARPA_WEED` | `—` | `—` |
| 99 | `IRAD_ARPA_GATE_SIZE` | `—` | `—` |
| 100 | `IRAD_ARPA_FILTER_RESPONS` | `—` | `—` |
| 101 | `IRAD_ARPA_LOST_COUNT` | `—` | `—` |
| 102 | `IRAD_ARPA_MAX_SPEED` | `—` | `—` |
| 103 | `IRAD_ARPA_TIME_VECT` | `—` | `—` |
| 104 | `IRAD_OPERATOR_FITNESS` | `—` | `—` |
| 105 | `IRAD_PICTURE_DEFAULT` | `—` | `—` |
| 106 | `IRAD_RFC_VERSION_NO` | `—` | `—` |
| 107 | `IRAD_COMBINE_DATA` | `—` | `—` |
| 111 | `IRAD_INTERSW_INFO` | `—` | `—` |
| 112 | `IRAD_ARPA_LOSTFILTER_RANGE` | `—` | `—` |
| 113 | `IRAD_ARPA_LOSTFILTER_SPEED` | `—` | `—` |
| 114 | `IRAD_STC_RANGE` | `—` | `—` |
| 115 | `IRAD_CUSTOM_ACTION` | `—` | `—` |
| 116 | `IRAD_BUILD_UP_TIME` | `—` | `—` |
| 117 | `IRAD_DISPUNIT_INFO` | `—` | `—` |
| 119 | `IRAD_ARPA_LOSTFILTER_MODE` | `—` | `—` |
| 121 | `IRAD_ASSOCIATION_STATUS` | `—` | `—` |
| 120 | `IRAD_ECHO_CLEAR` | `—` | `—` |
| 122 | `IRAD_CANCELSELECTTARGET` | `RmcSetCancelSelectTarget` | `RmcGetCancelSelectTarget` |
| 123 | `IRAD_CANCELSELECTLOSTTARGET` | `RmcSetCancelSelectLostTarget` | `RmcGetCancelSelectLostTarget` |
| 128 | `IRAD_ATFPARM` | `—` | `—` |
| 129 | `IRAD_TRAILPROCESS` | `RmcSetTrailProcess` | `RmcGetTrailProcess` |
| 130 | `IRAD_REACQTARGET` | `—` | `—` |
| 131 | `IRAD_ALIVECHECK` | `RmcSetAliveCheck` | `—` |
| 132 | `IRAD_TXECHOSEL` | `RmcSetTxEchoSelect` | `RmcGetTxEchoSelect` |
| 133 | `IRAD_RXECHOSEL` | `RmcSetRxEchoSelect` | `RmcGetRxEchoSelect` |
| 134 | `IRAD_RSPRESET` | `RmcSetRsPreset` | `RmcGetRsPreset` |
| 135 | `IRAD_STARTSELFMODE` | `RmcSetStartSelfMode` | `—` |
| 136 | `IRAD_JUMMINGABLE` | `RmcSetJummingAble` | `RmcGetJummingAble` |
| 137 | `IRAD_ICECLASS` | `RmcSetIceClass` | `RmcGetIceClass` |
| 138 | `IRAD_ATFSETTING` | `RmcSetAtfSettings` | `RmcGetAtfSettings` |
| 139 | `IRAD_RSALLCLEAR` | `RmcSetRsAllClear` | `—` |
| 140 | `IRAD_SSDTXCHANNEL` | `RmcSetSsdTxChannel` | `RmcGetSsdTxChannel` |
| 141 | `IRAD_HIGHSENSITIVITYMODE` | `RmcSetHighSensitivityMode` | `RmcGetHighSensitivityMode` |
| 142 | `IRAD_BEARINGRESOLUTIONSELECT` | `RmcSetBearingResolutionSelect` | `RmcGetBearingResolutionSelect` |
| 143 | `IRAD_ECHOCOLORTYPE` | `RmcSetEchoColorType` | `RmcGetEchoColorType` |
| 144 | `IRAD_ACCUSHIP` | `RmcSetAccuShip` | `RmcGetAccuShip` |
| 145 | `IRAD_OBSERVATION_MODE` | `—` | `—` |
| 146 | `IRAD_SART_SIGNAL` | `—` | `—` |
| 148 | `IRAD_IMAGECOR` | `RmcSetImageCor` | `RmcGetImageCor` |
| 149 | `IRAD_NN3COMMAND` | `RmcSetNN3Command` | `RmcGetNN3Command` |
| 150 | `IRAD_SETMEMORY` | `RmcSetMemory` | `—` |
| 151 | `IRAD_EQUIPCOMP` | `RmcSetEquipComp` | `RmcGetEquipComp` |
| 152 | `IRAD_PULSERANGE` | `RmcSetPulseRange` | `RmcGetPulseRange` |
| 153 | `IRAD_MAGNETRON` | `RmcSetMagnetron` | `RmcGetMagnetron` |
| 154 | `IRAD_MAGNETRONIND` | `RmcSetMagnetronInd` | `RmcGetMagnetronInd` |
| 155 | `IRAD_ARPDETECT` | `—` | `—` |
| 156 | `IRAD_ARPCATCH` | `—` | `—` |
| 157 | `IRAD_ARPPURSUIT` | `—` | `—` |
| 158 | `IRAD_RANGESELECT` | `RmcSetRangeSelect` | `RmcGetRangeSelect` |
| 159 | `IRAD_VIDEOLEVEL` | `RmcSetVideoLevel` | `RmcGetVideoLevel` |


---

## Radar Module Core (Rm*) — 67 functions

| Function | Address |
|----------|---------|
| `RmChangeHost` | `0x0000000000074600` |
| `RmEndImoEcho` | `0x0000000000072640` |
| `RmEndModule` | `0x0000000000079580` |
| `RmGetAliveFlag` | `0x0000000000079690` |
| `RmGetCommandSetting` | `0x0000000000079470` |
| `RmGetConnection` | `0x0000000000079608` |
| `RmGetConnectionEx` | `0x0000000000079650` |
| `RmGetGuardZoneStatus` | `0x0000000000071e10` |
| `RmGetGzDetectLevel` | `0x0000000000071a68` |
| `RmGetGzDetectResult` | `0x0000000000071c20` |
| `RmGetGzDetectSize` | `0x0000000000071a98` |
| `RmGetGzJudgeCount` | `0x0000000000071af8` |
| `RmGetGzJudgeOutOfRange` | `0x0000000000071d18` |
| `RmGetGzJudgeResult` | `0x0000000000071b28` |
| `RmGetGzJudgeType` | `0x0000000000071ac8` |
| `RmGetGzTargetZone` | `0x0000000000071a00` |
| `RmGetHeadingDataDegree` | `0x00000000000728f0` |
| `RmGetHeadingStatus` | `0x0000000000072920` |
| `RmGetHost` | `0x00000000000746f8` |
| `RmGetModuleVersionNo` | `0x00000000000793d0` |
| `RmGetRadarCommandType` | `0x00000000000be518` |
| `RmGetRadarRxCommandType` | `0x00000000000be4f8` |
| `RmGetRadarSource` | `0x00000000000744e0` |
| `RmGetRadarTxCommandType` | `0x00000000000be508` |
| `RmGetSensorHostCount` | `0x0000000000076e50` |
| `RmGetSensorHostName` | `0x0000000000076f18` |
| `RmGetServerClientMode` | `0x00000000000791f8` |
| `RmGetSweeps` | `0x00000000000726e0` |
| `RmInitializeImoEcho` | `0x0000000000072500` |
| `RmInitialModule` | `0x00000000000792a0` |
| `RmResetGuardZoneStatus` | `0x0000000000071e58` |
| `RmResetGzResult` | `0x00000000000714c0` |
| `RmSetCommandCallbackFunc` | `0x0000000000079368` |
| `RmSetCommandOffline` | `0x0000000000079768` |
| `RmSetCommandSetting` | `0x0000000000079420` |
| `RmSetCommandStringCallbackFunc` | `0x0000000000079398` |
| `RmSetCycleCallbackFunc` | `0x00000000000794f0` |
| `RmSetCycleCallbackTick` | `0x00000000000794d8` |
| `RmSetEchoCallbackFunc` | `0x0000000000073048` |
| `RmSetEchoInterpolationFunc` | `0x0000000000073b70` |
| `RmSetEchoSweepTotal` | `0x0000000000073c00` |
| `RmSetFreeCommandCallbackFunc` | `0x0000000000079520` |
| `RmSetGetFreeCommandDataCallbackFunc` | `0x0000000000079550` |
| `RmSetGzAlarmCallbackFunc` | `0x0000000000070fa8` |
| `RmSetGzDetectLevel` | `0x00000000000715f0` |
| `RmSetGzDetectSize` | `0x00000000000716c0` |
| `RmSetGzJudgeCount` | `0x0000000000071930` |
| `RmSetGzJudgeType` | `0x0000000000071860` |
| `RmSetGzSizeTbl` | `0x0000000000071790` |
| `RmSetGzTargetZone` | `0x0000000000071500` |
| `RmSetHeadingData` | `0x00000000000728c0` |
| `RmSetHeadingDataDegree` | `0x00000000000728d0` |
| `RmSetHeadingStatus` | `0x0000000000072910` |
| `RmSetImoCommandCallbackFunc` | `0x0000000000078328` |
| `RmSetInterpolationSweepNum` | `0x0000000000073bb8` |
| `RmSetLowLevelEcho` | `0x0000000000073c80` |
| `RmSetRadarCommandType` | `0x00000000000be510` |
| `RmSetRadarEchoProcessMode` | `0x00000000000726c8` |
| `RmSetRadarEchoTestEnable_ImVeryBadForPerfs` | `0x00000000000726b0` |
| `RmSetRadarRxCommandType` | `0x00000000000be4f0` |
| `RmSetRadarSource` | `0x0000000000074068` |
| `RmSetRadarSourceOffline` | `0x0000000000077208` |
| `RmSetRadarTxCommandType` | `0x00000000000be500` |
| `RmSetServerClientMode` | `0x00000000000791b8` |
| `RmSetTargetCpaTcpa` | `0x0000000000079798` |
| `RmSetTargetProximity` | `0x00000000000797e8` |
| `RmSetTileEchoCallbackFunc` | `0x00000000000775e8` |

---

## ARPA Module (Rma*) — 13 functions

| Function | Address |
|----------|---------|
| `RmaDeleteArpaInfo` | `0x000000000007a180` |
| `RmaGetAnalyseInfo` | `0x000000000007a860` |
| `RmaGetArpaInfo` | `0x0000000000079b78` |
| `RmaGetJudgeInfo` | `0x000000000007a6c0` |
| `RmaGetNextArpaInfo` | `0x0000000000079d78` |
| `RmaGetNextArpaInfoAll` | `0x0000000000079fb0` |
| `RmaGetNextTargetInfo` | `0x000000000007a4b8` |
| `RmaGetTargetCpaTcpa` | `0x0000000000079830` |
| `RmaGetTargetCpaTcpaAll` | `0x0000000000079948` |
| `RmaGetTargetInfo` | `0x000000000007a2f0` |
| `RmaGetTargetProximity` | `0x0000000000079a78` |
| `RmaInitialArpaModule` | `0x0000000000078340` |
| `RmaSetArpaCallbackFunc` | `0x0000000000078400` |

---

## Radar Command Set (RmcSet*) — 154 functions

| Function | Address |
|----------|---------|
| `RmcSet2ndEchoReject` | `0x00000000000adce8` |
| `RmcSetAccuShip` | `0x00000000000b0cd8` |
| `RmcSetACRAIN` | `0x00000000000ac0c8` |
| `RmcSetACSEA` | `0x00000000000abf20` |
| `RmcSetAisAlarm` | `0x00000000000aed48` |
| `RmcSetAlarmAck` | `0x00000000000aeef8` |
| `RmcSetAlarmStatus` | `0x00000000000ad010` |
| `RmcSetAliveCheck` | `0x00000000000b05f0` |
| `RmcSetAntennaHeight` | `0x00000000000ad4a0` |
| `RmcSetAntennaNo` | `0x00000000000ad9a8` |
| `RmcSetAntennaRevolution` | `0x00000000000ad778` |
| `RmcSetAntennaSwitch` | `0x00000000000ad810` |
| `RmcSetAntennaType` | `0x00000000000ac7c0` |
| `RmcSetArpaAcqCorre` | `0x00000000000af798` |
| `RmcSetArpaAcquireTarget` | `0x00000000000ac5f0` |
| `RmcSetArpaAcquireTargetDisplayId` | `0x00000000000ae938` |
| `RmcSetArpaAlarm` | `0x00000000000aedd0` |
| `RmcSetArpaAllClear` | `0x00000000000af630` |
| `RmcSetArpaAntenna` | `0x00000000000af710` |
| `RmcSetArpaCancelStatus` | `0x00000000000ae558` |
| `RmcSetArpaCancelTarget` | `0x00000000000ac680` |
| `RmcSetArpaCancelTargetDisplayId` | `0x00000000000ae9c8` |
| `RmcSetArpaCancelTargetId` | `0x00000000000aced0` |
| `RmcSetArpaCancelTargetNo` | `0x00000000000ae828` |
| `RmcSetArpaCatch` | `0x00000000000b1338` |
| `RmcSetArpaCpaTcpa` | `0x00000000000acab0` |
| `RmcSetArpaDetect` | `0x00000000000b1298` |
| `RmcSetArpaDispMode` | `0x00000000000ac708` |
| `RmcSetArpaEchoLevel` | `0x00000000000af5a8` |
| `RmcSetArpaFilterRespons` | `0x00000000000af930` |
| `RmcSetArpaGateSize` | `0x00000000000af8a8` |
| `RmcSetArpaLandSize` | `0x00000000000af688` |
| `RmcSetArpaLostCount` | `0x00000000000af9b8` |
| `RmcSetArpaLostFilterMode` | `0x00000000000b0160` |
| `RmcSetArpaLostFilterRange` | `0x00000000000afdf0` |
| `RmcSetArpaLostFilterSpeed` | `0x00000000000afe80` |
| `RmcSetArpaMaxRange` | `0x00000000000af520` |
| `RmcSetArpaMaxSpeed` | `0x00000000000afa40` |
| `RmcSetArpaPreset` | `0x00000000000ae5e0` |
| `RmcSetArpaPursuit` | `0x00000000000b13e0` |
| `RmcSetArpaReAcquireTarget` | `0x00000000000b0548` |
| `RmcSetArpaSelectTarget` | `0x00000000000ac560` |
| `RmcSetArpaTargetAllClear` | `0x00000000000acdf0` |
| `RmcSetArpaTest` | `0x00000000000adfa0` |
| `RmcSetArpaTimeVector` | `0x00000000000afac8` |
| `RmcSetArpaVector` | `0x00000000000aca18` |
| `RmcSetArpaWeed` | `0x00000000000af820` |
| `RmcSetArpaWithoutGyro` | `0x00000000000ae310` |
| `RmcSetAssociationStatus` | `0x00000000000b01e8` |
| `RmcSetAtfSettings` | `0x00000000000b0998` |
| `RmcSetATT` | `0x00000000000ad1c8` |
| `RmcSetBearingResolutionSelect` | `0x00000000000b0bb0` |
| `RmcSetBlindSector` | `0x00000000000acc88` |
| `RmcSetBuildUpTime` | `0x00000000000b0020` |
| `RmcSetCancelSelectLostTarget` | `0x00000000000b0360` |
| `RmcSetCancelSelectTarget` | `0x00000000000b02c8` |
| `RmcSetCombineData` | `0x00000000000afcb0` |
| `RmcSetCombineStatus` | `0x00000000000af0c0` |
| `RmcSetConnect` | `0x00000000000abb30` |
| `RmcSetConningPosition` | `0x00000000000aeb08` |
| `RmcSetCustomACRAIN` | `0x00000000000ac1b8` |
| `RmcSetCustomACSEA` | `0x00000000000ac010` |
| `RmcSetCustomAtfParameter` | `0x00000000000b03f8` |
| `RmcSetCustomGAIN` | `0x00000000000abe70` |
| `RmcSetCustomMemory` | `0x00000000000aff98` |
| `RmcSetCustomPicture` | `0x00000000000ac348` |
| `RmcSetCustomPictureAll` | `0x00000000000ac278` |
| `RmcSetDemoEcho` | `0x00000000000adbd8` |
| `RmcSetDisplayUnitInformation` | `0x00000000000b00a8` |
| `RmcSetDispMode` | `0x00000000000abbb8` |
| `RmcSetDrift` | `0x00000000000ae430` |
| `RmcSetEavWithoutGyro` | `0x00000000000adc60` |
| `RmcSetEchoClear` | `0x00000000000b0270` |
| `RmcSetEchoColorType` | `0x00000000000b0c38` |
| `RmcSetEquipComp` | `0x00000000000b1050` |
| `RmcSetExtAlarmAck` | `0x00000000000af388` |
| `RmcSetFarSTC` | `0x00000000000ad668` |
| `RmcSetFreeCommand` | `0x00000000000abac0` |
| `RmcSetGAIN` | `0x00000000000abd88` |
| `RmcSetGC10` | `0x00000000000ae4d0` |
| `RmcSetGpsAntennaPosition` | `0x00000000000aea50` |
| `RmcSetGuardFan` | `0x00000000000ae0e0` |
| `RmcSetGuardMode` | `0x00000000000ae028` |
| `RmcSetGuardPolygon` | `0x00000000000ae1a0` |
| `RmcSetGuardStatus` | `0x00000000000ac920` |
| `RmcSetHeading` | `0x00000000000ad0a8` |
| `RmcSetHeadingAdjust` | `0x00000000000ad280` |
| `RmcSetHeadingSource` | `0x00000000000af158` |
| `RmcSetHighSensitivityMode` | `0x00000000000b0b28` |
| `RmcSetIceClass` | `0x00000000000b0900` |
| `RmcSetImageCor` | `0x00000000000b0e70` |
| `RmcSetImoEchoSwitch` | `0x00000000000af300` |
| `RmcSetInterswInfo` | `0x00000000000afd60` |
| `RmcSetJummingAble` | `0x00000000000b0878` |
| `RmcSetKeyCommand` | `0x00000000000ac880` |
| `RmcSetLostAck` | `0x00000000000ac9c0` |
| `RmcSetMagnetron` | `0x00000000000b1170` |
| `RmcSetMagnetronInd` | `0x00000000000b1210` |
| `RmcSetMBSAdjust` | `0x00000000000ad408` |
| `RmcSetMemory` | `0x00000000000b0fb8` |
| `RmcSetMenuSet` | `0x00000000000af268` |
| `RmcSetMiddleSTC` | `0x00000000000ad5e0` |
| `RmcSetMonitorSelect` | `0x00000000000ad920` |
| `RmcSetNearSTC` | `0x00000000000ad558` |
| `RmcSetNetworkPriority` | `0x00000000000af1e0` |
| `RmcSetNextEcho` | `0x00000000000ae680` |
| `RmcSetNN3Command` | `0x00000000000b0ef8` |
| `RmcSetObservationMode` | `0x00000000000b0d60` |
| `RmcSetOnTime` | `0x00000000000ada30` |
| `RmcSetOperatorFitness` | `0x00000000000afb60` |
| `RmcSetPictureDefault` | `0x00000000000afbb8` |
| `RmcSetPM` | `0x00000000000add70` |
| `RmcSetPmAdjust` | `0x00000000000adb40` |
| `RmcSetPmAdjustEnd` | `0x00000000000af410` |
| `RmcSetPulseRange` | `0x00000000000b10d8` |
| `RmcSetPulseWidth` | `0x00000000000ac400` |
| `RmcSetRadarPosition` | `0x00000000000acd38` |
| `RmcSetRange` | `0x00000000000abc50` |
| `RmcSetRangeSelect` | `0x00000000000b14a0` |
| `RmcSetRange_v2` | `0x00000000000abce8` |
| `RmcSetReferencePoint` | `0x00000000000acf88` |
| `RmcSetRfcVersionNo` | `0x00000000000afc10` |
| `RmcSetRingSuppression` | `0x00000000000ad6f0` |
| `RmcSetRsAllClear` | `0x00000000000b0a48` |
| `RmcSetRsPreset` | `0x00000000000b0780` |
| `RmcSetRxEchoSelect` | `0x00000000000b06d0` |
| `RmcSetSART` | `0x00000000000ade08` |
| `RmcSetSARTSignal` | `0x00000000000b0de8` |
| `RmcSetSelectAlarmOut` | `0x00000000000aee58` |
| `RmcSetSelectAlarmPolarity` | `0x00000000000af020` |
| `RmcSetSelectAntenna` | `0x00000000000aeba0` |
| `RmcSetSelectCrt` | `0x00000000000aef98` |
| `RmcSetSelectQVDisplay` | `0x00000000000ad140` |
| `RmcSetSelectTtmOut` | `0x00000000000af498` |
| `RmcSetShipInfo` | `0x00000000000aecb0` |
| `RmcSetSpeedSelect` | `0x00000000000ae398` |
| `RmcSetSsdTxChannel` | `0x00000000000b0aa0` |
| `RmcSetStartSelfMode` | `0x00000000000b0820` |
| `RmcSetStcRange` | `0x00000000000aff10` |
| `RmcSetStoppedTx` | `0x00000000000ad898` |
| `RmcSetSweepPosition` | `0x00000000000ae8b0` |
| `RmcSetTimingAdjust` | `0x00000000000ad338` |
| `RmcSetTrailClear` | `0x00000000000ae7d0` |
| `RmcSetTrailMode` | `0x00000000000ae708` |
| `RmcSetTrailProcess` | `0x00000000000b04c0` |
| `RmcSetTune` | `0x00000000000acb58` |
| `RmcSetTuneAdjust` | `0x00000000000ace48` |
| `RmcSetTuneIndicator` | `0x00000000000acbf0` |
| `RmcSetTxEchoSelect` | `0x00000000000b0648` |
| `RmcSetTxStby` | `0x00000000000ac4c0` |
| `RmcSetTxTime` | `0x00000000000adab8` |
| `RmcSetVersionNo` | `0x00000000000ade90` |
| `RmcSetVideoLevel` | `0x00000000000b1570` |
| `RmcSetWakeupCount` | `0x00000000000aec28` |

---

## Radar Command Get (RmcGet*) — 143 functions

| Function | Address |
|----------|---------|
| `RmcGet2ndEchoReject` | `0x0000000000092708` |
| `RmcGetAccuShip` | `0x00000000000993a8` |
| `RmcGetACRAIN` | `0x000000000008e7a0` |
| `RmcGetACSEA` | `0x000000000008e5d8` |
| `RmcGetAisAlarm` | `0x0000000000094da8` |
| `RmcGetAlarmAck` | `0x00000000000951b0` |
| `RmcGetAlarmStatus` | `0x0000000000090860` |
| `RmcGetAntennaHeight` | `0x0000000000091330` |
| `RmcGetAntennaNo` | `0x0000000000091f28` |
| `RmcGetAntennaRevolution` | `0x00000000000919f8` |
| `RmcGetAntennaSwitch` | `0x0000000000091b50` |
| `RmcGetAntennaType` | `0x000000000008f668` |
| `RmcGetArpaAcqCorre` | `0x0000000000096590` |
| `RmcGetArpaAcquireTarget` | `0x000000000008f1a8` |
| `RmcGetArpaAcquireTargetDisplayId` | `0x00000000000943e0` |
| `RmcGetArpaAlarm` | `0x0000000000094ef0` |
| `RmcGetArpaAntenna` | `0x0000000000096448` |
| `RmcGetArpaCancelStatus` | `0x0000000000093af0` |
| `RmcGetArpaCancelTarget` | `0x000000000008f320` |
| `RmcGetArpaCancelTargetDisplayId` | `0x0000000000094578` |
| `RmcGetArpaCancelTargetId` | `0x00000000000905d0` |
| `RmcGetArpaCancelTargetNo` | `0x0000000000094148` |
| `RmcGetArpaCatch` | `0x000000000009a300` |
| `RmcGetArpaCpaTcpa` | `0x000000000008fcc0` |
| `RmcGetArpaDetect` | `0x000000000009a188` |
| `RmcGetArpaDispMode` | `0x000000000008f470` |
| `RmcGetArpaEchoLevel` | `0x00000000000961b8` |
| `RmcGetArpaFilterRespons` | `0x0000000000096968` |
| `RmcGetArpaGateSize` | `0x0000000000096820` |
| `RmcGetArpaLandSize` | `0x0000000000096300` |
| `RmcGetArpaLostCount` | `0x0000000000096ab0` |
| `RmcGetArpaLostFilterMode` | `0x0000000000097b18` |
| `RmcGetArpaLostFilterRange` | `0x0000000000097330` |
| `RmcGetArpaLostFilterSpeed` | `0x00000000000974a8` |
| `RmcGetArpaMaxRange` | `0x0000000000096070` |
| `RmcGetArpaMaxSpeed` | `0x0000000000096bf8` |
| `RmcGetArpaPreset` | `0x0000000000093c38` |
| `RmcGetArpaPursuit` | `0x000000000009a4a0` |
| `RmcGetArpaReAcquireTarget` | `0x00000000000983f0` |
| `RmcGetArpaSelectTarget` | `0x000000000008f030` |
| `RmcGetArpaTest` | `0x0000000000092d60` |
| `RmcGetArpaTimeVector` | `0x0000000000096d40` |
| `RmcGetArpaVector` | `0x000000000008fb68` |
| `RmcGetArpaWeed` | `0x00000000000966d8` |
| `RmcGetArpaWithoutGyro` | `0x0000000000093590` |
| `RmcGetAssociationStatus` | `0x0000000000097c60` |
| `RmcGetAtfSettings` | `0x0000000000098cf0` |
| `RmcGetATT` | `0x0000000000090c78` |
| `RmcGetBearingResolutionSelect` | `0x0000000000099120` |
| `RmcGetBlindSector` | `0x0000000000090110` |
| `RmcGetBuildUpTime` | `0x0000000000097858` |
| `RmcGetCancelSelectLostTarget` | `0x0000000000097ec8` |
| `RmcGetCancelSelectTarget` | `0x0000000000097d80` |
| `RmcGetCombineData` | `0x0000000000097008` |
| `RmcGetCombineStatus` | `0x0000000000095610` |
| `RmcGetConnect` | `0x000000000008d728` |
| `RmcGetConningPosition` | `0x0000000000094868` |
| `RmcGetCustomACRAIN` | `0x000000000008e028` |
| `RmcGetCustomACSEA` | `0x000000000008de10` |
| `RmcGetCustomAtfParameter` | `0x0000000000098010` |
| `RmcGetCustomGAIN` | `0x000000000008dc30` |
| `RmcGetCustomMemory` | `0x0000000000097728` |
| `RmcGetCustomPicture` | `0x000000000008e988` |
| `RmcGetCustomPictureAll` | `0x000000000008e260` |
| `RmcGetDemoEcho` | `0x0000000000092478` |
| `RmcGetDisplayUnitInformation` | `0x0000000000097978` |
| `RmcGetDispMode` | `0x000000000008d870` |
| `RmcGetDrift` | `0x0000000000093830` |
| `RmcGetEavWithoutGyro` | `0x00000000000925c0` |
| `RmcGetEchoColorType` | `0x0000000000099250` |
| `RmcGetEquipComp` | `0x0000000000099c10` |
| `RmcGetExtAlarmAck` | `0x0000000000095c98` |
| `RmcGetFarSTC` | `0x0000000000091768` |
| `RmcGetGAIN` | `0x000000000008e420` |
| `RmcGetGC10` | `0x00000000000939a8` |
| `RmcGetGpsAntennaPosition` | `0x00000000000946c0` |
| `RmcGetGuardFan` | `0x0000000000093080` |
| `RmcGetGuardMode` | `0x0000000000092ea8` |
| `RmcGetGuardPolygon` | `0x00000000000932b8` |
| `RmcGetGuardStatus` | `0x000000000008f9f0` |
| `RmcGetHeading` | `0x00000000000909f8` |
| `RmcGetHeadingAdjust` | `0x0000000000090e20` |
| `RmcGetHeadingSource` | `0x0000000000095768` |
| `RmcGetHighSensitivityMode` | `0x0000000000098ff0` |
| `RmcGetIceClass` | `0x0000000000098b98` |
| `RmcGetImageCor` | `0x0000000000099768` |
| `RmcGetImoEchoSwitch` | `0x0000000000095b50` |
| `RmcGetInterswInfo` | `0x00000000000971d8` |
| `RmcGetJummingAble` | `0x0000000000098a50` |
| `RmcGetKeyCommand` | `0x000000000008f880` |
| `RmcGetMagnetron` | `0x0000000000099e98` |
| `RmcGetMagnetronInd` | `0x000000000009a040` |
| `RmcGetMBSAdjust` | `0x00000000000911d8` |
| `RmcGetMemory` | `0x0000000000099ab8` |
| `RmcGetMenuSet` | `0x00000000000959f8` |
| `RmcGetMiddleSTC` | `0x0000000000091620` |
| `RmcGetMonitorSelect` | `0x0000000000091de0` |
| `RmcGetNearSTC` | `0x00000000000914d8` |
| `RmcGetNetworkPriority` | `0x00000000000958b0` |
| `RmcGetNextEcho` | `0x0000000000093dd8` |
| `RmcGetNN3Command` | `0x00000000000998b0` |
| `RmcGetObservationMode` | `0x00000000000994f0` |
| `RmcGetOnTime` | `0x0000000000092070` |
| `RmcGetPM` | `0x0000000000092850` |
| `RmcGetPmAdjust` | `0x0000000000092300` |
| `RmcGetPmAdjustEnd` | `0x0000000000095de0` |
| `RmcGetPulseRange` | `0x0000000000099d58` |
| `RmcGetPulseWidth` | `0x000000000008eb80` |
| `RmcGetRadarPosition` | `0x00000000000902e0` |
| `RmcGetRange` | `0x000000000008d9b0` |
| `RmcGetRangeSelect` | `0x000000000009a6b8` |
| `RmcGetRange_v2` | `0x000000000008daf0` |
| `RmcGetReferencePoint` | `0x0000000000090718` |
| `RmcGetRfcVersionNo` | `0x0000000000096e98` |
| `RmcGetRingSuppression` | `0x00000000000918b0` |
| `RmcGetRsPreset` | `0x00000000000988b0` |
| `RmcGetRxEchoSelect` | `0x0000000000098738` |
| `RmcGetSART` | `0x00000000000929a8` |
| `RmcGetSARTSignal` | `0x0000000000099620` |
| `RmcGetSelectAlarmOut` | `0x0000000000095038` |
| `RmcGetSelectAlarmPolarity` | `0x0000000000095478` |
| `RmcGetSelectAntenna` | `0x00000000000949c0` |
| `RmcGetSelectCrt` | `0x0000000000095330` |
| `RmcGetSelectQVDisplay` | `0x0000000000090b50` |
| `RmcGetSelectTtmOut` | `0x0000000000095f28` |
| `RmcGetShipInfo` | `0x0000000000094c50` |
| `RmcGetSpeedSelect` | `0x00000000000936d8` |
| `RmcGetSsdTxChannel` | `0x0000000000098ea8` |
| `RmcGetStcRange` | `0x0000000000097600` |
| `RmcGetStoppedTx` | `0x0000000000091c98` |
| `RmcGetSweepPosition` | `0x0000000000094290` |
| `RmcGetTimingAdjust` | `0x0000000000090fc8` |
| `RmcGetTrailMode` | `0x0000000000093f20` |
| `RmcGetTrailProcess` | `0x00000000000982a8` |
| `RmcGetTune` | `0x000000000008fe90` |
| `RmcGetTuneAdjust` | `0x0000000000090488` |
| `RmcGetTuneIndicator` | `0x000000000008ffd0` |
| `RmcGetTxEchoSelect` | `0x0000000000098608` |
| `RmcGetTxStby` | `0x000000000008ee98` |
| `RmcGetTxTime` | `0x00000000000921b8` |
| `RmcGetVersionNo` | `0x0000000000092af0` |
| `RmcGetVideoLevel` | `0x000000000009a888` |
| `RmcGetWakeupCount` | `0x0000000000094b08` |

---

## Radar Command Request (RmcReq*) — 133 functions

| Function | Address |
|----------|---------|
| `RmcReq2ndEchoReject` | `0x00000000000a7940` |
| `RmcReqAccuShip` | `0x00000000000aaf40` |
| `RmcReqACRAIN` | `0x00000000000a5ae8` |
| `RmcReqACSEA` | `0x00000000000a5910` |
| `RmcReqAisAlarm` | `0x00000000000a8a88` |
| `RmcReqAlarmAck` | `0x00000000000a8cc8` |
| `RmcReqAlarmStatus` | `0x00000000000a6880` |
| `RmcReqAntennaHeight` | `0x00000000000a6de8` |
| `RmcReqAntennaNo` | `0x00000000000a74c0` |
| `RmcReqAntennaRevolution` | `0x00000000000a71c0` |
| `RmcReqAntennaSwitch` | `0x00000000000a7280` |
| `RmcReqAntennaType` | `0x00000000000a6100` |
| `RmcReqArpaAcqCorre` | `0x00000000000a9768` |
| `RmcReqArpaAlarm` | `0x00000000000a8b48` |
| `RmcReqArpaAntenna` | `0x00000000000a96a8` |
| `RmcReqArpaCancelStatus` | `0x00000000000a82a8` |
| `RmcReqArpaCatch` | `0x00000000000ab778` |
| `RmcReqArpaCpaTcpa` | `0x00000000000a6368` |
| `RmcReqArpaDetect` | `0x00000000000ab6b8` |
| `RmcReqArpaDispMode` | `0x00000000000a6038` |
| `RmcReqArpaEchoLevel` | `0x00000000000a9528` |
| `RmcReqArpaFilterRespons` | `0x00000000000a99a8` |
| `RmcReqArpaGateSize` | `0x00000000000a98e8` |
| `RmcReqArpaLandSize` | `0x00000000000a95e8` |
| `RmcReqArpaLostCount` | `0x00000000000a9a68` |
| `RmcReqArpaLostFilterMode` | `0x00000000000aa2a8` |
| `RmcReqArpaLostFilterRange` | `0x00000000000a9ee8` |
| `RmcReqArpaLostFilterSpeed` | `0x00000000000a9fa8` |
| `RmcReqArpaMaxRange` | `0x00000000000a9468` |
| `RmcReqArpaMaxSpeed` | `0x00000000000a9b28` |
| `RmcReqArpaPreset` | `0x00000000000a8368` |
| `RmcReqArpaPursuit` | `0x00000000000ab838` |
| `RmcReqArpaReAcquireTarget` | `0x00000000000aa750` |
| `RmcReqArpaTest` | `0x00000000000a7c48` |
| `RmcReqArpaTimeVector` | `0x00000000000a9be8` |
| `RmcReqArpaVector` | `0x00000000000a62a8` |
| `RmcReqArpaWeed` | `0x00000000000a9828` |
| `RmcReqArpaWithoutGyro` | `0x00000000000a7fa8` |
| `RmcReqAssociationStatus` | `0x00000000000aa368` |
| `RmcReqAtfSettings` | `0x00000000000aac00` |
| `RmcReqATT` | `0x00000000000a6aa0` |
| `RmcReqBearingResolutionSelect` | `0x00000000000aae00` |
| `RmcReqBlindSector` | `0x00000000000a6568` |
| `RmcReqBuildUpTime` | `0x00000000000aa128` |
| `RmcReqCancelSelectLostTarget` | `0x00000000000aa4e8` |
| `RmcReqCancelSelectTarget` | `0x00000000000aa428` |
| `RmcReqCombineData` | `0x00000000000a9d68` |
| `RmcReqCombineStatus` | `0x00000000000a8f20` |
| `RmcReqConnect` | `0x00000000000a5540` |
| `RmcReqConningPosition` | `0x00000000000a8788` |
| `RmcReqCustomACRAIN` | `0x00000000000a5bf8` |
| `RmcReqCustomACSEA` | `0x00000000000a5a10` |
| `RmcReqCustomAtfParameter` | `0x00000000000aa5a8` |
| `RmcReqCustomGAIN` | `0x00000000000a5840` |
| `RmcReqCustomPicture` | `0x00000000000a5da8` |
| `RmcReqCustomPictureAll` | `0x00000000000a5cd8` |
| `RmcReqDemoEcho` | `0x00000000000a77c0` |
| `RmcReqDisplayUnitInformation` | `0x00000000000aa1e8` |
| `RmcReqDispMode` | `0x00000000000a5600` |
| `RmcReqDrift` | `0x00000000000a8128` |
| `RmcReqEavWithoutGyro` | `0x00000000000a7880` |
| `RmcReqEchoColorType` | `0x00000000000aaea0` |
| `RmcReqEquipComp` | `0x00000000000ab3d8` |
| `RmcReqFarSTC` | `0x00000000000a7040` |
| `RmcReqFreeCommand` | `0x00000000000a5478` |
| `RmcReqGAIN` | `0x00000000000a5740` |
| `RmcReqGC10` | `0x00000000000a81e8` |
| `RmcReqGpsAntennaPosition` | `0x00000000000a86b0` |
| `RmcReqGuardFan` | `0x00000000000a7de0` |
| `RmcReqGuardMode` | `0x00000000000a7d08` |
| `RmcReqGuardPolygon` | `0x00000000000a7ec8` |
| `RmcReqGuardStatus` | `0x00000000000a61c0` |
| `RmcReqHeading` | `0x00000000000a6940` |
| `RmcReqHeadingAdjust` | `0x00000000000a6b78` |
| `RmcReqHeadingSource` | `0x00000000000a8fe0` |
| `RmcReqHighSensitivityMode` | `0x00000000000aad60` |
| `RmcReqIceClass` | `0x00000000000aab40` |
| `RmcReqImageCor` | `0x00000000000ab160` |
| `RmcReqImoEchoSwitch` | `0x00000000000a9228` |
| `RmcReqInterswInfo` | `0x00000000000a9e28` |
| `RmcReqJummingAble` | `0x00000000000aaa80` |
| `RmcReqMagnetron` | `0x00000000000ab538` |
| `RmcReqMagnetronInd` | `0x00000000000ab5f8` |
| `RmcReqMBSAdjust` | `0x00000000000a6d28` |
| `RmcReqMemory` | `0x00000000000ab300` |
| `RmcReqMenuSet` | `0x00000000000a9160` |
| `RmcReqMiddleSTC` | `0x00000000000a6f80` |
| `RmcReqMonitorSelect` | `0x00000000000a7400` |
| `RmcReqNearSTC` | `0x00000000000a6ec0` |
| `RmcReqNetworkPriority` | `0x00000000000a90a0` |
| `RmcReqNextEcho` | `0x00000000000a8468` |
| `RmcReqNN3Command` | `0x00000000000ab220` |
| `RmcReqObservationMode` | `0x00000000000ab000` |
| `RmcReqOnTime` | `0x00000000000a7580` |
| `RmcReqPM` | `0x00000000000a7a00` |
| `RmcReqPmAdjust` | `0x00000000000a7700` |
| `RmcReqPmAdjustEnd` | `0x00000000000a92e8` |
| `RmcReqPulseRange` | `0x00000000000ab498` |
| `RmcReqPulseWidth` | `0x00000000000a5e90` |
| `RmcReqRadarPosition` | `0x00000000000a6628` |
| `RmcReqRange` | `0x00000000000a56a0` |
| `RmcReqRangeSelect` | `0x00000000000ab8f8` |
| `RmcReqReferencePoint` | `0x00000000000a67c0` |
| `RmcReqRfcVersionNo` | `0x00000000000a9ca8` |
| `RmcReqRingSuppression` | `0x00000000000a7100` |
| `RmcReqRsPreset` | `0x00000000000aa980` |
| `RmcReqRxEchoSelect` | `0x00000000000aa8b8` |
| `RmcReqSART` | `0x00000000000a7ac0` |
| `RmcReqSARTSignal` | `0x00000000000ab0a0` |
| `RmcReqSelectAlarmOut` | `0x00000000000a8c08` |
| `RmcReqSelectAlarmPolarity` | `0x00000000000a8e48` |
| `RmcReqSelectAntenna` | `0x00000000000a8848` |
| `RmcReqSelectCrt` | `0x00000000000a8d88` |
| `RmcReqSelectQVDisplay` | `0x00000000000a6a00` |
| `RmcReqSelectTtmOut` | `0x00000000000a93a8` |
| `RmcReqShipInfo` | `0x00000000000a89c8` |
| `RmcReqSpeedSelect` | `0x00000000000a8068` |
| `RmcReqSsdTxChannel` | `0x00000000000aaca0` |
| `RmcReqStcRange` | `0x00000000000aa068` |
| `RmcReqStoppedTx` | `0x00000000000a7340` |
| `RmcReqSweepPosition` | `0x00000000000a85f0` |
| `RmcReqTimingAdjust` | `0x00000000000a6c50` |
| `RmcReqTrailMode` | `0x00000000000a8528` |
| `RmcReqTrailProcess` | `0x00000000000aa690` |
| `RmcReqTune` | `0x00000000000a6428` |
| `RmcReqTuneAdjust` | `0x00000000000a6700` |
| `RmcReqTuneIndicator` | `0x00000000000a64c8` |
| `RmcReqTxEchoSelect` | `0x00000000000aa818` |
| `RmcReqTxStby` | `0x00000000000a5f98` |
| `RmcReqTxTime` | `0x00000000000a7640` |
| `RmcReqVersionNo` | `0x00000000000a7b80` |
| `RmcReqVideoLevel` | `0x00000000000aba00` |
| `RmcReqWakeupCount` | `0x00000000000a8908` |

---

## NAVNET Common (Nc*) — 310 functions

| Function | Address |
|----------|---------|
| `NcBasicInitializationForModules` | `0x0000000000058738` |
| `NcCifRead` | `0x00000000000562a0` |
| `NcCifWrite` | `0x0000000000056290` |
| `NcCloseTcpIp` | `0x00000000000646d0` |
| `NcCommonSPGetUnicastAddress` | `0x0000000000065cd0` |
| `NcCommonSPInitializeUnicast` | `0x00000000000658c8` |
| `NcCommonSPSetUnicastAddress` | `0x0000000000065d08` |
| `NcCommonSPTryConnect` | `0x0000000000065d20` |
| `NcCommonSpTxDepthRange` | `0x0000000000065e28` |
| `NcCommonSPWIFIGetUnicastAddress` | `0x00000000000668d0` |
| `NcCommonSPWIFIInitializeUnicast` | `0x00000000000664c8` |
| `NcCommonSPWIFISetUnicastAddress` | `0x0000000000066908` |
| `NcCommonSPWIFITryConnect` | `0x0000000000066920` |
| `NcCommonSpWifiTxDepthRange` | `0x0000000000066a28` |
| `NcCommonSPWIFIWriteUnicast` | `0x0000000000066770` |
| `NcCommonSPWriteUnicast` | `0x0000000000065b70` |
| `NcConnectDataBaseServer` | `0x00000000000589b0` |
| `NcConnectEtrServer` | `0x0000000000058910` |
| `NcConnectEtrServerEx` | `0x0000000000058918` |
| `NcConnectKeyServer` | `0x00000000000589e8` |
| `NcConnectMcuServer` | `0x0000000000058ae0` |
| `NcConnectRadarServer` | `0x0000000000058830` |
| `NcConnectRadarServerEx` | `0x0000000000058838` |
| `NcConnectSonarServer` | `0x0000000000058b80` |
| `NcConnectSonarServerEx` | `0x0000000000058b88` |
| `NcConnectSystemServer` | `0x0000000000058970` |
| `NcConnectTcpIp` | `0x00000000000645d0` |
| `NcDeleteHostTableByHostName` | `0x0000000000058778` |
| `NcDeleteHostTableByIP` | `0x0000000000058770` |
| `NcEndNavnetCommon` | `0x0000000000058740` |
| `NcEntryCallbackRxFuncCode` | `0x0000000000058598` |
| `NcEntryCommonUdpRxFunction` | `0x00000000000585b0` |
| `NcEntryFnetCallback` | `0x00000000000585a0` |
| `NcEntryFnetShareHostListUpdatedCallback` | `0x00000000000585a8` |
| `NcEntryTxFunc` | `0x0000000000062c08` |
| `NcEtrEchoTxTrigger` | `0x00000000000588c8` |
| `NcFnetLock` | `0x0000000000050a38` |
| `NcFnetService_ConnectToServer` | `0x0000000000058a50` |
| `NcFnetService_Get` | `0x0000000000058a38` |
| `NcFnetService_GetClientCEPID` | `0x0000000000058a70` |
| `NcFnetService_GetClientHostname` | `0x0000000000058a78` |
| `NcFnetService_GetConnectionStatus` | `0x0000000000058a98` |
| `NcFnetService_GetMaster` | `0x0000000000058a90` |
| `NcFnetService_InitialServer` | `0x0000000000058a48` |
| `NcFnetService_IsServer` | `0x0000000000058a88` |
| `NcFnetService_Read` | `0x0000000000058a68` |
| `NcFnetService_Register` | `0x0000000000058a30` |
| `NcFnetService_Send` | `0x0000000000058a58` |
| `NcFnetService_SendSingle` | `0x0000000000058a60` |
| `NcFnetService_Unregister` | `0x0000000000058a40` |
| `NcFnetService_WaitData` | `0x0000000000058a80` |
| `NcFnetUnlock` | `0x0000000000050a48` |
| `NcFuncThroughNmea183FHeading` | `0x0000000000064dd0` |
| `NcFuncThroughNmea183FHeave` | `0x0000000000064db8` |
| `NcFuncThroughNmea183FRollPitch` | `0x0000000000064de8` |
| `NcFuncThroughNmea183FWind` | `0x0000000000064e00` |
| `NcFuncThroughNmea2000FEngine` | `0x0000000000064e18` |
| `NcGetAliveFlag` | `0x0000000000050d98` |
| `NcGetARPAringBufNo` | `0x000000000005afb8` |
| `NcGetBasePortNo` | `0x000000000005a4e0` |
| `NcGetClientHostname` | `0x0000000000058618` |
| `NcGetClientIP` | `0x0000000000058610` |
| `NcGetClientIPAddrWithGroup` | `0x0000000000058798` |
| `NcGetCommonUdpCepID` | `0x0000000000058608` |
| `NcGetConnectionHost` | `0x00000000000585d0` |
| `NcGetDataBaseConnectionStatus` | `0x0000000000058998` |
| `NcGetDatabaseFunctionCode` | `0x0000000000058988` |
| `NcGetDataBaseServer` | `0x0000000000058990` |
| `NcGetEtrFunctionCode` | `0x0000000000058890` |
| `NcGetEtrIpAddress` | `0x00000000000588a0` |
| `NcGetEtrIpAddressEx` | `0x00000000000588a8` |
| `NcGetEtrSourceName` | `0x0000000000058898` |
| `NcGetFecDebugLogEnable` | `0x00000000000537b8` |
| `NcGetFnetMaster` | `0x00000000000585e8` |
| `NcGetFnetPortNo` | `0x00000000000585c8` |
| `NcGetGatewayMask` | `0x000000000005a508` |
| `NcGetHostName` | `0x000000000005a3e8` |
| `NcGetIpAddress` | `0x000000000005a438` |
| `NcGetKeyFunctionCode` | `0x00000000000589c8` |
| `NcGetKeyServer` | `0x00000000000589d8` |
| `NcGetKeySourceName` | `0x00000000000589d0` |
| `NcGetMaster` | `0x00000000000585d8` |
| `NcGetMasterEx` | `0x00000000000585e0` |
| `NcGetMcuFunctionCode` | `0x0000000000058ab0` |
| `NcGetMcuServer` | `0x0000000000058ac0` |
| `NcGetMcuSourceName` | `0x0000000000058ab8` |
| `NcGetModuleVersionNo` | `0x0000000000055a50` |
| `NcGetNetRingName0183FHeading` | `0x000000000005bd80` |
| `NcGetNetRingName0183FHeave` | `0x000000000005c898` |
| `NcGetNetRingName0183FRollPitch` | `0x000000000005d3b0` |
| `NcGetNetRingName0183FWind` | `0x000000000005dec8` |
| `NcGetNetRingName2000FEngine` | `0x000000000005e988` |
| `NcGetNetTargetRingNo` | `0x000000000004d708` |
| `NcGetNetworkBasePortNo` | `0x000000000005a318` |
| `NcGetNetworkDefaultGateway` | `0x000000000005a250` |
| `NcGetNetworkDefaultGatewayWifi` | `0x000000000005a5d8` |
| `NcGetNetworkDefaultGatewayWiredClassC` | `0x000000000005a6c8` |
| `NcGetNetworkIpAddress` | `0x000000000005a138` |
| `NcGetNetworkIpAddressWifi` | `0x000000000005a538` |
| `NcGetNetworkIpAddressWiredClassC` | `0x000000000005a628` |
| `NcGetNetworkSubnetMask` | `0x000000000005a1b8` |
| `NcGetNetworkSubnetMaskWifi` | `0x000000000005a588` |
| `NcGetNetworkSubnetMaskWiredClassC` | `0x000000000005a678` |
| `NcGetPassForMcu` | `0x0000000000058aa8` |
| `NcGetPassForNmeaSPPortARPA` | `0x0000000000058a28` |
| `NcGetPassForRelay` | `0x0000000000062d08` |
| `NcGetPassForSonar` | `0x0000000000058b38` |
| `NcGetPassForSonarEx` | `0x0000000000058b40` |
| `NcGetPutFnetCommand19OnOff` | `0x0000000000058ba0` |
| `NcGetRadarEchoRecvOnOffSetting` | `0x0000000000058bb0` |
| `NcGetRadarFunctionCode` | `0x00000000000587e0` |
| `NcGetRadarIpAddress` | `0x00000000000587f0` |
| `NcGetRadarIpAddressEx` | `0x00000000000587f8` |
| `NcGetRadarSourceName` | `0x00000000000587e8` |
| `NcGetRelayConnectionStatus` | `0x0000000000062d48` |
| `NcGetRelayFunctionCode` | `0x0000000000062d28` |
| `NcGetRelayServer` | `0x0000000000062d38` |
| `NcGetSonarFunctionCode` | `0x0000000000058b48` |
| `NcGetSonarIpAddress` | `0x0000000000058b58` |
| `NcGetSonarIpAddressEx` | `0x0000000000058b60` |
| `NcGetSonarSourceName` | `0x0000000000058b50` |
| `NcGetSubnetMask` | `0x000000000005a480` |
| `NcGetSystemConnectionStatus` | `0x0000000000058960` |
| `NcGetSystemFunctionCode` | `0x0000000000058950` |
| `NcGetSystemServer` | `0x0000000000058958` |
| `NcGetTargetHostName` | `0x00000000000585c0` |
| `NcGetTcpIpStatus` | `0x0000000000064ca8` |
| `NcGetThroughEtrEchoWifi` | `0x0000000000058940` |
| `NcGetThroughEtrEchoWiredClassC` | `0x0000000000058938` |
| `NcGmmClientConnectServer` | `0x0000000000056a88` |
| `NcGmmClientEntryFunctionReceivePacket` | `0x0000000000056a60` |
| `NcGmmClientGetServerName` | `0x0000000000056a70` |
| `NcGmmClientInitialize` | `0x0000000000056878` |
| `NcGmmClientWrite` | `0x00000000000568f8` |
| `NcGmmGetFunctionCode` | `0x0000000000056628` |
| `NcGmmServerEntryFunctionReceivePacket` | `0x0000000000056868` |
| `NcGmmServerInitialize` | `0x0000000000056648` |
| `NcGmmServerWrite` | `0x0000000000056778` |
| `NcHaveNAVNETfunction` | `0x0000000000058780` |
| `NcHostNameToIpAddr` | `0x0000000000058620` |
| `NcInitialCifServer` | `0x0000000000055fb0` |
| `NcInitialCommonSPPortStatus` | `0x0000000000065608` |
| `NcInitialCommonSPServer` | `0x0000000000058a18` |
| `NcInitialCommonSPWIFIPortStatus` | `0x0000000000066208` |
| `NcInitialCommonSPWIFIServer` | `0x0000000000058a20` |
| `NcInitialDataBaseServer` | `0x0000000000058980` |
| `NcInitialEtrClient` | `0x0000000000058880` |
| `NcInitialEtrServer` | `0x0000000000058878` |
| `NcInitialKeyServer` | `0x00000000000589c0` |
| `NcInitialLogOutput` | `0x0000000000065140` |
| `NcInitialMcu` | `0x0000000000058aa0` |
| `NcInitialNavnetCommon` | `0x0000000000058638` |
| `NcInitialNmea0183PortStatus` | `0x000000000005abc8` |
| `NcInitialNmea0183SPPortStatus` | `0x0000000000066d50` |
| `NcInitialNmea0183SPWIFIPortStatus` | `0x0000000000067a40` |
| `NcInitialNmea183FHeadingServer` | `0x000000000005bd90` |
| `NcInitialNmea183FHeaveServer` | `0x000000000005c8a8` |
| `NcInitialNmea183FRollPitchServer` | `0x000000000005d3c0` |
| `NcInitialNmea183FWindServer` | `0x000000000005ded8` |
| `NcInitialNmea183Server` | `0x0000000000058a00` |
| `NcInitialNmea183SPServer` | `0x0000000000058a08` |
| `NcInitialNmea183SPWIFIServer` | `0x0000000000058a10` |
| `NcInitialNmea2000FEngineServer` | `0x000000000005e998` |
| `NcInitialRadarClient` | `0x00000000000587d8` |
| `NcInitialRadarServer` | `0x00000000000587d0` |
| `NcInitialSonarClient` | `0x0000000000058b30` |
| `NcInitialSonarServer` | `0x0000000000058b28` |
| `NcInitialSystemServer` | `0x0000000000058948` |
| `NcInitialTargetInfo` | `0x0000000000058850` |
| `NcInitialTcpIp` | `0x0000000000064318` |
| `NcInitRelay` | `0x0000000000062bf8` |
| `NcIsEtrServer` | `0x00000000000588b0` |
| `NcIsEtrServerEx` | `0x00000000000588b8` |
| `NcIsRadarServer` | `0x0000000000058800` |
| `NcIsSonarServer` | `0x0000000000058b68` |
| `NcKillAllTask` | `0x0000000000058630` |
| `NcMakeBroadcastAddress` | `0x00000000000585f0` |
| `NcMakeBroadcastAddressWifi` | `0x00000000000585f8` |
| `NcMakeBroadcastAddressWiredClassC` | `0x0000000000058600` |
| `NcNetworkCIFWrite` | `0x0000000000055d68` |
| `NcNetworkReadCOMMONSP` | `0x00000000000658a8` |
| `NcNetworkReadCOMMONSPWIFI` | `0x00000000000664a8` |
| `NcNetworkReadNMEA0183` | `0x000000000005af98` |
| `NcNetworkReadNMEA0183FHeading` | `0x000000000005bfe0` |
| `NcNetworkReadNMEA0183FHeave` | `0x000000000005caf8` |
| `NcNetworkReadNMEA0183FRollPitch` | `0x000000000005d610` |
| `NcNetworkReadNMEA0183FWind` | `0x000000000005e118` |
| `NcNetworkReadNMEA0183SP` | `0x0000000000067438` |
| `NcNetworkReadNMEA0183SPWIFI` | `0x0000000000067f78` |
| `NcNetworkReadNMEA2000FEngine` | `0x000000000005eb98` |
| `NcNetworkSPWIFIWrite` | `0x0000000000067828` |
| `NcNetworkWrite` | `0x000000000005a9b0` |
| `NcNetworkWriteCOMMONSP` | `0x0000000000065890` |
| `NcNetworkWriteCOMMONSPWIFI` | `0x0000000000066490` |
| `NcNetworkWriteNMEA0183` | `0x000000000005af68` |
| `NcNetworkWriteNMEA0183FHeading` | `0x000000000005bfd0` |
| `NcNetworkWriteNMEA0183FHeave` | `0x000000000005cae8` |
| `NcNetworkWriteNMEA0183FRollPitch` | `0x000000000005d600` |
| `NcNetworkWriteNMEA0183FWind` | `0x000000000005e108` |
| `NcNetworkWriteNMEA0183SP` | `0x0000000000067270` |
| `NcNetworkWriteNMEA0183SPWIFI` | `0x0000000000067f68` |
| `NcNetworkWriteNMEA2000FEngine` | `0x000000000005eb88` |
| `NcNmea0183FHeading` | `0x000000000005bd60` |
| `NcNmea0183FHeave` | `0x000000000005c858` |
| `NcNmea0183FRollPitch` | `0x000000000005d370` |
| `NcNmea0183FWind` | `0x000000000005de88` |
| `NcNmea0183GetUnicastAddress` | `0x000000000005b4d8` |
| `NcNmea0183InitializeUnicast` | `0x000000000005b058` |
| `NcNmea0183SetUnicastAddress` | `0x000000000005b510` |
| `NcNmea0183SPTryConnect` | `0x0000000000067458` |
| `NcNmea0183SPWIFITryConnect` | `0x0000000000067f98` |
| `NcNmea0183WriteUnicast` | `0x000000000005b390` |
| `NcNmea2000FEngine` | `0x000000000005e948` |
| `NcNotifyKeyConnState` | `0x0000000000058b10` |
| `NcOpenTcpIp` | `0x0000000000064388` |
| `NcOutputEtrEcho` | `0x000000000004ed30` |
| `NcOutputLogData` | `0x0000000000065220` |
| `NcPutArpaData` | `0x0000000000058848` |
| `NcPutNmeaData` | `0x00000000000589f8` |
| `NcRadarEchoTxTrigger` | `0x0000000000058810` |
| `NcReadDatabaseData` | `0x00000000000589b8` |
| `NcReadEtrData` | `0x0000000000058920` |
| `NcReadKeyData` | `0x00000000000589f0` |
| `NcReadMcuClientData` | `0x0000000000058b00` |
| `NcReadMcuData` | `0x0000000000058ae8` |
| `NcReadMcuServerData` | `0x0000000000058af0` |
| `NcReadRadarData` | `0x0000000000058840` |
| `NcReadSonarData` | `0x0000000000058b90` |
| `NcReadSystemData` | `0x0000000000058978` |
| `NcReadTargetData` | `0x0000000000058860` |
| `NcReadTargetDataForCOMMONSP` | `0x0000000000065860` |
| `NcReadTargetDataForCOMMONSPWIFI` | `0x0000000000066460` |
| `NcReadTargetDataForNMEA0183` | `0x000000000005aed8` |
| `NcReadTargetDataForNMEA0183SP` | `0x00000000000671e0` |
| `NcReadTargetDataForNMEA0183SPWIFI` | `0x0000000000067ed8` |
| `NcReadTcpIp` | `0x0000000000064740` |
| `NcReadTcpIpWithoutCache` | `0x0000000000064900` |
| `NcSendChangeGatewayAddress` | `0x0000000000058758` |
| `NcSendChangeHostName` | `0x0000000000058750` |
| `NcSendChangeIpAddress` | `0x0000000000058748` |
| `NcSendChangeSubnetMask` | `0x0000000000058760` |
| `NcSendDataBaseCommand` | `0x00000000000589a0` |
| `NcSendDataBaseCommandSingle` | `0x00000000000589a8` |
| `NcSendDigitEchoData` | `0x00000000000588d8` |
| `NcSendEtrCommand` | `0x0000000000058900` |
| `NcSendEtrCommandEx` | `0x0000000000058908` |
| `NcSendEtrEchoData` | `0x00000000000588d0` |
| `NcSendKeyCommand` | `0x00000000000589e0` |
| `NcSendMcuCommandToClient` | `0x0000000000058ad8` |
| `NcSendMcuCommandToLocal` | `0x0000000000058ac8` |
| `NcSendMcuCommandToServer` | `0x0000000000058ad0` |
| `NcSendPutFnetStartRequest` | `0x00000000000585b8` |
| `NcSendRadarCommand` | `0x0000000000058820` |
| `NcSendRadarCommandEx` | `0x0000000000058828` |
| `NcSendRadarEchoData` | `0x0000000000058818` |
| `NcSendRelayData` | `0x0000000000062d70` |
| `NcSendRestart` | `0x0000000000058768` |
| `NcSendSonarCommand` | `0x0000000000058b70` |
| `NcSendSonarCommandEx` | `0x0000000000058b78` |
| `NcSendSystemCommand` | `0x0000000000058968` |
| `NcSendTargetData` | `0x0000000000058858` |
| `NcSetBasePortNo` | `0x000000000005a4b0` |
| `NcSetChangeGatewayAddressCallbackFunc` | `0x0000000000051290` |
| `NcSetChangeHostNameCallbackFunc` | `0x0000000000051280` |
| `NcSetChangeIpAddressCallbackFunc` | `0x0000000000051270` |
| `NcSetChangeSubnetMaskCallbackFunc` | `0x00000000000512a0` |
| `NcSetEtrConnectionFunc` | `0x0000000000058868` |
| `NcSetEtrEchoCallbackFunc` | `0x0000000000058870` |
| `NcSetEtrEchoTxTriggerCallbackFunc` | `0x00000000000588c0` |
| `NcSetEtrTxEchoAddress` | `0x0000000000058888` |
| `NcSetFecDebugLogEnable` | `0x00000000000537b0` |
| `NcSetGatewayMask` | `0x000000000005a4f0` |
| `NcSetHostName` | `0x000000000005a360` |
| `NcSetIpAddress` | `0x000000000005a430` |
| `NcSetNetworkBasePortNo` | `0x000000000005a2c8` |
| `NcSetNetworkDefaultGateway` | `0x000000000005a230` |
| `NcSetNetworkDefaultGatewayWifi` | `0x000000000005a5b8` |
| `NcSetNetworkDefaultGatewayWiredClassC` | `0x000000000005a6a8` |
| `NcSetNetworkIpAddress` | `0x000000000005a018` |
| `NcSetNetworkIpAddressWifi` | `0x000000000005a518` |
| `NcSetNetworkIpAddressWiredClassC` | `0x000000000005a608` |
| `NcSetNetworkSubnetMask` | `0x000000000005a1b0` |
| `NcSetNetworkSubnetMaskWifi` | `0x000000000005a568` |
| `NcSetNetworkSubnetMaskWiredClassC` | `0x000000000005a658` |
| `NcSetPutFnetCommand19OnOff` | `0x0000000000058b98` |
| `NcSetRadarConnectionFunc` | `0x00000000000587b8` |
| `NcSetRadarEchoCallbackFunc` | `0x00000000000587c0` |
| `NcSetRadarEchoRecvOnOff` | `0x0000000000058ba8` |
| `NcSetRadarEchoTxTriggerCallbackFunc` | `0x0000000000058808` |
| `NcSetRadarTileEchoCallbackFunc` | `0x00000000000587c8` |
| `NcSetRadarTxEchoAddress` | `0x00000000000587b0` |
| `NcSetRestartCallbackFunc` | `0x0000000000058578` |
| `NcSetShareGroups` | `0x0000000000058588` |
| `NcSetSleepCallbackFunc` | `0x0000000000058590` |
| `NcSetSleepMode` | `0x0000000000058580` |
| `NcSetSonarConnectionFunc` | `0x0000000000058b18` |
| `NcSetSonarEchoCallbackFunc` | `0x0000000000058b20` |
| `NcSetSubnetMask` | `0x000000000005a478` |
| `NcSetTcpIpStatus` | `0x0000000000064c70` |
| `NcSetThroughEtrEchoWifi` | `0x0000000000058930` |
| `NcSetThroughEtrEchoWiredClassC` | `0x0000000000058928` |
| `NcStartRelayConnection` | `0x0000000000062bd0` |
| `NcStopRelayConnection` | `0x0000000000062bf0` |
| `NcUpnpClose` | `0x0000000000058bc0` |
| `NcUpnpIsOpened` | `0x0000000000058bc8` |
| `NcUpnpOpen` | `0x0000000000058bb8` |
| `NcWakeUpFnetTask` | `0x0000000000058628` |
| `NcWatchAlarmFnetClientDown` | `0x0000000000058790` |
| `NcWatchAlarmFnetServerDown` | `0x0000000000058788` |
| `NcWriteTcpIp` | `0x0000000000064ab8` |

---

## ITRON RTOS Shim — 62 functions

The library includes a userspace ITRON (Industrial TRON) real-time OS emulation layer. ITRON is a Japanese RTOS standard commonly used in embedded systems. This shim maps ITRON APIs to Linux pthreads/sockets.

### Key ITRON primitives:

| Primitive | Function | Linux Equivalent |
|-----------|----------|-----------------|
| Task | `acre_tsk` (create), `sta_tsk` (start), `del_tsk` (delete) | `pthread_create` |
| Semaphore | `acre_sem` (create), `wai_sem` (wait), `sig_sem` (signal) | `sem_wait`/`sem_post` |
| Event Flag | `acre_flg` (create), `set_flg` (set), `clr_flg` (clear), `wai_flg` (wait) | `pthread_cond` + bitmask |
| Mailbox/MBF | `acre_mbf` (create), `snd_mbf` (send), `rcv_mbf` (receive) | Message queue / pipe |
| Cyclic | `acre_cyc` (create periodic timer) | `timer_create` |
| Interrupt | `c_di` (disable), `c_ei` (enable) | Mutex lock/unlock |
| Queue | `c_enq` (enqueue), `c_deq` (dequeue) | Ring buffer |

### Exported ITRON symbols:

| Function | Address |
|----------|---------|
| `acre_cyc` | `0x000000000004c770` |
| `acre_flg` | `0x000000000004c680` |
| `acre_mbf` | `0x000000000004c6d0` |
| `acre_sem` | `0x000000000004c630` |
| `acre_tsk` | `0x000000000004c550` |
| `c_deq` | `0x000000000004c4d0` |
| `c_di` | `0x000000000004c440` |
| `c_ei` | `0x000000000004c450` |
| `c_enq` | `0x000000000004c4c0` |
| `nn_acre_cyc` | `0x000000000004b900` |
| `nn_acre_flg` | `0x000000000004b770` |
| `nn_acre_mbf` | `0x000000000004b838` |
| `nn_acre_sem` | `0x000000000004b6a0` |
| `nn_acre_tsk` | `0x000000000004b528` |

---

## Error API (Le*) — 6 functions

| Function | Address |
|----------|---------|
| `cpnt_FurunoLink_CreateHeader_WithoutContentLength` | `0x000000000006d188` |
| `cpnt_StringLeftShift` | `0x0000000000068d18` |
| `CutLowLevelEcho` | `0x00000000000724c0` |
| `EscGetBottomLevel` | `0x00000000000d5a98` |
| `EscGetFishDetectLevel` | `0x00000000000dd720` |
| `EscGetFishLengthCorrection` | `0x00000000000da4b0` |
| `EscGetFmCwBottomLevel` | `0x00000000000dbec8` |
| `EscGetNoiseDetectionLevel` | `0x00000000000db1e0` |
| `esConvBottomLevel` | `0x00000000000c8688` |
| `esConvFishDetectLevel` | `0x00000000000d28d8` |
| `esConvFishLengthCorrection` | `0x00000000000ce7b0` |
| `esConvFmCwBottomLevel` | `0x00000000000d0868` |
| `esConvNoiseDetectionLevel` | `0x00000000000cf880` |
| `EscReqBottomLevel` | `0x00000000000e9148` |
| `EscReqFishDetectLevel` | `0x00000000000ec9e8` |
| `EscReqFishLengthCorrection` | `0x00000000000eb2c0` |
| `EscReqFmCwBottomLevel` | `0x00000000000ebea0` |
| `EscReqNoiseDetectionLevel` | `0x00000000000eb8f8` |
| `EscSetBottomLevel` | `0x00000000000ee1c8` |
| `EscSetFishDetectLevel` | `0x00000000000f2fb8` |
| `EscSetFishLengthCorrection` | `0x00000000000f1098` |
| `EscSetFmCwBottomLevel` | `0x00000000000f2068` |
| `EscSetNoiseDetectionLevel` | `0x00000000000f18b8` |
| `esMakeComBottomLevel` | `0x00000000000e0408` |
| `esMakeComFishDetectLevel` | `0x00000000000e6678` |
| `esMakeComFishLengthCorrection` | `0x00000000000e40f0` |
| `esMakeComFmCwBottomLevel` | `0x00000000000e5328` |
| `esMakeComNoiseDetectionLevel` | `0x00000000000e4b50` |
| `esSubSetBottomLevel` | `0x00000000000f5710` |
| `esSubSetFishDetectLevel` | `0x00000000000fe518` |
| `esSubSetFishLengthCorrection` | `0x00000000000faae8` |
| `esSubSetFmCwBottomLevel` | `0x00000000000fc8e0` |
| `esSubSetNoiseDetectionLevel` | `0x00000000000fbaa0` |
| `FecLogger_GetLogLevel` | `0x0000000000041c08` |
| `FecLogger_SetLogLevel` | `0x0000000000041b98` |
| `GzGetDetectLevel` | `0x0000000000071a90` |
| `GzPutDetectLevel` | `0x00000000000716b8` |
| `LeClearLastAPIError` | `0x0000000000100818` |
| `LeClearLastBackgroundError` | `0x0000000000100830` |
| `LeGetLastError` | `0x0000000000100848` |
| `RmcGetArpaEchoLevel` | `0x00000000000961b8` |
| `RmcGetVideoLevel` | `0x000000000009a888` |
| `rmConvArpaEchoLevel` | `0x00000000000868a0` |
| `rmConvVideoLevel` | `0x000000000008d538` |
| `RmcReqArpaEchoLevel` | `0x00000000000a9528` |
| `RmcReqVideoLevel` | `0x00000000000aba00` |
| `RmcSetArpaEchoLevel` | `0x00000000000af5a8` |
| `RmcSetVideoLevel` | `0x00000000000b1570` |
| `RmGetGzDetectLevel` | `0x0000000000071a68` |
| `rmMakeComArpaEchoLevel` | `0x00000000000a1a60` |
| `rmMakeComVideoLevel` | `0x00000000000a5380` |
| `RmSetGzDetectLevel` | `0x00000000000715f0` |
| `RmSetLowLevelEcho` | `0x0000000000073c80` |
| `rmSubSetArpaEchoLevel` | `0x00000000000b9738` |
| `rmSubSetVideoLevel` | `0x00000000000be398` |
| `uint32RotateLeft` | `0x00000000000c2080` |
| `uint64RotateLeft` | `0x00000000000c2098` |
| `uint8RotateLeft` | `0x00000000000c2060` |
| `uintxRotateLeft` | `0x00000000000c1f58` |

---

## Crypto — 14 functions

| Function | Address |
|----------|---------|
| `bfCheckCode` | `0x00000000000bf2c8` |
| `bfDecode` | `0x00000000000bf3f8` |
| `bfDecrypt` | `0x00000000000bf2b0` |
| `bfEncrypt` | `0x00000000000bf298` |
| `bfInitialize` | `0x00000000000bf118` |
| `Blowfish_decipher` | `0x00000000000be940` |
| `Blowfish_encipher` | `0x00000000000be520` |
| `InitializeBlowfish` | `0x00000000000bed68` |

---

## Licensing / FLM — 46 functions

| Function | Address |
|----------|---------|
| `FlmCertificateEquipment` | `0x00000000000bf7f8` |
| `FlmCertificateFunction` | `0x00000000000bf8a8` |
| `FlmGetPermissionStatus` | `0x00000000000bfb40` |
| `FlmGetVendorID` | `0x00000000000bf958` |
| `FlmInitial` | `0x00000000000bf560` |
| `FlmPreparation` | `0x00000000000bf768` |
| `FlmSetInitialData` | `0x00000000000bf6f8` |
| `InitialFlm` | `0x00000000000c1340` |
| `InitialFlmNewLicense` | `0x00000000000c0de0` |
| `InitialFlmSoftwareCode_1` | `0x00000000000c0cb8` |
| `InitialFlmTimebombLicense` | `0x00000000000c1208` |
| `Initialize_Trial` | `0x00000000000c1480` |
| `PreparationFlmNewLicense` | `0x00000000000c1030` |

---

## Imported Functions (from glibc) — 164

These are the ONLY external dependencies. The Rust reimplementation must provide equivalent functionality.

| Function |
|----------|
| `accept@@GLIBC_2.17` |
| `accept@GLIBC_2.17` |
| `bind@@GLIBC_2.17` |
| `bind@GLIBC_2.17` |
| `c[...]@GLIBC_2.17` |
| `close@@GLIBC_2.17` |
| `close@GLIBC_2.17` |
| `connect@@GLIBC_2.17` |
| `__ctype_b_loc@@G[...]` |
| `__ctype_tolower_[...]` |
| `__ctype_toupper_[...]` |
| `__cxa_finalize@@[...]` |
| `d[...]@GLIBC_2.17` |
| `difftime@@GLIBC_2.17` |
| `__errno_location[...]` |
| `exit@@GLIBC_2.17` |
| `exit@GLIBC_2.17` |
| `fclose@@GLIBC_2.17` |
| `fclose@GLIBC_2.17` |
| `fcntl@@GLIBC_2.17` |
| `fcntl@GLIBC_2.17` |
| `__fdelt_chk@@GLI[...]` |
| `fgets@@GLIBC_2.17` |
| `fgets@GLIBC_2.17` |
| `f[...]@GLIBC_2.17` |
| `fopen@@GLIBC_2.17` |
| `fopen@GLIBC_2.17` |
| `__fprintf_chk@@G[...]` |
| `fputs@@GLIBC_2.17` |
| `fputs@GLIBC_2.17` |
| `fread@@GLIBC_2.17` |
| `fread@GLIBC_2.17` |
| `free@@GLIBC_2.17` |
| `free@GLIBC_2.17` |
| `freeifaddrs@@GLI[...]` |
| `fwrite@@GLIBC_2.17` |
| `fwrite@GLIBC_2.17` |
| `getenv@@GLIBC_2.17` |
| `getenv@GLIBC_2.17` |
| `getifaddrs@@GLIB[...]` |
| `getpid@@GLIBC_2.17` |
| `getpid@GLIBC_2.17` |
| `gettimeofday@@GL[...]` |
| `g[...]@GLIBC_2.17` |
| `g[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `_[...]@GLIBC_2.17` |
| `gmtime@@GLIBC_2.17` |
| `gmtime@GLIBC_2.17` |
| `ioctl@@GLIBC_2.17` |
| `ioctl@GLIBC_2.17` |
| `__isoc99_sscanf@[...]` |
| `listen@@GLIBC_2.17` |
| `listen@GLIBC_2.17` |
| `malloc@@GLIBC_2.17` |
| `malloc@GLIBC_2.17` |
| `memcmp@@GLIBC_2.17` |
| `memcmp@GLIBC_2.17` |
| `__memcpy_chk@@GL[...]` |
| `memcpy@@GLIBC_2.17` |
| `memcpy@GLIBC_2.17` |
| `memmove@@GLIBC_2.17` |
| `memset@@GLIBC_2.17` |
| `memset@GLIBC_2.17` |
| `m[...]@GLIBC_2.17` |
| `nanosleep@@GLIBC_2.17` |
| `n[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `p[...]@GLIBC_2.17` |
| `__printf_chk@@GL[...]` |
| `pthread_attr_des[...]` |
| `pthread_attr_ini[...]` |
| `pthread_attr_set[...]` |
| `pthread_cond_ini[...]` |
| `pthread_cond_sig[...]` |
| `pthread_cond_tim[...]` |
| `pthread_cond_wai[...]` |
| `pthread_exit@@GL[...]` |
| `pthread_mutex_in[...]` |
| `pthread_mutex_lo[...]` |
| `pthread_mutex_un[...]` |
| `pthread_self@@GL[...]` |
| `pthread_setcance[...]` |
| `realloc@@GLIBC_2.17` |
| `recvfrom@@GLIBC_2.17` |
| `recv@@GLIBC_2.17` |
| `recv@GLIBC_2.17` |
| `r[...]@GLIBC_2.17` |
| `r[...]@GLIBC_2.17` |
| `select@@GLIBC_2.17` |
| `select@GLIBC_2.17` |
| `send@@GLIBC_2.17` |
| `send@GLIBC_2.17` |
| `sendto@@GLIBC_2.17` |
| `sendto@GLIBC_2.17` |
| `setsockopt@@GLIB[...]` |
| `s[...]@GLIBC_2.17` |
| `s[...]@GLIBC_2.17` |
| `s[...]@GLIBC_2.17` |
| `s[...]@GLIBC_2.17` |
| `s[...]@GLIBC_2.17` |
| `s[...]@GLIBC_2.17` |
| `s[...]@GLIBC_2.17` |
| `shutdown@@GLIBC_2.17` |
| `__snprintf_chk@@[...]` |
| `socket@@GLIBC_2.17` |
| `socket@GLIBC_2.17` |
| `__sprintf_chk@@G[...]` |
| `sprintf@@GLIBC_2.17` |
| `__stack_chk_fail[...]` |
| `__stpcpy_chk@@GL[...]` |
| `__strcat_chk@@GL[...]` |
| `strcat@@GLIBC_2.17` |
| `strcat@GLIBC_2.17` |
| `strchr@@GLIBC_2.17` |
| `strchr@GLIBC_2.17` |
| `strcmp@@GLIBC_2.17` |
| `strcmp@GLIBC_2.17` |
| `__strcpy_chk@@GL[...]` |
| `strcpy@@GLIBC_2.17` |
| `strcpy@GLIBC_2.17` |
| `strftime@@GLIBC_2.17` |
| `strlen@@GLIBC_2.17` |
| `strlen@GLIBC_2.17` |
| `strncmp@@GLIBC_2.17` |
| `strncpy@@GLIBC_2.17` |
| `strtok@@GLIBC_2.17` |
| `strtok@GLIBC_2.17` |
| `strtol@@GLIBC_2.17` |
| `strtol@GLIBC_2.17` |
| `syscall@@GLIBC_2.17` |
| `time@@GLIBC_2.17` |
| `time@GLIBC_2.17` |
| `__vsnprintf_chk@[...]` |
| `__vsprintf_chk@@[...]` |
| `__xstat@@GLIBC_2.17` |

---

## Functions NOT Relevant to Radar (can skip)

- **Echo Sounder (Esc\*/Es\*)**: 882 functions for fish-finder/depth sounder hardware. Not needed for radar.
- **FVOS**: 44 functions, likely a virtual OS abstraction. May need partial reimplementation if init path requires it.
- **Friend**: 8 functions, unknown purpose. Investigate if referenced by radar init path.

---

## Internal Function Architecture

### Command Processing Pattern

Every radar command follows a consistent 4-function chain:

```
RmcSet<Cmd>()          Public API: user calls this
  → rmSubSet<Cmd>()    Internal: validates params, prepares state
    → rmMakeCom<Cmd>() Internal: serializes into wire format packet
      → NcSendRadarCommand()  Network: sends packet over UDP
```

For receiving:
```
UDP packet arrives
  → rmConv<Cmd>()      Internal: deserializes wire bytes into state struct
    → RmcGet<Cmd>()    Public API: user reads the stored state
```

Request path:
```
RmcReq<Cmd>()          Public API: explicitly request latest value
  → sends request packet → radar responds → rmConv<Cmd>() → state updated
```

### Internal function counts:

| Type | Prefix | Count | Role |
|------|--------|-------|------|
| Subroutine handlers | `rmSubSet*` | 153 | Validate and prepare command |
| Packet builders | `rmMakeCom*` | 153 | Serialize to wire format |
| Response converters | `rmConv*` | 155 | Deserialize from wire format |
| Internal state | `rmSet*/rmGet*` | 6 | Low-level state access |
| **Total internal** | | **467** | |

This means reimplementing the radar command layer requires understanding the wire format, NOT reimplementing each of the 467 internal functions individually. Once we know the serialization format, one generic Rust function can replace the entire `rmMakeCom*` family.


---

## Full Internal Function Listings

### rmSubSet* (153 functions)

- `rmSubSet2ndEchoReject`
- `rmSubSetAccuShip`
- `rmSubSetACRAIN`
- `rmSubSetACSEA`
- `rmSubSetAisAlarm`
- `rmSubSetAlarmAck`
- `rmSubSetAlarmStatus`
- `rmSubSetAliveCheck`
- `rmSubSetAntennaHeight`
- `rmSubSetAntennaNo`
- `rmSubSetAntennaRevolution`
- `rmSubSetAntennaSwitch`
- `rmSubSetAntennaType`
- `rmSubSetArpaAcqCorre`
- `rmSubSetArpaAcquireTarget`
- `rmSubSetArpaAcquireTargetDisplayId`
- `rmSubSetArpaAlarm`
- `rmSubSetArpaAllClear`
- `rmSubSetArpaAnalyseInfo`
- `rmSubSetArpaAntenna`
- `rmSubSetArpaCancelStatus`
- `rmSubSetArpaCancelTarget`
- `rmSubSetArpaCancelTargetDisplayId`
- `rmSubSetArpaCancelTargetId`
- `rmSubSetArpaCancelTargetNo`
- `rmSubSetArpaCatch`
- `rmSubSetArpaCpaTcpa`
- `rmSubSetArpaDetect`
- `rmSubSetArpaDispMode`
- `rmSubSetArpaEchoLevel`
- `rmSubSetArpaFilterRespons`
- `rmSubSetArpaGateSize`
- `rmSubSetArpaJudgeInfo`
- `rmSubSetArpaLandSize`
- `rmSubSetArpaLostCount`
- `rmSubSetArpaLostFilterMode`
- `rmSubSetArpaLostFilterRange`
- `rmSubSetArpaLostFilterSpeed`
- `rmSubSetArpaMaxRange`
- `rmSubSetArpaMaxSpeed`
- `rmSubSetArpaPreset`
- `rmSubSetArpaPursuit`
- `rmSubSetArpaReAcquireTarget`
- `rmSubSetArpaSelectTarget`
- `rmSubSetArpaTargetAllClear`
- `rmSubSetArpaTargetInfo`
- `rmSubSetArpaTest`
- `rmSubSetArpaTimeVector`
- `rmSubSetArpaVector`
- `rmSubSetArpaWeed`
- `rmSubSetArpaWithoutGyro`
- `rmSubSetAssociationStatus`
- `rmSubSetAtfSettings`
- `rmSubSetATT`
- `rmSubSetBearingResolutionSelect`
- `rmSubSetBlindSector`
- `rmSubSetBuildUpTime`
- `rmSubSetCancelSelectLostTarget`
- `rmSubSetCancelSelectTarget`
- `rmSubSetCombineData`
- `rmSubSetCombineStatus`
- `rmSubSetConnect`
- `rmSubSetConningPosition`
- `rmSubSetCustomAtfParameter`
- `rmSubSetCustomMemory`
- `rmSubSetCustomPicture`
- `rmSubSetCustomPictureAll`
- `rmSubSetDemoEcho`
- `rmSubSetDisplayUnitInformation`
- `rmSubSetDispMode`
- `rmSubSetDrift`
- `rmSubSetEavWithoutGyro`
- `rmSubSetEchoClear`
- `rmSubSetEchoColorType`
- `rmSubSetEquipComp`
- `rmSubSetExtAlarmAck`
- `rmSubSetFarSTC`
- `rmSubSetFreeCommand`
- `rmSubSetGAIN`
- `rmSubSetGC10`
- `rmSubSetGpsAntennaPosition`
- `rmSubSetGuardFan`
- `rmSubSetGuardMode`
- `rmSubSetGuardPolygon`
- `rmSubSetGuardStatus`
- `rmSubSetHeading`
- `rmSubSetHeadingAdjust`
- `rmSubSetHeadingSource`
- `rmSubSetHighSensitivityMode`
- `rmSubSetIceClass`
- `rmSubSetImageCor`
- `rmSubSetImoEchoSwitch`
- `rmSubSetInterswInfo`
- `rmSubSetJummingAble`
- `rmSubSetKeyCommand`
- `rmSubSetLostAck`
- `rmSubSetMagnetron`
- `rmSubSetMagnetronInd`
- `rmSubSetMBSAdjust`
- `rmSubSetMemory`
- `rmSubSetMenuSet`
- `rmSubSetMiddleSTC`
- `rmSubSetMonitorSelect`
- `rmSubSetNearSTC`
- `rmSubSetNetworkPriority`
- `rmSubSetNextEcho`
- `rmSubSetNN3Command`
- `rmSubSetObservationMode`
- `rmSubSetOnTime`
- `rmSubSetOperatorFitness`
- `rmSubSetPictureDefault`
- `rmSubSetPM`
- `rmSubSetPmAdjust`
- `rmSubSetPmAdjustEnd`
- `rmSubSetPulseRange`
- `rmSubSetPulseWidth`
- `rmSubSetRadarPosition`
- `rmSubSetRange`
- `rmSubSetRangeSelect`
- `rmSubSetReferencePoint`
- `rmSubSetRfcVersionNo`
- `rmSubSetRingSuppression`
- `rmSubSetRsAllClear`
- `rmSubSetRsPreset`
- `rmSubSetRxEchoSelect`
- `rmSubSetSART`
- `rmSubSetSARTSignal`
- `rmSubSetSelectAlarmOut`
- `rmSubSetSelectAlarmPolarity`
- `rmSubSetSelectAntenna`
- `rmSubSetSelectCrt`
- `rmSubSetSelectQVDisplay`
- `rmSubSetSelectTtmOut`
- `rmSubSetShipInfo`
- `rmSubSetSpeedSelect`
- `rmSubSetSsdTxChannel`
- `rmSubSetStartSelfMode`
- `rmSubSetStcRange`
- `rmSubSetStoppedTx`
- `rmSubSetSweepPosition`
- `rmSubSetTimingAdjust`
- `rmSubSetTrailClear`
- `rmSubSetTrailMode`
- `rmSubSetTrailProcess`
- `rmSubSetTune`
- `rmSubSetTuneAdjust`
- `rmSubSetTuneIndicator`
- `rmSubSetTxEchoSelect`
- `rmSubSetTxStby`
- `rmSubSetTxTime`
- `rmSubSetVersionNo`
- `rmSubSetVideoLevel`
- `rmSubSetWakeupCount`

### rmMakeCom* (153 functions)

- `rmMakeCom2ndEchoReject`
- `rmMakeComAccuShip`
- `rmMakeComACRAIN`
- `rmMakeComACSEA`
- `rmMakeComAisAlarm`
- `rmMakeComAlarm`
- `rmMakeComAlarmAck`
- `rmMakeComAliveCheck`
- `rmMakeComAntennaHeight`
- `rmMakeComAntennaNo`
- `rmMakeComAntennaRevolution`
- `rmMakeComAntennaSwitch`
- `rmMakeComAntennaType`
- `rmMakeComArpaAcqCorre`
- `rmMakeComArpaAcquireTarget`
- `rmMakeComArpaAcquireTargetDisplayId`
- `rmMakeComArpaAlarm`
- `rmMakeComArpaAllClear`
- `rmMakeComArpaAntenna`
- `rmMakeComArpaCancelStatus`
- `rmMakeComArpaCancelTarget`
- `rmMakeComArpaCancelTargetDisplayId`
- `rmMakeComArpaCancelTargetId`
- `rmMakeComArpaCancelTargetNo`
- `rmMakeComArpaCatch`
- `rmMakeComArpaCpaTcpa`
- `rmMakeComArpaDetect`
- `rmMakeComArpaDispMode`
- `rmMakeComArpaEchoLevel`
- `rmMakeComArpaFilterRespons`
- `rmMakeComArpaGateSize`
- `rmMakeComArpaLandSize`
- `rmMakeComArpaLostCount`
- `rmMakeComArpaLostFilterMode`
- `rmMakeComArpaLostFilterRange`
- `rmMakeComArpaLostFilterSpeed`
- `rmMakeComArpaMaxRange`
- `rmMakeComArpaMaxSpeed`
- `rmMakeComArpaPreset`
- `rmMakeComArpaPursuit`
- `rmMakeComArpaReAcquireTarget`
- `rmMakeComArpaSelectTarget`
- `rmMakeComArpaTargetAllClear`
- `rmMakeComArpaTest`
- `rmMakeComArpaTimeVector`
- `rmMakeComArpaVector`
- `rmMakeComArpaWeed`
- `rmMakeComArpaWithoutGyro`
- `rmMakeComAssociationStatus`
- `rmMakeComAtfParameter`
- `rmMakeComAtfSettings`
- `rmMakeComATT`
- `rmMakeComBearingResolutionSelect`
- `rmMakeComBlindSector`
- `rmMakeComBuildUpTime`
- `rmMakeComCancelSelectLostTarget`
- `rmMakeComCancelSelectTarget`
- `rmMakeComCombineData`
- `rmMakeComCombineStatus`
- `rmMakeComConnect`
- `rmMakeComConningPosition`
- `rmMakeComCustomMemory`
- `rmMakeComCustomPicture`
- `rmMakeComCustomPictureAll`
- `rmMakeComDemoEcho`
- `rmMakeComDisplayUnitInformation`
- `rmMakeComDispMode`
- `rmMakeComDrift`
- `rmMakeComEavWithoutGyro`
- `rmMakeComEchoClear`
- `rmMakeComEchoColorType`
- `rmMakeComEquipComp`
- `rmMakeComExtAlarmAck`
- `rmMakeComFarSTC`
- `rmMakeComFreeCommand`
- `rmMakeComGAIN`
- `rmMakeComGC10`
- `rmMakeComGetKindCase`
- `rmMakeComGpsAntennaPosition`
- `rmMakeComGuardFan1`
- `rmMakeComGuardFan2`
- `rmMakeComGuardMode`
- `rmMakeComGuardPolygon`
- `rmMakeComGuardStatus`
- `rmMakeComHeading`
- `rmMakeComHeadingAdjust`
- `rmMakeComHeadingSource`
- `rmMakeComHighSensitivityMode`
- `rmMakeComIceClass`
- `rmMakeComImageCor`
- `rmMakeComImoEchoSwitch`
- `rmMakeComInterswInfo`
- `rmMakeComJummingAble`
- `rmMakeComKeyCommand`
- `rmMakeComLostAck`
- `rmMakeComMagnetron`
- `rmMakeComMagnetronInd`
- `rmMakeComMBSAdjust`
- `rmMakeComMenuSet`
- `rmMakeComMiddleSTC`
- `rmMakeComMonitorSelect`
- `rmMakeComNearSTC`
- `rmMakeComNetworkPriority`
- `rmMakeComNextEcho`
- `rmMakeComNN3Command`
- `rmMakeComObservationMode`
- `rmMakeComOnTime`
- `rmMakeComOperatorFitness`
- `rmMakeComPictureDefault`
- `rmMakeComPM`
- `rmMakeComPmAdjust`
- `rmMakeComPmAdjustEnd`
- `rmMakeComPulseRange`
- `rmMakeComPulseWidth`
- `rmMakeComRadarPosition`
- `rmMakeComRange`
- `rmMakeComRangeSelect`
- `rmMakeComReferencePoint`
- `rmMakeComRfcVersionNo`
- `rmMakeComRingSuppression`
- `rmMakeComRsAllClear`
- `rmMakeComRsPreset`
- `rmMakeComRxEchoSelect`
- `rmMakeComSART`
- `rmMakeComSARTSignal`
- `rmMakeComSelectAlarmOut`
- `rmMakeComSelectAlarmPolarity`
- `rmMakeComSelectAntenna`
- `rmMakeComSelectCrt`
- `rmMakeComSelectQVDisplay`
- `rmMakeComSelectTtmOut`
- `rmMakeComSetKindCase`
- `rmMakeComSetMemory`
- `rmMakeComShipInfo`
- `rmMakeComSpeedSelect`
- `rmMakeComSsdTxChannel`
- `rmMakeComStartSelfMode`
- `rmMakeComStcRange`
- `rmMakeComStoppedTx`
- `rmMakeComSweepPosition`
- `rmMakeComTimingAdjust`
- `rmMakeComTrailClear`
- `rmMakeComTrailMode`
- `rmMakeComTrailProcess`
- `rmMakeComTune`
- `rmMakeComTuneAdjust`
- `rmMakeComTuneIndicator`
- `rmMakeComTxEchoSelect`
- `rmMakeComTxStby`
- `rmMakeComTxTime`
- `rmMakeComVersionNo`
- `rmMakeComVideoLevel`
- `rmMakeComWakeupCount`

### rmConv* (155 functions)

- `rmConv2ndEchoReject`
- `rmConvAccuShip`
- `rmConvACRAIN`
- `rmConvACSEA`
- `rmConvAisAlarm`
- `rmConvAlarm`
- `rmConvAlarmAck`
- `rmConvAliveCheck`
- `rmConvAntennaHeight`
- `rmConvAntennaRevolution`
- `rmConvAntennaSwitch`
- `rmConvAntennaType`
- `rmConvAntNo`
- `rmConvARPA`
- `rmConvArpaAcqCorre`
- `rmConvArpaAcquireTarget`
- `rmConvArpaAcquireTargetDisplayId`
- `rmConvArpaAlarm`
- `rmConvArpaAllClear`
- `rmConvArpaAntenna`
- `rmConvArpaCancelStatus`
- `rmConvArpaCancelTarget`
- `rmConvArpaCancelTargetDisplayId`
- `rmConvArpaCancelTargetId`
- `rmConvArpaCancelTargetNo`
- `rmConvArpaCatch`
- `rmConvArpaCpaTcpa`
- `rmConvArpaDetect`
- `rmConvArpaDispMode`
- `rmConvArpaEchoLevel`
- `rmConvArpaFilterRespons`
- `rmConvArpaGateSize`
- `rmConvArpaLandSize`
- `rmConvArpaLostCount`
- `rmConvArpaLostFilterMode`
- `rmConvArpaLostFilterRange`
- `rmConvArpaLostFilterSpeed`
- `rmConvArpaMaxRange`
- `rmConvArpaMaxSpeed`
- `rmConvArpaPreset`
- `rmConvArpaPursuit`
- `rmConvArpaReAcquireTarget`
- `rmConvArpaSelectTarget`
- `rmConvArpaTargetAllClear`
- `rmConvArpaTimeVector`
- `rmConvArpaVector`
- `rmConvArpaWeed`
- `rmConvArpaWithoutGyro`
- `rmConvAssociationStatus`
- `rmConvAtfParameter`
- `rmConvAtfSettings`
- `rmConvATT`
- `rmConvBearingResolutionSelect`
- `rmConvBlindSector`
- `rmConvBuildUpTime`
- `rmConvCancelLostTarget`
- `rmConvCancelTarget`
- `rmConvCombineData`
- `rmConvCombineStatus`
- `rmConvConnect`
- `rmConvConningPosition`
- `rmConvCustomMemory`
- `rmConvCustomPicture`
- `rmConvCustomPictureAll`
- `rmConvDemoEcho`
- `rmConvDisplayUnitInformation`
- `rmConvDispMode`
- `rmConvDrift`
- `rmConvDummy`
- `rmConvEavWithoutGyro`
- `rmConvEchoClear`
- `rmConvEchoColorType`
- `rmConvEquipComp`
- `rmConvExtAlarmAck`
- `rmConvFarSTC`
- `rmConvFreeCommand`
- `rmConvGAIN`
- `rmConvGC10`
- `rmConvGetRxCommandType`
- `rmConvGpsAntennaPosition`
- `rmConvGuardFan1`
- `rmConvGuardFan2`
- `rmConvGuardMode`
- `rmConvGuardPolygon`
- `rmConvGuardStatus`
- `rmConvHeading`
- `rmConvHeadingAdjust`
- `rmConvHeadingSource`
- `rmConvHighSensitivityMode`
- `rmConvIceClass`
- `rmConvImageCor`
- `rmConvImoEchoSwitch`
- `rmConvInterswInfo`
- `rmConvJummingAble`
- `rmConvKeyCommand`
- `rmConvLostAck`
- `rmConvMagnetron`
- `rmConvMagnetronInd`
- `rmConvMBSAdjust`
- `rmConvMenuSet`
- `rmConvMiddleSTC`
- `rmConvMonitorSelect`
- `rmConvNearSTC`
- `rmConvNetworkPriority`
- `rmConvNextEcho`
- `rmConvNN3Command`
- `rmConvObservationMode`
- `rmConvOnTime`
- `rmConvOperatorFitness`
- `rmConvPictureDefault`
- `rmConvPM`
- `rmConvPmAdjust`
- `rmConvPmAdjustEnd`
- `rmConvPulseRange`
- `rmConvPulseWidth`
- `rmConvRadarPosition`
- `rmConvRange`
- `rmConvRangeSelect`
- `rmConvReferencePoint`
- `rmConvRfcVersionNo`
- `rmConvRingSuppression`
- `rmConvRsAllClear`
- `rmConvRsPreset`
- `rmConvRxEchoSelect`
- `rmConvSART`
- `rmConvSARTSignal`
- `rmConvSelectAlarmOut`
- `rmConvSelectAlarmPolarity`
- `rmConvSelectAntenna`
- `rmConvSelectCrt`
- `rmConvSelectQVDisplay`
- `rmConvSelectTtmOut`
- `rmConvSetMemory`
- `rmConvSetRxCommandType`
- `rmConvShipInfo`
- `rmConvSpeedSelect`
- `rmConvSsdTxChannel`
- `rmConvStartArpaTest`
- `rmConvStartSelfMode`
- `rmConvStcRange`
- `rmConvStoppedTx`
- `rmConvSweepPosition`
- `rmConvTimingAdjust`
- `rmConvTrailClear`
- `rmConvTrailMode`
- `rmConvTrailProcess`
- `rmConvTune`
- `rmConvTuneAdjust`
- `rmConvTuneIndicator`
- `rmConvTxEchoSelect`
- `rmConvTxStby`
- `rmConvTxTime`
- `rmConvVersionNo`
- `rmConvVideoLevel`
- `rmConvWakeupCount`
