/*
 * Testbench for the alu
 * (c) Pacito.Sys
 */
#include <stdio.h>
#include <string.h>

#define PRINT_INPUT_VALUES

#define NUM_VALUES 38
#define MAX_POS    0x7fffffff
#define MAX_NEG    0x80000000
#define MAX_NEG1   0x80000001
#define WZ_BIT     0x00080000
#define WC_BIT     0x00100000

void testit(int *);

char *opcodeName[] = {
    "splitb", "mergeb", "splitw", "mergew", "seussf", "seussr", "rgbsqz", "rgbexp",
    "rev", "rczr", "rczl", "wrc", "wrnc", "wrz", "wrnz", "modcz"};

int instruct[] = {
    0x0d600060, 0x0d600061, 0x0d600062, 0x0d600063, 0x0d600064, 0x0d600065, 0x0d600066, 0x0d600067,
    0x0d600069, 0x0d78006a, 0x0d78006b, 0x0d60006c, 0x0d60006d, 0x0d60006e, 0x0d60006f, 0x0d7c006f};

int test_values[38] = { 0, 1, 2, 0x7fffffff, 0x80000000, 0x80000001, 0xfffffffe, 0xffffffff,
    4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
    0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x800000, 0x1000000,
    0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000};

int mailbox[4];

void alu(int opcode, int S, int D, int C, int Z, int *alu_q, int *alu_c, int *alu_z)
{
    mailbox[0] = opcode;
    mailbox[1] = D;
    mailbox[2] = S;
    mailbox[3] = (Z << 1) | C;
    testit(mailbox);
    *alu_q = mailbox[1];
    *alu_c = mailbox[3] & 1;
    *alu_z = (mailbox[3] >> 1) & 1;
}

void writeTest(int index)
{
    int instr;
    int testnum = 0;
    int s1, d1, S, D, C, Z;
    int alu_q, alu_c, alu_z;
    char name[20];

    strcpy(name, "       ");
    memcpy(name, opcodeName[index], strlen(opcodeName[index]));
    printf("%s", name);
#ifdef PRINT_INPUT_VALUES
    printf(" ---D---- ---S---- CZ = ");
#endif
    printf("---Q---- CZ\n");
    instr = instruct[index] |  0xf0001200;
    for (s1 = 0; s1 < 1; s1++)
    {
        S = test_values[s1];
        for (d1 = 0; d1 < NUM_VALUES; d1++)
        {
            if (index == 15)
            {
                D = d1;
                instr = (instr & ~0x3fe00) | ((D & 255) << 9);
            }
            else
                D = test_values[d1];
            for (C = 0; C <= 1; C++)
            {
                for (Z = 0; Z <= 1; Z++)
                {
                    alu(instr, S, D, C, Z, &alu_q, &alu_c, &alu_z);
                    printf("%02x %03x", index, testnum++);
#ifdef PRINT_INPUT_VALUES
                    printf("  %08x %08x %1x%1x =", D, S, C, Z);
#endif
                    printf(" %08x %x%x\n", alu_q, alu_c, alu_z);
                }
            }
        }
    }
}

int main(void)
{
    int j;
    sleep(1);
    for (j = 0; j < 16; j++)
    {
        writeTest(j);
    }
    return 0;
}

void testit(int *list)
{
    __asm__("              rdlong  r1, r0");
    __asm__("              wrlong  r1, ##instruct");
    __asm__("              add     r0, #4");
    __asm__("              rdlong  r2, r0");
    __asm__("              add     r0, #4");
    __asm__("              rdlong  r3, r0");
    __asm__("              add     r0, #4");
    __asm__("              rdlong  r1, r0");
    __asm__("              shr     r1, #1 wc");
    __asm__("              and     r1, #1");
    __asm__("              xor     r1, #1 wz");
    __asm__("              jmp     #instruct");
    __asm__("instruct      mov     r2, r3 wcz");
    __asm__(" if_nz_and_nc mov     r1, #0");
    __asm__(" if_nz_and_c  mov     r1, #1");
    __asm__(" if_z_and_nc  mov     r1, #2");
    __asm__(" if_z_and_c   mov     r1, #3");
    __asm__("              wrlong  r1, r0");
    __asm__("              sub     r0, #8");
    __asm__("              wrlong  r2, r0");
}

