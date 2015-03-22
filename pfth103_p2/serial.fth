: waitcnt begin dup cnt@ - 0< until ;

: putch 256 or dup + clkfreq@ 9600 /
  11 >r
  cnt@
  begin
    r> 1- dup >r
  while
    rot dup
    30 lshift
    outa!
    1 rshift
    swap rot dup rot +
    waitcnt
  repeat
  r> 2drop 2drop
;

: test cnt@ 65 putch cnt@ swap - 80 / . ;

