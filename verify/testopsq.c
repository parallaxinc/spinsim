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
#define SETQ_INSTR 0xfd601028
#define SETPIV_INS 0xfd60103d
#define SETPIX_INS 0xfd60103e
#define SCA_INSTR  0xfa20100a
#define SCAS_INSTR 0xfa30100a

void testit(int *);

char *opcodeName[] = {
    "qmul", "qdiv", "qfrac", "qsqrt", "qrotate", "qvector", "qlog", "qexp",
    "muxq", "blnpix", "mixpix", "sca", "scas"
    };

int instruct[] = {
    0x0d000000, 0x0d100000, 0x0d200000, 0x0d300000, 0x0d400000, 0x0d500000, 0x0d60000e, 0x0d60000f,
    0x09f00000, 0x0a500000, 0x0a580000, 0x06040000, 0x06040000
    };

int snum[] = { NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES, 1, 1,
               NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES};
int dnum[] = { NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES, 38, 38,
               NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES};
int qnum[] = { 1, NUM_VALUES, NUM_VALUES, NUM_VALUES, 1, NUM_VALUES, 1, 1,
               NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES, NUM_VALUES};
int inst[] = { SETQ_INSTR, SETQ_INSTR, SETQ_INSTR, SETQ_INSTR, SETQ_INSTR, SETQ_INSTR, SETQ_INSTR, SETQ_INSTR,
               SETQ_INSTR, SETPIV_INS, SETPIX_INS, SCA_INSTR, SCAS_INSTR};

int test_values[38] = { 0, 1, 2, 0x7fffffff, 0x80000000, 0x80000001, 0xfffffffe, 0xffffffff,
    4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
    0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x800000, 0x1000000,
    0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000};

int mailbox[6];

void alu(int opcode, int S, int D, int C, int Z, int opcode1, int Q, int *alu_r, int *alu_c, int *alu_z, int *alu_x, int *alu_y)
{
    mailbox[0] = opcode;
    mailbox[1] = D;
    mailbox[2] = S;
    mailbox[3] = (Z << 1) | C;
    mailbox[4] = opcode1;
    mailbox[5] = Q;
    testit(mailbox);
    *alu_r = mailbox[1];
    *alu_c = mailbox[3] & 1;
    *alu_z = (mailbox[3] >> 1) & 1;
    *alu_x = mailbox[4];
    *alu_y = mailbox[5];
}

void writeTest(int index)
{
    int instr, instr1;
    int testnum = 0;
    int s1, d1, q1, S, D, C, Z, Q;
    int alu_r, alu_c, alu_z, alu_x, alu_y;
    char name[20];

    strcpy(name, "       ");
    memcpy(name, opcodeName[index], strlen(opcodeName[index]));
#if 0
    printf("%s", name);
#else
    printf("instr  ");
#endif
#ifdef PRINT_INPUT_VALUES
    printf(" ---D---- ---S---- CZ ---Q---- = ");
#endif
    printf("---R---- CZ ---X---- ---Y----\n");
    if (snum[index] <= 2)
        instr = instruct[index] |  0xf0001200;
    else
        instr = instruct[index] |  0xf000120a;
    instr1 = inst[index];
    for (q1 = 0; q1 < qnum[index]; q1++)
    {
        Q = test_values[q1];
        for (s1 = 0; s1 < snum[index]; s1++)
        {
            S = test_values[s1];
            for (d1 = 0; d1 < dnum[index]; d1++)
            {
                D = test_values[d1];
                for (C = 0; C < 1; C++)
                {
                    for (Z = 0; Z < 1; Z++)
                    {
                        alu(instr, S, D, C, Z, instr1, Q, &alu_r, &alu_c, &alu_z, &alu_x, &alu_y);
#if 0
                        printf("%02x %03x", index, testnum++);
#else
                        printf(name);
#endif
#ifdef PRINT_INPUT_VALUES
                        printf(" %08x %08x %1x%1x", D, S, C, Z);
                        printf(" %08x =", Q);
#endif
                        printf(" %08x %x%x", alu_r, alu_c, alu_z);
                        printf(" %08x %08x\n", alu_x, alu_y);
                    }
                }
            }
        }
    }
}

int main(void)
{
    int j;
    sleep(1);
    for (j = 0; j < 13; j++)
    {
        if (j == 1 || j == 2 || j == 4 || j == 5) continue;
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
    __asm__("              add     r0, #4");
    __asm__("              rdlong  r1, r0");
    __asm__("              wrlong  r1, ##instruct0");
    __asm__("              add     r0, #4");
    __asm__("              rdlong  r1, r0");
    __asm__("              jmp     #instruct0");
    __asm__("instruct0     setq    r1");
    __asm__("instruct      add     r2, r3 wcz");
    __asm__("              'add     r0, #4");
    __asm__("              sub     r0, #4");
    __asm__("              getqx   r1");
    __asm__("              wrlong  r1, r0");
    __asm__("              add     r0, #4");
    __asm__("              getqy   r1");
    __asm__("              wrlong  r1, r0");
    __asm__("              sub     r0, #8");
    __asm__(" if_nz_and_nc mov     r1, #0");
    __asm__(" if_nz_and_c  mov     r1, #1");
    __asm__(" if_z_and_nc  mov     r1, #2");
    __asm__(" if_z_and_c   mov     r1, #3");
    __asm__("              wrlong  r1, r0");
    __asm__("              sub     r0, #8");
    __asm__("              wrlong  r2, r0");
}

