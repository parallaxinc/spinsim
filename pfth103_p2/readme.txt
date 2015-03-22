                              pfth Version 1.03
                               November 8, 2013
                                  Dave Hein
                        (with additions from G. Herzog)

INTRODUCTION
------------

pfth is an ANS Forth interpreter that runs on the Propeller.  It is written in
PASM and Forth, and it can be built using the Prop Tool or BST.  pfth
implements all 133 of the ANS Forth core words, and 38 of the 45 core ext
words. pfth will run on any Propeller board that supports the standard serial
interface.  The default settings of the serial port are 115200 baud, 8 bits, no
parity and 1 stop bit.

After loading and communications is established, use 'words' to verify that the
display is correct.  The CR word is defined to emit both a carriage return and
a line feed.  If your terminal displays an extra line you can either disable
the linefeed character on your terminal, or re-define CR to only emit a
carriage return.


VERSIONS OF SPIN FILES
----------------------

There are four versions of pfth.

The first version can be used to build stand-alone Forth applications.  It is
in pfth.spin.  In this version, Forth programs are included using the Spin FILE
directive.

A second version interfaces to an SD card and can execute Forth programs on
the SD card.  This version is in sdpfth.spin. The program is set up for the C3
card, but it can be modified to support other cards.

The third version is named ospfth.spin, and runs under the Spinix operating
system.  When Spinix starts up pfth it provides information about the SD pins,
the current working directory and a parameter list.  The OS version of pfth
uses this information to initialize the SD card driver and change to the
working directory.  It includes the file given by the parameter list.

The final version is called chess.spin, and implements a chess program.  After
the program is loaded type "chess" to start playing.  A move is entered by
typing the source and destination postions separated by a "-".  As an example,
the move "D2-D4" will move the white queen's pawn two spaces ahead.


LEXICON
-------

pfth has over 50 kernel words, with most of them from the ANS core word set.
The source code for pfth is contained in pfth.spin, init.fth and other Forth
programs included at the end of pfth.spin.

Two of the non-standard words implemented by pfth are cog@ and cog!. These are
are used to read and write cog memory locations.  Their main purpose is to
access the Prop registers, such as CNT, INA and OUTA.

Another non-standard word is cogx1, which is used to execute native Prop
instructions.  The TOS contains the Prop instruction, and the next value on the
stack is used for the destination register value.  The result of the execution
of the instruction is returned on the stack.

Some other non-standard words are _lit, _gethex, _jz and .x. 
 
_lit is used to encode literal values. 
_gethex is used during the boot phase to convert numeric strings to hex values.
_jz implements a jump-on-zero primitive.  
.x is used for debug purposes, and it prints values as 8-digit hex numbers.

Some of the kernel words are shown below.

Interpreter Words          Math and Logical Words     Memory Access
-----------------          ----------------------     -------------
evaluate                   +                          !
execute                    -                          @
find                       *                          c!
word                       /                          c@
refill                     mod 
create                     and                        Console I/O
:                          or                         -----------
;                          xor                        emit
                           <                          key
                           =                          accept
Program Termination        >
-------------------        lshift                     Primitive Words
abort                      rshift                     ---------------
exit                                                  _lit
quit                       Variables                  _gethex
                           ---------                  _jz
Stack Operations           #tib                       .x
----------------           tib
drop                       >in                        Propeller Words
pick                       base                       ---------------
roll                       dp                         cog!
>r                         last                       cog@
r>                         state                      cogx1
depth
swap
dup


pfth also contains a small number of pre-compiled Forth words written
in Forth.  These words are here, allot, ",", _jmp and count.  The definition
of these words is as follows.

: here dp @ ;
: allot dp @ + dp ! ;
: , here ! 4 allot ;
: _jmp r> @ >r ;
: count 0 pick 1 + 1 roll c@ ;


AT START UP
-----------

When pfth starts up, it runs a small boot interpreter that compiles
the ANS dictionary contained in init.fth.  This file is included in the
PASM binary image using the PASM FILE directive.  Other Forth source files
may be included after init.fth to add additional words and to implement a
specific application.

