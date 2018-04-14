'******************************************************************************
' Author: Dave Hein
' Version 1.0
' Copyright (c) 2010, 2011
' See end of file for terms of use.
'******************************************************************************
{{
  This object provide file I/O functions for SpinSim.  It currently implements only popen, pclose and pread.
  The methods a similar to the ones the FSRW object. A dummy mount routine is provide for compatibility.
}}
con
  SYS_COMMAND = $12340000
  SYS_LOCKNUM = $12340002
  SYS_PARM    = $12340004

  SYS_FILE_OPEN     = 3
  SYS_FILE_CLOSE    = 4
  SYS_FILE_READ     = 5
  SYS_FILE_WRITE    = 6
  SYS_FILE_OPENDIR  = 7
  SYS_FILE_CLOSEDIR = 8
  SYS_FILE_READDIR  = 9
  SYS_FILE_SEEK     = 10
  SYS_FILE_TELL     = 11
  SYS_FILE_REMOVE   = 12
  SYS_FILE_CHDIR    = 13
  SYS_FILE_GETCWD   = 14
  SYS_FILE_MKDIR    = 15
  SYS_FILE_GETMOD   = 16

dat
  handle0 long 0
  dirhand0 long 0
  direntbuf long 0[20]

'****************************
' FSRW Routines
'****************************
pub mount(pin)
  ifnot word[SYS_LOCKNUM]
    word[SYS_LOCKNUM] := locknew + 1

pub mount_explicit(DO, CLK, DI, CS)
  ifnot word[SYS_LOCKNUM]
    word[SYS_LOCKNUM] := locknew + 1

pub popen(fname, mode)
  pclose
  handle0 := hopen(fname, mode)
  ifnot handle0
    return -1

pub pclose
  if handle0
    hclose(handle0)
  handle0~

pub pread(buffer, num)
  return hread(handle0, buffer, num)

pub pwrite(buffer, num)
  return hwrite(handle0, buffer, num)

pub opendir
  if dirhand0
    hclosedir(dirhand0)
  dirhand0 := hopendir
  ifnot dirhand0
    return -1

pub nextfile(fbuf)
  return hnextfile(dirhand0, fbuf)
  
pub seek(position)
  result := hseek(handle0, position)
  
pub tell
  result := htell(handle0)
  
pub get_filesize
  result := hget_filesize(handle0)

'****************************
' handle Routines
'****************************
pub hopen(fname, mode)
  if mode => "a" and mode =< "z"
    mode -= "a" - "A"
  if mode == "R"
    mode := string("rb")
  elseif mode == "W"
    mode := string("wb")
  elseif mode == "A"
    mode := string("ab")
  elseif mode == "D"
    return SystemCall(SYS_FILE_REMOVE, fname)
  else
    return 0
  result := SystemCall(SYS_FILE_OPEN, @fname)

pub hclose(handle)
  result := SystemCall(SYS_FILE_CLOSE, handle)

pub hread(handle, buffer, num)
  result := SystemCall(SYS_FILE_READ, @handle)

pub hwrite(handle, buffer, num)
  result := SystemCall(SYS_FILE_WRITE, @handle)

pub hopendir
  result := SystemCall(SYS_FILE_OPENDIR, 0)

pub hclosedir(handle)
  result := SystemCall(SYS_FILE_CLOSEDIR, handle)

pub hnextfile(handle, fbuf) | num
  result := hreaddir(handle)
  if result
    num := strsize(@direntbuf[2])
    if num > 31
      num := 31
    bytemove(fbuf, @direntbuf[2], num)
    byte[fbuf][num] := 0

pub hreaddir(handle) | pdirent
  pdirent := @direntbuf
  result := SystemCall(SYS_FILE_READDIR, @handle)
  if result
    result := @direntbuf
  
pub hseek(handle, position) | whence
  whence~
  result := SystemCall(SYS_FILE_SEEK, @handle)

pub htell(handle)
  result := SystemCall(SYS_FILE_TELL, handle)
  
pub hget_filesize(handle) | offset, whence, position, filesize
  position := SystemCall(SYS_FILE_TELL, handle)
  offset~
  whence := 2
  SystemCall(SYS_FILE_SEEK, @handle)
  filesize := SystemCall(SYS_FILE_TELL, handle)
  offset := position
  whence~
  SystemCall(SYS_FILE_SEEK, @handle)
  return filesize

pub chdir(path)
  result := SystemCall(SYS_FILE_CHDIR, path)
  
pub getcwd(str, num)
  result := SystemCall(SYS_FILE_CHDIR, @str)
  
pub mkdir(path)
  result := SystemCall(SYS_FILE_MKDIR, path)

pub getmod(fname)
  result := SystemCall(SYS_FILE_GETMOD, fname)

pub SystemCall(command, parm) | locknum
  locknum := word[SYS_LOCKNUM] - 1
  if locknum == -1
    return -1
    
  repeat until not lockset(locknum)
  long[SYS_PARM] := parm
  word[SYS_COMMAND] := command
  repeat while word[SYS_COMMAND]
  result := long[SYS_PARM]
  lockclr(locknum)
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
 
