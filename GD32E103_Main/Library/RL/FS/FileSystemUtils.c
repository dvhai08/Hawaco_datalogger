/******************************************************************************
 * @file    	FileSystemUtils.c
 * @author  	Khanhpd
 * @version 	V1.0.0
 * @date    	10/11/2014
 * @brief   	
 ******************************************************************************/


/******************************************************************************
                                   INCLUDES					    			 
 ******************************************************************************/
#include "DataDefine.h"
#include "File_Config.h"
#include "BGT.h"

/******************************************************************************
                                   GLOBAL VARIABLES					    			 
 ******************************************************************************/
extern System_t xSystem;

/******************************************************************************
                                   GLOBAL FUNCTIONS					    			 
 ******************************************************************************/


/******************************************************************************
                                   DATA TYPE DEFINE					    			 
 ******************************************************************************/
#define CMD_COUNT   (sizeof (cmd) / sizeof (cmd[0]))

enum
{
    BACKSPACE = 0x08,
    LF = 0x0A,
    CR = 0x0D,
    CNTLQ = 0x11,
    CNTLS = 0x13,
    ESC = 0x1B,
    DEL = 0x7F
};

/******************************************************************************
                                   PRIVATE VARIABLES					    			 
 ******************************************************************************/
static char cTemp[200];


/******************************************************************************
                                   LOCAL FUNCTIONS					    			 
 ******************************************************************************/
static void dot_format(uint64_t val, char *sp);
static char *get_entry(char *cp, char **pNext);

/*****************************************************************************/

