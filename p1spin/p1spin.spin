'******************************************************************************
' P1 Spin Interpreter for P2
'
' Copyright (c) 2014, Dave Hein
' See end of file for terms of use.
'******************************************************************************
CON
  _clkmode = xtal1+pll16x
  _clkfreq = 80_000_000

DAT
                        orgh    $380
                        org

			' Add address offset to Spin header
startup                 mov     temp1, ptestprog
                        add     temp1, #6
                        mov     temp3, #5
:loop                   rdword  temp2, temp1
                        add     temp2, ptestprog
                        wrword  temp2, temp1
                        add     temp1, #2
                        djnz    temp3, @:loop

			' Clear VAR area
                        mov     temp1, ptestprog
			add	temp1, #8		' Get address of vbase
			rdword	temp2, temp1		' Get vbase
			add	temp1, #2		' Get address of dbase
			rdword	temp3, temp1		' Get dbase
			sub	temp3, temp2		' Compute number of bytes
			shr	temp3, #2		' Compute number of longs
			mov	temp4, #0
:loop1			wrlong	temp4, temp2
			add	temp2, #4
			djnz	temp3, @:loop1

			' Initialize stack frame and result variable
                        mov     temp1, ptestprog
			add	temp1, #10		' Get address of dbase
			rdword	temp1, temp1		' Get dbase
			mov	temp2, #0
			wrlong	temp2, temp1		' Initialize result to zero
			sub	temp1, #2
			wrword  pexitcode, temp1	' Set return address to exit code
			sub	temp1, #6
			mov	temp2, #2
			wrword	temp2, temp1		' Write first word of stack frame

			' Start the Spin interpreter
                        mov     temp1, pinterp
                        mov     temp2, ptestprog
                        add     temp2, #4
                        coginit temp1, temp2, #0

temp1                   long    0
temp2                   long    0
temp3                   long    0
temp4                   long    0
pinterp                 long    @interp
ptestprog               long    @testprog
ppbase                  long    @pbase
pexitcode		long	@exitcode

                        orgh

DAT

' This table decodes the Spin bytecode.
jump_table
'              0/8     1/9     2/A     3/B     4/C     5/D     6/E     7/F
    word {00} ldfrmr, ldfrmr, ldfrmr, ldfrmr, _jmp,   _call,  callobj,callobx
    word {08} _tjz,   _djnz,  _jz,    _jnz,   casedon,caseval,caseran,lookdon
    word {10} lookupv,lookdnv,lookupr,lookdnr,_pop,   _run,   _strsiz,_strcmp
    word {18} _bytefl,_wordfl,_longfl,_waitpe,_bytemv,_wordmv,_longmv,_waitpn

    word {20} _clkset,cogstp, lckret, _waitcn,rwregx, rwregx, rwregx, _waitvi
    word {28} _cogini,lcknewr,lcksetr,lckclrr,_cogini,lcknew, lckset, lckclr
    word {30} _abort, abrtval,_ret,   retval, ldlix,  ldlix,  ldlix,  ldlip  
    word {38} ldbi,   ldwi,   ldmi,   ldli,   unsupp, rwregb, rwregbs,rwreg  

    word {40} ldlvc,  stlvc,  exlvc,  lalvc,  ldlvc,  stlvc,  exlvc,  lalvc  
    word {48} ldlvc,  stlvc,  exlvc,  lalvc,  ldlvc,  stlvc,  exlvc,  lalvc  
    word {50} ldlvc,  stlvc,  exlvc,  lalvc,  ldlvc,  stlvc,  exlvc,  lalvc  
    word {58} ldlvc,  stlvc,  exlvc,  lalvc,  ldlvc,  stlvc,  exlvc,  lalvc  

    word {60} ldllc,  stllc,  exllc,  lallc,  ldllc,  stllc,  exllc,  lallc  
    word {68} ldllc,  stllc,  exllc,  lallc,  ldllc,  stllc,  exllc,  lallc  
    word {70} ldllc,  stllc,  exllc,  lallc,  ldllc,  stllc,  exllc,  lallc  
    word {78} ldllc,  stllc,  exllc,  lallc,  ldllc,  stllc,  exllc,  lallc  

    word {80} ldba,   stba,   exba,   la_a,   ldbo,   stbo,   exbo,   la_o   
    word {88} ldbv,   stbv,   exbv,   la_v,   ldbl,   stbl,   exwl,   la_l   
    word {90} ldbax,  stbax,  exbax,  labax,  ldbox,  stbox,  exbox,  labox  
    word {98} ldbvx,  stbvx,  exbvx,  labvx,  ldblx,  stblx,  exblx,  lablx

    word {A0} ldwa,   stwa,   exwa,   la_a,   ldwo,   stwo,   exwo,   la_o   
    word {A8} ldwv,   stwv,   exwv,   la_v,   ldwl,   stwl,   exwl,   la_l   
    word {B0} ldwax,  stwax,  exwax,  lawax,  ldwox,  stwox,  exwox,  lawox
    word {B8} ldwvx,  stwvx,  exwvx,  lawvx,  ldwlx,  stwlx,  exwlx,  lawlx

    word {C0} ldla,   stla,   exla,   la_a,   ldlo,   stlo,   exlo,   la_o   
    word {C8} ldlv,   stlv,   exlv,   la_v,   ldll,   stll,   exll,   la_l   
    word {D0} ldlax,  stlax,  exlax,  lalax,  ldlox,  stlox,  exlox,  lalox  
    word {D8} ldlvx,  stlvx,  exlvx,  lalvx,  ldllx,  stllx,  exllx,  lallx

    word {E0} _ror,   _rol,   _shr,   _shl,   _min,   _max,   _neg,   _com   
    word {E8} _and,   _abs,   _or,    _xor,   _add,   _sub,   _sar,   _rev   
    word {F0} _andl,  encode, _orl,   decode, _mul,   _mulh,  _div,   _mod   
    word {F8} _sqrt,  _cmplt, _cmpgt, _cmpne, _cmpeq, _cmple, _cmpge, _notl  

' This table decodes the extra byte that is used to perform extended operations.
jump_table_ex
'              0/8     1/9     2/A     3/B     4/C     5/D     6/E     7/F
    word {00} store,  unsupx, repeatx,unsupx, unsupx, unsupx, repeats,unsupx
    word {08} randf,  unsupx, unsupx, unsupx, randr,  unsupx, unsupx, unsupx
    word {10} sexb,   unsupx, unsupx, unsupx, sexw,   unsupx, unsupx, unsupx
    word {18} postclr,unsupx, unsupx, unsupx, postset,unsupx, unsupx, unsupx

    word {20} unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, preinc, unsupx
    word {28} unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, postinc,unsupx
    word {30} unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, predec, unsupx
    word {38} unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, postdec,unsupx

    word {40} _rorx,  _rolx,  _shrx,  _shlx,  _minx,  _maxx,  _negx,  _comx  
    word {48} _andx,  _absx,  _orx,   _xorx,  _addx,  _subx,  _sarx,  _revx  
    word {50} _andlx, encodex,_orlx,  decodex,_mulx,  _mulhx, _divx,  _modx  
    word {58} _sqrtx, _cmpltx,_cmpgtx,_cmpnex,_cmpeqx,_cmplex,_cmpgex,_notlx 

    word {60} unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, unsupx
    word {68} unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, unsupx
    word {70} unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, unsupx
    word {78} unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, unsupx, unsupx

			' Bytecodes for cogstop(cogid)
