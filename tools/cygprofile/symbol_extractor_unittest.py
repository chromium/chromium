#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import tempfile
import unittest

import symbol_extractor


# The number of spaces that objdump prefixes each symbol with.
SPACES = ' ' * 14

class TestSymbolInfosFromStream(unittest.TestCase):

  SYMBOL_INFO_DUMP = r"""[
  {
    "FileSummary": {
      "File": "./exe.unstripped/chrome_crashpad_handler",
      "Format": "elf64-littleaarch64",
      "Arch": "aarch64",
      "AddressSize": "64bit",
      "LoadName": "<Not found>"
    },
    "Symbols": [
      {
        "Symbol": {
          "Name": {
            "Name": "",
            "Value": 0
          },
          "Value": 0,
          "Size": 0,
          "Binding": {
            "Name": "Local",
            "Value": 0
          },
          "Type": {
            "Name": "None",
            "Value": 0
          },
          "Other": {
            "Value": 0,
            "Flags": []
          },
          "Section": {
            "Name": "Undefined",
            "Value": 0
          }
        }
      },
      {
        "Symbol": {
          "Name": {
            "Name": "first",
            "Value": 42
          },
          "Value": 12697692,
          "Size": 44,
          "Binding": {
            "Name": "Local",
            "Value": 2
          },
          "Type": {
            "Name": "Function",
            "Value": 4
          },
          "Other": {
            "Value": 0,
            "Flags": []
          },
          "Section": {
            "Name": ".text",
            "Value": 65521
          }
        }
      },
      {
        "Symbol": {
          "Name": {
            "Name": "second",
            "Value": 42
          },
          "Value": 341,
          "Size": 18,
          "Binding": {
            "Name": "Global",
            "Value": 2
          },
          "Type": {
            "Name": "Function",
            "Value": 4
          },
          "Other": {
            "Value": 0,
            "Flags": []
          },
          "Section": {
            "Name": ".text",
            "Value": 65521
          }
        }
      },
      {
        "Symbol": {
          "Name": {
            "Name": "third",
            "Value": 53234419
          },
          "Value": 83614392,
          "Size": 80,
          "Binding": {
            "Name": "Local",
            "Value": 0
          },
          "Type": {
            "Name": "GNU_IFunc",
            "Value": 10
          },
          "Other": {
            "Value": 2,
            "Flags": [
              {
                "Name": "STV_HIDDEN",
                "Value": 2
              }
            ]
          },
          "Section": {
            "Name": ".text",
            "Value": 16
          }
        }
      },
      {
        "Symbol": {
          "Name": {
            "Name": "do_not_parse",
            "Value": 1216
          },
          "Value": 39499768,
          "Size": 80,
          "Binding": {
            "Name": "Local",
            "Value": 0
          },
          "Type": {
            "Name": "None",
            "Value": 0
          },
          "Other": {
            "Value": 0,
            "Flags": []
          },
          "Section": {
            "Name": ".text",
            "Value": 16
          }
        }
      }
    ]
  }
]"""

  def testSymbolInfosFromStream(self):
    with tempfile.TemporaryFile(suffix='.json') as fp:
      fp.write(self.SYMBOL_INFO_DUMP.encode('utf8'))
      fp.seek(0)
      symbol_infos = symbol_extractor._SymbolInfosFromStream(fp)
      self.assertEquals(len(symbol_infos), 3)
      first = symbol_extractor.SymbolInfo('first', 0x00c1c05c, 0x2c, '.text')
      self.assertEquals(first, symbol_infos[0])
      second = symbol_extractor.SymbolInfo('second', 0x00155, 0x12, '.text')
      self.assertEquals(second, symbol_infos[1])
      third = symbol_extractor.SymbolInfo('third', 0x4fbdab8, 0x50, '.text')
      self.assertEquals(third, symbol_infos[2])


class TestSymbolInfoMappings(unittest.TestCase):

  def setUp(self):
    self.symbol_infos = [
        symbol_extractor.SymbolInfo('firstNameAtOffset', 0x42, 42, '.text'),
        symbol_extractor.SymbolInfo('secondNameAtOffset', 0x42, 42, '.text'),
        symbol_extractor.SymbolInfo('thirdSymbol', 0x64, 20, '.text')]

  def testGroupSymbolInfosByOffset(self):
    offset_to_symbol_info = symbol_extractor.GroupSymbolInfosByOffset(
        self.symbol_infos)
    self.assertEquals(len(offset_to_symbol_info), 2)
    self.assertIn(0x42, offset_to_symbol_info)
    self.assertEquals(offset_to_symbol_info[0x42][0], self.symbol_infos[0])
    self.assertEquals(offset_to_symbol_info[0x42][1], self.symbol_infos[1])
    self.assertIn(0x64, offset_to_symbol_info)
    self.assertEquals(offset_to_symbol_info[0x64][0], self.symbol_infos[2])

if __name__ == '__main__':
  unittest.main()
