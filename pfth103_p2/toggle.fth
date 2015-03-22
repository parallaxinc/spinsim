\ ############################################################################
\ # toggle.fth - This program starts up a Forth cog that toggles P15
\ #
\ # Copyright (c) 2012 Dave Hein
\ # MIT Licensed
\ ############################################################################

create cogstack  80 allot     \ Allocate data stack space
create cogreturn 80 allot     \ Allocate return stack space
create delaycnt  80000000 ,   \ This variable controls the blink rate

hex
\ This word toggles bit P15 every "delaycnt" cycles
: toggle
  8000 dirasetbit cnt@        \ Set P15 for output and get CNT
  begin
    delaycnt @ + waitcnt      \ Wait "delaycnt" cycles
    8000 outasetbit           \ Set P15
    delaycnt @ + waitcnt      \ Wait "delaycnt" cycles
    8000 outaclrbit           \ Clear P15
  again ;                     \ Repeat forever
decimal

create cogconfig              \ Forth cog config structure
  ' toggle >body ,            \ Get execution token for TOGGLE
  cogstack ,                  \ Initial value of stack ptr
  cogstack ,                  \ Empty value for stack ptr
  cogreturn ,                 \ Initial value of return ptr
  cogreturn ,                 \ Empty value for return ptr

\ This word starts a cog running the TOGGLE word
: starttoggle forth @ cogconfig cognew ;