exitcode                byte $3f, $89, $21

                        org     0

'***********************************************************************
'                       Copy 5 Spin state variable to registers
'***********************************************************************
interp                  setinda #pbase
                        reps    #5, #1
                        cogid   id
                        rdword  inda++,++ptra
                        jmp     #loop
                        long    0[3]

'***********************************************************************
'                       Push x to the stack
'***********************************************************************
pushx1                  wrlong  x, dcurr
                        add     dcurr, #4

'***********************************************************************
' This is the main loop that fetches bytecodes and executes them
'***********************************************************************
loop                    mov     t1, jumps
                        rdbyte  op,pcurr                'get opcode
                        add     pcurr,#1
                        add     t1, op
                        add     t1, op
                        rdword  t2, t1
                        jmp     t2

'***********************************************************************
' This code handles extended operators, such as ++ and +=
'***********************************************************************
exoplong                add     adr, op2
exoplong1               rdlong  x, adr
                        mov     savex, #exoplong2
                        jmp     #exop1

exopword                add     adr, op2
exopword1               rdword  x, adr
                        mov     savex, #exopword2
                        jmp     #exop1

exopbyte                add     adr, op2
exopbyte1               rdbyte  x, adr
                        mov     savex, #exopbyte2

exop1                   rdbyte  op2,pcurr
                        mov     t1, op2
                        and     t1, #$7f
                        shl     t1, #1
                        add     t1, jumpx
                        rdword  t2, t1
                        add     pcurr,#1
                        jmp     t2

'***********************************************************************
' This code saves the result from the extended operation
'***********************************************************************
postsave                add     savex, #1
                        jmp     savex

exopbyte2               mov     y, x
postbyte                wrbyte  y, adr
                        and     x, #$ff
                        jmp     #exopdone

exopword2               mov     y, x
postword                wrword  y, adr
                        and     x, maskword
                        jmp     #exopdone

exoplong2               mov     y, x
postlong                wrlong  y, adr

exopdone                test    op2, #$80  wc          'check for load bit
                        mov     savex, #pushx1
        if_c            jmp     #pushx1
        if_nc           jmp     #loop

'***********************************************************************
' Load Immediate Instructions
'***********************************************************************
'
'                       Load Long Immediate 1, 0 and -1
ldlix                   sub     op, #$35
                        mov     x, op
                        jmp     #pushx1

'                       Load Long Immediate Packed
ldlip                   mov     x, #2
                        rdbyte  y,pcurr                 'constant mask, get data byte
                        add     pcurr,#1
                        rol     x,y                     'decode, x = 2 before
                        test    y,#%001_00000   wc      'decrement?
        if_c            sub     x,#1
                        test    y,#%010_00000   wc      'not?
        if_c            xor     x,masklong
                        jmp     #pushx1

'                       Load Byte Immediate
ldbi                    call    #fetchbyte
                        jmp     #pushx1

'                       Load Word Immediate
ldli                    call    #fetchbyte
                        mov     a, #3
                        jmp     #addbytes

'                       Load Word Immediate
ldmi                    call    #fetchbyte
                        mov     a, #2
                        jmp     #addbytes

'                       Load Word Immediate
ldwi                    call    #fetchbyte
                        mov     a, #1

addbytes                shl     x, #8
                        rdbyte  y, pcurr
                        add     pcurr, #1
                        or      x, y
                        djnz    a, @addbytes
                        jmp     #pushx1

'***********************************************************************
' Compact Local Variable Instructions
'***********************************************************************
'                       Load Long Local Compact
ldllc                   mov     op2, dbase
                        mov     adr, op
                        and     adr, #$1c

ldlong                  add     adr, op2
                        rdlong  x, adr
                        jmp     #pushx1

'                       Store Long Local Compact
stllc                   mov     adr, op
                        and     adr, #$1c
                        mov     op2, dbase

stlong                  add     adr, op2
                        call    #popx1
                        wrlong  x, adr
                        jmp     #loop

'                       Execute Long Local Compact
exllc                   mov     adr, op
                        and     adr, #$1c
                        mov     op2, dbase
                        jmp     #exoplong

'                       Load Address Long Local Compact
lallc                   and     op, #$1c
                        add     op, dbase
                        mov     x, op
                        jmp     #pushx1

'***********************************************************************
' DAT Variable Instructions
'***********************************************************************

'                       Load Long Object
ldlo                    call    #getadrz
                        mov     adr, pbase
                        jmp     #ldlong

'                       Store Long Object
stlo                    call    #getadrz
                        mov     adr, pbase
                        jmp     #stlong

'                       Execute Long Object
exlo                    call    #getadrz
                        mov     adr, pbase
                        jmp     #exoplong

'                       Load Address Object
la_o                    call    #getadrz
                        mov     x, op2
                        add     x, pbase
                        jmp     #pushx1

'***********************************************************************
' Indexed DAT Variable Instructions
'***********************************************************************

'                       Load Byte Object with Index
ldbox                   call    #getadrz
                        call    #pop_adr
                        add     adr, pbase
                        jmp     #ldbyte

ldbyte                  add     adr, op2
                        rdbyte  x, adr
                        jmp     #pushx1

ldword                  add     adr, op2
                        rdword  x, adr
                        jmp     #pushx1

'                       Store Byte Object with Index
stbox                   call    #getadrz
                        call    #pop_adr
                        add     adr, pbase
                        jmp     #stbyte

stbyte                  add     adr, op2
                        call    #popx1
                        wrbyte  x, adr
                        jmp     #loop

stword                  add     adr, op2
                        call    #popx1
                        wrword  x, adr
                        jmp     #loop

'                       Load Address Byte Object with Index
labox                   call    #getadrz
                        call    #popx1
                        add     x, op2
                        add     x, pbase
                        jmp     #pushx1

'***********************************************************************
' Absolute Address Instructions
'***********************************************************************

'                       Load Byte Absolute
ldba                    call    #popy1
                        jmp     #ldba1

'                       Load Word Absolute
ldwa                    call    #popy1
                        jmp     #ldwa1

'                       Load Long Absolute
ldla                    call    #popy1
	if_z		jmp	#ldclkfreq
                        jmp     #ldla1

ldbax                   call    #popay
                        add     y, a
ldba1                   rdbyte  x, y
                        jmp     #pushx1

'                       Load Word Absolute with Index
ldwax                   call    #popay
                        shl     a, #1
                        add     y, a
ldwa1                   rdword  x, y
                        jmp     #pushx1

'                       Load Long Absolute with Index
ldlax                   call    #popay
                        shl     a, #2
                        add     y, a
