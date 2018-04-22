/*******************************************************************************
' Author: Dave Hein
' Version 0.002
' Copyright (c) 2017
' See end of file for terms of use.
'******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define OPCODE_AMB_CALLD 1
#define OPCODE_BAD_BITS  2
#define OPCODE_BAD_ADDR  3
#define OPCODE_INVALID   4

static int hubstart = 0x1000;

static char *modczparms[] = {
    "_clr", "_nc_and_nz", "_nc_and_z", "_nc", "_c_and_nz", "_nz", "_c_ne_z", "_nc_or_nz",
    "_c_and_z", "_c_eq_z", "_z", "_nc_or_z", "_c", "_c_or_nz", "_c_or_z", "_set"};

static char *condnames[16] = {
    "_ret_       ", "if_nz_and_nc", "if_z_and_nc ", "if_nc       ",
    "if_nz_and_c ", "if_nz       ", "if_z_ne_c   ", "if_nz_or_nc ",
    "if_z_and_c  ", "if_z_eq_c   ", "if_z        ", "if_z_or_nc  ",
    "if_c        ", "if_nz_or_c  ", "if_z_or_c   ", "            "};

static char *group1[] = {
    "ror", "rol", "shr", "shl", "rcr", "rcl", "sar", "sal",
    "add", "addx", "adds", "addsx", "sub", "subx", "subs", "subsx",
    "cmp", "cmpx", "cmps", "cmpsx", "cmpr", "cmpm", "subr", "cmpsub",
    "fge", "fle", "fges", "fles", "sumc", "sumnc", "sumz", "sumnz",
    "bitl", "bith", "bitc", "bitnc", "bitz", "bitnz", "bitrnd", "bitnot",
    "and", "andn", "or", "xor", "muxc", "muxnc", "muxz", "muxnz",
    "mov", "not", "abs", "neg", "negc", "negnc", "negz", "negnz",
    "incmod", "decmod", "zerox", "signx", "encod", "ones", "test", "testn"};

static char *group2[] = {
    "setnib", "setnib", "setnib", "setnib", "setnib", "setnib", "setnib", "setnib",
    "getnib", "getnib", "getnib", "getnib", "getnib", "getnib", "getnib", "getnib",
    "rolnib", "rolnib", "rolnib", "rolnib", "rolnib", "rolnib", "rolnib", "rolnib",
    "setbyte", "setbyte", "setbyte", "setbyte", "getbyte", "getbyte", "getbyte", "getbyte",
    "rolbyte", "rolbyte", "rolbyte", "rolbyte", "setword", "setword", "getword", "getword",
    "rolword", "rolword", "altsn", "altgn", "altsb", "altgb", "altsw", "altgw",
    "altr", "altd", "alts", "altb", "alti", "setr", "setd", "sets",
    "decod", "bmask", "crcbit", "crcnib", "muxnits", "muxnibs", "muxq", "movbyts",
    "mul", "mul", "muls", "muls", "sca", "sca", "scas", "scas",
    "addpix", "mulpix", "blnpix", "mixpix", "addct1", "addct2", "addct3", "wmlong",
    "rqpin", "rdpin", "rqpin", "rdpin", "rdlut", "rdlut", "rdlut", "rdlut",
    "rdbyte", "rdbyte", "rdbyte", "rdbyte", "rdword", "rdword", "rdword", "rdword",
    "rdlong", "rdlong", "rdlong", "rdlong", "calld", "calld", "calld", "calld",
    "callpa", "callpa", "callpb", "callpb", "djz", "djnz", "djf", "djnf",
    "ijz", "ijnz", "tjz", "tjnz", "tjf", "tjnf", "tjs", "tjns", "tjv"};

static char *group3[] = {
    "jint", "jct1", "jct2", "jct3", "jse1", "jse2", "jse3", "jse4",
    "jpat", "jfbw", "jxmt", "jxfi", "jxro", "jxrl", "jatn", "jqmt",
    "jnint", "jnct1", "jnct2", "jnct3", "jnse1", "jnse2", "jnse3", "jnse4",
    "jnpat", "jnfbw", "jnxmt", "jnxfi", "jnxro", "jnxrl", "jnatn", "jnqmt"};

static char *group4 = "setpat";

static char *group5[] = {
    "wrpin", "wxpin", "wypin", "wrlut", "wrbyte", "wrword", "wrlong", "rdfast",
    "wrfast", "fblock", "xinit", "xzero", "xcont", "rep", "coginit", "coginit",
    "qmul", "qdiv", "qfrac", "qsqrt", "qrotate", "qvector"};

static char *group6[] = {
    "hubset", "cogid", "invalid", "cogstop", "locknew", "lockret", "locktry", "lockrel",
    "invalid", "invalid", "invalid", "invalid", "invalid", "invalid", "qlog", "qexp",
    "rfbyte", "rfword", "rflong", "rfvar", "rfvars", "wfbyte", "wfword", "wflong",
    "getqx", "getqy", "getct", "getrnd", "setdacs", "setxfrq", "getxacc", "waitx",
    "setse1", "setse2", "setse3", "setse4"};

static char immd6[] = {
    1, 5, 0, 1, 4, 1, 5, 5,
    0, 0, 0, 0, 0, 0, 1, 1,
    6, 6, 6, 6, 6, 1, 1, 1,
    6, 6, 0, 7, 1, 1, 0, 1,
    1, 1, 1, 1};

static char *group7[] = {
    "pollint", "pollct1", "pollct2", "pollct3", "pollse1", "pollse2", "pollse3", "pollse4",
    "pollpat", "pollfbw", "pollxmt", "pollxfi", "pollxro", "pollxrl", "pollatn", "pollqmt",
    "waitint", "waitct1", "waitct2", "waitct3", "waitse1", "waitse2", "waitse3", "waitse4",
    "waitpat", "waitfbw", "waitxmt", "waitxfi", "waitxro", "waitxrl", "waitatn", "invalid",
    "allowi", "stalli", "trgint1", "trgint2", "trgint3", "nixint1", "nixint2", "nixint3"};

static char *group8[] = {
    "setint1", "setint2", "setint3",
    "setq", "setq2", "push", "pop", "jmp", "call", "calla", "callb",
    "jmprel", "skip", "skipf", "execf", "getptr", "getbrk", "brk", "setluts",
    "setcy", "setci", "setcq", "setcfrq", "setcmod", "setpiv", "setpix", "cogatn",
    "dirl", "dirh", "dirc", "dirnc", "dirz", "dirnz", "dirrnd", "dirnot",
    "outl", "outh", "outc", "outnc", "outz", "outnz", "outrnd", "outnot",
    "fltl", "flth", "fltc", "fltnc", "fltz", "fltnz", "fltrnd", "fltnot",
    "drvl", "drvh", "drvc", "drvnc", "drvz", "drvnz", "drvrnd", "drvnot",
    "splitb", "mergeb", "splitw", "mergew", "seussf", "seussr", "rgbsqz", "rgbexp",
    "xoro32", "rev", "rczr", "rczl", "wrc", "wrnc", "wrz", "wrnz"};

static char immd8[] = {
    1, 1, 1,
    1, 0, 1, 6, 6, 6, 6, 6,
    1, 1, 1, 1, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 6, 6, 0, 0, 0, 0};

static char *group9[] = {
    "jmp", "call", "calla", "callb", "calld", "calld", "calld", "calld",
    "loc", "loc", "loc", "loc", "augs", "augs", "augs", "augs",
    "augd", "augd", "augd", "augd"};

#define OP_NOPARMS    0
#define OP_ONEPARM_D  1
#define OP_ONEPARM_S  2
#define OP_TWOPARMS   3
#define OP_MODCZ      4
#define OP_LOCPARMS   5
#define OP_INVALID    6
#define OP_NIBPARMS   7
#define OP_BYTEPARMS  8
#define OP_WORDPARMS  9
#define OP_AUGPARMS  10
#define OP_TWOREL9   11
#define OP_BIGJMP    12
#define OP_ONEREL9   13
#define OP_TESTB     14
#define OP_TESTP     15
#define OP_TWOPTR1   16
#define OP_TWOPTR2   17

char *GetOpname2(unsigned int instr, int *pczi, int *pformat, int *perrflag, int hubmode)
{
    int opcode = (instr >> 21) & 0x7f;
    static char name[100];
    int sfield = instr & 0x1ff;
    int dfield = (instr >> 9) & 0x1ff;
    int czflags = (instr >> 19) & 3;
    int cflag = (instr >> 20) & 1;
    int zflag = (instr >> 19) & 1;
    int lflag = (instr >> 18) & 1;

    //printf("opcode = $%2.2x, czflags = %d\n", opcode, czflags);
    *pformat = OP_TWOPARMS;

    *perrflag = 0;
    if (opcode <= 0x3f)
    {
        if (opcode >= 0x20 && opcode <= 0x27 && cflag != zflag)
        {
            if (opcode&1)
                strcpy(name, "testbn");
            else
                strcpy(name, "testb");
            *pformat = OP_TESTB;
        }
        else
            strcpy(name, group1[opcode]);
        *pczi = 6;
    }
    else if (opcode < 0x5e)
    {
        int index = ((opcode - 0x40) << 2) + czflags;
        if (index < 24)
            *pformat = OP_NIBPARMS;
        else if (index < 36)
            *pformat = OP_BYTEPARMS;
        else if (index < 42)
            *pformat = OP_WORDPARMS;
        else if (opcode >= 0x59)
            *pformat = OP_TWOREL9;
        else if ((opcode == 0x53 && czflags == 3) || (opcode >= 0x56 && opcode <= 0x58)) //WMLONG, RDXXXX
            *pformat = OP_TWOPTR1;
        strcpy(name, group2[index]);
        if (opcode == 0x50 || opcode == 0x51)
            *pczi = 2;
        else if (opcode == 0x54)
            *pczi = 4;
        else if (opcode >= 0x55 && opcode <= 0x59)
            *pczi = 6;
        else
            *pczi = 0;
    }
    else if (opcode == 0x5e)
    {
        if (cflag == 0)
        {
            if (zflag)
            {
                if (dfield > 0x1f)
                    strcpy(name, "invalid");
                else
                {
                    strcpy(name, group3[dfield]);
                    if (!zflag)
                        *perrflag = OPCODE_BAD_BITS;
                }
                *pformat = OP_ONEREL9;
            }
            else
            {
                strcpy(name, "tjv");
                *pformat = OP_TWOREL9;
            }
        }
        else
            strcpy(name, "invalid");
        *pczi = 0;
    }
    else if (opcode == 0x5f)
    {
        if (cflag)
            strcpy(name, group4);
        else
            strcpy(name, "invalid");
        *pczi = 0;
    }
    else if (opcode <= 0x6a)
    {
        if (opcode == 0x62 || (opcode == 0x63 && !cflag))
            *pformat = OP_TWOPTR1;
        strcpy(name, group5[((opcode - 0x60) << 1) + cflag]);
        if (opcode == 0x67)
            *pczi = 4;
        else
            *pczi = 0;
    }
    else if (opcode == 0x6b)
    {
        *pformat = OP_ONEPARM_D;
        if (sfield <= 0x23)
        {
            *pczi = immd6[sfield] & 6;
            strcpy(name, group6[sfield]);
            if (((instr >> 18) & ~immd6[sfield]) & 7)
                *perrflag = OPCODE_BAD_BITS;
            if (sfield == 27 && ((instr >> 18) & 1)) // getrnd
            {
                if ((instr >> 9) & 0x1ff)
                    *perrflag = OPCODE_BAD_BITS;
                else
                    *pformat = OP_NOPARMS;
            }
        }
        else if (sfield == 0x24)
        {
            *pformat = OP_NOPARMS;
            if (dfield > 0x27)
                strcpy(name, "invalid");
            else
                strcpy(name, group7[dfield]);
            if (dfield <= 0x1e)
            {
                *pczi = 6;
                if ((instr >> 18) & 1)
                    *perrflag = OPCODE_BAD_BITS;
            }
            else
            {
                *pczi = 0;
                if ((instr >> 18) & 7)
                    *perrflag = OPCODE_BAD_BITS;
            }
        }
        else if (sfield == 0x2d && lflag)
        {
            if (dfield)
                *perrflag = OPCODE_BAD_BITS;
            strcpy(name, "ret");
            *pformat = OP_NOPARMS;
            *pczi = 6;
        }
        else if (sfield == 0x2e && lflag)
        {
            if (dfield)
                *perrflag = OPCODE_BAD_BITS;
            strcpy(name, "reta");
            *pformat = OP_NOPARMS;
            *pczi = 6;
        }
        else if (sfield == 0x2f && lflag)
        {
            if (dfield)
                *perrflag = OPCODE_BAD_BITS;
            strcpy(name, "retb");
            *pformat = OP_NOPARMS;
            *pczi = 6;
        }
        else if (sfield >= 0x40 && sfield <= 0x47 && cflag != zflag)
        {
            if (sfield&1)
                strcpy(name, "testpn");
            else
                strcpy(name, "testp");
            *pformat = OP_TESTP;
            *pczi = 6;
        }
        else if (sfield == 0x6f && lflag)
        {
            *pformat = OP_MODCZ;
            if (dfield & 0x100)
                *perrflag = OPCODE_BAD_BITS;
            strcpy(name, "modcz");
            *pczi = 6;
        }
        else if (sfield > 0x6f)
        {
            strcpy(name, "invalid");
            *pczi = 0;
        }
        else
        {
            if (((instr >> 18) & ~immd8[sfield - 0x25]) & 7)
                *perrflag = OPCODE_BAD_BITS;
            strcpy(name, group8[sfield - 0x25]);
            *pczi = immd8[sfield - 0x25] & 6;
        }
    }
    else
    {
        strcpy(name, group9[opcode - 0x6c]);
        if (opcode >= 0x78)
            *pformat = OP_AUGPARMS;
        else if (opcode >= 0x70)
        {
            *pformat = OP_LOCPARMS;
            if (!hubmode && (instr & 0x00100000) && (instr & 3))
                *perrflag = OPCODE_BAD_BITS;
        }
        else
            *pformat = OP_BIGJMP;
	*pczi = 0;
    }

    if (!strcmp(name, "invalid"))
        *perrflag = OPCODE_INVALID;

    return name;
}

char *numstr(int value)
{
    static int index = 0;
    static char str[4][20];
    char *ptr = str[index];

    index = (index + 1) & 3;

    if (value >= 0)
    {
        if ( value <= 9)
            sprintf(ptr, "%d", value);
        else
            sprintf(ptr, "$%x", value);
    }
    else
    {
        value = -value;
        if ( value <= 9)
            sprintf(ptr, "-%d", value);
        else
            sprintf(ptr, "-$%x", value);
    }

    return ptr;
}

static char *testnames[] = {
    "", " wz", " wc", " wcz", "", " andz", " andc", " wcz",
    "", " orz", " orc", " wcz", "", " xorz", " xorc", " wcz"};

int InvalidAddress(int hubmode, int pc, int offset)
{
    int invalid;

    if (hubmode)
    {
        int target = pc + offset;
        invalid = (target < 0x400 || target >= 0x100000);
    }
    else
    {
        int target = (pc >> 2) + offset;
        invalid = (target < 0 || target >= 0x400);
    }


    return invalid;
}

char *ptrstr(int src, int *perrflag)
{
    static char str[100];
    int index = (src << 27) >> 27;

    if ((src & 0x40) && index == 0)
        *perrflag = OPCODE_BAD_BITS;
    if ((src & 0x60) == 0x20)
        *perrflag = OPCODE_BAD_BITS;
    str[0] = 0;
    if ((src & 0x70) == 0x40)
        strcat(str, "++");
    else if ((src & 0x70) == 0x50)
    {
        strcat(str, "--");
        index = -index;
    }
    if (src & 0x80)
        strcat(str, "ptrb");
    else
        strcat(str, "ptra");
    if ((src & 0x70) == 0x60)
        strcat(str, "++");
    else if ((src & 0x70) == 0x70)
    {
        strcat(str, "--");
        index = -index;
    }
    if (index == 16)
        *perrflag = OPCODE_BAD_BITS;
    if (index > 1 || index < -1 || (index & !(src & 0x40)))
        sprintf(str+strlen(str), "[%d]", index);
    return str;
}

void Disassemble2(int32_t instruct, int32_t pc, char *outstr, int *perrflag)
{
    int32_t i;
    int32_t opcode, czi;
    int32_t srcaddr, dstaddr;
    int32_t cond, format;
    char *wczstr = "";
    char opstr[20];
    char *istr[4] = {"", "#", "#", "#/"};
    int czi_mask;
    int32_t sflag = 0;
    int32_t dflag = 0;
    int hubmode = pc >= hubstart;

    cond = (instruct >> 28) & 15;

    // Extract parameters from the instruction
    opcode = (instruct >> 21) & 127;
    srcaddr = instruct & 511;
    dstaddr = (instruct >> 9) & 511;
    czi = (instruct >> 18) & 7;

    // Decode the immediate flags for the source and destination fields
    if ((czi & 1) && opcode <= 0x6a)
    {
#if 0
        if (!pasmvars->augsflag && (instruct & 0x100) && ((opcode >= 0x58 &&
            opcode <= 0x5a) || opcode == 0x62 || (opcode == 0x63 && !(czi&4))))
            sflag = 3;
        else if ((opcode >= 0x4e && opcode <= 0x4f) || opcode == 0x55 || opcode == 0x60)
            sflag = 2;
        else
            sflag = 1;
#else
        sflag = 1;
#endif
    }

    dflag = ((czi & 1) && (opcode == 0x6b)) ||
            ((czi & 2) && (opcode == 0x5a || opcode == 0x5e || (opcode == 0x5f && (czi & 4)) || (opcode >= 0x60 && opcode <= 0x6a)));

    // Check for extended address instructions
    if (opcode >= 0x6c)
    {
        if (opcode < 0x78)
        {
            srcaddr = instruct & 0xfffff;
            sflag = (czi >> 2) + 1;
            if (opcode >= 0x78)
                dstaddr = 0x1f6 + (opcode & 3);
        }
        else
        {
            srcaddr = instruct & 0x7fffff;
            sflag = 1;
        }
    }

    strcpy(opstr, GetOpname2(instruct, &czi_mask, &format, perrflag, hubmode));
    //if (errflag == OPCODE_BAD_BITS) strcpy(opstr, "invalid");

    czi &= czi_mask;
    if ((czi & 6) == 6) wczstr = " wcz";
    else if (czi & 4) wczstr = " wc";
    else if (czi & 2) wczstr = " wz";

    i = strlen(opstr);
    while (i < 7) opstr[i++] = ' ';
    opstr[i] = 0;

    // Check for NOP
    if (!instruct)
    {
	cond = 15;
	strcpy(opstr, "nop    ");
        format = OP_NOPARMS;
    }

    if (!strcmp(opstr, "invalid"))
        sprintf(outstr, " %s", opstr);
    else if (format == OP_MODCZ)
        sprintf(outstr, " %s %s %s, %s%s", condnames[cond], opstr,
            modczparms[(dstaddr >> 4) & 15], modczparms[dstaddr & 15], wczstr);
    else if (format == OP_LOCPARMS)
    {
        if (instruct & 0x00100000)
        {
            int AmbiguousCalld = 0;
            srcaddr = ((srcaddr << 12) >> 12) + 4;
            if (!strcmp(opstr, "calld  "))
            {
                if ((srcaddr & 3) == 0 && srcaddr <= 255*4 && srcaddr >= -256*4)
                    AmbiguousCalld = 1;
            }
            if (!hubmode) srcaddr >>= 2;
            if (InvalidAddress(hubmode, pc, srcaddr))
                *perrflag = OPCODE_BAD_ADDR;
            else if (AmbiguousCalld)
                *perrflag = OPCODE_AMB_CALLD;
            if (srcaddr >= 0)
                sprintf(outstr, " %s %s $%x, %s$+%s%s", condnames[cond],
                    opstr, 0x1f6 + ((instruct >> 21) & 3), istr[sflag], numstr(srcaddr), wczstr);
            else
                sprintf(outstr, " %s %s $%x, %s$-%s%s", condnames[cond],
                    opstr, 0x1f6 + ((instruct >> 21) & 3), istr[sflag], numstr(-srcaddr), wczstr);
        }
        else
            sprintf(outstr, " %s %s $%x, %s\\%s%s", condnames[cond],
                opstr, 0x1f6 + ((instruct >> 21) & 3), istr[sflag], numstr(srcaddr), wczstr);
    }
    else if (format == OP_AUGPARMS)
        sprintf(outstr, " %s %s %s%s << 9", condnames[cond], opstr, istr[sflag], numstr(srcaddr));
    else if (format == OP_BIGJMP)
    {
        if (instruct & 0x00100000)
        {
            srcaddr = ((srcaddr << 12) >> 12) + 4;
            if (!hubmode) srcaddr >>= 2;
            if (InvalidAddress(hubmode, pc, srcaddr))
                *perrflag = OPCODE_BAD_ADDR;
            if (srcaddr >= 0)
                sprintf(outstr, " %s %s %s$+%s%s", condnames[cond], opstr, istr[sflag], numstr(srcaddr), wczstr);
            else
                sprintf(outstr, " %s %s %s$-%s%s", condnames[cond],
                    opstr, istr[sflag], numstr(-srcaddr), wczstr);
        }
        else
            sprintf(outstr, " %s %s %s\\%s%s", condnames[cond], opstr, istr[sflag], numstr(srcaddr), wczstr);
    }
    else if (format == OP_TWOREL9)
    {
        if (instruct & 0x40000)
        {
            srcaddr = ((srcaddr << 23) >> 23) + 1;
            if (hubmode) srcaddr <<= 2;
            if (InvalidAddress(hubmode, pc, srcaddr))
                *perrflag = OPCODE_BAD_ADDR;
            if (srcaddr > 0)
                sprintf(outstr, " %s %s %s%s, %s$+%s%s", condnames[cond],
                    opstr, istr[dflag], numstr(dstaddr), istr[sflag], numstr(srcaddr), wczstr);
            else if (srcaddr == 0)
                sprintf(outstr, " %s %s %s%s, %s$%s", condnames[cond],
                    opstr, istr[dflag], numstr(dstaddr), istr[sflag], wczstr);
            else
                sprintf(outstr, " %s %s %s%s, %s$-%s%s", condnames[cond],
                    opstr, istr[dflag], numstr(dstaddr), istr[sflag], numstr(-srcaddr), wczstr);
        }
        else
            sprintf(outstr, " %s %s %s%s, %s%s%s", condnames[cond],
                opstr, istr[dflag], numstr(dstaddr), istr[sflag], numstr(srcaddr), wczstr);
    }
    else if (format == OP_ONEREL9)
    {
        if (instruct & 0x40000)
        {
            srcaddr = ((srcaddr << 23) >> 23) + 1;
            if (hubmode) srcaddr <<= 2;
            if (InvalidAddress(hubmode, pc, srcaddr))
                *perrflag = OPCODE_BAD_ADDR;
            if (srcaddr > 0)
                sprintf(outstr, " %s %s %s$+%s%s", condnames[cond], opstr, istr[sflag], numstr(srcaddr), wczstr);
            else if (srcaddr == 0)
                sprintf(outstr, " %s %s %s$%s", condnames[cond], opstr, istr[sflag], wczstr);
            else
                sprintf(outstr, " %s %s %s$-%s%s", condnames[cond], opstr, istr[sflag], numstr(-srcaddr), wczstr);
        }
        else
            sprintf(outstr, " %s %s %s%s%s", condnames[cond], opstr, istr[sflag], numstr(srcaddr), wczstr);
    }
    else if (format == OP_NIBPARMS)
        sprintf(outstr, " %s %s %s%s, %s%s%s, #%d", condnames[cond],
            opstr, istr[dflag], numstr(dstaddr), istr[sflag], numstr(srcaddr), wczstr, (instruct >> 19) & 7);
    else if (format == OP_BYTEPARMS)
        sprintf(outstr, " %s %s %s%s, %s%s%s, #%d", condnames[cond],
            opstr, istr[dflag], numstr(dstaddr), istr[sflag], numstr(srcaddr), wczstr, (instruct >> 19) & 3);
    else if (format == OP_WORDPARMS)
        sprintf(outstr, " %s %s %s%s, %s%s%s, #%d", condnames[cond],
            opstr, istr[dflag], numstr(dstaddr), istr[sflag], numstr(srcaddr), wczstr, (instruct >> 19) & 1);
    else if (format == OP_TWOPARMS)
        sprintf(outstr, " %s %s %s%s, %s%s%s", condnames[cond],
            opstr, istr[dflag], numstr(dstaddr), istr[sflag], numstr(srcaddr), wczstr);
    else if (format == OP_TWOPTR1)
    {
        if ((instruct & 0x40100) == 0x40100)
        {
            sprintf(outstr, " %s %s %s%s, %s%s", condnames[cond],
                opstr, istr[dflag], numstr(dstaddr), ptrstr(srcaddr, perrflag), wczstr);
        }
        else
            sprintf(outstr, " %s %s %s%s, %s%s%s", condnames[cond],
                opstr, istr[dflag], numstr(dstaddr), istr[sflag], numstr(srcaddr), wczstr);
    }
    else if (format == OP_TESTB)
        sprintf(outstr, " %s %s %s%s, %s%s%s", condnames[cond], opstr, istr[dflag], numstr(dstaddr),
            istr[sflag], numstr(srcaddr), testnames[((instruct >> 20) & 0xc) + (czi >> 1)]);
    else if (format == OP_TESTP)
        sprintf(outstr, " %s %s %s%s%s", condnames[cond], opstr, istr[dflag], numstr(dstaddr),
            testnames[((instruct << 1) & 0xc) + (czi >> 1)]);
    else if (format == OP_ONEPARM_S)
        sprintf(outstr, " %s %s %s%s%s", condnames[cond], opstr, istr[sflag], numstr(srcaddr), wczstr);
    else if (format == OP_ONEPARM_D)
        sprintf(outstr, " %s %s %s%s%s", condnames[cond], opstr, istr[dflag], numstr(dstaddr), wczstr);
    else if (format == OP_NOPARMS)
        sprintf(outstr, " %s %s %s", condnames[cond], opstr, wczstr);
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
