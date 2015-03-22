{
############################################################################
# PFTH - This program implements a Forth interpreter.
#
# Copyright (c) 2012, 2013 Dave Hein
# MIT Licensed
############################################################################
}
con
  _clkmode = xtal1+pll16x
  _clkfreq = 80_000_000

  ' SD Pin Definitions
  'DO  = 10
  'CLK = 11
  'DI  = 9
  'CS  = 25

  'Q = 16    ' Object Offset

  FLAG_IMMEDIATE = 1
  FLAG_CORE = $10 | $80
  FLAG_LIT  = $12 | $80
  FLAG_VAR  = $20 | $80
  FLAG_DEF  = $00 | $80
  FLAG_JMP  = $0A | $80
  FLAG_SEMI = FLAG_CORE | FLAG_IMMEDIATE
{
obj
  'spi : "mount"

'*******************************************************************************
' This Spin code waits three seconds and then starts the Forth cog
'*******************************************************************************
pub main(argc, argv)
  waitcnt(clkfreq+cnt)
  'spi.mount_explicit(@spi_vars, DO, CLK, DI, CS)
  coginit(cogid, @forth, @pfthconfig)
  
dat
pfthconfig              long      @xboot_1+Q            ' Initial word to execute
                        long      @stack+Q+16           ' Starting stack pointer
                        long      @stack+Q+16           ' Empty stack pointer value
                        long      @retstk+Q             ' Starting return pointer
                        long      @retstk+Q             ' Empty return pointer value
stack                   long      0[100]                ' Data stack
retstk                  long      0[100]                ' Return stack
                        
'*******************************************************************************
' pfth cog code
'*******************************************************************************
                        org     0                
forth
parm                    mov     parm, par
}
  Q             = 0             ' Object offset for the top object
  'rx_pin        = 91
  'tx_pin        = 90
  rx_pin        = 31
  tx_pin        = 30

DAT
                        orgh    $380
                        org     0

                        ' Entry point for the Forth interpreter
forth                   'mov    pc, a_interp
                        'or      outc, tx_mask
                        'or      dirc, tx_mask
                        'getcnt  temp2
                        'add     temp2, delay_time
                        'waitcnt temp2, #0
                        jmp     #parm
parval                  long    @pfthconfig
'tx_mask                 long    1 << (64 - TX_PIN)      ' must be in dirc
delay_time              long    80000000 * 2

parm                    mov     parm, parval
parm1                   rdlong  pc, parm
parm2                   add     parm, #4
parm3                   rdlong  stackptr, parm
parm4                   add     parm, #4
temp                    rdlong  stackptr0, parm
temp1                   add     parm, #4
temp2                   rdlong  returnptr, parm
temp3                   add     parm, #4
temp4                   rdlong  returnptr0, parm
                        jmp     #innerloop              ' Begin execution

'*******************************************************************************
' Execute the words contained in the body of a word
' Changes parm, temp1
'*******************************************************************************
execlistfunc            add     parm, #4                ' Get body from XT
innerloopcall           wrlong  pc, returnptr           ' Push PC to return stack
                        add     returnptr, #4
                        mov     pc, parm                ' Set new value for PC
                        
'*******************************************************************************
' Get an execution token from the location pointed to by the program counter
' Increment the program counter, fetch the code pointer and jump to it
' Changes parm, temp1, pc
'*******************************************************************************
innerloop               rdword  parm, pc                wz
 if_z                   jmp     #exitfunc
                        add     pc, #2
                        rdword  temp1, parm
                        jmp     temp1
                        
pc                      long    @xboot_1+Q             ' Program Counter

'*******************************************************************************
' Stop executing the current word, and return to the calling word
' No Changes              
'*******************************************************************************
exitfunc                sub     returnptr, #4
                        rdlong  pc, returnptr
                        jmp     #innerloop

'*******************************************************************************
' Abort or quit execution, and return to the interpreter
' No Changes
'*******************************************************************************
abortfunc               mov     stackptr, stackptr0
quitfunc                mov     returnptr, returnptr0
                        add     returnptr, #4           ' Use second entry return stack
                        rdlong  pc, returnptr      
                        jmp     #innerloop

'*******************************************************************************
' Push the value contained in the word's body onto the stack
' No changes
'*******************************************************************************
confunc                 add     parm, #4
                        rdlong  parm1, parm
                        jmp     #push_jmp
                        
'*******************************************************************************
' Push the address of the word's body onto the stack
' Execute the words pointed to by the does pointer, if non-zero
' No changes
'*******************************************************************************
varfunc                 mov     parm1, parm
                        add     parm1, #4
                        call    #push1
                        
'*******************************************************************************
' Execute the words pointed to by the does pointer, if non-zero
' No changes
'*******************************************************************************
deferfunc               add     parm, #2                ' DOES> pointer
                        rdword  parm, parm              wz
        if_z            jmp     #innerloop              ' Done with varfunc
                        jmp     #innerloopcall          ' Execute DOES> code

'*******************************************************************************
' Execute the word on the stack
' Changes parm, temp1
'*******************************************************************************
executefunc             sub     stackptr, #4
                        rdlong  parm, stackptr
                        rdlong  temp1, parm
                        jmp     temp1                   ' Execute code

'*******************************************************************************
' Execute the PASM instruction on the TOS using the next value on the stack as
' the destination register data.  Return the result on the stack.
' Changes parm1, parm2
'*******************************************************************************
cogx1func               call    #pop2
                        mov     cogx1instr, parm2
                        'movd    cogx1instr, #parm1
                        nop
cogx1instr              nop
                        jmp     #push_jmp

'*******************************************************************************
' Duplicate the top of stack
' Changes parm1
'*******************************************************************************
dupfunc                 call    #pop1
                        call    #push1
                        jmp     #push_jmp

'*******************************************************************************
' Swap the top two items on the stack
' Changes parm1, parm2
'*******************************************************************************
swapfunc                call    #pop2
                        wrlong  parm2, stackptr
                        add     stackptr, #4
                        jmp     #push_jmp
                                                
'*******************************************************************************
' Get the next word from the input buffer using the delimiter from the stack
' Changes parm, parm1, parm2, temp1, temp2
'*******************************************************************************
wordfunc                sub     stackptr, #4
                        rdlong  parm, stackptr
                        call    #word_del
                        mov     temp1, #1
                        shl     temp1, #15
                        sub     temp1, parm2
                        sub     temp1, #1
                        wrlong  temp1, stackptr
                        add     stackptr, #4
                        wrbyte  parm2, temp1
                        cmps    parm2, #0               wc, wz
        if_c_or_z       jmp     #innerloop