ldla1                   rdlong  x, y
                        jmp     #pushx1

'***********************************************************************
' Jump Instructions
'***********************************************************************

'                       Test and Jump on Zero
_tjz                    call    #getadrs
                        call    #popx1
        if_nz           add     dcurr, #4
                        jmp     #_jz1

'                       Jump on Zero
_jz                     call    #getadrs
                        call    #popx1
_jz1    if_z            add     pcurr, op2
                        jmp     #loop

'                       Decrement and Jump on NonZero
_djnz                   call    #getadrs
                        call    #popx1
                        sub     x, #1           wz
        if_nz           add     pcurr, op2
        if_nz           jmp     #pushx1
                        jmp     #loop

'                       Jump on NonZero
_jnz                    call    #getadrs
                        call    #popx1
        if_nz           add     pcurr, op2
                        jmp     #loop

'                       Jump
_jmp                    call    #getadrs
                        add     pcurr, op2
                        jmp     #loop

'***********************************************************************
' Math Instructions
'***********************************************************************
_shr                    call    #popyx
                        shr     x, y
                        jmp     savex

_shl                    call    #popyx
                        shl     x, y
                        jmp     savex

_ror                    call    #popyx
                        rol     x, y
                        jmp     savex

_rol                    call    #popyx
                        rol     x, y
                        jmp     savex

'                       Complement
_com                    call    #popx1
                        xor     x, masklong
                        jmp     savex

_and                    call    #popyx
                        and     x, y
                        jmp     savex

'                       Absolute
_abs                    call    #popx1
                        abs     x, x
                        jmp     savex

_or                     call    #popyx
                        or      x, y
                        jmp     savex

'                       Add
_add                    call    #popyx
                        add     x, y
                        jmp     savex

'                       Subtract
_sub                    call    #popyx
                        sub     x, y
                        jmp     savex

_neg                    call    #popx1
                        neg     x, x
                        jmp     savex

_xor                    call    #popyx
                        xor     x, y
                        jmp     savex

_sar                    call    #popyx
                        sar     x, y
                        jmp     savex

'                       Or Logical
_orl                    call    #popx1
        if_nz           neg     x, #1
                        call    #popy1
        if_nz           neg     x, #1
                        jmp     savex

_andl			call	#popx1
	if_nz		neg	x, #1
			call	#popy1
	if_z		mov	x, #0
			jmp	savex

_notl                   call    #popx1
                        muxz    x, masklong
                        jmp     savex

_mul                    call    #popyx
                        mul32   x, y
                        getmull x
                        jmp     savex

_mulh                   call    #popyx
                        mul32   x, y
                        getmulh x
                        jmp     savex

_div                    call    #popyx
                        div32   x, y
                        getdivq x
                        jmp     savex

_mod                    call    #popyx
                        div32   x, y
                        getdivr x
                        jmp     savex

'                       Compare if Less Than
_cmplt                  call    #popyx
                        cmps    x, y            wz, wc
                        muxc    x, masklong
                        jmp     savex

'                       Compare if Greater Than
_cmpgt                  call    #popyx
                        cmps    x, y            wz, wc
        if_nz_and_nc    neg     x, #1
        if_z_or_c       mov     x, #0
                        jmp     savex

'                       Compare if Less than or Equal
_cmple                  call    #popyx
                        cmps    x, y            wz, wc
        if_nz_and_nc    mov     x, #0
        if_z_or_c       neg     x, #1
                        jmp     savex

'                       Compare if Not Equal
_cmpne                  call    #popyx
                        sub     x, y            wz
        if_nz           neg     x, #1
                        jmp     savex


'                       Compare if Equal
_cmpeq                  call    #popyx
                        sub     x, y            wz
                        jmp     #_notlx

'                       Compare if Greater than or Equal
_cmpge                  call    #popyx
                        cmps    x, y            wz, wc
                        muxnc   x, masklong
                        jmp     savex

retval			call	#popx1
			jmp	#_ret1
_ret                    rdlong  x,dbase
_ret1                   call    #return1
pushz   if_z            jmp     #pushx1
        if_nz           jmp     #loop

'***********************************************************************
' Calling and Return Instructions
'***********************************************************************
return1                 mov     dcurr,dbase             'restore dcurr
                        sub     dcurr,#2                'pop pcurr
                        rdword  pcurr,dcurr
                        sub     dcurr,#2                'pop dbase
                        rdword  dbase,dcurr
                        sub     dcurr,#2                'pop vbase
                        rdword  vbase,dcurr
                        sub     dcurr,#2                'pop pbase (and flags)
                        rdword  pbase,dcurr
                        test    pbase,#%01      wz      'get push flag
                        test    pbase,#%10      wc      'get abort flag
                        and     pbase,maskpar           'trim pbase
                        ret

'                       Load Stack Frame
ldfrmr
ldfrm
ldfrmar
ldfrma                  jmp     #loadframe

callobj                 mov     a, #0
callobj1                call    #getcallparms
                        add     pbase, x
                        add     vbase, y

_call                   mov     a, #0
                        call    #getcallparms
                        jmp     #callmethod

'                       Read values from the Method Table
getcallparms            rdbyte  y,pcurr                 'get method table entry number
                        add     pcurr,#1
                        add     y,a                     'add index
                        shl     y,#2                    'lookup words from table
                        add     y,pbase
                        rdlong  y,y
                        mov     x,y                     'get low word
                        and     x,maskword
                        shr     y,#16                   'get high word
                        ret

'                       Call Method
callmethod              mov     dbase,dcall             'get new dcall
                        rdword  dcall,dcall             'set old dcall
                        wrword  pcurr,dbase             'set return pcurr
                        add     dbase,#2                'set call dbase
                        add     dcurr,y                 'set call dcurr
                        mov     pcurr,pbase             'set call pcurr
                        add     pcurr,x
                        jmp     #loop

loadframe               or      op,pbase                'add pbase into flags
                        wrword  op,dcurr                'push pbase plus flags
                        add     dcurr,#2
                        wrword  vbase,dcurr             'push vbase
                        add     dcurr,#2
                        wrword  dbase,dcurr             'push dbase
                        add     dcurr,#2
                        wrword  dcall,dcurr             'push dcall
                        mov     dcall,dcurr             'set new dcall
                        add     dcurr,#2
                        mov     x, #0                   'init result to 0
			jmp	#pushx1

_cogini1                coginit  y, a, #0
                        jmp     #pushz

'***********************************************************************
' Helper Functions
'***********************************************************************
'                       Get Unsigned Address
getadrz                 rdbyte  op2,pcurr
                        test    op2,#$80        wc
                        and     op2,#$7F
                        jmp     #getadr2
'                       Get Signed Address
getadrs                 rdbyte  op2,pcurr
                        test    op2,#$80        wc
                        shl     op2,#25
                        sar     op2,#25
getadr2                 add     pcurr,#1
        if_nc           ret
                        rdbyte  t2,pcurr
                        add     pcurr,#1
                        shl     op2,#8
                        or      op2,t2
                        ret

