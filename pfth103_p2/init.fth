: link>flags 2 + ;
: immediate 81 last @ link>flags c! ;
: \ 100 word drop ; immediate

\ The above lines implement the words to allow for "\" comments
\ All numbers are in hex at this point.

\ DEFINE CELL SIZE
: cellsize 4 ;
: cellmask 3 ;
: compsize 2 ;
: compmask 1 ;

\ BASIC STACK WORDS
: rot 2 roll ;
: over 1 pick ;
: 2dup over over ;
: 2drop drop drop ;
: 2swap 3 roll 3 roll ;
: 2over 3 pick 3 pick ;

\ WORD HEADER ACCESSORS
: >does 2 + ;
: >body 4 + ;
: name>xt dup c@ + 4 + 0 4 - and ;
: link>name 3 + ;
: link>xt link>name name>xt ;
: link>does link>xt 2 + ;
: link>body link>xt 4 + ;

\ DEFINE BASIC WORD BUILDERS
: source tib #tib @ ;
\ : compile, , ;
: ' 20 word find 0 = 0 = and ;
: _does r> dup >r 2 + last @ link>does w! ;
: _setjmp 0a last @ link>flags c! ;
: literal 0 compile, compile, ; immediate 
  last @ link>body dup @ swap 2 + w! \ Patch in address of _lit
: postpone ' compile, ; immediate
: ['] ' postpone literal ; immediate
: [compile] ' postpone literal ['] compile, compile, ; immediate
: does> [compile] _does [compile] exit ; immediate

