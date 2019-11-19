#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from json_schema import CachedLoad
from idl_schema import Load
from model import Platforms
import model
import unittest

class ModelTest(unittest.TestCase):
  def setUp(self):
    self.model = model.Model()
    self.permissions_json = CachedLoad('test/permissions.json')
    self.model.AddNamespace(self.permissions_json[0],
        'path/to/permissions.json')
    self.permissions = self.model.namespaces.get('permissions')
    self.windows_json = CachedLoad('test/windows.json')
    self.model.AddNamespace(self.windows_json[0],
        'path/to/window.json')
    self.windows = self.model.namespaces.get('windows')
    self.tabs_json = CachedLoad('test/tabs.json')
    self.model.AddNamespace(self.tabs_json[0],
        'path/to/tabs.json')
    self.tabs = self.model.namespaces.get('tabs')
    self.idl_chromeos = Load('test/idl_namespace_chromeos.idl')
    self.model.AddNamespace(self.idl_chromeos[0],
        'path/to/idl_namespace_chromeos.idl')
    self.idl_namespace_chromeos = self.model.namespaces.get(
        'idl_namespace_chromeos')
    self.idl_all_platforms = Load('test/idl_namespace_all_platforms.idl')
    self.model.AddNamespace(self.idl_all_platforms[0],
        'path/to/idl_namespace_all_platforms.idl')
    self.idl_namespace_all_platforms = self.model.namespaces.get(
        'idl_namespace_all_platforms')
    self.idl_non_specific_platforms = Load(
        'test/idl_namespace_non_specific_platforms.idl')
    self.model.AddNamespace(self.idl_non_specific_platforms[0],
        'path/to/idl_namespace_non_specific_platforms.idl')
    self.idl_namespace_non_specific_platforms = self.model.namespaces.get(
        'idl_namespace_non_specific_platforms')

  def testNamespaces(self):
    self.assertEquals(6, len(self.model.namespaces))
    self.assertTrue(self.permissions)

  def testHasFunctions(self):
    self.assertEquals(["contains", "getAll", "remove", "request"],
        sorted(self.permissions.functions.keys()))

  def testHasTypes(self):
    self.assertEquals(['Tab'], self.tabs.types.keys())
    self.assertEquals(['Permissions'], self.permissions.types.keys())
    self.assertEquals(['Window'], self.windows.types.keys())

  def testHasProperties(self):
    self.assertEquals(["active", "favIconUrl", "highlighted", "id",
        "incognito", "index", "pinned", "selected", "status", "title", "url",
        "windowId"],
        sorted(self.tabs.types['Tab'].properties.keys()))

  def testProperties(self):
    string_prop = self.tabs.types['Tab'].properties['status']
    self.assertEquals(model.PropertyType.STRING,
                      string_prop.type_.property_type)
    integer_prop = self.tabs.types['Tab'].properties['id']
    self.assertEquals(model.PropertyType.INTEGER,
                      integer_prop.type_.property_type)
    array_prop = self.windows.types['Window'].properties['tabs']
    self.assertEquals(model.PropertyType.ARRAY,
                      array_prop.type_.property_type)
    self.assertEquals(model.PropertyType.REF,
                      array_prop.type_.item_type.property_type)
    self.assertEquals('tabs.Tab', array_prop.type_.item_type.ref_type)
    object_prop = self.tabs.functions['query'].params[0]
    self.assertEquals(model.PropertyType.OBJECT,
                      object_prop.type_.property_type)
    self.assertEquals(
        ["active", "highlighted", "pinned", "status", "title", "url",
         "windowId", "windowType"],
        sorted(object_prop.type_.properties.keys()))

  def testChoices(self):
    self.assertEquals(model.PropertyType.CHOICES,
                      self.tabs.functions['move'].params[0].type_.property_type)

  def testPropertyNotImplemented(self):
    (self.permissions_json[0]['types'][0]
        ['properties']['permissions']['type']) = 'something'
    self.assertRaises(model.ParseException, self.model.AddNamespace,
        self.permissions_json[0], 'path/to/something.json')

  def testDescription(self):
    self.assertFalse(
        self.permissions.functions['contains'].params[0].description)
    self.assertEquals('True if the extension has the specified permissions.',
        self.permissions.functions['contains'].callback.params[0].description)

  def testPropertyUnixName(self):
    param = self.tabs.functions['move'].params[0]
    self.assertEquals('tab_ids', param.unix_name)

  def testUnixName(self):
    expectations = {
      'foo': 'foo',
      'fooBar': 'foo_bar',
      'fooBarBaz': 'foo_bar_baz',
      'fooBARBaz': 'foo_bar_baz',
      'fooBAR': 'foo_bar',
      'FOO': 'foo',
      'FOOBar': 'foo_bar',
      'foo.bar': 'foo_bar',
      'foo.BAR': 'foo_bar',
      'foo.barBAZ': 'foo_bar_baz',
      'foo_Bar_Baz_box': 'foo_bar_baz_box',
      }
    for name in expectations:
      self.assertEquals(expectations[name], model.UnixName(name))

  def testCamelName(self):
    expectations = {
      'foo': 'foo',
      'fooBar': 'fooBar',
      'foo_bar_baz': 'fooBarBaz',
      'FOO_BAR': 'FOOBar',
      'FOO_bar': 'FOOBar',
      '_bar': 'Bar',
      '_bar_baz': 'BarBaz',
      'bar_': 'bar',
      'bar_baz_': 'barBaz',
      }
    for testcase, expected in expectations.items():
      self.assertEquals(expected, model.CamelName(testcase))

  def testPlatforms(self):
    self.assertEqual([Platforms.CHROMEOS],
                     self.idl_namespace_chromeos.platforms)
    self.assertEqual(
        [Platforms.CHROMEOS, Platforms.CHROMEOS_TOUCH, Platforms.LINUX,
         Platforms.MAC, Platforms.WIN],
        self.idl_namespace_all_platforms.platforms)
    self.assertEqual(None,
        self.idl_namespace_non_specific_platforms.platforms)

if __name__ == '__main__':
  unittest.main()
