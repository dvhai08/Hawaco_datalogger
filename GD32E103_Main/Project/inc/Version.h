//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------

#ifndef __VERSION_H__
#define __VERSION_H__

#define __RELEASE_DATE_KEIL__	__DATE__
#define __RELEASE_TIME_KEIL__	__TIME__


// <h> Phien ban firmware
	//   <o> Ma phien ban <1-99>
	#define FIRMWARE_VERSION_CODE	1
	
	//   <s> Release date
	#define __RELEASE_DATE__        "2016/08/30"
		
	//	<o> Phien ban Protocol	<1-99>
	#define PROTOCOL_VERSION_CODE	0

	#define FIRMWARE_VERSION_HEADER	"HWC_GW."
// </h>

// <h> Phien ban hardware
	//	<o> Ma phien ban <1-99>
	#define HARDWARE_VERSION_CODE	1	
	
	//	<o> Loai thiet bi <1-99>
	#define DEVICE_TYPE		4		
// </h>

//   <e> Test code
//     	<i>  Tinh nang test code, ma test se duoc add vao DeviceID de phuc vu cho viec test cac phien ban khac nhau
	#define __TEST_ENABLE__ 0	
	
	#if (__TEST_ENABLE__ == 1)
		//   <s.8> Ma test
		#define __TEST_CODE__ "020316.01"
		
		#warning CHU Y DANG BUILD PHIEN BAN DUNG DE TEST
	#endif
// 	</e>



#endif // __VERSION_H__

