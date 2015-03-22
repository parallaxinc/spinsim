'******************************************************************************
' Author: Dave Hein
' Version 1.0
' Copyright (c) 2010, 2011
' See end of file for terms of use.
'******************************************************************************
{{
  This object provides console I/O functions for SpinSim.  It implements the same methods as FullDuplexSerial.
}}
con
  SYS_COMMAND = $12340000
  SYS_LOCKNUM = $12340002
  SYS_PARM    = $12340004

  SYS_CON_PUTCH     = 1
  SYS_CON_GETCH     = 2

PUB start(rxpin, txpin, mode, baudrate) : okay
  ifnot word[SYS_LOCKNUM]
    word[SYS_LOCKNUM] := locknew + 1
  return 1

PUB stop


PUB rxflush

'' Flush receive buffer

  repeat while rxcheck => 0
  

PUB rxtime(ms) : rxbyte | t

'' Wait ms milliseconds for a byte to be received
'' returns -1 if no byte received, $00..$FF if byte

  t := cnt
  repeat until (rxbyte := rxcheck) => 0 or (cnt - t) / (clkfreq / 1000) > ms
  

PUB rx : rxbyte

'' Receive byte (may wait for byte)
'' returns $00..$FF

  repeat while (rxbyte := rxcheck) < 0

PUB rxcheck | locknum
  locknum := word[SYS_LOCKNUM] - 1
  if locknum == -1
    return -1
  repeat until not lockset(locknum)
  word[SYS_COMMAND] := SYS_CON_GETCH
  repeat while word[SYS_COMMAND]
  result := long[SYS_PARM]
  lockclr(locknum)


PUB tx(txbyte) | locknum

'' Send byte (may wait for room in buffer)
  locknum := word[SYS_LOCKNUM] - 1
  if locknum == -1
    return
  repeat until not lockset(locknum)
  long[SYS_PARM] := txbyte
  word[SYS_COMMAND] := SYS_CON_PUTCH
  repeat while word[SYS_COMMAND]
  lockclr(locknum)


PUB str(stringptr)

'' Send string                    

  repeat strsize(stringptr)
    tx(byte[stringptr++])
    

PUB dec(value) | i, x

'' Print a decimal number

  x := value == NEGX                                                            'Check for max negative
  if value < 0
    value := ||(value+x)                                                        'If negative, make positive; adjust for max negative
    tx("-")                                                                     'and output sign

  i := 1_000_000_000                                                            'Initialize divisor

  repeat 10                                                                     'Loop for 10 digits
    if value => i                                                               
      tx(value / i + "0" + x*(i == 1))                                          'If non-zero digit, output digit; adjust for max negative
      value //= i                                                               'and digit from value
      result~~                                                                  'flag non-zero found
    elseif result or i == 1
      tx("0")                                                                   'If zero digit (or only digit) output it
    i /= 10                                                                     'Update divisor

PUB hex(value, digits)

'' Print a hexadecimal number

  value <<= (8 - digits) << 2
  repeat digits
    tx(lookupz((value <-= 4) & $F : "0".."9", "A".."F"))
    'tx(hexdigit[(value <-= 4) & $F])

PUB bin(value, digits)

'' Print a binary number

  value <<= 32 - digits
  repeat digits
    tx((value <-= 1) & 1 + "0")

PUB out(char)
  tx(char)
{{
+------------------------------------------------------------------------------------------------------------------------------+
|                                                   TERMS OF USE: MIT License                                                  |
+------------------------------------------------------------------------------------------------------------------------------+
|Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation    |
|files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,    |
|modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software|
|is furnished to do so, subject to the following conditions:                                                                   |
|                                                                                                                              |
|The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.|
|                                                                                                                              |
|THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE          |
|WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR         |
|COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,   |
|ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                         |
+------------------------------------------------------------------------------------------------------------------------------+
}}