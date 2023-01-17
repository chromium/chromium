# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


try:
  import hashlib
  _new_md5 = hashlib.md5
except ImportError:
  import md5
  _new_md5 = md5.new


"""64-bit fingerprint support for strings.

Usage:
    from extern import FP
    print('Fingerprint is %ld' % FP.FingerPrint('Hello world!'))
"""


def _UnsignedFingerPrintImpl(str, encoding='utf-8'):
  """Generate a 64-bit fingerprint by taking the first half of the md5
  of the string.
  """
  hex128 = _new_md5(str.encode(encoding)).hexdigest()
  int64 = int(hex128[:16], 16)
  return int64


def UnsignedFingerPrint(str, encoding='utf-8'):
  """Generate a 64-bit fingerprint.

  The default implementation uses _UnsignedFingerPrintImpl, which
  takes the first half of the md5 of the string, but the
  implementation may be switched using SetUnsignedFingerPrintImpl.
  """
  return _UnsignedFingerPrintImpl(str, encoding)


def FingerPrint(str, encoding='utf-8'):
  fp = UnsignedFingerPrint(str, encoding=encoding)
  # interpret fingerprint as signed longs
  if fp & 0x8000000000000000:
    fp = -((~fp & 0xFFFFFFFFFFFFFFFF) + 1)
  return fp


def UseUnsignedFingerPrintFromModule(module_name):
  """Imports module_name and replaces UnsignedFingerPrint in the
  current module with the function of the same name from the imported
  module.

  Returns the function object previously known as
  grit.extern.FP.UnsignedFingerPrint.
  """
  hash_module = __import__(module_name, fromlist=[module_name])
  return SetUnsignedFingerPrint(hash_module.UnsignedFingerPrint)


def SetUnsignedFingerPrint(function_object):
  """Sets grit.extern.FP.UnsignedFingerPrint to point to
  function_object.

  Returns the function object previously known as
  grit.extern.FP.UnsignedFingerPrint.
  """
  global UnsignedFingerPrint
  original_function_object = UnsignedFingerPrint
  UnsignedFingerPrint = function_object
  return original_function_object
