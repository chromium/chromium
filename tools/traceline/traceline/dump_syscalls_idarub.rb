#!/usr/bin/env ruby

# Copyright 2009 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is an idarub script for extracting system call numbers from a DLL that
# has been loaded into the IDA disassembler.  The interesting system call stubs
# are contained in ntdll.dll, user32.dll, gdi32.dll, and imm32.dll.

require 'idarub'

ida, = IdaRub.auto_client

curea = 0

filename = ida.get_root_filename

while true
  curea = ida.find_binary(
      curea, ida.BADADDR, 'ba 00 03 fe 7f', 16, ida.SEARCH_DOWN)
  break if curea == ida.BADADDR

  raise "z" if ida.get_byte(curea - 5) != 0xb8

  syscall = ida.get_long(curea - 4)
  # Remove the IDA _ prefix and the @argsize trailing decorator...
  funcname = ida.get_func_name(curea).split('@', 2)[0].split('_', 2)[-1]
  puts '%d: "%s!%s",' % [syscall, filename, funcname]

  curea += 1
end
