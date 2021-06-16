#include "version_control.h"
#include <string.h>

uint8_t version_control_compare(char *new_version)
{
    // vnum stores each numeric part of version 
    int i, j, cur_vnum = 0, new_vnum = 0;
	if (!new_version)
	{
		return VERSION_CONTROL_FW_OLDER;
	}
	
    // loop untill both string are processed 
    for (i=0, j=0; (i<strlen(VERSION_CONTROL_FW) || j<strlen(new_version)); i++, j++) 
	{ 
        // storing numeric part of current version in cur_vnum 
        while (i < strlen(VERSION_CONTROL_FW) && VERSION_CONTROL_FW[i] != '.') 
		{
            cur_vnum = cur_vnum * 10 + (VERSION_CONTROL_FW[i] - '0'); 
            i++; 
        }
        //  storing numeric part of ota version in new_vnum
        while (j < strlen(new_version) && new_version[j] != '.') 
		{
            new_vnum = new_vnum * 10 + (new_version[j] - '0'); 
            j++; 
        }
        // if not greater, reset variables and go for next numeric part
        if (cur_vnum > new_vnum)
		{
			return VERSION_CONTROL_FW_OLDER;
		}
        if (cur_vnum < new_vnum) 
		{
			return VERSION_CONTROL_FW_NEWER;
		}
        cur_vnum = new_vnum = 0;
    }
	
    return VERSION_CONTROL_FW_EQUAL;
}


