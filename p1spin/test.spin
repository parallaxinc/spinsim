CON
  _clkmode = xtal1+pll16x
  _clkfreq = 80_000_000

OBJ
  ser : "Simple_Serial"
  dry : "dry11"

PUB main | ptr, count
  'waitcnt(clkfreq*3+cnt)
  ser.init(31, 30, 19200)
  ser.str(string(13, "Testing Spin Interpreter", 13))
  dry.Dhrystone11
  waitcnt(clkfreq/10+cnt)