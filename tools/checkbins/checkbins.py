#!/usr/bin/env python3
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Makes sure that all EXE and DLL files in the provided directory were built
correctly.

In essense it runs a subset of BinScope tests ensuring that binaries have
/NXCOMPAT, /DYNAMICBASE and /SAFESEH.
"""

import json
import os
import optparse
import sys

REPO_ROOT = os.path.join(os.path.dirname(__file__), '..', '..')
FILES_CFG = os.path.join(REPO_ROOT, 'chrome', 'tools', 'build', 'win',
                         'FILES.cfg')
PEFILE_DIR = os.path.join(REPO_ROOT, 'third_party', 'pefile_py3')
sys.path.append(PEFILE_DIR)

import pefile

PE_FILE_EXTENSIONS = ['.exe', '.dll']
# https://docs.microsoft.com/en-us/windows/win32/debug/pe-format
DYNAMICBASE_FLAG = 0x0040
NXCOMPAT_FLAG = 0x0100
NO_SEH_FLAG = 0x0400
GUARD_CF_FLAG = 0x4000
MACHINE_TYPE_AMD64 = 0x8664
MACHINE_TYPE_ARM64 = 0xaa64
CETCOMPAT_BIT = 0  # offset in extended dll characteristics

# Please do not add your file here without confirming that it indeed doesn't
# require /NXCOMPAT and /DYNAMICBASE.  Contact //sandbox/win/OWNERS or your
# local Windows guru for advice.
EXCLUDED_FILES = [
    'crashpad_util_test_process_info_test_child.exe',
    'mini_installer.exe',
    'previous_version_mini_installer.exe',
]

# PE files that are otherwise included but for which /cetcompat is not required.
CETCOMPAT_NOT_REQUIRED = [
    'chrome_proxy.exe',
    'chrome_pwa_launcher.exe',
    'dxcompiler.dll',  # TODO(crbug.com/40927190)
    'elevation_service.exe',
    'nacl64.exe',
    'notification_helper.exe',
]


def IsPEFile(path):
  return (os.path.isfile(path) and
          os.path.splitext(path)[1].lower() in PE_FILE_EXTENSIONS and
          os.path.basename(path) not in EXCLUDED_FILES)


def IsBitSet(data, bit_idx):
  return 0 != data[int(bit_idx / 8)] & (1 << (bit_idx % 8))


def IsCetExpected(path):
  return os.path.basename(path) not in CETCOMPAT_NOT_REQUIRED


def main(options, args):
  directory = args[0]
  pe_total = 0
  pe_passed = 0

  failures = []

  # Load FILES.cfg - it is a python file setting a FILES variable.
  exec_globals = {'__builtins__': None}
  with open(FILES_CFG, encoding="utf-8") as f:
    code = compile(f.read(), FILES_CFG, 'exec')
    exec(code, exec_globals)
  files_cfg = exec_globals['FILES']

  # Determines whether a specified file is in the 'default'
  # filegroup - which means it's shipped with Chrome.
  def IsInDefaultFileGroup(path):
    for fileobj in files_cfg:
      if fileobj['filename'] == os.path.basename(path):
        if 'default' in fileobj.get('filegroup', {}):
          return True
    return False

  for file in os.listdir(directory):
    path = os.path.abspath(os.path.join(directory, file))
    if not IsPEFile(path):
      continue
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(directories=[
        pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG'],
        pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_DEBUG']
    ])
    pe_total = pe_total + 1
    success = True

    # Check for /DYNAMICBASE.
    if pe.OPTIONAL_HEADER.DllCharacteristics & DYNAMICBASE_FLAG:
      if options.verbose:
        print("Checking %s for /DYNAMICBASE... PASS" % path)
    else:
      success = False
      print("Checking %s for /DYNAMICBASE... FAIL" % path)

    # Check for /NXCOMPAT.
    if pe.OPTIONAL_HEADER.DllCharacteristics & NXCOMPAT_FLAG:
      if options.verbose:
        print("Checking %s for /NXCOMPAT... PASS" % path)
    else:
      success = False
      print("Checking %s for /NXCOMPAT... FAIL" % path)

    # Check for /SAFESEH. Binaries should meet one of the following
    # criteria:
    #   1) Have no SEH table as indicated by the DLL characteristics
    #   2) Have a LOAD_CONFIG section containing a valid SEH table
    #   3) Be a 64-bit binary, in which case /SAFESEH isn't required
    #
    # Refer to the following MSDN article for more information:
    # http://msdn.microsoft.com/en-us/library/9a89h429.aspx
    if (pe.OPTIONAL_HEADER.DllCharacteristics & NO_SEH_FLAG or
        (hasattr(pe, "DIRECTORY_ENTRY_LOAD_CONFIG") and
         pe.DIRECTORY_ENTRY_LOAD_CONFIG.struct.SEHandlerCount > 0 and
         pe.DIRECTORY_ENTRY_LOAD_CONFIG.struct.SEHandlerTable != 0) or
        pe.FILE_HEADER.Machine in (MACHINE_TYPE_AMD64, MACHINE_TYPE_ARM64)):
      if options.verbose:
        print("Checking %s for /SAFESEH... PASS" % path)
    else:
      success = False
      print("Checking %s for /SAFESEH... FAIL" % path)

    # ASLR is weakened on Windows 64-bit when the ImageBase is below 4GB
    # (because the loader will never be rebase the image above 4GB).
    if pe.FILE_HEADER.Machine in (MACHINE_TYPE_AMD64, MACHINE_TYPE_ARM64):
      if pe.OPTIONAL_HEADER.ImageBase <= 0xFFFFFFFF:
        print("Checking %s ImageBase (0x%X < 4GB)... FAIL" %
              (path, pe.OPTIONAL_HEADER.ImageBase))
        success = False
      elif options.verbose:
        print("Checking %s ImageBase (0x%X > 4GB)... PASS" %
              (path, pe.OPTIONAL_HEADER.ImageBase))

    # Can only guarantee that files that are built by Chromium
    # are protected by /GUARD:CF. Some system libraries are not.
    if IsInDefaultFileGroup(path):
      # Check for /GUARD:CF.
      if pe.OPTIONAL_HEADER.DllCharacteristics & GUARD_CF_FLAG:
        if options.verbose:
          print("Checking %s for /GUARD:CF... PASS" % path)
      else:
        success = False
        print("Checking %s for /GUARD:CF... FAIL" % path)
    else:
      if options.verbose:
        print("Skipping check for /GUARD:CF for %s." % path)

    # Check cetcompat for x64 - debug directory type
    # IMAGE_DEBUG_TYPE_EX_DLLCHARACTERISTICS.
    if pe.FILE_HEADER.Machine == MACHINE_TYPE_AMD64:
      if IsInDefaultFileGroup(path) and IsCetExpected(path):
        found_cetcompat = False
        for dbg_ent in pe.DIRECTORY_ENTRY_DEBUG:
          if dbg_ent.struct.Type == pefile.DEBUG_TYPE[
              'IMAGE_DEBUG_TYPE_EX_DLLCHARACTERISTICS']:
            # pefile does not read this, so access the raw data.
            ex_dll_offset = dbg_ent.struct.PointerToRawData
            ex_dll_length = dbg_ent.struct.SizeOfData
            ex_dll_char_data = pe.__data__[ex_dll_offset:ex_dll_offset +
                                           ex_dll_length]
            if IsBitSet(ex_dll_char_data, CETCOMPAT_BIT):
              found_cetcompat = True
            break  # only one ex_dllcharacteristics so can stop once seen.

        if found_cetcompat:
          if options.verbose:
            print("Checking %s for /CETCOMPAT... PASS" % path)
        else:
          success = False
          print("Checking %s for /CETCOMPAT... FAIL" % path)
      else:
        if options.verbose:
          print("Skipping check for /CETCOMPAT for %s." % path)

    # Update tally.
    if success:
      pe_passed = pe_passed + 1
    else:
      failures.append(path)

  print("Result: %d files found, %d files passed" % (pe_total, pe_passed))

  if options.json:
    with open(options.json, 'w') as f:
      json.dump(failures, f)

  if pe_passed != pe_total:
    sys.exit(1)

if __name__ == '__main__':
  usage = "Usage: %prog [options] DIRECTORY"
  option_parser = optparse.OptionParser(usage=usage)
  option_parser.add_option("-v", "--verbose", action="store_true",
                           default=False, help="Print debug logging")
  option_parser.add_option("--json", help="Path to JSON output file")
  options, args = option_parser.parse_args()
  if not args:
    option_parser.print_help()
    sys.exit(0)
  main(options, args)
