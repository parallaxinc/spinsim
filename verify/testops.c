/*
 * Testbench for the alu
 * (c) Pacito.Sys
 */
#include <stdio.h>
#include <string.h>

#define PRINT_INPUT_VALUES

#define NUM_VALUES 8
#define MAX_POS    0x7fffffff
#define MAX_NEG    0x80000000
#define MAX_NEG1   0x80000001
#define WZ_BIT     0x00080000
#define WC_BIT     0x00100000

void testit(int *);

char *opcodeName[] = {
    "ror", "rol", "shr", "shl", "rcr", "rcl", "sar", "sal",
    "add", "addx", "adds", "addsx", "sub", "subx", "subs", "subsx",
    "cmp", "cmpx", "cmps", "cmpsx", "cmpr", "cmpm", "subr", "cmpsub",
    "fge", "fle", "fges", "fles", "sumc", "sumnc", "sumz", "sumnz",
    "bitl", "bith", "bitc", "bitnc", "bitz", "bitnz", "bitnot",
    "andn", "and", "or", "xor", "muxc", "muxnc", "muxz", "muxnz",
    "mov", "not", "abs", "neg", "negc", "negnc", "negz", "negnz",
    "incmod", "decmod", "encod", "testn", "test", "anyb", "setnib", "getnib",
    "rolnib", "setbyte", "getbyte", "rolbyte", "getword", "sets", "signx", "movbyts",
    "muls"};

int instruct[] = {
    0x00000000, 0x00200000, 0x00400000, 0x00600000, 0x00800000, 0x00a00000, 0x00c00000, 0x00e00000,
    0x01000000, 0x01200000, 0x01400000, 0x01600000, 0x01800000, 0x01a00000, 0x01c00000, 0x01e00000,
    0x02000000, 0x02200000, 0x02400000, 0x02600000, 0x02800000, 0x02a00000, 0x02c00000, 0x02e00000,
    0x03000000, 0x03200000, 0x03400000, 0x03600000, 0x03800000, 0x03a00000, 0x03c00000, 0x03e00000,
    0x04000000, 0x04200000, 0x04400000, 0x04600000, 0x04800000, 0x04a00000, 0x04e00000,
    0x05000000, 0x05200000, 0x05400000, 0x05600000, 0x05800000, 0x05a00000, 0x05c00000, 0x05e00000,
    0x06000000, 0x06200000, 0x06400000, 0x06600000, 0x06800000, 0x06a00000, 0x06c00000, 0x06e00000,
    0x07000000, 0x07200000, 0x07400000, 0x07800000, 0x07a00000, 0x07c00000, 0x08000000, 0x08400000,
    0x08800000, 0x08c00000, 0x08e00000, 0x09000000, 0x09300000, 0x09b80000, 0x09d80000, 0x09f80000,
    0x0a100000};

int test_values[] = { 0, 1, 2, 0x7fffffff, 0x80000000, 0x80000001, 0xfffffffe, 0xffffffff };

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
    instr = instruct[index] |  0xf018120a;
    for (s1 = 0; s1 < NUM_VALUES; s1++)
    {
        S = test_values[s1];
        for (d1 = 0; d1 < NUM_VALUES; d1++)
        {
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
    for (j = 0; j < 72; j++)
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
