#ifndef		__DOWNLOADFILE_H__
#define		__DOWNLOADFILE_H__

#include "stdint.h"

#define	NODATA	-2
#define	CHECKSUMFAIL	-1
#define	WRITEFAIL	-3
#define	OKDATA	1

void RemoteUpdate_Initialize(void);
int32_t GetDataFile(uint8_t* Buffer);
uint8_t	  ProcessDownloadData(uint8_t* Buffer, uint8_t* FileName ,uint8_t Target, uint16_t	PacketID, uint16_t	BeginAddress, uint32_t	DataLength);
void InitRemoteUpdate(void);
void Update_DanhDauViecDangThucThi(uint8_t LoaiUpdate);
void Update_LuuSoPacketDaNhan(uint16_t Packeted);
void Update_WriteFirmwareLength(uint32_t Length);
uint16_t Update_DocSoPacketDaNhan(void);
uint8_t CheckFirmwareData(uint32_t	Total, uint32_t	BeginAddr, uint32_t	Length);
uint32_t ReadFirmwareSize(void);

#endif	/* __DOWNLOADFILE_H__ */