'                       Fetch a bytecode
fetchbyte               rdbyte  x, pcurr
                        add     pcurr, #1
                        ret

'                       Pop a from the stack
popayx                  sub     dcurr,#4
                        rdlong  a,dcurr

'                       Pop y from the stack
popyx                   sub     dcurr,#4
                        rdlong  y,dcurr

'                       Pop x from the stack
popx1                   sub     dcurr,#4
                        rdlong  x,dcurr         wz
                        ret

'                       Pop a from the stack
popay                   sub     dcurr,#4
                        rdlong  a,dcurr

'                       Pop y from the stack
popy1                   sub     dcurr, #4
                        rdlong  y, dcurr        wz
                        ret

'                       Pop adr from the stack
pop_adr                 sub     dcurr,#4
                        rdlong  adr,dcurr
                        ret

'***********************************************************************
' Extended Instructions
'***********************************************************************
store                   call    #popx1
                        jmp     savex

postclr                 mov     y, #0
                        jmp     #postsave

postset                 neg     y, #1
                        jmp     #postsave

postinc                 mov     y, x
                        add     y, #1
                        jmp     #postsave

postdec                 mov     y, x
                        sub     y, #1
                        jmp     #postsave

_rolx                   call    #popy1
                        rol     x, y
                        jmp     savex

_shlx                   call    #popy1
                        shl     x, y
                        jmp     savex

_divx                   call    #popy1
                        jmp     #_div+1

_modx                   call    #popy1
                        jmp     #_mod+1

_notlx                  muxz    x, masklong
                        jmp     savex

_rorx                   call    #popy1
                        ror     x, y
                        jmp     savex

_shrx                   call    #popy1
                        shr     x, y
                        jmp     savex

_andx                   call    #popy1
                        and     x, y
                        jmp     savex

_absx                   abs     x, x
                        jmp     savex

_negx                   neg     x, x
                        jmp     savex

_comx                   xor     x, masklong
                        jmp     savex

_orx                    call    #popy1
                        or      x, y
                        jmp     savex

'***********************************************************************
' Read/Write Register Instructions
'***********************************************************************
rwreg0                  rdbyte  op, pcurr
                        add     pcurr, #1
                        mov     adr, op
rwreg1                  and     adr, #$1f
                        add     adr, #$1e0
                        call    #mapreg
                        mov     regrevflag, #0
                        mov     reglsb, t1
                        mov     regnbits, t2
                        sub     regnbits,reglsb wc
        if_c            neg     regnbits, regnbits
        if_c            mov     reglsb, t2
        if_c            mov     regrevflag, #1
                        xor     regnbits, #31
                        neg     regmask, #1
                        shr     regmask, regnbits
                        test    op, #$20        wz
        if_z            jmp     #ldreg

streg                   sets    streg1, adr
                        setd    streg2, adr
                        call    #popx1
                        and     x, regmask
                        cmp     regrevflag, #0  wz
        if_nz           rev     x, regnbits
                        shl     regmask, reglsb
streg1                  mov     y, 0-0
                        andn    y, regmask
                        shl     x, reglsb
                        or      y, x
streg2                  mov     0-0, y
                        jmp     #loop

ldreg                   sets    ldreg1, adr
                        nop
                        cmp     adr, #$1f1      wz
        if_z            getcnt  x
ldreg1  if_nz           mov     x, 0-0
                        mov     regsave, x
                        shr     x, reglsb
                        and     x, regmask
                        cmp     regrevflag, #0  wz
        if_nz           rev     x, regnbits
                        test    op, #$40        wz
        if_z            jmp     #pushx1
                        mov     savex, #exopreg2
			jmp	#exop1

exopreg2                mov     y, x
postreg                 setd    exopreg3, adr
                        and     x, regmask
                        and     y, regmask
                        cmp     regrevflag, #0  wz
        if_nz           rev     y, regnbits
                        shl     y, reglsb
                        shl     regmask, reglsb
                        andn    regsave, regmask
                        or      y, regsave
exopreg3                mov     0-0, y
                        jmp     #exopdone

			' Mapping for port A
mapreg                  cmp	adr, #$1f2	wz     'P1 INA
        if_z            mov     adr, #$1f4             'P2 PINA
        if_z            ret
			cmp	adr, #$1f4	wz     'P1 OUTA
        if_z            mov     adr, #$1f8             'P2 OUTA
        if_z            ret
			cmp	adr, #$1f6	wz     'P1 DIRA
        if_z            mov     adr, #$1fc             'P2 DIRA
                        ret
{
			' Mapping for port C
mapreg                  cmp	adr, #$1f2	wz     'P1 INA
        if_z            mov     adr, #$1f6             'P2 PINC
        if_z            ret
			cmp	adr, #$1f4	wz     'P1 OUTA
        if_z            mov     adr, #$1fa             'P2 OUTC
        if_z            ret
			cmp	adr, #$1f6	wz     'P1 DIRA
        if_z            mov     adr, #$1fe             'P2 DIRC
                        ret
}
regrevflag              long    0
reglsb                  long    0
regmask                 long    0
regnbits                long    0
regsave                 long    0

'***********************************************************************
' Constants and Variables
'***********************************************************************
bitcycles               long    80000000 / 115200
jumps                   long    @jump_table
jumpx                   long    @jump_table_ex
savex                   long    pushx1
masklong                long    $FFFFFFFF
maskword                long    $FFFF
maskpar                 long    $0000FFFC
			fit	$1E9

			org	$1E9
id                      res	1
dcall                   res	1
pbase                   res	1
vbase                   res	1
dbase                   res	1
pcurr                   res	1
dcurr                   res	1

                        fit     $1F0

                        org     0
x                       res     1
y                       res     1
a                       res     1
t1                      res     1
t2                      res     1
op                      res     1
op2                     res     1
adr                     res     1

'***********************************************************************
' These instructions execute from hub memory
'***********************************************************************
                        orgh

' casedone
casedon                 sub     dcurr, #8
                        rdlong  pcurr, dcurr
                        add     pcurr, pbase
                        jmp     #loop

' casevalue
caseval                 call    #getadrs
                        call    #popyx
                        add     dcurr, #4
                        cmp     x, y            wz
        if_z            add     pcurr, op2
                        jmp     #loop

' caserange
caseran                 call    #getadrs
                        call    #popayx
                        add     dcurr, #4
                        cmps    a, y            wz, wc
        if_nz_and_nc    jmp     #caseran1
                        cmps    a, x            wz, wc
        if_nz_and_nc    jmp     #loop
                        cmps    x, y            wz, wc
        if_nz_and_nc    jmp     #loop
                        add     pcurr, op2
                        jmp     #loop
caseran1                cmps    y, x            wz, wc
        if_nz_and_nc    jmp     #loop
                        cmps    x, a            wz, wc
        if_nz_and_nc    jmp     #loop
                        add     pcurr, op2
                        jmp     #loop

' lookdone
lookdon                 sub     dcurr, #12
                        mov     x, #0
                        jmp     #pushx1

