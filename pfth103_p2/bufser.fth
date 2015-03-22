\ Define data space that will be used by the serial cog
create bufserstack    40 allot     \ Data stack space
create bufserreturn   40 allot     \ Return stack space
create bufserbuffer   32 allot     \ Serial buffer
create bufserrdindex   0 ,         \ Read index
create bufserwrindex   0 ,         \ Write index

\ This word runs in a separate cog and writes input characters into the buffer
: bufsercog
  bufserbuffer
  begin
    getchar over bufserwrindex @ + c!
    bufserwrindex dup @ 1 + 31 and swap !
  again ;

\ This word replaces the old KEY word with one that reads from the buffer
: bufkey
  bufserrdindex @
  begin
    dup
    bufserwrindex @
    -
  until
  bufserbuffer + c@
  bufserrdindex dup @ 1 + 31 and swap !
  ;

\ This is the cog configuration structure used by the serial cog
create bufserconfig           \ Forth cog config structure
  ' bufsercog >body ,         \ Get execution token for TOGGLE
  bufserstack ,               \ Initial value of stack ptr
  bufserstack ,               \ Empty value for stack ptr
  bufserreturn ,              \ Initial value of return ptr
  bufserreturn ,              \ Empty value for return ptr

\ This word starts a cog running the BUFSER word
: startbufsercog forth @ bufserconfig cognew ;

\ This word starts the serial cog and sets key to the new one
: bufser startbufsercog ['] bufkey is key ;

\ Buffer the serial input into memory until an ESC is received
: bufferit here 256 + dup
  ." Buffering serial input to memory" cr
  ." Enter <ESC> to exit" cr
  begin
    key dup 27 <>
  while
    over c! 1+
  repeat
  drop over -
  \ over infile cog!
;

\ Determine the length of the current line
: linelen ( addr count -- addr len )
  >r dup ( addr ptr ) ( left )
  begin
    r@ 0 <=
    if over - r> drop exit then
    dup c@ dup 10 = swap 13 = or
    if over - r> drop exit then
    1+ r> 1- >r
  again
;

\ Variables to store the buffer address and count
variable evalbufaddr
variable evalbufcount

\ Evaluate a buffer containing multiple lines
: evalbuf ( addr count )
  evalbufcount !
  evalbufaddr !
  begin
    evalbufcount @ 0 >
  while
    evalbufaddr @ evalbufcount @
    linelen
    dup 1+ dup
    evalbufaddr @ + evalbufaddr !
    evalbufcount @ swap - evalbufcount !
    over over type cr
    evaluate
  repeat
;

\ Redefine switch to use buffered serial
\ : switch bufser ;
