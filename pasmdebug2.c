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

#define REG_PINA 0x1f4

extern char *hubram;
extern int32_t memsize;
extern int32_t loopcount;
extern int32_t cycleaccurate;
extern int32_t pin_val;

extern FILE *tracefile;

static char *condnames[16] = {
    "if_never    ", "if_nz_and_nc", "if_z_and_nc ", "if_nc       ",
    "if_nz_and_c ", "if_nz       ", "if_z_ne_c   ", "if_nz_or_nc ",
    "if_z_and_c  ", "if_z_eq_c   ", "if_z        ", "if_z_or_nc  ",
    "if_c        ", "if_nz_or_c  ", "if_z_or_c   ", "            "};

char *opcodes_lower[64] = {
    "rdbyte","rdbytec","rdword","rdwordc","rdlong","rdlongc","rdaux","rdauxr",
    "isob","notb","clrb","setb","setbc","setbnc","setbz","setbnz",
    "andn","and","or","xor","muxc","muxnc","muxz","muxnz",
    "ror","rol","shr","shl","rcr","rcl","sar","rev",
    "mov","not","abs","neg","negc","negnc","negz","negnz",
    "add","sub","addx","subx","adds","subs","addsx","subsx",
    "sumc","sumnc","sumz","sumnz","min","max","mins","maxs",
    "addabs","subabs","incmod","decmod","cmpsub","subr","mul","scl"};

char *opcodes_upper[62][4] = {
    {"decod2", "decod2", "decod2", "decod2"},
    {"decod3", "decod3", "decod3", "decod3"},
    {"decod4", "decod4", "decod4", "decod4"},
    {"decod5", "decod5", "decod5", "decod5"},
    {"encod",  "blmask", "encod",  "blmask"},
    {"onecnt", "zercnt", "onecnt", "zercnt"},
    {"incpat", "incpat", "decpat", "decpat"},
    {"splitb", "mergeb", "splitw", "mergew"},

    {"getnib", "setnib", "getnib", "setnib"},
    {"getnib", "setnib", "getnib", "setnib"},
    {"getnib", "setnib", "getnib", "setnib"},
    {"getnib", "setnib", "getnib", "setnib"},
    {"getword","setword","getword","setword"},
    {"setwrds","rolnib", "rolbyte","rolword"},
    {"sets",   "setd",   "setx",   "seti"},
    {"cognew", "cognew", "waitcnt","waitcnt"},

    {"getbyte","setbyte","getbyte","setbyte"},
    {"getbyte","setbyte","getbyte","setbyte"},
    {"setbyts","movbyts","packrgb","unpkrgb"},
    {"addpix", "mulpix", "blnpix", "mixpix"},
    {"jmpsw",  "jmpsw",  "jmpsw",  "jmpsw"},
    {"jmpswd", "jmpswd", "jmpswd", "jmpswd"},
    {"ijz",    "ijzd",   "ijnz",   "ijnzd"},
    {"djz",    "djzd",   "djnz",   "djnzd"},

    {"testb",   "testb",   "testb",   "testb"},
    {"testn",  "testn",  "testn",  "testn"},
    {"test",   "test",   "test",   "test"},
    {"cmp",    "cmp",    "cmp",    "cmp"},
    {"cmpx",   "cmpx",   "cmpx",   "cmpx"},
    {"cmps",   "cmps",   "cmps",   "cmps"},
    {"cmpsx",  "cmpsx",  "cmpsx",  "cmpsx"},
    {"cmpr",   "cmpr",   "cmpr",   "cmpr"},

    {"coginit","waitvid","coginit","waitvid"},
    {"coginit","waitvid","coginit","waitvid"},
    {"coginit","waitvid","coginit","waitvid"},
    {"coginit","waitvid","coginit","waitvid"},
    {"waitpeq","waitpeq","waitpeq","waitpeq"},
    {"waitpeq","waitpeq","waitpeq","waitpeq"},
    {"waitpne","waitpne","waitpne","waitpne"},
    {"waitpne","waitpne","waitpne","waitpne"},

    {"wrbyte", "wrbyte", "wrword", "wrword"},
    {"wrlong", "wrlong", "frac",   "frac"},
    {"wraux",  "wraux",  "wrauxr", "wrauxr"},
    {"setacca","setacca","setaccb","setaccb"},
    {"maca",   "maca",   "macb",   "macb"},
    {"mul32",  "mul32",  "mul32u", "mul32u"},
    {"div32",  "div32",  "div32u", "div32u"},
    {"div64",  "div64",  "div64u", "div64u"},

    {"sqrt64", "sqrt64", "qsincos","qsincos"},
    {"qarctan","qarctan","qrotate","qrotate"},
    {"setsera","setsera","setserb","setserb"},
    {"setctrs","setctrs","setwavs","setwavs"},
    {"setfrqs","setfrqs","setphss","setphss"},
    {"addphss","addphss","subphss","subphss"},
    {"jp",     "jp",     "jpd",    "jpd"},
    {"jnp",    "jnp",    "jnpd",   "jnpd"},

    {"cfgpins","cfgpins","cfgpins","cfgpins"},
    {"cfgpins","cfgpins","jmptask","jmptask"},
    {"setxfr", "setxfr", "setmix", "setmix"},
    {"jz",     "jzd",    "jnz",    "jnzd"},
    {"locbase","locbyte","locword","loclong"},
    {"jmplist","locinst","augs",   "augd"}};