\ CONDITIONAL EXECUTION AND LOOPING
: if ['] _jz compile, here 2 allot ; immediate
: else ['] _jmp compile, here 2 + swap w! here 2 allot ; immediate
: then here swap w! ; immediate
: begin here ; immediate
: until ['] _jz compile, compile, ; immediate
: again ['] _jmp compile, compile, ; immediate
: while ['] _jz compile, here 2 allot ; immediate
: repeat ['] _jmp compile, here 2 + swap w! compile, ; immediate
: do  ['] _lit compile, here 2 allot ['] drop compile,
      ['] swap compile, ['] >r compile, ['] >r compile, here ; immediate
: ?do ['] 2dup compile, ['] > compile, ['] _jz compile, here 2 allot
      ['] swap compile, ['] >r compile, ['] >r compile, here ; immediate
\ : _loop r> swap r> + r> dup >r swap dup >r > 0 = swap >r ;
: loop    ['] _lit compile, 1 compile, ['] _loop compile, ['] _jz compile, compile, ['] r> compile,
          ['] r> compile, here swap w! ['] 2drop compile,    ; immediate
: +loop    ['] _loop compile, ['] _jz compile,    compile, ['] r> compile,
           ['] r> compile, here swap w! ['] 2drop compile,    ; immediate
: leave r> r> drop r> dup >r >r >r ;
: i r> r> dup >r swap >r ;
: j r> r> r> r> dup >r swap >r swap >r swap >r ;

\ DEFINE >FLAGS AND >LINK
: >flags begin 1 - dup c@ 80 and until ;
: >link >flags 2 - ;

\ DEFINE DEFER AND IS
\ Change code pointer from varfunc to deferfunc
: defer create last @ link>xt dup w@ 3 + swap w! ;
: is state @
  if [compile] >body ' >does postpone literal [compile] w!
  else >body ' >does w!
  then ; immediate

\ REDEFINE REFILL AS A DEFERRED WORD
' refill
defer refill
is refill

\ DEFINE "(" COMMENT WORD NOW THAT WE CAN LOOP
: ( begin
      #tib @ >in @
      ?do tib i + c@ 29 = if i 1 + >in ! r> r> drop drop exit then loop
      refill 0 =
    until ; immediate

( PAD AND PRINT SUPPORT )
create pad 100 allot
create printptr 4 allot
: _d2a dup 0a < if 30 else 57 then + ;
: _a2d dup 30 <
  if
    drop 0 1 -
  else
    dup 39 >
    if
      dup 41 <
      if
         drop 0 1 -
      else
        dup 5a >
        if
          dup 61 <
          if
            drop 0 1 -
          else
            dup 7a >
            if
              drop 0 1 -
            else
              57 -
            then
          then
        else
          37 -
        then
      then
    else
      30 -
    then
  then
  dup base @ < 0 =
  if
    drop 0 1 -
  then
;
: c!-- dup >r c! r> 1 - ;
: cprint printptr @ c! printptr @ 1 - printptr ! ;

( DOUBLE WORDS )
: s>d 0 pick 0 < ;
: m* * s>d ;
: um* * 0 ;
: d+ drop 1 roll drop + s>d ;
: d- drop 1 roll drop - s>d ;
: d* drop 1 roll drop * s>d ;
: d/ drop 1 roll drop / s>d ;
: dmod drop 1 roll drop mod s>d ;
: _u/ over over swap 1 rshift swap / dup + dup >r over * rot swap - swap < 1 + r> + ;
: u/ over 0 < if _u/ else / then ;
: ud/ drop 1 roll drop u/ 0 ;
: _umod swap dup 1 rshift 2 pick mod dup + swap 1 and + swap mod ;
: umod over 0 < if _umod else mod then ;
: udmod drop 1 roll drop umod 0 ;

( CORE WORDS )
: +! dup @ rot + swap ! ;
: /mod over over >r >r mod r> r> / ;
: [ state 0 ! ;
: ] state 1 ! ;
: r@ r> r> dup >r swap >r ;
: sm/rem >r 2dup r@ s>d d/ drop r> swap >r s>d dmod drop r> ;
: um/mod >r 2dup r@ s>d ud/ drop r> swap >r s>d udmod drop r> ;
: fm/mod over over xor 1 31 lshift and if sm/rem else sm/rem then ; ( TODO )
: */mod >r m* r> sm/rem ;
: */ */mod swap drop ;
: <# pad ff + printptr ! ;
: hold cprint ;
: # drop dup base @ umod _d2a cprint base @ u/ 0 ;
: #s begin # over over or 0 = until ;
: #> drop drop printptr @ 1 + dup pad 100 + swap - ;
: sign 0 < if 2d hold then ;
: abs dup 0 < if 0 swap - then ;
: type 0 ?do dup c@ emit 1 + loop drop ;
: ._ dup abs 0 <# #s rot sign #> type ;
: . ._ 20 emit ;

: >number dup 0 ?do >r dup c@ _a2d dup 0 < if drop r> leave else swap >r >r
  base @ 0 d* r> 0 d+ r> 1 + r> 1 - then loop ;
: 0= 0 = ;
: 0< 0 < ;
: 1+ 1 + ;
: 1- 1 - ;
: 2! swap over ! cellsize + ! ;
: 2* dup + ;
: 2/ dup 80000000 and swap 1 rshift or ;
: 2@ dup cellsize + @ swap @ ;
: ?dup dup if dup then ;
: aligned cellmask + 0 cellsize - and ;
: align here aligned here - allot ;
: bl 20 ;
: c, here c! 1 allot ;
: cell+ cellsize + ;
: cells cellsize * ;
: char+ 1 + ;
: chars ;
\ : count dup char+ swap c@ ;
: char 20 word count 0= if drop 0 else c@ then ;
: [char] char postpone literal ; immediate
\ : constant create here ! cellsize allot does> @ ;
: constant create , last @ link>xt dup w@ 3 - swap w! ;
: cr 0a emit 0d emit ;
: decimal 0a base ! ;
: environment? drop drop 0 ;
: fill swap >r swap r> 0 ?do 2dup c! 1 + loop 2drop ;
: hex 10 base ! ;
: invert 0 1 - xor ;
: max 2dup < if swap then drop ;
: min 2dup > if swap then drop ;
\ : cmove >r swap r> 0 ?do 2dup c@ swap c! 1+ swap 1+ swap loop 2drop ;
: cmove> >r swap r> dup >r 1- dup >r + swap r> + swap r> ?do 2dup c@ swap c!
  1- swap 1- swap loop 2drop ;
: move r> 2dup > if r> cmove else r> cmove> then ;
: negate 0 swap - ;
: recurse last @ , ; immediate
: _lit" r> dup 1 + swap dup c@ dup rot + compsize + 0 compsize - and >r ;
  84 last @ link>flags c! ( Set STRING flag )
: _compile" [char] " word count dup >r dup >r c, here r> cmove r> allot
  compsize here - compmask and allot ; immediate
create s"buf 50 allot
: s" state @ if ['] _lit" compile, postpone _compile" else 
     [char] " word count >r s"buf r@ cmove s"buf r> then ; immediate
: ." postpone s" ['] type compile, ; immediate
: _abort" if type abort else drop drop then ;
\ : abort" postpone s" ['] _abort" compile, ; immediate
: abort" postpone s" ['] _abort" compile, ;
: space 20 emit ;
: spaces 0 ?do space loop ;
: u._ 0 <# #s #> type ;
: u. u._ 20 emit ;
: u< over over xor 1 31 lshift and if swap then < ;
: unloop r> r> r> drop drop >r ;
: variable create cellsize allot ;

( CORE EXT )
: 0<> 0= invert ;
: 0> 0 > ;
: 2>r r> rot >r swap >r >r ;
: 2r> r> r> r> rot >r swap ;
: 2r@ r> r> r> 2dup >r >r swap rot >r ;
: <> = 0= ;
: erase 0 ?do dup 0 swap ! 1 + loop drop ;
variable span
: expect accept span ! ;
: false 0 ;
: marker create last @ , does> @ dup dp ! @ last ! ;
: nip swap drop ;
: parse word count ;
: true 0 1 - ;
: tuck swap over ;
: to ' >body state @ if postpone literal [compile] ! else ! then ; immediate
\ : value create here ! cellsize allot does> @ ;
: value create , last @ link>xt dup w@ 3 - swap w! ;
: within over - >r - r> u< ;
: .r_ >r dup abs 0 <# #s rot sign #> dup r> swap - spaces type ;
: .r .r_ 20 emit ;
: u.r_ >r 0 <# #s #> dup r> swap - spaces type ;
: u.r .r_ 20 emit ;
: u> over over xor 80000000 and if swap then > ;
: unused 8000 here - ;
: case 0 ; immediate
: of ['] over compile, ['] = compile,
     ['] _jz compile, here 4 allot ['] drop compile, ; immediate
: endof ['] _jmp compile, here 2 + swap w! here 2 allot ; immediate
: endcase ['] drop compile, begin ?dup while here swap w! repeat ; immediate
: c" ['] _lit" compile, postpone _compile" ['] drop compile, ['] 1- compile, ; immediate
: .( [char] ) word count type ; immediate
: :noname align here ['] words @ , [ ;

( DOUBLE )
: d= rot = rot rot = and ;
: d0= or 0 = ;
: 2constant create swap , , does> dup @ swap cellsize + @ ;

( STRING )
: blank 0 ?do dup bl swap c! 1+ loop drop ;
: -trailing dup 0 ?do 2dup + 1- c@ bl = if 1- else leave then loop ;
: /string dup >r - swap r> + swap ;

( TOOLS )
: ? @ . ;
: .s 3c emit depth ._ 3e emit 20 emit depth 0 ?do depth i - 1 - pick . loop ;
: dump 0 ?do i 0f and 0 = if cr dup . then dup c@ 3 .r 1 + loop drop cr ;
: forget 20 word find if >link dup dp ! w@ last ! else abort" ?" then ;
: .name dup link>name count type space ;
: ?newline dup >r link>name c@ dup rot + 1 + dup 4e > if cr else swap then drop r> ;
: words 0 last @ begin dup while ?newline .name w@ repeat 2drop ;

( UTILITY )
: at-xy 2 emit swap emit emit ;
: page 0 emit ;

( VERSION STRING )
: pfthversion s" pfth 1.03" ;

create evalmode 0 ,
0 value source-id
create srcstk0 30 allot
srcstk0 value srcstk

: resetstack depth 0 <
  if
    begin depth while 0 repeat
  else
    begin depth while drop repeat
  then
;

: getnumber 2dup >r >r swap dup c@ [char] - =
  if
    swap
    dup 1 <
    if
      2drop 2drop r> r> 1
    else
      swap 1 + swap 1 - 
      >number dup
      if
        2drop 2drop r> r> 1
      else
        2drop drop negate 0 r> r> 2drop
      then
    then
  else
    swap
    >number dup
    if
      2drop 2drop r> r> 1
    else
      2drop drop 0 r> r> 2drop
    then
  then
;

: compilenumber
  dup ['] _lit compile, compile,
  dup ffff 10 lshift and
  if
    10 rshift
    ['] _lit compile, compile,
    ['] _lit compile, 10 compile,
    ['] lshift compile,
    ['] or compile,
  else
    drop
  then
;

: _interpret
  begin
    20 word dup c@
  while
    find dup
    if
      state @ =
      if
        compile,
      else
        execute
      then
    else
      dup rot count getnumber
      if
        type ." ?" cr
      else
        state @
        if
          compilenumber
        then
      then
    then
  repeat
  drop
;

: .savesrc ." _savesrc " srcstk0 . srcstk . cr ;
: .loadsrc ." _loadsrc " srcstk0 . srcstk . cr ;

: _savesrc ( .savesrc ) tib srcstk ! #tib @ srcstk 4 + ! >in @ srcstk 8 + ! source-id srcstk 0c + ! srcstk 10 + to srcstk ;
: _loadsrc srcstk 10 - to srcstk ( .loadsrc ) srcstk @ to tib srcstk 4 + @ #tib ! srcstk 8 + @ >in ! srcstk 0c + @ to source-id ;
: evaluate _savesrc 0 1 - to source-id #tib ! to tib 0 >in ! _interpret _loadsrc ;

( INTERPRETER )
: interpret
begin
  _interpret
  depth 0 <
  if
    ." Stack Underflow" cr
    resetstack
  else
    source-id 0 1 - =
    if
      _loadsrc
      \ 0 to source-id
    else
      source-id 0=
      if ."  ok" cr then
      refill
    then
  then
again
;

decimal
interpret

