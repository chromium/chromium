#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
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
    self.model.AddNamespace(self.windows_json[0], 'path/to/window.json')
    self.windows = self.model.namespaces.get('windows')
    self.tabs_json = CachedLoad('test/tabs.json')
    self.model.AddNamespace(self.tabs_json[0], 'path/to/tabs.json')
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
    self.returns_async_json = CachedLoad('test/returns_async.json')
    self.model.AddNamespace(self.returns_async_json[0],
                            'path/to/returns_async.json')
    self.returns_async = self.model.namespaces.get('returns_async')
    self.idl_returns_async_idl = Load('test/idl_returns_async.idl')
    self.model.AddNamespace(self.idl_returns_async_idl[0],
                            'path/to/idl_returns_async.idl')
    self.idl_returns_async = self.model.namespaces.get('idl_returns_async')
    self.nodoc_json = CachedLoad('test/namespace_nodoc.json')
    self.model.AddNamespace(self.nodoc_json[0], 'path/to/namespace_nodoc.json')
    self.nodoc = self.model.namespaces.get('nodoc')
    self.fakeapi_json = CachedLoad('test/namespace_fakeapi.json')
    self.model.AddNamespace(self.fakeapi_json[0],
                            'path/to/namespace_fakeapi.json')
    self.fakeapi = self.model.namespaces.get('fakeapi')

    self.function_platforms_idl = Load('test/function_platforms.idl')
    self.model.AddNamespace(self.function_platforms_idl[0],
                            '/path/to/function_platforms.idl')
    self.function_platforms = self.model.namespaces.get('function_platforms')

    self.function_platform_win_linux_json = CachedLoad(
        'test/function_platform_win_linux.json')
    self.model.AddNamespace(self.function_platform_win_linux_json[0],
                            'path/to/function_platform_win_linux.json')
    self.function_platform_win_linux = self.model.namespaces.get(
        'function_platform_win_linux')

  def testNamespaces(self):
    self.assertEqual(12, len(self.model.namespaces))
    self.assertTrue(self.permissions)

  def testHasFunctions(self):
    self.assertEqual(["contains", "getAll", "remove", "request"],
                     sorted(self.permissions.functions.keys()))

  def testHasTypes(self):
    self.assertEqual(['Tab'], list(self.tabs.types.keys()))
    self.assertEqual(['Permissions'], list(self.permissions.types.keys()))
    self.assertEqual(['Window'], list(self.windows.types.keys()))

  def testHasProperties(self):
    self.assertEqual([
        "active", "favIconUrl", "highlighted", "id", "incognito", "index",
        "pinned", "selected", "status", "title", "url", "windowId"
    ], sorted(self.tabs.types['Tab'].properties.keys()))

  def testProperties(self):
    string_prop = self.tabs.types['Tab'].properties['status']
    self.assertEqual(model.PropertyType.STRING, string_prop.type_.property_type)
    integer_prop = self.tabs.types['Tab'].properties['id']
    self.assertEqual(model.PropertyType.INTEGER,
                     integer_prop.type_.property_type)
    array_prop = self.windows.types['Window'].properties['tabs']
    self.assertEqual(model.PropertyType.ARRAY, array_prop.type_.property_type)
    self.assertEqual(model.PropertyType.REF,
                     array_prop.type_.item_type.property_type)
    self.assertEqual('tabs.Tab', array_prop.type_.item_type.ref_type)
    object_prop = self.tabs.functions['query'].params[0]
    self.assertEqual(model.PropertyType.OBJECT, object_prop.type_.property_type)
    self.assertEqual([
        "active", "highlighted", "pinned", "status", "title", "url", "windowId",
        "windowType"
    ], sorted(object_prop.type_.properties.keys()))

  def testChoices(self):
    self.assertEqual(model.PropertyType.CHOICES,
                     self.tabs.functions['move'].params[0].type_.property_type)

  def testPropertyNotImplemented(self):
    (self.permissions_json[0]['types'][0]['properties']['permissions']['type']
     ) = 'something'
    self.assertRaises(model.ParseException, self.model.AddNamespace,
                      self.permissions_json[0], 'path/to/something.json')

  def testDefaultSpecifiedRedundantly(self):
    test_json = CachedLoad('test/redundant_default_attribute.json')
    self.assertRaisesRegex(
        model.ParseException,
        'Model parse exception at:\nredundantDefaultAttribute\noptionalFalse\n'
        '  in path/to/redundant_default_attribute.json\n'
        'The attribute "optional" is specified as "False", but this is the '
        'default value if the attribute is not included\. It should be '
        'removed\.', self.model.AddNamespace, test_json[0],
        'path/to/redundant_default_attribute.json')

  def testReturnsAsyncMissingParametersKey(self):
    test_json = CachedLoad('test/returns_async_missing_parameters_key.json')
    self.assertRaisesRegex(
        ValueError, 'parameters key not specified on returns_async: '
        'returnsAsyncMissingParametersKey.asyncNoParametersKey in '
        'path/to/returns_async_missing_parameters_key.json',
        self.model.AddNamespace, test_json[0],
        'path/to/returns_async_missing_parameters_key.json')

  def testDescription(self):
    self.assertFalse(
        self.permissions.functions['contains'].params[0].description)
    self.assertEqual(
        'True if the extension has the specified permissions.', self.
        permissions.functions['contains'].returns_async.params[0].description)

  def testAsyncPromise(self):
    supportsPromises = self.returns_async.functions['supportsPromises']
    self.assertTrue(supportsPromises.returns_async.can_return_promise)
    doesNotSupportPromises = self.returns_async.functions[
        'doesNotSupportPromises']
    self.assertFalse(doesNotSupportPromises.returns_async.can_return_promise)
    supportsPromisesIdl = self.idl_returns_async.functions['supportsPromises']
    self.assertTrue(supportsPromisesIdl.returns_async.can_return_promise)
    doesNotSupportPromisesIdl = self.idl_returns_async.functions[
        'doesNotSupportPromises']
    self.assertFalse(doesNotSupportPromisesIdl.returns_async.can_return_promise)

  def testPropertyUnixName(self):
    param = self.tabs.functions['move'].params[0]
    self.assertEqual('tab_ids', param.unix_name)

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
      self.assertEqual(expectations[name], model.UnixName(name))

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
      self.assertEqual(expected, model.CamelName(testcase))

  def testPlatforms(self):
    self.assertEqual([Platforms.CHROMEOS],
                     self.idl_namespace_chromeos.platforms)
    self.assertEqual([
        Platforms.CHROMEOS, Platforms.FUCHSIA, Platforms.LINUX, Platforms.MAC,
        Platforms.WIN
    ], self.idl_namespace_all_platforms.platforms)
    self.assertEqual(None, self.idl_namespace_non_specific_platforms.platforms)

  def testInvalidNamespacePlatform(self):
    invalid_namespace_platform = Load('test/invalid_platform_namespace.idl')
    with self.assertRaises(ValueError) as context:
      self.model.AddNamespace(invalid_namespace_platform[0],
                              'path/to/something.json')
    self.assertIn('Invalid platform specified: invalid', str(context.exception))

  def testInvalidFunctionPlatform(self):
    invalid_function_platform = Load('test/invalid_function_platform.idl')
    with self.assertRaises(ValueError) as context:
      self.model.AddNamespace(invalid_function_platform[0],
                              'path/to/something.json')
    self.assertIn('Invalid platform specified: windows', str(context.exception))

  def testPlatformsOnFunctionsIDL(self):
    function_win_linux = self.function_platforms.functions['function_win_linux']
    self.assertEqual([Platforms.WIN, Platforms.LINUX],
                     function_win_linux.platforms)

    function_all = self.function_platforms.functions['function_all']
    self.assertIsNone(function_all.platforms)

    function_cros = self.function_platforms.functions['function_cros']
    self.assertEqual([Platforms.CHROMEOS], function_cros.platforms)

    function_fuchsia = self.function_platforms.functions['function_fuchsia']
    self.assertEqual([Platforms.FUCHSIA], function_fuchsia.platforms)

  def testPlatformsOnFunctionsJSON(self):
    test_function = self.function_platform_win_linux.functions['test']
    self.assertEqual([Platforms.WIN, Platforms.LINUX], test_function.platforms)

  def testHasNoDoc(self):
    fakeapi_NoDocType = self.fakeapi.types['NoDocType']
    self.assertTrue(fakeapi_NoDocType.nodoc)

    fakeapi_FakeType = self.fakeapi.types['FakeType']
    selected_property = fakeapi_FakeType.properties['nodocProperty']
    self.assertTrue(selected_property.nodoc)

    nodocMethod_method = self.fakeapi.functions['nodocMethod']
    self.assertTrue(nodocMethod_method.nodoc)

    onFooNoDoc_event = self.fakeapi.events['onFooNoDoc']
    self.assertTrue(onFooNoDoc_event.nodoc)

    onFoo_event = self.fakeapi.events['onFoo']
    self.assertFalse(onFoo_event.nodoc)

    self.assertTrue(self.nodoc.nodoc, 'Namespace should also be marked nodoc')
    nodoc_ValidType = self.nodoc.types['ValidType']
    self.assertFalse(nodoc_ValidType.nodoc)

  def testInvalidNamespacePlatform(self):
    invalid_namespace_platform = CachedLoad('test/invalid_empty_enum_key.json')
    with self.assertRaises(ValueError) as context:
      self.model.AddNamespace(invalid_namespace_platform[0],
                              'path/to/invalid_empty_enum_key.json')
    self.assertIn('Enum value cannot be an empty string',
                  str(context.exception))


if __name__ == '__main__':
  unittest.main()
