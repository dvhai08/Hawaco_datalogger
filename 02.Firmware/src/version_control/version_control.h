#ifndef VERSION_CONTROL_H
#define VERSION_CONTROL_H

#ifdef DTG01
#define VERSION_CONTROL_DEVICE			"DTG1"
#define VERSION_CONTROL_FW				"0.0.8"
#define VERSION_CONTROL_HW				"0.0.1"
#endif
#ifdef DTG02
#define VERSION_CONTROL_DEVICE			"DTG2"
#define VERSION_CONTROL_FW				"0.2.6"
#define VERSION_CONTROL_HW				"0.0.1"
#endif

#ifdef DTG02V2
#define VERSION_CONTROL_DEVICE			"DTG2"
#define VERSION_CONTROL_FW				"0.3.6"
#define VERSION_CONTROL_HW				"0.0.2"
#endif

#ifdef DTG02V3
#define VERSION_CONTROL_DEVICE			"DTG2"
#define VERSION_CONTROL_FW				"0.0.1"
#define VERSION_CONTROL_HW				"0.0.3"
#endif

#define VERSION_CONTROL_BUILD_DATE		__DATE__
#define VERSION_CONTROL_BUILD_TIME		__TIME__

// VERSION_CONTROL_RESULT
#define VERSION_CONTROL_FW_OLDER		0
#define VERSION_CONTROL_FW_EQUAL		1
#define VERSION_CONTROL_FW_NEWER		2

#include <stdint.h>


/*!
 * @brief		Comapare version control
 * @retval		version status @ref VERSION_CONTROL_RESULT
 */
uint8_t version_control_compare(char *new_version);


#endif /* VERSION_CONTROL_H */
