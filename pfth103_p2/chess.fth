\ This program was written by Lennart Benschop and converted to ANS Forth by
\ by Jeff Fox.  It was further modified by Dave Hein to run under pfth.  The
\ orignal source is available at http://www.ultatechnology.com/chess.html.

HEX

: scroll cr ;
: cls page ;
: key? 1 ;
: off false swap ! ;
: >defer 2 + w@ 4 - ;

3 constant maxlevel
create bp0
maxlevel 1 + c0 * allot

variable bpv

: bp bpv @ ;
: b@ bpv @ + c@ ;
: b! bpv @ + c! ;

: boardvar create ,
 does> c@ bpv @ + ;
  0c boardvar start
  0d boardvar castlew
  0e boardvar castleb
  0f boardvar ep
  1c boardvar starting
  1d boardvar piece
  1e boardvar best
  1f boardvar farther?
  2c boardvar wlcastle?
  2d boardvar blcastle?
  2e boardvar check
  2f boardvar pawnmove
  3c boardvar kingw
  3d boardvar kingb
  3e boardvar inpassing
  3f boardvar advance
  4c boardvar valuew
  5c boardvar alfa
  6c boardvar beta
  7c boardvar (eval)
  8c boardvar highest
  9c boardvar cutoff
  ac boardvar valueb
  bc boardvar played

variable level

variable lastcnt

: +level
  bp dup c0 + c0 cmove
  c0 bpv +!   1 level +! ;

: -level
  -c0 bpv +!  -1 level +! ;


create symbols

  CHAR . , CHAR p , CHAR k , CHAR b ,
  CHAR r , CHAR q , CHAR K ,

create values
 0 , 40 , c0 , c0 , 140 , 240 , 3000 ,

: .board
  cls
  0 0 at-xy 20 spaces
  cr 2 spaces
  [CHAR] H 1 +  [CHAR] A do i emit 2 spaces loop
  bp 20 + 8 0 do
    cr 20 spaces
    cr [CHAR] 8 i - emit
    0a 2 do space
     dup i + c@ dup
     07 and cells symbols + 1 type
     dup 80 and if ." W" drop else
      if ." B" else ." ." then
     then
    loop
    10 +
  loop cr drop ;

: .pos
  10 /mod
  swap 2 - [CHAR] A + emit
  [CHAR] 8 2 + swap - emit ;

\ constants that indicate the directions on the board
-11 constant nw    -0f constant no
 0f constant zw     11 constant zo
-10 constant n      10 constant z
 -1 constant w       1 constant o

create spring
-12 , -21 , -1f , -0e , 12 , 21 , 1f , 0e ,

defer tmove

defer attacktest

: mine?
  b@ dup 0= 0= swap 80 and start c@ = and ;

variable movits

: moveit
  starting c@ best c!   1 farther? c!
  begin
   best c@  over +  dup best c!
   dup mine? over b@ 87 = or 0=
   farther? c@ and while
    tmove
    b@ 0= farther? c!
  repeat
  drop drop
 1 movits +! ;

: Bishop
  no nw zo zw  moveit moveit moveit moveit ;

: Rook
  n o z w  moveit moveit moveit moveit ;

: Queen
  n o z w no nw zo zw  8 0 do moveit loop ;

: Knight
   8 0 do
   i cells spring + @
   starting c@ +  dup best c!
   dup mine? swap b@ 87 = or 0=
   if tmove then
   loop ;

: ?castle
  start c@ 80 = if castlew else castleb then  c@  check  c@ 0=  and ;

: ?lcastle
  start c@ 80 = if wlcastle? else blcastle? then  c@  check  c@ 0=  and ;

: king
  n o z w no nw zo zw   8 0 do
   starting c@ + dup best c!
   dup mine? swap b@ 87 = or 0=
   if tmove then
  loop
  ?castle if 28 start c@ if 70 + then
           dup bp + 1- @ 0=
           if
            dup 1- attacktest 0=
            if
             best c! tmove
            else drop then
           else drop then
        then
  ?lcastle if 24 start c@ if 70 + then
            dup bp + @ over bp + 1- @ or 0=
            if
             dup 1 + attacktest 0=
             if
              best c! tmove
             else drop then
            else drop then
         then ;

: Pawnrow
  start c@ if negate then ;

: Pawnz
  dup best c!
  f0 and start c@ if 20 else 90 then =
  if  6 2 do i advance c! tmove loop
  else tmove then
  0 pawnmove c! 0 inpassing c! 0 advance c! ;

: Pawn
  starting c@ z Pawnrow +
  dup b@ if
   drop
  else
   dup Pawnz
   z Pawnrow + dup b@ if
    drop
   else
    starting c@ f0 and
    start c@ if 80 else 30 then =
    if starting c@ 0f and pawnmove c!
       Pawnz
    else drop
    then
   then
  then
  zw zo  2 0 do
   Pawnrow starting c@ +
   dup f0 and start c@ if 40 else 70 then =
   over 0f and ep c@ = and
   if 1 inpassing c!
      dup Pawnz
   then
   dup b@ dup 0= 2 pick mine? or
   swap 87 = or
   if drop else Pawnz then
  loop ;

