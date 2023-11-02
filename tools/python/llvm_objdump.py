# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import subprocess

_CHROME_SRC = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
_LLVM_OBJDUMP_PATH = os.path.join(_CHROME_SRC, 'third_party', 'llvm-build',
                                  'Release+Asserts', 'bin', 'llvm-objdump')

# Function lines look like:
#   000177b0 <android::IBinder::~IBinder()+0x2c>:
# We pull out the address and function first. Then we check for an optional
# offset. This is tricky due to functions that look like "operator+(..)+0x2c"
_FUNC = re.compile(r"(^[a-f0-9]*) <(.*)>:$")
_OFFSET = re.compile(r"(.*)\+0x([a-f0-9]*)")

# A disassembly line looks like:
#   177b2:  b510        push  {r4, lr}
_ASM = re.compile(r"(^[ a-f0-9]*):[ a-f0-0]*.*$")


def _StripPC(addr, cpu_arch):
  """Strips the Thumb bit from a program counter address when appropriate.

  Args:
    addr: the program counter address
    cpu_arch: Target CPU architecture.

  Returns:
    The stripped program counter address.
  """
  if cpu_arch == "arm":
    return addr & ~1
  return addr


class ObjdumpInformation(object):
  def __init__(self, address, library, symbol, offset):
    self.address = address
    self.library = library
    self.symbol = symbol
    self.offset = offset


class LLVMObjdumper(object):
  def __init__(self):
    """Creates an instance of LLVMObjdumper that interacts with llvm-objdump.
    """
    self._llvm_objdump_parameters = [
        '--disassemble',
        '--demangle',
        '--section=.text',
    ]

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    pass

  @staticmethod
  def GetSymbolDataFromObjdumpOutput(objdump_out, address, cpu_arch):
    stripped_target_address = _StripPC(address, cpu_arch)
    for line in objdump_out.split(os.linesep):
      components = _FUNC.match(line)
      if components:
        # This is a new function, so record the current function and its
        # address.
        current_symbol_addr = int(components.group(1), 16)
        current_symbol = components.group(2)

        # Does it have an optional offset like: "foo(..)+0x2c"?
        components = _OFFSET.match(current_symbol)
        if components:
          current_symbol = components.group(1)
          offset = components.group(2)
          if offset:
            current_symbol_addr -= int(offset, 16)

      # Is it a disassembly line like: "177b2:  b510        push  {r4, lr}"?
      components = _ASM.match(line)
      if components:
        addr = components.group(1)
        i_addr = int(addr, 16)
        if i_addr == stripped_target_address:
          return (current_symbol, stripped_target_address - current_symbol_addr)

    return (None, None)

  def GetSymbolInformation(self, lib, address, cpu_arch):
    """Returns the corresponding function names and line numbers.

    Args:
      lib: library to search for info.
      address: address to look for info.
      cpu_arch: architecture where the dump was taken

    Returns:
      An ObjdumpInformation object
    """
    if not os.path.isfile(_LLVM_OBJDUMP_PATH):
      logging.error('Cannot find llvm-objdump. path=%s', _LLVM_OBJDUMP_PATH)
      return None

    stripped_address = _StripPC(address, cpu_arch)

    full_arguments = [_LLVM_OBJDUMP_PATH] + self._llvm_objdump_parameters
    full_arguments.append('--start-address=' + str(stripped_address))
    full_arguments.append('--stop-address=' + str(stripped_address + 8))
    full_arguments.append(lib)

    objdump_process = subprocess.Popen(full_arguments,
                                       stdout=subprocess.PIPE,
                                       stdin=subprocess.PIPE,
                                       universal_newlines=True)

    stdout, stderr = objdump_process.communicate()
    objdump_process_return_code = objdump_process.poll()

    if objdump_process_return_code != 0:
      logging.error(
          'Invocation of llvm-objdump failed!' +
          ' tool-command-line=\'{}\', return-code={}, std-error=\'{}\''.format(
              ' '.join(full_arguments), objdump_process_return_code, stderr))
      return None

    symbol, offset = LLVMObjdumper.GetSymbolDataFromObjdumpOutput(
        stdout, address, cpu_arch)

    return ObjdumpInformation(address=address,
                              library=lib,
                              symbol=symbol,
                              offset=offset)
