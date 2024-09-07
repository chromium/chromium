#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
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
                                            'provider_class', 'out_root', 'gen',
                                            'out_base_filename')

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
        'blocklist': [
            'ABCDEF0123456789ABCDEF0123456789ABCDEF01',
            '10FEDCBA9876543210FEDCBA9876543210FEDCBA'
        ],
        'channel':
        'stable',
        'command_line_switch':
        'switch',
        'component_extensions_auto_granted':
        False,
        'contexts': [
            'privileged_extension', 'privileged_web_page',
            'lock_screen_extension'
        ],
        'default_parent':
        True,
        'dependencies': ['dependency1', 'dependency2'],
        'developer_mode_only':
        True,
        'disallow_for_service_workers':
        True,
        'extension_types': ['extension'],
        'location':
        'component',
        'internal':
        True,
        'matches': ['*://*/*'],
        'max_manifest_version':
        1,
        'requires_delegated_availability_check':
        True,
        'noparent':
        True,
        'platforms': ['mac', 'win'],
        'session_types': ['kiosk', 'regular'],
        'allowlist': [
            '0123456789ABCDEF0123456789ABCDEF01234567',
            '76543210FEDCBA9876543210FEDCBA9876543210'
        ],
        'required_buildflags': ['use_cups']
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
        'contexts': ['privileged_extension'],
        'channel': 'stable',
        'unknownkey': 'unknownvalue'
    })
    self._hasError(f, 'Unrecognized key')

  def testUnknownEnumValue(self):
    f = self._parseFeature({
        'contexts': ['privileged_extension', 'unknown_context'],
        'channel':
        'stable'
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
    f = self._parseFeature({'extension_types': []})
    self._hasError(f, 'List must specify at least one element.')

  def testEmptyListWithAllowEmpty(self):
    # `dependencies` is the only key that allows an empty list.
    f = self._parseFeature({'dependencies': []})
    self.assertFalse(f.GetErrors())

  def testApiFeaturesNeedContexts(self):
    f = self._parseFeature({
        'extension_types': ['extension'],
        'channel': 'trunk'
    })
    f.Validate('APIFeature', {})
    self._hasError(f, 'APIFeatures must specify the contexts property')

  def testAPIFeaturesCanSpecifyEmptyContexts(self):
    f = self._parseFeature({
        'extension_types': ['extension'],
        'channel': 'trunk',
        'contexts': []
    })
    f.Validate('APIFeature', {})
    self.assertFalse(f.GetErrors())

  def testManifestFeaturesNeedExtensionTypes(self):
    f = self._parseFeature({'dependencies': 'alpha', 'channel': 'beta'})
    f.Validate('ManifestFeature', {})
    self._hasError(f,
                   'ManifestFeatures must specify at least one extension type')

  def testManifestFeaturesCantHaveContexts(self):
    f = self._parseFeature({
        'dependencies': 'alpha',
        'channel': 'beta',
        'extension_types': ['extension'],
        'contexts': ['privileged_extension']
    })
    f.Validate('ManifestFeature', {})
    self._hasError(f, 'ManifestFeatures do not support contexts')

  def testPermissionFeaturesNeedExtensionTypes(self):
    f = self._parseFeature({'dependencies': 'alpha', 'channel': 'beta'})
    f.Validate('PermissionFeature', {})
    self._hasError(
        f, 'PermissionFeatures must specify at least one extension type')

  def testPermissionFeaturesCantHaveContexts(self):
    f = self._parseFeature({
        'dependencies': 'alpha',
        'channel': 'beta',
        'extension_types': ['extension'],
        'contexts': ['privileged_extension']
    })
    f.Validate('PermissionFeature', {})
    self._hasError(f, 'PermissionFeatures do not support contexts')

  def testAllPermissionsNeedChannelOrDependencies(self):
    api_feature = self._parseFeature({'contexts': ['privileged_extension']})
    api_feature.Validate('APIFeature', {})
    self._hasError(api_feature,
                   'Features must specify either a channel or dependencies')
    permission_feature = self._parseFeature({'extension_types': ['extension']})
    permission_feature.Validate('PermissionFeature', {})
    self._hasError(permission_feature,
                   'Features must specify either a channel or dependencies')
    manifest_feature = self._parseFeature({'extension_types': ['extension']})
    manifest_feature.Validate('ManifestFeature', {})
    self._hasError(manifest_feature,
                   'Features must specify either a channel or dependencies')
    channel_feature = self._parseFeature({
        'contexts': ['privileged_extension'],
        'channel': 'trunk'
    })
    channel_feature.Validate('APIFeature', {})
    self.assertFalse(channel_feature.GetErrors())
    dependency_feature = self._parseFeature({
        'contexts': ['privileged_extension'],
        'dependencies': ['alpha']
    })
    dependency_feature.Validate('APIFeature', {})
    self.assertFalse(dependency_feature.GetErrors())

  def testBothAliasAndSource(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
        'feature_alpha': {
            'channel': 'beta',
            'contexts': ['privileged_extension'],
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
            'contexts': ['privileged_extension'],
            'alias': 'feature_beta'
        },
        'feature_beta': [{
            'channel': 'beta',
            'contexts': ['privileged_extension'],
            'source': 'feature_alpha'
        }, {
            'channel': 'dev',
            'context': ['privileged_extension']
        }]
    }
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
            'contexts': ['privileged_extension'],
            'alias': 'feature_beta'
        },
        'feature_beta': {
            'channel': 'beta',
            'contexts': ['privileged_extension'],
            'source': 'feature_alpha'
        }
    }
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
            'contexts': ['privileged_extension'],
            'alias': 'feature_beta'
        }, {
            'contexts': ['privileged_extension'],
            'channel': 'beta',
            'alias': 'feature_beta'
        }]
    }
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self._hasError(
        feature, 'Error parsing feature "feature_alpha" at key ' +
        '"alias": Key can be set at most once per feature.')

  def testAliasReferenceInComplexFeature(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
        'feature_alpha': [{
            'channel': 'beta',
            'contexts': ['privileged_extension'],
            'alias': 'feature_beta'
        }, {
            'contexts': ['privileged_extension'],
            'channel': 'beta',
        }],
        'feature_beta': {
            'channel': 'beta',
            'contexts': ['privileged_extension'],
            'source': 'feature_alpha'
        }
    }
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
            'contexts': ['privileged_extension'],
            'alias': 'feature_beta'
        },
        'feature_beta': {
            'contexts': ['privileged_extension'],
            'channel': 'beta',
            'source': 'does_not_exist'
        }
    }
    compiler.Compile()

    feature = compiler._features.get('feature_beta')
    self.assertTrue(feature)
    self._hasError(
        feature, 'A feature source property should reference a ' +
        'feature whose alias property references it back.')

  def testAliasMissingReferenceInComplexFeature(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
        'feature_alpha': [{
            'channel': 'beta',
            'contexts': ['privileged_extension'],
            'alias': 'feature_beta'
        }, {
            'contexts': ['privileged_extension'],
            'channel': 'beta'
        }]
    }
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self._hasError(
        feature, 'A feature alias property should reference a ' +
        'feature whose source property references it back.')

  def testAliasReferenceMissingSourceInComplexFeature(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
        'feature_alpha': {
            'contexts': ['privileged_extension'],
            'channel': 'beta',
        },
        'feature_beta': {
            'channel': 'beta',
            'contexts': ['privileged_extension'],
            'alias': 'feature_alpha'
        }
    }
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self.assertFalse(feature.GetErrors())

    feature = compiler._features.get('feature_beta')
    self.assertTrue(feature)
    self._hasError(
        feature, 'A feature alias property should reference a ' +
        'feature whose source property references it back.')

  def testComplexParentWithoutDefaultParent(self):
    c = feature_compiler.FeatureCompiler(None, None, 'APIFeature', None, None,
                                         None, None)
    c._CompileFeature('bookmarks', [{
        'contexts': ['privileged_extension'],
    }, {
        'channel': 'stable',
        'contexts': ['webui'],
    }])

    with self.assertRaisesRegex(AssertionError,
                                'No default parent found for bookmarks'):
      c._CompileFeature('bookmarks.export', {"allowlist": ["asdf"]})

  def testComplexFeatureWithSinglePropertyBlock(self):
    compiler = self._createTestFeatureCompiler('APIFeature')

    error = ('Error parsing feature "feature_alpha": A complex feature '
             'definition is only needed when there are multiple objects '
             'specifying different groups of properties for feature '
             'availability. You can reduce it down to a single object on the '
             'feature key instead of a list.')
    with self.assertRaisesRegex(AssertionError, error):
      compiler._CompileFeature('feature_alpha',
                               [{
                                   'contexts': ['privileged_extension'],
                                   'channel': 'stable',
                               }])

  def testRealIdsDisallowedInAllowlist(self):
    fake_id = 'a' * 32
    f = self._parseFeature({
        'allowlist': [fake_id],
        'extension_types': ['extension'],
        'channel': 'beta'
    })
    f.Validate('PermissionFeature', {})
    self._hasError(
        f, 'list should only have hex-encoded SHA1 hashes of extension ids')

  def testHostedAppsCantUseAllowlistedFeatures_SimpleFeature(self):
    f = self._parseFeature({
        'extension_types': ['extension', 'hosted_app'],
        'allowlist': ['0123456789ABCDEF0123456789ABCDEF01234567'],
        'channel':
        'beta',
    })
    f.Validate('PermissionFeature', {})
    self._hasError(f, 'Hosted apps are not allowed to use restricted features')

  def testHostedAppsCantUseAllowlistedFeatures_ComplexFeature(self):
    c = feature_compiler.FeatureCompiler(None, None, 'PermissionFeature', None,
                                         None, None, None)
    c._CompileFeature(
        'invalid_feature',
        [{
            'extension_types': ['extension'],
            'channel': 'beta',
        }, {
            'channel': 'beta',
            'extension_types': ['hosted_app'],
            'allowlist': ['0123456789ABCDEF0123456789ABCDEF01234567'],
        }])
    c._CompileFeature(
        'valid_feature',
        [{
            'extension_types': ['extension'],
            'channel': 'beta',
            'allowlist': ['0123456789ABCDEF0123456789ABCDEF01234567'],
        }, {
            'channel': 'beta',
            'extension_types': ['hosted_app'],
        }])

    valid_feature = c._features.get('valid_feature')
    self.assertTrue(valid_feature)
    self.assertFalse(valid_feature.GetErrors())

    invalid_feature = c._features.get('invalid_feature')
    self.assertTrue(invalid_feature)
    self._hasError(invalid_feature,
                   'Hosted apps are not allowed to use restricted features')

  def testHostedAppsCantUseAllowlistedFeatures_ChildFeature(self):
    c = feature_compiler.FeatureCompiler(None, None, 'PermissionFeature', None,
                                         None, None, None)
    c._CompileFeature('parent', {
        'extension_types': ['hosted_app'],
        'channel': 'beta',
    })

    c._CompileFeature(
        'parent.child',
        {'allowlist': ['0123456789ABCDEF0123456789ABCDEF01234567']})
    feature = c._features.get('parent.child')
    self.assertTrue(feature)
    self._hasError(feature,
                   'Hosted apps are not allowed to use restricted features')

  def testEmptyContextsDisallowed(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
        'feature_alpha': {
            'channel': 'beta',
            'contexts': [],
            'extension_types': ['extension']
        }
    }
    compiler.Compile()

    feature = compiler._features.get('feature_alpha')
    self.assertTrue(feature)
    self._hasError(feature,
                   'An empty contexts list is not allowed for this feature.')

  def testEmptyContextsAllowed(self):
    compiler = self._createTestFeatureCompiler('APIFeature')
    compiler._json = {
        'empty_contexts': {
            'channel': 'beta',
            'contexts': [],
            'extension_types': ['extension']
        }
    }
    compiler.Compile()

    feature = compiler._features.get('empty_contexts')
    self.assertTrue(feature)
    self.assertFalse(feature.GetErrors())

  def testFeatureHiddenBehindBuildflag(self):
    compiler = self._createTestFeatureCompiler('APIFeature')

    compiler._json = {
        'feature_cups': {
            'channel': 'beta',
            'contexts': ['privileged_extension'],
            'extension_types': ['extension'],
            'required_buildflags': ['use_cups']
        }
    }
    compiler.Compile()
    cc_code = compiler.Render()

    # The code below is formatted correctly!
    self.assertEqual(
        cc_code.Render(), '''  {
    #if BUILDFLAG(USE_CUPS)
    SimpleFeature* feature = new SimpleFeature();
    feature->set_name("feature_cups");
    feature->set_channel(version_info::Channel::BETA);
    feature->set_contexts({mojom::ContextType::kPrivilegedExtension});
    feature->set_extension_types({Manifest::TYPE_EXTENSION});
    provider->AddFeature("feature_cups", feature);
    #endif
  }''')


if __name__ == '__main__':
  unittest.main()