The boot interpreter can only handle 16-bit hex numbers, so pfth is in the hex
mode when first starting up.  The boot interpreter has no error handling
capability, and is only used to build the initial dictionary.  The boot
interpreter uses a limited vocabulary consisting of the following 20
kernel words.

     dp state _lit _gethex : ; c@ @ ! + = pick roll drop
     r> >r word find execute refill

The boot interpreter is shown below in a pseudo-Forth language.  The
labels (label1), (label2), etc. are actually encoded as PASM labels, but
they are shown symbolically in the code below.

: xboot
(label1)
 20 word 0 pick c@ _jz (label2)  ( Get word, refill if empty )
 find 0 pick       _jz (label3)  ( Find word, get number if not found )
 state @ =         _jz (label4)  ( Go execute if not compile mode or immediate )
 ,                 _jmp (label1) ( Otherwise, compile and loop again )
(label4)
 execute           _jmp (label1) ( Execute and loop again )
(label3)
 drop count _gethex              ( Get number )
 state @           _jz (label1)  ( Loop again if not compile mode )
 _lit _lit , ,     _jmp (label1) ( Otherwise, compile number and loop again )
(label2)
 drop refill       _jmp (label1) ( Refill and loop again )
;

The boot interpreter compiles init.fth, which then runs the main interpreter.
The main interpreter performs some error handling by checking for undefined
words and stack underflows.  It also handles negative numbers and numbers in
any base.


SOURCE PROGRAMS
---------------

There are a number of additional Forth source programs included in this
distribution that can be run by pfth.  Open and read these as text files to
learn more details.

comus.fth    - provides a several useful non-standard words that are commonly
               used.  

starting.fth - contains samples of code from the "Starting Forth" tutorial.

rc4time.fth  - implements the RC4 code included in the Wikipedia Forth entry.
               It also displays the time required to run the RC4 code.

i2c.fth      - is a Forth implementation of Mike Green's basic_i2c_driver from
               the OBEX.

fds.fth      - implementes some of the functions from the FullDuplexSerial
               driver.  It uses the binary PASM code from FullDuplexSerial.

toggle.fth   - will start up a cog running the Forth interpreter.  It toggles
               P15, but it can easily be modified to toggle another pin.

ted.fth      - a simple line-oriented text editor based on the ED text editor.

linux.fth    - implements some basic linux commands, such as ls, cat, rm and cp.


STARTING FORTH COGS
-------------------

Forth cogs are started with cognew or coginit by specifying the Forth cog image
as the execution pointer and a 5-long structure as the data pointer.  The
5-long structure is defined as follow

First long:  Address of the body of a word that will be executed on startup
Second long: Initial value of the stack pointer
Third long:  Address of the beginning of the stack
Fourth long: Initial value of the return stack pointer
Fifth long:  Address of the beginning the the return stack


CELL SIZE
---------

The cell size for pfth is 32 bits.  The words !, @ and "," words access 32-bit
values that are 32-bit aligned.  Additional words are provide for smaller unit
access, such as w! and w@ for word access, and c! and c@ for byte access.

The compiled list cell size is 16 bits.  The compile, word must be used when
compiling execution tokens into a list rather than just using the "," word.


DICTIONARY ENTRY FORMAT
-----------------------

The format for the pfth dictionary entry is shown below.  The beginning of a
word entry and its body are long-aligned.  The link pointer contains the
address of the previous word in the dictionary.

The flags byte contains various flag bits that indicate if the word is an
immediate word, a literal number or string, or a jump word.  The name length
byte and the name string specify the name of the word.  It is followed by
padding bytes that ensure long alignment.

The code pointer contains the cog address of the PASM code that is to be
executed.  The execution token for a word points to this field.  The does>
pointer contains the hub address of a list of execution tokens that is to be
called.

The body is a variable length field that contains the contents of a variable or
the list of execution tokens for a compiled word.  The list for a compiled word
consists of one execution token per word, and is terminated by a zero-valued
word.

        Offset       Content         Size
        ------       -------         ----
          0          Link Pointer    word
          2          Flags           byte
          3          Name Length     byte
          4          Name String     Len bytes
        4+Len        Padding         (-Len) & 3 bytes
    4+(Len+3)&(-4)   Code Pointer    word
    6+(Len+3)&(-4)   DOES> Pointer   word
    8+(Len+3)&(-4)   Body            Variable

