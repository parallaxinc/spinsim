: cond.name ( link )
  dup link>flags c@ dup 2 and ( link flag literal )
  if
    8 and
    if
      .name
    then
  else
    4 and
    if
      [char] s emit [char] " emit bl emit
    else
      .name
    then
  then
;

: seefunc ( xt )
  >body   ( listptr )
  begin
    dup w@ ( listptr xt )
  while
    dup w@ ( listptr xt )
    >link .name link>flags c@ dup 2 and ( listptr flags literal )
    if
      8 and ( listptr flags jump )
      if
        2 + dup dup w@ swap - 2 - 2 / .
      else
        2 + dup w@ .
      then
    else
      4 and ( listptr string )
      if
        2 + dup count type [char] " emit space
        dup c@ + 0 2 - and
      then
    then
    compsize + ( listptr+=compsize)
  repeat
  drop
;

: see
  ' dup ( xt xt )
  if
    dup >flags c@ dup 16 and ( xt flags kernel )
    if
      drop
      drop
      ." Kernel Word"
    else
        32 and
        if
          drop
          ." Variable"
        else
          seefunc
        then
    then
  else
    drop
    ."  ?"
  then
;