int zci_upper[62][4] = {
    {7, 7, 7, 7},  {7, 7, 7, 7},  {7, 7, 7, 7},  {7, 7, 7, 7},
    {5, 5, 5, 5},  {5, 5, 5, 5},  {3, 3, 3, 3},  {1, 1, 1, 1},

    {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},
    {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},  {3, 3, 3, 3},

    {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},
    {7, 7, 7, 7},  {7, 7, 7, 7},  {1, 1, 1, 1},  {1, 1, 1, 1},

    {7, 7, 7, 7},  {7, 7, 7, 7},  {7, 7, 7, 7},  {7, 7, 7, 7},
    {7, 7, 7, 7},  {7, 7, 7, 7},  {7, 7, 7, 7},  {7, 7, 7, 7},

    {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},
    {3, 3, 3, 3},  {3, 3, 3, 3},  {3, 3, 3, 3},  {3, 3, 3, 3},

    {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},
    {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},

    {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},
    {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},

    {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1},  {1, 1, 1, 1}};

char *opcodes_ind[16] = {
    "invalid", "fixinda", "setinda", "setinda",
    "fixindb", "fixinds", "invalid", "invalid",
    "setindb", "invalid", "setinds", "setinds",
    "setindb", "invalid", "setinds", "setinds"};

char *opcodes_16[14] = {
    "locptra","locptrb","jmp","jmpd","call","calld","calla",
    "callad","callb","callbd","callx","callxd","cally","callyd"};

char *opcodes_set1[49] = {
    "cogid","taskid","locknew","getlfsr","getcnt","getcntx","getacal","getacah",
    "getacbl","getacbh","getptra","getptrb","getptrx","getptry","serina","serinb",
    "getmull","getmulh","getdivq","getdivr","getsqrt","getqx","getqy","getqz",
    "getphsa","getphza","getcosa","getsina","getphsb","getphzb","getcosb","getsinb",
    "pushzc","popzc","subcnt","getpix","binbcd","bcdbin","bingry","grybin",
    "eswap4","eswap8","seussf","seussr","incd","decd","incds","decds","pop"};