:loop                   add     temp1, #1
                        rdbyte  temp2, parm1
                        add     parm1, #1
                        wrbyte  temp2, temp1
                        djnz    parm2, @:loop
                        jmp     #innerloop

'*******************************************************************************
' Find the word specfied on the stack in the dictionary
' Changes parm1, parm2, temp4
'*******************************************************************************
findfunc                call    #pop1
                        mov     temp4, parm1
                        add     parm1, #1
                        rdbyte  parm2, temp4
                        call    #findword
                        mov     parm1, parm             wz
        if_z            jmp     #findfunc1
                        call    #link2xt
                        call    #push1
                        add     parm, #2                ' Point to flag byte
                        rdbyte  parm1, parm
                        and     parm1, #1               ' Check immediate bit
                        shl     parm1, #1
                        sub     parm1, #1               ' Return 1 if set, -1 if not
                        call    #push1
                        jmp     #innerloop
findfunc1               mov     parm1, temp4
                        call    #push1
                        mov     parm1, #0
                        call    #push1
                        jmp     #innerloop

'*******************************************************************************
' Send the character from the stack to the output port
' Changes parm
'*******************************************************************************
emitfunc                call    #pop1
                        mov     parm, parm1
                        call    #putch
                        jmp     #innerloop

'*******************************************************************************
' Get a character from the input port and put it on the stack
' Changes parm, parm1
'*******************************************************************************
getcharfunc             call    #getch
                        mov     parm1, parm
                        jmp     #push_jmp

'*******************************************************************************
' Get a character from the files stored in memory and put it on the stack
' Changes parm1
'*******************************************************************************
getfcharfunc            rdbyte  parm1, infileptr
                        add     infileptr, #1
                        jmp     #push_jmp

'*******************************************************************************
' Get an address and value from the stack, and store the value at the address
' No changes              
'*******************************************************************************
storefunc               call    #pop2
                        wrlong  parm1, parm2
                        jmp     #innerloop

'*******************************************************************************
' Fetch a value from the address specified on the stack, and put it on the stack
' Changes parm1              
'*******************************************************************************
fetchfunc               call    #pop1
                        rdlong  parm1, parm1
                        jmp     #push_jmp

'*******************************************************************************
' Get an address and word from the stack, and store the word at the address
' No changes              
'*******************************************************************************
wstorefunc              call    #pop2
                        wrword  parm1, parm2
                        jmp     #innerloop

'*******************************************************************************
' Fetch a word from the address specified on the stack, and put it on the stack
' Changes parm1              
'*******************************************************************************
wfetchfunc              call    #pop1
                        rdword  parm1, parm1
                        jmp     #push_jmp
              
'*******************************************************************************
' Get an address and byte from the stack, and store the byte at the address
' No changes              
'*******************************************************************************
cstorefunc              call    #pop2
                        wrbyte  parm1, parm2
                        jmp     #innerloop

'*******************************************************************************
' Fetch a byte from the address specified on the stack, and put it on the stack
' Changes parm1              
'*******************************************************************************
cfetchfunc              call    #pop1
                        rdbyte  parm1, parm1
                        jmp     #push_jmp
              
'*******************************************************************************
' Add two values from the stack, and write the result back to the stack
' Changes parm1, parm2              
'*******************************************************************************
plusfunc                call    #pop2
                        add     parm1, parm2
                        jmp     #push_jmp
              
'*******************************************************************************
' Subtract two values from the stack, and write the result back to the stack
' Changes parm1, parm2              
'*******************************************************************************
minusfunc               call    #pop2
                        sub     parm1, parm2
                        jmp     #push_jmp
              
'*******************************************************************************
' Multiply two values from the stack, and write the result back to the stack
' Changes parm1, parm2              
'*******************************************************************************
multfunc                call    #pop2
                        call    #multiply
                        jmp     #push_jmp
              
'*******************************************************************************
' Divide two values from the stack, and write the result back to the stack
' Changes parm1, parm2              
'*******************************************************************************
dividefunc              call    #pop2
                        call    #divide
                        mov     parm1, parm2
                        test    parm3, #1               wc
        if_c            neg     parm1, parm1
                        jmp     #push_jmp
              
'*******************************************************************************
' Compute the modulus from two values from the stack, and write the result back
' to the stack
' Changes parm1, parm2              
'*******************************************************************************
modfunc                 call    #pop2
                        call    #divide
                        test    parm3, #2               wc
        if_c            neg     parm1, parm1
                        jmp     #push_jmp
              
'*******************************************************************************
' Compare two values from the stack to determine if the second one is less than
' the first one, and write the result back to the stack
' Changes parm1, parm2              
'*******************************************************************************
lessfunc                call    #pop2
                        cmps    parm1, parm2            wc
        if_c            neg     parm1, #1
        if_nc           mov     parm1, #0
                        jmp     #push_jmp
              
'*******************************************************************************
' Compare two values from the stack to determine if they are equal, and write
' the result back to the stack
' Changes parm1, parm2              
'*******************************************************************************
equalfunc               call    #pop2
                        cmp     parm1, parm2            wz
        if_z            neg     parm1, #1
        if_nz           mov     parm1, #0
                        jmp     #push_jmp
              
'*******************************************************************************
' Compare two values from the stack to determine if the second one is greater
' than the first one, and write the result back to the stack
' Changes parm1, parm2              
'*******************************************************************************
greaterfunc             call    #pop2
                        cmps    parm1, parm2            wc, wz
        if_nz_and_nc    neg     parm1, #1
        if_z_or_c       mov     parm1, #0
                        jmp     #push_jmp

'*******************************************************************************
' Compute the logical AND of two values from the stack, and write the result
' back to the stack
' Changes parm1, parm2              
'*******************************************************************************
andfunc                 call    #pop2
                        and     parm1, parm2
                        jmp     #push_jmp
              
'*******************************************************************************
' Compute the logical OR of two values from the stack, and write the result
' back to the stack
' Changes parm1, parm2              
'*******************************************************************************
orfunc                  call    #pop2
                        or      parm1, parm2
                        jmp     #push_jmp
              
'*******************************************************************************
' Compute the logical XOR of two values from the stack, and write the result
' back to the stack
' Changes parm1, parm2              
'*******************************************************************************
xorfunc                 call    #pop2
                        xor     parm1, parm2
                        jmp     #push_jmp
              
