/*******************************************************************************
' Author: Dave Hein
' Version 0.54
' Copyright (c) 2012
' See end of file for terms of use.
'******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#ifdef LINUX
#include <dirent.h>
#include <sys/stat.h>
#include "conion.h"
#else
#include <conio.h>
#include <direct.h>
#endif
#include "interp.h"
#include "spindebug.h"
#include "eeprom.h"
#include "spinsim.h"

extern int32_t loopcount;
extern int32_t printflag;
extern int32_t baudrate;
extern int32_t eeprom;
extern char *hubram;
extern int32_t printbreak;
extern PasmVarsT PasmVars[8];

void GetDebugString(char *ptr);
int32_t RunProp(int32_t maxloops);

void Help(void)
{
    printf("Debug Commands\n");
    printf("help           - Print command list\n");
    printf("exit           - Exit spinsim\n");
    printf("step           - Run one cycle\n");
    printf("stepx          - Run next executed instruction\n");
    printf("run            - Run continuously\n");
    printf("verbose #      - Set verbosity level\n");
    printf("reboot         - Reboot the Prop\n");
    printf("setbr cog addr - Set breakpoint for cog to addr\n");
    printf("state cog      - Dump cog state\n");
}

char *SkipChar(char *str, int value)
{
    while (*str)
    {
        if (*str != value) break;
        str++;
    }
    return str;
}

void DumpState(PasmVarsT *pasmvars)
{
    printf("cflag = %d, zflag = %d, waitflag = %d\n",
	pasmvars->cflag, pasmvars->zflag, pasmvars->waitflag);
    printf("ptra = %5.5x, ptrb = %5.5x, ptrx = %2.2x, ptry = %2.2x, inda = %3.3x, indb = %3.3x\n",
	pasmvars->ptra, pasmvars->ptra, pasmvars->ptrx,
	pasmvars->ptry, pasmvars->inda, pasmvars->indb);
    printf("pc1 = %8.8x, instruct1 = %8.8x\n", pasmvars->pc1, pasmvars->instruct1);
    printf("pc2 = %8.8x, instruct2 = %8.8x\n", pasmvars->pc2, pasmvars->instruct2);
    printf("pc3 = %8.8x, instruct3 = %8.8x\n", pasmvars->pc3, pasmvars->instruct3);
    printf("pc4 = %8.8x, instruct4 = %8.8x\n", pasmvars->pc4, pasmvars->instruct4);
}

void Debug(void)
{
    int runflag = 1;
    char buffer[200];
    char lastcmd[200];
    int maxloops;
    int stepflag = 0;
    int saveprintflag = 0;

    strcpy(lastcmd, "help");
    while (runflag)
    {
        while (1)
        {
            printf("\nDEBUG> ");
	    fflush(stdout);
            GetDebugString(buffer);
	    if (buffer[0] == 0) strcpy(buffer, lastcmd);
	    strcpy(lastcmd, buffer);
            if (!strcmp(buffer, "exit"))
            {
                runflag = 0;
                break;
            }
            else if (!strcmp(buffer, "step"))
            {
		stepflag = 1;
		saveprintflag = printflag;
		printflag = 0xffffffff;
                LONG(SYS_DEBUG) = printflag;
                maxloops = loopcount + 1;
                break;
            }
            else if (!strcmp(buffer, "stepx"))
            {
		stepflag = 1;
		saveprintflag = printflag;
		printflag = 0x22222222;
                LONG(SYS_DEBUG) = printflag;
		printbreak = 1;
                maxloops = -1;
                break;
            }
            else if (!strncmp(buffer, "verbose", 7))
            {
		char *ptr = SkipChar(buffer+7, ' ');
		if (*ptr == 0)
		    printflag = 0xffffffff;
		else
		{
		    sscanf(ptr, "%x", &printflag);
		    if (ptr[1] == 0)
			printflag *= 0x11111111;
		}
                LONG(SYS_DEBUG) = printflag;
            }
            else if (!strcmp(buffer, "help"))
            {
                Help();
            }
            else if (!strcmp(buffer, "run"))
            {
                maxloops = -1;
                break;
            }
            else if (!strncmp(buffer, "setbr ", 6))
            {
                int cognum, address;
                sscanf(buffer+6, "%x %x", &cognum, &address);
                PasmVars[cognum&7].breakpnt = address;
                LONG(SYS_DEBUG) = printflag;
            }
            else if (!strcmp(buffer, "reboot"))
            {
                RebootProp();
            }
            else if (!strncmp(buffer, "state ", 6))
            {
		int cognum = buffer[6] & 7;
		DumpState(&PasmVars[cognum]);
            }
	    else
		printf("?\n");
        }
        if (runflag) RunProp(maxloops);
	if (stepflag)
	{
	    stepflag = 0;
	    printbreak = 0;
	    printflag = saveprintflag;
	    LONG(SYS_DEBUG) = printflag;
	}
    }
}

void GetDebugString(char *ptr)
{
    int value;

    while (1)
    {
#ifndef STD_CONSOLE_INPUT
        while (!kbhit());
#endif
        value = getch();
        putchx(value);
        if (value == 13 || value == 10)
        {
            *ptr = 0;
            return;
        }
        *ptr++ = value;
    }
}

int32_t RunProp(int32_t maxloops)
{
    int32_t runflag = 1;

    while (runflag && (maxloops < 0 || loopcount < maxloops))
    {
        runflag = step_chip();
        CheckCommand();
        if (baudrate)
        {
            CheckSerialOut();
            if (CheckSerialIn()) return 1;
        }
        if (eeprom)
            CheckEEProm();
    }
    return 0;
}
/*
+------------------------------------------------------------------------------------------------------------------------------+
|                                                   TERMS OF USE: MIT License                                                  |
+------------------------------------------------------------------------------------------------------------------------------+
|Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation    |
|files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,    |
|modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software|
|is furnished to do so, subject to the following conditions:                                                                   |
|                                                                                                                              |
|The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.|
|                                                                                                                              |
|THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE          |
|WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR         |
|COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,   |
|ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                         |
+------------------------------------------------------------------------------------------------------------------------------+
*/
