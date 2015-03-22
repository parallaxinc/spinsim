0 value ii        0 value jj
0 value KeyAddr   0 value KeyLen
create SArray   256 allot   \ state array of 256 bytes
create Results  100 allot
: KeyArray      KeyLen mod   KeyAddr ;

: get_byte      + c@ ;
: set_byte      + c! ;
: as_byte       255 and ;
: reset_ij      0 TO ii   0 TO jj ;
: i_update      1 +   as_byte TO ii ;
: j_update      ii SArray get_byte +   as_byte TO jj ;
: swap_s_ij
    jj SArray get_byte
       ii SArray get_byte  jj SArray set_byte
    ii SArray set_byte
;

: rc4_init ( KeyAddr KeyLen -- )
    256 min TO KeyLen   TO KeyAddr
    256 0 DO   i i SArray set_byte   LOOP
    reset_ij
    BEGIN
        ii KeyArray get_byte   jj +  j_update
        swap_s_ij
        ii 255 < WHILE
        ii i_update
    REPEAT
    reset_ij
;
: rc4_byte
    ii i_update   jj j_update
    swap_s_ij
    ii SArray get_byte   jj SArray get_byte +   as_byte SArray get_byte  xor
;

\ : cnt@ 0 ;

hex
create AKey   61 c, 8a c, 63 c, d2 c, fb c,
\ create AKey   97 c, 8a c, 99 c, d2 c, fb c,
: test    0 DO  rc4_byte Results i + c!  LOOP  ;
: test1 cr 0 do Results i + c@ . loop cr ;
: time hex cnt@
AKey 5 rc4_init
2c f9 4c ee dc  5 test   \ output should be: f1 38 29 c9 de
cnt@ 5 test1 swap - 13880 / decimal . ." msec" cr ;
decimal