' lookupval
lookupv                 call    #pop_adr
                        call    #popayx
                        add     dcurr, #12
                        cmps    a, x            wz, wc
        if_c            jmp     #lookupv1
        if_z            jmp     #lookupv2
                        add     x, #1
                        sub     dcurr, #12
                        wrlong  x, dcurr
                        add     dcurr, #12
                        jmp     #loop
lookupv1                mov     pcurr, y
                        add     pcurr, pbase
                        sub     dcurr, #12
                        mov     x, #0
                        jmp     #pushx1
lookupv2                mov     pcurr, y
                        add     pcurr, pbase
                        sub     dcurr, #12
                        mov     x, adr
                        jmp     #pushx1
' lookdnval
lookdnv                 call    #popyx
                        add     dcurr, #4
                        cmp     x, y            wz
        if_nz           jmp     #lookdnv1
                        sub     dcurr, #8
                        rdlong  pcurr, dcurr
                        add     pcurr, pbase
                        jmp     #loop
lookdnv1                sub     dcurr, #12
                        rdlong  x, dcurr
                        add     x, #1
                        wrlong  x, dcurr
                        add     dcurr, #12
                        jmp     #loop

' lookuprange
lookupr                 call    #popyx
                        mov     t1, y
                        mov     t2, x
                        call    #popayx
                        add     dcurr, #12
                        mov     adr, t1 
                        sub     adr, t2
                        abs     adr, adr
                        sub     a, x            wc
        if_c            jmp     #lookupr1
                        cmp     a, adr          wc, wz
        if_c_or_z       jmp     #lookupr2
                        add     x, adr
                        add     x, #1
                        sub     dcurr, #12
                        wrlong  x, dcurr
                        add     dcurr, #12
                        jmp     #loop
lookupr1                sub     dcurr, #12
                        mov     pcurr, y
                        add     pcurr, pbase
                        mov     x, #0
                        jmp     #pushx1
lookupr2                sub     dcurr, #12
                        mov     pcurr, y
                        add     pcurr, pbase
                        cmp     t2, t1          wc, wz
                        mov     x, t2
        if_c_or_z       add     x, a
        if_nc_and_nz    sub     x, a
                        jmp     #pushx1

' lookdnrange
lookdnr                 call    #popayx
                        add     dcurr, #4
                        mov     t1, a
                        mov     t2, y
                        max     t1, y                   ' Get minimum
                        min     t2, a                   ' Get maximum
                        cmps    x, t1           wc
        if_c            jmp     #lookdnr1
                        cmps    t2, x           wc
        if_c            jmp     #lookdnr1
                        sub     dcurr, #8
                        rdlong  pcurr, dcurr
                        add     pcurr, pbase
                        call    #pop_adr
                        cmps    y, a            wc
                        sub     x, y
        if_nc           neg     x, x
                        add     x, adr 
                        jmp     #pushx1
lookdnr1                sub     dcurr, #12
                        rdlong  x, dcurr
                        add     x, #1
                        add     x, t2
                        sub     x, t1
                        wrlong  x, dcurr
                        add     dcurr, #12
                        jmp     #loop

_waitcn                 call    #popx1
                        waitcnt x, #0
                        jmp     #loop

_waitpe                 call    #popayx
                        test    a, #1           wz
                        getcnt  t1
			' Port A and B
:loop   if_nz           waitpeq x, y, #1        wc
        if_z            waitpeq x, y, #0        wc
{
			' Port C and D
:loop   if_nz           waitpeq x, y, #3        wc
        if_z            waitpeq x, y, #2        wc
}
        if_c            jmp     #:loop
                        jmp     #loop

_waitpn                 call    #popayx
                        test    a, #1           wz
                        getcnt  t1
:loop   if_z            waitpne x, y, #1        wc
        if_nz           waitpne x, y, #0        wc
        if_c            jmp     #:loop
                        jmp     #loop

cogstp                  call    #popx1
                        cogstop x
                        jmp     #loop

_cogini                 call    #popayx                 'coginit, pop parameters
                        test    x, #8           wc
                        and     x, #7
                        setnib  _cogini1, x, #6
                        test    op,#%100        wz      'push result?
        if_nc           jmp     #_cogini1
                        cognew  y, a            wc
        if_c            neg     x, #1                   '-1 if c, else 0..7
        if_nc           mov     x, y                    '-1 if c, else 0..7
                        jmp     #pushz

callobx                 call    #popx1
                        mov     a, x
                        jmp     #callobj1

_pop                    call    #popx1
                        sub     dcurr, x
                        jmp     #loop

abrtval                 call    #popx1
                        jmp     #_abort1
_abort                  rdlong  x,dbase
_abort1                 call    #return1
        if_nc           jmp     #_abort1
                        jmp     #pushz

rwreg                   mov     t2, #31
                        mov     t1, #0
			jmp	#rwreg0
                        
rwregb                  call    #popx1
                        and     x, #31
                        mov     t2, x
                        mov     t1, x
                        jmp     #rwreg0

rwregbs                 call    #popyx
                        and     x, #31
                        and     y, #31
                        mov     t2, x
                        mov     t1, y
                        jmp     #rwreg0

rwregx                  call    #pop_adr
                        or      adr, #$10
                        shl     op, #5
                        jmp     #rwreg1

'***********************************************************************
' Prepare to start a Spin cog
'***********************************************************************
                        'run(metindex, stackaddr)
_run                    call    #popyx

                        'stackptr := (metindex >> 8) << 2
                        mov     adr, x
                        shr     adr, #8
                        shl     adr, #2		wz

                        'metindex := ((metindex & 255) << 2) + PBASE
                        and     x, #255
                        shl     x, #2
                        add     x, pbase

			'skip bytemove if no parms
	if_z		jmp	#_run1

                        't2 := stackaddr + 12
                        mov     t2, y
                        add     t2, #12

                        't1 := DCURR - stackptr
                        mov     t1, dcurr
                        sub     t1, adr

                        'a  := stackptr
                        mov     a, adr

                        'bytemove(t2, t1, a)
:loop                   rdbyte  op2, t1
                        add     t1, #1
                        wrbyte  op2, t2
                        add     t2, #1
                        djnz    a, @:loop

                        'pop(stackptr)
                        sub     dcurr, adr

                        'long[stackaddr] := -1
_run1                   neg     t1, #1
                        wrlong  t1, y
                        add     y, #4

                        'long[stackaddr][1] := (@exitcode << 16)
			mov	t1, ##@exitcode
			shl	t1, #16
                        wrlong  t1, y
                        add     y, #4

                        'word[stackaddr][2] := 0
                        mov     t1, #0
                        wrlong  t1, y

                        'stackptr += stackaddr + 12
                        add     adr, y
                        add     adr, #4

                        'word[stackptr][1] := pbase
                        add     adr, #2
                        wrword  pbase, adr

                        'word[stackptr][2] := vbase
                        add     adr, #2
                        wrword  vbase, adr

                        'word[stackptr][3] := stackaddr + 8
                        add     adr, #2
                        wrword  y, adr

                        'word[stackptr][4] := word[metindex] + PBASE
                        add     adr, #2
                        rdword  t1, x
                        add     t1, pbase
                        wrword  t1, adr

                        'word[stackptr][5] := word[metindex][1] + stackptr
                        add     adr, #2
                        add     x, #2
                        rdword  t1, x
                        add     t1, adr
                        sub     t1, #10
                        wrword  t1, adr
                        sub     adr, #10

                        '(-1, @interp, stackptr)
                        neg     t1, #1
                        wrlong  t1, dcurr
                        add     dcurr, #4
			mov	t1, ##@interp
                        wrlong  t1, dcurr
                        add     dcurr, #4
                        wrlong  adr, dcurr
                        add     dcurr, #4

                        jmp     #loop

