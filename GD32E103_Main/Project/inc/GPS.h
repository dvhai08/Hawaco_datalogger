#ifndef __GPS_H__
#define __GPS_H__

#include "DataDefine.h"

#define MAX_GPRMC_BUFFER    80

typedef struct
{	
	PointStruct_t CurrentPosition;		//Toa do
	DateTime_t CurrentDateTime;		//Thoi gian
	uint8_t CurrentSpeed;		//Van toc	
	
    uint16_t Course; //Huong di chuyen
    uint8_t Valid; //Co valid hay khong

    SmallBuffer_t DataBuffer;
	uint16_t TimeOutConnection;
    uint8_t NewGPSData;
} GPS_Manager_t;

typedef struct {
	PointStruct_t CurrentPosition;		//Toa do
	DateTime_t GPSTime;		//Thoi gian
	uint8_t GPSSpeed;		//Van toc GPS
	uint16_t KnotSpeed;		//Van toc tinh theo Knot*100
	Float_t Course; 	//Huong di chuyen	
	uint8_t Valid;
} GPSInforStruct;

void GetDataFromGPSModule(uint8_t gpsdata);
void InitGPS(void);
void GPS_ManagerTick(void);
uint8_t DecodeGPSMessage(void);
void GPS_ProcessNewMessage(void);

uint8_t GPS_IsValid(void);
uint8_t GPS_IsLostData(void);
DateTime_t GPS_GetDateTime(void);
uint8_t GPS_GetSpeed(void);
PointStruct_t GPS_GetPosition(void);
void UpdateTimeFromGPS (void);

#endif // __GPS_H__

