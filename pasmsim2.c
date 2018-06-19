/*******************************************************************************
' Author: Dave Hein
' Version 0.21
' Copyright (c) 2010 - 2015
' See end of file for terms of use.
'******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "interp.h"
#include "spinsim.h"

#define NUM_ICACHE 1
#define ADDR_MASK 0xfffff

#define IGNORE_WZ_WC
#define PRINT_RAM_ACCESS
//#define PRINT_STREAM_FIFO

#define REG_IJMP3 0x1f0
#define REG_IRET3 0x1f1
#define REG_IJMP2 0x1f2
#define REG_IRET2 0x1f3
#define REG_IJMP1 0x1f4
#define REG_IRET1 0x1f5
#define REG_PA    0x1f6
#define REG_PB    0x1f7
#define REG_PTRA  0x1f8
#define REG_PTRB  0x1f9
#define REG_DIRA  0x1fa
#define REG_DIRB  0x1fb
#define REG_OUTA  0x1fc
#define REG_OUTB  0x1fd
#define REG_PINA  0x1fe
#define REG_PINB  0x1ff

extern char *hubram;
extern int32_t memsize;
extern char lockstate[16];
extern char lockalloc[16];
extern PasmVarsT PasmVars[16];
extern int32_t pasmspin;
extern int32_t cycleaccurate;
extern int32_t loopcount;
extern int32_t pin_val_a;
extern int32_t pin_val_b;
extern SerialT serial_in;
extern SerialT serial_out;
extern int32_t nohubslots;
extern FILE *tracefile;
extern int32_t kludge;

static int wrl_flags0 = 0;
static int wrl_flags1 = 0;
static int rdl_flags0 = 0;
static int rdl_flags1 = 0;

char *GetOpname2(unsigned int, int *, int *, int *);

static void NotImplemented(int instruction)
{
    int dummy;
    char *opname = GetOpname2(instruction, &dummy, &dummy, &dummy);
    printf("%s%s not implemented - %8.8x%s", NEW_LINE, opname, instruction, NEW_LINE);
    spinsim_exit(1);
}

static int32_t getpinval(int32_t val)
{
    int pin_num = val & 63;
    if (pin_num < 32) return (pin_val_a >> pin_num) & 1;
    return (pin_val_b >> (pin_num - 32)) & 1;
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

static uint32_t sqrt64(uint64_t y)
{
    uint64_t x, t1;

    x = 0;
    t1 = 1;
    //t1 <<= 60;
    t1 <<= 62;
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

static int32_t seuss(int32_t value, int32_t forward)
{
    uint32_t i, x;
    uint8_t bitnum[32] = {
        11,  5, 18, 24, 27, 19, 20, 30, 28, 26, 21, 25,  3,  8,  7, 23,
        13, 12, 16,  2, 15,  1,  9, 31,  0, 29, 17, 10, 14,  4,  6, 22};

    if (forward)
    {
        x = 0x354dae51;
        for (i = 0; i < 32; i++)
        {
            if (value & (1 << i))
                x ^= (1 << bitnum[i]);
        }
    }
    else
    {
        x = 0xeb55032d;
        for (i = 0; i < 32; i++)
        {
            if (value & (1 << bitnum[i]))
                x ^= (1 << i);
        }
    }
    return x;
}

#if 0
static int32_t CheckWaitPin(PasmVarsT *pasmvars, int32_t instruct, int32_t value1, int32_t value2)
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
#endif

static void CheckWaitIntState(PasmVarsT *pasmvars)
{
//printf("\nCheckWaitIntState: intflags = %8.8x\n", pasmvars->intflags);
    // Check if interrupts disabled or INT1 active
    if (pasmvars->intstate & 3) return;

    // Check INT1
    if (pasmvars->intenable1)
    {
        int bitmask = (1 << pasmvars->intenable1);
        if (pasmvars->intflags & bitmask)
        {
            pasmvars->intflags |= 1;
//printf("INT1\n");
            return;
        }
    }

    // Check if interrupts disabled or INT1 or INT2 active
    if (pasmvars->intstate & 7) return;

    // Check INT2
    if (pasmvars->intenable2)
    {
        int bitmask = (1 << pasmvars->intenable2);
        if (pasmvars->intflags & bitmask)
        {
//printf("INT2\n");
            pasmvars->intflags |= 1;
            return;
        }
    }

    // Check if interrupts disabled or INT1, INT2 or INT3 active
    if (pasmvars->intstate & 15) return;

    // Check INT3
    if (pasmvars->intenable3)
    {
        int bitmask = (1 << pasmvars->intenable3);
        if (pasmvars->intflags & bitmask)
        {
//printf("INT3\n");
            pasmvars->intflags |= 1;
            return;
        }
    }
}

static int32_t stream_fifo_level(PasmVarsT *pasmvars)
{
    return (pasmvars->str_fifo_windex - pasmvars->str_fifo_rindex) & 15;
}

#define INSTR_MASK1    0x0fe001ff
#define INSTR_MASK2    0x0fe3e1ff
#define INSTR_LOCKNEW  0x0d600004
#define INSTR_LOCKSET  0x0d600007
#define INSTR_QLOG     0x0d60000e
#define INSTR_QEXP     0x0d60000f
#define INSTR_RFBYTE   0x0d600010
#define INSTR_RFWORD   0x0d600011
#define INSTR_RFLONG   0x0d600012
#define INSTR_WFBYTE   0x0d600015
#define INSTR_WFWORD   0x0d600016
#define INSTR_WFLONG   0x0d600017
#define INSTR_GETQX    0x0d600018
#define INSTR_GETQY    0x0d600019
#define INSTR_WAITX    0x0d60001f
#define INSTR_WAITXXX  0x0d602024
#define OPCODE_WMLONG  0x53
#define OPCODE_RDBYTE  0x56
#define OPCODE_RDWORD  0x57
#define OPCODE_RDLONG  0x58
#define OPCODE_CALLD   0x59
#define OPCODE_CALLPB  0x5e
#define OPCODE_JINT    0x5f
#define OPCODE_WRBYTE  0x62
#define OPCODE_WRWORD  0x62
#define OPCODE_WRLONG  0x63
#define OPCODE_QMUL    0x68
#define OPCODE_QVECTOR 0x6a

static int32_t CheckWaitFlag2(PasmVarsT *pasmvars, int instruct, int value1, int value2, int streamflag)
{
    int32_t hubcycles;
    int32_t waitmode = 0;
    int32_t waitflag = pasmvars->waitflag;
    int32_t opcode = (instruct >> 21) & 127;
    int32_t czi = (instruct >> 18) & 7;
    int32_t hubop = 0;
    int32_t instruct1 = (instruct & INSTR_MASK1);
    int32_t instruct2 = (instruct & INSTR_MASK2);
    int32_t temp;
    //int32_t srcaddr = instruct & 0x1ff;

    //printf("CheckWaitFlag2: waitflag = %d, instruct = %8.8x\n", waitflag, instruct);

    if (waitflag)
    {
        if (pasmvars->waitmode == WAIT_CACHE)
        {
            printf("We shouldn't be here!%s", NEW_LINE);
            return 0;
        }

        if (pasmvars->waitmode == WAIT_CORDIC)
        {
            if (instruct1 == INSTR_GETQX) // getqx
            {
                if (pasmvars->qxposted) waitflag = 0;
            }
            else if (instruct1 == INSTR_GETQY) // getqy
            {
                if (pasmvars->qyposted) waitflag = 0;
            }
	    pasmvars->waitflag = waitflag;
	    if (waitflag == 0) pasmvars->waitmode = 0;
	    return waitflag;
        }

        if (pasmvars->waitmode != WAIT_FLAG)
        {
	    waitflag--;
	    if (waitflag == 0)
            {
                if (pasmvars->waitmode == WAIT_HUB && streamflag)
	            waitflag = 16;
                else
                    pasmvars->waitmode = 0;
            }
	    pasmvars->waitflag = waitflag;
	    return waitflag;
        }
    }

    if (pasmvars->rwrep)
    {
         if (!streamflag) return 0;
         //printf("Better wait\n");
         pasmvars->waitmode = WAIT_HUB;
         pasmvars->waitflag = 16;
         return pasmvars->waitflag;
    }

    if (nohubslots)
	hubcycles = 0;
    else
	hubcycles = (pasmvars->cogid + (value2 >> 2) - loopcount) & 15;

    if (instruct1 == INSTR_GETQX) // getqx
    {
        if (!pasmvars->qxposted && pasmvars->cordic_count)
        {
            waitflag = 1;
            waitmode = WAIT_CORDIC;
        }
    }
    else if (instruct1 == INSTR_GETQY) // getqy
    {
        if (!pasmvars->qyposted && pasmvars->cordic_count)
        {
            waitflag = 1;
            waitmode = WAIT_CORDIC;
        }
    }
    else if (instruct2 == INSTR_WAITXXX) // waitxxx
    {
        if ((instruct & 0x3ffff) == 0x02024) // waitint
            CheckWaitIntState(pasmvars);
        temp = 1 << ((instruct >> 9) & 15);
        if (pasmvars->intflags & temp)
        {
	    waitmode = 0;
	    waitflag = 0;
        }
        else
        {
	    waitmode = WAIT_FLAG;
	    waitflag = 1;
        }
    }
    else if (instruct1 == INSTR_WAITX) // waitx
    {
	waitmode = WAIT_CNT;
	waitflag = value1;
    }
    else if (opcode >= OPCODE_RDBYTE && opcode <= OPCODE_RDLONG) // rdxxxx
    {
	hubop = 1;
	waitmode = WAIT_HUB;
	waitflag = hubcycles + 2;
    }
    else if (opcode == OPCODE_WRBYTE || (opcode == OPCODE_WRLONG && (czi&4) == 0) || (opcode == OPCODE_WMLONG && (czi>>1) == 3)) // wrxxxx
    {
	hubop = 1;
	waitmode = WAIT_HUB;
	waitflag = hubcycles + 2;
    }
    else if ((opcode >= OPCODE_QMUL && opcode <= OPCODE_QVECTOR) || instruct1 == INSTR_QLOG || instruct1 == INSTR_QEXP) // qxxxx
    {
	hubop = 1;
	waitmode = WAIT_HUB;
	waitflag = ((pasmvars->cogid - loopcount) & 15) + 1;
    }
    else if (instruct1 >= INSTR_LOCKNEW && instruct1 <= INSTR_LOCKSET) // lockxxx
    {
	hubop = 1;
	waitmode = WAIT_HUB;
	waitflag = ((pasmvars->cogid - loopcount) & 15) + 1;
    }
    else if (instruct1 >= INSTR_RFBYTE && instruct1 <= INSTR_RFLONG) // rfxxxx
    {
        if (stream_fifo_level(pasmvars) < 2)
        {
            waitmode = WAIT_CACHE;
            waitflag = 1;
        }
        else
            waitflag = 0;
    }
    else if (instruct1 >= INSTR_WFBYTE && instruct1 <= INSTR_WFLONG) // wfxxxx
    {
        if (stream_fifo_level(pasmvars) >= 15)
        {
            waitmode = WAIT_CACHE;
            waitflag = 1;
        }
        else
            waitflag = 0;
    }
    else
        waitflag = 0;

    pasmvars->waitflag = waitflag;
    pasmvars->waitmode = waitmode;

    return hubop;
}

// Compute the hub RAM address from the pointer instruction - 1SUPIIIII
static int32_t GetPointer(PasmVarsT *pasmvars, int32_t ptrinst, int32_t size)
{
    int32_t address = 0; // Set to zero to avoid compiler warning
    int32_t offset = (ptrinst << 27) >> (27 - size);
    //printf("GetPointer: ptrinst = %x, case = %d, size = %d, offset = %d\n",
        //ptrinst, (ptrinst >> 5) & 7, size, offset);

    switch ((ptrinst >> 5) & 7)
    {
        case 0: // ptra[offset]
        address = (pasmvars->ptra + offset) & ADDR_MASK;
        break;

        case 1: // ptra
        address = pasmvars->ptra;
        break;

        case 2: // ptra[++offset]
        address = (pasmvars->ptra + offset) & ADDR_MASK;
        //pasmvars->ptra = address;
        pasmvars->ptra0 = address;
        break;

        case 3: // ptra[offset++]
        address = pasmvars->ptra;
        //pasmvars->ptra = (pasmvars->ptra + offset) & ADDR_MASK;
        pasmvars->ptra0 = (pasmvars->ptra + offset) & ADDR_MASK;
        break;

        case 4: // ptrb[offset]
        address = (pasmvars->ptrb + offset) & ADDR_MASK;
        break;

        case 5: // ptrb
        address = pasmvars->ptrb;
        break;

        case 6: // ptrb[++offset]
        address = (pasmvars->ptrb + offset) & ADDR_MASK;
        //pasmvars->ptrb = address;
        pasmvars->ptrb0 = address;
        break;

        case 7: // ptrb[offset++]
        address = pasmvars->ptrb;
        //pasmvars->ptrb = (pasmvars->ptrb + offset) & ADDR_MASK;
        pasmvars->ptrb0 = (pasmvars->ptrb + offset) & ADDR_MASK;
        break;
    }

    return address;
}

void UpdatePins2(void)
{
    int32_t i;
    int32_t mask1;
    int32_t val;
    int32_t mask;

    val = mask = 0;
    for (i = 0; i < 16; i++)
    {
	if (PasmVars[i].state)
	{
	    mask1 = PasmVars[i].mem[REG_DIRA]; // dira
	    val |= mask1 & PasmVars[i].mem[REG_OUTA]; // outa
	    mask |= mask1;
	}
    }
    pin_val_a = (~mask) | val;

    val = mask = 0;
    for (i = 0; i < 16; i++)
    {
	if (PasmVars[i].state)
	{
	    mask1 = PasmVars[i].mem[REG_DIRB]; // dirb
	    val |= mask1 & PasmVars[i].mem[REG_OUTB]; // outb
	    mask |= mask1;
	}
    }
    pin_val_b = (~mask) | val;
    //printf("UpdatePins2: %8.8x %8.8x\n", pin_val_b, pin_val_a);
}

static void SaveRegisters(PasmVarsT *pasmvars)
{
    pasmvars->ptra0 = pasmvars->ptra;
    pasmvars->ptrb0 = pasmvars->ptrb;
}

static void UpdateRegisters(PasmVarsT *pasmvars)
{
    pasmvars->ptra = pasmvars->ptra0;
    pasmvars->ptrb = pasmvars->ptrb0;
}

static void start_fast_mode(PasmVarsT *pasmvars, int32_t addr0, int32_t addr1, int32_t mode)
{
    pasmvars->str_fifo_mode = mode;
    pasmvars->str_fifo_tail_addr = addr0;
    pasmvars->str_fifo_head_addr = addr0;
    pasmvars->str_fifo_rindex = pasmvars->str_fifo_windex;
    pasmvars->str_fifo_addr0 = addr0;
    pasmvars->str_fifo_addr1 = addr1;
    pasmvars->str_fifo_work_flag = 0;
}

static void check_hubexec_mode(PasmVarsT *pasmvars)
{
    if (pasmvars->pc & 0xffc00)
    {
        start_fast_mode(pasmvars, pasmvars->pc, pasmvars->pc, 3);
        pasmvars->waitmode = WAIT_CACHE;
        pasmvars->waitflag = 1;
    }
    else if (pasmvars->str_fifo_mode == 3)
        pasmvars->str_fifo_mode = 0;
}

#if 0
static int32_t CheckForHubExecWait(PasmVarsT *pasmvars)
{
    if (stream_fifo_level(pasmvars) == 0 || pasmvars->str_fifo_tail_addr != pasmvars->pc || pasmvars->str_fifo_mode != 3)
    {
        check_hubexec_mode(pasmvars);
#if 0
        start_fast_mode(pasmvars, pasmvars->pc, pasmvars->pc, 3);
        pasmvars->waitmode = WAIT_CACHE;
        pasmvars->waitflag = 1;
#endif
        return 1;
    }
    return 0;
}
#endif

static int32_t check_read_stream_fifo_level(PasmVarsT *pasmvars, char *ptr)
{
    if (stream_fifo_level(pasmvars) == 0)
    {
        printf("%s: ERROR! level = %d, addr = %8.8x%s", ptr,
            stream_fifo_level(pasmvars), pasmvars->str_fifo_tail_addr, NEW_LINE);
        return 1;
    }
    return 0;
}

static int32_t read_stream_fifo(PasmVarsT *pasmvars, int target_addr)
{
    int32_t index;

    if (check_read_stream_fifo_level(pasmvars, "read_stream_fifo")) return 0;
    if (target_addr >= 0 && pasmvars->str_fifo_tail_addr != target_addr)
    {
        printf("read_stream_fifo: ERROR! level = %d, addr = %8.8x, target = %8.8x%s",
            stream_fifo_level(pasmvars), pasmvars->str_fifo_tail_addr, target_addr, NEW_LINE);
        return 0;
    }
    index = pasmvars->str_fifo_rindex;
    pasmvars->str_fifo_rindex = (index + 1) & 15;
    pasmvars->str_fifo_tail_addr += 4;
    if (pasmvars->str_fifo_tail_addr == pasmvars->str_fifo_addr1)
        pasmvars->str_fifo_tail_addr = pasmvars->str_fifo_addr0;
    return pasmvars->str_fifo_buffer[index];
}

static int32_t read_stream_fifo_byte(PasmVarsT *pasmvars)
{
    int bytenum = pasmvars->str_fifo_tail_addr & 3;
    int index = pasmvars->str_fifo_rindex;
    uint8_t *bufptr = (uint8_t *)&pasmvars->str_fifo_buffer[index];
    if (check_read_stream_fifo_level(pasmvars, "read_stream_fifo_byte")) return 0;
    if (bytenum == 3) pasmvars->str_fifo_rindex = (index + 1) & 15;
    pasmvars->str_fifo_tail_addr++;
    if (pasmvars->str_fifo_tail_addr == pasmvars->str_fifo_addr1)
        pasmvars->str_fifo_tail_addr = pasmvars->str_fifo_addr0;
    return bufptr[bytenum];
}

static int32_t read_stream_fifo_word(PasmVarsT *pasmvars)
{
    int value;
    value = read_stream_fifo_byte(pasmvars);
    value |= read_stream_fifo_byte(pasmvars) << 8;
    return value;
}

static int32_t read_stream_fifo_long(PasmVarsT *pasmvars)
{
    int value;
    if (pasmvars->str_fifo_tail_addr & 3)
    {
        value = read_stream_fifo_word(pasmvars);
        value |= read_stream_fifo_word(pasmvars) << 16;
    }
    else
        value = read_stream_fifo(pasmvars, -1);
    return value;
}

static void write_stream_fifo(PasmVarsT *pasmvars, int value)
{
    int32_t index;

    if (stream_fifo_level(pasmvars) >= 15)
    {
        printf("write_stream_fifo: ERROR! level = %d, addr = %8.8x%s",
            stream_fifo_level(pasmvars), pasmvars->str_fifo_head_addr, NEW_LINE);
        return;
    }
    index = pasmvars->str_fifo_windex;
    pasmvars->str_fifo_buffer[index] = value;
    pasmvars->str_fifo_windex = (index + 1) & 15;
    pasmvars->str_fifo_head_addr += 4;
    if (pasmvars->str_fifo_head_addr == pasmvars->str_fifo_addr1)
        pasmvars->str_fifo_head_addr = pasmvars->str_fifo_addr0;
}

static void write_stream_fifo_byte(PasmVarsT *pasmvars, int value)
{
    int bytenum = pasmvars->str_fifo_head_addr & 3;
    int shift = bytenum * 8;
    int mask = 255 << shift;
    int work_word;

    if (stream_fifo_level(pasmvars) >= 15)
    {
        printf("write_stream_fifo_byte: ERROR! level = %d, addr = %8.8x%s",
            stream_fifo_level(pasmvars), pasmvars->str_fifo_head_addr, NEW_LINE);
        return;
    }

    if (!pasmvars->str_fifo_work_flag)
        work_word = LONG(pasmvars->str_fifo_head_addr);
    else
        work_word = pasmvars->str_fifo_work_word;


    work_word = (work_word & ~mask) | ((value << shift) & mask);

    //printf("work_word = %8.8x, shift = %x, mask = %8.8x, addr = %8.8x\n",
        //work_word, shift, mask, pasmvars->str_fifo_head_addr);

    pasmvars->str_fifo_head_addr++;
    if (pasmvars->str_fifo_head_addr == pasmvars->str_fifo_addr1)
        pasmvars->str_fifo_head_addr = pasmvars->str_fifo_addr0;

    if (bytenum == 3)
    {
        int index = pasmvars->str_fifo_windex;
        pasmvars->str_fifo_buffer[index] = work_word;
        pasmvars->str_fifo_windex = (index + 1) & 15;
        pasmvars->str_fifo_work_flag = 0;
    }
    else
    {
        pasmvars->str_fifo_work_word = work_word;
        pasmvars->str_fifo_work_flag = 1;
    }
}

static void write_stream_fifo_word(PasmVarsT *pasmvars, int value)
{
    write_stream_fifo_byte(pasmvars, value);
    write_stream_fifo_byte(pasmvars, value >> 8);
}

static void write_stream_fifo_long(PasmVarsT *pasmvars, int value)
{
    if (pasmvars->str_fifo_head_addr & 3)
    {
        write_stream_fifo_byte(pasmvars, value);
        write_stream_fifo_byte(pasmvars, value >> 16);
    }
    else
    {
        write_stream_fifo(pasmvars, value);
    }
}

static int32_t CheckStreamFifo(PasmVarsT *pasmvars)
{
    int addr, index, value, level;

    if (pasmvars->str_fifo_mode == 0) return 0;
    level = stream_fifo_level(pasmvars);

    if (pasmvars->str_fifo_mode == 1 || pasmvars->str_fifo_mode == 3)
    {
        if (level >= 10) return 0;

        index = pasmvars->str_fifo_windex;
        addr = pasmvars->str_fifo_head_addr;
        value = LONG(addr);

        if ((pasmvars->cogid + (addr >> 2) - loopcount) & 15) return 0;

        if (pasmvars->printflag == 0xf)
            printf("Cog %2d: %8.8x fifo[%d] = hram[%x] = %8.8x%s",
            pasmvars->cogid, loopcount, level, addr, value, NEW_LINE);

        write_stream_fifo(pasmvars, value);
    }
    else if (pasmvars->str_fifo_mode == 2)
    {
        if (level == 0 && pasmvars->str_fifo_work_flag == 0) return 0;

        addr = pasmvars->str_fifo_tail_addr;
        if ((pasmvars->cogid + (addr >> 2) - loopcount) & 15) return 0;

        if (level)
        {
            index = pasmvars->str_fifo_rindex;
            value = pasmvars->str_fifo_buffer[index];

            if (pasmvars->printflag == 0xf)
                printf("Cog %2d: %8.8x hram[%x] = fifo[%d] = %8.8x%s",
                pasmvars->cogid, loopcount, addr, level, value, NEW_LINE);

            LONG(addr) = read_stream_fifo(pasmvars, -1);
        }
        else
        {
            value = pasmvars->str_fifo_work_word;

            if (pasmvars->printflag == 0xf)
                printf("Cog %2d: %8.8x hram[%x] = workword = %8.8x%s",
                pasmvars->cogid, loopcount, addr, value, NEW_LINE);

            LONG(addr) = pasmvars->str_fifo_work_word;
            pasmvars->str_fifo_work_flag = 0;
        }
    }
    return 1;
}

#if 0
static void CheckSerialPorts(PasmVarsT *pasmvars)
{
    int addr, value1, value2;

    if (pasmvars->serina.mode)
        SerialReceive(&pasmvars->serina, pin_val);

    if (pasmvars->serinb.mode)
        SerialReceive(&pasmvars->serinb, pin_val);

    if (pasmvars->serouta.mode)
    {
        addr = ((pasmvars->serouta.pin_num >> 5) & 3) + REG_OUTA;
        value1 = pasmvars->mem[addr];
        value2 = SerialSend(&pasmvars->serouta, value1);
        if (value1 != value2)
        {
            pasmvars->mem[addr] = value2;
	    UpdatePins2();
        }
    }

    if (pasmvars->seroutb.mode)
    {
        addr = ((pasmvars->seroutb.pin_num >> 5) & 3) + REG_OUTA;
        value1 = pasmvars->mem[addr];
        value2 = SerialSend(&pasmvars->seroutb, value1);
        if (value1 != value2)
        {
            pasmvars->mem[addr] = value2;
	    UpdatePins2();
        }
    }
}
#endif

void UpdateRWlongFlags(void)
{
    rdl_flags1 = rdl_flags0;
    wrl_flags1 = wrl_flags0;
    rdl_flags0 = 0;
    wrl_flags0 = 0;
}

static void CheckInterruptFlags(PasmVarsT *pasmvars)
{
    int count = GetCnt();
    //if (count == pasmvars->cntreg1) { pasmvars->intflags |= (1 << 1); printf("\nCT1 FLAG SET\n"); printf("intflags = %8.8x\n", pasmvars->intflags); }
    if (count == pasmvars->cntreg1) pasmvars->intflags |= (1 << 1);
    if (count == pasmvars->cntreg2) pasmvars->intflags |= (1 << 2);
    if (count == pasmvars->cntreg3) pasmvars->intflags |= (1 << 3);
    if (pasmvars->pinpatmode)
    {
        switch (pasmvars->pinpatmode)
        {
            case 1:
            if ((pin_val_a & pasmvars->pinpatmask) == pasmvars->pinpattern)
            {
                pasmvars->intflags |= (1 << 4);
                pasmvars->pinpatmode = 0;
            }
            break;
            case 2:
            if ((pin_val_a & pasmvars->pinpatmask) != pasmvars->pinpattern)
            {
                pasmvars->intflags |= (1 << 4);
                pasmvars->pinpatmode = 0;
            }
            break;
            case 3:
            if ((pin_val_b & pasmvars->pinpatmask) == pasmvars->pinpattern)
            {
                pasmvars->intflags |= (1 << 4);
                pasmvars->pinpatmode = 0;
            }
            break;
            case 4:
            if ((pin_val_b & pasmvars->pinpatmask) != pasmvars->pinpattern)
            {
                pasmvars->intflags |= (1 << 4);
                pasmvars->pinpatmode = 0;
            }
            break;
            default:
            pasmvars->pinpatmode = 0;
            break;
        }
    }
    if (pasmvars->pinedge & 0xc0)
    {
        int pin_prev = (pasmvars->pinedge >> 8) & 1;
        if (pin_prev != getpinval(pasmvars->pinedge))
        {
            int pin_mode = (pasmvars->pinedge >> 6) & 3;
#ifdef DEBUG_STUFF
printf("\nPIN EDGE EVENT: mode = %d, prev = %d, curr = %d\n", pin_mode, pin_prev, getpinval(pasmvars->pinedge));
#endif
            if (pin_mode == 3 || (pin_mode == 1 && !pin_prev) || (pin_mode == 2 && pin_prev))
            {
                //pasmvars->pinedge = 0;
                pasmvars->intflags |= (1 << 5);
            }
            pasmvars->pinedge ^= 0x100;
        }
    }
    if (pasmvars->rdl_mask & rdl_flags1)
    {
        //pasmvars->rdl_mask = 0;
        pasmvars->intflags |= (1 << 6);
#ifdef DEBUG_STUFF
        printf("\nSet RDL flag bit.  mask = %8.8x flags = %8.8x\n", pasmvars->rdl_mask, rdl_flags1);
#endif
    }
    if (pasmvars->wrl_mask & wrl_flags1)
    {
        //pasmvars->wrl_mask = 0;
        pasmvars->intflags |= (1 << 7);
#ifdef DEBUG_STUFF
        printf("\nSet WRL flag bit.  mask = %8.8x flags = %8.8x\n", pasmvars->wrl_mask, wrl_flags1);
#endif
    }
    if (pasmvars->lockedge & 0x30)
    {
        int lock_num = (pasmvars->lockedge & 15);
        int lock_prev = (pasmvars->lockedge >> 6) & 1;
        if (lock_prev != lockstate[lock_num])
        {
            int lock_mode = (pasmvars->lockedge >> 4) & 3;
#ifdef DEBUG_STUFF
            printf("\nLOCK EDGE EVENT: mode = %d, prev = %d, curr = %d\n",
                lock_mode, lock_prev, lockstate[lock_num]);
#endif
            if (lock_mode == 3 || (lock_mode == 1 && !lock_prev) || (lock_mode == 2 && lock_prev))
            {
                pasmvars->intflags |= (1 << 8);
            }
            pasmvars->lockedge ^= 0x40;
        }
    }
}

static int CheckForInterrupt(PasmVarsT *pasmvars)
{
    // Defer interrupt if invalidated instruction in pipeline
    if (pasmvars->pc1 & INVALIDATE_INSTR) return 0;

    // Defer interrupt if AUGS or AUGD is pending
    if (pasmvars->augsflag || pasmvars->augdflag) return 0;

    // Defer interrupt if memflag is set
    if (pasmvars->memflag) return 0;

    // Check if interrupts disabled or INT1 active
    if (pasmvars->intstate & 3) return 0;

    // Check INT1
    if (pasmvars->intenable1)
    {
        int bitmask = (1 << pasmvars->intenable1);
        if (pasmvars->intflags & bitmask)
        {
            pasmvars->intflags &= ~bitmask;
            pasmvars->intflags |= 1;
            pasmvars->instruct2 = 0xfabbebf4;
            pasmvars->intstate |= 2;
#ifdef DEBUG_STUFF
printf("%d: INT1 %8.8x\n", pasmvars->cogid, bitmask);
#endif
            return 1;
        }
    }

    // Check if interrupts disabled or INT1 or INT2 active
    if (pasmvars->intstate & 7) return 0;

    // Check INT2
    if (pasmvars->intenable2)
    {
        int bitmask = (1 << pasmvars->intenable2);
        if (pasmvars->intflags & bitmask)
        {
            pasmvars->intflags &= ~bitmask;
            pasmvars->intflags |= 1;
            pasmvars->instruct2 = 0xfabbe7f2;
            pasmvars->intstate |= 4;
#ifdef DEBUG_STUFF
printf("%d: INT2 %8.8x\n", pasmvars->cogid, bitmask);
#endif
            return 1;
        }
    }

    // Check if interrupts disabled or INT1, INT2 or INT3 active
    if (pasmvars->intstate & 15) return 0;

    // Check INT3
    if (pasmvars->intenable3)
    {
        int bitmask = (1 << pasmvars->intenable3);
        if (pasmvars->intflags & bitmask)
        {
            pasmvars->intflags &= ~bitmask;
            pasmvars->intflags |= 1;
            pasmvars->instruct2 = 0xfabbe3f0;
            pasmvars->intstate |= 8;
#ifdef DEBUG_STUFF
printf("%d: INT3 %8.8x\n", pasmvars->cogid, bitmask);
#endif
            return 1;
        }
    }
    return 0;
}

static int32_t read_unaligned_word(int32_t addr)
{
    uint16_t value;
    memcpy(&value, &hubram[MAP_ADDR(addr)], 2);
    return value;
}

static int32_t read_unaligned_long(int32_t addr)
{
    int32_t value;
    memcpy(&value, &hubram[MAP_ADDR(addr)], 4);
    return value;
}

static void write_unaligned_word(int32_t addr, int32_t value)
{
    memcpy(&hubram[MAP_ADDR(addr)], &value, 2);
}

static void write_unaligned_long(int32_t addr, int32_t value)
{
    memcpy(&hubram[MAP_ADDR(addr)], &value, 4);
}

static int32_t start_cordic(PasmVarsT *pasmvars)
{
    int32_t cordic_depth = pasmvars->cordic_depth++;

    if (cordic_depth >= 3)
    {
        printf("%sERROR: CORDIC overflow! - %d%s", NEW_LINE, cordic_depth, NEW_LINE);
        cordic_depth = 2;
    }

    if (!cordic_depth)
    {
        pasmvars->qxposted = 0;
        pasmvars->qyposted = 0;
        pasmvars->cordic_count = 38;
    }
    return cordic_depth;
}

static int32_t ProcessAltiIncrement(int32_t value1, int32_t value2)
{
    int32_t temp, mask;
    int32_t rrr = (value2 >> 6) & 7;
    int32_t ddd = (value2 >> 3) & 7;
    int32_t sss = value2 & 7;

    if (rrr & 2)
    {
        temp = (value2 >> 15) & 7;
        mask = (1 << (9 - temp)) - 1;
        temp = value1 >> 19;
        temp += ((rrr&1)<<1) - 1;
        temp &= mask;
        value1 = (value1 & ~(mask << 19)) | (temp << 19);
    }

    if (ddd & 2)
    {
        temp = (value2 >> 12) & 7;
        mask = (1 << (9 - temp)) - 1;
        temp = value1 >> 9;
        temp += ((ddd&1)<<1) - 1;
        temp &= mask;
        value1 = (value1 & ~(mask << 9)) | (temp << 9);
    }

    if (sss & 2)
    {
        temp = (value2 >> 9) & 7;
        mask = (1 << (9 - temp)) - 1;
        temp = value1;
        temp += ((sss&1)<<1) - 1;
        temp &= mask;
        value1 = (value1 & ~mask) | temp;
    }


    return value1;
}

#if 0
static void ProcessAltiFetch(PasmVarsT *pasmvars)
{
    int32_t instruct = pasmvars->instruct2;
    int32_t value = pasmvars->altivalue;
    int32_t rrr = (pasmvars->altiflag >> 6) & 7;
    int32_t ddd = (pasmvars->altiflag >> 3) & 7;
    int32_t sss = pasmvars->altiflag & 7;

    switch (rrr)
    {
        case 1:
        pasmvars->altrflag = 1;
        pasmvars->altrvalue = -1;
        break;

        case 4:
        case 6:
        case 7:
        pasmvars->altrflag = 1;
        pasmvars->altrvalue = (instruct >> 19) & 0x1ff;
        break;

        case 5:
        instruct = (instruct & 0x3ffff) | (value & ~0x3ffff);
        break;
    }

    switch (ddd)
    {
        case 1:
        instruct = (instruct & ~0x3fe00) | ((value << 9) & 0x3fe00);
        break;

        case 4:
        case 5:
        case 6:
        case 7:
        instruct = (instruct & ~0x3fe00) | (value & 0x3fe00);
        break;
    }

    switch (sss)
    {
        case 1:
        instruct = (instruct & ~0x1ff) | ((value >> 9) & 0x1ff);
        break;

        case 4:
        case 5:
        case 6:
        case 7:
        instruct = (instruct & ~0x1ff) | (value & 0x1ff);
        break;
    }

    pasmvars->instruct2 = instruct;
    pasmvars->altiflag = 0;
}
#endif

int32_t pointer_shift(int32_t instruct)
{
    int32_t opcode = (instruct >> 21) & 127;
    int32_t czi = (instruct >> 18) & 7;

    if (opcode == OPCODE_WMLONG && (czi >> 1) == 3) return 2; // wmlong

    if (opcode == OPCODE_RDBYTE) return 0; // rdbyte
    if (opcode == OPCODE_RDWORD) return 1; // rdword
    if (opcode == OPCODE_RDLONG) return 2; // rdlong

    if (opcode == OPCODE_WRBYTE && (czi >> 2) == 0) return 0; // wrbyte
    if (opcode == OPCODE_WRWORD && (czi >> 2) == 1) return 1; // wrword
    if (opcode == OPCODE_WRLONG && (czi >> 2) == 0) return 2; // wrlong

    return -1;
}

int32_t ModifyFlag(int cflag, int zflag, int func)
{
    int retval;
    switch (func & 15)
    {
        case 0:
        retval = 0;
        break;

        case 1:
        retval = (cflag ^ 1) & (zflag ^ 1);
        break;

        case 2:
        retval = (cflag ^ 1) & zflag;
        break;

        case 3:
        retval = (cflag ^ 1);
        break;

        case 4:
        retval = cflag & (zflag ^ 1);
        break;

        case 5:
        retval = (zflag ^ 1);
        break;

        case 6:
        retval = (cflag != zflag);
        break;

        case 7:
        retval = (cflag ^ 1) | (zflag ^ 1);
        break;

        case 8:
        retval = cflag & zflag;
        break;

        case 9:
        retval = (cflag == zflag);
        break;

        case 10:
        retval = zflag;
        break;

        case 11:
        retval = (cflag ^ 1) | zflag;
        break;

        case 12:
        retval = cflag;
        break;

        case 13:
        retval = cflag | (zflag ^ 1);
        break;

        case 14:
        retval = cflag | zflag;
        break;

        case 15:
        retval = 1;
        break;
    }
    return retval;
}

int32_t GetMixVal(int32_t mode, int32_t var, int32_t sbyte, int32_t dbyte)
{
    int32_t rval;

    switch (mode & 7)
    {
        case 0:
        rval = 0;
        break;

        case 1:
        rval = 0xff;
        break;

        case 2:
        rval = var & 255;
        break;

        case 3:
        rval = (~var) & 255;
        break;

        case 4:
        rval = sbyte & 255;
        break;

        case 5:
        rval = (~sbyte) & 255;
        break;

        case 6:
        rval = dbyte & 255;
        break;

        case 7:
        rval = (~dbyte) & 255;
        break;
    }
    return rval;
}

void CheckSkip(PasmVarsT *pasmvars)
{
    int i;

    if (pasmvars->skip_mode & 1)
    {
        for (i = 0; i < 7; i++)
        {
            if (!(pasmvars->skip_mode & 1)) break;
            pasmvars->pc++;
            pasmvars->skip_mode >>= 1;
        }
        if (pasmvars->skip_mode & 1)
            pasmvars->skip_mask |= 2;
    }
    pasmvars->skip_mode >>= 1;
}

int32_t ExecutePasmInstruction2(PasmVarsT *pasmvars)
{
    int32_t cflag = pasmvars->cflag;
    int32_t zflag = pasmvars->zflag;
    int32_t instruct, pc, cond;
    int32_t opcode, value2, value1, czi;
    int32_t srcaddr, dstaddr, rsltaddr;
    int32_t result = 0;
    int32_t i, temp, write_czr, sflag = 0, dflag = 0, psflag = 0;
    int32_t rflag = 0;
    int32_t returnflag = 0;
    int32_t breakflag = 0;
    int32_t streamflag = 0;
    int32_t pc_incr;
    int32_t memflag = pasmvars->memflag;
    int32_t post_ret = 0;
    int32_t alt_instr = 0;

#if 0
    // Check serial ports
    CheckSerialPorts(pasmvars);
#endif

    // Check interrupt flags
    CheckInterruptFlags(pasmvars);

    // Check if cordic engine working
    if (pasmvars->cordic_count)
    {
//printf("\ncordic_count = %d, cordic_depth = %d\n", pasmvars->cordic_count, pasmvars->cordic_depth);
	pasmvars->cordic_count--;
        if (!pasmvars->cordic_count)
        {
            if (--pasmvars->cordic_depth)
                pasmvars->cordic_count = 16;
            pasmvars->qxreg = pasmvars->qxqueue[0];
            pasmvars->qyreg = pasmvars->qyqueue[0];
            pasmvars->qxposted = 1;
            pasmvars->qyposted = 1;
            for (i = 0; i < pasmvars->cordic_depth; i++)
            {
                pasmvars->qxqueue[i] = pasmvars->qxqueue[i+1];
                pasmvars->qyqueue[i] = pasmvars->qyqueue[i+1];
            }
        }
    }

    // Check if streaming FIFO needs filling
    streamflag = CheckStreamFifo(pasmvars);

    // Check if we can skip further processing if not printing, not waiting
    // for a pin state and wait flag is greater than 1.
    if (pasmvars->waitflag > 1 && !pasmvars->printflag && pasmvars->waitmode != WAIT_PIN)
    {
	pasmvars->waitflag--;
	return 0;
    }

    // Check for cache wait
    if (pasmvars->waitflag && pasmvars->waitmode == WAIT_CACHE)
    {
        if (stream_fifo_level(pasmvars) >= 2)
        {
	    pasmvars->waitflag = 0;
	    pasmvars->waitmode = 0;
        }
        else
        {
            if (pasmvars->printflag)
                DebugPasmInstruction2(pasmvars);
	    return 0;
        }
    }

    // Fetch a new instruction and update the pipeline
    if (!pasmvars->waitflag && pasmvars->phase == 0) // && !pasmvars->rwrep)
    {
        if (CheckForInterrupt(pasmvars))
        {
            if (pasmvars->pc2 & 0xffc00)
                pasmvars->pc2 = (pasmvars->pc1 - 4);
            else
                pasmvars->pc2 = (pasmvars->pc1 - 1);
	    if (pasmvars->printflag) DebugPasmInstruction2(pasmvars);
            pasmvars->phase = 1;
            return 0;
        }

	//if (pasmvars->pc >= 0x400 && CheckForHubExecWait(pasmvars)) return 0;

        pasmvars->instruct2 = pasmvars->instruct1;

#if 0
        // Modify instruction if previous instruction was ALTI
        if (pasmvars->altiflag) ProcessAltiFetch(pasmvars);
#endif

	// Update instruction pipeline and fetch new instruction
	if (pasmvars->pc < 0x200)
        {
            //pasmvars->str_fifo_tail_addr = 0;
            CheckSkip(pasmvars);
            pasmvars->instruct1 = pasmvars->mem[pasmvars->pc];
        }
	else if (pasmvars->pc < 0x400)
        {
            //pasmvars->str_fifo_tail_addr = 0;
            CheckSkip(pasmvars);
            pasmvars->instruct1 = pasmvars->lut[pasmvars->pc - 0x200];
        }
	else
#if 0
            pasmvars->instruct1 = read_stream_fifo(pasmvars, pasmvars->pc);
#else
            pasmvars->instruct1 = read_stream_fifo_long(pasmvars);
#endif

	// Update PC pipeline and increment PC
        pasmvars->pc2 = pasmvars->pc1;
        pasmvars->pc1 = pasmvars->pc;
	if (pasmvars->repcnt && pasmvars->pc >= pasmvars->reptop)
	{
	    if (!pasmvars->repforever) pasmvars->repcnt--;
	    pasmvars->pc = pasmvars->repbot;
            check_hubexec_mode(pasmvars);
	}
	else
        {
            if (pasmvars->pc & 0xffc00)
	        pasmvars->pc = (pasmvars->pc + 4) & ADDR_MASK;
            else
	        pasmvars->pc = (pasmvars->pc + 1) & ADDR_MASK;
        }

	if (pasmvars->printflag) { /*printf(" XXX ");*/ DebugPasmInstruction2(pasmvars); }
        pasmvars->phase = 1;
	return 0;
    }

    // Get the instruction and pc
    instruct = pasmvars->instruct2;
    if (pasmvars->altiflag)
    {
//static int change_count = 0;
//printf("altiflag set.  Changing instruction from %08x", instruct);
        instruct = (instruct & 0x0003ffff) | (pasmvars->altivalue << 18);
//printf(" to %08x\n", instruct);
//if (++change_count > 100) spinsim_exit(1);
    }
    pc = pasmvars->pc2;
    if (pc & 0xffc00)
        pc_incr = 4;
    else
        pc_incr = 1;

    // Return if instruction has been invalidated or skip bit is set and not printing
    if ((pc & INVALIDATE_INSTR) || (pasmvars->skip_mask & 1))
    {
	if (!pasmvars->printflag)
	{
            pasmvars->skip_mask >>= 1;
            pasmvars->phase = 0; // Change phase to fetch mode
	    return 0;
	}
	returnflag = 1;
    }

    // Extract bit fields from the instruction
    opcode     = (instruct >> 21) & 127;
    czi        = (instruct >> 18) & 7;
    cond       = (instruct >> 28) & 15;
    dstaddr    = (instruct >> 9) & 511;
    srcaddr    = instruct & 511;
    rsltaddr   = dstaddr;

    // Check for post return
    if (cond == 0 && instruct != 0)
    {
        cond = 15;
        post_ret = 1;
    }

    // Replace the source, destination or result address if an alternate is set
    if (pasmvars->altsflag)
        srcaddr = pasmvars->altsvalue;
    if (pasmvars->altdflag)
        dstaddr = pasmvars->altdvalue;
    if (pasmvars->altrflag)
        rsltaddr = pasmvars->altrvalue;

    // Decode the immediate flags for the source and destination fields
    if ((czi & 1) && opcode <= 0x6a)
    {
	if (!pasmvars->augsflag && (instruct & 0x100) && ((opcode >= OPCODE_RDBYTE &&
            opcode <= OPCODE_RDLONG) || opcode == OPCODE_WRBYTE || (opcode == OPCODE_WRLONG && !(czi&4))
            || (opcode == OPCODE_WMLONG && (czi>>1) == 3)))
            sflag = 3;
        else if ((opcode >= OPCODE_CALLD && opcode <= OPCODE_CALLPB) || (opcode == OPCODE_JINT && !(czi&4)))
            sflag = 2;
        else
            sflag = 1;
    }

    dflag = ((czi & 1) && (opcode == 0x6b)) ||
            ((czi & 2) && (opcode == 0x5d || opcode == 0x5e || (opcode == 0x5f && (czi & 4)) || (opcode >= 0x60 && opcode <= 0x6a)));

    // Check for extended address instructions
    if (opcode >= 0x6c)
    {
        if (opcode < 0x78)
        {
            srcaddr = instruct & 0xfffff;
            sflag = (czi >> 2) + 1;
            if (opcode >= 0x70)
                rsltaddr = 0x1f6 + (opcode & 3);
        }
        else
        {
            srcaddr = instruct & 0x7fffff;
            sflag = 1;
        }
    }

    // Determine if ptra or ptrb are referenced
    psflag = (sflag == 3);

    // Determine if instruction uses relative address
    rflag = (sflag == 2);

    // Save ptra, ptrb, inda and indb
    SaveRegisters(pasmvars);

    // Return if condition code is not met
    if (!((cond >> ((cflag << 1) | zflag)) & 1))
    {
	if (!pasmvars->printflag)
	{
            pasmvars->skip_mask >>= 1;
            pasmvars->phase = 0; // Change phase to fetch mode
	    return 0;
	}
	returnflag = 1;
    }

    // Set the flag that controls writing the result, zero flag and carry flag
    write_czr = czi | 1; // Assume writing result

    // Get value1 from the destination field
    if (dflag)
    {
	if (pasmvars->augdflag)
	    value1 = (pasmvars->augdvalue << 9) | dstaddr;
	else
	    value1 = dstaddr;
    }
    else if (dstaddr == REG_PINA)
        value1 = pin_val_a;
    else if (dstaddr == REG_PINB)
        value1 = pin_val_b;
    else if (dstaddr == REG_PTRA)
	value1 = pasmvars->ptra;
    else if (dstaddr == REG_PTRB)
	value1 = pasmvars->ptrb;
    else if (memflag == 2)
	value1 = pasmvars->lut[dstaddr+pasmvars->rwrep];
    else
	value1 = pasmvars->mem[dstaddr+pasmvars->rwrep];

    // Get value2 from the source field
    //printf("psflag = %d, sflag = %d, srcaddr = %d\n", psflag, sflag, srcaddr);
    if (pasmvars->altsvflag)
        value2 = pasmvars->altsvvalue;
    else if (psflag)
        value2 = GetPointer(pasmvars, srcaddr, pointer_shift(instruct));
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
        value2 = pin_val_a;
    else if (srcaddr == REG_PINB)
        value2 = pin_val_b;
    else if (srcaddr == REG_PTRA)
	value2 = pasmvars->ptra;
    else if (srcaddr == REG_PTRB)
	value2 = pasmvars->ptrb;
    else
	value2 = pasmvars->mem[srcaddr];

    //if (pasmvars->waitmode == WAIT_FLAG) printf("Trace 1 - waitflag = %d\n", pasmvars->waitflag);

    // Check the wait flag if the return flag is not set
    if (returnflag)
    {
        if (pasmvars->waitmode == WAIT_FLAG) printf("Trace 1a - waitflag = %d\n", pasmvars->waitflag);
        if (pasmvars->waitflag)
	{
	    pasmvars->waitflag--;
	    if (!pasmvars->waitflag) pasmvars->waitmode = 0;
	}
    }
    else
	CheckWaitFlag2(pasmvars, instruct, value1, value2, streamflag);

    //if (pasmvars->waitmode == WAIT_FLAG) printf("Trace 2 - waitflag = %d\n", pasmvars->waitflag);

    // Print instruction if printflag set
    if (pasmvars->printflag) DebugPasmInstruction2(pasmvars);

    // Return if returnflag or waitflag is set
    if (returnflag || pasmvars->waitflag)
    {
        if (returnflag)
        {
            pasmvars->skip_mask >>= 1;
            pasmvars->phase = 0; // Change phase to fetch mode
        }
        return 0;
    }

    // Check sflag and dflag and reset augsflag and augdflag if needed
    if (!memflag)
    {
        if (sflag) pasmvars->augsflag = 0;
        if (dflag) pasmvars->augdflag = 0;
    }


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
    // Clear bits within write_czr to prevent writing variables.
    switch(opcode >> 3) // Decode the four most significant bits
    {
        case 0: // rotate and shift
	value2 &= 0x1f; // Get five LSB's
	switch (opcode)
	{
	    case 0: // ror
	    result = (((uint32_t)value1) >> value2) | (value1 << (32 - value2));
            if (value2)
                value1 >>= (value2 - 1);
            cflag = value1 & 1;
	    break;

	    case 1: // rol
	    result = (((uint32_t)value1) >> (32 - value2)) | (value1 << value2);
            if (value2)
                value1 <<= (value2 - 1);
	    cflag = (value1 >> 31) & 1;
	    break;

	    case 2: // shr
	    result = (((uint32_t)value1) >> value2);
            if (value2)
                value1 >>= (value2 - 1);
            cflag = value1 & 1;
	    break;

	    case 3: // shl
	    result = (value1 << value2);
            if (value2)
                value1 <<= (value2 - 1);
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
            if (value2)
                value1 >>= (value2 - 1);
            cflag = value1 & 1;
	    break;

	    case 5: // rcl
	    result = cflag ? (1 << value2) - 1 : 0;
	    result |= (value1 << value2);
            if (value2)
                value1 <<= (value2 - 1);
	    cflag = (value1 >> 31) & 1;
	    break;

	    case 6: // sar
	    result = value1 >> value2;
            if (value2)
                value1 >>= (value2 - 1);
            cflag = value1 & 1;
	    break;

            case 7: // sal
            result = (value1 << value2);
            if (value1 & 1) result |= (1 << value2) - 1;
            if (value2)
                value1 <<= (value2 - 1);
	    cflag = (value1 >> 31) & 1;
            break;
	}
	zflag = (result == 0);
	break;

        case 1: // addxx, subxx
	switch (opcode & 7)
	{
            int64_t d_result;
	    case 0: // add
	    result = value1 + value2;
	    cflag = (((value1 & value2) | ((value1 | value2) & (~result))) >> 31) & 1;
	    zflag = (result == 0);
	    break;

	    case 1: // addx
	    result = value1 + value2 + cflag;
	    cflag = (((value1 & value2) | ((value1 | value2) & (~result))) >> 31) & 1;
	    zflag = (result == 0) & zflag;
	    break;

	    case 2: // adds
#ifdef OLD_SIGNED_CFLAG
	    result = value1 + value2;
	    cflag = (((~(value1 ^ value2)) & (value1 ^ result)) >> 31) & 1;
#else
            d_result = (int64_t)value1 + (int64_t)value2;
            result = (int32_t)d_result;
            cflag = (int32_t)(d_result >> 32) & 1;
#endif
	    zflag = (result == 0);
            if (kludge) cflag = 0;
	    break;

	    case 3: // addsx
#ifdef OLD_SIGNED_CFLAG
	    result = value1 + value2 + cflag;
            cflag = ((value1 ^ value2) & 0x80000000) ? ((result >> 31) & 1) : ((value1 >> 31) & 1);
#else
            d_result = (int64_t)value1 + (int64_t)value2 + (int64_t)cflag;
            result = (int32_t)d_result;
            cflag = (int32_t)(d_result >> 32) & 1;
#endif
	    zflag = (result == 0) & zflag;
            if (kludge) cflag = 0;
	    break;

	    case 4: // sub
	    result = value1 - value2;
            //printf("result = %d, value1 = %d, value2 = %d\n", result, value1, value2);
	    cflag = ((uint32_t)value1) < ((uint32_t)value2);
	    zflag = (result == 0);
	    break;

	    case 5: // subx
	    result = value1 - value2 - cflag;
	    if (value2 != 0xffffffff || !cflag)
	        cflag = ((uint32_t)value1) < ((uint32_t)(value2 + cflag));
	    zflag = (result == 0) & zflag;
	    break;

	    case 6: // subs
#ifdef OLD_SIGNED_CFLAG
	    result = value1 - value2;
	    cflag = (((value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
#else
            d_result = (int64_t)value1 - (int64_t)value2;
            result = (int32_t)d_result;
            cflag = (int32_t)(d_result >> 32) & 1;
#endif
	    zflag = (result == 0);
            if (kludge) cflag = 0;
	    break;

	    case 7: // subsx
#ifdef OLD_SIGNED_CFLAG
	    result = value1 - value2 - cflag;
	    cflag = (((value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
#else
            d_result = (int64_t)value1 - (int64_t)value2 - (int64_t)cflag;
            result = (int32_t)d_result;
            cflag = (int32_t)(d_result >> 32) & 1;
#endif
	    zflag = (result == 0) & zflag;
            if (kludge) cflag = 0;
	    break;
	}
	break;

        case 2: // cmpxx, subr, cmpsub
	switch (opcode & 7)
	{
	    case 0: // cmp
            //printf("\ncmp $%x $%x\n", value1, value2);
	    result = value1 - value2;
	    zflag = (result == 0);
	    cflag = ((uint32_t)value1) < ((uint32_t)value2);
            write_czr &= 6;
	    break;

	    case 1: // cmpx
	    result = value1 - value2 - cflag;
	    if (value2 != 0xffffffff || !cflag)
	        cflag = ((uint32_t)value1) < ((uint32_t)(value2 + cflag));
	    zflag = (result == 0) & zflag;
            write_czr &= 6;
	    break;

	    case 2: // cmps
            //printf("\ncmps $%x $%x\n", value1, value2);
	    result = value1 - value2;
	    zflag = (result == 0);
	    cflag =  value1 < value2;
            write_czr &= 6;
	    break;

	    case 3: // cmpsx
	    result = value1 - value2 - cflag;
	    cflag = value1 < ((int64_t)value2 + cflag);
	    zflag = (result == 0) & zflag;
            write_czr &= 6;
	    break;

	    case 4: // cmpr
	    result = value2 - value1;
	    zflag = (result == 0);
	    cflag = ((uint32_t)value2) < ((uint32_t)value1);
            write_czr &= 6;
	    break;

	    case 5: // cmpm
	    result = value1 - value2;
	    zflag = (result == 0);
	    cflag =  (result >> 31) & 1;
            write_czr &= 6;
            break;

	    case 6: // subr
	    result = value2 - value1;
	    cflag = ((uint32_t)value2) < ((uint32_t)value1);
	    zflag = (result == 0);
            break;

	    case 7: // cmpsub
	    cflag = (((uint32_t)value1) >= ((uint32_t)value2));
	    result = cflag ? value1 - value2 : value1;
	    zflag = (result == 0);
            break;
	}
	break;

        case 3: // fge, fle, fges, fles, sumxx
        switch (opcode & 7)
        {
            int64_t d_result;
	    case 0: // fge
	    cflag = (((uint32_t)value1) < ((uint32_t)value2));
	    result = cflag ? value2 : value1;
	    break;

	    case 1: // fle
	    cflag = (((uint32_t)value1) > ((uint32_t)value2));
	    result = cflag ? value2 : value1;
	    break;

	    case 2: // fges
	    cflag = (value1 < value2);
	    result = cflag ? value2 : value1;
	    break;

	    case 3: // fles
	    cflag = (value1 > value2);
	    result = cflag ? value2 : value1;
	    break;

	    case 4: // sumc
#ifdef OLD_SIGNED_CFLAG
	    result = cflag ? value1 - value2 : value1 + value2;
	    cflag = (~cflag) << 31;
	    cflag = (((cflag ^ value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
#else
	    d_result = cflag ? (int64_t)value1 - (int64_t)value2 : (int64_t)value1 + (int64_t)value2;
            result = (int32_t)d_result;
            cflag = (int32_t)(d_result >> 32) & 1;
#endif
            if (kludge) cflag = 0;
	    break;

	    case 5: // sumnc
#ifdef OLD_SIGNED_CFLAG
	    result = cflag ? value1 + value2 : value1 - value2;
	    cflag = cflag << 31;
	    cflag = (((cflag ^ value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
#else
	    d_result = cflag ? (int64_t)value1 + (int64_t)value2 : (int64_t)value1 - (int64_t)value2;
            result = (int32_t)d_result;
            cflag = (int32_t)(d_result >> 32) & 1;
#endif
            if (kludge) cflag = 0;
	    break;

	    case 6: // sumz
#ifdef OLD_SIGNED_CFLAG
	    result = zflag ? value1 - value2 : value1 + value2;
	    cflag = (~zflag) << 31;
	    cflag = (((cflag ^ value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
#else
	    d_result = zflag ? (int64_t)value1 - (int64_t)value2 : (int64_t)value1 + (int64_t)value2;
            result = (int32_t)d_result;
            cflag = (int32_t)(d_result >> 32) & 1;
#endif
            if (kludge) cflag = 0;
	    break;

	    case 7: // sumnz
#ifdef OLD_SIGNED_CFLAG
	    result = zflag ? value1 + value2 : value1 - value2;
	    cflag = zflag << 31;
	    cflag = (((cflag ^ value1 ^ value2) & (value1 ^ result)) >> 31) & 1;
#else
	    d_result = zflag ? (int64_t)value1 + (int64_t)value2 : (int64_t)value1 - (int64_t)value2;
            result = (int32_t)d_result;
            cflag = (int32_t)(d_result >> 32) & 1;
#endif
            if (kludge) cflag = 0;
	    break;

        }
        zflag = (result == 0);
	break;

        case 4: // testb, testbn, bitl, bith, bitc, bitnc, bitz, bitnz, bitrnd, bitnot
        //temp = (value2 >> 5) & 7;
	value2 = (value2 & 0x1f);
	//cflag = (value1 >> value2) & 1;
        temp = ((czi >> 1) ^ (czi >> 2)) & 1;
        switch (opcode & 7)
        {
	    case 0: // bitl, testb
            value2 &= 31;
            cflag = zflag = (value1 >> value2) & 1;
            if (temp) // testb
                write_czr &= 6;
            else // bitl
            {
                write_czr |= 1;
                result = value1 & ~(1 << value2);
            }
	    break;

	    case 1: // bith, testbn
            value2 &= 31;
            if (temp) // testbn
            {
                write_czr &= 6;
                cflag = zflag = !((value1 >> value2) & 1);
            }
            else // bith
            {
                write_czr |= 1;
                cflag = zflag = (value1 >> value2) & 1;
                result = value1 | (1 << value2);
            }
	    break;

	    case 2: // bitc, testb
            value2 &= 31;
            if (temp) // testb
            {
                write_czr &= 6;
                cflag &= (value1 >> value2) & 1;
                zflag &= (value1 >> value2) & 1;
            }
            else // bitc
            {
                write_czr |= 1;
                cflag = zflag = (value1 >> value2) & 1;
                result = (value1 & ~(1 << value2)) | (pasmvars->cflag << value2);
            }
	    break;

	    case 3: // bitnc, testbn
            value2 &= 31;
            if (temp) // testbn
            {
                write_czr &= 6;
                cflag &= !((value1 >> value2) & 1);
                zflag &= !((value1 >> value2) & 1);
            }
            else // bitnc
            {
                write_czr |= 1;
                cflag = zflag = (value1 >> value2) & 1;
                result = (value1 & ~(1 << value2)) | ((!pasmvars->cflag) << value2);
            }
	    break;

	    case 4: // bitz, testb
            value2 &= 31;
            if (temp) //testb
            {
                write_czr &= 6;
                cflag |= (value1 >> value2) & 1;
                zflag |= (value1 >> value2) & 1;
            }
            else // bitz
            {
                write_czr |= 1;
                cflag = zflag = (value1 >> value2) & 1;
                result = (value1 & ~(1 << value2)) | (pasmvars->zflag << value2);
            }
	    break;

	    case 5: // bitnz, testbn
            value2 &= 31;
            if (temp) // testbn
            {
                write_czr &= 6;
                cflag |= !((value1 >> value2) & 1);
                zflag |= !((value1 >> value2) & 1);
            }
            else // bitnz
            {
                write_czr |= 1;
                cflag = zflag = (value1 >> value2) & 1;
                result = (value1 & ~(1 << value2)) | ((!pasmvars->zflag) << value2);
            }
	    break;

	    case 6: // bitrnd, testb
            value2 &= 31;
            if (temp) // testb
            {
                write_czr &= 6;
                cflag ^= (value1 >> value2) & 1;
                zflag ^= (value1 >> value2) & 1;
            }
            else // bitrnd
            {
                write_czr |= 1;
                cflag = zflag = (value1 >> value2) & 1;
                NotImplemented(instruct);
            }
	    break;

	    case 7: // bitnot, testbn
            value2 &= 31;
            if (temp) // testbn
            {
                write_czr &= 6;
                cflag ^= !((value1 >> value2) & 1);
                zflag ^= !((value1 >> value2) & 1);
            }
            else // bitnot
            {
                write_czr |= 1;
                cflag = zflag = (value1 >> value2) & 1;
                result = value1 ^ (1 << value2);
            }
	    break;
	}
        break;

        case 5: // and, andn, or, xor, muxxx
	switch (opcode & 7)
	{
	    case 0: // and
	    result = value1 & value2;
	    break;

	    case 1: // andn
	    result = value1 & (~value2);
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

        case 6: // mov, not abs, negxx
	switch (opcode & 7)
	{
	    case 0: // mov
	    result = value2;
	    cflag = (value2 >> 31) & 1;
	    break;

	    case 1: // not
	    result = ~value2;
	    cflag = result < 0;
	    break;

	    case 2: // abs
	    cflag = (value2 >> 31) & 1;
	    result = _abs(value2);
	    break;

	    case 3: // neg
	    result = -value2;
	    cflag = result < 0;
	    break;

	    case 4: // negc
	    result = cflag ? -value2 : value2;
	    cflag = result < 0;
	    break;

	    case 5: // negnc
	    result = cflag ? value2 : -value2;
	    cflag = result < 0;
	    break;

	    case 6: // negz
	    result = zflag ? -value2 : value2;
	    cflag = result < 0;
	    break;

	    case 7: // negnz
	    result = zflag ? value2 : -value2;
	    cflag = result < 0;
	    break;
	}
	zflag = (result == 0);
	break;

        case 7: // incmod, decmod, zerox, signx, encod, ones, test, testn
	switch (opcode & 7)
	{
            case 0: // incmod
	    cflag = (value1 == value2);
	    if (cflag)
	        result = 0;
	    else
	        result = value1 + 1;
	    break;

            case 1: // decmod
	    cflag = (value1 == 0);
	    if (cflag)
	        result = value2;
	    else
	        result = value1 - 1;
	    break;

            case 2: // zerox
            result = value1 & ~(0xfffffffe << (value2 & 31));
            break;

            case 3: // signx
            temp = 31 - (value2 & 31);
            result = (value1 << temp) >> temp;
            cflag = (result >> 31) & 1;
            break;

	    case 4: // encod
            cflag = (value2 != 0);
            for (result = 31; result > 0; result--)
            {
                if (value2 & 0x80000000) break;
                value2 <<= 1;
            }
            if (kludge) cflag = result & 1;
	    break;

            case 5: // ones
            result = 0;
            for (i = 0; i < 32; i++)
            {
                result += (value2 & 1);
                value2 >>= 1;
            }
            cflag = (result & 1);
            break;

	    case 6: // test
	    result = value1 & value2;
	    cflag = parity(result);
            write_czr &= 6;
	    break;

	    case 7: // testn
	    result = value1 & (~value2);
	    cflag = parity(result);
            write_czr &= 6;
	    break;
	}
	zflag = (result == 0);
	break;

        case 8: // setnib, getnib, rolnib, setbyte, getbyte
	write_czr = 1;
	temp = (instruct >> 17) & 0x1c;
        switch(opcode & 7)
        {
            case 0: // setnib
            case 1: // setnib
            if (pasmvars->altnflag)
                temp = pasmvars->altnvalue << 2;
	    result = (value2 & 15) << temp;
//printf("setnib: shift = %d, value = %08x, value1 = %08x, ", temp, result, value1);
	    temp = ~(15 << temp);
	    result |= (value1 & temp);
//printf("result = %08x\n", result);
            break;

            case 2: // getnib
            case 3: // getnib
            if (pasmvars->altnflag)
                temp = pasmvars->altnvalue << 2;
	    result = (value2 >> temp) & 15;
            break;

            case 4: // rolnib
            case 5: // rolnib
            if (pasmvars->altnflag)
                temp = pasmvars->altnvalue << 2;
	    result = (value1 << 4) | ((value2 >> temp) & 15);
            break;

	    case 6: // setbyte
            if (pasmvars->altnflag)
                temp = pasmvars->altnvalue << 2;
            temp = (temp << 1) & 0x18;
	    result = (value2 & 0xff) << temp;
	    temp = ~(0xff << temp);
	    result |= (value1 & temp);
	    break;

	    case 7: // getbyte
            if (pasmvars->altnflag)
                temp = pasmvars->altnvalue << 2;
            temp = (temp << 1) & 0x18;
	    result = (value2 >> temp) & 0xff;
	    break;
	}
	break;

        case 9: // rolbyte, setword, getword, rolword, altxx, setx, decod, bmaks, muxnits, muxnibs, muxq, movbyts
	write_czr = 1;
	temp = (instruct >> 16) & 0x18;
        switch(opcode&7)
        {
            case 0: // rolbyte
            if (pasmvars->altnflag)
                temp = pasmvars->altnvalue << 3;
	    result = (value1 << 8) | ((value2 >> temp) & 255);
            break;

            case 1: // setword, getword
            if (pasmvars->altnflag)
                temp = pasmvars->altnvalue << 3;
            temp = (temp << 1) & 0x10;
	    write_czr &= 3;
            if (czi & 4) // getword
            {
	        result = (value2 >> temp) & 0xffff;
            }
            else // setword
            {
	        result = (value2 & 0xffff) << temp;
	        temp = ~(0xffff << temp);
	        result |= (value1 & temp);
            }
            break;

            case 2: // rolword, altsn, altgn
            write_czr = 1;
            switch(czi >> 1)
            {
                case 0: // rolword
                case 1: // rolword
                if (pasmvars->altnflag)
                    temp = pasmvars->altnvalue;
                else
                    temp = (czi & 2);
                if (temp)
                    result = (value1 << 16) | (((unsigned int)value2) >> 16);
                else
                    result = (value1 << 16) | (value2 & 0xffff);
                break;

	        case 2: // altsn
                pasmvars->altrflag = pasmvars->altnflag = pasmvars->altdflag = 1;
                pasmvars->altnvalue = (value1 & 7);
                pasmvars->altrvalue = pasmvars->altdvalue = ((value1 >> 3) + value2) & 511;
                result = value1 + ((value2 << (31 - 17)) >> (31 - 8));
//printf("altsn: n = %x, d = %x, r = %x\n", pasmvars->altnvalue, pasmvars->altdvalue, result);
                alt_instr = 1;
	        break;

	        case 3: // altgn
                pasmvars->altnflag = pasmvars->altsflag = 1;
                pasmvars->altnvalue = (value1 & 7);
                pasmvars->altsvalue = ((value1 >> 3) + value2) & 511;
                result = value1 + (value2 << ((31 - 17)) >> (31 - 8));
//printf("altgn: n = %x, s = %x, r = %x\n", pasmvars->altnvalue, pasmvars->altsvalue, result);
                alt_instr = 1;
	        break;
            }
            break;

            case 3: // altsb, altgb, altsw, altgw
            write_czr = 1;
            switch(czi >> 1)
            {
                case 0: // altsb
                pasmvars->altrflag = pasmvars->altnflag = pasmvars->altdflag = 1;
                pasmvars->altnvalue = (value1 & 3);
                pasmvars->altrvalue = pasmvars->altdvalue = ((value1 >> 2) + value2) & 511;
                result = value1 + (value2 << ((31 - 17)) >> (31 - 8));
                alt_instr = 1;
	        break;

                case 1: // altgb
                pasmvars->altnflag = pasmvars->altsflag = 1;
                pasmvars->altnvalue = (value1 & 3);
                pasmvars->altsvalue = ((value1 >> 2) + value2) & 511;
                result = value1 + (value2 << ((31 - 17)) >> (31 - 8));
                alt_instr = 1;
                break;

                case 2: // altsw
                pasmvars->altrflag = pasmvars->altnflag = pasmvars->altdflag = 1;
                pasmvars->altnvalue = (value1 & 1);
                pasmvars->altrvalue = pasmvars->altdvalue = ((value1 >> 1) + value2) & 511;
                result = value1 + (value2 << ((31 - 17)) >> (31 - 8));
                alt_instr = 1;
	        break;

                case 3: // altgw
                pasmvars->altnflag = pasmvars->altsflag = 1;
                pasmvars->altnvalue = (value1 & 1);
                pasmvars->altsvalue = ((value1 >> 1) + value2) & 511;
                result = value1 + (value2 << ((31 - 17)) >> (31 - 8));
                alt_instr = 1;
                break;
            }
            break;

            case 4: // altr, altd, alts, altb
            write_czr = 1;
            switch (czi >> 1)
            {
                case 0: // altr
                pasmvars->altrflag = 1;
                pasmvars->altrvalue = (value1 + value2) & 511;
                result = value1 + (value2 << ((31 - 17)) >> (31 - 8));
#if 0
                pasmvars->phase = 0;
                return breakflag;
#else
                alt_instr = 1;
                break;
#endif

	        case 1: // altd
                pasmvars->altdflag = pasmvars->altrflag = 1;
                pasmvars->altdvalue = pasmvars->altrvalue = (value1 + value2) & 511;
                result = value1 + (value2 << ((31 - 17)) >> (31 - 8));
#if 0
                pasmvars->phase = 0;
                return breakflag;
#else
                alt_instr = 1;
                break;
#endif

	        case 2: // alts
                pasmvars->altsflag = 1;
                pasmvars->altsvalue = (value1 + value2) & 511;
                //printf("\nALTS: %x\n", pasmvars->altsvalue);
                result = value1 + (value2 << ((31 - 17)) >> (31 - 8));
#if 0
                pasmvars->phase = 0;
                return breakflag;
#else
                alt_instr = 1;
                break;
#endif

                case 3: // altb
                pasmvars->altdflag = pasmvars->altrflag = 1;
                pasmvars->altdvalue = pasmvars->altrvalue = ((value1 >> 5) + value2) & 511;
                result = value1 + (value2 << ((31 - 17)) >> (31 - 8));
#if 0
                pasmvars->phase = 0;
                return breakflag;
#else
                alt_instr = 1;
                break;
#endif
            }
            break;

            case 5: // alti, setr, setd, sets
            write_czr = 1;
            switch (czi >> 1)
            {
                case 0: // alti
                if (value2 & (1 << 2))
                {
                    pasmvars->altsflag = 1;
                    pasmvars->altsvalue = value1 & 511;
                }
                if (value2 & (1 << 5))
                {
                    pasmvars->altrflag = pasmvars->altdflag = 1;
                    pasmvars->altrvalue = pasmvars->altdvalue = (value1 >> 9) & 511;
                }
                if (value2 & (1 << 8))
                {
                    if (((value2 >> 6) & 7) == 5)
                    {
                        pasmvars->altiflag = 1;
                        pasmvars->altivalue = (uint32_t)value1 >> 18;
                    }
                    else
                    {
                        pasmvars->altrflag = 1;
                        pasmvars->altrvalue = (value1 >> 19) & 511;
                    }
                }
                else if (((value2 >> 6) & 7) == 1)
                {
                    pasmvars->altrflag = 1;
                    pasmvars->altrvalue = -1;
                }

                result = ProcessAltiIncrement(value1, value2);
#if 0
                pasmvars->phase = 0;
                return breakflag;
#else
                alt_instr = 1;
                break;
#endif

	        case 1: // setr
	        result = (value1 & 0xf007ffff) | ((value2 & 0x1ff) << 19);
	        break;

	        case 2: // setd
	        result = (value1 & 0xfffc01ff) | ((value2 & 0x1ff) << 9);
	        break;

	        case 3: // sets
	        result = (value1 & 0xfffffe00) | (value2 & 0x1ff);
	        break;
            }
            break;

	    case 6: // decod, bmask, crcbit, crcnib
            write_czr = 1;
            switch (czi >> 1)
            {
	        case 0: // decod
	        result = 1 << (value2 & 0x1f);
	        break;

                case 1: // bmask
                result = ~(0xfffffffe << (value2 & 0x1f));
                break;

                case 2: // crcbit
                NotImplemented(instruct);
                break;

                case 3: // crcnib
                NotImplemented(instruct);
                break;
            }
            break;

	    case 7: // muxnits, muxnibs, muxq, movbyts
            write_czr = 1;
            switch (czi >> 1)
            {
                case 0: // muxnits
                temp = 3;
                for (i = 0; i < 16; i++)
                {
                    if (value2 & temp)
                        value1 = (value1 & ~temp) | (value2 & temp);
                    temp <<= 2;
                }
                result = value1;
                break;

                case 1: // muxnibs
                temp = 15;
                for (i = 0; i < 8; i++)
                {
                    if (value2 & temp)
                        value1 = (value1 & ~temp) | (value2 & temp);
                    temp <<= 4;
                }
                result = value1;
                break;

                case 2: // muxq
                temp = pasmvars->qreg;
                result = (value1 & ~temp) | (value2 & temp);
                break;

	        case 3: // movbyts
                {
                    unsigned char *ptr1 = (unsigned char *)&value1;
                    unsigned char *ptr2 = (unsigned char *)&result;
                    ptr2[0] = ptr1[value2 & 3];
                    ptr2[1] = ptr1[(value2 >> 2) & 3];
                    ptr2[2] = ptr1[(value2 >> 4) & 3];
                    ptr2[3] = ptr1[(value2 >> 6) & 3];
                }
            }
            break;
	}
	zflag = (result == 0);
        break;

        case 10: // testx, anyb, waitcnt, addctx, wmlong, calld, rdlut
        write_czr &= 6;
        switch(opcode & 7)
        {
            case 0: // mul, muls
            write_czr = 3;
            if (czi&4) // muls
            {
	        value1 = (value1 << 16) >> 16;
	        value2 = (value2 << 16) >> 16;
	        result = value1 * value2;
            }
            else // mul
            {
	        value1 &= 0xffff;
	        value2 &= 0xffff;
	        result = value1 * value2;
            }
            zflag = (result == 0);
            break;

            case 1: // sca, scas
            write_czr &= 2;
            if (czi&4) // scas
            {
	        value1 = (value1 << 16) >> 16;
	        value2 = (value2 << 16) >> 16;
	        result = (value1 * value2) >> 14;
            }
            else // sca
            {
	        value1 &= 0xffff;
	        value2 &= 0xffff;
	        result = (uint32_t)(value1 * value2) >> 16;
            }
            alt_instr = 1;
            pasmvars->altsvflag = 1;
            pasmvars->altsvvalue = result;
            zflag = (result == 0);
            break;

            case 2: // addpix, mulpix, blnpix, mixpix
            write_czr = 1;
            switch (czi >> 1)
            {
                int32_t dmix, smix;

                case 0: // addpix
                for (i = 0; i < 4; i++)
                {
                    temp = (value1 & 255) + (value2 &255);
                    if (temp > 255) temp = 255;
                    result = ((uint32_t)result >> 8) | (temp << 24);
                    value1 >>= 8;
                    value2 >>= 8;
                }
                break;

                case 1: // mulpix
                for (i = 0; i < 4; i++)
                {
                    temp = (value1 & 255) * (value2 & 255);
                    temp = (temp + 255) >> 8;
                    result = ((uint32_t)result >> 8) | (temp << 24);
                    value1 >>= 8;
                    value2 >>= 8;
                }
                break;

                case 2: // blnpix
                smix = (pasmvars->blnpix_var & 255);
                dmix = (smix ^ 255);
                for (i = 0; i < 4; i++)
                {
#if 1
                    temp = (value1 & 255) * dmix;
                    temp += (value2 & 255) * smix;
                    temp = (temp + 255) >> 8;
#else
                    dmix = 256 - smix;
                    temp = (value1 & 255) * dmix;
                    temp += (value2 & 255) * smix;
                    temp = (temp + 255) >> 8;
                    if (temp > 255) temp = 255;
#endif
                    result = ((uint32_t)result >> 8) | (temp << 24);
                    value1 >>= 8;
                    value2 >>= 8;
                }
                break;

                case 3: // mixpix
                for (i = 0; i < 4; i++)
                {
                    smix = GetMixVal(pasmvars->mixpix_mode, pasmvars->blnpix_var, value2, value1);
                    dmix = GetMixVal(pasmvars->mixpix_mode >> 3, pasmvars->blnpix_var, value2, value1);
                    temp = (value1 & 255) * dmix;
                    temp += (value2 & 255) * smix;
                    temp = (temp + 255) >> 8;
                    result = ((uint32_t)result >> 8) | (temp << 24);
                    value1 >>= 8;
                    value2 >>= 8;
                }
                break;
            }
            break;
     
            case 3: // addct1, addct2, addct3, wmlong
            write_czr = 1;
            switch (czi >> 1)
            {
                case 0: // addct1
                result = value1 + value2;
                pasmvars->cntreg1 = result;
                pasmvars->intflags &= ~(1 << 1);
                break;

                case 1: // addct2
                result = value1 + value2;
                pasmvars->cntreg2 = result;
                pasmvars->intflags &= ~(1 << 2);
                break;

                case 2: // addct3
                result = value1 + value2;
                pasmvars->cntreg3 = result;
                pasmvars->intflags &= ~(1 << 3);
                break;

                case 3: // wmlong
if (streamflag) printf("\nSTREAM COLLISION\n");
                value2 += pasmvars->rwrep << 2;
                write_czr = 0;
                if (value2 >= 0xfff80 && value2 < 0xfffc0)
                    wrl_flags0 |= 1 << ((value2 >> 2) & 15);
	        result = read_unaligned_long(value2);
                if ((value1 & 0xff) != 0xff)
                    result = (result & ~0xff) | (value1 & 0xff);
                if ((value1 & 0xff) != 0xff00)
                    result = (result & ~0xff00) | (value1 & 0xff00);
                if ((value1 & 0xff) != 0xff0000)
                    result = (result & ~0xff0000) | (value1 & 0xff0000);
                if ((value1 & 0xff) != 0xff000000)
                    result = (result & ~0xff000000) | (value1 & 0xff000000);
	        write_unaligned_long(value2, result);
                if (pasmvars->printflag > 1)
	            fprintf(tracefile, ", wrl[%x] = %x", value2, value1);
                if (memflag)
                {
                    if (pasmvars->qreg & 0x1ff)
                    {
                        pasmvars->rwrep++;
                        pasmvars->qreg--;
                    }
                    else
                    {
                        pasmvars->rwrep = 0;
                        pasmvars->memflag = 0;
                        if (sflag) pasmvars->augsflag = 0;
                        if (dflag) pasmvars->augdflag = 0;
                    }
                }
                break;
            }
            write_czr |= 1;
            break;

	    case 4: // rqpin, rdpin
            NotImplemented(instruct);
            break;

            case 5: // rdlut
            result = pasmvars->lut[value1 & 0x1ff];
	    zflag = (result == 0);
            write_czr |= 1;
            break;

	    case 6: // rdbyte
            write_czr |= 1;
if (streamflag) printf("\nSTREAM COLLISION\n");
	    result = BYTE(value2);
	    zflag = (result == 0);
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdb[%x]", value2);
            break;

	    case 7: // rdword
            write_czr |= 1;
if (streamflag) printf("\nSTREAM COLLISION\n");
	    result = read_unaligned_word(value2);
	    zflag = (result == 0);
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdw[%x]", value2);
            break;
	}
	break;
           
        case 11: // rdlong, calld, ijxx, djxx, tjxx, callpa, callpb, jxxx, jnxxx, setpat
        switch(opcode&7)
        {
#if 1
	    case 0: // rdlong
            write_czr |= 1;
if (streamflag) printf("\nSTREAM COLLISION\n");
            rsltaddr += pasmvars->rwrep;
            value2 += pasmvars->rwrep << 2;
            if (value2 >= 0xfff80 && value2 < 0xfffc0)
            {
#ifdef DEBUG_STUFF
                printf("\n%d: RDLONG %8.8x\n", pasmvars->cogid, value2);
#endif
                rdl_flags0 |= 1 << ((value2 >> 2) & 15);
            }
	    result = read_unaligned_long(value2);
	    zflag = (result == 0);
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdl[%x]", value2);
#if 0
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, "(%x)", result);
#endif
            if (memflag)
            {
                if (pasmvars->qreg & 0x1ff)
                {
                    pasmvars->rwrep++;
                    pasmvars->qreg--;
                }
                else
                {
                    pasmvars->rwrep = 0;
                    pasmvars->memflag = 0;
                    if (sflag) pasmvars->augsflag = 0;
                    if (dflag) pasmvars->augdflag = 0;
                }
            }
            break;

	    case 1: // calld
            write_czr |= 1;
            if (pasmvars->printflag > 1) fprintf(tracefile, ", CALLD");
            if (write_czr == 7)
            {
                if (srcaddr == REG_IRET1)
                    pasmvars->intstate &= ~2;
                else if (srcaddr == REG_IRET2)
                    pasmvars->intstate &= ~4;
                else if (srcaddr == REG_IRET3)
                    pasmvars->intstate &= ~8;
            }
            if (rflag)
            {
                 value2 = (value2 << 23) >> 23;
                 pasmvars->pc = (pc + pc_incr * (1 + value2)) & ADDR_MASK;
            }
            else
                 pasmvars->pc = value2 & ADDR_MASK;
            pasmvars->pc1 |= INVALIDATE_INSTR;
            result = (pc + pc_incr) | (pasmvars->zflag << 21) | (pasmvars->cflag << 20);
            cflag = (result >> 20) & 1;
            zflag = (result >> 21) & 1;
            check_hubexec_mode(pasmvars);
            break;

	    case 2: // callpa, callpb
            if (czi&4)
                rsltaddr = REG_PB;
            else
                rsltaddr = REG_PA;
            write_czr = 1;
            result = value1;
            pasmvars->pc = value1 & ADDR_MASK;
            pasmvars->pc1 |= INVALIDATE_INSTR;
            pc += pc_incr;
            pc |= (pasmvars->zflag << 21) | (pasmvars->cflag << 20);
            pasmvars->retstack[pasmvars->retptr] = pc;
            pasmvars->retptr = (pasmvars->retptr + 1) & 7;
            if (pasmvars->retptr == 0)
                printf("return stack overflow%s", NEW_LINE);
            check_hubexec_mode(pasmvars);
            break;

	    case 3: // djz, djnz, djf, djnf
            value1--;
	    cflag = (value1 == -1);
            write_czr = 1;
            result = value1;
	    zflag = (result == 0);
	    // Determine if we should jump
            if (czi & 4) // djf, djnf
            {
	        if (cflag != ((czi >> 1) & 1))
	        {
                    value2 = (value2 << 23) >> 23;
	            pasmvars->pc = (pc + pc_incr * (1 + value2)) & ADDR_MASK;
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    check_hubexec_mode(pasmvars);
	        }
            }
            else // djz, djnz
            {
	        if (zflag != ((czi >> 1) & 1))
	        {
                    value2 = (value2 << 23) >> 23;
	            pasmvars->pc = (pc + pc_incr * (1 + value2)) & ADDR_MASK;
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    check_hubexec_mode(pasmvars);
	        }
            }
            break;

	    case 4: // ijz, ijnz, tjz, tjnz
            if (czi&4) // tjz, tjnz
            {
                write_czr = 0;
                result = value1;
            }
            else // ijz, ijnz
            {
                write_czr = 1;
                result = value1 + 1;
            }
	    zflag = (result == 0);
	    cflag = (result == 0);
	    // Determine if we should jump
	    if (zflag != ((czi >> 1) & 1))
	    {
                value2 = (value2 << 23) >> 23;
	        pasmvars->pc = (pc + pc_incr * (1 + value2)) & ADDR_MASK;
                pasmvars->pc1 |= INVALIDATE_INSTR;
                check_hubexec_mode(pasmvars);
            }
            break;

	    case 5: // tjf, tjnf, tjs, tjns
            write_czr = 0;
            result = value1;
	    zflag = (result == 0);
	    cflag = (result == -1);
	    // Determine if we should jump
            if (czi & 4) // tjs, tjns
            {
	        if (((result >> 31) & 1) != ((czi >> 1) & 1))
	        {
                    value2 = (value2 << 23) >> 23;
	            pasmvars->pc = (pc + pc_incr * (1 + value2)) & ADDR_MASK;
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    check_hubexec_mode(pasmvars);
	        }
            }
            else // tjf, tjnf
            {
	        if (cflag != ((czi >> 1) & 1))
	        {
                    value2 = (value2 << 23) >> 23;
	            pasmvars->pc = (pc + pc_incr * (1 + value2)) & ADDR_MASK;
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    check_hubexec_mode(pasmvars);
	        }
            }
            break;

	    case 6: // tjv, jint, jnint, ...
            write_czr = 0;
            if ((czi&6) == 0) // tjv
                NotImplemented(instruct);
            else if ((czi&6) == 2) // jint, jnint, ....
            {
                temp = (value1 & 15);
                temp = (pasmvars->intflags >> temp) & 1;
                if (value1 & 16) temp ^= 1;
                if (temp)
                {
                    if (instruct & 0x40000)
	                pasmvars->pc = value2 & 0xfffff;
                    else
                    {
                        value2 = (value2 << 23) >> 23;
	                pasmvars->pc = (pc + pc_incr * (1 + value2)) & ADDR_MASK;
                    }
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    check_hubexec_mode(pasmvars);
                }
            }
            else // empty
                NotImplemented(instruct);
            break;

	    case 7: // setpat
            write_czr = 0;
            if (czi&4) // setpat
                NotImplemented(instruct);
            else // empty
                NotImplemented(instruct);
            break;
#else
	    case 0: // rdbyte
if (streamflag) printf("\nSTREAM COLLISION\n");
	    result = BYTE(value2);
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdb[%x]", value2);
            break;

	    case 1: // rdword
if (streamflag) printf("\nSTREAM COLLISION\n");
	    result = read_unaligned_word(value2);
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdw[%x]", value2);
            break;

	    case 2: // rdlong
if (streamflag) printf("\nSTREAM COLLISION\n");
            rsltaddr += pasmvars->rwrep;
            value2 += pasmvars->rwrep << 2;
            if (value2 >= 0xfff80 && value2 < 0xfffc0)
            {
#ifdef DEBUG_STUFF
                printf("\n%d: RDLONG %8.8x\n", pasmvars->cogid, value2);
#endif
                rdl_flags0 |= 1 << ((value2 >> 2) & 15);
            }
	    result = read_unaligned_long(value2);
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, ", rdl[%x]", value2);
#if 0
            if (pasmvars->printflag > 1)
	        fprintf(tracefile, "(%x)", result);
#endif
            if (memflag)
            {
                if (pasmvars->qreg & 0x1ff)
                {
                    pasmvars->rwrep++;
                    pasmvars->qreg--;
                }
                else
                {
                    pasmvars->rwrep = 0;
                    pasmvars->memflag = 0;
                    if (sflag) pasmvars->augsflag = 0;
                    if (dflag) pasmvars->augdflag = 0;
                }
            }
            break;

	    case 3: // addpix, mulpix, blnpix, mixpix
            switch (czi >> 1)
            {
                case 0: // addpix
	        NotImplemented(instruct);
                break;

                case 1: // mulpix
	        NotImplemented(instruct);
                break;

                case 2: // blnpix
	        NotImplemented(instruct);
                break;

                case 3: // mixpix
	        NotImplemented(instruct);
                break;
            }
	    break;

	    case 4:
	    NotImplemented(instruct);
	    break;

	    case 5:
	    NotImplemented(instruct);
	    break;

	    case 6: // setpae, setpan
            write_czr &= 6;
            pasmvars->pinpatmode = 1 + ((czi >> 2) & 1);
            pasmvars->pinpatmask = value1;
            pasmvars->pinpattern = value2;
	    break;

	    case 7: // setpbe, setpbn
            write_czr &= 6;
            pasmvars->pinpatmode = 3 + ((czi >> 2) & 1);
            pasmvars->pinpatmask = value1;
            pasmvars->pinpattern = value2;
	    break;
#endif
        }
	zflag = (result == 0);
        break;

	case 12: // wrpin, wxpin, wypin, wrlut, wrxxxx, rdfast, wrfast, fblock, xinit, xzero, xcont, rep, coginit
	switch(opcode & 7)
	{
            case 0: // wrpin, wxpin
            if (czi&4) // wxpin
	        NotImplemented(instruct);
            else // wrpin
	        NotImplemented(instruct);
            break;

            case 1: // wypin, wrlut
            if (czi&4) // wrlut
            {
                result = value2;
                pasmvars->lut[value1 & 0x1ff] = value2;
                temp = pasmvars->cogid ^ 1;
                if (PasmVars[temp].share_lut)
                    PasmVars[temp].lut[value1 & 0x1ff] = value2;
            }
            else // wypin
	        NotImplemented(instruct);
            break;

            case 2: // wrbyte, wrword
            if (czi & 4) // wrword
            {
if (streamflag) printf("\nSTREAM COLLISION\n");
                write_czr = 0;
                write_unaligned_word(value2, value1);
                if (pasmvars->printflag > 1)
	            fprintf(tracefile, ", wrw[%x] = %x", value2, value1);
            }
            else // wrbyte
            {
if (streamflag) printf("\nSTREAM COLLISION\n");
                write_czr = 0;
	        BYTE(value2) = value1;
                if (pasmvars->printflag > 1)
	            fprintf(tracefile, ", wrb[%x] = %x", value2, value1);
            }
            break;

            case 3: // wrlong, rdfast
            if (czi & 4) // rdfast
            {
                write_czr = 0;
                value1 = (value1 & 0x3fff) << 6;
                value2 &= 0xfffff;
                start_fast_mode(pasmvars, value2, value2 + value1, 1);
            }
            else // wrlong
            {
                value2 += pasmvars->rwrep << 2;
if (streamflag) printf("\nSTREAM COLLISION\n");
                write_czr = 0;
                if (value2 >= 0xfff80 && value2 < 0xfffc0)
                {
#ifdef DEBUG_STUFF
                    printf("\n%d: WRLONG %8.8x\n", pasmvars->cogid, value2);
#endif
                    wrl_flags0 |= 1 << ((value2 >> 2) & 15);
                }
                write_unaligned_long(value2, value1);
                if (pasmvars->printflag > 1)
	            fprintf(tracefile, ", wrl[%x] = %x", value2, value1);
                if (memflag)
                {
                    if (pasmvars->qreg & 0x1ff)
                    {
                        pasmvars->rwrep++;
                        pasmvars->qreg--;
                    }
                    else
                    {
                        pasmvars->rwrep = 0;
                        pasmvars->memflag = 0;
                        if (sflag) pasmvars->augsflag = 0;
                        if (dflag) pasmvars->augdflag = 0;
                    }
                }
            }
            break;

            case 4: // wrfast, fblock
            if (czi & 4) // fblock
            {
                int mode = pasmvars->str_fifo_mode;
                write_czr = 0;
                value1 = (value1 & 0x3fff) << 6;
                value2 &= 0xfffff;
                start_fast_mode(pasmvars, value2, value2 + value1, mode);
            }
            else // wrfast
            {
                write_czr = 0;
                value1 = (value1 & 0x3fff) << 6;
                value2 &= 0xfffff;
                start_fast_mode(pasmvars, value2, value2 + value1, 2);
            }
            break;

            case 5: // xinit, xzero
            if (czi & 4) // xzero
		NotImplemented(instruct);
            else // xinit
		NotImplemented(instruct);
            break;

            case 6: // xcont, rep
            if (czi & 4) // rep
            {
                pasmvars->repcnt = value2 - 1;
                pasmvars->repbot = pasmvars->pc1;
                if (value2 == 0) pasmvars->repforever = 1;
                if (pasmvars->pc >= 0x400) value1 <<= 2;
                pasmvars->reptop = (pasmvars->pc2 + value1) & ADDR_MASK;
                write_czr = 0;
            }
            else // xcont
		NotImplemented(instruct);
            break;

	    case 7: // coginit
            if (value1 & 0x10)
            {
                for (result = 0; result < 16; result++)
                {
                    if (!PasmVars[result].state) break;
                }
                if (result == 16) result = -1;
            }
            else
                result = value1 & 15;
            //printf("\ncoginit: cog = %d, value1 = %8.8x, value2 = %8.8x\n", result, value1, value2);
            if (result != -1)
            {
                StartPasmCog2(&PasmVars[result], pasmvars->qreg, (value2 & ADDR_MASK), result, value1 & 0x20);
                UpdatePins2();
                // Return without saving if we restarted this cog
                if (result == pasmvars->cogid) return breakflag;
                cflag = 1;
            }
            else
                cflag = 0;
            write_czr &= 6;
            break;
	}
	zflag = (result == 0);
	break;

        case 13: // qxxx, jmp, call, calla, callb, misc
        switch(opcode & 7)
        {
	    case 0: // qmul, qdiv
            write_czr &= 6;
            i = start_cordic(pasmvars);
            if (czi & 4) // qdiv
            {
                uint64_t d_value1 = (uint32_t)value1;
                uint64_t d_value2 = (uint32_t)value2;
                d_value1 |= (uint64_t)(uint32_t)pasmvars->qreg << 32;
                if (value2)
                {
                    uint64_t r_value1 = d_value1 / d_value2;
                    uint64_t r_value2 = d_value1 % d_value2;
                    if (r_value1 > 0xffffffffu)
                    {
                        pasmvars->qxqueue[i] = 0xffffffff - pasmvars->qreg + value2;
                        pasmvars->qyqueue[i] = value1 + pasmvars->qreg;
                    }
                    else
                    {
                        pasmvars->qxqueue[i] = r_value1;
                        pasmvars->qyqueue[i] = r_value2;
                    }
                }
                else
                {
                    pasmvars->qxqueue[i] = 0xffffffff - pasmvars->qreg;
                    pasmvars->qyqueue[i] = value1;
                }
            }
            else // qmul
            {
                uint64_t d_value1 = (uint32_t)value1;
                uint64_t d_value2 = (uint32_t)value2;
                d_value1 *= d_value2;
                pasmvars->qxqueue[i] = d_value1 & 0xffffffff;
                pasmvars->qyqueue[i] = (d_value1 >> 32) & 0xffffffff;
            }
            break;

	    case 1: // qfrac, qsqrt
            write_czr &= 6;
            i = start_cordic(pasmvars);
            if (czi & 4) // qsqrt
            {
                uint64_t d_value1 = (uint32_t)value2;
                d_value1 = (d_value1 << 32) | (uint32_t)value1;
                pasmvars->qxqueue[i] = sqrt64(d_value1);
                pasmvars->qyqueue[i] = 0;
            }
            else // qfrac
            {
#if 0
                if (value2)
                {
                    double x = (double)value1 + ((double)(uint32_t)pasmvars->qreg / 4294967296.0);
                    double y = (double)value2;
                    double z = x/y;
                    z -= floor(z);
                    pasmvars->qxqueue[i] = z * 4294967296.0;
                    pasmvars->qyqueue[i] = 0;
                }
                else
                {
                    pasmvars->qxqueue[i] = 0xffffffff - value1;
                    pasmvars->qyqueue[i] = pasmvars->qreg;
                }
#else
                uint64_t d_value1 = (uint32_t)value1;
                uint64_t d_value2 = (uint32_t)value2;
                d_value1 = (d_value1 << 32) | (uint32_t)pasmvars->qreg;
                if (value2)
                {
                    uint64_t r_value1 = d_value1 / d_value2;
                    uint64_t r_value2 = d_value1 % d_value2;
                    if (r_value1 > 0xffffffffu)
                    {
                        pasmvars->qxqueue[i] = 0xffffffff - value1 + value2;
                        pasmvars->qyqueue[i] = value1 + pasmvars->qreg;
                    }
                    else
                    {
                        pasmvars->qxqueue[i] = r_value1;
                        pasmvars->qyqueue[i] = r_value2;
                    }
                }
                else
                {
                    pasmvars->qxqueue[i] = 0xffffffff - value1;
                    pasmvars->qyqueue[i] = pasmvars->qreg;
                }
#endif
            }
            break;

	    case 2: // qrotate, qvector
            write_czr &= 6;
            i = start_cordic(pasmvars);
            if (czi & 4) // qvector
            {
#if 0
                double x = (double)value1;
                double y = pasmvars->qreg;
                double angle = (double)value2;
#else
                double x = (double)value1;
                double y = (double)value2;
#endif
                double rho, theta;
                rho = sqrt(x*x + y*y) + 0.5;
                theta = atan2(y, x);
                pasmvars->qxqueue[i] = rho;
                theta *= 4294967296.0 / (2.0 * 3.14159265359);
                if (theta >= 0.0)
                    theta += 0.5;
                else
                    theta -= 0.5;
                pasmvars->qyqueue[i] = theta;
            }
            else // qrotate
            {
                double x = (double)value1;
                double y = pasmvars->qreg;
                double angle = (double)value2;
                angle *= 2.0 * 3.14159265359 / 4294967296.0;
                pasmvars->qxqueue[i] = cos(angle) * x - sin(angle)*y;
                pasmvars->qyqueue[i] = sin(angle) * x + cos(angle)*y;
            }
            break;

	    case 3: // misc
            srcaddr = instruct & 511; // Ensure alts doesn't modify
            //printf("MISC: srcaddr = 0x%x\n", srcaddr);
            if (srcaddr == 0x24) srcaddr = (instruct & 0x3ffff);
            switch (srcaddr)
            {
                case 0: // hubset - TODO Need to implement.  Ignore for now.
                break;

                case 1: // cogid
                result = pasmvars->cogid;
                break;

                case 3: // cogstop
		for (result = 0; result < 8; result++)
		{
		    if (!PasmVars[result].state) break;
		}
		cflag = (result == 8);
		result = value1 & 7;
		zflag = (result == 0);
                PasmVars[result].state = 0;
		UpdatePins2();
		// Return without saving if we stopped this cog
		if (result == pasmvars->cogid) return breakflag;
		break;

		case 4: // locknew
		for (result = 0; result < 16; result++)
		{
		    if (!lockalloc[result]) break;
		}
		if (result == 16)
		{
		    cflag = 1;
		    result = 15;
		}
		else
		{
		    cflag = 0;
		    lockalloc[result] = 1;
		}
		zflag = (result == 0);
		break;

		case 5: // lockret
		for (result = 0; result < 16; result++)
		{
		    if (!lockalloc[result]) break;
		}
		cflag = (result == 16);
		result = value1 & 15;
		zflag = (result == 0);
		lockalloc[result] = 0;
		break;

		case 6: // locktry
		result = value1 & 15;
		zflag = (result == 0);
                if (!lockstate[result])
                {
                    cflag = 1;
                    lockstate[result] = 0x100 | pasmvars->cogid;
                }
                else
                    cflag = 0;
		break;

		case 7: // lockrel
                zflag = 0;
		temp = value1 & 15;
                result = lockstate[temp] & 15;
                cflag = (lockstate[temp] != 0);
                if (cflag && result == pasmvars->cogid)
                {
                    cflag = 0;
                    lockstate[result] = 0;
                }
                write_czr &= 6;
                if ((czi&4) == 0)
                    write_czr = 0;
                else if (czi&1)
                    write_czr = 4;
                else
                    write_czr = 5;
		break;

                case 14: // qlog
                write_czr &= 6;
                i = start_cordic(pasmvars);
                {
                    double x = (double)(unsigned int)value1;
                    x = log(x)/log(2.0);
                    x = x * 134217728.0 + 0.5;
                    if (x >= 4294967296.0)
                        x = 4294967295.0;
                    result = (unsigned int)x;
                }
                pasmvars->qxqueue[i] = result;
                pasmvars->qyqueue[i] = 0;
                break;

                case 15: // qexp
                write_czr &= 6;
                i = start_cordic(pasmvars);
                if (value1 == 0xffffffff)
                    result = 0xffffffe8;
                else
                {
                    double x = (double)(unsigned int)value1;
                    x = pow(2.0, x / 134217728.0) + 0.5;
                    result = (unsigned int)x;
                }
                pasmvars->qxqueue[i] = result;
                pasmvars->qyqueue[i] = 0;
                break;

                case 16: // rfbyte
                result = read_stream_fifo_byte(pasmvars);
                zflag = (result == 0);
                break;

                case 17: // rfword
                result = read_stream_fifo_word(pasmvars);
                zflag = (result == 0);
                break;

                case 18: // rflong
                result = read_stream_fifo_long(pasmvars);
                zflag = (result == 0);
                break;

                case 19: // rfvar
                result = temp = 0;
                do
                {
                    result |= read_stream_fifo_byte(pasmvars) << temp;
                    temp += 7;
                } while ((temp < 28) && (result & (1 << temp)));
                cflag = 0;
                zflag = (result == 0);
                break;

                case 20: // rfvars
                result = temp = 0;
                do
                {
                    result |= read_stream_fifo_byte(pasmvars) << temp;
                    temp += 7;
                } while ((temp < 28) && (result & (1 << temp)));
                temp = 31 - temp;
                result = (result << temp) >> temp;
                cflag = (result >> 31) & 1;
                zflag = (result == 0);
                break;

                case 21: // wfbyte
                write_czr = 0;
                write_stream_fifo_byte(pasmvars, value1);
                if (pasmvars->printflag > 1)
	            fprintf(tracefile, ", fifo = %2.2x", value1);
                break;

                case 22: // wfword
                write_czr = 0;
                write_stream_fifo_word(pasmvars, value1);
                if (pasmvars->printflag > 1)
	            fprintf(tracefile, ", fifo = %4.4x", value1);
                break;

                case 23: // wflong
                write_czr = 0;
                write_stream_fifo_long(pasmvars, value1);
                if (pasmvars->printflag > 1)
	            fprintf(tracefile, ", fifo = %8.8x", value1);
                break;

                case 24: // getqx
                pasmvars->qxposted = 0;
                result = pasmvars->qxreg;
                break;

                case 25: // getqy
                pasmvars->qyposted = 0;
                result = pasmvars->qyreg;
                break;

                case 26: // getct
                result = GetCnt();
                break;

                case 27: // getrnd
                result = rand();
                break;

                case 28: // setdacs
                NotImplemented(instruct);
                break;

                case 29: // setxfrq
                NotImplemented(instruct);
                break;

                case 30: // getxacc
                NotImplemented(instruct);
                break;

                case 31: // waitx
                write_czr &= 6;
                break;

                case 32: // setse1
                write_czr &= 6;
                NotImplemented(instruct);
                break;

                case 33: // setse2
                write_czr &= 6;
                NotImplemented(instruct);
                break;

                case 34: // setse3
                write_czr &= 6;
                NotImplemented(instruct);
                break;

                case 35: // setse4
                write_czr &= 6;
                NotImplemented(instruct);
                break;

                case 0x0024: // pollint
                case 0x0224: // pollct1
                case 0x0424: // pollct2
                case 0x0624: // pollct3
                case 0x0824: // pollse1
                case 0x0a24: // pollse2
                case 0x0c24: // pollse3
                case 0x0e24: // pollse4
                case 0x1024: // pollpat
                case 0x1224: // pollfbw
                case 0x1424: // pollxmt
                case 0x1624: // pollxfi
                case 0x1824: // pollxro
                case 0x1a24: // pollxlr
                case 0x1c24: // pollatn
                case 0x1e24: // pollqmt
                temp = 1 << ((srcaddr >> 9) & 15);
                cflag = ((pasmvars->intflags & temp) != 0);
                pasmvars->intflags &= ~temp;
                write_czr &= 6;
                break;

                case 0x2024: // waitint
                case 0x2224: // waitct1
                case 0x2424: // waitct2
                case 0x2624: // waitct3
                case 0x2824: // waitse1
                case 0x2a24: // waitse2
                case 0x2c24: // waitse3
                case 0x2e24: // waitse4
                case 0x3024: // waitpat
                case 0x3224: // waitfbw
                case 0x3424: // waitxmt
                case 0x3624: // waitxfi
                case 0x3824: // waitxro
                case 0x3a24: // waitxrl
                case 0x3c24: // waitatn
                temp = 1 << ((srcaddr >> 9) & 15);
                pasmvars->intflags &= ~temp;
                write_czr &= 6;
                break;

                case 0x4024: // allowi
                pasmvars->intstate &= ~1;
                write_czr &= 6;
                break;

                case 0x4224: // stalli
                pasmvars->intstate |= 1;
                write_czr &= 6;
                break;

                case 0x4424: // trgint1
                case 0x4624: // trgint2
                case 0x4824: // trgint3
                NotImplemented(instruct);
                break;

                case 0x4a24: // nixint1
                case 0x4c24: // nixint2
                case 0x4e24: // nixint3
                NotImplemented(instruct);
                break;

                case 37: // setint1
                pasmvars->intenable1 = value1;
                write_czr &= 6;
                break;

                case 38: // setint2
                pasmvars->intenable2 = value1;
                write_czr &= 6;
                break;

                case 39: // setint3
                pasmvars->intenable3 = value1;
                write_czr &= 6;
                break;

                case 40: // setq
                write_czr &= 6;
                pasmvars->qreg = value1;
                pasmvars->memflag = 1;
                pasmvars->phase = 0;
                pasmvars->skip_mask >>= 1;
                //printf("qreg = %d\n", value1);
                return breakflag;
                break;

                case 41: // setq2
                write_czr &= 6;
                pasmvars->qreg = value1;
                pasmvars->memflag = 2;
                pasmvars->phase = 0;
                pasmvars->skip_mask >>= 1;
                //printf("qreg = %d\n", value1);
                return breakflag;
                break;

                case 42: // push
		pasmvars->retstack[pasmvars->retptr] = value1;
		pasmvars->retptr = (pasmvars->retptr + 1) & 7;
		if (pasmvars->retptr == 0)
		    printf("return stack overflow%s", NEW_LINE);
                break;

                case 43: // pop
		pasmvars->retptr = (pasmvars->retptr - 1) & 7;
		pasmvars->retstack[pasmvars->retptr] = value1;
		if (pasmvars->retptr == 7)
		    printf("return stack underflow%s", NEW_LINE);
                break;

                case 44: // jmp
                pasmvars->pc = value1 & ADDR_MASK;
                pasmvars->pc1 |= INVALIDATE_INSTR;
                check_hubexec_mode(pasmvars);
                write_czr &= 6;
                break;

                case 45: // call, ret
                //if (pasmvars->printflag > 1) fprintf(tracefile, ", CALL or RET %x", result);
                if (instruct & 0x40000) // ret
                {
		    pasmvars->retptr = (pasmvars->retptr - 1) & 7;
		    result = pasmvars->retstack[pasmvars->retptr];
		    if (pasmvars->retptr == 7)
		        printf("return stack underflow%s", NEW_LINE);
		    cflag = (result >> 20) & 1;
		    zflag = (result >> 21) & 1;
		    pasmvars->pc = result & ADDR_MASK;
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    if (pasmvars->printflag > 1) fprintf(tracefile, ", RET %x", result);
                }
                else // call
                {
                    pasmvars->pc = value1 & ADDR_MASK;
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pc += pc_incr;
                    pc |= (pasmvars->zflag << 21) | (pasmvars->cflag << 20);
                    pasmvars->retstack[pasmvars->retptr] = pc;
                    pasmvars->retptr = (pasmvars->retptr + 1) & 7;
                    if (pasmvars->retptr == 0)
                        printf("return stack overflow%s", NEW_LINE);
                }
                write_czr &= 6;
                check_hubexec_mode(pasmvars);
                break;

                case 46: // calla, reta
                if (instruct & 0x40000) // reta
                {
		    pasmvars->ptra = (pasmvars->ptra - 4) & ADDR_MASK;
                    result = read_unaligned_long(pasmvars->ptra);
		    cflag = (result >> 20) & 1;
		    zflag = (result >> 21) & 1;
		    pasmvars->pc = result & ADDR_MASK;
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                }
                else // calla
                {
                    pasmvars->pc = value1 & ADDR_MASK;
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pc += pc_incr;
                    pc |= (pasmvars->zflag << 21) | (pasmvars->cflag << 20);
                    write_unaligned_long(pasmvars->ptra, pc);
                    pasmvars->ptra = (pasmvars->ptra + 4) & ADDR_MASK;
                }
                write_czr &= 6;
                check_hubexec_mode(pasmvars);
                break;

                case 47: // callb, retb
                if (instruct & 0x40000) // retb
                {
		    pasmvars->ptrb = (pasmvars->ptrb - 4) & ADDR_MASK;
                    result = read_unaligned_long(pasmvars->ptrb);
		    cflag = (result >> 20) & 1;
		    zflag = (result >> 21) & 1;
		    pasmvars->pc = result & ADDR_MASK;
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                }
                else // callb
                {
                    pasmvars->pc = value1 & ADDR_MASK;
                    pasmvars->pc1 |= INVALIDATE_INSTR;
                    pc += pc_incr;
                    pc |= (pasmvars->zflag << 21) | (pasmvars->cflag << 20);
                    write_unaligned_long(pasmvars->ptrb, pc);
                    pasmvars->ptrb = (pasmvars->ptrb + 4) & ADDR_MASK;
                }
                write_czr &= 6;
                check_hubexec_mode(pasmvars);
                break;

                case 48: // jmprel
                write_czr &= 6;
                if (pc & 0xfffffc00)
                    pasmvars->pc = (pc + 4 + value1) & ADDR_MASK;
                else
                    pasmvars->pc = (pc + 1 + (value1 >> 2)) & 0x3ff;
                pasmvars->pc1 |= INVALIDATE_INSTR;
                check_hubexec_mode(pasmvars);
                break;

                case 49: // skip
                pasmvars->skip_mode = 0;
                pasmvars->skip_mask = value1;
                pasmvars->phase = 0; // Change phase to fetch mode
                return breakflag;
                break;

                case 50: // skipf
	        if (pasmvars->pc < 0x400)
                {
                    pasmvars->skip_mode = (uint32_t)value1 >> 1;
                    pasmvars->skip_mask = value1 & 1;
                }
                else
                {
                    pasmvars->skip_mode = 0;
                    pasmvars->skip_mask = value1;
                }
                pasmvars->phase = 0; // Change phase to fetch mode
                return breakflag;
                break;

                case 51: // execf
                pasmvars->skip_mode = (value1 >> 8) & 0x7ffffe;
                pasmvars->skip_mask = 0;
                pasmvars->pc = (value1 & 0x3ff);
                pasmvars->pc1 |= INVALIDATE_INSTR;
                check_hubexec_mode(pasmvars);
                pasmvars->phase = 0; // Change phase to fetch mode
                return breakflag;
                break;

                case 52: // getptr
                if (pasmvars->str_fifo_mode == 2)
                    result = pasmvars->str_fifo_tail_addr;
                else
                    result = pasmvars->str_fifo_head_addr;
                zflag = (result = 0);
                break;

#if 0
                case 53: // getint
                result = pasmvars->intflags;
                result |= (pasmvars->intstate &  2) << 15;
                result |= (pasmvars->intstate &  6) << 16;
                result |= (pasmvars->intstate & 12) << 17;
                result |= (pasmvars->intstate &  8) << 18;
                zflag = (result = 0);
                break;
#else
                case 53: // getbrk, cogbrk
                if (czi & 6) // getbrk
                    NotImplemented(instruct);
                else         // cogbrk
                    NotImplemented(instruct);
                break;
#endif

                case 54: // brk
                NotImplemented(instruct);
                break;

                case 55: // setluts
                pasmvars->share_lut = (value1 & 1);
                break;

                case 56: // setcy
                NotImplemented(instruct);
                break;

                case 57: // setci
                NotImplemented(instruct);
                break;

                case 58: // setcq
                NotImplemented(instruct);
                break;

                case 59: // setcfrq
                NotImplemented(instruct);
                break;

                case 60: // setcmod
                NotImplemented(instruct);
                break;

                case 61: // setpiv
                pasmvars->blnpix_var = value1 & 255;
                break;

                case 62: // setpix
                pasmvars->mixpix_mode = value1 & 63;
                break;

                case 63: // cogatn
                for (i = 0; i < 16; i++)
                {
                    if ((value1 & 1) && PasmVars[i].state)
                        PasmVars[i].intflags |= (1 << 14);
                    value1 >>= 1;
                }
                break;

                case 64: // dirl, testp
                if (((czi>>2) ^ (czi>>1)) & 1) // testp
                {
                    NotImplemented(instruct);
                }
                else // dirl
                {
                    if (value1 & 32)
                    {
                        rsltaddr = REG_DIRB;
                        result = pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31));
                    }
                    else
                    {
                        rsltaddr = REG_DIRA;
                        result = pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31));
                    }
                }
                break;

                case 65: // dirh, testpn
                if (((czi>>2) ^ (czi>>1)) & 1) // testpn
                {
                    NotImplemented(instruct);
                }
                else // dirh
                {
                    if (value1 & 32)
                    {
                        rsltaddr = REG_DIRB;
                        result = pasmvars->mem[REG_DIRB] | (1 << (value1 & 31));
                    }
                    else
                    {
                        rsltaddr = REG_DIRA;
                        result = pasmvars->mem[REG_DIRA] | (1 << (value1 & 31));
                    }
                }
                break;

                case 66: // dirc, testp
                if (((czi>>2) ^ (czi>>1)) & 1) // testp
                {
                    NotImplemented(instruct);
                }
                else // dirl
                {
                    if (value1 & 32)
                    {
                        rsltaddr = REG_DIRB;
                        result = (pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31))) | (pasmvars->cflag << (value1 & 31));
                    }
                    else
                    {
                        rsltaddr = REG_DIRA;
                        result = (pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31))) | (pasmvars->cflag << (value1 & 31));
                    }
                }
                break;

                case 67: // dirnc, testpn
                if (((czi>>2) ^ (czi>>1)) & 1) // testpn
                {
                    NotImplemented(instruct);
                }
                else // dirh
                {
                    if (value1 & 32)
                    {
                        rsltaddr = REG_DIRB;
                        result = (pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31))) | ((pasmvars->cflag ^ 1) << (value1 & 31));
                    }
                    else
                    {
                        rsltaddr = REG_DIRA;
                        result = (pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31))) | ((pasmvars->cflag ^ 1) << (value1 & 31));
                    }
                }
                break;

                case 68: // dirz, testp
                if (((czi>>2) ^ (czi>>1)) & 1) // testp
                {
                    NotImplemented(instruct);
                }
                else // dirl
                {
                    if (value1 & 32)
                    {
                        rsltaddr = REG_DIRB;
                        result = (pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31))) | (pasmvars->zflag << (value1 & 31));
                    }
                    else
                    {
                        rsltaddr = REG_DIRA;
                        result = (pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31))) | (pasmvars->zflag << (value1 & 31));
                    }
                }
                break;

                case 69: // dirnz, testpn
                if (((czi>>2) ^ (czi>>1)) & 1) // testpn
                {
                    NotImplemented(instruct);
                }
                else // dirh
                {
                    if (value1 & 32)
                    {
                        rsltaddr = REG_DIRB;
                        result = (pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31))) | ((pasmvars->zflag ^ 1) << (value1 & 31));
                    }
                    else
                    {
                        rsltaddr = REG_DIRA;
                        result = (pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31))) | ((pasmvars->zflag ^ 1) << (value1 & 31));
                    }
                }
                break;

                case 70: // dirrnd, testp
                if (((czi>>2) ^ (czi>>1)) & 1) // testp
                {
                    NotImplemented(instruct);
                }
                else // dirl
                {
                    NotImplemented(instruct);
                }
                break;

                case 71: // dirnot, testpn
                if (((czi>>2) ^ (czi>>1)) & 1) // testpn
                {
                    NotImplemented(instruct);
                }
                else // dirh
                {
                    if (value1 & 32)
                    {
                        rsltaddr = REG_DIRB;
                        result = pasmvars->mem[REG_DIRB] ^ (1 << (value1 & 31));
                    }
                    else
                    {
                        rsltaddr = REG_DIRA;
                        result = pasmvars->mem[REG_DIRA] ^ (1 << (value1 & 31));
                    }
                }
                break;

                case 72: // outl
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31));
                }
                break;

                case 73: // outh
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_OUTB] | (1 << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_OUTA] | (1 << (value1 & 31));
                }
                break;

                case 74: // outc
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | (pasmvars->cflag << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | (pasmvars->cflag << (value1 & 31));
                }
                break;

                case 75: // outnc
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | ((pasmvars->cflag ^ 1) << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | ((pasmvars->cflag ^ 1) << (value1 & 31));
                }
                break;

                case 76: // outz
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | (pasmvars->zflag << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | (pasmvars->zflag << (value1 & 31));
                }
                break;

                case 77: // outnz
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | ((pasmvars->zflag ^ 1) << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | ((pasmvars->zflag ^ 1) << (value1 & 31));
                }
                break;

                case 78: // outrnd
                NotImplemented(instruct);
                break;

                case 79: // outnot
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_OUTB] ^ (1 << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_OUTB] ^ (1 << (value1 & 31));
                }
                break;

                case 80: // fltl
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31));
                }
                break;

                case 81: // flth
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = pasmvars->mem[REG_OUTB] | (1 << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = pasmvars->mem[REG_OUTA] | (1 << (value1 & 31));
                }
                break;

                case 82: // fltc
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | (pasmvars->cflag << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | (pasmvars->cflag << (value1 & 31));
                }
                break;

                case 83: // fltnc
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | ((pasmvars->cflag ^ 1) << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | ((pasmvars->cflag ^ 1) << (value1 & 31));
                }
                break;

                case 84: // fltz
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | (pasmvars->zflag << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | (pasmvars->zflag << (value1 & 31));
                }
                break;

                case 85: // fltnz
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | ((pasmvars->zflag ^ 1) << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | ((pasmvars->zflag ^ 1) << (value1 & 31));
                }
                break;

                case 86: // fltrnd
                NotImplemented(instruct);
                break;

                case 87: // fltnot
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = pasmvars->mem[REG_OUTB] ^ (1 << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] & ~(1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = pasmvars->mem[REG_OUTA] ^ (1 << (value1 & 31));
                }
                break;

                case 88: // drvl
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31));
                }
                break;

                case 89: // drvh
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = pasmvars->mem[REG_OUTB] | (1 << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = pasmvars->mem[REG_OUTA] | (1 << (value1 & 31));
                }
                break;

                case 90: // drvc
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | (pasmvars->cflag << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | (pasmvars->cflag << (value1 & 31));
                }
                break;

                case 91: // drvnc
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | ((pasmvars->cflag ^ 1) << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | ((pasmvars->cflag ^ 1) << (value1 & 31));
                }
                break;

                case 92: // drvz
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | (pasmvars->zflag << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | (pasmvars->zflag << (value1 & 31));
                }
                break;

                case 93: // drvnz
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = (pasmvars->mem[REG_OUTB] & ~(1 << (value1 & 31))) | ((pasmvars->zflag ^ 1) << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = (pasmvars->mem[REG_OUTA] & ~(1 << (value1 & 31))) | ((pasmvars->zflag ^ 1) << (value1 & 31));
                }
                break;

                case 94: // drvrnd
                NotImplemented(instruct);
                break;

                case 95: // drvnot
                if (value1 & 32)
                {
                    rsltaddr = REG_OUTB;
                    result = pasmvars->mem[REG_DIRB] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRB, result);
                    pasmvars->mem[REG_DIRB] = result;
                    result = pasmvars->mem[REG_OUTB] ^ (1 << (value1 & 31));
                }
                else
                {
                    rsltaddr = REG_OUTA;
                    result = pasmvars->mem[REG_DIRA] | (1 << (value1 & 31));
                    if (pasmvars->printflag > 1)
                        fprintf(tracefile, ", cmem[%x] = %x", REG_DIRA, result);
                    pasmvars->mem[REG_DIRA] = result;
                    result = pasmvars->mem[REG_OUTA] ^ (1 << (value1 & 31));
                }
                break;

                case 96: // splitb
	        for (i = 0; i < 8; i++)
	        {
		    result = (result << 1) |
		        ((value1 >> 7)&0x1000000) | ((value1 >> 14)&0x10000) |
		        ((value1 >> 21)&0x100) | ((value1 >> 28)&1);
		    value1 <<= 4;
	        }
                break;

                case 97: // mergeb
	        for (i = 0; i < 8; i++)
	        {
		    result = (result << 4) |
		        ((value1 >> 28) & 8) | ((value1 >> 21) & 4) |
		        ((value1 >> 14) & 2) | ((value1 >>  7) & 1);
		    value1 <<= 1;
	        }
                break;

                case 98: // splitw
	        for (i = 0; i < 16; i++)
	        {
		    result = (result << 1) |
		        ((value1 >> 15)&0x10000) | ((value1 >> 30)&1);
		    value1 <<= 2;
	        }
                break;

                case 99: // mergew
	        for (i = 0; i < 16; i++)
	        {
		    result = (result << 2) |
		        ((value1 >> 30) & 2) | ((value1 >> 15) & 1);
		    value1 <<= 1;
	        }
                break;

                case 100: // seussf
                result = seuss(value1, 1);
                break;

                case 101: // seussr
                result = seuss(value1, 0);
                break;

                case 102: // rgbsqz
                result = (((value1 >> 27) & 0x1f) << 11) | (((value1 >> 18) & 0x3f) << 5) | ((value1 >> 11) & 0x1f);
                break;

                case 103: // rgbexp
                result = (((value1 >> 11) & 0x1f) << 27) | (((value1 >> 13) & 7) << 24) |
                         (((value1 >>  5) & 0x3f) << 18) | (((value1 >>  9) & 3) << 16) |
                         ((value1 & 0x1f) << 11) | (((value1 >> 2) & 7) << 8);  
                break;

                case 104: // xoro32
                NotImplemented(instruct);
                break;

                case 105: // rev
                for (value2 = 0; value2 < 32; value2++)
	        {
		    result = (result << 1) | (value1 & 1);
		    value1 >>= 1;
	        }
                break;

                case 106: // rczr
                result = (cflag << 31) | (zflag << 30) | ((uint32_t)value1 >> 2);
                zflag = value1 & 1;
                cflag = (value1 >> 1) & 1;
                break;

                case 107: // rczl
                result = (value1 << 2) | (cflag << 1) | zflag;
                zflag = (value1 >> 30) & 1;
                cflag = (value1 >> 31) & 1;
                break;

                case 108: // wrc
                result = cflag;
                break;

                case 109: // wrnc
                result = cflag ^ 1;
                break;

                case 110: // wrz
                result = zflag;
                break;

                case 111: // wrnz, modcz
                if (czi&1) // modcz
                {
                    write_czr &= 6;
                    temp  = ModifyFlag(cflag, zflag, value1 >> 4);
                    zflag = ModifyFlag(cflag, zflag, value1);
                    cflag = temp;
                }
                else // wrnz
                    result = zflag ^ 1;
                break;

                default:
                NotImplemented(instruct);
                break;
            }
            break;

            case 4: // jmp