'*******************************************************************************
' Right-shift the second value on the stack by the number of bits specified by
' the first value on the stack, and write the result to the stack
' Changes parm1, parm2              
'*******************************************************************************
rshiftfunc              call    #pop2
                        shr     parm1, parm2
                        jmp     #push_jmp
              
'*******************************************************************************
' Left-shift the second value on the stack by the number of bits specified by
' the first value on the stack, and write the result to the stack
' Changes parm1, parm2              
'*******************************************************************************
lshiftfunc              call    #pop2
                        shl     parm1, parm2
                        jmp     #push_jmp

'*******************************************************************************
' Push the stack depth to the stack
' Changes parm1              
'*******************************************************************************
depthfunc               mov     parm1, stackptr
                        sub     parm1, stackptr0
                        sar     parm1, #2
                        jmp     #push_jmp

'*******************************************************************************
' Drop the top value from the stack
' No changes
'*******************************************************************************
dropfunc                sub     stackptr, #4
                        jmp     #innerloop

'*******************************************************************************
' Use the value on top of the stack as an index to another value in the stack,
' and write its value to the stack
' No changes
'*******************************************************************************
pickfunc                call    #pop1
                        call    #indexstack
                        jmp     #push_jmp

'*******************************************************************************
' Use the value on top of the stack as and index to remove another value from
' the stack, and place it at the top of the stack.
' Changes temp1, temp2, temp3, temp4
'*******************************************************************************
rollfunc                call    #pop1
                        cmp     parm1, #0               wc, wz
        if_c_or_z       jmp     #innerloop
                        mov     temp3, parm1
                        call    #indexstack
                        mov     temp2, temp1
:loop                   add     temp2, #4
                        rdlong  temp4, temp2
                        wrlong  temp4, temp1
                        add     temp1, #4
                        djnz    temp3, @:loop
                        wrlong  parm1, temp1
                        jmp     #innerloop

'*******************************************************************************
' Pop the value from the top of the stack, and push it onto the return stack.
' No changes
'*******************************************************************************
torfunc                 call    #pop1
                        wrlong  parm1, returnptr
                        add     returnptr, #4
                        jmp     #innerloop

'*******************************************************************************
' Pop the value from the top of the return stack and push it to the stack.
' Changes parm1
'*******************************************************************************
fromrfunc               sub     returnptr, #4
                        rdlong  parm1, returnptr
                        jmp     #push_jmp

'*******************************************************************************
' Push the value on the stack pointed to by the PC and increment the PC
' Changes parm1
'*******************************************************************************
_litfunc                rdword  parm1, pc
                        add     pc, #2
                        jmp     #push_jmp

'*******************************************************************************
' Convert the string described by the address and length on the top of the
' stack to a hex number, and push it to the stack
' Changes parm1
'*******************************************************************************
_gethexfunc             call    #pop2
                        call    #gethex
                        mov     parm1, parm
                        jmp     #push_jmp

'*******************************************************************************
' Create a variable, and add it to the dictionary
' Changes parm3
'*******************************************************************************
createfunc              mov     parm3, #varfunc
                        mov     parm4, #FLAG_VAR
                        call    #create
                        jmp     #innerloop

'*******************************************************************************
' Create an executable word, and add it to the dictionary.  Set the compile
' state to -1
' Changes parm3, temp1
'*******************************************************************************
colonfunc               mov     parm3, #execlistfunc
                        mov     parm4, #FLAG_DEF
                        call    #create
        if_z            jmp     #innerloop
                        neg     temp1, #1
                        wrlong  temp1, a_state
                        jmp     #innerloop

'*******************************************************************************
' Compile a zero into memory indicating the end of an executable word, and set
' the compile flag to zero
' Changes temp1, temp2
'*******************************************************************************
semicolonfunc           mov     temp1, #0
                        wrlong  temp1, a_state
                        rdlong  temp2, a_dp
                        wrword  temp1, temp2
                        add     temp2, #2
                        wrlong  temp2, a_dp
                        jmp     #innerloop

'*******************************************************************************
' Fetch a value from the specified cog address, and put it on the stack
' the compile flag to zero
' Changes parm1
'*******************************************************************************
cogfetchfunc            call    #pop1
                        'movs    cogfetch1, parm1
                        nop
cogfetch1               mov     parm1, 0-0
                        jmp     #push_jmp

'*******************************************************************************
' Get a cog address and value from the stack, and store the value at the address
' the compile flag to zero
' Changes parm1, parm2
'*******************************************************************************
cogstorefunc            call    #pop2
                        'movd    cogstore1, parm2
                        nop
cogstore1               'mov     0-0, parm1
                        jmp     #innerloop


'*******************************************************************************
' Print out an 8-digit hex number to the output port.
' Changes parm
'*******************************************************************************
dotxfunc                mov     parm, #"$"
                        call    #putch
                        call    #pop1
                        call    #printhex
                        mov     parm, #" "
                        call    #putch
                        jmp     #innerloop

'*******************************************************************************
' If top of stack is zero, jump to address contained in location at current PC.
' Otherwise, increment the PC
' Changes parm1
'*******************************************************************************
_jzfunc                 call    #pop1
        if_z            rdword  pc, pc
        if_nz           add     pc, #2
                        jmp     #innerloop

'*******************************************************************************
' Copy bytes from the source to the destination
' Changes parm1
'*******************************************************************************
cmovefunc               sub     stackptr, #4
                        rdlong  parm3, stackptr
                        call    #pop2
                        cmps    parm3, #0               wz, wc
        if_c_or_z       jmp     #innerloop
:loop                   rdbyte  temp1, parm1
                        add     parm1, #1
                        wrbyte  temp1, parm2
                        add     parm2, #1
                        djnz    parm3, @:loop
                        jmp     #innerloop

'*******************************************************************************
' Perform the increment and compare for the loop word
' Changes parm1, parm2, parm3
'*******************************************************************************
_loopfunc               call    #pop1                   ' Get increment
                        sub     returnptr, #8
                        rdlong  parm3, returnptr        ' Get upper limit
                        add     returnptr, #4
                        rdlong  parm2, returnptr        ' Get index
                        add     parm1, parm2            ' index + increment
                        wrlong  parm1, returnptr        ' Push index back
                        add     returnptr, #4
                        cmps    parm1, parm3            wc
        if_nc           neg     parm1, #1
        if_c            mov     parm1, #0
                        jmp     #push_jmp
                        