'***********************************************************************
' String, Block Move and Block Fill Instructions
'***********************************************************************
'                       strsize
_strsiz                 call    #popx1
                        mov     y, x
:loop                   rdbyte  a, x            wz
        if_nz           add     x, #1
        if_nz           jmp     #:loop
                        sub     x, y
                        jmp     #pushx1

'                       strcomp
_strcmp                 call    #popyx
:loop			rdbyte  t1, x		wz
			add	x, #1
	if_z		jmp	#_strcmp1
			rdbyte	t2, y		wz
			add	y, #1
	if_z		jmp	#_strcmp2
			cmp	t1, t2		wz
	if_z		jmp	#:loop
_strcmp2		mov	x, #0
			jmp	#pushx1
_strcmp1		rdbyte	t2, y		wz
			jmp	#_notlx

'                       longfill
_longfl                 call    #popayx
:loop                   wrlong  y, x
                        add     x, #4
                        djnz    a, @:loop
                        jmp     #loop

'                       wordfill
_wordfl                 call    #popayx
:loop                   wrword  y, x
                        add     x, #2
                        djnz    a, @:loop
                        jmp     #loop

'                       bytefill
_bytefl                 call    #popayx
:loop                   wrbyte  y, x
                        add     x, #1
                        djnz    a, @:loop
                        jmp     #loop

'                       longmove
_longmv                 call    #popayx
:loop                   rdlong  t1, y
                        add     y, #4
                        wrlong  t1, x
                        add     x, #4
                        djnz    a, @:loop
                        jmp     #loop

'                       wordmove
_wordmv                 call    #popayx
:loop                   rdword  t1, y
                        add     y, #2
                        wrword  t1, x
                        add     x, #2
                        djnz    a, @:loop
                        jmp     #loop

'                       bytemove
_bytemv                 call    #popayx
:loop                   rdbyte  t1, y
                        add     y, #1
                        wrbyte  t1, x
                        add     x, #1
                        djnz    a, @:loop
                        jmp     #loop

'***********************************************************************
' Lock instructions
'***********************************************************************
lcknewr                 locknew x               wc
        if_c            neg     x, #1
                        jmp     #pushx1

lcksetr                 call    #popx1
                        lockset x               wc
        if_c            neg     x, #1
        if_nc           mov     x, #0
                        jmp     #pushx1

lckclrr                 call    #popx1
                        lockclr x               wc
        if_c            neg     x, #1
        if_nc           mov     x, #0
                        jmp     #pushx1

lcknew                  locknew x
                        jmp     #loop

lckset                  call    #popx1
                        lockset x
                        jmp     #loop

lckclr                  call    #popx1
                        lockclr x
                        jmp     #loop

lckret                  call    #popx1
                        lockret x
                        jmp     #loop

'***********************************************************************
' Local Variable Instructions
'***********************************************************************

'                       Load Long Var
ldll                    call    #getadrz
                        mov     adr, dbase
                        jmp     #ldlong

'                       Store Long Var
stll                    call    #getadrz
                        mov     adr, dbase
                        jmp     #stlong

'                       Execute Long Var
exll                    call    #getadrz
                        mov     adr, dbase
                        jmp     #exoplong

'                       Load Address Var
la_l                    call    #getadrz
                        mov     x, op2
                        add     x, dbase
                        jmp     #pushx1

'                       Load Word Var
ldwl                    call    #getadrz
                        mov     adr, dbase
                        jmp     #ldword

'                       Store Word Var
stwl                    call    #getadrz
                        mov     adr, dbase
                        jmp     #stword

'                       Execute Word Var
exwl                    call    #getadrz
                        mov     adr, dbase
                        jmp     #exopword

'                       Load Byte Var
ldbl                    call    #getadrz
                        mov     adr, dbase
                        jmp     #ldbyte

'                       Store Byte Var
stbl                    call    #getadrz
                        mov     adr, dbase
                        jmp     #stbyte

'                       Execute Byte Var
exbl                    call    #getadrz
                        mov     adr, dbase
                        jmp     #exopbyte

'***********************************************************************
' Compact VAR Variable Instructions
'***********************************************************************
'                       Load Long Local Compact
ldlvc                   mov     op2, vbase
                        mov     adr, op
                        and     adr, #$1c
                        jmp     #ldlong

'                       Store Long Local Compact
stlvc                   mov     adr, op
                        and     adr, #$1c
                        mov     op2, vbase
                        jmp     #stlong

'                       Execute Long Local Compact
exlvc                   mov     adr, op
                        and     adr, #$1c
                        mov     op2, vbase
                        jmp     #exoplong

'                       Load Address Long Local Compact
lalvc                   and     op, #$1c
                        add     op, vbase
                        mov     x, op
                        jmp     #pushx1

'***********************************************************************
' VAR Variable Instructions
'***********************************************************************

'                       Load Long Var
ldlv                    call    #getadrz
                        mov     adr, vbase
                        jmp     #ldlong

'                       Store Long Var
stlv                    call    #getadrz
                        mov     adr, vbase
                        jmp     #stlong

'                       Load Word Var
ldwv                    call    #getadrz
                        mov     adr, vbase
                        jmp     #ldword

'                       Store Word Var
stwv                    call    #getadrz
                        mov     adr, vbase
                        jmp     #stword

'                       Load Byte Var
ldbv                    call    #getadrz
                        mov     adr, vbase
                        jmp     #ldbyte

'                       Store Byte Var
stbv                    call    #getadrz
                        mov     adr, vbase
                        jmp     #stbyte

'                       Execute Long Var
exlv                    call    #getadrz
                        mov     adr, vbase
                        jmp     #exoplong

'                       Execute Word Var
exwv                    call    #getadrz
                        mov     adr, vbase
                        jmp     #exopword

'                       Execute Byte Var
exbv                    call    #getadrz
                        mov     adr, vbase
                        jmp     #exopbyte

'                       Load Address Var
la_v                    call    #getadrz
                        mov     x, op2
                        add     x, vbase
                        jmp     #pushx1

'***********************************************************************
' Indexed Local Variable Instructions
'***********************************************************************

'                       Load Long Local with Index
ldllx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #2
                        add     adr, dbase
                        jmp     #ldlong

'                       Store Long Local with Index
stllx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #2
                        add     adr, dbase
                        jmp     #stlong

