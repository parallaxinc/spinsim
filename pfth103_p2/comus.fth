( Useful non-standard words )
: @+ dup cell+ swap @ ;
: !+ over ! cell+ ;
: c@+ dup char+ swap c@ ;
: c!+ over c! char+ ;
: between 1+ within ;
: bounds over + swap ;
: buffer: create allot ;
: cell 4 ;
: cell- cell - ;
: not 0= ;
: parse-word bl word count ;
: perform @ execute ;
: >= < 0= ;
: <= > 0= ;
: -rot rot rot ;
: 2- 2 - ;
: 2+ 2 + ;
: 3dup dup 2over rot ;
: 4dup 2over 2over ;
: noop ;
: off false swap ! ;
: on true swap ! ;
: for  ['] >r compile, ['] _lit compile, 0 compile, ['] >r compile, here ;
       immediate
: next ['] _lit compile, 1 compile, ['] _loop compile, ['] _jz compile,
       compile, ['] r> compile, ['] r> compile, ['] 2drop compile, ; immediate
: zstrlen dup begin dup c@ while 1+ repeat swap - ;
: zcount dup zstrlen ;