'*******************************************************************************
' The following code implements the basic functions used by the kernel words
'*******************************************************************************

'*******************************************************************************
' Create a word entry in the dictionary
' Changes parm, parm1, parm2, temp1, temp2              
'*******************************************************************************
create                  mov     parm, #" "
                        call    #word_del
        if_z            jmp     #create_ret
                        rdlong  temp1, a_dp             ' Align DP
                        add     temp1, #3
                        and     temp1, minus4
                        rdlong  temp2, a_last
                        wrword  temp2, temp1            ' Write the link pointer
                        wrlong  temp1, a_last           ' Update LAST
                        add     temp1, #2
                        
                        wrbyte  parm4, temp1            ' Write the flag
                        add     temp1, #1
                        wrbyte  parm2, temp1            ' Write the length
                        add     temp1, #1
                        cmps    parm2, #0               wc, wz
        if_c_or_z       jmp     #create_done
:loop                   rdbyte  temp2, parm1            ' Copy the name
                        add     parm1, #1
                        wrbyte  temp2, temp1
                        add     temp1, #1               wz
                        djnz    parm2, @:loop

create_done             mov     temp2, #0               ' Pad with 0's to align
:loop1                  test    temp1, #3               wz
        if_z            jmp     #create_aligned
                        wrbyte  temp2, temp1
                        add     temp1, #1
                        jmp     #:loop1

create_aligned          wrword  parm3, temp1            ' Write the code pointer
                        add     temp1, #2
                        wrword  temp2, temp1            ' Write the DOES> pointer             
                        add     temp1, #2               wz ' Clear zero flag
                                  
                        wrlong  temp1, a_dp
create_ret              ret

'*******************************************************************************
' Get one character from the input port.
' Input none
' Changes parm, temp, temp1, temp2
' Output parm
'*******************************************************************************
{
getch                   mov     parm, ina
                        and     parm, inbit             wz
        if_nz           jmp     #getch
                        mov     temp2, cnt
                        mov     temp, bitcycles
                        shr     temp, #1
                        add     temp2, temp
                        mov     temp1, #10
:loop                   waitcnt temp2, bitcycles
                        mov     temp, ina
                        and     temp, inbit
                        ror     parm, #1
                        or      parm, temp
                        djnz    temp1, #:loop
                        ror     parm, #31 - 8
                        and     parm, #255
getch_ret               ret

inbit                   long    $80000000
}
getch                   getp    #RX_PIN                 wz
        if_nz           jmp     #getch
                        getcnt  temp2
                        mov     temp3, bitcycles
                        shr     temp3, #1
                        add     temp2, temp3
                        mov     temp1, #10
                        mov     parm, #0
:loop                   waitcnt temp2, bitcycles
                        ror     parm, #1
                        getp    #RX_PIN                 wc
        if_c            or      parm, #1
                        djnz    temp1, @:loop
                        rol     parm, #8
                        and     parm, #255
                        ret
bitcycles               long    80_000_000 / 115_200

'*******************************************************************************
' Send one character to the output port.
' Input parm
' Changes parm, temp1, temp2
' Output none             
'*******************************************************************************
{
putch                   rdlong  temp1, a_verbose       wz
        if_z            jmp     putch_ret
                        or      parm, #$100
                        shl     parm, #1
                        mov     temp1, #10
                        mov     temp2, bitcycles
                        add     temp2, cnt
:loop                   shr     parm, #1               wc
        if_c            or      outa, outbit
        if_nc           andn    outa, outbit
                        waitcnt temp2, bitcycles
                        djnz    temp1, #:loop
putch_ret               ret

outbit                  long    $40000000
}
putch                   rdlong  temp1, a_verbose       wz
        if_z            ret
                        or      parm, stopbits
                        shl     parm, #1
                        mov     temp1, #11
                        getcnt  temp2
                        add     temp2, bitcycles
:loop                   ror     parm, #1               wc
                        setpc   #TX_PIN
                        waitcnt temp2, bitcycles
                        djnz    temp1, @:loop
                        ret
stopbits                long    $300

'*******************************************************************************
' Skip the specified character in the input buffer
' Input parm
' Changes temp, temp1
' Output none
'*******************************************************************************
skipchar                cmps    temp1, temp2            wc
        if_nc           jmp     #skipchar_ret
                        rdlong  temp, a_tib
                        add     temp, temp1
                        rdbyte  temp, temp
                        cmp     temp, parm              wz
        if_nz           jmp     #skipchar_ret
                        add     temp1, #1
                        jmp     #skipchar
skipchar_ret            ret

'*******************************************************************************
' Find the next occurance of the specified character in the input buffer
' Input parm
' Changes temp, temp1
' Output none
'*******************************************************************************
findchar                cmps    temp1, temp2            wc
        if_nc           jmp     #findchar_ret
                        rdlong  temp, a_tib
                        add     temp, temp1
                        rdbyte  temp, temp
                        cmp     temp, parm              wz
        if_z            jmp     #findchar_ret
                        add     temp1, #1
                        jmp     #findchar
findchar_ret            ret

'*******************************************************************************
' Find the next word in the input buffer delimited by the specified character
' Input parm
' Changes parm1, parm2, temp1, temp2
' Output none
'*******************************************************************************
word_del
                        rdlong  temp1, a_inputidx
                        rdlong  temp2, a_inputlen
                        call    #skipchar
                        mov     parm1, temp1
                        call    #findchar
                        mov     parm2, temp1
                        sub     parm2, parm1            wz
                        rdlong  temp, a_tib
                        add     parm1, temp
                        cmps    temp1, temp2            wc
        if_c            add     temp1, #1
                        wrlong  temp1, a_inputidx
word_del_ret            ret

'*******************************************************************************
' Find the specified word in the dictionary
' Input parm1, parm2
' Changes parm, parm3, parm4
' Output parm
'*******************************************************************************
findword                rdlong  parm, a_last            wz
        if_z            jmp     #findword_ret
:loop                   mov     parm3, parm
                        add     parm3, #3
                        rdbyte  parm4, parm3
                        add     parm3, #1
                        call    #compare
        if_z            jmp     #findword_ret
                        rdword  parm, parm              wz
        if_nz           jmp     #:loop
findword_ret            ret