char *opcodes_set2[] = {
    "clkset", "cogstop", "lockset", "lockclr", "lockret", "rdwidec", "rdwide",
    "wrwide", "getp", "getnp", "serouta", "seroutb", "cmpcnt", "waitpx",
    "waitpr", "waitpf", "setzc", "setmap", "setxch", "settask", "setrace",
    "saracca", "saraccb", "saraccs", "setptra", "setptrb", "addptra", "addptrb",
    "subptra", "subptrb", "setwide", "setwidz", "setptrx", "setptry",
    "addptrx", "addptry", "subptrx", "subptry", "passcnt", "wait", "offp",
    "notp", "clrp", "setp", "setpc", "setpnc", "setpz", "stpnz", "div64d",
    "sqrt32", "qlog", "qexp", "setqi", "setqz", "cfgdacs", "setdacs",
    "cfgdac0", "cfgdac1", "cfgdac2", "cfgdac3", "setdac0", "setdac1",
    "setdac2", "seddac3", "setctra", "setwava", "setfrqa", "setphsa",
    "addphsa", "subphsa", "setvid", "setvidy", "setctrb", "setwavb", "setfrqb",
    "setphsb", "addphsb", "subphsb", "setvidi", "setvidq", "setpix", "setpixz",
    "setpixu", "setpixv", "setpixa", "setpixr", "setpixg", "setpixb",
    "setpora", "setporb", "setporc", "setpord", "push"};
    
char *opcodes_set3[40] = {
    "jmp", "jmpd", "call", "calld", "calla", "callad", "callb", "callbd",
    "callx", "callxd", "cally", "callyd", "reta", "retad", "retb", "retbd",
    "retx", "retxd", "rety", "retyd", "ret", "retd", "polctra", "polctrb",
    "polvid", "capctra", "capctrb", "capctrs", "setpixw", "clracca", "clraccb",
    "clraccs", "chkptrx", "chkptry", "syntra", "synctrb", "dcachex", "icachex",
    "icachep", "icachen"};

int zci_set3[40] = {
                 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
    6, 6, 6, 6,  6, 6, 6, 6,  6, 6, 6, 6,  6, 0, 0, 0,
    0, 0, 0, 0,  6, 6, 0, 0,  0, 0, 0, 0};


char *GetOpname2(unsigned int instr, int *pzci)
{
    int opcode = instr >> 25;
    int zci = (instr >> 22) & 7;
    int cond = (instr >> 18) & 15;
    int src = instr & 0x1ff;
    int dst = (instr >> 9) & 0x1ff;
    int zc = zci >> 1;

    //printf("opcode = $%2.2x, zci = %d\n", opcode, zci);
   
    if (opcode < 64)
    {
	*pzci = 7;
        return opcodes_lower[opcode];
    }
    else if(opcode < 126)
    {
	*pzci = zci_upper[opcode-64][zc];
        return opcodes_upper[opcode-64][zc];
    }
    else if (opcode == 126 && zci == 0)
    {
        *pzci = 0;
        return opcodes_ind[cond];
    }
    else if (opcode == 126)
    {
        *pzci = 0;
        return opcodes_16[(zci-1)*2 + (dst >> 8)];
    }
    else if (src <= 48)
    {
        if (src < 44 || src == 48)
            *pzci = 7;
        else
            *pzci = 5;
        return opcodes_set1[src];
    }
    if (src < 64)
    {
        *pzci = 0;
        return "invalid";
    }
    if (src < 128)
    {
        *pzci = 1;
        return "repd";
    }
    if (src < 220)
    {
        if (src == 136 || src == 137 || src == 144)
            *pzci = 7;
        else if (src == 130 || src == 131)
            *pzci = 3;
        else
            *pzci = 1;
        return opcodes_set2[src-128];
    }
    if (src < 0xf4)
    {
        *pzci = 0;
        return "invalid";
    }
    if (src <= 0x11b)
    {
        *pzci = zci_set3[src-0xf4];
        return opcodes_set3[src-0xf4];
    }
    *pzci = 0;
    return "invalid";
}