/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
static char *get_entry(char *cp, char **pNext)
{
    char *sp, lfn = 0, sep_ch = ' ';

    if(cp == NULL) /* skip NULL pointers           */
    {
        *pNext = cp;
        return(cp);
    }

    for(; *cp == ' ' || *cp == '\"'; cp++) /* skip blanks and starting  "  */
    {
        if(*cp == '\"')
        {
            sep_ch = '\"';
            lfn = 1;
        }
        *cp = 0;
    }

    for(sp = cp; *sp != CR && *sp != LF && *sp != 0; sp++)
    {
        if(lfn && *sp == '\"') break;
        if(!lfn && *sp == ' ') break;
    }

    for(; *sp == sep_ch || *sp == CR || *sp == LF; sp++)
    {
        *sp = 0;
        if(lfn && *sp == sep_ch)
        {
            sp++;
            break;
        }
    }

    *pNext = (*sp) ? sp : NULL; /* next entry                   */
    return(cp);
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
static void dot_format (U64 val, char *sp) 
{
    if (val >= (U64)1e9) 
    {
        sp += sprintf (sp,"%d.",(U32)(val/(U64)1e9));
        val %= (U64)1e9;
        sp += sprintf (sp,"%03d.",(U32)(val/(U64)1e6));
        val %= (U64)1e6;
        sprintf (sp,"%03d.%03d",(U32)(val/1000),(U32)(val%1000));
        return;
    }
    
    if (val >= (U64)1e6) 
    {
        sp += sprintf (sp,"%d.",(U32)(val/(U64)1e6));
        val %= (U64)1e6;
        sprintf (sp,"%03d.%03d",(U32)(val/1000),(U32)(val%1000));
        return;
    }
    
    if (val >= 1000) 
    {
        sprintf (sp,"%d.%03d",(U32)(val/1000),(U32)(val%1000));
        return;
    }
    
    sprintf (sp,"%d",(U32)(val));
}
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void FS_ListFile(char *par)
{
    U64 fsize;
    U32 files, dirs, i;
    char temp[32], *mask, *next, ch;
    FINFO info;

    mask = get_entry(par, &next);
    if(mask == NULL)
    {
        mask = "*.*";
    }

    DebugPrint("\nFile System Directory...");
    files = 0;
    dirs = 0;
    fsize = 0;
    info.fileID = 0;
    while(ffind(mask, &info) == 0)
    {
        if(info.attrib & ATTR_DIRECTORY)
        {
            i = 0;
            while(strlen((const char *) info.name + i) > 41)
            {
                ch = info.name[i + 41];
                info.name[i + 41] = 0;
                
				sprintf(cTemp,"\n%-41s", &info.name[i]);
				DebugPrint(cTemp);
				
                info.name[i + 41] = ch;
                i += 41;
            }
			sprintf(cTemp,"\n%-41s    <DIR>       ", &info.name[i]);
            DebugPrint(cTemp);
			
            sprintf(cTemp,"  %02d.%02d.%04d  %02d:%02d",
                info.time.day, info.time.mon, info.time.year,
                info.time.hr, info.time.min);
			DebugPrint(cTemp);
			
            dirs++;
        }
        else
        {
            dot_format(info.size, &temp[0]);
            i = 0;
            while(strlen((const char *) info.name + i) > 41)
            {
                ch = info.name[i + 41];
                info.name[i + 41] = 0;
                
				sprintf(cTemp,"\n%-41s", &info.name[i]);
				DebugPrint(cTemp);
				
                info.name[i + 41] = ch;
                i += 41;
            }
            sprintf(cTemp,"\n%-41s %14s ", &info.name[i], temp);
			DebugPrint(cTemp);
			
            sprintf(cTemp,"  %02d.%02d.%04d  %02d:%02d",
                info.time.day, info.time.mon, info.time.year,
                info.time.hr, info.time.min);
			DebugPrint(cTemp);
			
            fsize += info.size;
            files++;
        }
    }
    if(info.fileID == 0)
    {
        DebugPrint("\nNo files...");
    }
    else
    {
        dot_format(fsize, &temp[0]);
        sprintf(cTemp,"\n              %9d File(s)    %21s bytes", files, temp);
		DebugPrint(cTemp);
    }
    dot_format(ffree(""), &temp[0]);
    if(dirs)
    {
        sprintf(cTemp,"\n              %9d Dir(s)     %21s bytes free.\n", dirs, temp);
		DebugPrint(cTemp);
    }
    else
    {		
        sprintf(cTemp,"\n%56s bytes free.\n", temp);
		DebugPrint(cTemp);
    }
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
unsigned char count[10]= {0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x30};	 //some data
void TestFileSystem(void)
{
	FILE *Fptr;

	FS_ListFile(0);
	
	DebugPrint("\rTest File System...");

	Fptr = fopen("TEST1.TXT","w");												   //create a series of files and write data into them
	
	if(Fptr == NULL)
	{
	   DebugPrint("\rCreating file TEST1.txt fail...");
	   return;            
	}

	fwrite(&count,1,10,Fptr);
		
	fclose(Fptr);

	DebugPrint("\rCreating file TEST1.txt successfull...");
    
}
/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
void InitFileSystem(void)
{
    int iTemp;
    
    iTemp = finit(NULL);
    
    if(iTemp == 0)
    {
        DebugPrint("\rKhoi tao the nho : [OK]");        
        
        xSystem.GLStatus.TrangThaiHeThong[SDCARDFAIL] = TRANGTHAI_OK;   
        
//        FS_ListFile(0);
    }
    else
    {
        DebugPrint("\rKhoi tao the nho : [FAIL]");
        
        switch(iTemp)
        {
            case 1:
                DebugPrint(" - Khong co the");
                xSystem.GLStatus.TrangThaiHeThong[SDCARDFAIL] = TRANGTHAI_FAIL;
                break;
            case 2:
                DebugPrint(" - Khong dung dinh dang");
            case 3:
                DebugPrint(" - Khong dung cau hinh");
            case 4:
                DebugPrint(" - FAT journal fail");
                DebugPrint("\rThuc hien format the nho");
                
                iTemp = fformat(" /FAT32");
                if(iTemp == 0)
                {
                    DebugPrint("\rFormat the nho thanh cong");
                    xSystem.GLStatus.TrangThaiHeThong[SDCARDFAIL] = TRANGTHAI_OK;     
                }
                else
                {
                    DebugPrint("\rKhong format duoc the nho, error code : %u",iTemp);
                    xSystem.GLStatus.TrangThaiHeThong[SDCARDFAIL] = TRANGTHAI_FAIL;
                }
                
                break;
        }        
    }
    
    if(xSystem.GLStatus.TrangThaiHeThong[SDCARDFAIL] == TRANGTHAI_OK)
    {
//        BGT_CheckFileSystemTick();
    }
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
U32 fs_get_date (void) 
{
    U32 d,m,y,date;

    /* Adapt the function; Add a system call to read RTC.  */
    d = xSystem.Rtc->GetDateTime().Day;              /* Day:   1 - 31      */
    m = xSystem.Rtc->GetDateTime().Month;             /* Month: 1 - 12      */
    y = xSystem.Rtc->GetDateTime().Year + 2000;           /* Year:  1980 - 2107 */

    date = (y << 16) | (m << 8) | d;

    return (date);
}

/*****************************************************************************/
/**
 * @brief	:  
 * @param	:  
 * @retval	:
 * @author	:	Khanhpd
 * @created	:	10/11/2014
 * @version	:
 * @reviewer:	
 */
U32 fs_get_time (void) 
{
    U32 h,m,s,time;

    /* Modify here; Add a system call to read RTC. */
    h = xSystem.Rtc->GetDateTime().Hour; /* Hours:   0 - 23 */
    m = xSystem.Rtc->GetDateTime().Minute;  /* Minutes: 0 - 59 */
    s = xSystem.Rtc->GetDateTime().Second;  /* Seconds: 0 - 59 */

    time = (h << 16) | (m << 8) | s;
    return (time);
}

/********************************* END OF FILE *******************************/