'*******************************************************************************
' Do a case insensitive comparison of two character strings
' Input parm1, parm2, parm3, parm4
' Changes parm3, parm4, temp, temp1, temp2
' Outut Z
'*******************************************************************************
compare                 cmps    parm2, #1               wc, wz
        if_c            jmp     #compare_ret
                        cmp     parm2, parm4            wz
        if_nz           jmp     #compare_ret
                        mov     temp, parm1
:loop                   rdbyte  temp1, temp
                        call    #toupper
                        mov     temp2, temp1
                        rdbyte  temp1, parm3
                        call    #toupper
                        cmp     temp1, temp2            wz
        if_nz           jmp     #compare_ret
                        add     temp, #1
                        add     parm3, #1
                        djnz    parm4, @:loop
compare_ret             ret

'*******************************************************************************
' Convert a character to uppercase
' Input temp1
' Changes temp1
' Ouput temp1
'*******************************************************************************
toupper                 cmp     temp1, #"a"             wc
        if_c            jmp     #toupper_ret
                        cmp     temp1, #"z"             wc, wz
        if_nc_and_nz    jmp     #toupper_ret
                        sub     temp1, #"a" - "A"
toupper_ret             ret              


'*******************************************************************************
' Print an 8-digit hex value to the output port
' Input parm1
' Changes parm, parm1, parm2
' Output none
'*******************************************************************************
printhex                mov     parm2, #8
:loop                   rol     parm1, #4
                        mov     parm, #15
                        and     parm, parm1
                        add     parm, a_hexstr
                        rdbyte  parm, parm
                        call    #putch
                        djnz    parm2, @:loop
printhex_ret            ret


'*******************************************************************************
' Convert a string to a hex number
' Input parm1, parm2
' Changes parm, temp, temp1, temp2
' Output parm
'*******************************************************************************
gethex                  mov     parm, #0
                        cmps    parm2, #0               wc, wz
        if_c_or_z       jmp     #gethex_ret
                        mov     temp1, parm1
                        mov     temp2, parm2
:loop                   rdbyte  temp, temp1
                        add     temp1, #1
                        sub     temp, #"0"
                        cmps    temp, #10               wc
        if_nc           sub     temp, #"a"-"0"-10
                        shl     parm, #4
                        add     parm, temp
                        djnz    temp2, @:loop
gethex_ret              ret

'*******************************************************************************
' Push a value onto the data stack
' Input parm1
' No changes
' Output none
'*******************************************************************************
push1                   wrlong  parm1, stackptr
                        add     stackptr, #4
push_ret                ret

'*******************************************************************************
' Push a value onto the data stack and jump to the innerloop
' Input parm1
' No changes
' Output none
'*******************************************************************************
push_jmp                wrlong  parm1, stackptr
                        add     stackptr, #4
                        jmp     #innerloop

'*******************************************************************************
' Pop two values off of the data stack
' Input none
' Changes parm1, parm2
' Output parm1, parm2
'*******************************************************************************
pop2                    sub     stackptr, #4
                        rdlong  parm2, stackptr

'*******************************************************************************
' Pop one value off of the data stack
' Input none
' Changes parm1
' Ouput parm1
'*******************************************************************************
pop1                    sub     stackptr, #4
                        rdlong  parm1, stackptr         wz
pop_ret
pop2_ret                ret

'*******************************************************************************
' Read a value on the stack based on an index number
' Changes parm1, temp1
'*******************************************************************************
indexstack              neg     temp1, parm1
                        shl     temp1, #2
                        sub     temp1, #4
                        add     temp1, stackptr
                        rdlong  parm1, temp1
indexstack_ret          ret

'*******************************************************************************
' Compute the XT from the address of the link
' Input:   parm1
' Output:  parm1
' Changes: temp1
'*******************************************************************************
link2xt                 mov     temp1, parm1
                        add     temp1, #3
                        rdbyte  parm1, temp1            ' Get name length
                        add     parm1, temp1
                        add     parm1, #4
                        and     parm1, minus4           ' Align
link2xt_ret             ret

'*******************************************************************************
' Multiply two 32-bit numbers
' Changes parm2, temp1, temp2
'*******************************************************************************
multiply                mov     temp1, #0
                        mov     temp2, #32
                        shr     parm1, #1               wc
mmul    if_c            add     temp1, parm2            wc
                        rcr     temp1, #1               wc
                        rcr     parm1, #1               wc
                        djnz    temp2, @mmul
multiply_ret            ret

'*******************************************************************************
' Divide two 32-bit numbers producing a quotient and a remainder
' Changes parm1, parm2, parm3, temp1, temp2
'*******************************************************************************
divide                  mov     temp2, #32
                        mov     temp1, #0
                        abs     parm1, parm1            wc
                        muxc    parm3, #%11
                        abs     parm2, parm2            wc,wz
        if_c            xor     parm3, #%01
'        if_nz           jmp     #mdiv
'                        mov     parm1, #0
'                        jmp     divide_ret
mdiv                    shr     parm2, #1               wc,wz
                        rcr     temp1, #1
        if_nz           djnz    temp2, @mdiv
mdiv2                   cmpsub  parm1, temp1            wc
                        rcl     parm2, #1
                        shr     temp1, #1
                        djnz    temp2, @mdiv2
divide_ret              ret

'*******************************************************************************
' These are working registers.  The parm registers are generally used to pass
' parameters from one routine to another, and the temp registers are used as
' temporary storage within a routine.
'*******************************************************************************

'*******************************************************************************
' Addresses of variables in the dictionary, and the hex table
'*******************************************************************************
a_hexstr                long      @hexstr+Q
a_last                  long      @last+Q
a_state                 long      @state+Q
a_dp                    long      @dp+Q
a_tib                   long      @tib+Q
a_verbose               long      @verbose+Q
a_inputidx              long      @greaterin+Q
a_inputlen              long      @poundtib+Q

'*******************************************************************************
' The data and return stack pointers, and their base addresses
'*******************************************************************************
'stackptr                long      0
'stackptr0               long      0
'returnptr               long      0
'returnptr0              long      0
stackptr                long      @stack+16
stackptr0               long      @stack+16
returnptr               long      @retstk
returnptr0              long      @retstk

'*******************************************************************************
' The input file pointer used during initialization
'*******************************************************************************
infileptr               long      @infile+Q

'*******************************************************************************
' Constants
'*******************************************************************************
minus4                  long      -4

                        fit       $1f0

                        orgh

