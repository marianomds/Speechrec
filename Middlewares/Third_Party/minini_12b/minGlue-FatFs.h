/*  Glue functions for the minIni library, based on the FatFs and Petit-FatFs
 *  libraries, see http://elm-chan.org/fsw/ff/00index_e.html
 *
 *  By CompuPhase, 2008-2012
 *  This "glue file" is in the public domain. It is distributed without
 *  warranties or conditions of any kind, either express or implied.
 *
 *  (The FatFs and Petit-FatFs libraries are copyright by ChaN and licensed at
 *  its own terms.)
 */
 
#ifndef MIN_GLUE_FATFS_H
#define MIN_GLUE_FATFS_H

#include "arm_math.h"

#define INI_BUFFERSIZE  256       /* The maximum line length that is supported, as well as the maximum path
																		 length for temporary file (if write access is enabled). The default value is
																		 512.*/

/* You must set _USE_STRFUNC to 1 or 2 in the include file ff.h (or tff.h)
 * to enable the "string functions" fgets() and fputs().
 */
#include "ff.h"                   /* include tff.h for Tiny-FatFs */

#define INI_FILETYPE    FIL				/* The type for a variable that represents a file. This is a required setting. */

#define ini_openread(filename,file)   (f_open((file), (filename), FA_READ  + FA_OPEN_EXISTING) == FR_OK)
#define ini_openwrite(filename,file)  (f_open((file), (filename), FA_WRITE + FA_CREATE_ALWAYS) == FR_OK)
#define ini_close(file)               (f_close(file) == FR_OK)
#define ini_read(buffer,size,file)    f_gets((buffer), (size),(file))
#define ini_write(buffer,file)        f_puts((buffer), (file))
#define ini_remove(filename)          (f_unlink(filename) == FR_OK)

#define INI_FILEPOS                   DWORD		/* The type for a position in a file. This is a required setting if writing support
																								 is enabled.*/
#define ini_tell(file,pos)            (*(pos) = f_tell((file)))
#define ini_seek(file,pos)            (f_lseek((file), *(pos)) == FR_OK)


#define INI_REAL											float32_t	/* The type for a variable that represents a rational number. If left undefined,
																									 rational number support is disabled*/

#define ini_ftoa(string,value)				sprintf((string),"%f",(value))
#define ini_atof(string)							(INI_REAL)strtod((string),NULL)


#define INI_ANSIONLY									0			/* If this macro is defined, ini files are forced to be written with 8-bit characters
																							 (ascii or ansi character sets), regardless of whether the remainder of the
																		 					 application is written as Unicode.*/

#define INI_LINETERM									0			/* This macro should be set to the line termination character (or characters).
																							 If left undefined, the default is a line-feed character. Note that the standard
																							 file I/O library may translate a line-feed character to a carriage-return/line-
																							 feed pair (this depends on the file I/O library).*/

#define INI_NOBROWSE									0			/* Excludes the function ini browse from the minIni library. */
																	
#define INI_READONLY									1			/* If this macro is defined, write access is disabled (and the code for writing
																							 ini files is stripped from the minIni library).*/

#define NDEBUG												1			/* If defined, the assert macro in the minIni source code is disabled. Typi-
																							 cally developers build with assertions enabled during development and dis-
																							 able them for a release version. If your platform lacks an assert macro,
																							 you may want to define the NDEBUG macro in minGlue.h.*/
																		 
#define PORTABLE_STRNICMP							0			/* When this macro is defined, minIni uses an internal, portable strnicmp
																							 function. This is required for platforms that lack this function —note that
																							 minIni already handles the common case where this function goes under
																							 the name strncasecmp.*/

static int ini_rename(TCHAR *source, TCHAR *dest)
{
  /* Function f_rename() does not allow drive letters in the destination file */
  char *drive = strchr(dest, ':');
  drive = (drive == NULL) ? dest : drive + 1;
  return (f_rename(source, drive) == FR_OK);
}

#endif /* MIN_GLUE_FATFS_H */
