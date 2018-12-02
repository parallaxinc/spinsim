Spinsim 0.99

This version of spinsim supports most of the opcodes in the P2 v32b instruction
set.  The opcodes that are not supported are as follows:
```
xzero   xinit   xcont   hubset  setdacs setxfrq getxacc getbrk
cogbrk  brk     setcy   setci   setcq   setcfrq setcmod getrnd
xoro32  skip    skipf   execf 
```
skip, skipf and execf have been partially implemented, but do not handle jumps
or interrupts correctly.

Spinsim runs in the P1 mode by default.  It can be set to run in the P2 mode
with the -t parameter.  In the P2 mode, spinsim will load a P2 binary file into
hub RAM, and start cog 0 with the code located at $000.  A simulated serial
port is supported by using the -b option.  The default baud rate is 115200, but
other rates can be used by specifying it with -b, such as -b9600.  The serial
port uses pins 63 and 62 when in the P2 mode.

Spinsim is built under Linux, MinGW or Cygwin by using the Makefile, and typing
make.  The Windows executable, spinsim.exe is included with this distribution.

The sub-directory verify contains five programs that have been used to test
spinsim against the FPGA implementation.  Approximately 150 instructions have
been verified to match the hardware.  The verify directory contains the
original C source code and the output from running the test programs on the
FPGA.

A test program can be run by going into the verify directory and typing
```
../spinsim -t -b testopsa.bin
```
The output can be redirected to a file and compared with the hardware file
to verify that spinsim matches the hardware.

Spinsim supports the cordic instructions, but implements them with C functions
instead of simulating the cordic hardware.  The instructions xvector and
xrotate are functionally equivalent to the P2, but will produce slightly
different results.  qdiv and qfract bit exact results as long as the quotient
fits within 32 bits.  It produces different results if the quotient overflows
a 32-bit value.  The I/O streamer is currently not supported.

Spinsim contains a simple debugger, which is enabled with the -d command-line
option.  The debugger prints the prompt "DEBUG>" to indicate that it is ready
to accept a command.  The "help" command will print the following:

Debug Commands
```
help           - Print command list
exit           - Exit spinsim
step           - Run one cycle
stepx          - Run next executed instruction
run            - Run continuously
verbose #      - Set verbosity level
reboot         - Reboot the Prop
setbr cog addr - Set breakpoint for cog to addr
state cog      - Dump cog state
peekc cog addr - Print out a cog memory location
peekh addr     - Print out a hub memory location
```

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
```
0 - Disable printing
1 - Print only the executed instructions
2 - Print only the executed instructions, and show the execution results
3 - Print executed and instruction not executed due to condition code
4 - Also print instructions invalidated in the pipeline due to jumps
5 - Also print instructions that include hub and hardware waits
6 - Also print instructions that include icache waits
7 - Also print instructions waiting for a pin state
8 - Print all cycles, including waitcnt waits
```
The verbosity level is entered as a hexadecimal number.  If the verbosity level
is entered as a single digit it will apply to all cogs.  If more than one digit
is entered each digit will be used for each cog, starting with cog 0 for the
right-most digit.  As an example, a value of 456 will cause 6 to be used for
cog 0, 5 for cog 1, and 4 for cog 2.  All other cogs will use 0.
