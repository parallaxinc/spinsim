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
    "testb0", "testb1", "testb2", "testb3", "testbn0", "testbn1", "testbn2", "testbn3",
    "setnib0", "setnib1", "setnib2", "setnib3", "setnib4", "setnib5", "setnib6", "setnib7",
    "getnib0", "getnib1", "getnib2", "getnib3", "getnib4", "getnib5", "getnib6", "getnib7",
    "rolnib0", "rolnib1", "rolnib2", "rolnib3", "rolnib4", "rolnib5", "rolnib6", "rolnib7",
    "setbyte0", "setbyte1", "setbyte2", "setbyte3", "getbyte0", "getbyte1", "getbyte2", "getbyte3",
    "rolbyte0", "rolbyte1", "rolbyte2", "rolbyte3", "setword0", "setword1", "getword0", "getword1",
    "rolword0", "rolword1", "setr", "setd", "sets", "decod", "bmask", "zerox",
    "signx", "muxnits", "muxnibs", "movbyts", "mul", "muls", "addpix", "mulpix"};

int instruct[] = {
    0x04100000, 0x04500000, 0x04900000, 0x04d00000, 0x04300000, 0x04700000, 0x04b00000, 0x04f00000,
    0x08000000, 0x08080000, 0x08100000, 0x08180000, 0x08200000, 0x08280000, 0x08300000, 0x08380000,
    0x08400000, 0x08480000, 0x08500000, 0x08580000, 0x08600000, 0x08680000, 0x08700000, 0x08780000,
    0x08800000, 0x08880000, 0x08900000, 0x08980000, 0x08a00000, 0x08a80000, 0x08b00000, 0x08b80000,
    0x08c00000, 0x08c80000, 0x08d00000, 0x08d80000, 0x08e00000, 0x08e80000, 0x08f00000, 0x08f80000,
    0x09000000, 0x09080000, 0x09100000, 0x09180000, 0x09200000, 0x09280000, 0x09300000, 0x09380000,
    0x09400000, 0x09480000, 0x09a80000, 0x09b00000, 0x09b80000, 0x09c00000, 0x09c80000, 0x07400000,
    0x07600000, 0x09e00000, 0x09e80000, 0x09f80000, 0x0a080000, 0x0a180000, 0x0a400000, 0x0a480000};

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

    strcpy(name, "        ");
    memcpy(name, opcodeName[index], strlen(opcodeName[index]));
    printf("%s", name);
#ifdef PRINT_INPUT_VALUES
    printf(" ---D---- ---S---- CZ = ");
#endif
    printf("---Q---- CZ\n");
    instr = instruct[index] |  0xf000120a;
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
                    printf("%02x  %03x", index, testnum++);
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
    for (j = 0; j < 64; j++)
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

