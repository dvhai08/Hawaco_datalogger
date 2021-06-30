#ifndef VERSION_CONTROL_H
#define VERSION_CONTROL_H

#define VERSION_CONTROL_FW				"0.0.2"
#define VERSION_CONTROL_HW				"0.0.2"
#ifdef DTG01
#define VERSION_CONTROL_DEVICE			"DTG1"
#else
#define VERSION_CONTROL_DEVICE			"DTG2"
#endif
#define VERSION_CONTROL_BUILD_DATE		__DATE__
#define VERSION_CONTROL_BUILD_TIME		__TIME__

// VERSION_CONTROL_RESULT
#define VERSION_CONTROL_FW_OLDER		0
#define VERSION_CONTROL_FW_EQUAL		1
#define VERSION_CONTROL_FW_NEWER		2

#include <stdint.h>


/**
 * @brief		Comapare version control
 * @retval		version status @ref VERSION_CONTROL_RESULT
 */
uint8_t version_control_compare(char *new_version);


#endif /* VERSION_CONTROL_H */
