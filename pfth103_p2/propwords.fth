( PROP WORDS)
hex

( REGISTER ACCESS )
: cnt@ 1f1 cog@ ;
: ina@ 1f2 cog@ ;
: outa@ 1f4 cog@ ;
: outa! 1f4 cog! ;
: dira@ 1f6 cog@ ;
: dira! 1f6 cog! ;
: clkfreq@ 0 @ ;

( BIT SETTING AND CLEARING )
: dirasetbit dira@ or dira! ;
: diraclrbit invert dira@ and dira! ;
: outasetbit outa@ or outa! ;
: outaclrbit invert outa@ and outa! ;

( HUBOPS )
: cogid   ( ... cogid )   0 0cfc0001 cogx1 ;
: locknew ( ... locknum ) 0 0cfc0004 cogx1 ;
: lockret ( locknum ... ) 0cfc0005 cogx1 drop ;
: cogstop ( cognum ... )  0c7c0003 cogx1 drop ;
: coginit ( codeptr dataptr cognum ... cognum )
  >r 0e lshift or 2 lshift r> or 0ffc0002 cogx1 ;
: cognew  ( codeptr dataptr ... cognum ) 8 coginit ;
: waitcnt ( count ... count ) f8fc0000 cogx1 ;
: reboot 80 0cfc0000 cogx1 ;

decimal

( ANS UTILITY )
: ms ( msec ... ) cnt@ swap clkfreq@ 1000 / * + waitcnt drop ;

( ANS TOOLS EXT )
: bye reboot ;

( ENABLE SERIAL OUTPUT )
1 30 lshift dup outa! dira!