create pieces

 ' noop , ' Pawn , ' Knight , ' Bishop , ' Rook , ' Queen , ' king ,

: piecemove
\ using above jump table for each type of piece - jump table uses , (CELLS)
  piece c@ cells pieces + @ execute ;

: ?piecemove
  starting c@ dup mine? if
   b@ 07 and piece c!
   0 pawnmove c! 0 inpassing c! 0 advance c!
   piecemove
  else drop then ;

: allmoves
  [char] . emit
  start c@ 0= if
   22 starting c!
   8 0 do
    8 0 do
     ?piecemove starting c@ 1 + starting c!
    loop
    starting c@ 8 + starting c!
   loop
  else
   92 starting c!
   8 0 do
    8 0 do
     ?piecemove starting c@ 1 + starting c!
    loop
    starting c@ 18 - starting c!
   loop
  then ;

variable attack

: ?attack
  best c@ dup mine? 0=
  swap b@ 07 and piece c@ = and
  attack @ or attack ! ;

: attacked?
  attack off 0 7 1 do
   i piece c!
   piecemove
   attack @ if drop 1 leave then
  loop ;

variable starting'
variable best'
variable start'
variable tmove'

: settest
  starting c@ starting' c!
  best  c@ best'  c!
  start c@ start' c!
  ['] tmove >defer tmove' !
  ['] ?attack is tmove ;

: po@
  starting' c@ starting c!
  best' c@ best c!
  start' c@ start c!
  tmove' @ is tmove ;

: changecolor
  start c@ 80 xor start c! ;

variable endf
variable playlevel
variable #legal
variable selected
variable compcolor
variable move#

create bp1 c0 allot

: endgame?
  start c@ if valueb else valuew then @ c1 < ;

: evalboard
  valueb @ valuew @ - start c@ if negate then
  55 mine? 1 and + 56 mine? 1 and + 65 mine? 1 and + 66 mine? 1 and +
  changecolor 55 mine? + 56 mine? + 65 mine? + 66 mine? + changecolor

  endgame? if
   start c@ if kingb else kingw then c@
   dup f0 and dup 20 = swap 90 = or 7 and
   swap 0f and dup 2 = swap 9 = or 7 and + +
  then ;

: ?check
  settest
  start c@ if kingw else kingb then c@
  starting c! attacked? check  c!
  po@ ;

: (attacktest)
  ['] tmove >defer ['] ?attack <> if
   settest
   starting c!
   attacked?
   po@
  else drop true
  then ;

' (attacktest) is attacktest

variable seed

: rnd
 seed @ 743 * 43 + dup seed ! ;
\ 1 ;

: domove
  best c@ b@ 7 and cells values + @ negate start c@
  if valueb else valuew then +!
  starting c@ b@ best c@ b!
  0 starting c@ b!
  advance c@ if
   advance c@ dup cells values + @ 40 - start c@
   if valueb else valueb then +!
   start c@ or best c@ b!
  then
  piece c@ 4 = if
   starting c@ 0f and 2 =
   if
    0 start c@ if wlcastle? else blcastle? then c!
   then
   starting c@ 0f and 9 =
   if
    0 start c@ if castlew else castleb then c!
   then
  then
  piece c@ 6 = if
   0 0 start c@ if castlew else castleb then dup >r c!
   r> 1f + c!
   best c@ starting c@ - 2 =
   if
    4 start c@ or best c@ 1- b!
    0 best c@ 1 + b!
   then
   best c@ starting c@ - -2 =
   if
    4 start c@ or best c@ 1 + b!
    0 best c@ 2 - b!
   then
   best c@ start c@ if kingw else kingb then c!
  then
  inpassing c@ if
   0 best c@ n Pawnrow + b!
   -40 start c@ if valueb else valuew then +!
  then
  pawnmove c@ ep c! ;

: deeper
  cutoff @
  invert if
  +level
   domove
   ?check  check  c@ if -level exit then
   -1 played  c0 - !
   level @ playlevel @ = if
    evalboard
    (eval) c0 - !
   else
    alfa @ highest !
    alfa @ negate beta @ negate alfa ! beta !
    changecolor
    0 played !
    allmoves
    played @ 0= if
     ?check  check  c@ if -2000 highest ! else 0 highest ! then
    then
    highest @ negate
    (eval) c0 - !
   then
  -level
   (eval) @ highest @ max
   highest !
   highest @ beta @ > if TRUE cutoff ! then


  then ;

: analyse
  +level
   domove
   ?check  check  c@ 0= if
    1 #legal +!
    changecolor
    ['] tmove >defer
    ['] deeper is tmove
    0 played !
    allmoves
    is tmove
    played @ 0= if
     ?check  check  c@ if -2000 highest ! else 0 highest ! then
    then
     highest @ beta c0 - @ = if
    rnd 2000 > if #legal @ selected ! then
    then
    highest @ beta c0 - @ < if
     #legal @ selected !
     highest @ beta c0 - !
    then
   then
  -level ;

: select
  +level
   domove
   ?check  check  c@ 0= if
    1 #legal +!
    #legal @ selected @ = if
     bp bp1 c0 cmove
     starting c@ .pos ." -" best c@ .pos space
    then
   then
  -level ;

: against
  +level
   domove
   ?check  check  c@ 0= if
    1 #legal +!
   then
  -level ;

: compmove
 .board
  ['] analyse is tmove
  0 #legal !
  -4000 alfa ! 4000 beta !

\  0 18 at-xy cr
  scroll

  \ 28 spaces
  start c@ if 1 move# +! move# @ 3 .r space else 4 spaces then
  ?check  check  c@ if ."  Check" then
  1 selected !
  allmoves
  #legal @ 0= if
   check  c@ if
    ." mate"
   else
    ." Pat"
   then
   TRUE endf !
  else
   ['] select is tmove
   0 #legal !
   allmoves
   bp1 bp0 c0 cmove
   changecolor
   ['] against is tmove
   0 #legal !
   allmoves
   ?check  check  c@ if ."  Check" then
   #legal @ 0= if
    check  c@ if
     ." mate"
    else
     ." Pat"
    then
    TRUE endf !
   then
  then
  .board ;

variable startingm
variable bestm
variable personmove

: legal
  startingm @ starting c@ =
  bestm @ best c@ = and
  personmove @ advance c@ = and
  if
  +level
   domove
   ?check  check  c@ 0= if
    1 #legal !
    bp bp1 c0 cmove
   then
  -level
  then ;

create inputbuf 6 allot

: inpos
  dup inputbuf + c@ [CHAR] A -
  dup 8 u<
  rot inputbuf + 1 + c@ [CHAR] 1 -
  dup 8 u< rot and
  swap 7 swap - 10 * rot + 22 + ;

: promote
  0 6 2 do over symbols i cells + c@ = if drop i then loop ;

: person
 begin
   .board

   scroll

   \ 28 spaces
   start c@ if 1 move# +! move# @ 3 .r else 3 spaces then

   inputbuf 5 expect    cr

   \ [char] X emit inputbuf 5 type [char] X emit

   inputbuf c@ [CHAR] Q = if quit then
   0 inpos startingm !
   2 inputbuf + c@ [CHAR] - = and
   3 inpos bestm !
   and
   bestm @ f0 and start c@ if 20 else 90 then =
   startingm b@ 07 and 1 =  and
   if
    ." What piece? " 0 0 begin drop drop key promote dup until
    personmove ! emit
   else
    0 personmove !
   then
   if
    ['] legal is tmove
    0 #legal !
    startingm c@ starting c! ?piecemove
    #legal @
   else
    0
   then
   dup 0=  start c@ and if -1 move# +! then
  until
  bp1 bp0 c0 cmove
  changecolor

  cr

 .board ;

: setmove
  compcolor @ 0<  start c@ 80 =  = if compmove else person then ;

variable manVsMachine

: askcolor
  manVSmachine @
  if ." Do you want White Y/N"
     key dup [CHAR] Y = swap [CHAR] y = or
     if 1 else -1 then compcolor !
  then ;

: asklevel
  cr ." Level? 2-"
   maxlevel . key [CHAR] 0 - 2 max maxlevel min playlevel !
  cls ;

: init
  0 level !   bp0 bpv !
  bp c0 87 fill
  4 2 3 6 5 3 2 4   8 0 do bp 22 + i + c! loop
  bp 32 + 8 01 fill
  bp 42 + 8 00 fill   bp 52 + 8 00 fill
  bp 62 + 8 00 fill   bp 72 + 8 00 fill
  bp 82 + 8 81 fill
  84 82 83 86 85 83 82 84   8 0 do bp 92 + i + c! loop
  1 castlew c! 1 castleb c! 0 ep c! 1 wlcastle? c! 1 blcastle? c! 0 advance c!
  80 start c! 96 kingw c! 26 kingb c!
  askcolor cr asklevel
  0 move# ! 0 endf !
  0 check  c! 9c0 valuew ! 9c0 valueb ! ;

: play
  begin setmove endf @ until ;

: games
  begin init play again ;

: autoplay
  begin setmove compcolor @ negate compcolor ! key? if quit then endf @ until ;

: auto
  init -1 compcolor ! autoplay ;

: chess
  cls
  ." ANS Forth Chess" cr
  ." Do you want to play against the computer? Y/N" cr
  begin rnd drop key? until key
  dup [CHAR] Y = swap [CHAR] y = or dup manVsMachine !
  if games else auto then ;

decimal
