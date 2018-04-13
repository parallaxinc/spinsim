/*******************************************************************************
' Author: Dave Hein
' Version 0.21
' Copyright (c) 2010, 2011
' See end of file for terms of use.
'******************************************************************************/
//#include <sys/types.h>
#include <stdint.h>

#define BYTE(addr) (((uint8_t *)hubram)[MAP_ADDR(addr)])
#define WORD(addr) (((uint16_t *)hubram)[MAP_ADDR(addr) >> 1])
#define LONG(addr) (((int32_t *)hubram)[MAP_ADDR(addr) >> 2])

// Define system I/O addresses and commands
#define SYS_COMMAND    0x12340000
#define SYS_LOCKNUM    0x12340002
#define SYS_PARM       0x12340004
#define SYS_DEBUG      0x12340008

#define INVALIDATE_INSTR 0x80000000

typedef struct SerialS {
    int32_t mode;
    int32_t flag;
    int32_t state;
    int32_t count;
    int32_t value;
    int32_t pin_num;
    int32_t bitcycles;
} SerialT;

// This struct is used by the PASM simulator
typedef struct PasmVarsS {
    int32_t mem[512];
    int32_t state;
    int32_t pc;
    int32_t cflag;
    int32_t zflag;
    int32_t cogid;
    int32_t waitflag;
    int32_t waitmode;
    int32_t breakpnt;
    // P2 variables
    int32_t lut[512];
    int32_t instruct1;
    int32_t instruct2;
    int32_t instruct3;
    int32_t instruct4;
    int32_t pc1;
    int32_t pc2;
    int32_t pc3;
    int32_t pc4;
    int32_t str_fifo_buffer[16];
    int32_t str_fifo_rindex;
    int32_t str_fifo_windex;
    int32_t str_fifo_head_addr;
    int32_t str_fifo_tail_addr;
    int32_t str_fifo_addr0;
    int32_t str_fifo_addr1;
    int32_t str_fifo_mode;
    int32_t str_fifo_work_word;
    int32_t str_fifo_work_flag;
    int32_t ptra;
    int32_t ptra0;
    int32_t ptrb;
    int32_t ptrb0;
    int32_t ptrx;
    int32_t ptry;
    int32_t inda;
    int32_t inda0;
    int32_t indabot;
    int32_t indatop;
    int32_t indb;
    int32_t indb0;
    int32_t indbbot;
    int32_t indbtop;
    int32_t repcnt;
    int32_t repbot;
    int32_t reptop;
    int32_t repforever;
    int32_t retstack[8];
    int32_t printflag;
    int32_t retptr;
    int32_t memflag;
    int32_t qreg;
    int32_t qxreg;
    int32_t qyreg;
    int32_t qxposted;
    int32_t qyposted;
    int32_t cordic_count;
    int32_t cordic_depth;
    int32_t qxqueue[3];
    int32_t qyqueue[3];
    int32_t augsvalue;
    int32_t augsflag;
    int32_t augdvalue;
    int32_t augdflag;
    int32_t altsflag;
    int32_t altsvalue;
    int32_t altdflag;
    int32_t altdvalue;
    int32_t altrflag;
    int32_t altrvalue;
    int32_t altiflag;
    int32_t altivalue;
    int32_t altnflag;
    int32_t altnvalue;
    int32_t altsvflag;
    int32_t altsvvalue;
    int32_t prefetch;
    int32_t sqrt;
    int32_t lastd;
    int32_t phase;
    int32_t rwrep;
    int32_t cntreg1;
    int32_t cntreg2;
    int32_t cntreg3;
    int32_t intflags;
    int32_t intstate;
    int32_t intenable1;
    int32_t intenable2;
    int32_t intenable3;
    int32_t pinpatmode;
    int32_t pinpatmask;
    int32_t pinpattern;
    int32_t pinedge;
    int32_t lockedge;
    int32_t rdl_mask;
    int32_t wrl_mask;
    int32_t blnpix_var;
    int32_t mixpix_mode;
    int32_t share_lut;
    uint32_t skip_mask;
    uint32_t skip_mode;
    SerialT serina;
    SerialT serinb;
    SerialT serouta;
    SerialT seroutb;
    int64_t acca;
    int64_t accb;
    int64_t mul;
} PasmVarsT;

// This struct is used by the Spin simulator
// It is a subset of PasmVarsT starting at mem[0x1e0]
typedef struct SpinVarsS {
    int32_t x1e0;     // $1e0
    int32_t x1e1;     // $1e1
    int32_t x1e2;     // $1e2
    int32_t x1e3;     // $1e3
    int32_t x1e4;     // $1e4
    int32_t masklong; // $1e5
    int32_t masktop;  // $1e6
    int32_t maskwr;   // $1e7
    int32_t lsb;      // $1e8
    int32_t id;       // $1e9
    int32_t dcall;    // $1ea
    int32_t pbase;    // $1eb
    int32_t vbase;    // $1ec
    int32_t dbase;    // $1ed
    int32_t pcurr;    // $1ee
    int32_t dcurr;    // $1ef
    int32_t par;      // $1f0
    int32_t cnt;      // $1f1
    int32_t ina;      // $1f2
    int32_t inb;      // $1f3
    int32_t outa;     // $1f4
    int32_t outb;     // $1f5
    int32_t dira;     // $1f6
    int32_t dirb;     // $1f7
    int32_t ctra;     // $1f8
    int32_t ctrb;     // $1f9
    int32_t frqa;     // $1fa
    int32_t frqb;     // $1fb
    int32_t phsa;     // $1fc
    int32_t phsb;     // $1fd
    int32_t vcfg;     // $1fe
    int32_t vscl;     // $1ff
    int32_t state;
} SpinVarsT;

void RebootProp(void);
int32_t GetCnt(void);
void UpdatePins(void);
int32_t MAP_ADDR(int32_t addr);
void StartCog(SpinVarsT *spinvars, int par, int cogid);
int  CheckSerialIn(SerialT *serial);
void CheckSerialOut(SerialT *serial);
int  SerialSend(SerialT *serial, int portval);
void SerialReceive(SerialT *serial, int portval);
void SerialInit(SerialT *serial, int pin_num, int baudrate, int mode);
void StartPasmCog(PasmVarsT *pasmvars, int par, int addr, int cogid);
void StartPasmCog2(PasmVarsT *pasmvars, int par, int addr, int cogid, int hubexec);
void StartPasmCog3(PasmVarsT *pasmvars, int par, int addr, int cogid);
void DebugPasmInstruction(PasmVarsT *pasmvars);
void DebugPasmInstruction2(PasmVarsT *pasmvars);
void DebugPasmInstruction3(PasmVarsT *pasmvars);
int  ExecutePasmInstruction(PasmVarsT *pasmvars);
int  ExecutePasmInstruction2(PasmVarsT *pasmvars);
int  ExecutePasmInstruction3(PasmVarsT *pasmvars);
/*
+ -----------------------------------------------------------------------------------------------------------------------------+
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
