#ifndef		__DEBUG_H__
#define	__DEBUG_H__

#define		DEBUG_ENABLE		1		//Vao che do debug
#define		DEBUG_DEVICEINFO	2
#define		DEBUG_TESTFLASH		3
#define		DEBUG_TESTGPS			5
#define		DEBUG_TESTTIME		6
#define		DEBUG_TESTBLINK		7
#define		DEBUG_TESTLEDLIGHT		8
#define		DEBUG_TESTDUTY		9
#define		DEBUG_OPERATETIME		10
#define		DEBUG_MOVEDISTANCE	11
#define		DEBUG_TESTCLEAR		12
#define		DEBUG_TESTRESET		14
#define		DEBUG_TESTGSM			15
#define		DEBUG_TESTACQUY		18
#define		DEBUG_TESTSERVER	20
#define		DEBUG_TESTCURRENT	21
#define		DEBUG_TESTALARM	22

#define		DEBUG_TESTCAUHINH		23
#define		DEBUG_TESTOUTSIDEWATCHDOG	26
#define		DEBUG_TESTOUTADC	27

#define		DEBUG_STATEOFFTIME		30
#define		DEBUG_TESTMODE_ON	31	//Che do test cong debug ON
#define		DEBUG_TESTMODE_OFF	32	//Che do test cong debug OFF

#define		DEBUG_GPSON		52		//Bat che do test GPS
#define		DEBUG_GPSOFF		53		//Tat che do debug GPS
#define		DEBUG_GPSTOGGLE	54	//Dao che do debug GPS
#define		DEBUG_GPSPOSITION	55	//Xem tọa độ GPS

#define		DEBUG_GSMON		56
#define		DEBUG_GSMOFF		57
#define		DEBUG_GSMSLEEP		58
#define		DEBUG_FLASH		59	
#define		DEBUG_TESTSLEEPMODE	60

#define		DEBUG_CLEAROPTIME		100
#define		DEBUG_CLOSETCP	102
#define		DEBUG_UDFWFTP	103

#define		TESTMODE_MODULE_IMEI 201
#define		TESTMODE_SIM_IMEI    202
#define		TESTMODE_STRENGTH    203
#define		TESTMODE_VSOLAR      204
#define		TESTMODE_VACQUY      205
#define		TESTMODE_ILOAD       206
#define		TESTMODE_VCELL       207
#define		TESTMODE_GSM         208
#define		TESTMODE_FLASH       209
#define		TESTMODE_GPS 				 210

void Debug_Tick(void);

#endif	/* __DEBUG_H__ */
