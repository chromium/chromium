#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import feature_compiler
import unittest

class FeatureCompilerTest(unittest.TestCase):
  """Test the FeatureCompiler. Note that we test that the expected features are
  generated more thoroughly in features_generation_unittest.cc. And, of course,
  this is most exhaustively tested through Chrome's compilation process (if a
  feature fails to parse, the compile fails).
  These tests primarily focus on catching errors during parsing.
  """
  def _parseFeature(self, value):
    """Parses a feature from the given value and returns the result."""
    f = feature_compiler.Feature('alpha')
    f.Parse(value, {})
    return f

  def _createTestFeatureCompiler(self, feature_class):
    return feature_compiler.FeatureCompiler('chrome_root', [], feature_class,
        'provider_class', 'out_root', 'out_base_filename')

  def _hasError(self, f, error):
    """Asserts that |error| is present somewhere in the given feature's
    errors."""
    errors = f.GetErrors()
    self.assertTrue(errors)
    self.assertNotEqual(-1, str(errors).find(error), str(errors))

  def setUp(self):
    feature_compiler.ENABLE_ASSERTIONS = False

  def testFeature(self):
    # Test some basic feature parsing for a sanity check.
    f = self._parseFeature({
      'blacklist': [
        'ABCDEF0123456789ABCDEF0123456789ABCDEF01',
        '10FEDCBA9876543210FEDCBA9876543210FEDCBA'
      ],
      'channel': 'stable',
      'command_line_switch': 'switch',
      'component_extensions_auto_granted': False,
      'contexts': [
        'blessed_extension',
        'blessed_web_page',
        'lock_screen_extension'
      ],
      'default_parent': True,
      'dependencies': ['dependency1', 'dependency2'],
      'disallow_for_service_workers': True,
      'extension_types': ['extension'],
      'location': 'component',
      'internal': True,
      'matches': ['*://*/*'],
      'max_manifest_version': 1,
      'noparent': True,
      'platforms': ['mac', 'win'],
      'session_types': ['kiosk', 'regular'],
      'whitelist': [
        '0123456789ABCDEF0123456789ABCDEF01234567',
        '76543210FEDCBA9876543210FEDCBA9876543210'
      ]
    })
    self.assertFalse(f.GetErrors())

  def testInvalidAll(self):
    f = self._parseFeature({
      'channel': 'stable',
      'dependencies': 'all',
    })
    self._hasError(f, 'Illegal value: "all"')

  def testUnknownKeyError(self):
    f = self._parseFeature({
      'contexts': ['blessed_extension'],
      'channel': 'stable',
      'unknownkey': 'unknownvalue'
    })
    self._hasError(f, 'Unrecognized key')

  def testUnknownEnumValue(self):
    f = self._parseFeature({
      'contexts': ['blessed_extension', 'unknown_context'],
      'channel': 'stable'
    })
    self._hasError(f, 'Illegal value: "unknown_context"')

  def testImproperType(self):
    f = self._parseFeature({'min_manifest_version': '1'})
    self._hasError(f, 'Illegal value: "1"')

  def testImproperSubType(self):
    f = self._parseFeature({'dependencies': [1, 2, 3]})
    self._hasError(f, 'Illegal value: "1"')

  def testImproperValue(self):
    f = self._parseFeature({'noparent': False})
    self._hasError(f, 'Illegal value: "False"')

  def testEmptyList(self):
    f = self._parseFeature({'contexts': []})
    self._hasError(f, 'List must specify at least one element.')

  def testEmptyListWithAllowEmpty(self):
    # `dependencies` is the only key that allows an empty list.
    f = self._parseFeature({'dependencies': []})
    self.assertFalse(f.GetErrors())

  def testApiFeaturesNeedContexts(self):
    f = self._parseFeature({'dependencies': 'alpha',
                            'extension_types': ['extension'],
                            'channel': 'trunk'})
    f.Validate('APIFeature', {})
    self._hasError(f, 'APIFeatures must specify at least one context')

  def testManifestFeaturesNeedExtensionTypes(self):
    f = self._parseFeature({'dependencies': 'alpha', 'channel': 'beta'})
    f.Validate('ManifestFeature', {})
    self._hasError(f,
                   'ManifestFeatures must specify at least one extension type')

  def testManifestFeaturesCantHaveContexts(self):
    f = self._parseFeature({'dependencies': 'alpha',
                            'channel': 'beta',
                            'extension_types': ['extension'],
                            'contexts': ['blessed_extension']})
    f.Validate('ManifestFeature', {})
    self._hasError(f, 'ManifestFeatures do not support contexts')

  def testPermissionFeaturesNeedExtensionTypes(self):
    f = self._parseFeature({'dependencies': 'alpha', 'channel': 'beta'})
    f.Validate('PermissionFeature', {})
    self._hasError(
        f, 'PermissionFeatures must specify at least one extension type')

  def testPermissionFeaturesCantHaveContexts(self):
    f = self._parseFeature({'dependencies': 'alpha',
                            'channel': 'beta',
                            'extension_types': ['extension'],
                            'contexts': ['blessed_extension']})
    f.Validate('PermissionFeature', {})
    self._hasError(f, 'PermissionFeatures do not support contexts')

  def testAllPermissionsNeedChannelOrDependencies(self):
    api_feature = self._parseFeature({'contexts': ['blessed_extension']})
    api_feature.Validate('APIFeature', {})
    self._hasError(
        api_feature, 'Features must specify either a channel or dependencies')
    permission_feature = self._parseFeature({'extension_types': ['extension']})
    permission_feature.Validate('PermissionFeature', {})
    self._hasError(permission_feature,
                   'Features must specify either a channel or dependencies')
    manifest_feature = self._parseFeature({'extension_types': ['extension']})
    manifest_feature.Validate('ManifestFeature', {})
    self._hasError(manifest_feature,
                   'Features must specify either a channel or dependencies')
    channel_feature = self._parseFeature({'contexts': ['blessed_extension'],
                                          'channel': 'trunk'})
    channel_feature.Validate('APIFeature', {})
    self.assertFalse(channel_feature.GetErrors())
    dependency_feature = self._parseFeature(
                             {'contexts': ['blessed_extension'],
                              'dependencies': ['alpha']})
    dependency_feature.Validate('APIFeature', {})
    self.assertFalse(dependency_feature.GetErrors())

  def testBothAliasAndSource(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
      'feature_alpha': {
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'alias': 'feature_alpha',
        'source': 'feature_alpha'
      }
    }
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self._hasError(feature, 'Features cannot specify both alias and source.')

  def testAliasOnNonApiFeature(self):
    compiler = self._createTestFeatureCompiler('PermissionFeature')
    compiler._json = {
      'feature_alpha': {
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'alias': 'feature_beta'
      },
      'feature_beta': [{
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'source': 'feature_alpha'
      },{
        'channel': 'dev',
        'context': ['blessed_extension']
      }]
    };
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self._hasError(feature, 'PermissionFeatures do not support alias.')

    feature = compiler._features.get('feature_beta')
    self.assertTrue(feature)
    self._hasError(feature, 'PermissionFeatures do not support source.')

  def testAliasFeature(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
      'feature_alpha': {
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'alias': 'feature_beta'
      },
      'feature_beta': {
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'source': 'feature_alpha'
      }
    };
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self.assertFalse(feature.GetErrors())

    feature = compiler._features.get('feature_beta')
    self.assertTrue(feature)
    self.assertFalse(feature.GetErrors())

  def testMultipleAliasesInComplexFeature(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
      'feature_alpha': [{
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'alias': 'feature_beta'
      }, {
        'contexts': ['blessed_extension'],
        'channel': 'beta',
        'alias': 'feature_beta'
      }]
    };
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self._hasError(feature, 'Error parsing feature "feature_alpha" at key ' +
                            '"alias": Key can be set at most once per feature.')

  def testAliasReferenceInComplexFeature(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
      'feature_alpha': [{
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'alias': 'feature_beta'
      }, {
        'contexts': ['blessed_extension'],
        'channel': 'beta',
      }],
      'feature_beta': {
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'source': 'feature_alpha'
      }
    };
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self.assertFalse(feature.GetErrors())

    feature = compiler._features.get('feature_beta')
    self.assertTrue(feature)
    self.assertFalse(feature.GetErrors())

  def testSourceMissingReference(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
      'feature_alpha': {
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'alias': 'feature_beta'
      },
      'feature_beta': {
        'contexts': ['blessed_extension'],
        'channel': 'beta',
        'source': 'does_not_exist'
      }
    };
    compiler.Compile()

    feature = compiler._features.get('feature_beta')
    self.assertTrue(feature)
    self._hasError(feature, 'A feature source property should reference a ' +
                            'feature whose alias property references it back.')


  def testAliasMissingReferenceInComplexFeature(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
      'feature_alpha': [{
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'alias': 'feature_beta'
      }, {
        'contexts': ['blessed_extension'],
        'channel': 'beta'
      }]
    };
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self._hasError(feature, 'A feature alias property should reference a ' +
                            'feature whose source property references it back.')

  def testAliasReferenceMissingSourceInComplexFeature(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
      'feature_alpha': {
        'contexts': ['blessed_extension'],
        'channel': 'beta',
      },
      'feature_beta': {
        'channel': 'beta',
        'contexts': ['blessed_extension'],
        'alias': 'feature_alpha'
      }
    };
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self.assertFalse(feature.GetErrors())

    feature = compiler._features.get('feature_beta')
    self.assertTrue(feature)
    self._hasError(feature, 'A feature alias property should reference a ' +
                            'feature whose source property references it back.')

  def testComplexParentWithoutDefaultParent(self):
    c = feature_compiler.FeatureCompiler(
        None, None, 'APIFeature', None, None, None)
    c._CompileFeature('bookmarks',
        [{
          'contexts': ['blessed_extension'],
        }, {
          'channel': 'stable',
          'contexts': ['webui'],
        }])

    with self.assertRaisesRegexp(AssertionError,
                                 'No default parent found for bookmarks'):
      c._CompileFeature('bookmarks.export', { "whitelist": ["asdf"] })

  def testRealIdsDisallowedInWhitelist(self):
    fake_id = 'a' * 32;
    f = self._parseFeature({'whitelist': [fake_id],
                            'extension_types': ['extension'],
                            'channel': 'beta'})
    f.Validate('PermissionFeature', {})
    self._hasError(
        f, 'list should only have hex-encoded SHA1 hashes of extension ids')


if __name__ == '__main__':
  unittest.main()
