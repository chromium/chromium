#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.resource_map'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit import util
from grit.format import resource_map


class FormatResourceMapUnittest(unittest.TestCase):
  def testFormatResourceMap(self):
    grd = util.ParseGrdForUnittest('''
        <outputs>
          <output type="rc_header" filename="the_rc_header.h" />
          <output type="resource_map_header"
                  filename="the_resource_map_header.h" />
        </outputs>
        <release seq="3">
          <structures first_id="300">
            <structure type="menu" name="IDC_KLONKMENU"
                       file="grit\\testdata\\klonk.rc" encoding="utf-16" />
          </structures>
          <includes first_id="10000">
            <include type="foo" file="abc" name="IDS_FIRSTPRESENT" />
            <include type="foo" file="rst" resource_path="new_path/rst_resource"
                     name="IDS_WITHRESOURCEPATH" />
            <if expr="False">
              <include type="foo" file="def" name="IDS_MISSING" />
            </if>
            <if expr="lang != 'es'">
              <include type="foo" file="ghi" name="IDS_LANGUAGESPECIFIC" />
            </if>
            <if expr="lang == 'es'">
              <include type="foo" file="jkl" name="IDS_LANGUAGESPECIFIC" />
            </if>
            <include type="foo" file="mno" name="IDS_THIRDPRESENT" />
            <include type="foo" file="opq" name="IDS_FOURTHPRESENT"
                     skip_in_resource_map="true" />
         </includes>
       </release>''', run_gatherers=True)
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_map_header')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include <stddef.h>
#include "ui/base/webui/resource_path.h"
extern const webui::ResourcePath kTheRcHeader[5];
extern const size_t kTheRcHeaderSize;''', output)
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_map_source')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include "the_resource_map_header.h"
#include <stddef.h>
#include <iterator>
#include "the_rc_header.h"
const webui::ResourcePath kTheRcHeader[5] = {
  {"IDC_KLONKMENU", IDC_KLONKMENU},
  {"IDS_FIRSTPRESENT", IDS_FIRSTPRESENT},
  {"IDS_WITHRESOURCEPATH", IDS_WITHRESOURCEPATH},
  {"IDS_LANGUAGESPECIFIC", IDS_LANGUAGESPECIFIC},
  {"IDS_THIRDPRESENT", IDS_THIRDPRESENT},
};
const size_t kTheRcHeaderSize = std::size(kTheRcHeader);''', output)
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_file_map_source')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include "the_resource_map_header.h"
#include <stddef.h>
#include <iterator>
#include "the_rc_header.h"
const webui::ResourcePath kTheRcHeader[5] = {
  {"grit/testdata/klonk.rc", IDC_KLONKMENU},
  {"abc", IDS_FIRSTPRESENT},
  {"new_path/rst_resource", IDS_WITHRESOURCEPATH},
  {"ghi", IDS_LANGUAGESPECIFIC},
  {"mno", IDS_THIRDPRESENT},
};
const size_t kTheRcHeaderSize = std::size(kTheRcHeader);''', output)

  def testFormatResourceMapWithGeneratedFile(self):
    os.environ["root_gen_dir"] = "gen"

    grd = util.ParseGrdForUnittest('''\
        <outputs>
          <output type="rc_header" filename="the_rc_header.h" />
          <output type="resource_map_header"
                  filename="resource_map_header.h" />
        </outputs>
        <release seq="3">
          <includes first_id="10000">
            <include type="BINDATA"
                     file="${root_gen_dir}/foo/bar/baz.js"
                     name="IDR_FOO_BAR_BAZ_JS"
                     use_base_dir="false"
                     compress="gzip" />
         </includes>
        </release>''', run_gatherers=True)

    with self.assertRaises(AssertionError) as assertion_error:
      formatter = resource_map.GetFormatter('resource_file_map_source')
      util.StripBlankLinesAndComments(''.join(formatter(grd, 'en', '.')))
    self.assertTrue(str(assertion_error.exception). \
        startswith('resource_path attribute missing for IDR_FOO_BAR_BAZ_JS'))

  def testFormatResourceMapWithOutputAllEqualsFalseForStructures(self):
    grd = util.ParseGrdForUnittest('''
        <outputs>
          <output type="rc_header" filename="the_rc_header.h" />
          <output type="resource_map_header"
                  filename="the_resource_map_header.h" />
          <output type="resource_map_source"
                  filename="the_resource_map_header.cc" />
        </outputs>
        <release seq="3">
          <structures first_id="300">
            <structure type="chrome_scaled_image" name="IDR_KLONKMENU"
                       file="foo.png" />
            <if expr="False">
              <structure type="chrome_scaled_image" name="IDR_MISSING"
                         file="bar.png" />
            </if>
            <if expr="True">
              <structure type="chrome_scaled_image" name="IDR_BLOB"
                         file="blob.png" />
            </if>
            <if expr="True">
              <then>
                <structure type="chrome_scaled_image" name="IDR_METEOR"
                           file="meteor.png" />
              </then>
              <else>
                <structure type="chrome_scaled_image" name="IDR_METEOR"
                           file="roetem.png" />
              </else>
            </if>
            <if expr="False">
              <structure type="chrome_scaled_image" name="IDR_LAST"
                         file="zyx.png" />
            </if>
            <if expr="True">
              <structure type="chrome_scaled_image" name="IDR_LAST"
                         file="xyz.png" />
            </if>
         </structures>
        </release>''', run_gatherers=True)
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_map_header')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include <stddef.h>
#include "ui/base/webui/resource_path.h"
extern const webui::ResourcePath kTheRcHeader[4];
extern const size_t kTheRcHeaderSize;''', output)
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_map_source')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include "the_resource_map_header.h"
#include <stddef.h>
#include <iterator>
#include "the_rc_header.h"
const webui::ResourcePath kTheRcHeader[4] = {
  {"IDR_KLONKMENU", IDR_KLONKMENU},
  {"IDR_BLOB", IDR_BLOB},
  {"IDR_METEOR", IDR_METEOR},
  {"IDR_LAST", IDR_LAST},
};
const size_t kTheRcHeaderSize = std::size(kTheRcHeader);''', output)
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_map_source')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include "the_resource_map_header.h"
#include <stddef.h>
#include <iterator>
#include "the_rc_header.h"
const webui::ResourcePath kTheRcHeader[4] = {
  {"IDR_KLONKMENU", IDR_KLONKMENU},
  {"IDR_BLOB", IDR_BLOB},
  {"IDR_METEOR", IDR_METEOR},
  {"IDR_LAST", IDR_LAST},
};
const size_t kTheRcHeaderSize = std::size(kTheRcHeader);''', output)

  def testFormatResourceMapWithOutputAllEqualsFalseForIncludes(self):
    grd = util.ParseGrdForUnittest('''
        <outputs>
          <output type="rc_header" filename="the_rc_header.h" />
          <output type="resource_map_header"
                  filename="the_resource_map_header.h" />
        </outputs>
        <release seq="3">
          <structures first_id="300">
            <structure type="menu" name="IDC_KLONKMENU"
                       file="grit\\testdata\\klonk.rc" encoding="utf-16" />
          </structures>
          <includes first_id="10000">
            <include type="foo" file="abc" name="IDS_FIRSTPRESENT" />
            <if expr="False">
              <include type="foo" file="def" name="IDS_MISSING" />
            </if>
            <include type="foo" file="mno" name="IDS_THIRDPRESENT" />
            <if expr="True">
              <include type="foo" file="blob" name="IDS_BLOB" />
            </if>
            <if expr="True">
              <then>
                <include type="foo" file="meteor" name="IDS_METEOR" />
              </then>
              <else>
                <include type="foo" file="roetem" name="IDS_METEOR" />
              </else>
            </if>
            <if expr="False">
              <include type="foo" file="zyx" name="IDS_LAST" />
            </if>
            <if expr="True">
              <include type="foo" file="xyz" name="IDS_LAST" />
            </if>
         </includes>
        </release>''', run_gatherers=True)
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_map_header')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include <stddef.h>
#include "ui/base/webui/resource_path.h"
extern const webui::ResourcePath kTheRcHeader[6];
extern const size_t kTheRcHeaderSize;''', output)
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_map_source')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include "the_resource_map_header.h"
#include <stddef.h>
#include <iterator>
#include "the_rc_header.h"
const webui::ResourcePath kTheRcHeader[6] = {
  {"IDC_KLONKMENU", IDC_KLONKMENU},
  {"IDS_FIRSTPRESENT", IDS_FIRSTPRESENT},
  {"IDS_THIRDPRESENT", IDS_THIRDPRESENT},
  {"IDS_BLOB", IDS_BLOB},
  {"IDS_METEOR", IDS_METEOR},
  {"IDS_LAST", IDS_LAST},
};
const size_t kTheRcHeaderSize = std::size(kTheRcHeader);''', output)
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_file_map_source')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include "the_resource_map_header.h"
#include <stddef.h>
#include <iterator>
#include "the_rc_header.h"
const webui::ResourcePath kTheRcHeader[6] = {
  {"grit/testdata/klonk.rc", IDC_KLONKMENU},
  {"abc", IDS_FIRSTPRESENT},
  {"mno", IDS_THIRDPRESENT},
  {"blob", IDS_BLOB},
  {"meteor", IDS_METEOR},
  {"xyz", IDS_LAST},
};
const size_t kTheRcHeaderSize = std::size(kTheRcHeader);''', output)

  def testFormatStringResourceMap(self):
    grd = util.ParseGrdForUnittest('''
        <outputs>
          <output type="rc_header" filename="the_rc_header.h" />
          <output type="resource_map_header" filename="the_rc_map_header.h" />
          <output type="resource_map_source" filename="the_rc_map_source.cc" />
        </outputs>
        <release seq="1" allow_pseudo="false">
          <messages fallback_to_english="true">
            <message name="IDS_PRODUCT_NAME" desc="The application name">
              Application
            </message>
            <if expr="True">
              <message name="IDS_DEFAULT_TAB_TITLE_TITLE_CASE"
                  desc="In Title Case: The default title in a tab.">
                New Tab
              </message>
            </if>
            <if expr="False">
              <message name="IDS_DEFAULT_TAB_TITLE"
                  desc="The default title in a tab.">
                New tab
              </message>
            </if>
          </messages>
        </release>''', run_gatherers=True)
    grd.InitializeIds()
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_map_header')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include <stddef.h>
#include "ui/base/webui/resource_path.h"
extern const webui::ResourcePath kTheRcHeader[2];
extern const size_t kTheRcHeaderSize;''', output)
    output = util.StripBlankLinesAndComments(''.join(
        resource_map.GetFormatter('resource_map_source')(grd, 'en', '.')))
    self.assertEqual(
        '''\
#include "the_rc_map_header.h"
#include <stddef.h>
#include <iterator>
#include "the_rc_header.h"
const webui::ResourcePath kTheRcHeader[2] = {
  {"IDS_PRODUCT_NAME", IDS_PRODUCT_NAME},
  {"IDS_DEFAULT_TAB_TITLE_TITLE_CASE", IDS_DEFAULT_TAB_TITLE_TITLE_CASE},
};
const size_t kTheRcHeaderSize = std::size(kTheRcHeader);''', output)


if __name__ == '__main__':
  unittest.main()