'                       Execute Long Local with Index
exllx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #2
                        add     adr, dbase
                        jmp     #exoplong

'                       Load Address Long Local with Index
lallx                   call    #getadrz
                        call    #popx1
                        shl     x, #2
                        add     x, op2
                        add     x, dbase
                        jmp     #pushx1

'                       Load Word Local with Index
ldwlx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #1
                        add     adr, dbase
                        jmp     #ldword

'                       Store Word Local with Index
stwlx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #1
                        add     adr, dbase
                        jmp     #stword

'                       Execute Word Local with Index
exwlx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #1
                        add     adr, dbase
                        jmp     #exopword

'                       Load Address Word Local with Index
lawlx                   call    #getadrz
                        call    #popx1
                        shl     x, #1
                        add     x, op2
                        add     x, dbase
                        jmp     #pushx1

'                       Load Byte Local with Index
ldblx                   call    #getadrz
                        call    #pop_adr
                        add     adr, dbase
                        jmp     #ldbyte

'                       Store Byte Local with Index
stblx                   call    #getadrz
                        call    #pop_adr
                        add     adr, dbase
                        jmp     #stbyte

'                       Execute Byte Local with Index
exblx                   call    #getadrz
                        call    #pop_adr
                        add     adr, dbase
                        jmp     #exopbyte

'                       Load Address Byte Local with Index
lablx                   call    #getadrz
                        call    #popx1
                        add     x, op2
                        add     x, dbase
                        jmp     #pushx1

'***********************************************************************
' Indexed VAR Variable Instructions
'***********************************************************************

'                       Load Long VAR with Index
ldlvx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #2
                        add     adr, vbase
                        jmp     #ldlong

'                       Store Long VAR with Index
stlvx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #2
                        add     adr, vbase
                        jmp     #stlong

'                       Execute Long VAR with Index
exlvx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #2
                        add     adr, vbase
                        jmp     #exoplong

'                       Load Address Long VAR with Index
lalvx                   call    #getadrz
                        call    #popx1
                        shl     x, #2
                        add     x, op2
                        add     x, vbase
                        jmp     #pushx1

'                       Load Word VAR with Index
ldwvx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #1
                        add     adr, vbase
                        jmp     #ldword

'                       Store Word VAR with Index
stwvx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #1
                        add     adr, vbase
                        jmp     #stword

'                       Execute Word VAR with Index
exwvx                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #1
                        add     adr, vbase
                        jmp     #exopword

'                       Load Address Word VAR with Index
lawvx                   call    #getadrz
                        call    #popx1
                        shl     x, #1
                        add     x, op2
                        add     x, vbase
                        jmp     #pushx1

'                       Load Byte VAR with Index
ldbvx                   call    #getadrz
                        call    #pop_adr
                        add     adr, vbase
                        jmp     #ldbyte

'                       Store Byte VAR with Index
stbvx                   call    #getadrz
                        call    #pop_adr
                        add     adr, vbase
                        jmp     #stbyte

'                       Execute Byte VAR with Index
exbvx                   call    #getadrz
                        call    #pop_adr
                        add     adr, vbase
                        jmp     #exopbyte

'                       Load Address Byte VAR with Index
labvx                   call    #getadrz
                        call    #popx1
                        add     x, op2
                        add     x, vbase
                        jmp     #pushx1

'***********************************************************************
' DAT Variable Instructions
'***********************************************************************

'                       Load Word Object
ldwo                    call    #getadrz
                        mov     adr, pbase
                        jmp     #ldword

'                       Store Word Object
stwo                    call    #getadrz
                        mov     adr, pbase
                        jmp     #stword

'                       Execute Word Object
exwo                    call    #getadrz
                        mov     adr, pbase
                        jmp     #exopword

'                       Load Byte Object
ldbo                    call    #getadrz
                        mov     adr, pbase
                        jmp     #ldbyte

'                       Store Byte Object
stbo                    call    #getadrz
                        mov     adr, pbase
                        jmp     #stbyte

'                       Execute Byte Object
exbo                    call    #getadrz
                        mov     adr, pbase
                        jmp     #exopbyte

'***********************************************************************
' Indexed DAT Variable Instructions
'***********************************************************************

'                       Load Long Object with Index
ldlox                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #2
                        add     adr, pbase
                        jmp     #ldlong

'                       Store Long Object with Index
stlox                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #2
                        add     adr, pbase
                        jmp     #stlong

'                       Execute Long Object with Index
exlox                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #2
                        add     adr, pbase
                        jmp     #exoplong

'                       Load Address Long Object with Index
lalox                   call    #getadrz
                        call    #popx1
                        shl     x, #2
                        add     x, op2
                        add     x, pbase
                        jmp     #pushx1

'                       Load Word Object with Index
ldwox                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #1
                        add     adr, pbase
                        jmp     #ldword

'                       Store Word Object with Index
stwox                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #1
                        add     adr, pbase
                        jmp     #stword

'                       Execute Word Object with Index
exwox                   call    #getadrz
                        call    #pop_adr
                        shl     adr, #1
                        add     adr, pbase
                        jmp     #exopword

'                       Load Address Word Object with Index
lawox                   call    #getadrz
                        call    #popx1
                        shl     x, #1
                        add     x, op2
                        add     x, pbase
                        jmp     #pushx1

'                       Execute Byte Object with Index
exbox                   call    #getadrz
                        call    #pop_adr
                        add     adr, pbase
                        jmp     #exopbyte

'***********************************************************************
' Absolute Address Instructions
'***********************************************************************

'                       Store Byte Absolute
stba                    call    #popyx
                        jmp     #stba1

'                       Store Word Absolute
stwa                    call    #popyx
                        jmp     #stwa1

'                       Store Long Absolute
stla                    call    #popyx
                        jmp     #stla1

'                       Execute Long Absolute
exla                    sub     dcurr, #4
                        rdlong  adr, dcurr
                        jmp     #exoplong1

'                       Execute Word Absolute
exwa                    sub     dcurr, #4
                        rdlong  adr, dcurr
                        jmp     #exopword1

'                       Execute Byte Absolute
exba                    sub     dcurr, #4
                        rdlong  adr, dcurr
                        jmp     #exopbyte1

'                       Load Address Absolute (NOP)
la_a                    jmp     #loop

'***********************************************************************
' Absolute Address with Index Instructions
'***********************************************************************
'                       Load Byte Absolute with Index
'                       Store Byte Absolute with Index
stbax                   call    #popayx
                        add     y, a
stba1                   wrbyte  x, y
                        jmp     #loop

'                       Store Word Absolute with Index
stwax                   call    #popayx
                        shl     a, #1
                        add     y, a
stwa1                   wrword  x, y
                        jmp     #loop

'                       Store Long Absolute with Index
stlax                   call    #popayx
                        shl     a, #2
                        add     y, a
stla1                   wrlong  x, y
                        jmp     #loop

'                       Execute Long Absolute with Index
exlax                   call    #popay
                        shl     a, #2
                        mov     adr, y
                        add     adr, a
                        jmp     #exoplong1