//printf("jmp %d %x\n", rflag, instruct & ADDR_MASK);
            if (rflag)
            {
#ifdef OLD_WAY
                pasmvars->pc = (pc + pc_incr * (1 + instruct)) & ADDR_MASK;
#else
                if (pc & 0xfffffc00)
                    pasmvars->pc = (pc + 4 + instruct) & ADDR_MASK;
                else
                    pasmvars->pc = (pc + 1 + (instruct >> 2)) & 0x3ff;
#endif
            }
            else
                pasmvars->pc = instruct & ADDR_MASK;
            pasmvars->pc1 |= INVALIDATE_INSTR;
            write_czr &= 6;
            check_hubexec_mode(pasmvars);
            break;

            case 5: // call
            if (rflag)
            {
#ifdef OLD_WAY
                pasmvars->pc = (pc + pc_incr * (1 + instruct)) & ADDR_MASK;
#else
                if (pc & 0xfffffc00)
                    pasmvars->pc = (pc + 4 + instruct) & ADDR_MASK;
                else
                    pasmvars->pc = (pc + 1 + (instruct >> 2)) & 0x3ff;
#endif
            }
            else
                pasmvars->pc = instruct & ADDR_MASK;
            pasmvars->pc1 |= INVALIDATE_INSTR;
            pc += pc_incr;
            pc |= (pasmvars->zflag << 21) | (pasmvars->cflag << 20);
            pasmvars->retstack[pasmvars->retptr] = pc;
            pasmvars->retptr = (pasmvars->retptr + 1) & 7;
            if (pasmvars->retptr == 0)
                printf("return stack overflow%s", NEW_LINE);
            write_czr &= 6;
            check_hubexec_mode(pasmvars);
            break;

            case 6: // calla
            if (rflag)
            {
#ifdef OLD_WAY
                pasmvars->pc = (pc + pc_incr * (1 + instruct)) & ADDR_MASK;
#else
                if (pc & 0xfffffc00)
                    pasmvars->pc = (pc + 4 + instruct) & ADDR_MASK;
                else
                    pasmvars->pc = (pc + 1 + (instruct >> 2)) & 0x3ff;
#endif
            }
            else
                 pasmvars->pc = instruct & ADDR_MASK;
            pasmvars->pc1 |= INVALIDATE_INSTR;
            pc += pc_incr;
            pc |= (pasmvars->zflag << 21) | (pasmvars->cflag << 20);
            write_unaligned_long(pasmvars->ptra, pc);
            pasmvars->ptra = (pasmvars->ptra + 4) & ADDR_MASK;
            write_czr &= 6;
            check_hubexec_mode(pasmvars);
            break;

            case 7: // callb
            if (rflag)
            {
#ifdef OLD_WAY
                pasmvars->pc = (pc + pc_incr * (1 + instruct)) & ADDR_MASK;
#else
                if (pc & 0xfffffc00)
                    pasmvars->pc = (pc + 4 + instruct) & ADDR_MASK;
                else
                    pasmvars->pc = (pc + 1 + (instruct >> 2)) & 0x3ff;
#endif
            }
            else
                 pasmvars->pc = instruct & ADDR_MASK;
            pasmvars->pc1 |= INVALIDATE_INSTR;
            pc += pc_incr;
            pc |= (pasmvars->zflag << 21) | (pasmvars->cflag << 20);
            write_unaligned_long(pasmvars->ptrb, pc);
            pasmvars->ptrb = (pasmvars->ptrb + 4) & ADDR_MASK;
            write_czr &= 6;
            check_hubexec_mode(pasmvars);
            break;
        }
        break;

        case 14: // calld, loc
        rsltaddr = (opcode & 3) + 0x1f6;
        if (opcode & 4) // loc
        {
            if (czi & 4)
                result = (pc + value2) << 2;
            else
                result = value2 << 2;
            result &= ADDR_MASK;
        }
        else // calld
        {
            if (czi & 4)
                 pasmvars->pc = (pc + pc_incr + instruct) & ADDR_MASK;
            else
                 pasmvars->pc = instruct & ADDR_MASK;
            pasmvars->pc1 |= INVALIDATE_INSTR;
            result = (pc + pc_incr) | (pasmvars->zflag << 21) | (pasmvars->cflag << 20);
            cflag = (result >> 20) & 1;
            zflag = (result >> 21) & 1;
            write_czr |= 1;
            check_hubexec_mode(pasmvars);
        }
	break;

        case 15: // augs, augd
        if (opcode & 4) // augd
        {
            pasmvars->augdflag = 1;
            pasmvars->augdvalue = instruct & 0x007fffff;
        }
        else // augs
        {
            pasmvars->augsflag = 1;
            pasmvars->augsvalue = instruct & 0x007fffff;
        }
        write_czr &= 6;
        break;
    }

    // Conditionally update flags and write result
    if (write_czr & 2)
    {
	pasmvars->zflag = zflag;
        if (pasmvars->printflag > 1) fprintf(tracefile, ", z = %d", zflag);
    }
    if (write_czr & 4)
    {
	pasmvars->cflag = cflag;
        if (pasmvars->printflag > 1) fprintf(tracefile, ", c = %d", cflag);
    }
    if ((write_czr & 1) && rsltaddr != -1)
    {
        pasmvars->lastd = result;
        if (memflag == 2)
        {
            pasmvars->lut[rsltaddr] = result;
            if (pasmvars->printflag > 1)
                fprintf(tracefile, ", clut[%x] = %x", rsltaddr, result);
        }
        else if (rsltaddr == REG_PTRA)
        {
	    pasmvars->ptra = result;
            if (pasmvars->printflag > 1)
                fprintf(tracefile, ", ptra = %x", result);
        }
        else if (rsltaddr == REG_PTRB)
        {
	    pasmvars->ptrb = result;
            if (pasmvars->printflag > 1)
                fprintf(tracefile, ", ptrb = %x", result);
        }
        else
        {
            pasmvars->mem[rsltaddr] = result;
            if (pasmvars->printflag > 1)
                fprintf(tracefile, ", cram[%x] = %x", rsltaddr, result);
        }
	// Check if we need to update the pins
	if (rsltaddr >= REG_DIRA && rsltaddr <= REG_OUTB) UpdatePins2();
    }
