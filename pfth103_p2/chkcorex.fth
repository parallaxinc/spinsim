( This program checks for all 45 ANS Forth core ext words )

: checkword 1 + ' 0 = 
  if swap 1 + swap source type ."  failed" 13 emit 10 emit then ;

: checkdone  swap dup
  if swap dup rot rot swap - . ." out of "
  else drop ." All " then
  . ." ANS Forth core ext words implemented" 13 emit 10 emit ;

0 0
checkword #tib
checkword .(
checkword .r
checkword 0<>
checkword 0>
checkword 2>r
checkword 2r>
checkword 2r@
checkword :noname
checkword <>
checkword ?do
checkword again
checkword c"
checkword case
checkword compile,
checkword convert
checkword endcase
checkword endof
checkword erase
checkword expect
checkword false
checkword hex
checkword marker
checkword nip
checkword of
checkword pad
checkword parse
checkword pick
checkword query
checkword refill
checkword restore-input
checkword roll
checkword save-input
checkword source-id
checkword span
checkword tib
checkword to
checkword true
checkword tuck
checkword u.r
checkword u>
checkword value
checkword within
checkword [compile]
checkword \
checkdone
