#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for <structure> nodes.
'''

from __future__ import print_function

import os
import os.path
import sys
import zlib
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import platform
import tempfile
import unittest
import struct

from grit import constants
from grit import util
from grit.node import brotli_util
from grit.node import structure
from grit.format import rc


class StructureUnittest(unittest.TestCase):
  def testSkeleton(self):
    grd = util.ParseGrdForUnittest('''
        <structures>
          <structure type="dialog" name="IDD_ABOUTBOX" file="klonk.rc" encoding="utf-16-le">
            <skeleton expr="lang == 'fr'" variant_of_revision="1" file="klonk-alternate-skeleton.rc" />
          </structure>
        </structures>''', base_dir=util.PathFromRoot('grit/testdata'))
    grd.SetOutputLanguage('fr')
    grd.RunGatherers()
    transl = ''.join(rc.Format(grd, 'fr', '.'))
    self.failUnless(transl.count('040704') and transl.count('110978'))
    self.failUnless(transl.count('2005",IDC_STATIC'))

  def testRunCommandOnCurrentPlatform(self):
    node = structure.StructureNode()
    node.attrs = node.DefaultAttributes()
    self.failUnless(node.RunCommandOnCurrentPlatform())
    node.attrs['run_command_on_platforms'] = 'Nosuch'
    self.failIf(node.RunCommandOnCurrentPlatform())
    node.attrs['run_command_on_platforms'] = (
        'Nosuch,%s,Othernot' % platform.system())
    self.failUnless(node.RunCommandOnCurrentPlatform())

  def testVariables(self):
    grd = util.ParseGrdForUnittest('''
        <structures>
          <structure type="chrome_html" name="hello_tmpl" file="structure_variables.html" expand_variables="true" variables="GREETING=Hello,THINGS=foo,, bar,, baz,EQUATION=2+2==4,filename=simple" flattenhtml="true"></structure>
        </structures>''', base_dir=util.PathFromRoot('grit/testdata'))
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    node, = grd.GetChildrenOfType(structure.StructureNode)
    filename = node.Process(tempfile.gettempdir())
    filepath = os.path.join(tempfile.gettempdir(), filename)
    with open(filepath) as f:
      result = f.read()
      self.failUnlessEqual(('<h1>Hello!</h1>\n'
                            'Some cool things are foo, bar, baz.\n'
                            'Did you know that 2+2==4?\n'
                            '<p>\n'
                            '  Hello!\n'
                            '</p>\n'), result)
    os.remove(filepath)

  def testGetPath(self):
    base_dir = util.PathFromRoot('grit/testdata')
    grd = util.ParseGrdForUnittest('''
        <structures>
          <structure type="chrome_html" name="hello_tmpl" file="structure_variables.html" expand_variables="true" variables="GREETING=Hello,THINGS=foo,, bar,, baz,EQUATION=2+2==4,filename=simple" flattenhtml="true" use_base_dir="true"></structure>
        </structures>''', base_dir)
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    node, = grd.GetChildrenOfType(structure.StructureNode)
    self.assertEqual(grd.ToRealPath(node.GetInputPath()),
                     os.path.abspath(os.path.join(
                         base_dir, r'structure_variables.html')))

  def testGetPathNoBasedir(self):
    base_dir = util.PathFromRoot('grit/testdata')
    abs_path = os.path.join(base_dir, r'structure_variables.html')
    rel_path = os.path.relpath(abs_path, os.getcwd())
    grd = util.ParseGrdForUnittest('''
        <structures>
          <structure type="chrome_html" name="hello_tmpl" file="''' + rel_path + '''" expand_variables="true" variables="GREETING=Hello,THINGS=foo,, bar,, baz,EQUATION=2+2==4,filename=simple" flattenhtml="true" use_base_dir="false"></structure>
        </structures>''', util.PathFromRoot('grit/testdata'))
    grd.SetOutputLanguage('en')
    grd.RunGatherers()
    node, = grd.GetChildrenOfType(structure.StructureNode)
    self.assertEqual(grd.ToRealPath(node.GetInputPath()),
                     os.path.abspath(os.path.join(
                         base_dir, r'structure_variables.html')))

  def testCompressGzip(self):
    test_data_root = util.PathFromRoot('grit/testdata')
    root = util.ParseGrdForUnittest('''
        <structures>
          <structure name="TEST_TXT" file="test_text.txt"
                   compress="gzip" type="chrome_html" />
        </structures>''', base_dir=test_data_root)
    node, = root.GetChildrenOfType(structure.StructureNode)
    node.RunPreSubstitutionGatherer()
    compressed = node.GetDataPackValue(lang='en', encoding=1)

    decompressed_data = zlib.decompress(compressed, 16 + zlib.MAX_WBITS)
    self.assertEqual(util.ReadFile(
        os.path.join(test_data_root, 'test_text.txt'), util.BINARY),
                     decompressed_data)

  def testCompressBrotli(self):
    test_data_root = util.PathFromRoot('grit/testdata')
    root = util.ParseGrdForUnittest(
        '''
        <structures>
          <structure name="TEST_TXT" file="test_text.txt"
                   compress="brotli" type="chrome_html" />
        </structures>''',
        base_dir=test_data_root)
    node, = root.GetChildrenOfType(structure.StructureNode)
    node.RunPreSubstitutionGatherer()

    # Using the mock brotli decompression executable.
    brotli_util.SetBrotliCommand([sys.executable,
                                 os.path.join(os.path.dirname(__file__),
                                 'mock_brotli.py')])
    compressed = node.GetDataPackValue(lang='en', encoding=1)
    # Assert that the first two bytes in compressed format is BROTLI_CONST.
    self.assertEqual(constants.BROTLI_CONST, compressed[0:2])

    # Compare the actual size of the uncompressed test data with
    # the size appended during compression.
    actual_size = len(util.ReadFile(
        os.path.join(test_data_root, 'test_text.txt'), util.BINARY))
    uncompress_size = struct.unpack('<i', compressed[2:6])[0]
    uncompress_size += struct.unpack('<h', compressed[6:8])[0] << 4*8
    self.assertEqual(actual_size, uncompress_size)

    self.assertEqual(b'This has been mock compressed!', compressed[8:])

  def testNotCompressed(self):
    test_data_root = util.PathFromRoot('grit/testdata')
    root = util.ParseGrdForUnittest('''
        <structures>
          <structure name="TEST_TXT" file="test_text.txt" type="chrome_html" />
        </structures>''', base_dir=test_data_root)
    node, = root.GetChildrenOfType(structure.StructureNode)
    node.RunPreSubstitutionGatherer()
    data = node.GetDataPackValue(lang='en', encoding=1)

    self.assertEqual(util.ReadFile(
        os.path.join(test_data_root, 'test_text.txt'), util.BINARY), data)


if __name__ == '__main__':
  unittest.main()
