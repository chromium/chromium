# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import os
import subprocess
import threading

_CHROME_SRC = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
_LLVM_SYMBOLIZER_PATH = os.path.join(
    _CHROME_SRC, 'third_party', 'llvm-build', 'Release+Asserts', 'bin',
    'llvm-symbolizer')

_UNKNOWN = '<UNKNOWN>'

_ELF_MAGIC_HEADER_BYTES = b'\x7f\x45\x4c\x46'

@functools.lru_cache
def IsValidLLVMSymbolizerTarget(file_path):
  """ Verify the passed file is a valid target for llvm-symbolization

  Args:
    file_path: Path to a file to be checked

  Return:
    True if the file exists and has the correct ELF header, False otherwise
  """
  try:
    with open(file_path, 'rb') as f:
      header_bytes = f.read(4)
      return header_bytes == _ELF_MAGIC_HEADER_BYTES
  except IOError:
    return False


class LLVMSymbolizer(object):
  def __init__(self):
    """Create a LLVMSymbolizer instance that interacts with the llvm symbolizer.

    The purpose of the LLVMSymbolizer is to get function names and line
    numbers of an address from the symbols library.
    """
    self._llvm_symbolizer_subprocess = None
    self._llvm_symbolizer_parameters = [
        '--functions',
        '--demangle',
        '--inlines',
    ]

    # Allow only one thread to call GetSymbolInformation at a time.
    self._lock = threading.Lock()

  def Start(self):
    """Start the llvm symbolizer subprocess.

    Create a subprocess of the llvm symbolizer executable, which will be used
    to retrieve function names etc.
    """
    if os.path.isfile(_LLVM_SYMBOLIZER_PATH):
      self._llvm_symbolizer_subprocess = subprocess.Popen(
          [_LLVM_SYMBOLIZER_PATH] + self._llvm_symbolizer_parameters,
          stdout=subprocess.PIPE,
          stdin=subprocess.PIPE,
          universal_newlines=True)
    else:
      logging.error('Cannot find llvm_symbolizer here: %s.' %
                    _LLVM_SYMBOLIZER_PATH)
      self._llvm_symbolizer_subprocess = None

  def Close(self):
    """Close the llvm symbolizer subprocess.

    Close the subprocess by closing stdin, stdout and killing the subprocess.
    """
    with self._lock:
      if self._llvm_symbolizer_subprocess:
        self._llvm_symbolizer_subprocess.kill()
        self._llvm_symbolizer_subprocess = None

  def __enter__(self):
    """Start the llvm symbolizer subprocess."""
    self.Start()
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    """Close the llvm symbolizer subprocess."""
    self.Close()

  def GetSymbolInformation(self, lib, addr):
    """Return the corresponding function names and line numbers.

    Args:
      lib: library to search for info.
      addr: address to look for info.

    Returns:
      A triplet of address, module-name and list of symbols
    """
    if (self._llvm_symbolizer_subprocess is None):
      logging.error('Can\'t run llvm-symbolizer! ' +
                    'Subprocess for llvm-symbolizer has not been started!')
      return [(_UNKNOWN, lib)]

    if not lib:
      logging.error('Can\'t run llvm-symbolizer! No target is given!')
      return [(_UNKNOWN, lib)]

    if not IsValidLLVMSymbolizerTarget(lib):
      logging.error(
          'Can\'t run llvm-symbolizer! ' +
          'Given binary is not a valid target. path=%s', lib)
      return [(_UNKNOWN, lib)]

    proc = self._llvm_symbolizer_subprocess
    with self._lock:
      proc.stdin.write('%s %s\n' % (lib, hex(addr)))
      proc.stdin.flush()
      result = []
      # Read until an empty line is observed, which indicates the end of the
      # output. Each line with a function name is always followed by one line
      # with the corresponding line number.
      while True:
        line = proc.stdout.readline()
        if line != '\n':
          line_numbers = proc.stdout.readline()
          result.append((line[:-1], line_numbers[:-1]))
        else:
          return result

  @staticmethod
  def IsValidTarget(path):
    return IsValidLLVMSymbolizerTarget(path)