pfthconfig              long      @xboot_1+Q            ' Initial word to execute
                        long      @stack+Q+16           ' Starting stack pointer
                        long      @stack+Q+16           ' Empty stack pointer value
                        long      @retstk+Q             ' Starting return pointer
                        long      @retstk+Q             ' Empty return pointer value
stack                   long      0[100]                ' Data stack
retstk                  long      0[100]                ' Return stack

'*******************************************************************************
' Input buffer and hex table
'*******************************************************************************
hexstr                  byte      "0123456789abcdef"
inputbuf                byte      0[200]

'*******************************************************************************
' This is the beginning of the dictionary.  The kernel words are specified below
'*******************************************************************************
exit_L        word      0
              byte      FLAG_CORE, 4, "exit"
              long
exit_X        word      exitfunc, 0

quit_L        word      @exit_L+Q
              byte      FLAG_CORE, 4, "quit"
              long
quit_X        word      quitfunc, 0

abort_L       word      @quit_L+Q
              byte      FLAG_CORE, 5, "abort"
              long
abort_X       word      abortfunc, 0

execute_L     word      @abort_L+Q
              byte      FLAG_CORE, 7, "execute"
              long
execute_X     word      executefunc, 0

word_L        word      @execute_L+Q
              byte      FLAG_CORE, 4, "word"
              long
word_X        word      wordfunc, 0

find_L        word      @word_L+Q
              byte      FLAG_CORE, 4, "find"
              long
find_X        word      findfunc, 0

getchar_L     word      @find_L+Q
              byte      FLAG_CORE, 7, "getchar"
              long
getchar_X     word      getcharfunc, 0

getfchar_L    word      @getchar_L+Q
              byte      FLAG_CORE, 8, "getfchar"
              long
getfchar_X    word      getfcharfunc, 0

key_L         word      @getfchar_L+Q
              byte      FLAG_CORE, 3, "key"
              long
key_X         word      deferfunc, @key_B+Q
key_B         word      @getfchar_X+Q, 0
'key_B         word      @getchar_X+Q, 0

create_L      word      @key_L+Q
              byte      FLAG_CORE, 6, "create"
              long
create_X      word      createfunc, 0

_lit_L        word      @create_L+Q
              byte      FLAG_LIT, 4, "_lit"
              long
_lit_X        word      _litfunc, 0

_gethex_L     word      @_lit_L+Q
              byte      FLAG_CORE, 7, "_gethex"
              long
_gethex_X     word      _gethexfunc, 0

emit_L        word      @_gethex_L+Q
              byte      FLAG_CORE, 4, "emit"
              long
emit_X        word      emitfunc, 0

store_L       word      @emit_L+Q
              byte      FLAG_CORE, 1, "!"
              long
store_X       word      storefunc, 0

fetch_L       word      @store_L+Q
              byte      FLAG_CORE, 1, "@"
              long
fetch_X       word      fetchfunc, 0

wstore_L      word      @fetch_L+Q
              byte      FLAG_CORE, 2, "w!"
              long
wstore_X      word      wstorefunc, 0

wfetch_L      word      @wstore_L+Q
              byte      FLAG_CORE, 2, "w@"
              long
wfetch_X      word      wfetchfunc, 0

cstore_L      word      @wfetch_L+Q
              byte      FLAG_CORE, 2, "c!"
              long
cstore_X      word      cstorefunc, 0

cfetch_L      word      @cstore_L+Q
              byte      FLAG_CORE, 2, "c@"
              long
cfetch_X      word      cfetchfunc, 0

plus_L        word      @cfetch_L+Q
              byte      FLAG_CORE, 1, "+"
              long
plus_X        word      plusfunc, 0

minus_L       word      @plus_L+Q
              byte      FLAG_CORE, 1, "-"
              long
minus_X       word      minusfunc, 0

multiply_L    word      @minus_L+Q
              byte      FLAG_CORE, 1, "*"
              long
multiply_X    word      multfunc, 0

divide_L      word      @multiply_L+Q
              byte      FLAG_CORE, 1, "/"
              long
divide_X      word      dividefunc, 0

mod_L         word      @divide_L+Q
              byte      FLAG_CORE, 3, "mod"
              long
mod_X         word      modfunc, 0

and_L         word      @mod_L+Q
              byte      FLAG_CORE, 3, "and"
              long
and_X         word      andfunc, 0

or_L          word      @and_L+Q
              byte      FLAG_CORE, 2, "or"
              long
or_X          word      orfunc, 0

xor_L         word      @or_L+Q
              byte      FLAG_CORE, 3, "xor"
              long
xor_X         word      xorfunc, 0

less_L        word      @xor_L+Q
              byte      FLAG_CORE, 1, "<"
              long
less_X        word      lessfunc, 0

equal_L       word      @less_L+Q
              byte      FLAG_CORE, 1, "="
              long
equal_X       word      equalfunc, 0

greater_L     word      @equal_L+Q
              byte      FLAG_CORE, 1, ">"
              long
greater_X     word      greaterfunc, 0

rshift_L      word      @greater_L+Q
              byte      FLAG_CORE, 6, "rshift"
              long
rshift_X      word      rshiftfunc, 0

lshift_L      word      @rshift_L+Q
              byte      FLAG_CORE, 6, "lshift"
              long
lshift_X      word      lshiftfunc, 0

depth_L       word      @lshift_L+Q
              byte      FLAG_CORE, 5, "depth"
              long
depth_X       word      depthfunc, 0

tib_L         word      @depth_L+Q
              byte      FLAG_VAR, 3, "tib"
              long
tib_X         word      varfunc, @tib+Q+4
tib           long      @inputbuf+Q
              word      @fetch_X+Q, 0
              long

poundtib_L    word      @tib_L+Q
              byte      FLAG_VAR, 4, "#tib"
              long
poundtib_X    word      varfunc, 0
poundtib      long      0

greaterin_L   word      @poundtib_L+Q
              byte      FLAG_VAR, 3, ">in"
              long
greaterin_X   word      varfunc, 0
greaterin     long      0

dp_L          word      @greaterin_L+Q
              byte      FLAG_VAR, 2, "dp"
              long
dp_X          word      varfunc, 0
dp            long      @_here+Q

last_L        word      @dp_L+Q
              byte      FLAG_VAR, 4, "last"
              long
last_X        word      varfunc, 0
last          long      @_last+Q

state_L       word      @last_L+Q
              byte      FLAG_VAR, 5, "state"
              long