'                       Load Address Long Absolute with Index
lalax                   call    #popyx
                        shl     y, #2
                        add     x, y
                        jmp     #pushx1

'                       Execute Word Absolute with Index
exwax                   call    #popay
                        shl     a, #1
                        mov     adr, y
                        add     adr, a
                        jmp     #exopword1

'                       Load Address Word Absolute with Index
lawax                   call    #popyx
                        shl     y, #1
                        add     x, y
                        jmp     #pushx1

'                       Execute Word Absolute with Index
exbax                   call    #popay
                        mov     adr, y
                        add     adr, a
                        jmp     #exopbyte1

'                       Load Address Byte Absolute with Index
labax                   call    #popyx
                        add     x, y
                        jmp     #pushx1

'***********************************************************************
' Extended Instructions
'***********************************************************************

sexb                    shl     x, #24
                        sar     x, #24
                        jmp     savex

sexw                    shl     x, #16
                        sar     x, #16
                        jmp     savex

preinc                  add     x, #1
                        jmp     savex

predec                  sub     x, #1
                        jmp     savex

'                       Repeat-from-to-step Instruction
repeats                 call    #popay
                        sub     dcurr, #4
                        rdlong  t1, dcurr
                        jmp     #repeaty

'                       Repeat-from-to Instruction
repeatx
                        call    #popay                  'pop data (a=to, y=from, t1=step)
                        mov     t1, #1
repeaty                 call    #getadrs
                        cmps    a,y             wc      'reverse range?
                        sumc    x,t1                    'add/sub step to/from var
                        call    #range                  'check if x in range y..a according to c
        if_nc           add     pcurr,op2               'if in range, branch
                        mov     op2, #0                 'ensure we don't push to the stack
                        jmp     savex

' Check range
' must be preceded by:  cmps    a,y             wc
'
range   if_c            xor     a,y                     'if reverse range, swap range values
        if_c            xor     y,a
        if_c            xor     a,y
                        cmps    x,y             wc      'c=0 if x within range
        if_nc           cmps    a,x             wc
                        ret

randf                   neg     t1, #1          wz
                        jmp     #rand1
randr                   mov     t1, #0		wz
rand1                   min     x,#1
                        mov     y,#32
                        mov     a,#%10111
        if_z            ror     a,#1
rand2                   test    x,a             wc
        if_nz           rcr     x,#1
        if_z            rcl     x,#1
                        djnz    y,@rand2
                        jmp     savex

_sarx                   call    #popy1
                        sar     x, y
                        jmp     savex

_mulx                   call    #popy1
                        mul32   x, y
                        getmull x
                        jmp     savex

_mulhx                  call    #popy1
                        mul32   x, y
                        getmulh x
                        jmp     savex

_addx                   call    #popy1
                        add     x, y
                        jmp     savex

_subx                   call    #popy1
                        sub     x, y
                        jmp     savex

_xorx                   call    #popy1
                        xor     x, y
                        jmp     savex

'***********************************************************************
' Math Instructions
'***********************************************************************
_min                    call    #popyx
                        min     x, y
                        jmp     savex

_max                    call    #popyx
                        max     x, y
                        jmp     savex

_rev                    call    #popyx
                        rev     x, y
                        jmp     savex

decode                  call    #popy1
                        mov     x, #1
                        shl     x, y
                        jmp     savex

encode                  call    #popy1
                        mov     x, #1
encode1 if_z            jmp     savex
                        add     x, #1
                        shr     y, #1           wz
                        jmp     #encode1

_sqrt                   call    #popx1
_sqrtx                  sqrt32  x
                        getsqrt x
                        jmp     savex

'***********************************************************************
' Extended Math Instructions
'***********************************************************************
_andlx                  cmp     x, #0 wz
                        jmp     #_andl+1

_orlx                   cmp     x, #0 wz
                        jmp     #_orl+1

_cmpltx                 call    #popy1
                        jmp     #_cmplt+1

_cmpgtx                 call    #popy1
                        jmp     #_cmpgt+1

_cmpnex                 call    #popy1
                        jmp     #_cmpne+1

_cmpeqx                 call    #popy1
                        jmp     #_cmpeq+1

_cmplex                 call    #popy1
                        jmp     #_cmple+1

_cmpgex                 call    #popy1
                        jmp     #_cmpge+1

_minx                   call    #popy1
                        min     x, y
                        jmp     savex

_maxx                   call    #popy1
                        max     x, y
                        jmp     savex

_revx                   call    #popy1
                        rev     x, y
                        jmp     savex

decodex                 mov     y, x
                        jmp     #decode+1

encodex                 mov     y, x
                        jmp     #encode+1

'***********************************************************************
' This code is used when absolute address 0 is accessed
'***********************************************************************
ldclkfreq		mov	x, ##_clkfreq
			jmp	#pushx1

'***********************************************************************
' Trap for unsupported instructions
'***********************************************************************
_waitvi
_clkset
unsupp			mov	op2, #0
unsupx			mov	t1, ##@str1
			call	#putstr
			mov	t1, op
			call	#puthex
			mov	x, #" "
			call	#putch
			mov	t1, op2
			call	#puthex
			mov	x, #13
			call	#putch
                        cogid   x
                        cogstop x

puthex			rol	t1, #28
			call	#puthexdigit
			rol	t1, #4
			call	#puthexdigit
			ret

puthexdigit		mov	x, #15
			and	x, t1
			mov	y, ##@hexdigits
			add	y, x
			rdbyte	x, y
			call	#putch
			ret

putstr			rdbyte	x, t1 		wz
	if_z		ret
			add	t1, #1
			call	#putch
			jmp	#putstr

putch                   or      x, #$100
                        shl     x, #1
                        mov     y, #10
                        getcnt  a
                        add     a, bitcycles
:loop                   ror     x, #1               wc
                        'setpc   #30
                        setpc   #90
                        waitcnt a, bitcycles
                        djnz    y, @:loop
                        ret

str1			byte	"Unsupported opcode ", 0
hexdigits		byte	"0123456789ABCDEF"

'***********************************************************************
' Spin program binary file
'***********************************************************************
testprog                file "test.binary"

{{
+-----------------------------------------------------------------------------+
|                       TERMS OF USE: MIT License                             |
+-----------------------------------------------------------------------------+
|Permission is hereby granted, free of charge, to any person obtaining a copy |
|of this software and associated documentation files (the "Software"), to deal|
|in the Software without restriction, including without limitation the rights |
|to use, copy, modify, merge, publish, distribute, sublicense, and/or sell    |
|copies of the Software, and to permit persons to whom the Software is        |
|furnished to do so, subject to the following conditions:                     |
|                                                                             |
|The above copyright notice and this permission notice shall be included in   |
|all copies or substantial portions of the Software.                          |
|                                                                             |
|THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR   |
|IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,     |
|FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE  |
|AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER       |
|LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,|
|OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE|
|SOFTWARE.                                                                    |
+-----------------------------------------------------------------------------+
}}
