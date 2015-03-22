Spinsim 0.75

This version of spinsim supports about two-thirds of the opcodes in the P2
instruction set.  The list of implemented opcodes is shown below, along with
the list of opcodes that have not been implemented yet.  Spinsim runs in the P1
mode by default.  It can be set to run in the P2 mode with the -t parameter.

In the P2 mode, spinsim will load a P2 OBJ file into hub RAM, and start cog 0
with the code located at $E00.  A simulated serial port is supported by using
the -b option.  The default baud rate is 115,200, but other rates can be used
by specifying it with -b, such as -b9600.  The serial port uses pin 31 and 30,
just like with P1.

Spinsim is built using the Makefile, and typing make.  I have successfully
built and run it under Cygwin and Ubuntu Linux.

The sample program, pfth.spin can be run by building it with the PNut P2
assembler.  PNut is contained in the Terasic_Prop2_Emulation zip file,
which can be downloaded from the first post in the "HUB EXEC Update Here"
thread in the Propeller 2 forum.

There are two demo programs, which are the pfth Forth interpreter and the
p1spin Spin interpreter that runs P1 Spin binaries on the P2 processor.
The pfth program is run under spinsim as follows:

./spinsim -t -b pfth.obj

p1spin runs at a baud rate of 57600, so it is run as follows:

./spinsim -t -b57600 p1spin.obj

Spinsim supports execution from cog memory and hub execution, but it does not
support multi-tasking.  Only the core processor is supported, and none of the
counters, video hardware or cordic hardware is simulated.  Support for multi-
tasking, peripheral hardware and cordic instructions will be added later.

Spinsim contains a simple debugger, which is enabled with the -d command-line
option.  The debugger prints the prompt "DEBUG>" to indicate that it is ready
to accept a command.  The "help" command will print the following:

Debug Commands
help           - Print command list
exit           - Exit spinsim
step           - Run one cycle
stepx          - Run next executed instruction
run            - Run continuously
verbose #      - Set verbosity level
reboot         - Reboot the Prop
setbr cog addr - Set breakpoint for cog to addr
state cog      - Dump cog state

The "step" command will run one cycle, and the "stepx" command will run any
non-executing cycles until it encounters an instruction that is executed.
The previous command can be executed again by just pushing the enter key.
This is useful for stepping where the "step" command is typed once, and
the enter key can then be used to step again.

The "run" command will run until a breakpoint is encountered or ^] is typed.
^] is typed by holding down the control key and pressing the "]" key.
While running, spinsim will print out the results of each cycle as
controlled by the verbosity level, which is set by the "verbose" command.
The verbosity level can also be set with the command-line parameter "-v#".

The verbosity levels are as follows:

0 - Disable printing
1 - Print only the executed instructions
2 - Print only the executed instructions, and show the execution results
3 - Print executed and instruction not executed due to condition code
4 - Also print instructions invalidated in the pipeline due to jumps
5 - Also print instructions that include hub and hardware waits
6 - Also print instructions that include icache waits
7 - Also print instructions waiting for a pin state
8 - Print all cycles, including waitcnt waits

The verbosity level is entered as a hexadecimal number.  If the verbosity level
is entered as a single digit it will apply to all cogs.  If more than one digit
is entered each digit will be used for each cog, starting with cog 0 for the
right-most digit.  As an example, a value of 456 will cause 6 to be used for
cog 0, 5 for cog 1, and 4 for cog 2.  All other cogs will use 0.


Implemented Opcodes
-------------------
abs     add     addabs  addptra addptrb addptrx addptry adds
addsx   addx    and     andn    augd    augs    bingry  blmask
call    calla   callad  callb   callbd  calld   callx   callxd
cally   callyd  chkptrx chkptry clkset  clracca clraccb clraccs
clrb    clrp    cmp     cmpcnt  cmps    cmpsub  cmpsx   cmpx
cogid   coginit cognew  cogstop dcachex decd    decds   decmod
decod2  decod3  decod4  decod5  div32   div32u  div64   div64d
div64u  djnz    djnzd   djz     djzd    encod   fixinda fixindb
fixindx frac    getacah getacal getacbh getacbl getbyte getcnt
getdivq getdivr getmulh getmull getnib  getnp   getp    getptra
getptrb getptrx getptry getsqrt getword icachen icachep icachex
ijnz    ijnzd   ijz     ijzd    incd    incds   incmod  isob
jmp     jmpd    jmpsw   jmpswd  jnp     jnpd    jnz     jnzd
jp      jpd     jz      jzd     locbase locbyte lockclr locknew
lockret lockset loclong locptra locptrb locword maca    macb
max     maxs    mergew  min     mins    mov     mul     mul32
mul32u  muxc    muxnc   muxnz   muxz    neg     negc    negnc
negnz   negz    not     notb    notp    offp    onecnt  or
pop     popzc   push    pushzc  rcl     rcr     rdaux   rdauxr
rdbyte  rdbytec rdlong  rdlongc rdwide  rdwidec rdword  rdwordc
repd    reps    ret     reta    retad   retb    retbd   retd
retx    retxd   rety    retyd   rev     rol     ror     sar
saracca saraccb saraccs setacca setaccb setb    setbc   setbnc
setbnz  setbyte setbz   setd    seti    setindb setindx setnib
setp    setpc   setpnc  setpnz  setptra setptrb setptrx setptry
setpz   sets    setwide setwidz setword setzc   seussf  seussr
shl     shr     splitw  sqrt32  sqrt64  sub     subabs  subcnt
subptra subptrb subptrx subptry subr    subs    subsx   subx
sumc    sumnc   sumnz   sumz    test    testn   wait    waitcnt
waitpeq waitpne wraux   wrauxr  wrbyte  wrlong  wrwide  wrword
xor     zercnt


Opcodes Not Implemented
-----------------------
addphsa addphsb addphss addpix  bcdbin  binbcd  blnpix  capctra
capctrb capctrs cfgdac0 cfgdac1 cfgdac2 cfgdac3 cfgdacs cfgpins
cmpr    decpat  eswap4  eswap8  getcntx getcosa getcosb getlfsr
getphsa getphsb getphza getphzb getpix  getqx   getqy   getqz
getsina getsinb grybin  incpat  jmplist jmptask locinst mergeb
mixpix  movbyts mulpix  packrgb passcnt polctra polctrb polvid
qartan  qexp    qlog    qrotate qsincos rolbyte rolnib  rolword
scl     serina  serinb  serouta seroutb setbyts setctra setctrb
setctrs setdac0 setdac1 setdac2 setdac3 setdacs setfrqa setfrqb
setfrqs setmap  setmix  setphsa setphsb setphss setpix  setpixa
setpixb setpixg setpixr setpixu setpixv setpixw setpixz setpora
setporb setporc setpord setqi   setqz   setrace setsera setserb
settask setvid  setvidi setvidq setvidy setwava setwavb setwavs
setwrds setx    setxch  setxft  splitb  subphsa subphsb subphss
synctra synctrb taskid  testb   unpkrgb waitpf  waitpr  waitpx
waitvid 
