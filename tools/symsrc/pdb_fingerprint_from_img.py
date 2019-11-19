#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This will retrieve a PDBs "fingerprint" from it's corresponding executable
image (.dll or .exe).  This is used when retrieving the PDB from the symbol
server.  The .pdb (or cab compressed .pd_) is expected at a path like:
 foo.pdb/FINGERPRINT/foo.pdb

We can retrieve the same information from the .PDB file itself, but this file
format is much more difficult and undocumented.  Instead, we can look at the
DLL's reference to the PDB, and use that to retrieve the information."""

from __future__ import print_function

import sys
import pefile


__CV_INFO_PDB70_format__ = ('CV_INFO_PDB70',
  ('4s,CvSignature', '16s,Signature', 'L,Age'))

__GUID_format__ = ('GUID',
  ('L,Data1', 'H,Data2', 'H,Data3', '8s,Data4'))


def GetPDBInfoFromImg(filename):
  """Returns the PDB fingerprint and the pdb filename given an image file"""

  pe = pefile.PE(filename)

  for dbg in pe.DIRECTORY_ENTRY_DEBUG:
    if dbg.struct.Type == 2:  # IMAGE_DEBUG_TYPE_CODEVIEW
      off = dbg.struct.AddressOfRawData
      size = dbg.struct.SizeOfData
      data = pe.get_memory_mapped_image()[off:off+size]

      cv = pefile.Structure(__CV_INFO_PDB70_format__)
      cv.__unpack__(data)
      cv.PdbFileName = data[cv.sizeof():]
      guid = pefile.Structure(__GUID_format__)
      guid.__unpack__(cv.Signature)
      guid.Data4_0 = ''.join("%02X" % ord(x) for x in guid.Data4[0:2])
      guid.Data4_1 = ''.join("%02X" % ord(x) for x in guid.Data4[2:])

      return ("%08X%04X%04X%s%s%d" % (
          guid.Data1, guid.Data2, guid.Data3,
          guid.Data4_0, guid.Data4_1, cv.Age),
          cv.PdbFileName.split('\x00', 1)[0])

    break


def main():
  if len(sys.argv) != 2:
    print("usage: file.dll")
    return 1

  (fingerprint, filename) = GetPDBInfoFromImg(sys.argv[1])
  print("%s %s" % (fingerprint, filename))
  return 0


if __name__ == '__main__':
  sys.exit(main())
