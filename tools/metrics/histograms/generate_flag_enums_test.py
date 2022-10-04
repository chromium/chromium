#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import generate_flag_enums

ENUMS_XML_CONTAINING_XY = """
<histogram-configuration>

<enums>

<enum name="LoginCustomFlags">
  <int value="-2137051209" label="y:enabled"/>
  <int value="-1661325532" label="y:disabled"/>
  <int value="-1441640912" label="x:enabled"/>
  <int value="-894241176" label="x:disabled"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

ENUMS_XML_CONTAINING_XYZ = """
<histogram-configuration>

<enums>

<enum name="LoginCustomFlags">
  <int value="-2137768499" label="z:disabled"/>
  <int value="-2137051209" label="y:enabled"/>
  <int value="-1661325532" label="y:disabled"/>
  <int value="-1441640912" label="x:enabled"/>
  <int value="-894241176" label="x:disabled"/>
  <int value="1073077752" label="z:enabled"/>
</enum>

</enums>

</histogram-configuration>
""".strip()

ENTRIES_Z = [
    '<int value="-2137768499" label="z:disabled"/>',
    '<int value="1073077752" label="z:enabled"/>',
]
ENTRIES_X = [
    '<int value="-894241176" label="x:disabled"/>',
    '<int value="-1441640912" label="x:enabled"/>',
]


class GenerateFlagEnumsTest(unittest.TestCase):
  def test_get_entries_from_feature_string(self):
    entries = generate_flag_enums.get_entries_from_feature_string('x')
    self.assertListEqual(entries, ENTRIES_X)

  def test_add_entries_to_xml(self):
    # Should not add entries already in the enums xml.
    output_xml = generate_flag_enums.add_entries_to_xml(ENUMS_XML_CONTAINING_XY,
                                                        ENTRIES_X)
    self.assertMultiLineEqual(output_xml.strip(), ENUMS_XML_CONTAINING_XY)

    # Should add entries not in the enums xml and order them.
    output_xml = generate_flag_enums.add_entries_to_xml(ENUMS_XML_CONTAINING_XY,
                                                        ENTRIES_Z)
    self.assertMultiLineEqual(output_xml.strip(), ENUMS_XML_CONTAINING_XYZ)


if __name__ == '__main__':
  unittest.main()
