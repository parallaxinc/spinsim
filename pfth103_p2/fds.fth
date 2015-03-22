\ ############################################################################
\ # fds.fth - This program implements a full duplex serial port.  It is base
\ # on the FullDuplexSerial Spin object.
\ #
\ # Copyright (c) 2012 Dave Hein
\ # MIT Licensed
\ ############################################################################

0 value fds_cog
create fds_vars 68 allot

: fds_var create , does> @ fds_vars + ;

 0 fds_var rx_head
 4 fds_var rx_tail
 8 fds_var tx_head
12 fds_var tx_tail
16 fds_var rx_pin
20 fds_var tx_pin
24 fds_var rxtx_mode
28 fds_var bit_ticks
32 fds_var buffer_ptr
36 fds_var rx_buffer
52 fds_var tx_buffer

hex

create fds_entry
  a0bca9f0 , 80fca810 , 08bcaa54 , a0fcb201 , 2cbcb255 , 80fca804 , 08bcaa54 ,
  a0fcbe01 , 2cbcbe55 , 80fca804 , 08bcae54 , 80fca804 , 08bcb054 , 80fca804 ,
  08bcb454 , a0bcc05a , 80fcc010 , 627cae04 , 617cae02 , 689be85f , 68abec5f ,
  a0fcc833 , 5cbcbc64 , 627cae01 , 613cb3f2 , 5c640016 , a0fcb809 , a0bcba58 ,
  28fcba01 , 80bcbbf1 , 80bcba58 , 5cbcbc64 , a0bca85d , 84bca9f1 , c17ca800 ,
  5c4c001f , 613cb3f2 , 30fcb601 , e4fcb81e , 28fcb617 , 60fcb6ff , 627cae01 ,
  6cd4b6ff , 08bcabf0 , 80bcaa5a , 003cb655 , 84bcaa5a , 80fcaa01 , 60fcaa0f ,
  083cabf0 , 5c7c0016 , 5cbcc85e , a0bca9f0 , 80fca808 , 08bcaa54 , 80fca804 ,
  08bcac54 , 863caa56 , 5c680033 , 80bcac60 , 00bcc256 , 84bcac60 , 80fcac01 ,
  60fcac0f , 083cac54 , 68fcc300 , 2cfcc202 , 68fcc201 , a0fcc40b , a0bcc7f1 ,
  627cae04 , 617cae02 , 6ce0c201 , 29fcc201 , 70abe85f , 7497ec5f , 80bcc658 ,
  5cbcc85e , a0bca863 , 84bca9f1 , c17ca800 , 5c4c004d , e4fcc446 , 5c7c0033 ,

decimal

: fds_start ( rxpin txpin mode baudrate ... )
  >r
  fds_vars 16 0 fill
  rxtx_mode ! tx_pin ! rx_pin !
  clkfreq@ r> / bit_ticks !
  rx_buffer buffer_ptr !
  0 dira!
  fds_entry rx_head cognew
  1+ dup to fds_cog
;

: fds_stop fds_cog if fds_cog 1- cogstop 0 to fds_cog then ;

: fds_rxcheck rx_tail @ rx_head @ 2dup . . cr =
  if
    -1
  else
    rx_tail @ rx_buffer + c@
    rx_tail @ 1+ 15 and rx_tail !
  then
;

: fds_rx begin fds_rxcheck dup -1 = while drop repeat ;

: fds_tx begin tx_tail @ tx_head @ 1+ 15 and <> until
  tx_head @ tx_buffer + c!
  tx_head @ 1+ 15 and tx_head !
  rxtx_mode @ 8 and if fds_rx then
;

: fdstest
  ." Type 'q' to quit" cr
  31 30 0 115200 fds_start
  drop
  begin
    fds_rx
    dup [char] q <>
  while
    [char] < fds_tx fds_tx [char] > fds_tx
  repeat
  drop
  fds_stop
  1 30 lshift dira!
;

