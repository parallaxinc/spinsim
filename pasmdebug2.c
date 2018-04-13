/*******************************************************************************
' Author: Dave Hein
' Version 0.21
' Copyright (c) 2010, 2011
' See end of file for terms of use.
'******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "interp.h"
#include "spinsim.h"

#define ADDR_MASK 0xfffff

extern char *hubram;
extern int32_t memsize;
extern int32_t loopcount;
extern int32_t cycleaccurate;
extern int32_t pin_val;

extern FILE *tracefile;

void Disassemble2(int instruct, int pc, char *debugstr, int *errflag);

void StartPasmCog2(PasmVarsT *pasmvars, int32_t par, int32_t addr, int32_t cogid, int32_t hubexec)
{
    int32_t i;

    //printf("\nStartPasmCog2: %8.8x, %8.8x, %d, %d\n", par, addr, cogid, hubexec);
    par &= ADDR_MASK;
    addr &= ADDR_MASK;
    pasmvars->waitflag = 0;
    pasmvars->cflag = 0;
    pasmvars->zflag = 0;
    pasmvars->cogid = cogid;
    pasmvars->state = 5;
    pasmvars->ptra = par;
    pasmvars->ptrb = addr;
    pasmvars->ptrx = 0;
    pasmvars->ptry = 0;
    pasmvars->inda = 0x1f2;
    pasmvars->indatop = 0;
    pasmvars->indabot = 0;
    pasmvars->indb = 0x1f3;
    pasmvars->indbtop = 0;
    pasmvars->indbbot = 0;
    pasmvars->repcnt = 0;
    pasmvars->repbot = 0;
    pasmvars->reptop = 0;
    pasmvars->repforever = 0;
    pasmvars->instruct1 = 0;
    pasmvars->instruct2 = 0;
    pasmvars->instruct3 = 0;
    pasmvars->instruct4 = 0;
    pasmvars->pc1 = INVALIDATE_INSTR;
    pasmvars->pc2 = INVALIDATE_INSTR;
    pasmvars->pc3 = INVALIDATE_INSTR;
    pasmvars->pc4 = INVALIDATE_INSTR;
    pasmvars->retptr = 0;
    pasmvars->acca = 0;
    pasmvars->accb = 0;
    pasmvars->cordic_count = 0;
    pasmvars->cordic_depth = 0;
    pasmvars->qxposted = 0;
    pasmvars->qyposted = 0;
    pasmvars->breakpnt = -1;
    pasmvars->augsflag = 0;
    pasmvars->augsvalue = 0;
    pasmvars->augdflag = 0;
    pasmvars->augdvalue = 0;
    pasmvars->altsflag = 0;
    pasmvars->altsvalue = 0;
    pasmvars->altdflag = 0;
    pasmvars->altdvalue = 0;
    pasmvars->altrflag = 0;
    pasmvars->altrvalue = 0;
    pasmvars->altiflag = 0;
    pasmvars->altivalue = 0;
    pasmvars->prefetch = 0;
    pasmvars->serina.mode = 0;
    pasmvars->serinb.mode = 0;
    pasmvars->serouta.mode = 0;
    pasmvars->seroutb.mode = 0;
    pasmvars->phase = 0;
    pasmvars->rwrep = 0;
    pasmvars->str_fifo_work_flag = 0;
    pasmvars->str_fifo_addr0 = 0;
    pasmvars->str_fifo_addr1 = 0;
    pasmvars->str_fifo_head_addr = 0;
    pasmvars->str_fifo_tail_addr = 0;
    pasmvars->str_fifo_rindex = 0;
    pasmvars->str_fifo_windex = 0;
    pasmvars->str_fifo_mode = 0;
    pasmvars->str_fifo_buffer[0] = 0;
    pasmvars->cntreg1 = 0;
    pasmvars->cntreg2 = 0;
    pasmvars->cntreg3 = 0;
    pasmvars->intflags = 0;
    pasmvars->intstate = 0;
    pasmvars->qreg = 0;
    pasmvars->memflag = 0;
    pasmvars->intenable1 = 0;
    pasmvars->intenable2 = 0;
    pasmvars->intenable3 = 0;
    pasmvars->pinpatmode = 0;
    pasmvars->pinpatmask = 0;
    pasmvars->pinpattern = 0;
    pasmvars->pinedge = 0;
    pasmvars->lockedge = 0;
    pasmvars->rdl_mask = 0;
    pasmvars->wrl_mask = 0;

    if (!hubexec)
    {
        for (i = 0; i < 0x1f4; i++)
        {
            pasmvars->mem[i] = LONG(addr);
            addr += 4;
        }
        pasmvars->pc = 0;
    }
    else
        pasmvars->pc = addr;
    for (i = 0x1f4; i < 512; i++) pasmvars->mem[i] = 0;
}

void DebugPasmInstruction2(PasmVarsT *pasmvars)
{
    int32_t i;
    int32_t cflag = pasmvars->cflag;
    int32_t zflag = pasmvars->zflag;
    int32_t instruct, pc, cond, xflag;
    char opstr[20];
    char *xstr[12] = {" ", "X", "I", "H", "C", "P", "W", "F", "w", "E", "?", "S"};

    // Fetch the instruction
    pc = pasmvars->pc2;
    instruct = pasmvars->instruct2;
    cond = (instruct >> 28) & 15;
    if (cond == 0 && instruct != 0) cond = 15;
    xflag = ((cond >> ((cflag << 1) | zflag)) & 1);
    xflag ^= 1;

    // Check if the instruction is invalidated in the pipeline
    if (pc & INVALIDATE_INSTR)
    {
	pc &= ~INVALIDATE_INSTR;
	xflag = 2;
    }

    if (pasmvars->waitflag && pasmvars->waitmode == WAIT_CACHE) xflag = 4;
    else if (xflag == 4) xflag = 5;

    if (pasmvars->waitflag)
    {
	if (pasmvars->waitmode == WAIT_CACHE)
	    xflag = 4;
	else if (pasmvars->waitmode == WAIT_CNT)
	    xflag = 6;
	else if (pasmvars->waitmode == WAIT_PIN)
	    xflag = 5;
	else if (pasmvars->waitmode == WAIT_HUB)
	    xflag = 3;
	else if (pasmvars->waitmode == WAIT_FLAG)
	    xflag = 8;
	else if (pasmvars->waitmode == WAIT_CORDIC)
	    xflag = 9;
	else
	    xflag = 10;
    }
    else if (pasmvars->phase == 0)
        xflag = 7;
    else if (pasmvars->skip_mask & 1)
        xflag = 11;

    i = strlen(opstr);
    while (i < 7) opstr[i++] = ' ';
    opstr[i] = 0;

    // Check for NOP
    if (!instruct)
    {
	cond = 15;
	if (xflag == 1) xflag = 0;
	strcpy(opstr, "nop    ");
    }

    if (pasmvars->printflag - 2 < xflag && xflag != 0)
    {
	pasmvars->printflag = 0;
	return;
    }

    {
        int errflag;
        char debugstr[100];

        Disassemble2(instruct, pc, debugstr, &errflag);
        fprintf(tracefile, "Cog %2d: %8.8x ", pasmvars->cogid, loopcount);
        fprintf(tracefile, "%4.4x %8.8x %s %s", pc,
            instruct, xstr[xflag], debugstr);
    }
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
