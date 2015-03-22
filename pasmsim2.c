/*******************************************************************************
' Author: Dave Hein
' Version 0.21
' Copyright (c) 2010, 2011
' See end of file for terms of use.
'******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "interp.h"
#include "spinsim.h"

#define IGNORE_WZ_WC
#define PRINT_RAM_ACCESS

#define AUX_SIZE 256
#define AUX_MASK (AUX_SIZE - 1)

#define REG_INDA 0x1f2
#define REG_INDB 0x1f3
#define REG_PINA 0x1f4
#define REG_PINB 0x1f5
#define REG_PINC 0x1f6
#define REG_PIND 0x1f7
#define REG_OUTA 0x1f8
#define REG_OUTB 0x1f9
#define REG_OUTC 0x1fa
#define REG_OUTD 0x1fb
#define REG_DIRA 0x1fc
#define REG_DIRB 0x1fd
#define REG_DIRC 0x1fe
#define REG_DIRD 0x1ff

extern char *hubram;
extern int32_t memsize;
extern char lockstate[8];
extern char lockalloc[8];
extern PasmVarsT PasmVars[8];
extern int32_t pasmspin;
extern int32_t cycleaccurate;
extern int32_t loopcount;
extern int32_t proptwo;
extern int32_t pin_val;

extern FILE *tracefile;

char *GetOpname2(unsigned int, int *);

void NotImplemented(int instruction)
{
    int dummy;
    char *opname = GetOpname2(instruction, &dummy);
    printf("\n%s not implemented - %8.8x\n", opname, instruction);
    spinsim_exit(1);
}

static int32_t parity(int32_t val)
{
    val ^= val >> 16;
    val ^= val >> 8;
    val ^= val >> 4;
    val ^= val >> 2;
    val ^= val >> 1;
    return val & 1;
}

static int32_t _abs(int32_t val)
{
    return val < 0 ? -val : val;
}

static int32_t seuss(int32_t value, int32_t forward)
{
    uint32_t a, c, x, y;

    x = value;
    if (!x) x = 1;
    y = 32;
    a = 0x17;
    if (!forward) a = (a >> 1) | (a << 31);
    while (y--)
    {
        c = x & a;
        while (c & 0xfffffffe) c = (c >> 1) ^ (c & 1);
        if (forward)
            x = (x >> 1) | (c << 31);
        else
            x = (x << 1) | c;
    }
    return x;
}

uint32_t sqrt32(uint32_t y)
{
    uint32_t x, t1;

    x = 0;
    t1 = 1 << 30;
    while (t1)
    {
        x |= t1;
        if (x <= y)
        {
            y -= x;
            x += t1;
        }
        else
            x -= t1;
        x >>= 1;
        t1 >>= 2;
    }
    return x;
}

uint32_t sqrt64(uint64_t y)
{
    uint64_t x, t1;

    x = 0;
    t1 = 1;
    t1 <<= 60;
    while (t1)
    {
        x |= t1;
        if (x <= y)
        {
            y -= x;
            x += t1;
        }
        else
            x -= t1;
        x >>= 1;
        t1 >>= 2;
    }
    return (uint32_t)x;
}

int32_t CheckWaitPin(PasmVarsT *pasmvars, int32_t instruct, int32_t value1, int32_t value2)
{
    int32_t match, pin_values;
    int32_t portnum = (instruct >> 24) & 3;

    if (portnum == 0)
	pin_values = pin_val;
    else
	pin_values = 0xffffffff;

    if (instruct & 0x04000000)
	match = ((pin_values & value2) != value1); // waitpne
    else
	match = ((pin_values & value2) == value1); // waitpne

    return match;
}

int wait1[128] = {
    7, 3, 7, 3, 7, 3, 2, 2,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 2, 2,
    0, 0, 0, 0, 0, 2, 0, 0,    0, 0, 0, 0, 0, 0, 0, 9,
    0, 0, 2, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,16,16,16,16,    1,10, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0,13, 0};

int wait2[0x120] = {
    6, 0, 6, 0, 0, 0, 8, 8,    8, 8, 0, 0, 0, 0, 0, 0,
   12,12,12,12,12, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 2,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 5, 5, 1, 4, 1, 1,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,11,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   14,14,14,14, 0, 0, 0, 0,
   15,15,15,15, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0};

static int32_t CheckWaitFlag2(PasmVarsT *pasmvars, int instruct, int value1, int value2)
{
    int32_t hubcycles;
    int32_t waitmode = 0;
    int32_t srcaddr = instruct & 511;
    int32_t waitflag = pasmvars->waitflag;
    int32_t opcode = (instruct >> (32 - 7)) & 127;
    int32_t hubop = 0;

    if (waitflag)
    {
	waitflag--;
        if ((instruct & 0xf8000000) == 0xc8000000)
	{
	    if (CheckWaitPin(pasmvars, instruct, value1, value2))
		waitflag = 0;
	}
	pasmvars->waitflag = waitflag;
	if (waitflag == 0) pasmvars->waitmode = 0;
	return waitflag;
    }

    if (opcode != 127)
	waitflag = wait1[opcode];
    else if (srcaddr < 0x120)
	waitflag = wait2[srcaddr];
    else
	waitflag = 0;

    // if (waitflag) printf("CheckWaitFlag2: %8.8x %d\n", instruct, waitflag);

    hubcycles = (pasmvars->cogid - loopcount) & 7;

    switch (waitflag)
    {
	case 0: // 1
	break;

	case 1: // 1 - 8
	hubop = 1;
	waitflag = hubcycles;
	waitmode = WAIT_HUB;
	break;

	case 2: // 2
	waitflag = 1;
	waitmode = WAIT_MULT;
	break;

	case 3: // 1 or 3 - 10 rdxxxxc
//printf("\nvalue2 = %8.8x, dcachehubaddr = %8.8x\n", value2&0xffffffe0, pasmvars->dcachehubaddr);
	if ((value2 & 0xfffffe0) == pasmvars->dcachehubaddr)
	    waitflag = 0;
	else
	{
	    hubop = 1;
	    waitmode = WAIT_HUB;
	    waitflag = hubcycles + 2;
	}
	break;

	case 4: // 1 or 1 - 8
	if ((value2 & 0xfffffe0) == pasmvars->dcachehubaddr)
	    waitflag = 0;
	else
	{
	    hubop = 1;
	    waitmode = WAIT_HUB;
	    waitflag = hubcycles;
	}
	break;

	case 5: // 1 - 9 for lockset and lockclr
	hubop = 1;
	waitmode = WAIT_HUB;
	waitflag = hubcycles;
	if (instruct & 0x00800000)
	    waitflag++;
	break;

	case 6: // 2 - 9
	hubop = 1;
	waitmode = WAIT_HUB;
	waitflag = hubcycles + 1;
	break;

	case 7: // 3 - 10 rdxxxx
	hubop = 1;
	waitmode = WAIT_HUB;
	waitflag = hubcycles + 2;
	break;

	case 8: // mac
	waitflag = 0;
	break;

	case 9: // 1 - 9 for cognew or N for waitcnt
	if (instruct & 0x01000000)
	{
	    waitmode = WAIT_CNT;
	    waitflag = value1 - GetCnt();
	}
	else
	{
	    hubop = 1;
	    waitmode = WAIT_HUB;
	    waitflag = hubcycles;
	    if (instruct & 0x00800000)
		waitflag++;
	}
	break;

	case 10: // 1 - 8 for wrlong or 1 for frac
	if (instruct & 0x01000000)
	    waitflag = 0;
	else
	{
	    hubop = 1;
	    waitmode = WAIT_HUB;
	    waitflag = hubcycles;
	}
	break;

	case 11: // N for wait
	if (!value1)
	    waitflag = 0;
	else
	{
	    waitmode = WAIT_CNT;
	    waitflag = value1 - 1;
	}
	break;

	case 12: // mulcount
	waitmode = WAIT_MULT;
	waitflag = pasmvars->mulcount;
	break;

	case 13: // 1 - 8 for callax, callbx
	if ((instruct & 0x01800000) == 0x01000000)
	{
	    hubop = 1;
	    waitmode = WAIT_HUB;
	    waitflag = hubcycles;
	}
	else
	    waitflag = 0;
	break;

	case 14: // 1 - 8 for callax, callbx
	hubop = 1;
	waitmode = WAIT_HUB;
	waitflag = hubcycles;
	break;

	case 15: // 2 - 9 for retax, retbx
	hubop = 1;
	waitmode = WAIT_HUB;
	waitflag = hubcycles + 1;
	break;

	case 16: // N for waitpxx
	waitmode = WAIT_PIN;
	waitflag = pasmvars->lastd - GetCnt();
	break;
    }

    pasmvars->waitflag = waitflag;
    pasmvars->waitmode = waitmode;

    return hubop;
}

// Compute the hub RAM address from the pointer instruction - SUPIIIIII
int32_t GetPointer(PasmVarsT *pasmvars, int32_t ptrinst, int32_t size)
{
    int32_t address = 0; // Set to zero to avoid compiler warning
    int32_t offset = (ptrinst << 26) >> (26 - size);

    switch ((ptrinst >> 6) & 7)
    {
        case 0: // ptra[offset]
        address = (pasmvars->ptra + offset) & 0x3ffff;
        break;

        case 1: // ptra
        address = pasmvars->ptra;
        break;

        case 2: // ptra[++offset]
        address = (pasmvars->ptra + offset) & 0x3ffff;
        //pasmvars->ptra = address;
        pasmvars->ptra0 = address;
        break;

        case 3: // ptra[offset++]
        address = pasmvars->ptra;
        //pasmvars->ptra = (pasmvars->ptra + offset) & 0x3ffff;
        pasmvars->ptra0 = (pasmvars->ptra + offset) & 0x3ffff;
        break;

        case 4: // ptrb[offset]
        address = (pasmvars->ptrb + offset) & 0x3ffff;
        break;

        case 5: // ptrb
        address = pasmvars->ptrb;
        break;

        case 6: // ptrb[++offset]
        address = (pasmvars->ptrb + offset) & 0x3ffff;
        //pasmvars->ptrb = address;
        pasmvars->ptrb0 = address;
        break;

        case 7: // ptrb[offset++]
        address = pasmvars->ptrb;
        //pasmvars->ptrb = (pasmvars->ptrb + offset) & 0x3ffff;
        pasmvars->ptrb0 = (pasmvars->ptrb + offset) & 0x3ffff;
        break;
    }

    return address;
}

// Compute the aux RAM pointer from the pointer instruction - 1SUPIIIII
int32_t GetAuxPointer(PasmVarsT *pasmvars, int32_t ptrinst)
{
    int32_t address = 0; // Set to zero to avoid compiler warning
    int32_t offset = (ptrinst << 27) >> 27;

    switch ((ptrinst >> 5) & 7)
    {
        case 0: // ptrx[offset]
        address = (pasmvars->ptrx + offset) & 255;
        break;

        case 1: // ptrx
        address = pasmvars->ptrx;
        break;

        case 2: // ptrx[++offset]
        address = (pasmvars->ptrx + offset) & 255;
        pasmvars->ptrx = address;
        break;

        case 3: // ptrx[offset++]
        address = pasmvars->ptrx;
        pasmvars->ptrx = (pasmvars->ptrx + offset) & 255;
        break;

        case 4: // ptry[offset]
        address = (pasmvars->ptry + offset) & 255;
        break;

        case 5: // ptry
        address = pasmvars->ptry;
        break;

        case 6: // ptry[++offset]
        address = (pasmvars->ptry + offset) & 255;
        pasmvars->ptry = address;
        break;

        case 7: // ptry[offset++]
        address = pasmvars->ptry;
        pasmvars->ptry = (pasmvars->ptry + offset) & 255;
        break;
    }

    return address;
}

void UpdatePins2(void)
{
    int32_t i;
    int32_t mask1;
    int32_t val = 0;
    int32_t mask = 0;

    for (i = 0; i < 8; i++)
    {
	if (PasmVars[i].state)
	{
	    mask1 = PasmVars[i].mem[REG_DIRA]; // dira
	    val |= mask1 & PasmVars[i].mem[REG_OUTA]; // outa
	    mask |= mask1;
	}
    }
    pin_val = (~mask) | val;
    //printf("UpdatePins2: %8.8x\n", pin_val);
}

void CheckIndRegs(PasmVarsT *pasmvars, int32_t instruct, int32_t sflag, int32_t dflag, int32_t *pdstaddr, int32_t *psrcaddr)
{
    int32_t dstaddr = *pdstaddr;
    int32_t srcaddr = *psrcaddr;
    int32_t cond = (instruct >> 18) & 15;
    int32_t opcode_zci = (instruct >> 22) & 0x3ff;
    int32_t indirects = (!sflag && (srcaddr & 0x1fe) == 0x1f2);
    int32_t indirectd = (!dflag && (dstaddr & 0x1fe) == 0x1f2);
    int32_t pre_indexs = cond & 3;
    int32_t pre_indexd = cond >> 2;
    int32_t post_indexs = pre_indexs;
    int32_t post_indexd = pre_indexd;
    static int32_t postincrtab[4] = {0, 1, -1, 1};
    int32_t incra = 0;
    int32_t incrb = 0;

    // Don't handle fixindx or setindx here
    if (opcode_zci == 0x3f0) return;

    // Return if neither source or destination is indirect
    if (!indirects && !indirectd) return;

    // Or post-increment indices if source and destination use same register
    if (indirects && indirectd && srcaddr == dstaddr)
        post_indexs = post_indexd = (pre_indexs | pre_indexd);

    if (indirectd)
    {
	if (dstaddr == REG_INDA)
	{
	    incra = postincrtab[post_indexd];
            dstaddr = pasmvars->inda + (pre_indexd == 3);
            if (dstaddr > pasmvars->indatop) dstaddr = pasmvars->indabot;
	}
	else
	{
	    incrb = postincrtab[post_indexd];
            dstaddr = pasmvars->indb + (pre_indexd == 3);
            if (dstaddr > pasmvars->indbtop) dstaddr = pasmvars->indbbot;
	}
    }

    if (indirects)
    {
	if (srcaddr == REG_INDA)
	{
	    incra = postincrtab[post_indexd];
            srcaddr = pasmvars->inda + (pre_indexd == 3);
            if (srcaddr > pasmvars->indatop) srcaddr = pasmvars->indabot;
	}
	else
	{
	    incrb = postincrtab[post_indexd];
            srcaddr = pasmvars->indb + (pre_indexd == 3);
            if (srcaddr > pasmvars->indbtop) srcaddr = pasmvars->indbbot;
	}
    }

    if (incra == 1)
    {
	if (pasmvars->inda < pasmvars->indatop)
	    pasmvars->inda0 = pasmvars->inda + 1;
	else
	    pasmvars->inda0 = pasmvars->indabot;
    }
    else if (incra == -1)
    {
	if (pasmvars->inda > pasmvars->indabot)
	    pasmvars->inda0 = pasmvars->inda - 1;
	else
	    pasmvars->inda0 = pasmvars->indatop;
    }

    if (incrb == 1)
    {
	if (pasmvars->indb < pasmvars->indbtop)
	    pasmvars->indb0 = pasmvars->indb + 1;
	else
	    pasmvars->indb0 = pasmvars->indbbot;
    }
    else if (incrb == -1)
    {
	if (pasmvars->indb > pasmvars->indbbot)
	    pasmvars->indb0 = pasmvars->indb - 1;
	else
	    pasmvars->indb0 = pasmvars->indbtop;
    }

    // Update values of srcaddr and dstaddr
    *psrcaddr = srcaddr;
    *pdstaddr = dstaddr;
}

#if 0
void IncrementIndRegs(PasmVarsT *pasmvars)
{
    pasmvars->inda += pasmvars->indaincr;
    if (pasmvars->inda > pasmvars->indatop)
	pasmvars->inda = pasmvars->indabot;
    else if (pasmvars->inda < pasmvars->indabot)
	pasmvars->inda = pasmvars->indatop;

    pasmvars->indb += pasmvars->indbincr;
    if (pasmvars->indb > pasmvars->indbtop)
	pasmvars->indb = pasmvars->indbbot;
    else if (pasmvars->indb < pasmvars->indbbot)
	pasmvars->indb = pasmvars->indbtop;
}
#endif

void SaveRegisters(PasmVarsT *pasmvars)
{
    pasmvars->inda0 = pasmvars->inda;
    pasmvars->indb0 = pasmvars->indb;
    pasmvars->ptra0 = pasmvars->ptra;
    pasmvars->ptrb0 = pasmvars->ptrb;
}

void UpdateRegisters(PasmVarsT *pasmvars)
{
    pasmvars->inda = pasmvars->inda0;
    pasmvars->indb = pasmvars->indb0;
    pasmvars->ptra = pasmvars->ptra0;
    pasmvars->ptrb = pasmvars->ptrb0;
}

int32_t FetchHubInstruction(PasmVarsT *pasmvars, int32_t prefetch)
{
    int32_t i, j, pc, lineaddr, maxnotused;

    if (prefetch)
	pc = (pasmvars->pc1 + 8) & 0xffff;
    else
	pc = pasmvars->pc & 0xffff;
    lineaddr = (pc << 2) & 0x3ffe0;

    // Check if already cached
    for (i = 0; i < 4; i++)
    {
        if (lineaddr == pasmvars->icachehubaddr[i]) break;
    }

    // Update usage if not prefetch
    if (!prefetch)
    {
	for (j = 0; j < 4; j++)
	{
	    if (j == i)
		pasmvars->icachenotused[j] = 0;
	    else
	    {
		pasmvars->icachenotused[j]++;
		if (pasmvars->icachenotused[j] > 255)
		    pasmvars->icachenotused[j] = 255;
	    }
	}
    }

    // Return if cached
    if (i < 4)
        return pasmvars->icache[i][pc&7];

    // Find the least used cache buffer
    j = 0;
    maxnotused = pasmvars->icachenotused[0];
    for (i = 1; i < 4; i++)
    {
        if (pasmvars->icachenotused[i] > maxnotused)
	{
	    j = i;
	    maxnotused = pasmvars->icachenotused[i];
	}
    }

    // Fill selected cache buffer and return
#if 0
    if (!prefetch)
	printf("FetchHubInstruction: Fetch into cache %d, pc = %4.4x\n", j, pc);
    else
	printf("FetchHubInstruction: Prefetch into cache %d, pc = %4.4x\n", j, pc);
#endif
    if (!prefetch)
    {
	pasmvars->waitmode = WAIT_CACHE;
	pasmvars->waitflag = ((pasmvars->cogid - loopcount) & 7) + 2;
    }
    pasmvars->icache[j][0] = LONG(lineaddr);
    pasmvars->icache[j][1] = LONG(lineaddr + 4);
    pasmvars->icache[j][2] = LONG(lineaddr + 8);
    pasmvars->icache[j][3] = LONG(lineaddr + 12);
    pasmvars->icache[j][4] = LONG(lineaddr + 16);
    pasmvars->icache[j][5] = LONG(lineaddr + 20);
    pasmvars->icache[j][6] = LONG(lineaddr + 24);
    pasmvars->icache[j][7] = LONG(lineaddr + 28);
    pasmvars->icachehubaddr[j] = lineaddr;
    pasmvars->icachenotused[j] = 0;
    return pasmvars->icache[j][pc&7];
}

void CheckPrefetch(PasmVarsT *pasmvars)
{
    if (!pasmvars->prefetch) return;
    if (pasmvars->cogid != (loopcount & 7)) return;
    if (pasmvars->waitmode & WAIT_HUB) return;
    if (pasmvars->pc < 0x200) return;
    FetchHubInstruction(pasmvars, 1);
}

int32_t ExecutePasmInstruction2(PasmVarsT *pasmvars)
{
    int32_t cflag = pasmvars->cflag;
    int32_t zflag = pasmvars->zflag;
    int32_t instruct, pc, cond;
    int32_t opcode, value2, value1, zci;
    int32_t srcaddr, dstaddr;
    int32_t result = 0;
    int32_t temp, write_zcr, sflag, dflag, psflag, pdflag;
    int32_t opcode_zci, indirect, rflag, aflag;
    int32_t returnflag = 0;
    int32_t breakflag = 0;
    int32_t hubop = 0;

    // Check if multiplier working
    if (pasmvars->mulcount)
	pasmvars->mulcount--;

    // Check if we can skip further processing if not printing, not waiting
    // for a pin state and wait flag is greater than 1.
    if (pasmvars->waitflag > 1 && !pasmvars->printflag && pasmvars->waitmode != WAIT_PIN)
    {
	CheckPrefetch(pasmvars);
	pasmvars->waitflag--;
	return 0;
    }

    // Fetch a new instruction and update the pipeline
    if (!pasmvars->waitflag)
    {
	// Update instruction pipeline and fetch new instruction
        pasmvars->instruct4 = pasmvars->instruct3;
        pasmvars->instruct3 = pasmvars->instruct2;
        pasmvars->instruct2 = pasmvars->instruct1;
	if ((pasmvars->pc & 0xfff8) == pasmvars->dcachecogaddr)
            pasmvars->instruct1 = pasmvars->dcache[pasmvars->pc & 7];
	else if (pasmvars->pc < 0x200)
            pasmvars->instruct1 = pasmvars->mem[pasmvars->pc];
	else if (pasmvars->pc)
            pasmvars->instruct1 = FetchHubInstruction(pasmvars, 0);
	// Update PC pipeline and increment PC
        pasmvars->pc4 = pasmvars->pc3;
        pasmvars->pc3 = pasmvars->pc2;
        pasmvars->pc2 = pasmvars->pc1;
        pasmvars->pc1 = pasmvars->pc;
	if (pasmvars->repcnt && pasmvars->pc >= pasmvars->reptop)
	{
	    if (!pasmvars->repforever) pasmvars->repcnt--;
	    pasmvars->pc = pasmvars->repbot;
	}
	else
	    pasmvars->pc = (pasmvars->pc + 1) & 0xffff;
	// Check for REPS instruction in the second stage of the pipeline
	if ((pasmvars->instruct2 & 0xffc00000) == 0xfac00000 && !(pasmvars->pc2 & INVALIDATE_INSTR))
	{
	    pasmvars->repcnt = (pasmvars->instruct2 >> 6) & 0xffff;
	    pasmvars->repbot = pasmvars->pc;
	    pasmvars->reptop = (pasmvars->pc + (pasmvars->instruct2 & 63)) & 0xffff;
	}
    }

    // Check for cache wait
    if (pasmvars->waitflag && pasmvars->waitmode == WAIT_CACHE)
    {
	if (--pasmvars->waitflag == 0)
		pasmvars->waitmode = 0;
	else
	{
	    if (pasmvars->printflag) DebugPasmInstruction2(pasmvars);
	    return 0;
	}
    }

    // Get the instruction and pc at the end of the pipeline
    instruct = pasmvars->instruct4;
    pc = pasmvars->pc4;

    // Return if instruction has been invalidated and not printing
    if (pc & INVALIDATE_INSTR)
    {
	if (!pasmvars->printflag)
	{
	    CheckPrefetch(pasmvars);
#if 0
	    if (pasmvars->waitflag)
	    {
		pasmvars->waitflag--;
		if (!pasmvars->waitflag) pasmvars->waitmode = 0;
	    }
#endif
	    return 0;
	}
	returnflag = 1;
    }

    // Extract bit fields from the instruction
    opcode     = (instruct >> 25) & 127;
    zci        = (instruct >> 22) & 7;
    cond       = (instruct >> 18) & 15;
    dstaddr    = (instruct >> 9) & 511;
    srcaddr    = instruct & 511;
    opcode_zci = (opcode << 3) | zci;

    // Decode the immediate flags for the source and destination fields
    sflag = (opcode_zci >= 0x3ea) | (zci & 1);

    dflag = (opcode >= 0x68 && opcode <= 0x7a && (zci & 2)) |
	((opcode_zci & 0x3e2) == 0x302 && opcode_zci < 0x31e) | 
	(opcode != 0x7f && opcode_zci >= 0x3eb) | 
	(opcode == 0x7f && srcaddr >= 0x40 && srcaddr <= 0xdc && (zci & 1)) |
	(opcode == 0x7f && srcaddr >= 0x100);

    // Determine if indirect registers are used
    indirect = (!sflag && (srcaddr & 0x1fe) == 0x1f2) |
	(!dflag && (dstaddr & 0x1fe) == 0x1f2) |
	(opcode_zci == 0x3f0);

    // Determine if ptra or ptrb are referenced
    psflag = ((opcode < 6) | (opcode_zci >= 0x340 && opcode_zci <= 0x34b)) & sflag;
    pdflag = (opcode == 0x7f && srcaddr >= 0x085 && srcaddr <= 0x087 && dflag);

    // Determine if instruction uses relative address
    rflag = (((opcode >= 0x54 && opcode <= 0x57) |
	(opcode >= 0x76 && opcode <= 0x77) |
	(opcode_zci >= 0x3d8 && opcode_zci <= 0x3e8)) & sflag) |
	(opcode_zci == 0x3ea);

    // Determine if instruction is AUGS or AUGD
    aflag = (opcode_zci >= 0x3ec && opcode_zci <= 0x3ef);

    // Save ptra, ptrb, inda and indb
    SaveRegisters(pasmvars);

    // Handle the indirect registers
    if (indirect) CheckIndRegs(pasmvars, instruct, sflag, dflag, &dstaddr, &srcaddr);

    // else return if condition code is not met and
    // not using indirect registers and not augx instruction
    else if (!((cond >> ((cflag << 1) | zflag)) & 1) && !aflag)
    {
	if (!pasmvars->printflag)
	{
	    CheckPrefetch(pasmvars);
#if 0
	    if (pasmvars->waitflag)
	    {
		pasmvars->waitflag--;
		if (!pasmvars->waitflag) pasmvars->waitmode = 0;
	    }
#endif
	    return 0;
	}
	returnflag = 1;
    }

    // Set the flag that controls writing the result, zero flag and carry flag
    write_zcr = zci | 1; // Assume writing result

    // Get value1 from the destination field
    if (pdflag)
	value1 = GetPointer(pasmvars, dstaddr, 5);
    else if (dflag)
    {
	if (pasmvars->augdflag)
	    value1 = (pasmvars->augdvalue << 9) | dstaddr;
	else
	    value1 = dstaddr;
    }
    else if ((dstaddr & 0x1f8) == pasmvars->dcachecogaddr)
	value1 = pasmvars->dcache[dstaddr & 7];
    else
	value1 = pasmvars->mem[dstaddr];

    // Get value2 from the source field
    if (psflag)
    {
	if (opcode < 6)
	    value2 = GetPointer(pasmvars, srcaddr, opcode >> 1);
	else
	    value2 = GetPointer(pasmvars, srcaddr, (opcode_zci >> 2) & 3);
    }
    else if (sflag)
    {
	if (pasmvars->augsflag)
	    value2 = (pasmvars->augsvalue << 9) | srcaddr;
	else if (rflag)
            value2 = (srcaddr << 23) >> 23;
	else
            value2 = srcaddr;
    }
    else if (srcaddr == REG_PINA)
        value2 = pin_val;
    else if ((srcaddr & 0x1f8) == pasmvars->dcachecogaddr)
	value2 = pasmvars->dcache[srcaddr & 7];
    else
	value2 = pasmvars->mem[srcaddr];

    // Check sflag and dflag and reset augsflag and augdflag if needed
    if (sflag) pasmvars->augsflag = 0;
    if (dflag) pasmvars->augdflag = 0;

    // Check the wait flag if the return flag is not set
    if (returnflag)
    {
        if (pasmvars->waitflag)
	{
	    pasmvars->waitflag--;
	    if (!pasmvars->waitflag) pasmvars->waitmode = 0;
	}
    }
    else
	hubop = CheckWaitFlag2(pasmvars, instruct, value1, value2);

    if (!hubop)
	CheckPrefetch(pasmvars);

    // Print instruction if printflag set
    if (pasmvars->printflag) DebugPasmInstruction2(pasmvars);

    // Return if returnflag or waitflag is set
    if (returnflag || pasmvars->waitflag) return 0;

    // Check for breakpoint
    if (pc == pasmvars->breakpnt)
    {
	if (!pasmvars->printflag) pasmvars->printflag = 15;
	DebugPasmInstruction2(pasmvars);
	breakflag = 1;
    }

    // Update ptra, ptrb, inda and indb
    UpdateRegisters(pasmvars);

    // Decode the opcode and execute the instruction
    // Set the variables result, zflag and cflag.
    // Clear bits within write_zcr to prevent writing variables.
    switch(opcode >> 3) // Decode the four most significant bits
    {
	case 0: // rdxxxx, rdxxxc, rdaux, rdauxr
        if (opcode < 6)
        {
#if 0
	    // Check if using a ptr register
	    if (sflag) value2 = GetPointer(pasmvars, srcaddr, opcode >> 1);

#endif
	    if (opcode & 1) // cache read
            {
		int32_t hubaddr = value2 & 0xffffffe0;
		if (hubaddr != pasmvars->dcachehubaddr)
		{
		    pasmvars->dcache[0] = LONG(hubaddr);
		    pasmvars->dcache[1] = LONG(hubaddr+4);
		    pasmvars->dcache[2] = LONG(hubaddr+8);
		    pasmvars->dcache[3] = LONG(hubaddr+12);
		    pasmvars->dcache[4] = LONG(hubaddr+16);
		    pasmvars->dcache[5] = LONG(hubaddr+20);
		    pasmvars->dcache[6] = LONG(hubaddr+24);
		    pasmvars->dcache[7] = LONG(hubaddr+28);
		    pasmvars->dcachehubaddr = hubaddr;
		}
	    }
	}
	switch(opcode)
	{
	    uint8_t *bptr;
	    uint16_t *wptr;

	    case 0: // rdbyte
	    result = BYTE(value2);
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdb[%x]", value2);
            break;

	    case 1: // rdbytec
	    bptr = (uint8_t *)pasmvars->dcache;
	    result = bptr[value2 & 31];
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdb[%x]", value2);
            break;

	    case 2: // rdword
	    result = WORD(value2);
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdw[%x]", value2);
            break;

	    case 3: // rdwordc
	    wptr = (uint16_t *)pasmvars->dcache;
	    result = wptr[(value2 >> 1) & 15];
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdw[%x]", value2);
            break;

	    case 4: // rdlong
	    result = LONG(value2);
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdl[%x]", value2);
            break;

	    case 5: // rdlongc
	    result = pasmvars->dcache[(value2 >> 2) & 7];
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdl[%x]", value2);
            break;

	    case 6: // rdaux
	    case 7: // rdauxr
	    if ((zci & 1) && (srcaddr & 0x100))
		temp = GetAuxPointer(pasmvars, srcaddr);
	    else
		temp = value2 & 0xff;
	    if (opcode & 1)
		temp ^= 0xff;
	    result = pasmvars->auxram[temp];
            break;
	}
	zflag = (result == 0);
	break;

        case 1: // isob, notb, clrb, setbxx
	value2 = (value2 & 0x1f);
	cflag = (value1 >> value2) & 1;
        switch ((value2 >> 5) & 7)
        {
	    case 0: // isob
	    result = (value1 >> value2) & 1;
	    break;

	    case 1: // notb
	    result = value1 ^ (1 << value2);
	    break;

	    case 2: // clrb
	    result = value1 & (~(1 << value2));
	    break;

	    case 3: // setb
	    result = value1 | (1 << value2);
	    break;

	    case 4: // setbc
	    result = value1 & (~(1 << value2));
	    result |= cflag << value2;
	    break;

	    case 5: // setbnc
	    result = value1 & (~(1 << value2));
	    result |= (cflag ^ 1) << value2;
	    break;

	    case 6: // setbz
	    result = value1 & (~(1 << value2));
	    result |= zflag << value2;
	    break;

	    case 7: // setbnz
	    result = value1 & (~(1 << value2));
	    result |= (zflag ^ 1) << value2;
	    break;
	}
	zflag = (result == 0);
        break;

        case 2: // andn, and, or, xor, muxxx
	switch (opcode & 7)
	{
	    case 0: // andn
	    result = value1 & (~value2);
	    break;

	    case 1: // and
	    result = value1 & value2;
	    break;

	    case 2: // or
	    result = value1 | value2;
	    break;

	    case 3: // xor
	    result = value1 ^ value2;
	    break;

	    case 4: // muxc
	    result = (value1 & (~value2)) | (value2 & (-cflag));
	    break;

	    case 5: // muxnc
	    result = (value1 & (~value2)) | (value2 & (~(-cflag)));
	    break;

	    case 6: // muxz
	    result = (value1 & (~value2)) | (value2 & (-zflag));
	    break;

	    case 7: // muxnz
	    result = (value1 & (~value2)) | (value2 & (~(-zflag)));
	    break;
	}
	zflag = (result == 0);
	cflag = parity(result);
	break;

        case 3: // rotate and shift
	value2 &= 0x1f; // Get five LSB's
	switch (opcode & 7)
	{
	    case 0: // ror
	    result = (((uint32_t)value1) >> value2) | (value1 << (32 - value2));
	    cflag = value1 & 1;
	    break;

	    case 1: // rol
	    result = (((uint32_t)value1) >> (32 - value2)) | (value1 << value2);
	    cflag = (value1 >> 31) & 1;
	    break;

	    case 2: // shr
	    result = (((uint32_t)value1) >> value2);
	    cflag = value1 & 1;
	    break;

	    case 3: // shl
	    result = (value1 << value2);
	    cflag = (value1 >> 31) & 1;
	    break;

	    case 4: // rcr
	    if (value2)
	    {
	        result = (cflag << 31) | (((uint32_t)value1) >> 1);
		result >>= (value2 - 1);
	    }
	    else
	        result = value1;
	    cflag = value1 & 1;
	    break;

	    case 5: // rcl
	    result = cflag ? (1 << value2) - 1 : 0;
	    result |= (value1 << value2);
	    cflag = (value1 >> 31) & 1;
	    break;

	    case 6: // sar
	    result = value1 >> value2;
	    cflag = value1 & 1;
	    break;

	    case 7: // rev
	    cflag = value1 & 1;
	    value2 = 32 - value2;
	    result = 0;
	    while (value2-- > 0)
	    {
		result = (result << 1) | (value1 & 1);
		value1 >>= 1;
	    }
	    break;
	}
	zflag = (result == 0);
	break;

        case 4: // mov, not abs, negxx
	switch (opcode & 7)
	{
	    case 0: // mov
	    result = value2;
	    cflag = (value2 >> 31) & 1;
	    break;

	    case 1: // not
	    cflag = value2 < 0;
	    result = ~value2;
	    break;

	    case 2: // abs
	    cflag = (value2 >> 31) & 1;
	    result = _abs(value2);
	    break;

	    case 3: // neg
	    cflag = (value2 >> 31) & 1;
	    result = -value2;
	    break;

	    case 4: // negc
	    result = cflag ? -value2 : value2;
	    cflag = (value2 >> 31) & 1;
	    break;

	    case 5: // negnc
	    result = cflag ? value2 : -value2;
	    cflag = (value2 >> 31) & 1;
	    break;

	    case 6: // negz
	    result = zflag ? -value2 : value2;
	    cflag = (value2 >> 31) & 1;
	    break;

	    case 7: // negnz
	    result = zflag ? value2 : -value2;
	    cflag = (value2 >> 31) & 1;
	    break;
	}
	zflag = (result == 0);
	break;

        case 5: // addxx, subxx
	switch (opcode & 7)
	{
	    case 0: // add
	    result = value1 + value2;
	    cflag = (((value1 & value2) | ((value1 | value2) & (~result))) >> 31) & 1;
	    break;

	    case 1: // sub
	    result = value1 - value2;
	    cflag = ((uint32_t)value1) < ((uint32_t)value2);
	    break;

	    case 2: // addx
	    result = value1 + value2 + cflag;
	    cflag = (((value1 & value2) | ((value1 | value2) & (~result))) >> 31) & 1;
	    zflag = (result == 0) & zflag;
	    break;

	    case 3: // subx
	    result = value1 - value2 - cflag;
	    if (value2 != 0xffffffff || !cflag)
	        cflag = ((uint32_t)value1) < ((uint32_t)(value2 + cflag));
	    zflag = (result == 0) & zflag;
	    break;

	    case 4: // adds
	    result = value1 + value2;
	    cflag = (((~(value1 ^ value2)) & (value1 ^ result)) >> 31) & 1;
	    zflag = (result == 0);
	    break;

	    case 5: // subs
	    result = value1 - value2;
	    zflag = (result == 0);
	    cflag = (((value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
	    break;

	    case 6: // addsx
	    result = value1 + value2 + cflag;
	    cflag = (((~(value1 ^ value2)) & (value1 ^ result)) >> 31) & 1;
	    zflag = (result == 0) & zflag;
	    break;

	    case 7: // subsx
	    result = value1 - value2 - cflag;
	    cflag = (((value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
	    zflag = (result == 0) & zflag;
	    break;
	}
	zflag = (result == 0);
	break;


        case 6: // sumxx, min, max, mins, maxs
        switch (opcode & 7)
        {
	    case 0: // sumc
	    result = cflag ? value1 - value2 : value1 + value2;
	    cflag = (~cflag) << 31;
	    cflag = (((cflag ^ value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
	    break;

	    case 1: // sumnc
	    result = cflag ? value1 + value2 : value1 - value2;
	    cflag = cflag << 31;
	    cflag = (((cflag ^ value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
	    break;

	    case 2: // sumz
	    result = zflag ? value1 - value2 : value1 + value2;
	    cflag = (~zflag) << 31;
	    cflag = (((cflag ^ value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
	    break;

	    case 3: // sumnz
	    result = zflag ? value1 + value2 : value1 - value2;
	    cflag = zflag << 31;
	    cflag = (((cflag ^ value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
	    break;

	    case 4: // min
	    cflag = (((uint32_t)value1) < ((uint32_t)value2));
	    zflag = (value2 == 0);
	    result = cflag ? value2 : value1;
	    break;

	    case 5: // max
	    cflag = (((uint32_t)value1) < ((uint32_t)value2));
	    zflag = (value2 == 0);
	    result = cflag ? value1 : value2;
	    break;

	    case 6: // mins
	    cflag = (value1 < value2);
	    zflag = (value2 == 0);
	    result = cflag ? value2 : value1;
	    break;

	    case 7: // maxs
	    cflag = (value1 < value2);
	    zflag = (value2 == 0);
	    result = cflag ? value1 : value2;
	    break;
        }
	break;

        case 7: // addabs, subabs, incmod, decmod, cmpsub, subr, mul, scl
	switch (opcode & 7)
	{
	    case 0: // addabs
	    cflag = (value2 >> 31) & 1;
	    value2 = _abs(value2);
	    result = value1 + value2;
	    cflag ^= (((value1 & value2) | ((value1 | value2) & (~result))) >> 31) & 1;
	    break;

	    case 1: // subabs
	    result = _abs(value2);
	    cflag = ((value2 >> 31) & 1) ^
	        (((uint32_t)value1) < ((uint32_t)result));
	    result = value1 - result;
	    break;

            case 2: // incmod
	    cflag = (value1 == value2);
	    if (cflag)
	        result = 0;
	    else
	        result = value1 + 1;
	    break;

            case 3: // decmod
	    cflag = (value1 == 0);
	    if (cflag)
	        result = value2;
	    else
	        result = value1 - 1;
	    break;

	    case 4: // cmpsub
	    cflag = (((uint32_t)value1) >= ((uint32_t)value2));
	    result = cflag ? value1 - value2 : value1;
	    zflag = (result == 0);
	    break;

            case 5: // subr
	    result = value2 - value1;
	    cflag = ((uint32_t)value2) < ((uint32_t)value1);
	    break;

            case 6: // mul
	    value1 = (value1 << 12) >> 12;
	    value2 = (value2 << 12) >> 12;
	    result = value1 * value2;
            break;

            case 7: // scl
            NotImplemented(instruct);
            break;
	}
	zflag = (result == 0);
	break;

        case 8: // decodex, encod, blmask, onecnt, zercnt, incpar, decpar, splitx, mergex
        switch(opcode&7)
        {
	    case 0: // decod2
	    result = 1 << (value1 & 3);
	    result |= result << 4;
	    result |= result << 8;
	    result |= result << 16;
	    break;

	    case 1: // decod3
	    result = 1 << (value1 & 7);
	    result |= result << 8;
	    result |= result << 16;
	    break;

	    case 2: // decod4
	    result = 1 << (value1 & 0xf);
	    result |= result << 16;
	    break;

	    case 3: // decod5
	    result = 1 << (value1 & 0x1f);
	    break;

	    case 4: // encod, blmask
            if (!(zci & 2)) // encod
            {
		for (result = 32; result > 0; result--)
		{
		    if (value2 & 0x80000000) break;
		    value2 <<= 1;
		}
            }
            else // blmask
            {
	        value1 = (value1 & 0x1f) + 1;
	        if (value1 == 32)
	            result = 0xffffffff;
	        else
	            result = (1 << value1) - 1;
            }
	    break;

	    case 5: // onecnt, zercnt
            if (!(zci & 2)) // onecnt
            {
	        for (result = 0; value1; value1 <<= 1)
	        {
		    if (value1 & 0x80000000) result++;
	        }
            }
            else // zercnt
            {
	        for (result = 32; value1; value1 <<= 1)
	        {
		    if (value1 & 0x80000000) result--;
	        }
            }
	    break;

            case 6: // incpat, decpat
            if (!(zci & 4)) // incpat
            {
                NotImplemented(instruct);
            }
            else // decpat
            {
                NotImplemented(instruct);
            }
	    break;

            case 7: //splitb, mergeb, splitw, mergew
            switch (zci >> 1)
            {
                int i;
	        case 0: // splitb
                NotImplemented(instruct);
                break;

	        case 1: // mergeb
                NotImplemented(instruct);
                break;

	        case 2: // splitw
	        for (i = 0; i < 16; i++)
	        {
		    result = (result << 1) |
		        ((value1 >> 15)&0x10000) | ((value1 >> 30)&1);
		    value1 <<= 1;
	        }
                break;

	        case 3: // mergew
	        for (i = 0; i < 16; i++)
	        {
		    result = (result << 2) |
		        ((value1 >> 30) & 2) | ((value1 >> 15) & 1);
		    value1 <<= 1;
	        }
	        break;
            }
	    break;
        }
        break;


        case 9: // getnib, setbin, getword, setword, setwrds, rolnib, ...
	switch (opcode & 7)
	{
	    case 0: // getnib, setnib
	    case 1:
	    case 2:
	    case 3:
	    temp = (instruct >> 22) & 0x1c;
	    if (!(zci&2)) // getnib
	    {
		result = (value1 >> temp) & 15;
	    }
	    else // setnib
	    {
		result = (value2 & 15) << temp;
		temp = ~(15 << temp);
		result |= (value1 & temp);
	    }
	    break;

	    case 4: // getword, setword
	    temp = (instruct >> 20) & 16;
	    if (!(zci&2)) // getword
	    {
		result = (value1 >> temp) & 0xffff;
	    }
	    else // setword
	    {
		result = (value2 & 0xffff) << temp;
		temp = ~(0xffff << temp);
		result |= (value1 & temp);
	    }
	    break;

	    case 5: // setwrds, rolnib, rolbyte, rolword
	    NotImplemented(instruct);
	    break;

	    case 6: // sets, setd, setx, seti
	    switch (zci >> 1)
	    {
		case 0: // sets
		result = (value1 & 0xfffffe00) | (value2 &0x1ff);
		break;

		case 1: // setd
		result = (value1 & 0xfffc01ff) | ((value2 &0x1ff) << 9);
		break;

		case 2: // setx
		result = (value1 & 0xff83ffff) | ((value2 &0x1f) << 18);
		NotImplemented(instruct);
		break;

		case 3: // seti
		result = (value1 & 0x007fffff) | ((value2 &0x1ff) << 23);
		break;
	    }
	    write_zcr &= 1;
	    break;

	    case 7:
	    write_zcr &= 3;
	    if (!(zci & 4)) // cognew
	    {
	        // Look for next available cog
	        for (result = 0; result < 8; result++)
	        {
	            if (!PasmVars[result].state) break;
	        }
	        if (result == 8) // None available
	        {
		    cflag = 1;
		    result = 7;
	        }
		else // Start cog
		{
		    cflag = 0;
		    StartPasmCog2(&PasmVars[result], (value2 & 0x3ffff), (value1 & 0x3fffc), result);
		}
	    }
	    else // waitcnt
	    {
		if (value1 != GetCnt())
	        {
		    printf("ERROR: waitcnt value1 = %8.8x, cnt = %8.8x\n", value1, GetCnt());
		    spinsim_exit(1);
	        }
	        result = value1 + value2;
	        zflag = (result == 0);
	        cflag = (((value1 & value2) | ((value1 | value2) & (~result))) >> 31) & 1;
	    }
	    break;
	}
	break;

        case 10: // getbyte, setbyte, movbyts, xxxxrgb, xxxpix, jmpxx, xjxx
	switch (opcode & 7)
	{
	    case 0: // getbyte, setbyte
	    case 1:
	    temp = (instruct >> 21) & 0x18;
	    if (!(zci&2)) // getbyte
	    {
		result = (value1 >> temp) & 255;
	    }
	    else // setbyte
	    {
		result = (value2 & 255) << temp;
		temp = ~(255 << temp);
		result |= (value1 & temp);
	    }
	    break;

	    case 2: // setbyts, movbyts, packrgb, unpkrgb
	    NotImplemented(instruct);
	    break;

	    case 3: // addpix, mulpix, blnpix, mixpix
	    NotImplemented(instruct);
	    break;

	    case 4: // jmpsw
	    case 5: // jmpswd
	    if (zci&1)
		pasmvars->pc = (pc + value2) & 0xffff;
	    else
		pasmvars->pc = value2 & 0xffff;
	    // Invalidate the instruction pipeline if non-delayed
	    if (!(instruct & 0x20000))
	    {
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
		    pc++;
	    }
	    else
		pc += 4;
	    result = pc;
	    zflag = (result == 0);
	    cflag = 0;
	    break;

            case 6: // ijz, ijzd, ijnz, ijnzd
	    result = value1 + 1;
	    zflag = (result == 0);
	    cflag = (result == 0);
	    // Determine if we should jump
	    if (zflag != (zci >> 2))
	    {
		if (instruct & 0x00400000)
		    pasmvars->pc = (pc + value2) & 0xffff;
		else
		    pasmvars->pc = value2 & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed jump
	        if (!(zci & 2))
	        {
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
	        }
	    }
	    write_zcr &= 1;
	    break;

	    case 7: // djz, djzd, djnz, djnzd
	    result = value1 - 1;
	    zflag = (result == 0);
	    cflag = (result == -1);
	    // Determine if we should jump
	    if (zflag != (zci >> 2))
	    {
		if (instruct & 0x00400000)
		    pasmvars->pc = (pc + value2) & 0xffff;
		else
		    pasmvars->pc = value2 & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed jump
	        if (!(zci & 2))
	        {
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
	        }
	    }
	    write_zcr &= 1;
	    break;
	}
	break;

        case 11: // testxx, cmpxx
	write_zcr &= 6;
	switch (opcode & 7)
	{
	    case 0: // testb
	    NotImplemented(instruct);
	    break;

	    case 1: // testn
	    result = value1 & (~value2);
	    zflag = (result == 0);
	    cflag = parity(result);
	    break;

	    case 2: // test
	    result = value1 & value2;
	    zflag = (result == 0);
	    cflag = parity(result);
	    break;

	    case 3: // cmp
            //printf("\ncmp $%x $%x\n", value1, value2);
	    result = value1 - value2;
	    zflag = (result == 0);
	    cflag = ((uint32_t)value1) < ((uint32_t)value2);
	    break;

	    case 4: // cmpx
	    result = value1 - value2 - cflag;
	    if (value2 != 0xffffffff || !cflag)
	        cflag = ((uint32_t)value1) < ((uint32_t)(value2 + cflag));
	    zflag = (result == 0) & zflag;
	    break;

	    case 5: // cmps
            //printf("\ncmps $%x $%x\n", value1, value2);
	    result = value1 - value2;
	    zflag = (result == 0);
	    cflag =  value1 < value2;
	    break;

	    case 6: // cmpsx
	    result = value1 - value2 - cflag;
	    cflag = (((value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
	    zflag = (result == 0) & zflag;
	    break;

	    case 7: // cmpr
	    NotImplemented(instruct);
	    break;
	}
	break;

        case 12: // coginit, waitvid, waitpeq, waitpne
	switch (opcode & 7)
	{
	    case 0: // coginit, waitvid
	    case 1:
	    case 2:
	    case 3:
	    if (!(zci&2)) // coginit
	    {
		result = (instruct >> 24) & 7;
		//printf("\ncoginit: cog = %d, value1 = %8.8x, dflag = %d\n", result, value1, dflag);
		StartPasmCog2(&PasmVars[result], (value2 & 0x3ffff), (value1 & 0x3fffc), result);
	        UpdatePins2();
		// Return without saving if we restarted this cog
		if (result == pasmvars->cogid) return breakflag;
	    }
	    else // waitvid
	    {
		NotImplemented(instruct);
	    }
	    write_zcr = 0;
	    break;

	    case 4: // waitpeq
	    case 5:
	    case 6: // waitpne
	    case 7:
            write_zcr &= 2;
	    cflag = !CheckWaitPin(pasmvars, instruct, value1, value2);
	    break;
	}
	break;

        case 13: // wrxxx, frac, setaccx, macx, mul32x, div32x, div64x
        temp = ((opcode & 7) << 1) | (zci >> 2);

#if 0
	if (temp <= 2)
	{
	    // Check if using a ptr register
	    if (sflag) value2 = GetPointer(pasmvars, srcaddr, temp & 3);
	}

#endif
	switch (temp)
	{
	    case 0: // wrbyte
	    BYTE(value2) = value1;
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", wrb[%x] = %x", value2, value1);
	    break;

	    case 1: // wrword
	    WORD(value2) = value1;
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", wrw[%x] = %x", value2, value1);
	    break;

	    case 2: // wrlong
	    LONG(value2) = value1;
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", wrl[%x] = %x", value2, value1);
	    break;

	    case 3: // frac
	    if (value2)
	    {
		pasmvars->divq = ((uint64_t)value1 << 32) / (uint64_t)value2;
		pasmvars->divr = ((uint64_t)value1 << 32) % (uint64_t)value2;
	    }
	    else
	    {
		pasmvars->divq = 0;
		pasmvars->divr = 0;
	    }
	    break;

	    case 4: // wraux
	    case 5: // wrauxr
	    if ((zci & 1) && (srcaddr & 0x100))
		temp = GetAuxPointer(pasmvars, srcaddr);
	    else
		temp = value2 & 0xff;
	    if (opcode & 1)
		temp ^= 0xff;
	    pasmvars->auxram[temp] = value1;
	    break;

	    case 6: // setacca
	    pasmvars->acca = (((int64_t)value1) << 32) | (uint64_t)value2;
	    break;

	    case 7: // setaccb
	    pasmvars->accb = (((int64_t)value1) << 32) | (uint64_t)value2;
	    break;

	    case 8: // maca
	    value1 = (value1 << 12) >> 12;
	    value2 = (value2 << 12) >> 12;
	    pasmvars->acca += (int64_t)value1 * (int64_t)value2;
	    break;

	    case 9: // macb
	    value1 = (value1 << 12) >> 12;
	    value2 = (value2 << 12) >> 12;
	    pasmvars->accb += (int64_t)value1 * (int64_t)value2;
	    break;

	    case 10: // mul32
	    pasmvars->mulcount = 17;
	    pasmvars->mul = (int64_t)value1 * (int64_t)value2;
	    break;

	    case 11: // mul32u
	    pasmvars->mulcount = 17;
	    pasmvars->mul = (uint64_t)value1 * (uint64_t)value2;
	    break;

	    case 12: // div32
	    pasmvars->mulcount = 17;
	    if (value2)
	    {
		pasmvars->divq = value1 / value2;
		pasmvars->divr = value1 % value2;
	    }
	    else
	    {
		pasmvars->divq = 0;
		pasmvars->divr = 0;
	    }
	    break;

	    case 13: // div32u
	    pasmvars->mulcount = 17;
	    if (value2)
	    {
		pasmvars->divq = (uint32_t)value1 / (uint32_t)value2;
		pasmvars->divr = (uint32_t)value1 % (uint32_t)value2;
	    }
	    else
	    {
		pasmvars->divq = 0;
		pasmvars->divr = 0;
	    }
	    break;

	    case 14: // div64
	    pasmvars->mulcount = 17;
	    if (pasmvars->divisor)
	    {
		pasmvars->divq = ((((int64_t)value1) << 32) | (uint64_t)value2) / (int64_t)pasmvars->divisor;
		pasmvars->divr = ((((int64_t)value1) << 32) | (uint64_t)value2) % (int64_t)pasmvars->divisor;
	    }
	    else
	    {
		pasmvars->divq = 0;
		pasmvars->divr = 0;
	    }
	    break;

	    case 15: // div64u
	    pasmvars->mulcount = 17;
	    if (pasmvars->divisor)
	    {
		pasmvars->divq = ((((uint64_t)value1) << 32) | (uint64_t)value2) / (uint64_t)pasmvars->divisor;
		pasmvars->divr = ((((uint64_t)value1) << 32) | (uint64_t)value2) % (uint64_t)pasmvars->divisor;
	    }
	    else
	    {
		pasmvars->divq = 0;
		pasmvars->divr = 0;
	    }
	    break;
	}
	write_zcr = 0;
	break;

        case 14: // sqrt64, qsincos, qarctan, qrotate, setserx, setctrs, ...
        temp = ((opcode & 7) << 1) | (zci >> 2);
	switch (temp)
	{
	    case 0: // sqrt64
	    write_zcr = 0;
	    pasmvars->mulcount = 32;
            pasmvars->sqrt = sqrt64(((uint64_t)value1<<32) | (uint32_t)value2);
	    break;

	    case 1: // qsincos
	    NotImplemented(instruct);
	    break;

	    case 2: // qartan
	    NotImplemented(instruct);
	    break;

	    case 3: // qrotate
	    NotImplemented(instruct);
	    break;

	    case 4: // setsera
	    NotImplemented(instruct);
	    break;

	    case 5: // setserb
	    NotImplemented(instruct);
	    break;

	    case 6: // setctrs
	    NotImplemented(instruct);
	    break;

	    case 7: // setwavs
	    NotImplemented(instruct);
	    break;

	    case 8: // setfrqs
	    NotImplemented(instruct);
	    break;

	    case 9: // setphss
	    NotImplemented(instruct);
	    break;

	    case 10: // addphss
	    NotImplemented(instruct);
	    break;

	    case 11: // subphss
	    NotImplemented(instruct);
	    break;

	    case 12: // jp
	    case 13: // jpd
	    if (value1 < 32 && ((pin_val >> value1) & 1))
	    {
		if (instruct & 0x10000)
		    pasmvars->pc = (pc + value2) & 0xffff;
		else
	            pasmvars->pc = value2 & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed
		if (!(instruct & 1))
		{
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
		}
	    }
	    break;

	    case 14: // jnp
	    case 15: // jnpd
	    if (value1 < 32 && !((pin_val >> value1) & 1))
	    {
		if (instruct & 0x10000)
		    pasmvars->pc = (pc + value2) & 0xffff;
		else
	            pasmvars->pc = value2 & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed
		if (!(instruct & 1))
		{
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
		}
	    }
	    break;
	}
	write_zcr = 0;
        break;

        case 15:
        switch(opcode & 7)
        {
            case 0: // cfgpins
            case 1: // cfgpins, jmptask
	    NotImplemented(instruct);
            break;

            case 2: // setxft, setmix
	    NotImplemented(instruct);
            break;

            case 3: // jz, jzd, jnz, jnzd
	    if ((value1 == 0) != (zci >> 2))
	    {
		if (instruct & 0x00400000)
		    pasmvars->pc = (pc + value2) & 0xffff;
		else
		    pasmvars->pc = value2 & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed jump
	        if (!(zci & 2))
	        {
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
	        }
	    }
	    write_zcr = 0;
            break;

            case 4: // locbase, locbyte, locword, loclong
	    if (sflag)
	        result = (pc + value2) << 2;
	    else
	        result = value2 << 2;
	    switch (zci >> 1)
	    {
		case 0: // locbase
		result &= 0x3ffff;
		break;

		case 1: // locbyte
		result = (result + value1) & 0x3ffff;
		break;

		case 2: // locword
		result = (result + (value1 << 1)) & 0x3ffff;
		break;

		case 3: // loclong
		result = (result + (value1 << 2)) & 0x3ffff;
		break;
	    }
            break;

            case 5: // jmplist, locinst, reps, augs, augd
	    switch (zci)
	    {
		case 0: // jmplist
		case 1:
		NotImplemented(instruct);
		break;

		case 2: // locinst
		NotImplemented(instruct);
		break;

		case 3: // reps
		break;

		case 4: // augs
		case 5:
		pasmvars->augsflag = 1;
		pasmvars->augsvalue = instruct & 0x007fffff;
		break;

		case 6: // augd
		case 7:
		pasmvars->augdflag = 1;
		pasmvars->augdvalue = instruct & 0x007fffff;
		break;
	    }
	    write_zcr = 0;
            break;

            case 6: // fixindx, setindx, locptrx, jmpx, callx, callax, callbx, callxx, callyx
	    switch (zci)
	    {
		case 0: // fixindx, setindx
		switch (cond & 3) // Handle inda
		{
		    case 0:
		    break;

		    case 1: // fixinda
		    pasmvars->inda = srcaddr;
		    if (srcaddr < dstaddr)
		    {
			pasmvars->indabot = srcaddr;
			pasmvars->indatop = dstaddr;
		    }
		    else
		    {
			pasmvars->indabot = dstaddr;
			pasmvars->indatop = srcaddr;
		    }
		    break;

		    case 2: // setindb #addr
		    pasmvars->inda = srcaddr;
		    pasmvars->indabot = 0;
		    pasmvars->indatop = 511;
		    break;

		    case 3: // setindb ++/--delt
		    pasmvars->inda = (pasmvars->inda + ((srcaddr << 23) >> 23)) & 511;
		    pasmvars->indabot = 0;
		    pasmvars->indatop = 511;
		    break;
		}
		switch (cond >> 2) // Handle indb
		{
		    case 0:
		    break;

		    case 1: // fixindb
		    pasmvars->indb = srcaddr;
		    if (srcaddr < dstaddr)
		    {
			pasmvars->indbbot = srcaddr;
			pasmvars->indbtop = dstaddr;
		    }
		    else
		    {
			pasmvars->indbbot = dstaddr;
			pasmvars->indbtop = srcaddr;
		    }
		    break;

		    case 2: // setindb #addr
		    pasmvars->indb = dstaddr;
		    pasmvars->indbbot = 0;
		    pasmvars->indbtop = 511;
		    break;

		    case 3: // setindb ++/--delt
		    pasmvars->indb = (pasmvars->indb + ((dstaddr << 23) >> 23)) & 511;
		    pasmvars->indbbot = 0;
		    pasmvars->indbtop = 511;
		}
		break;

		case 1: // locptra, locptrb
		switch ((instruct >> 16) & 3)
		{
		    case 0: // locptra #abs
		    pasmvars->ptra = (instruct & 0xffff) << 2;
		    break;

		    case 1: // locptra @rel
		    pasmvars->ptra = ((pc + instruct) & 0xffff) << 2;
		    break;

		    case 2: // locptrb #abs
		    pasmvars->ptrb = (instruct & 0xffff) << 2;
		    break;

		    case 3: // locptrb @rel
		    pasmvars->ptrb = ((pc + instruct) & 0xffff) << 2;
		    break;
		}
		break;

		case 2: // jmp, jmpd
		if (instruct & 0x10000)
	            pasmvars->pc = (pc + instruct) & 0xffff;
		else
	            pasmvars->pc = instruct & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed
		if (!(instruct & 0x20000))
		{
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
		}
		break;

		case 3: // call, calld
		if (instruct & 0x10000)
	            pasmvars->pc = (pc + instruct) & 0xffff;
		else
	            pasmvars->pc = instruct & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed
		if (!(instruct & 0x20000))
		{
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
		    pc++;
		}
		else
		    pc += 4;
		pc |= (pasmvars->zflag << 17) | (pasmvars->cflag << 16);
		pasmvars->retstack[pasmvars->retptr] = pc;
		pasmvars->retptr = (pasmvars->retptr + 1) & 3;
		if (pasmvars->retptr == 0)
		    printf("return stack overflow\n");
		break;

		case 4: // calla, callad
		if (instruct & 0x10000)
	            pasmvars->pc = (pc + instruct) & 0xffff;
		else
	            pasmvars->pc = instruct & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed
		if (!(instruct & 0x20000))
		{
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
		    pc++;
		}
		else
		{
		    pc += 4;
		}
		pc |= (pasmvars->zflag << 17) | (pasmvars->cflag << 16);
		LONG(pasmvars->ptra) = pc;
		pasmvars->ptra = (pasmvars->ptra + 4) & 0x3ffff;
		break;

		case 5: // callb, callbd
		if (instruct & 0x10000)
	            pasmvars->pc = (pc + instruct) & 0xffff;
		else
	            pasmvars->pc = instruct & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed
		if (!(instruct & 0x20000))
		{
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
		    pc++;
		}
		else
		{
		    pc += 4;
		}
		pc |= (pasmvars->zflag << 17) | (pasmvars->cflag << 16);
		LONG(pasmvars->ptrb) = pc;
		pasmvars->ptrb = (pasmvars->ptrb + 4) & 0x3ffff;
		break;

		case 6: // callx, callxd
		if (instruct & 0x10000)
	            pasmvars->pc = (pc + instruct) & 0xffff;
		else
	            pasmvars->pc = instruct & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed
		if (!(instruct & 0x20000))
		{
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
		    pc++;
		}
		else
		    pc += 4;
		pc |= (pasmvars->zflag << 17) | (pasmvars->cflag << 16);
		pasmvars->auxram[pasmvars->ptrx] = pc;
		pasmvars->ptrx = (pasmvars->ptrx + 1) & AUX_MASK;
		break;

		case 7: // cally, callyd
		if (instruct & 0x10000)
	            pasmvars->pc = (pc + instruct) & 0xffff;
		else
	            pasmvars->pc = instruct & 0xffff;
	        // Invalidate the instruction pipeline if non-delayed
		if (!(instruct & 0x20000))
		{
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pasmvars->pc2 |= INVALIDATE_INSTR;
                    pasmvars->pc3 |= INVALIDATE_INSTR;
		    pc++;
		}
		else
		    pc += 4;
		pc |= (pasmvars->zflag << 17) | (pasmvars->cflag << 16);
		pasmvars->auxram[pasmvars->ptry^255] = pc;
		pasmvars->ptry = (pasmvars->ptry + 1) & AUX_MASK;
		break;
	    }
	    write_zcr = 0;
            break;

            case 7:
	    if ((instruct&511) >= 0x40 && (instruct&511) <= 0xdc)
	    {
		write_zcr &= 6;
	        if (zci & 1)
	            value1 = (instruct >> 9) & 511;
	    }

	    switch ((instruct >> 6) & 7)
	    {
		case 0:
		switch (instruct & 511)
		{
		    case 0x000: // cogid
		    result = pasmvars->cogid;
		    break;

		    case 0x001: // taskid
		    NotImplemented(instruct);
		    break;

		    case 0x002: // locknew
		    for (result = 0; result < 8; result++)
		    {
		        if (!lockalloc[result]) break;
		    }
		    if (result == 8)
		    {
		        cflag = 1;
		        result = 7;
		    }
		    else
		    {
		        cflag = 0;
		        lockalloc[result] = 1;
		    }
		    zflag = (result == 0);
		    break;

		    case 0x003: // getlfsr
		    NotImplemented(instruct);
		    break;

		    case 0x004: // getcnt
		    result = GetCnt();
		    zflag = (result == 0);
		    cflag = 0;
		    break;

		    case 0x005: // getcntx
		    NotImplemented(instruct);
		    break;

		    case 0x006: // getacal
		    result = pasmvars->acca & 0xffffffff;
		    break;

		    case 0x007: // getacah
		    result = pasmvars->acca >> 32;
		    break;

		    case 0x008: // getacbl
		    result = pasmvars->accb & 0xffffffff;
		    break;

		    case 0x009: // getacbh
		    result = pasmvars->accb >> 32;
		    break;

		    case 0x00a: // getptra
		    result = pasmvars->ptra;
		    zflag = (result == 0);
		    cflag = 0;
		    break;

		    case 0x00b: // getptrb
		    result = pasmvars->ptrb;
		    zflag = (result == 0);
		    cflag = 0;
		    break;

		    case 0x00c: // getptrx
		    result = pasmvars->ptrx;
		    break;

		    case 0x00d: // getptry
		    result = pasmvars->ptry;
		    break;

		    case 0x00e: // serina
		    NotImplemented(instruct);
		    break;

		    case 0x00f: // serinb
		    NotImplemented(instruct);
		    break;

		    case 0x010: // getmull
		    result = pasmvars->mul & 0xffffffff;
		    zflag = (result == 0);
		    cflag = 0;
		    break;

		    case 0x011: // getmulh
		    result = (pasmvars->mul >> 32) & 0xffffffff;
		    zflag = (result == 0);
		    cflag = 0;
		    break;

		    case 0x012: // getdivq
		    result = pasmvars->divq;
		    zflag = (result == 0);
		    cflag = 0;
		    break;

		    case 0x013: // getdivr
		    result = pasmvars->divr;
		    zflag = (result == 0);
		    cflag = 0;
		    break;

		    case 0x014: // getsqrt
		    result = pasmvars->sqrt;
		    zflag = (result == 0);
		    cflag = 0;
		    break;

		    case 0x015: // getqx
		    NotImplemented(instruct);
		    break;

		    case 0x016: // getqy
		    NotImplemented(instruct);
		    break;

		    case 0x017: // getqz
		    NotImplemented(instruct);
		    break;

		    case 0x018: // getphsa
		    NotImplemented(instruct);
		    break;

		    case 0x019: // getphza
		    NotImplemented(instruct);
		    break;

		    case 0x01a: // getcosa
		    NotImplemented(instruct);
		    break;

		    case 0x01b: // getsina
		    NotImplemented(instruct);
		    break;

		    case 0x01c: // getphsb
		    NotImplemented(instruct);
		    break;

		    case 0x01d: // getphzb
		    NotImplemented(instruct);
		    break;

		    case 0x01e: // getcosb
		    NotImplemented(instruct);
		    break;

		    case 0x01f: // getsinb
		    NotImplemented(instruct);
		    break;

		    case 0x020: // pushzc
		    result = (value1 << 2) | (zflag << 1) | cflag;
		    zflag = (value1 >> 31) & 1;
		    cflag = (value1 >> 30) & 1;
		    break;

		    case 0x021: // popzc
		    result = ((value1 >> 2) & 0x3fffffff) | (zflag << 31) | (cflag << 30);
		    zflag = (value1 >> 1) & 1;
		    cflag = value1 & 1;
		    break;

		    case 0x022: // subcnt
		    result = GetCnt() - value1;
		    cflag = (result >> 31) & 1;
		    zflag = (result == 0);
		    NotImplemented(instruct);
		    break;

		    case 0x023: // getpix
		    NotImplemented(instruct);
		    break;

		    case 0x024: // binbcd
		    NotImplemented(instruct);
		    break;

		    case 0x025: // bcdbin
		    NotImplemented(instruct);
		    break;

		    case 0x026: // bingry
		    result = value1 ^ ((value1 >> 1) & 0x7fffffff);
		    break;

		    case 0x027: // grybin
		    NotImplemented(instruct);
		    break;

		    case 0x028: // eswap4
		    NotImplemented(instruct);
		    break;

		    case 0x029: // eswap8
		    NotImplemented(instruct);
		    break;

		    case 0x02a: // seussf
		    result = seuss(value1, 1);
		    break;

		    case 0x02b: // seussr
		    result = seuss(value1, 0);
		    break;

		    case 0x02c: // incd
		    result = value1 + 0x200;
		    zflag = (result == 0);
		    write_zcr &= 5;
		    break;

		    case 0x02d: // decd
		    result = value1 - 0x200;
		    zflag = (result == 0);
		    write_zcr &= 5;
		    break;

		    case 0x02e: // incds
		    result = value1 + 0x201;
		    zflag = (result == 0);
		    write_zcr &= 5;
		    break;

		    case 0x02f: // decds
		    result = value1 - 0x201;
		    zflag = (result == 0);
		    write_zcr &= 5;
		    break;

		    case 0x030: // pop
		    pasmvars->retptr = (pasmvars->retptr - 1) & 3;
		    pasmvars->retstack[pasmvars->retptr] = value1;
		    if (pasmvars->retptr == 3)
			printf("return stack underflow\n");
		    break;

		    default:
		    NotImplemented(instruct);
		    break;
		}
		break;

		case 1: // repd
		if ((instruct & 0xffc3fe00) == 0xfe03fe00)
		{
		    pasmvars->repcnt = 1;
		    pasmvars->repforever = 1;
		}
		else
		    pasmvars->repcnt = value1;
		pasmvars->repbot = (pasmvars->pc) & 0xffff;
		pasmvars->reptop = (pasmvars->pc + (srcaddr & 63)) & 0xffff;
		break;

		case 2:
		write_zcr = 0;
		switch (instruct & 511)
		{
		    case 0x080: // clkset
		    result = value1 & 0x1ff;
		    if (result & 0x100)
		    {
		        RebootProp();
		        return breakflag;
		    }
		    break;

		    case 0x081: // cogstop
                    PasmVars[value1&7].state = 0;
		    UpdatePins2();
		    break;

		    case 0x082: // lockset
		    result = value1 & 7;
		    cflag = lockstate[result] & 1;
		    lockstate[result] = -1;
		    write_zcr = (zci & 2);
		    break;

		    case 0x083: // lockclr
		    result = value1 & 7;
		    cflag = lockstate[result] & 1;
		    lockstate[result] = 0;
		    write_zcr = (zci & 2);
		    break;

		    case 0x084: // lockret
		    for (result = 0; result < 8; result++)
		    {
		        if (!lockalloc[result]) break;
		    }
		    cflag = (result == 8);
		    result = value1 & 7;
		    zflag = (result == 0);
		    lockalloc[result] = 0;
		    break;

		    case 0x085: // rdwidec
		    case 0x086: // rdwide
#if 0
	            // Check if using a ptr register
		    if (zci & 1) value1 = GetPointer(pasmvars, dstaddr, 5);
#endif
                    value1 &= 0xffffffe0;
		    if (!(instruct & 1) || value1 != pasmvars->dcachehubaddr)
		    {
			pasmvars->dcache[0] = LONG(value1);
			pasmvars->dcache[1] = LONG(value1+4);
			pasmvars->dcache[2] = LONG(value1+8);
			pasmvars->dcache[3] = LONG(value1+12);
			pasmvars->dcache[4] = LONG(value1+16);
			pasmvars->dcache[5] = LONG(value1+20);
			pasmvars->dcache[6] = LONG(value1+24);
			pasmvars->dcache[7] = LONG(value1+28);
			pasmvars->dcachehubaddr = value1;
		    }
#if 0
		    fprintf(tracefile, "rdwide(%8.8x) %8.8x %8.8x %8.8x %8.8x\n",
		        value1, pasmvars->dcache[0], pasmvars->dcache[1],
		        pasmvars->dcache[2], pasmvars->dcache[3]);
#endif
		    write_zcr = 0;
		    break;

		    case 0x087: // wrwide
#if 0
	            // Check if using a ptr register
		    if (zci & 1) value1 = GetPointer(pasmvars, dstaddr, 5);
#endif
                    value1 &= 0xffffffe0;
	            LONG(value1) = pasmvars->dcache[0];
	            LONG(value1+4) = pasmvars->dcache[1];
	            LONG(value1+8) = pasmvars->dcache[2];
	            LONG(value1+12) = pasmvars->dcache[3];
	            LONG(value1+16) = pasmvars->dcache[4];
	            LONG(value1+20) = pasmvars->dcache[5];
	            LONG(value1+24) = pasmvars->dcache[6];
	            LONG(value1+28) = pasmvars->dcache[7];
#if 0
		    fprintf(tracefile, "wrwide(%8.8x) %8.8x %8.8x %8.8x %8.8x\n",
		        value1, pasmvars->dcache[0], pasmvars->dcache[1],
		        pasmvars->dcache[2], pasmvars->dcache[3]);
#endif
		    write_zcr = 0;
		    break;

		    case 0x088: // getp
		    if (value1 < 32)
		        cflag = (pin_val >> value1) & 1;
		    else
		        cflag = 1;
		    zflag = cflag ^ 1;
		    write_zcr = (zci & 6);
		    break;

		    case 0x089: // getnp
		    if (value1 < 32)
		        zflag = (pin_val >> value1) & 1;
		    else
		        zflag = 1;
		    cflag = zflag ^ 1;
		    write_zcr = (zci & 6);
		    break;

		    case 0x08a: // serouta
		    NotImplemented(instruct);
		    break;

		    case 0x08b: // seroutb
		    NotImplemented(instruct);
		    break;

		    case 0x08c: // cmpcnt
		    result = GetCnt() - value1;
		    cflag = (result >> 31) & 1;
		    zflag = (result == 0);
		    write_zcr = (zci & 6);
		    break;

		    case 0x08d: // waitpx
		    NotImplemented(instruct);
		    break;

		    case 0x08e: // waitpr
		    NotImplemented(instruct);
		    break;

		    case 0x08f: // waitpf
		    NotImplemented(instruct);
		    break;

		    case 0x090: // setzc
		    zflag = (value1 >> 1) & 1;
		    cflag = value1 & 1;
		    write_zcr = (zci & 6);
		    break;

		    case 0x091: // setmap
		    NotImplemented(instruct);
		    break;

		    case 0x092: // setxch
		    NotImplemented(instruct);
		    break;

		    case 0x093: // settask
		    NotImplemented(instruct);
		    break;

		    case 0x094: // setrace
		    NotImplemented(instruct);
		    break;

		    case 0x095: // saracca
		    pasmvars->acca >>= (value1 & 63);
		    write_zcr = 0;
		    break;

		    case 0x096: // saraccb
		    pasmvars->accb >>= (value1 & 63);
		    write_zcr = 0;
		    break;

		    case 0x097: // saraccs
		    pasmvars->acca >>= (value1 & 63);
		    pasmvars->accb >>= (value1 & 63);
		    write_zcr = 0;
		    break;

		    case 0x098: // setptra
		    pasmvars->ptra = value1 & 0x3ffff;
		    write_zcr = 0;
		    break;

		    case 0x099: // setptrb
		    pasmvars->ptrb = value1 & 0x3ffff;
		    write_zcr = 0;
		    break;

		    case 0x09a: // addptra
		    pasmvars->ptra = (pasmvars->ptra + value1) & 0x3ffff;
		    write_zcr = 0;
		    break;

		    case 0x09b: // addptrb
		    pasmvars->ptrb = (pasmvars->ptrb + value1) & 0x3ffff;
		    break;

		    case 0x09c: // subptra
		    pasmvars->ptra = (pasmvars->ptra - value1) & 0x3ffff;
		    break;

		    case 0x09d: // subptrb
		    pasmvars->ptrb = (pasmvars->ptrb - value1) & 0x3ffff;
		    break;

		    case 0x09e: // setwide
		    pasmvars->dcachecogaddr = value1 & 0x1f8;
		    break;

		    case 0x09f: // setwidz
		    NotImplemented(instruct);
		    break;

		    case 0x0a0: // setptrx
		    pasmvars->ptrx = value1 & 255;
		    break;

		    case 0x0a1: // setptry
		    pasmvars->ptry = value1 & 255;
		    break;

		    case 0x0a2: // addptrx
		    pasmvars->ptrx = (pasmvars->ptrx + value1) & 255;
		    break;

		    case 0x0a3: // addptry
		    pasmvars->ptry = (pasmvars->ptry + value1) & 255;
		    break;

		    case 0x0a4: // subptrx
		    pasmvars->ptrx = (pasmvars->ptrx - value1) & 255;
		    break;

		    case 0x0a5: // subptry
		    pasmvars->ptry = (pasmvars->ptry - value1) & 255;
		    break;

		    case 0x0a6: // passcnt
		    NotImplemented(instruct);
		    break;

		    case 0x0a7: // wait
		    write_zcr = 0;
		    break;

		    case 0x0a8: // offp
		    write_zcr = 1;
		    dstaddr = ((value1 & 127) >> 5) + REG_DIRA;
		    pasmvars->mem[dstaddr] &= ~(1 << (value1 & 31));
		    dstaddr = ((value1 & 127) >> 5) + REG_OUTA;
		    result = pasmvars->mem[dstaddr] & ~(1 << (value1 & 31));
		    break;

		    case 0x0a9: // notp
		    write_zcr = 1;
		    dstaddr = ((value1 & 127) >> 5) + REG_DIRA;
		    pasmvars->mem[dstaddr] |= (1 << (value1 & 31));
		    dstaddr = ((value1 & 127) >> 5) + REG_OUTA;
		    result = pasmvars->mem[dstaddr] ^ (1 << (value1 & 31));
		    break;

		    case 0x0aa: // clrp
		    write_zcr = 1;
		    dstaddr = ((value1 & 127) >> 5) + REG_DIRA;
		    pasmvars->mem[dstaddr] |= (1 << (value1 & 31));
		    dstaddr = ((value1 & 127) >> 5) + REG_OUTA;
		    result = pasmvars->mem[dstaddr] & ~(1 << (value1 & 31));
		    break;

		    case 0x0ab: // setp
		    write_zcr = 1;
		    dstaddr = ((value1 & 127) >> 5) + REG_DIRA;
		    pasmvars->mem[dstaddr] |= (1 << (value1 & 31));
		    dstaddr = ((value1 & 127) >> 5) + REG_OUTA;
		    result = pasmvars->mem[dstaddr] | (1 << (value1 & 31));
		    break;

		    case 0x0ac: // setpc
		    write_zcr = 1;
		    dstaddr = ((value1 & 127) >> 5) + REG_DIRA;
		    pasmvars->mem[dstaddr] |= (1 << (value1 & 31));
		    dstaddr = ((value1 & 127) >> 5) + REG_OUTA;
		    if (pasmvars->cflag)
		        result = pasmvars->mem[dstaddr] | (1 << (value1 & 31));
		    else
		        result = pasmvars->mem[dstaddr] & ~(1 << (value1 & 31));
		    break;

		    case 0x0ad: // setpnc
		    write_zcr = 1;
		    dstaddr = ((value1 & 127) >> 5) + REG_DIRA;
		    pasmvars->mem[dstaddr] |= (1 << (value1 & 31));
		    dstaddr = ((value1 & 127) >> 5) + REG_OUTA;
		    if (!pasmvars->cflag)
		        result = pasmvars->mem[dstaddr] | (1 << (value1 & 31));
		    else
		        result = pasmvars->mem[dstaddr] & ~(1 << (value1 & 31));
		    break;

		    case 0x0ae: // setpz
		    write_zcr = 1;
		    dstaddr = ((value1 & 127) >> 5) + REG_DIRA;
		    pasmvars->mem[dstaddr] |= (1 << (value1 & 31));
		    dstaddr = ((value1 & 127) >> 5) + REG_OUTA;
		    if (pasmvars->zflag)
		        result = pasmvars->mem[dstaddr] | (1 << (value1 & 31));
		    else
		        result = pasmvars->mem[dstaddr] & ~(1 << (value1 & 31));
		    break;

		    case 0x0af: // setpnz
		    write_zcr = 1;
		    dstaddr = ((value1 & 127) >> 5) + REG_DIRA;
		    pasmvars->mem[dstaddr] |= (1 << (value1 & 31));
		    dstaddr = ((value1 & 127) >> 5) + REG_OUTA;
		    if (!pasmvars->zflag)
		        result = pasmvars->mem[dstaddr] | (1 << (value1 & 31));
		    else
		        result = pasmvars->mem[dstaddr] & ~(1 << (value1 & 31));
		    break;

		    case 0x0b0: // div64d
		    write_zcr = 0;
		    pasmvars->divisor = value1;
		    break;

		    case 0x0b1: // sqrt32
		    write_zcr = 0;
		    pasmvars->mulcount = 16;
                    pasmvars->sqrt = sqrt32(value1);
		    break;

		    case 0x0b2: // qlog
		    NotImplemented(instruct);
		    break;

		    case 0x0b3: // qexp
		    NotImplemented(instruct);
		    break;

		    case 0x0b4: // setqi
		    NotImplemented(instruct);
		    break;

		    case 0x0b5: // setqz
		    NotImplemented(instruct);
		    break;

		    case 0x0b6: // cfgdacs
		    NotImplemented(instruct);
		    break;

		    case 0x0b7: // setdacs
		    NotImplemented(instruct);
		    break;

		    case 0x0b8: // cfgdac0
		    NotImplemented(instruct);
		    break;

		    case 0x0b9: // cfgdac1
		    NotImplemented(instruct);
		    break;

		    case 0x0ba: // cfgdac2
		    NotImplemented(instruct);
		    break;

		    case 0x0bb: // cfgdac3
		    NotImplemented(instruct);
		    break;

		    case 0x0bc: // setdac0
		    NotImplemented(instruct);
		    break;

		    case 0x0bd: // setdac1
		    NotImplemented(instruct);
		    break;

		    case 0x0be: // setdac2
		    NotImplemented(instruct);
		    break;

		    case 0x0bf: // setdac3
		    NotImplemented(instruct);
		    break;

		    default:
		    NotImplemented(instruct);
		    break;
		}
		break;

		case 3:
		write_zcr = 0;
		switch (instruct & 511)
		{
		    case 0x0c0: // setctra
		    NotImplemented(instruct);
		    break;

		    case 0x0c1: // setwava
		    NotImplemented(instruct);
		    break;

		    case 0x0c2: // setfrqa
		    NotImplemented(instruct);
		    break;

		    case 0x0c3: // setphsa
		    NotImplemented(instruct);
		    break;

		    case 0x0c4: // addphsa
		    NotImplemented(instruct);
		    break;

		    case 0x0c5: // subphsa
		    NotImplemented(instruct);
		    break;

		    case 0x0c6: // setvid
		    NotImplemented(instruct);
		    break;

		    case 0x0c7: // setvidy
		    NotImplemented(instruct);
		    break;

		    case 0x0c8: // setctrb
		    NotImplemented(instruct);
		    break;

		    case 0x0c9: // setwavb
		    NotImplemented(instruct);
		    break;

		    case 0x0ca: // setfrqb
		    NotImplemented(instruct);
		    break;

		    case 0x0cb: // setphsb
		    NotImplemented(instruct);
		    break;

		    case 0x0cc: // addphsb
		    NotImplemented(instruct);
		    break;

		    case 0x0cd: // subphsb
		    NotImplemented(instruct);
		    break;

		    case 0x0ce: // setvidi
		    NotImplemented(instruct);
		    break;

		    case 0x0cf: // setvidq
		    NotImplemented(instruct);
		    break;

		    case 0x0d0: // setpix
		    NotImplemented(instruct);
		    break;

		    case 0x0d1: // setpixz
		    NotImplemented(instruct);
		    break;

		    case 0x0d2: // setpixu
		    NotImplemented(instruct);
		    break;

		    case 0x0d3: // setpixv
		    NotImplemented(instruct);
		    break;

		    case 0x0d4: // setpixa
		    NotImplemented(instruct);
		    break;

		    case 0x0d5: // setpixr
		    NotImplemented(instruct);
		    break;

		    case 0x0d6: // setpixg
		    NotImplemented(instruct);
		    break;

		    case 0x0d7: // setpixb
		    NotImplemented(instruct);
		    break;

		    case 0x0d8: // setpora
		    NotImplemented(instruct);
		    break;

		    case 0x0d9: // setporb
		    NotImplemented(instruct);
		    break;

		    case 0x0da: // setporc
		    NotImplemented(instruct);
		    break;

		    case 0x0db: // setpord
		    NotImplemented(instruct);
		    break;

		    case 0x0dc: // push
		    pasmvars->retstack[pasmvars->retptr] = value1;
		    pasmvars->retptr = (pasmvars->retptr + 1) & 3;
		    if (pasmvars->retptr == 0)
			printf("return stack overflow\n");
		    break;

	            case 0x0f4: // jmp
	            case 0x0f5: // jmpd
	            pasmvars->pc = value1 & 0xffff;
		    // Invalidate the instruction pipeline if non-delayed
		    if (!(instruct&1))
		    {
			pasmvars->pc1 |= INVALIDATE_INSTR;
			pasmvars->pc2 |= INVALIDATE_INSTR;
                        pasmvars->pc3 |= INVALIDATE_INSTR;
		    }
	            break;

		    case 0x0f6: // call
		    case 0x0f7: // calld
	            pasmvars->pc = value1 & 0xffff;
		    // Invalidate the instruction pipeline if non-delayed
		    if (!(instruct&1))
		    {
			pasmvars->pc1 |= INVALIDATE_INSTR;
			pasmvars->pc2 |= INVALIDATE_INSTR;
                        pasmvars->pc3 |= INVALIDATE_INSTR;
			pc++;
		    }
		    else
			pc += 4;
		    pc |= (pasmvars->zflag << 17) | (pasmvars->cflag << 16);
		    pasmvars->retstack[pasmvars->retptr] = pc;
		    pasmvars->retptr = (pasmvars->retptr + 1) & 3;
		    if (pasmvars->retptr == 0)
			printf("return stack overflow\n");
	            break;

		    case 0x0f8: // calla
		    NotImplemented(instruct);
		    break;

		    case 0x0f9: // callad
		    NotImplemented(instruct);
		    break;

		    case 0x0fa: // callb
		    NotImplemented(instruct);
		    break;

		    case 0x0fb: // callbd
		    NotImplemented(instruct);
		    break;

		    case 0x0fc: // callx
		    NotImplemented(instruct);
		    break;

		    case 0x0fd: // callxd
		    NotImplemented(instruct);
		    break;

		    case 0x0fe: // cally
		    NotImplemented(instruct);
		    break;

		    case 0x0ff: // callyd
		    NotImplemented(instruct);
		    break;

		    default:
		    NotImplemented(instruct);
		    break;
		}
		break;

		case 4:
		write_zcr &= 6;
		switch (instruct & 511)
		{
		    case 0x100: // reta
		    case 0x101: // retad
		    pasmvars->ptra = (pasmvars->ptra - 4) & 0x3ffff;
		    result = LONG(pasmvars->ptra);
		    cflag = (result >> 16) & 1;
		    zflag = (result >> 17) & 1;
		    pasmvars->pc = result & 0xffff;
	            // Invalidate the instruction pipeline if non-delayed
		    if (!(instruct&1))
		    {
			pasmvars->pc1 |= INVALIDATE_INSTR;
			pasmvars->pc2 |= INVALIDATE_INSTR;
                        pasmvars->pc3 |= INVALIDATE_INSTR;
		    }
		    break;

		    case 0x102: // retb
		    case 0x103: // retbd
		    pasmvars->ptrb = (pasmvars->ptrb - 4) & 0x3ffff;
		    result = LONG(pasmvars->ptrb);
		    cflag = (result >> 16) & 1;
		    zflag = (result >> 17) & 1;
		    pasmvars->pc = result & 0xffff;
	            // Invalidate the instruction pipeline if non-delayed
		    if (!(instruct&1))
		    {
			pasmvars->pc1 |= INVALIDATE_INSTR;
			pasmvars->pc2 |= INVALIDATE_INSTR;
                        pasmvars->pc3 |= INVALIDATE_INSTR;
		    }
		    break;

		    case 0x104: // retx
		    case 0x105: // retxd
		    pasmvars->ptrx = (pasmvars->ptrx - 1) & AUX_MASK;
		    result = pasmvars->auxram[pasmvars->ptrx];
		    cflag = (result >> 16) & 1;
		    zflag = (result >> 17) & 1;
		    pasmvars->pc = result & 0xffff;
	            // Invalidate the instruction pipeline if non-delayed
		    if (!(instruct&1))
		    {
			pasmvars->pc1 |= INVALIDATE_INSTR;
			pasmvars->pc2 |= INVALIDATE_INSTR;
                        pasmvars->pc3 |= INVALIDATE_INSTR;
		    }
		    break;

		    case 0x106: // rety
		    case 0x107: // retyd
		    pasmvars->ptry = (pasmvars->ptry - 1) & AUX_MASK;
		    result = pasmvars->auxram[pasmvars->ptry^255];
		    cflag = (result >> 16) & 1;
		    zflag = (result >> 17) & 1;
		    pasmvars->pc = result & 0xffff;
	            // Invalidate the instruction pipeline if non-delayed
		    if (!(instruct&1))
		    {
			pasmvars->pc1 |= INVALIDATE_INSTR;
			pasmvars->pc2 |= INVALIDATE_INSTR;
                        pasmvars->pc3 |= INVALIDATE_INSTR;
		    }
		    break;

		    case 0x108: // ret
		    case 0x109: // retd
		    pasmvars->retptr = (pasmvars->retptr - 1) & 3;
		    result = pasmvars->retstack[pasmvars->retptr];
		    cflag = (result >> 16) & 1;
		    zflag = (result >> 17) & 1;
		    pasmvars->pc = result & 0xffff;
		    if (pasmvars->retptr == 3)
			printf("return stack underflow\n");
	            // Invalidate the instruction pipeline if non-delayed
		    if (!(instruct&1))
		    {
			pasmvars->pc1 |= INVALIDATE_INSTR;
			pasmvars->pc2 |= INVALIDATE_INSTR;
                        pasmvars->pc3 |= INVALIDATE_INSTR;
		    }
		    break;

		    case 0x10a: // polctra
		    NotImplemented(instruct);
		    break;

		    case 0x10b: // polctrb
		    NotImplemented(instruct);
		    break;

		    case 0x10c: // polvid
		    NotImplemented(instruct);
		    break;

		    case 0x10d: // capctra
		    NotImplemented(instruct);
		    write_zcr = 0;
		    break;

		    case 0x10e: // capctrb
		    NotImplemented(instruct);
		    write_zcr = 0;
		    break;

		    case 0x10f: // capctrs
		    NotImplemented(instruct);
		    write_zcr = 0;
		    break;

		    case 0x110: // setpixw
		    NotImplemented(instruct);
		    write_zcr = 0;
		    break;

		    case 0x111: // clracca
		    pasmvars->acca = 0;
		    write_zcr = 0;
		    break;

		    case 0x112: // clraccb
		    pasmvars->accb = 0;
		    write_zcr = 0;
		    break;

		    case 0x113: // clraccs
		    pasmvars->acca = 0;
		    pasmvars->accb = 0;
		    write_zcr = 0;
		    break;

		    case 0x114: // chkptrx
		    zflag = (pasmvars->ptrx == 0);
		    cflag = (pasmvars->ptrx >> 7) & 1;
		    break;

		    case 0x115: // chkptry
		    zflag = (pasmvars->ptry == 0);
		    cflag = (pasmvars->ptry >> 7) & 1;
		    break;

		    case 0x116: // synctra
		    NotImplemented(instruct);
		    write_zcr = 0;
		    break;

		    case 0x117: // synctrb
		    NotImplemented(instruct);
		    write_zcr = 0;
		    break;

		    case 0x118: // dcachex
		    pasmvars->dcachehubaddr = 0xffffffff;
		    write_zcr = 0;
		    break;

		    case 0x119: // icachex
		    pasmvars->icachehubaddr[0] = 0xffffffff;
		    pasmvars->icachehubaddr[1] = 0xffffffff;
		    pasmvars->icachehubaddr[2] = 0xffffffff;
		    pasmvars->icachehubaddr[3] = 0xffffffff;
		    write_zcr = 0;
		    break;

		    case 0x11a: // icachep
		    pasmvars->prefetch = 1;
		    write_zcr = 0;
		    break;

		    case 0x11b: // icachen
		    pasmvars->prefetch = 0;
		    write_zcr = 0;
		    break;

		    default:
		    NotImplemented(instruct);
		    write_zcr = 0;
		    break;
		}
		break;

		default:
		NotImplemented(instruct);
		break;
	    }
        }
    }

    // Conditionally update flags and write result
    if (write_zcr & 4)
    {
	pasmvars->zflag = zflag;
        if (pasmvars->printflag > 1) fprintf(tracefile, ", z = %d", zflag);
    }
    if (write_zcr & 2)
    {
	pasmvars->cflag = cflag;
        if (pasmvars->printflag > 1) fprintf(tracefile, ", c = %d", cflag);
    }
    if (write_zcr & 1)
    {
        pasmvars->lastd = result;
	if ((dstaddr & 0x1f8) == pasmvars->dcachecogaddr)
	{
	    pasmvars->dcache[dstaddr & 7] = result;
            if (pasmvars->printflag > 1)
	      fprintf(tracefile, ", dcache[%d] = %x", dstaddr & 7, result);
	}
	else
	{
	    pasmvars->mem[dstaddr] = result;
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", cram[%x] = %x", dstaddr, result);
	}
	// Check if we need to update the pins
	if (dstaddr >= REG_OUTA && dstaddr <= REG_DIRD) UpdatePins2();
    }
    if (pasmvars->waitflag)
    {
        fprintf(tracefile, "XXXXXXXXXX BAD XXXXXXXXXXXXXXX\n");
	pasmvars->waitflag--;
	if (!pasmvars->waitflag) pasmvars->waitmode = 0;
    }
    return breakflag;
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