state_X       word      varfunc, 0
state         long      0

base_L        word      @state_L+Q
              byte      FLAG_VAR, 4, "base"
              long
base_X        word      varfunc, 0
base          long      16

verbose_L     word      @base_L+Q
              byte      FLAG_VAR, 7, "verbose"
              long
verbose_X     word      varfunc, 0
verbose       long      0

forth_L       word      @verbose_L+Q
              byte      FLAG_VAR, 5, "forth"
              long
forth_X       word      varfunc, 0
              long      @forth+Q

drop_L        word      @forth_L+Q
              byte      FLAG_CORE, 4, "drop"
              long
drop_X        word      dropfunc, 0

dup_L         word      @drop_L+Q
              byte      FLAG_CORE, 3, "dup"
              long
dup_X         word      dupfunc, 0

swap_L        word      @dup_L+Q
              byte      FLAG_CORE, 4, "swap"
              long
swap_X        word      swapfunc, 0

pick_L        word      @swap_L+Q
              byte      FLAG_CORE, 4, "pick"
              long
pick_X        word      pickfunc, 0

roll_L        word      @pick_L+Q
              byte      FLAG_CORE, 4, "roll"
              long
roll_X        word      rollfunc, 0

tor_L         word      @roll_L+Q
              byte      FLAG_CORE, 2, ">r"
              long
tor_X         word      torfunc, 0

fromr_L       word      @tor_L+Q
              byte      FLAG_CORE, 2, "r>"
              long
fromr_X       word      fromrfunc, 0

colon_L       word      @fromr_L+Q
              byte      FLAG_CORE, 1, ":"
              long
colon_X       word      colonfunc, 0

semicolon_L   word      @colon_L+Q
              byte      FLAG_SEMI, 1, ";"
              long
semicolon_X   word      semicolonfunc, 0 

cogfetch_L    word      @semicolon_L+Q
              byte      FLAG_CORE, 4, "cog@"
              long
cogfetch_X    word      cogfetchfunc, 0

cogstore_L    word      @cogfetch_L+Q
              byte      FLAG_CORE, 4, "cog!"
              long
cogstore_X    word      cogstorefunc, 0

cogx1_L       word      @cogstore_L+Q
              byte      FLAG_CORE, 5, "cogx1"
              long
cogx1_X       word      cogx1func, 0

_jz_L         word      @cogx1_L+Q
              byte      FLAG_JMP, 3, "_jz"
              long
_jz_X         word      _jzfunc, 0

cmove_L       word      @_jz_L+Q
              byte      FLAG_CORE, 5, "cmove"
              long
cmove_X       word      cmovefunc, 0

dotx_L        word      @cmove_L+Q
              byte      FLAG_CORE, 2, ".x"
              long
dotx_X        word      dotxfunc, 0

'*******************************************************************************
' SPI/SD Variables
'*******************************************************************************
spi_vars_L    word      @dotx_L+Q
              byte      FLAG_VAR, 8, "spi_vars"
              long
spi_vars_X    word      varfunc, 0
spi_vars      long      0       ' SPI_engine_cog
              long      0       ' SPI_command
              long      0       ' SPI_block_index
              long      0       ' SPI_buffer_address
              long      0       ' SD_rootdir
              long      0       ' SD_filesystem
              long      0       ' SD_clustershift
              long      0       ' SD_dataregion
              long      0       ' SD_fat1
              long      0       ' SD_sectorsperfat
              long      0       ' SD_currdir

argc_L        word      @spi_vars_L+Q
              byte      FLAG_VAR, 4, "argc"
              long
argc_X        word      confunc, 0
argc_B        long      0

argv_L        word      @argc_L+Q
              byte      FLAG_VAR, 4, "argv"
              long
argv_X        word      confunc, 0
argv_B        long      0

hostcwd_L     word      @argv_L+Q
              byte      FLAG_VAR, 7, "hostcwd"
              long
hostcwd_X     word      confunc, 0
hostcwd_B     long      0

'*******************************************************************************
' A small number of compiled words follow below.  These are used by the boot
' interpreter.
'*******************************************************************************
              ' : here dp @ ;
here_L        word      @hostcwd_L+Q
              byte      FLAG_DEF, 4, "here"
              long
here_X        word      execlistfunc, 0
              word      @dp_X+Q, @fetch_X+Q, 0
              long
              
              ' : allot dp @ + dp ! ;
allot_L       word      @here_L+Q
              byte      FLAG_DEF, 5, "allot"
              long
allot_X       word      execlistfunc, 0
              word      @dp_X+Q, @fetch_X+Q, @plus_X+Q, @dp_X+Q, @store_X+Q, 0
              long

              ' : , here ! 4 allot ;
comma_L       word      @allot_L+Q
              byte      FLAG_DEF, 1, ","
              long
comma_X       word      execlistfunc, 0
              word      @here_X+Q, @store_X+Q, @_lit_X+Q, 4, @allot_X+Q, 0
              long

              ' : _jmp r> @ >r ;
_jmp_L        word      @comma_L+Q
              byte      FLAG_JMP, 4, "_jmp"
              long
_jmp_X        word      execlistfunc, 0
              word      @fromr_X+Q, @wfetch_X+Q, @tor_X+Q, 0
              long

              ' : count 0 pick 1 + 1 roll c@ ;
count_L       word      @_jmp_L+Q
              byte      FLAG_DEF, 5, "count"
              long
count_X       word      execlistfunc, 0
              word      @_lit_X+Q, 0, @pick_X+Q, @_lit_X+Q, 1, @plus_X+Q, @_lit_X+Q, 1, @roll_X+Q, @cfetch_X+Q, 0
              long

              ' : accept ( addr size -- num ) \ Accept a string from the input source
accept_L      word      @count_L+Q
              byte      FLAG_DEF, 6, "accept"
              long
accept_X      word      execlistfunc, 0
              ' >r dup
              word      @tor_X+Q, @dup_X+Q
              ' r> dup 1 < _jz _accept4
accept_1      word      @fromr_X+Q, @dup_X+Q, @_lit_X+Q, 1, @less_X+Q, @_jz_X+Q, @accept_4+Q
              ' drop swap - exit
              word      @drop_X+Q, @swap_X+Q, @minus_X+Q, @exit_X+Q
              ' >r key
