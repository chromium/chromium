#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for the rc_header formatter'''

# GRD samples exceed the 80 character limit.
# pylint: disable-msg=C6310


import os
import sys
import unittest

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit import util
from grit.format import rc_header


class RcHeaderFormatterUnittest(unittest.TestCase):
  def FormatAll(self, grd):
    output = rc_header.FormatDefines(grd)
    return ''.join(output).replace(' ', '')

  def testFormatter(self):
    grd = util.ParseGrdForUnittest('''
        <includes first_id="300" comment="bingo">
          <include type="gif" name="ID_LOGO" file="images/logo.gif" />
        </includes>
        <messages first_id="10000">
          <message name="IDS_GREETING" desc="Printed to greet the currently logged in user">
            Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
          </message>
          <message name="IDS_BONGO">
            Bongo!
          </message>
        </messages>
        <structures>
          <structure type="dialog" name="IDD_NARROW_DIALOG" file="rc_files/dialogs.rc" />
          <structure type="version" name="VS_VERSION_INFO" file="rc_files/version.rc" />
        </structures>''')
    output = self.FormatAll(grd)
    self.assertTrue(output.count('IDS_GREETING10000'))
    self.assertTrue(output.count('ID_LOGO300'))

  def testOnlyDefineResourcesThatSatisfyOutputCondition(self):
    grd = util.ParseGrdForUnittest('''
        <includes first_id="300" comment="bingo">
          <include type="gif" name="ID_LOGO" file="images/logo.gif" />
        </includes>
        <messages first_id="10000">
          <message name="IDS_FIRSTPRESENTSTRING" desc="Present in .rc file.">
            I will appear in the .rc file.
          </message>
          <if expr="False"> <!--Do not include in the .rc files until used.-->
            <message name="IDS_MISSINGSTRING" desc="Not present in .rc file.">
              I will not appear in the .rc file.
            </message>
          </if>
          <if expr="lang != 'es'">
            <message name="IDS_LANGUAGESPECIFICSTRING" desc="Present in .rc file.">
              Hello.
            </message>
          </if>
          <if expr="lang == 'es'">
            <message name="IDS_LANGUAGESPECIFICSTRING" desc="Present in .rc file.">
              Hola.
            </message>
          </if>
          <message name="IDS_THIRDPRESENTSTRING" desc="Present in .rc file.">
            I will also appear in the .rc file.
          </message>
       </messages>''')
    output = self.FormatAll(grd)
    self.assertTrue(output.count('IDS_FIRSTPRESENTSTRING10000'))
    self.assertFalse(output.count('IDS_MISSINGSTRING'))
    self.assertTrue(output.count('IDS_LANGUAGESPECIFICSTRING10002'))
    self.assertTrue(output.count('IDS_THIRDPRESENTSTRING10003'))

  def testEmit(self):
    grd = util.ParseGrdForUnittest('''
        <outputs>
          <output type="rc_all" filename="dummy">
            <emit emit_type="prepend">Wrong</emit>
          </output>
          <if expr="False">
            <output type="rc_header" filename="dummy">
              <emit emit_type="prepend">No</emit>
            </output>
          </if>
          <output type="rc_header" filename="dummy">
            <emit emit_type="append">Error</emit>
          </output>
          <output type="rc_header" filename="dummy">
            <emit emit_type="prepend">Bingo</emit>
          </output>
        </outputs>''')
    output = ''.join(rc_header.Format(grd, 'en', '.'))
    output = util.StripBlankLinesAndComments(output)
    self.assertEqual('#pragma once\nBingo', output)

  def testRcHeaderFormat(self):
    grd = util.ParseGrdForUnittest('''
        <includes first_id="300" comment="bingo">
          <include type="gif" name="IDR_LOGO" file="images/logo.gif" />
        </includes>
        <messages first_id="10000">
          <message name="IDS_GREETING" desc="Printed to greet the currently logged in user">
            Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
          </message>
          <message name="IDS_BONGO">
            Bongo!
          </message>
        </messages>''')

    # Using the default settings.
    output = rc_header.FormatDefines(grd)
    self.assertEqual(('#define IDR_LOGO 300\n'
                      '#define IDS_GREETING 10000\n'
                      '#define IDS_BONGO 10001\n'), ''.join(output))

    # Using resource allowlist support.
    grd.SetAllowlistSupportEnabled(True)
    output = rc_header.FormatDefines(grd)
    self.assertEqual(('#define IDR_LOGO '
                      '(::ui::AllowlistedResource<300>(), 300)\n'
                      '#define IDS_GREETING '
                      '(::ui::AllowlistedResource<10000>(), 10000)\n'
                      '#define IDS_BONGO '
                      '(::ui::AllowlistedResource<10001>(), 10001)\n'),
                     ''.join(output))

if __name__ == '__main__':
  unittest.main()
