\ ############################################################################
\ # i2c.fth - This program reads and writes EEPROMs using the I2C protocol.
\ # This program is based on Mike Green's basic_i2c_driver, which is found in
\ # the Parallax Object Exchange (OBEX).
\ #
\ # The first parameter for all routines is the pin number of the clock.  It
\ # is assumed that the data pin number is one greater than the clock pin
\ # number.
\ #
\ # Copyright (c) 2012 Dave Hein
\ # MIT Licensed
\ ############################################################################

: i2c_dira_sda_high dup  dira@ or dira! ;
: i2c_outa_sda_high dup  outa@ or outa! ;
: i2c_dira_scl_high over dira@ or dira! ;
: i2c_outa_scl_high over outa@ or outa! ;
: i2c_dira_sda_low  dup  invert dira@ and dira! ;
: i2c_outa_sda_low  dup  invert outa@ and outa! ;
: i2c_dira_scl_low  over invert dira@ and dira! ;
: i2c_outa_scl_low  over invert outa@ and outa! ;

\ This routine should be called before calling any of the other ones to ensure
\ that the EEPROM is in a known ready state.
: i2c_init ( scl ... )
  1 swap lshift dup 2*           \ sda := scl + 1
  i2c_outa_scl_high              \ outa[scl] := 1
  i2c_dira_scl_high              \ dira[scl] := 1
  i2c_dira_sda_low               \ dira[sda] := 0
  9 0 do                         \ repeat 9
    i2c_outa_scl_low             \   outa[scl] := 0
    i2c_outa_scl_high            \   outa[scl[ := 1
    dup ina@ and if leave then   \   if ina[sda] quit
  loop
  2drop
;

\ This routine sends a start bit
: i2c_start ( scl ... )
  1 swap lshift dup 2*           \ sda := scl + 1
  i2c_outa_scl_high              \ outa[scl]~~
  i2c_dira_scl_high              \ dira[scl]~~
  i2c_outa_sda_high              \ outa[sda]~~
  i2c_dira_sda_high              \ dira[sda]~~
  i2c_outa_sda_low               \ outa[sda]~
  i2c_outa_scl_low               \ outa[scl]~
  2drop
;

\ This routine sends a stop bit
: i2c_stop ( scl ... )
  1 swap lshift dup 2*           \ sda := scl + 1
  i2c_outa_scl_high              \ outa[scl]~~
  i2c_outa_sda_high              \ outa[sda]~~
  i2c_dira_scl_low               \ dira[scl]~
  i2c_dira_sda_low               \ dira[sda]~
  2drop
;

\ This routine sends one byte and returns the ACK bit
: i2c_write ( scl data ... ackbit )
  23 lshift swap                 \ data <<= 23
  1 swap lshift dup 2*           \ sda := scl + 1
  8 0 do                         \ repeat 8
    rot 2* dup 0<
    if
      rot rot
      i2c_outa_sda_high          \ outa[sda] := 1
    else
      rot rot
      i2c_outa_sda_low           \ outa[sda] := 0
    then
    i2c_outa_scl_high            \ outa[scl]~~
    i2c_outa_scl_low             \ dira[scl]~
  loop
  i2c_dira_sda_low               \ dira[sda]~
  i2c_outa_scl_high              \ outa[scl]~~
  dup ina@ and >r                \ ackbit := ina[sda]
  i2c_outa_scl_low               \ outa[scl]~
  i2c_outa_sda_low               \ outa[sda]~
  i2c_dira_sda_high              \ dira[sda]~~
  2drop drop
  r>                             \ return ackbit
;

\ This routine reads one byte from the EEPROM
: i2c_read ( scl ackbit ... data )
  >r                             \ save ackbit
  0 swap                         \ data := 0
  1 swap lshift dup 2*           \ sda := scl + 1
  i2c_dira_sda_low               \ dira[sda]~
  8 0 do                         \ repeat 8
    i2c_outa_scl_high            \ outa[scl]~~
    rot 2*                       \ data <<= 1
    over ina@ and if 1 or then   \ if ina[sda] data |= 1
    rot rot
    i2c_outa_scl_low             \ outa[scl]~
  loop
  r> if
    i2c_outa_sda_high            \ outa[sda]~~
  else
    i2c_outa_sda_low             \ outa[sda]~
  then
  i2c_dira_sda_high              \ dira[sda]~~
  i2c_outa_scl_high              \ outa[scl]~~
  i2c_outa_scl_low               \ outa[scl]~
  i2c_outa_sda_low               \ outa[sda]~
  2drop                          \ return data
;

\ This routine reads up to one page of data from the EEPROM
: i2c_readpage ( scl devsel addrreg dataptr count ... ackbit )
  >r >r                        \ Move count and dataptr to the return stack
  dup 15 rshift 14 and rot or  \ Assemble the devsel byte
  dup >r rot dup dup >r        \ Copy devsel and scl to the return stack
  i2c_start                    \ Send a start bit
  swap                         \ Arrange the scl and devsel on the stack
  i2c_write drop               \ Send the devsel byte
  dup 8 rshift 255 and r@ swap \ Extract the second address byte
  i2c_write drop               \ Send the second address byte
  255 and r@ swap              \ Extract the third address byte
  i2c_write drop               \ Send it
  r@                           \ Get the scl from the return stack
  i2c_start                    \ Send a start bit
  r> r> 1 or over >r           \ Get the scl and devsel byte and set the LSB
  i2c_write drop               \ Send the devsel byte
  r> r> r> 1 ?do               \ Get scl, dataptr and count and start do loop
    over 0 i2c_read over c! 1+ \ Read a byte from the EEPROM and save it
  loop
  over 1 i2c_read swap c!      \ Read the last byte from the EEPROM
  i2c_stop                     \ Send a stop bit
  0                            \ Return the ack bit
;

variable i2c_var

\ This routine reads a byte from the specified address
: i2c_readbyte ( scl devsel addrreg )
  i2c_var 1 i2c_readpage drop i2c_var c@ ;

\ This routine reads a word from the specified address
: i2c_readword ( scl devsel addrreg )
  0 i2c_var ! i2c_var 2 i2c_readpage drop i2c_var @ ;

\ This routine reads a long from the specified address
: i2c_readlong ( scl devsel addrreg )
  i2c_var 4 i2c_readpage drop i2c_var @ ;

\ This routine writes up to one page of data to the EEPROM
: i2c_writepage ( scl devsel addrreg dataptr count ... ackbit )
  >r >r                       \ ( scl devsel addrreg ) r( count dataptr )
  dup 15 rshift 14 and rot or \ ( scl addrreg devsel )
  rot dup >r                  \ ( addrreg devsel scl ) r( count dataptr scl )
  i2c_start                   \ ( addrreg devsel     ) r( count dataptr scl )
  r@ swap                     \ ( addrreg slc devsel ) r( count dataptr scl )
  i2c_write drop              \ ( addrreg            ) r( count dataptr scl )
  dup 8 rshift 255 and r@ swap
  i2c_write drop
  255 and r@ swap
  i2c_write drop
  r> r> r> 0 ?do
    2dup c@ i2c_write drop 1+
  loop
  drop
  i2c_stop
  0
;

\ This routine writes a byte to the specified address
: i2c_writebyte ( scl devsel addrreg data )
  i2c_var ! i2c_var 1 i2c_writepage drop ;

\ This routine writes a word to the specified address
: i2c_writeword ( scl devsel addrreg data )
  i2c_var ! i2c_var 2 i2c_writepage drop ;

\ This routine writes a long to the specified address
: i2c_writelong ( scl devsel addrreg data )
  i2c_var ! i2c_var 4 i2c_writepage drop ;

\ This routine returns a zero if the EEPROM is ready after a write
\ Otherwise it returns a non-zero value
: i2c_writewait ( scl devsel addrreg )
  15 rshift 14 and or
  over i2c_start
  over >r i2c_write
  r> i2c_stop
;

\ This word will be run at startup
: startup
  1 30 lshift dup outa! dira!
  pfthversion type cr
;

\ Set up the cog config struct
: setupconfig
  496 cog@             \ Get config struct address from PAR
  dup 16 + @           \ Get the address of the return stack
  4 + over 12 + !      \ Add four and set initial address of return stack
  ['] interpret >body  \ Get the address of the Forth interpreter
  over 16 + @ !        \ Write it to the return stack
  ['] startup >body    \ Get the address of the startup word
  swap !               \ Write it to the intial value of the program counter
;

\ Save the hub RAM to EEPROM
: eesave
  setupconfig
  28 i2c_init
  512 0
  do
    28 160 i 64 * dup 64 i2c_writepage drop
    begin 28 160 0 i2c_writewait 0= until
    i 7 and 7 = if [char] . emit then
  loop
;