accept_4      word      @tor_X+Q, @key_X+Q
              ' dup 0d = over 0a = or
              word      @dup_X+Q, @_lit_X+Q, $0d, @equal_X+Q, @_lit_X+Q, 1, @pick_X+Q, @_lit_X+Q, $0a, @equal_X+Q, @or_X+Q
              ' _jz _accept2
              word      @_jz_X+Q, @accept_2+Q
              ' cr drop swap -
              word      @_lit_X+Q, 13, @emit_X+Q, @_lit_X+Q, 10, @emit_X+Q, @drop_X+Q, @swap_X+Q, @minus_X+Q
              ' r> drop exit
              word      @fromr_X+Q, @drop_X+Q, @exit_X+Q
              ' dup 8 = _jz _accept3
accept_2      word      @dup_X+Q, @_lit_X+Q, 8, @equal_X+Q, @_jz_X+Q, @accept_3+Q
              ' drop over over - _jz _accept1
              word      @drop_X+Q, @_lit_X+Q, 1, @pick_X+Q, @_lit_X+Q, 1, @pick_X+Q, @minus_X+Q, @_jz_X+Q, @accept_1+Q
              ' 1 - r> 1 + >r
              word      @_lit_X+Q, 1, @minus_X+Q, @fromr_X+Q, @_lit_X+Q, 1, @plus_X+Q, @tor_X+Q
              ' 8 emit bl emit 8 emit _jmp _accept1
              word      @_lit_X+Q, 8, @emit_X+Q, @_lit_X+Q, 32, @emit_X+Q, @_lit_X+Q, 8, @emit_X+Q, @_jmp_X+Q, @accept_1+Q
              ' dup emit over c! 1 +
accept_3      word      @dup_X+Q, @emit_X+Q, @_lit_X+Q, 1, @pick_X+Q, @cstore_X+Q, @_lit_X+Q, 1, @plus_X+Q
              ' r> 1 - >r _jmp _accept1
              word      @fromr_X+Q, @_lit_X+Q, 1, @minus_X+Q, @tor_X+Q, @_jmp_X+Q, @accept_1+Q, 0
              long
              
              ' : refill tib 200 accept #tib ! 0 >in ! ;
refill_L      word      @accept_L+Q
              byte      FLAG_DEF, 6, "refill"
              long
refill_X      word      execlistfunc, 0
              word      @tib_X+Q, @_lit_X+Q, 200, @accept_X+Q, @poundtib_X+Q, @store_X+Q, @_lit_X+Q, 0, @greaterin_X+Q, @store_X+Q, 0
              long

              ' : compile, here w! 2 allot ;
compcomma_L   word      @refill_L+Q
              byte      FLAG_DEF, 8, "compile,"
              long
compcomma_X   word      execlistfunc, 0
              word      @here_X+Q, @wstore_X+Q, @_lit_X+Q, 2, @allot_X+Q, 0
              long
              
'*******************************************************************************
' The boot interpreter follows below.
'*******************************************************************************
              ' : xboot ( This word runs a simple interpreter )
xboot_L       word      @compcomma_L+Q
              byte      FLAG_DEF, 5, "xboot"
              long
xboot_X       word      execlistfunc, 0

              ' 20 word 0 pick c@ _jz _xboot2 ( Get word, refill if empty )
xboot_1       word      @_lit_X+Q, $20, @word_X+Q, @_lit_X+Q, 0, @pick_X+Q, @cfetch_X+Q, @_jz_X+Q, @xboot_2+Q
              
              ' find 0 pick _jz _xboot3 ( Find word, get number if not found )
              word      @find_X+Q, @_lit_X+Q, 0, @pick_X+Q, @_jz_X+Q, @xboot_3+Q
              
              ' state @ = _jz _xboot4 ( Go execute if not compile mode or immediate )
              word      @state_X+Q, @fetch_X+Q, @equal_X+Q, @_jz_X+Q, @xboot_4+Q
              
              ' compile, _jmp _xboot1 ( Otherwise, compile and loop again )
              word       @compcomma_X+Q, @_jmp_X+Q, @xboot_1+Q
              
              ' execute _jmp _xboot1 ( Execute and loop again )
xboot_4       word      @execute_X+Q, @_jmp_X+Q, @xboot_1+Q

              ' drop count _gethex ( Get number )
xboot_3       word      @drop_X+Q, @count_X+Q, @_gethex_X+Q

              ' state @ _jz _xboot1 ( Loop again if not compile mode )
              word      @state_X+Q, @fetch_X+Q, @_jz_X+Q, @xboot_1+Q
              
              ' ['] _lit , , _jmp _xboot1 ( Otherwise, compile number and loop again )
              word       @_lit_X+Q, @_lit_X+Q, @compcomma_X+Q, @compcomma_X+Q, @_jmp_X+Q, @xboot_1+Q

              ' drop refill _jmp _xboot1 ( Refill and loop again )
xboot_2       word      @drop_X+Q, @refill_X+Q, @_lit_X+Q, 13, @emit_X+Q, @_jmp_X+Q, @xboot_1+Q, 0
              long

switch_L      word      @xboot_L+Q
              byte      FLAG_DEF, 6, "switch"
              long
switch_X      word      execlistfunc, 0
              word      @_lit_X+Q, @getchar_X+Q, @_lit_X+Q, @key_B+Q, @store_X+Q, 0
              long

_last         long

_loop_L       word      @switch_L+Q
              byte      FLAG_CORE, 5, "_loop"
              long
_loop_X       word      _loopfunc, 0

_here         long

'*******************************************************************************
' The Forth source files follow below.  They will be compiled into the
' dictionary, which will over-write the source data.  Some padding space is
' included to ensure that we don't over-write the source data before it is
' compiled.
'*******************************************************************************
              long      0[100]
infile        byte      "1 verbose !", 13
              file      "init.fth"
              file      "comus.fth"
              'file      "see.fth"
              'file      "propwords.fth"
              'file      "bufser.fth"
              'file      "i2c.fth"
              'file      "fds.fth"
              'file      "time.fth"
              'file      "toggle.fth"
              'file      "primes.fth"
              'byte      13, " 1 verbose !", 13
              'byte      " .s ", 13
              'file      "chess.fth"

'*******************************************************************************
' Enable serial output, print version string and switch to serial input
'*******************************************************************************
              byte      13
              byte      " 1 verbose !"
              byte      " pfthversion type cr"
              byte      " switch", 13
              

{
+--------------------------------------------------------------------
|  TERMS OF USE: MIT License
+--------------------------------------------------------------------
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
+------------------------------------------------------------------
}