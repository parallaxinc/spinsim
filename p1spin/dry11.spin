'**********************************************
' Dhrystone 1.1
'**********************************************

OBJ
  ser : "Simple_Serial"

CON
  _clkmode = xtal1+pll16x
  _clkfreq = 80000000
  LOOPS = 500

  Ident1 = 1
  Ident2 = 2
  Ident3 = 3
  Ident4 = 4
  Ident5 = 5

' typedef struct Record
  PtrComp = 0
  Discr = 4
  EnumComp = 8
  IntComp = 12
  StringComp = 16

  NULL = 0

DAT
  Version byte "1.1", 0
  Array1Glob long 0[51]
  Array2Glob long 0[51*51]
  xxx        long 0[12] 
  yyy        long 0[12]
  PtrGlb     long 0
  PtrGlbNext long 0
  IntGlob    long 0
  BoolGlob   long 0
  Char1Glob  long 0
  Char2Glob  long 0
  String1Loc long 0[8]
  String2Loc long 0[8]
  starttime  long 0
  benchtime  long 0
  nulltime   long 0

PUB Dhrystone11

  PtrGlb := @xxx
  PtrGlbNext := @yyy
  Proc0

{
 * Package 1
 }
PUB Proc0 | IntLoc1, IntLoc2, IntLoc3, CharLoc, CharIndex, EnumLoc, i
  starttime := time_msec
  i := 0
  repeat while (i < LOOPS)
   ++i
  nulltime := time_msec - starttime { Computes o'head of loop }
  long[PtrGlb + PtrComp] := PtrGlbNext
  long[PtrGlb + Discr] := Ident1
  long[PtrGlb + EnumComp] := Ident3
  long[PtrGlb + IntComp] := 40
  strcpy((PtrGlb + StringComp), string("DHRYSTONE PROGRAM, SOME STRING"))
  strcpy(@String1Loc, string("DHRYSTONE PROGRAM, 1'ST STRING")) {GOOF}
  long[@Array2Glob][8 * 51 + 7] := 10 { Was missing in published program }
  {****************
  -- Start Timer --
  ****************}
  starttime := time_msec
  i := 0
  repeat while (i < LOOPS)
    Proc5
    Proc4
    IntLoc1 := 2
    IntLoc2 := 3
    strcpy(@String2Loc, string("DHRYSTONE PROGRAM, 2'ND STRING"))
    EnumLoc := Ident2
    BoolGlob := not Func2(@String1Loc, @String2Loc)
    repeat while (IntLoc1 < IntLoc2)
      IntLoc3 := 5 * IntLoc1 - IntLoc2
      Proc7(IntLoc1, IntLoc2, @IntLoc3)
      ++IntLoc1
    Proc8(@Array1Glob, @Array2Glob, IntLoc1, IntLoc3)
    Proc1(PtrGlb)
    byte[@CharIndex] := "A"
    repeat while (byte[@CharIndex] =< Char2Glob)
      if (EnumLoc == Func1(byte[@CharIndex], "C"))
        Proc6(Ident1, @EnumLoc)
      byte[@CharIndex] := byte[@charIndex] + 1
    IntLoc3 := IntLoc2 * IntLoc1
    IntLoc2 := IntLoc3 / IntLoc1
    IntLoc2 := 7 *(IntLoc3 - IntLoc2) - IntLoc1
    Proc2(@IntLoc1)
   ++i
  {****************
  -- Stop Timer --
  ****************}
  benchtime := time_msec - starttime - nulltime
  ser.str(string("Dhrystone("))
  ser.str(@Version)
  ser.str(string(") time for "))
  ser.dec(LOOPS)
  ser.str(string(" passes = "))
  ser.dec(benchtime)
  ser.str(string(" msec", 13))
  ser.str(string("This machine benchmarks at "))
  ser.dec(LOOPS * 1000 / benchtime)
  ser.str(string(" dhrystones/second", 13))

PUB Proc1(PtrParIn)
  memcpy(long[PtrParIn + PtrComp], PtrGlb, 48)
  long[PtrParIn + IntComp] := 5
  long[long[PtrParIn + PtrComp] + IntComp] := long[PtrParIn + IntComp]
  long[long[PtrParIn + PtrComp] + PtrComp] := long[PtrParIn + PtrComp]
  Proc3(long[long[PtrParIn + PtrComp] + PtrComp])
  if (long[long[PtrParIn + PtrComp] + Discr] == Ident1)
    long[long[PtrParIn + PtrComp] + IntComp] := 6
    Proc6(long[PtrParIn + EnumComp], long[PtrParIn + PtrComp] + EnumComp)
    long[long[PtrParIn + PtrComp] + PtrComp] := long[PtrGlb + PtrComp]
    Proc7(long[long[PtrParIn + PtrComp] + IntComp], 10, long[PtrParIn + PtrComp] + IntComp)
  else
    memcpy(PtrParIn, long[PtrParIn + PtrComp], 48)

PUB Proc2(IntParIO) | IntLoc, EnumLoc
  IntLoc := long[IntParIO] + 10

  repeat 'while ()
    if (Char1Glob == "A")
      --IntLoc
      long[IntParIO] := IntLoc - IntGlob
      EnumLoc := Ident1
    if (EnumLoc == Ident1)
      quit

PUB Proc3(PtrParOut)
  if (PtrGlb <> NULL)
    PtrParOut := long[PtrGlb + PtrComp]
  else
    IntGlob := 100
  Proc7(10, IntGlob, PtrGlb + IntComp)

PUB Proc4 | BoolLoc
  BoolLoc := Char1Glob == "A"
  BoolLoc |= BoolGlob
  Char2Glob := "B"

PUB Proc5
  Char1Glob := "A"
  BoolGlob := FALSE

PUB Proc6(EnumParIn, EnumParOut)
  long[EnumParOut] := EnumParIn
  ifnot Func3(EnumParIn)
    long[EnumParOut] := Ident4
  if EnumParIn == Ident1
    long[EnumParOut] := Ident1
  elseif EnumParIn == Ident2
    if (IntGlob > 100)
      long[EnumParOut] := Ident1
    else
      long[EnumParOut] := Ident4
  elseif EnumParIn == Ident3
    long[EnumParOut] := Ident2
  elseif (EnumParIn == Ident4) or (EnumParIn == Ident5)
    long[EnumParOut] := Ident3

PUB Proc7(IntParI1, IntParI2, IntParOut) | IntLoc
  IntLoc := IntParI1 + 2
  long[IntParOut] := IntParI2 + IntLoc

PUB Proc8(Array1Par, Array2Par, IntParI1, IntParI2) | IntLoc, IntIndex
  IntLoc := IntParI1 + 5
  long[Array1Par][IntLoc] := IntParI2
  long[Array1Par][IntLoc + 1] := long[Array1Par][IntLoc]
  long[Array1Par][IntLoc + 30] := IntLoc
  IntIndex := IntLoc
  repeat while (IntIndex =<(IntLoc + 1))
    long[Array2Par][IntLoc * 51 + IntIndex] := IntLoc
   ++IntIndex
  ++long[Array2Par][IntLoc * 51 + IntLoc - 1]
  long[Array2Par][(IntLoc + 20) * 51 + IntLoc] := long[Array1Par][IntLoc]
  IntGlob := 5

PUB Func1(CharPar1, CharPar2) | CharLoc1, CharLoc2
  byte[@CharLoc1] := byte[@CharPar1]
  byte[@CharLoc2] := byte[@CharLoc1]
  if (byte[@CharLoc2] <> byte[@CharPar2])
    return (Ident1)
  else
    return (Ident2)

PUB Func2(StrParI1, StrParI2) | IntLoc, CharLoc
  IntLoc := 1
  repeat while (IntLoc =< 1)
    if (Func1(byte[StrParI1][IntLoc], byte[StrParI2][IntLoc + 1]) == Ident1)
      byte[@CharLoc] := "A"
      ++IntLoc
    if (byte[@CharLoc] => "W" and byte[@CharLoc] =< "Z")
      IntLoc := 7
    if (byte[@CharLoc] == "X")
      return (TRUE)
    else
      if (strcmp(StrParI1, StrParI2) > 0)
        IntLoc += 7
        return (TRUE)
      else
        return (FALSE)

PUB Func3(EnumParIn) | EnumLoc
  EnumLoc := EnumParIn
  if (EnumLoc == Ident3)
    return (TRUE)
  return (FALSE)

PUB strcpy(dst, src)
  bytemove(dst, src, strsize(src) + 1)
{
  repeat strsize(src)
    byte[dst++] := byte[src++]
}

PUB memcpy(dst, src, num)
  bytemove(dst, src, num)
{
  repeat num
    byte[dst++] := byte[src++]
}

PUB strcmp(str1, str2)
  result := not strcomp(str1, str2)
{
  repeat while byte[str1]
    if byte[str1] <> byte[str2]
      quit
    str1++
    str2++
  return byte[str1] - byte[str2]
}

PUB time_msec
  return cnt / (_clkfreq/1000)