void StartPasmCog2(PasmVarsT *pasmvars, int32_t par, int32_t addr, int32_t cogid)
{
    int32_t i;

    // printf("\nStartPasmCog2: %8.8x, %8.8x, %d\n", par, addr, cogid);
    par &= 0x3ffff;
    addr &= 0x3fffc;
    pasmvars->waitflag = 0;
    pasmvars->cflag = 0;
    pasmvars->zflag = 0;
    pasmvars->pc = 0;
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
    pasmvars->dcachehubaddr = 0xffffffff;
    pasmvars->dcachecogaddr = 0xffffffff;
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
    pasmvars->mulcount = 0;
    pasmvars->breakpnt = -1;
    pasmvars->augsflag = 0;
    pasmvars->augsvalue = 0;
    pasmvars->augdflag = 0;
    pasmvars->augdvalue = 0;
    pasmvars->icachehubaddr[0] = 0xffffffff;
    pasmvars->icachehubaddr[1] = 0xffffffff;
    pasmvars->icachehubaddr[2] = 0xffffffff;
    pasmvars->icachehubaddr[3] = 0xffffffff;
    pasmvars->icachenotused[0] = 0;
    pasmvars->icachenotused[1] = 0;
    pasmvars->icachenotused[2] = 0;
    pasmvars->icachenotused[3] = 0;
    pasmvars->prefetch = 1;

    for (i = 0; i < 0x1f4; i++)
    {
	pasmvars->mem[i] = LONG(addr);
	addr += 4;
    }
    for (i = 0x1f4; i < 512; i++) pasmvars->mem[i] = 0;
}

void DebugPasmInstruction2(PasmVarsT *pasmvars)
{
    int32_t i;
    int32_t cflag = pasmvars->cflag;
    int32_t zflag = pasmvars->zflag;
    int32_t instruct, pc, cond, xflag;
    int32_t opcode, zci;
    int32_t srcaddr, dstaddr;
    char *wzstr = "";
    char *wcstr = "";
    char opstr[20];
    char *istr[3] = {" ", "#", "@"};
    char *xstr[8] = {" ", "X", "I", "H", "C", "P", "W", "?"};
    int zci_mask;
    int32_t sflag, dflag, indirect, opcode_zci;

    // Fetch the instruction
    pc = pasmvars->pc4;
    instruct = pasmvars->instruct4;
    cond = (instruct >> 18) & 15;

    // Extract parameters from the instruction
    opcode = (instruct >> 25) & 127;
    srcaddr = instruct & 511;
    dstaddr = (instruct >> 9) & 511;
    zci = (instruct >> 22) & 7;
    opcode_zci = (opcode << 3) | zci;

    // Decode the immediate flags for the source and destination fields
    sflag = (opcode_zci >= 0x3ea) | (zci & 1);

    dflag = (opcode >= 0x68 && opcode <= 0x7a && (zci & 2)) |
	(opcode_zci >= 0x302 && opcode_zci <= 0x31f) | 
	(opcode != 0x7f && opcode_zci >= 0x3eb) | 
	(opcode == 0x7f && srcaddr >= 0x40 && srcaddr <= 0xdc && (zci & 1)) |
	(opcode == 0x7f && srcaddr >= 0x100);

    // Determine if indirect registers are used
    indirect = (!sflag && (srcaddr & 0x1fe) == 0x1f2) |
	(!dflag && (dstaddr & 0x1fe) == 0x1f2) |
	(opcode_zci == 0x3f0);

    if (indirect) cond = 0xf;
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
	else
	    xflag = 7;
    }

    strcpy(opstr, GetOpname2(instruct, &zci_mask));

    zci &= zci_mask;
    if (zci & 4) wzstr = " wz";
    if (zci & 2) wcstr = " wc";

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

    // Check for REPS
    if ((instruct & 0xffc00000) == 0xfac00000)
    {
	cond = 15;
	if (xflag == 1) xflag = 0;
	strcpy(opstr, "reps   ");
    }

    // Check for AUGS or AUGD
    if (opcode_zci >= 0x3ec && opcode_zci <= 0x3ef)
    {
	cond = 15;
	if (xflag == 1) xflag = 0;
    }

    if (pasmvars->printflag - 2 < xflag && xflag != 0)
    {
	pasmvars->printflag = 0;
	return;
    }

    fprintf(tracefile, "Cog %d: %8.8x ", pasmvars->cogid, loopcount);
    fprintf(tracefile, "%4.4x %8.8x %s %s %s %s%3.3x, %s%3.3x%s%s", pc,
        instruct, xstr[xflag], condnames[cond], opstr, istr[dflag], dstaddr,
	istr[sflag], srcaddr, wzstr, wcstr);
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
