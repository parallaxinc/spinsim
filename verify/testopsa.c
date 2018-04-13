/*
 * Testbench for the alu
 * (c) Pacito.Sys
 */
#include <stdio.h>
#include <string.h>

#define PRINT_INPUT_VALUES

#define MAX_POS    0x7fffffff
#define MAX_NEG    0x80000000
#define MAX_NEG1   0x80000001
#define WZ_BIT     0x00080000
#define WC_BIT     0x00100000

void testit(int *);

char *opcodeName[] = {
    "altsn", "altgn/g", "altgn/r", "altsb", "altgb/g", "altgb/r", "altsw", "altgw/g",
    "altgw/r", "altr", "altd", "alts", "altb", "alti"};

int instruct[] = {
    0xf804015a, 0xf8401400, 0xf8801400, 0xf8c4015a, 0xf8e01400, 0xf9001400, 0xf924015a, 0xf9301400,
    0xf9401400, 0xf1001208, 0xf1001208, 0xf1001208, 0xf1001208, 0xf1001009};

int inst[] = {
    0xf9501009, 0xf9581009, 0xf9581009, 0xf9601009, 0xf9681009, 0xf9681009, 0xf9701009, 0xf9781009,
    0xf9781009, 0xf9801009, 0xf9881009, 0xf9901009, 0xf9981009, 0xf9a01208};

int vshift[] = {3, 3, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0, 5, 0};

int mailbox[6];

void alu(int opcode, int S, int D, int C, int Z, int opcode1, int Q, int *alu_r, int *alu_c, int *alu_z, int *alu_x, int *alu_y)
{
    mailbox[0] = opcode;
    mailbox[1] = opcode1;
    mailbox[2] = Q;
    mailbox[3] = D;
    mailbox[4] = S;
    testit(mailbox);
    *alu_r = mailbox[2];
    *alu_x = mailbox[3];
    *alu_y = mailbox[4];
    *alu_c = 0;
    *alu_z = 0;
}

void writeTest(int index)
{
    int instr, instr1;
    int testnum = 0;
    int r1, r2, r3, incr, roff, val;
    int alu_r, alu_c, alu_z, alu_x, alu_y;
    char name[20];
    int vnum = 1 << vshift[index];
    int vshi = vshift[index];

    if (vshi == 5) vnum = 1;

    strcpy(name, "       ");
    memcpy(name, opcodeName[index], strlen(opcodeName[index]));
    printf("instr  ");
#ifdef PRINT_INPUT_VALUES
    printf(" ---r3--- ---r2--- CZ ---r1--- = ");
#endif
    printf("---r1--- CZ ---r2--- ---r3---\n");
    instr = instruct[index];
    instr1 = inst[index];
    for (r3 = 0x12345678; r3 <= 0x12345678; r3++)
    {
        for (r2 = 0x78900003; r2 <= 0x78900003; r2++)
        {
            for (incr = -2; incr <= 2; incr++)
            {
                for (roff = 5; roff <= 7; roff++)
                {
                    for (val = 0; val < vnum; val++)
                    {
                        r1 = (roff << vshi) | val;
                        r2 |= (incr & 511) << 9;
                        alu(instr, r3, r2, 0, 0, instr1, r1, &alu_r, &alu_c, &alu_z, &alu_x, &alu_y);
                        printf(name);
#ifdef PRINT_INPUT_VALUES
                        printf(" %08x %08x %1x%1x", r3, r2, 0, 0);
                        printf(" %08x =", r1);
#endif
                        printf(" %08x %x%x", alu_r, alu_c, alu_z);
                        printf(" %08x %08x\n", alu_x, alu_y);
                    }
                }
            }
        }
    }
}

void writeTestAlti(int index)
{
    int instr, instr1;
    int testnum = 0;
    int r1, r2, r3, incr, roff, val;
    int alu_r, alu_c, alu_z, alu_x, alu_y;
    char name[20];

    strcpy(name, "       ");
    memcpy(name, opcodeName[index], strlen(opcodeName[index]));
    printf("instr  ");
#ifdef PRINT_INPUT_VALUES
    printf(" ---r3--- ---r2--- CZ ---r1--- = ");
#endif
    printf("---r1--- CZ ---r2--- ---r3---\n");
    instr = instruct[index];
    instr1 = inst[index];
    r3 = 0x12345678;
    for (val = 0; val < 512; val++)
    {
        if ((val >> 6) == 5)
            r2 = 0xf1801208;
        else
            r2 = (10 << 19) | (9 << 9) | 8;
        r1 = 0x2ee00 | val;
        alu(instr, r3, r2, 0, 0, instr1, r1, &alu_r, &alu_c, &alu_z, &alu_x, &alu_y);
        printf(name);
#ifdef PRINT_INPUT_VALUES
        printf(" %08x %08x %1x%1x", r3, r2, 0, 0);
        printf(" %08x =", r1);
#endif
        printf(" %08x %x%x", alu_r, alu_c, alu_z);
        printf(" %08x %08x\n", alu_x, alu_y);
    }
}

int main(void)
{
    int j;
    sleep(1);
    for (j = 0; j < 13; j++)
    {
        writeTest(j);
    }
    writeTestAlti(13);
    return 0;
}

void testit(int *list)
{
    __asm__("              rdlong  r1, r0");
    __asm__("              wrlong  r1, ##instruct");
    __asm__("              add     r0, #4");
    __asm__("              rdlong  r1, r0");
    __asm__("              wrlong  r1, ##instruct0");
    __asm__("              add     r0, #4");
    __asm__("              rdlong  r1, r0");
    __asm__("              add     r0, #4");
    __asm__("              rdlong  r2, r0");
    __asm__("              add     r0, #4");
    __asm__("              rdlong  r3, r0");
    __asm__("              jmp     #instruct0");
    __asm__("instruct0     setq    r1");
    __asm__("instruct      add     r2, r3 wcz");
    __asm__("              wrlong  r3, r0");
    __asm__("              sub     r0, #4");
    __asm__("              wrlong  r2, r0");
    __asm__("              sub     r0, #4");
    __asm__("              wrlong  r1, r0");
}