#if 0
    if (pasmvars->waitflag)
    {
        fprintf(tracefile, "XXXXXXXXXX BAD XXXXXXXXXXXXXXX\n");
	pasmvars->waitflag--;
	if (!pasmvars->waitflag) pasmvars->waitmode = 0;
    }
#endif

    // Check for post return
    if (post_ret)
    {
        pasmvars->retptr = (pasmvars->retptr - 1) & 7;
        temp = pasmvars->retstack[pasmvars->retptr];
        if (pasmvars->retptr == 7)
            printf("return stack underflow%s", NEW_LINE);
        pasmvars->pc = temp & ADDR_MASK;
        pasmvars->pc1 |= INVALIDATE_INSTR;
        if (pasmvars->printflag > 1) fprintf(tracefile, ", RET %x", temp);
    }

    // Clear the alternate register flags
    //if (pasmvars->altsflag) printf("\nClear altsflag\n");
    if (!alt_instr)
    {
        pasmvars->altiflag = 0;
        pasmvars->altsflag = 0;
        pasmvars->altdflag = 0;
        pasmvars->altrflag = 0;
        pasmvars->altnflag = 0;
        pasmvars->altsvflag = 0;
    }

    if (!pasmvars->rwrep)
    {
        pasmvars->phase = 0; // Change phase to fetch mode
        pasmvars->qreg = 0;
        pasmvars->memflag = 0;
        pasmvars->skip_mask >>= 1;
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
