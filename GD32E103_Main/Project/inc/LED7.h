#ifndef __UI_H__
#define __UI_H__

#include <stdint.h>
#include "gd32e10x.h"
#include "hardware.h"

#define	DIGIT_ARRAY_SIZE	5
#define	MODE_ARRAY_SIZE	6	/* FM1, FM2, FM3, MIC, LINE IN, IDLE */

typedef enum {
	LOCAL_FM1 = 0,
	LOCAL_FM2,
	LOCAL_FM3,
	LOCAL_MIC,	/* Chay tu MIC */
	LOCAL_LINE,	/* Chay tu duong Line In Mixer */
	LOCAL_IDLE
} LocalMode_e;

typedef struct {
	uint8_t DigitArray[DIGIT_ARRAY_SIZE];
	uint8_t index;
	
	uint8_t ModeArray[MODE_ARRAY_SIZE];
	uint8_t lastModeIndex;
	uint8_t modeIndex;
	uint8_t PressKeyTimout;
} KeySetup_t;

//LED
void Led7_Init(void);
void Led7_Tick(void);
void Led7_ScanSeg(void);

void Led7_DispId(char *Id);
void Led7_DispNum(uint32_t num);
void Led7_DispVolume(uint8_t volume);
void Led7_DispSelftTest(uint8_t num);
void Led7_UpdateTactTime(void);

void Led7_DispSetupMode(uint8_t modeIndex, uint8_t blinkDisp);
void Led7_DispFrequency(uint16_t freq);

//Button
void Button_Init(void);
void Button_Tick(void);

//Volume
void Volume_Tick(void);

//Tacttime
void ManagementTactTimeTick(void);


#endif /* __UI_H__ */
