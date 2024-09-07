# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import copy
from datetime import datetime
from functools import partial
import json
import os
import posixpath
import re
import sys

from code_util import Code
import json_parse

# The template for the header file of the generated FeatureProvider.
HEADER_FILE_TEMPLATE = """
// Copyright %(year)s The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GENERATED FROM THE FEATURES FILE:
//   %(source_files)s
// by tools/json_schema_compiler.
// DO NOT EDIT.

#ifndef %(header_guard)s
#define %(header_guard)s

namespace extensions {
class FeatureProvider;

void %(method_name)s(FeatureProvider* provider);

}  // namespace extensions

#endif  // %(header_guard)s
"""

# The beginning of the .cc file for the generated FeatureProvider.
CC_FILE_BEGIN = """
// Copyright %(year)s The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GENERATED FROM THE FEATURES FILE:
//   %(source_files)s
// by tools/json_schema_compiler.
// DO NOT EDIT.

#include "%(header_file_path)s"

#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "printing/buildflags/buildflags.h"

namespace extensions {

void %(method_name)s(FeatureProvider* provider) {
"""

# The end of the .cc file for the generated FeatureProvider.
CC_FILE_END = """
}

}  // namespace extensions
"""


def ToPosixPath(path):
  """Returns |path| with separator converted to POSIX style.

  This is needed to generate C++ #include paths.
  """
  return path.replace(os.path.sep, posixpath.sep)


# Returns true if the list 'l' only contains strings that are a hex-encoded SHA1
# hashes.
def ListContainsOnlySha1Hashes(l):
  return len(list(filter(lambda s: not re.match("^[A-F0-9]{40}$", s), l))) == 0


# A "grammar" for what is and isn't allowed in the features.json files. This
# grammar has to list all possible keys and the requirements for each. The
# format of each entry is:
#   'key': {
#     allowed_type_1: optional_properties,
#     allowed_type_2: optional_properties,
#   }
# |allowed_types| are the types of values that can be used for a given key. The
# possible values are list, str, bool, and int.
# |optional_properties| provide more restrictions on the given type. The options
# are:
#   'subtype': Only applicable for lists. If provided, this enforces that each
#              entry in the list is of the specified type.
#   'enum_map': A map of strings to C++ enums. When the compiler sees the given
#               enum string, it will replace it with the C++ version in the
#               compiled code. For instance, if a feature specifies
#               'channel': 'stable', the generated C++ will assign
#               version_info::Channel::STABLE to channel. The keys in this map
#               also serve as a list all of possible values.
#   'allow_all': Only applicable for lists. If present, this will check for
#                a value of "all" for a list value, and will replace it with
#                the collection of all possible values. For instance, if a
#                feature specifies 'contexts': 'all', the generated C++ will
#                assign the list of mojom::ContextType::kPrivilegedExtension,
#                mojom::ContextType::kPrivilegedWebPage et al for contexts. If
#                not specified, defaults to false.
#   'allow_empty': Only applicable for lists. Whether an empty list is a valid
#                  value. If omitted, empty lists are prohibited.
#   'validators': A list of (function, str) pairs with a function to run on the
#                 value for a feature. Validators allow for more flexible or
#                 one-off style validation than just what's in the grammar (such
#                 as validating the content of a string). The validator function
#                 should return True if the value is valid, and False otherwise.
#                 If the value is invalid, the specified error will be added for
#                 that key.
#   'values': A list of all possible allowed values for a given key.
#   'shared': Boolean that, if set, ensures that only one of the associated
#       features has the feature property set. Used primarily for complex
#       features - for simple features, there is always at most one feature
#       setting an option.
# If a type definition does not have any restrictions (beyond the type itself),
# an empty definition ({}) is used.
FEATURE_GRAMMAR = ({
    'alias': {
        str: {},
        'shared': True
    },
    'allowlist': {
        list: {
            'subtype':
            str,
            'validators':
            [(ListContainsOnlySha1Hashes,
              'list should only have hex-encoded SHA1 hashes of extension ids')]
        }
    },
    'blocklist': {
        list: {
            'subtype':
            str,
            'validators':
            [(ListContainsOnlySha1Hashes,
              'list should only have hex-encoded SHA1 hashes of extension ids')]
        }
    },
    'channel': {
        str: {
            'enum_map': {
                'trunk': 'version_info::Channel::UNKNOWN',
                'canary': 'version_info::Channel::CANARY',
                'dev': 'version_info::Channel::DEV',
                'beta': 'version_info::Channel::BETA',
                'stable': 'version_info::Channel::STABLE',
            }
        }
    },
    'command_line_switch': {
        str: {}
    },
    'component_extensions_auto_granted': {
        bool: {}
    },
    'contexts': {
        list: {
            'enum_map': {
                'privileged_extension':
                'mojom::ContextType::kPrivilegedExtension',
                'privileged_web_page':
                'mojom::ContextType::kPrivilegedWebPage',
                'content_script':
                'mojom::ContextType::kContentScript',
                'lock_screen_extension':
                'mojom::ContextType::kLockscreenExtension',
                'offscreen_extension':
                'mojom::ContextType::kOffscreenExtension',
                'user_script':
                'mojom::ContextType::kUserScript',
                'web_page':
                'mojom::ContextType::kWebPage',
                'webui':
                'mojom::ContextType::kWebUi',
                'webui_untrusted':
                'mojom::ContextType::kUntrustedWebUi',
                'unprivileged_extension':
                'mojom::ContextType::kUnprivilegedExtension',
            },
            'allow_all': True,
            'allow_empty': True
        },
    },
    'default_parent': {
        bool: {
            'values': [True]
        }
    },
    'dependencies': {
        list: {
            # We allow an empty list of dependencies for child features that
            # want to override their parents' dependency set.
            'allow_empty': True,
            'subtype': str
        }
    },
    'developer_mode_only': {
        bool: {}
    },
    'disallow_for_service_workers': {
        bool: {}
    },
    'extension_types': {
        list: {
            'enum_map': {
                'extension':
                'Manifest::TYPE_EXTENSION',
                'hosted_app':
                'Manifest::TYPE_HOSTED_APP',
                'legacy_packaged_app':
                'Manifest::TYPE_LEGACY_PACKAGED_APP',
                'platform_app':
                'Manifest::TYPE_PLATFORM_APP',
                'shared_module':
                'Manifest::TYPE_SHARED_MODULE',
                'theme':
                'Manifest::TYPE_THEME',
                'login_screen_extension':
                'Manifest::TYPE_LOGIN_SCREEN_EXTENSION',
                'chromeos_system_extension':
                'Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION',
            },
            'allow_all': True
        },
    },
    'feature_flag': {
        str: {}
    },
    'location': {
        str: {
            'enum_map': {
                'component': 'SimpleFeature::COMPONENT_LOCATION',
                'external_component':
                'SimpleFeature::EXTERNAL_COMPONENT_LOCATION',
                'policy': 'SimpleFeature::POLICY_LOCATION',
                'unpacked': 'SimpleFeature::UNPACKED_LOCATION',
            }
        }
    },
    'internal': {
        bool: {
            'values': [True]
        }
    },
    'matches': {
        list: {
            'subtype': str
        }
    },
    'max_manifest_version': {
        int: {
            'values': [1, 2]
        }
    },
    'min_manifest_version': {
        int: {
            'values': [2, 3]
        }
    },
    'requires_delegated_availability_check': {
        bool: {
            'values': [True]
        }
    },
    'noparent': {
        bool: {
            'values': [True]
        }
    },
    'platforms': {
        list: {
            'enum_map': {
                'chromeos': 'Feature::CHROMEOS_PLATFORM',
                'fuchsia': 'Feature::FUCHSIA_PLATFORM',
                'lacros': 'Feature::LACROS_PLATFORM',
                'linux': 'Feature::LINUX_PLATFORM',
                'mac': 'Feature::MACOSX_PLATFORM',
                'win': 'Feature::WIN_PLATFORM',
            }
        }
    },
    'required_buildflags': {
        list: {
            'values': ['use_cups']
        }
    },
    'session_types': {
        list: {
            'enum_map': {
                'regular':
                'mojom::FeatureSessionType::kRegular',
                'kiosk':
                'mojom::FeatureSessionType::kKiosk',
                'kiosk.autolaunched':
                'mojom::FeatureSessionType::kAutolaunchedKiosk',
            }
        }
    },
    'source': {
        str: {},
        'shared': True
    },
})

FEATURE_TYPES = [
    'APIFeature', 'BehaviorFeature', 'ManifestFeature', 'PermissionFeature'
]


def HasProperty(property_name, value):
  return property_name in value


def HasAtLeastOneProperty(property_names, value):
  return any([HasProperty(name, value) for name in property_names])


def DoesNotHaveAllProperties(property_names, value):
  return not all([HasProperty(name, value) for name in property_names])


def DoesNotHaveProperty(property_name, value):
  return property_name not in value


def DoesNotHavePropertyInComplexFeature(property_name, feature, all_features):
  if type(feature) is ComplexFeature:
    for child_feature in feature.feature_list:
      if child_feature.GetValue(property_name):
        return False
  return True


def IsEmptyContextsAllowed(feature, all_features):
  # An alias feature wouldn't have the 'contexts' feature value.
  if feature.GetValue('source'):
    return True

  if type(feature) is ComplexFeature:
    for child_feature in feature.feature_list:
      if not IsEmptyContextsAllowed(child_feature, all_features):
        return False
    return True

  contexts = feature.GetValue('contexts')
  assert contexts, 'contexts must have been specified for the APIFeature'

  allowlisted_empty_context_namespaces = [
      'manifestTypes',
      'extensionsManifestTypes',
      'empty_contexts'  # Only added for testing.
  ]
  return (contexts != '{}'
          or feature.name in allowlisted_empty_context_namespaces)


def IsFeatureCrossReference(property_name, reverse_property_name, feature,
                            all_features):
  """ Verifies that |property_name| on |feature| references a feature that
  references |feature| back using |reverse_property_name| property.
  |property_name| and |reverse_property_name| are expected to have string
  values.
  """
  value = feature.GetValue(property_name)
  if not value:
    return True
  # String property values will be wrapped in "", strip those.
  value_regex = re.compile('^"(.+)"$')
  parsed_value = value_regex.match(value)
  assert parsed_value, (
      'IsFeatureCrossReference should only be used on unicode properties')

  referenced_feature = all_features.get(parsed_value.group(1))
  if not referenced_feature:
    return False
  reverse_reference_value = referenced_feature.GetValue(reverse_property_name)
  if not reverse_reference_value:
    return False
  # Don't validate reverse reference value for child features - chances are that
  # the value was inherited from a feature parent, in which case it won't match
  # current feature name.
  if feature.has_parent:
    return True
  return reverse_reference_value == ('"%s"' % feature.name)


# Verifies that a feature with an allowlist is not available to hosted apps,
# returning true on success.
def DoesNotHaveAllowlistForHostedApps(value):
  if not 'allowlist' in value:
    return True

  # Hack Alert: |value| here has the code for the generated C++ feature. Since
  # we're looking at the individual values, we do a bit of yucky back-parsing
  # to get a better look at the feature. This would be cleaner if we were
  # operating on the JSON feature itself, but we currently never generate a
  # JSON-based feature object that has all the values inherited from its
  # parents. Since this is the only scenario we need this type of validation,
  # doing it in a slightly ugly way isn't too bad. If we need more of these,
  # we should find a smoother way to do it (e.g. first generate JSON-based
  # features with inherited properties, do any necessary validation, then
  # generate the C++ code strings).

  # The feature did not specify extension types; this is fine for e.g.
  # API features (which would typically rely on a permission feature, which
  # is required to specify types).
  if not 'extension_types' in value:
    return True

  types = value['extension_types']
  # |types| looks like "{Manifest::TYPE_1, Manifest::TYPE_2}", so just looking
  # for the "TYPE_HOSTED_APP substring is sufficient.
  if 'TYPE_HOSTED_APP' not in types:
    return True

  # Helper to convert our C++ string array like "{\"aaa\", \"bbb\"}" (which is
  # what the allowlist looks like) to a python list of strings.
  def cpp_list_to_list(cpp_list):
    assert type(cpp_list) is str
    assert cpp_list[0] == '{'
    assert cpp_list[-1] == '}'
    new_list = json.loads('[%s]' % cpp_list[1:-1])
    assert type(new_list) is list
    return new_list

  # Exceptions (see the feature files).
  # DO NOT ADD MORE.
  HOSTED_APP_EXCEPTIONS = [
      'B44D08FD98F1523ED5837D78D0A606EA9D6206E5',
  ]

  allowlist = cpp_list_to_list(value['allowlist'])
  for entry in allowlist:
    if entry not in HOSTED_APP_EXCEPTIONS:
      return False

  return True


SIMPLE_FEATURE_CPP_CLASSES = ({
    'APIFeature': 'SimpleFeature',
    'ManifestFeature': 'ManifestFeature',
    'PermissionFeature': 'PermissionFeature',
    'BehaviorFeature': 'SimpleFeature',
})

VALIDATION = ({
    'all': [
        (partial(HasAtLeastOneProperty, ['channel', 'dependencies']),
         'Features must specify either a channel or dependencies'),
        (DoesNotHaveAllowlistForHostedApps,
         'Hosted apps are not allowed to use restricted features'),
    ],
    'APIFeature':
    [(partial(HasProperty,
              'contexts'), 'APIFeatures must specify the contexts property'),
     (partial(DoesNotHaveAllProperties, ['alias', 'source']),
      'Features cannot specify both alias and source.')],
    'ManifestFeature': [
        (partial(HasProperty, 'extension_types'),
         'ManifestFeatures must specify at least one extension type'),
        (partial(DoesNotHaveProperty,
                 'contexts'), 'ManifestFeatures do not support contexts.'),
        (partial(DoesNotHaveProperty,
                 'alias'), 'ManifestFeatures do not support alias.'),
        (partial(DoesNotHaveProperty,
                 'source'), 'ManifestFeatures do not support source.'),
        # The `required_buildflags` field is intended to be used to toggle the
        # availability of certain APIs; if we support this for feature types
        # other than APIFeature, we may emit warnings that are visible to
        # developers which is not desirable.
        (partial(DoesNotHaveProperty, 'required_buildflags'),
         'ManifestFeatures do not support required_buildflags.'),
    ],
    'BehaviorFeature': [
        (partial(DoesNotHaveProperty,
                 'alias'), 'BehaviorFeatures do not support alias.'),
        (partial(DoesNotHaveProperty,
                 'source'), 'BehaviorFeatures do not support source.'),
        # The `required_buildflags` field is intended to be used to toggle the
        # availability of certain APIs; if we support this for feature types
        # other than APIFeature, we may emit warnings that are visible to
        # developers which is not desirable.
        (partial(DoesNotHaveProperty, 'required_buildflags'),
         'BehaviorFeatures do not support required_buildflags.'),
    ],
    'PermissionFeature': [
        (partial(HasProperty, 'extension_types'),
         'PermissionFeatures must specify at least one extension type'),
        (partial(DoesNotHaveProperty,
                 'contexts'), 'PermissionFeatures do not support contexts.'),
        (partial(DoesNotHaveProperty,
                 'alias'), 'PermissionFeatures do not support alias.'),
        (partial(DoesNotHaveProperty,
                 'source'), 'PermissionFeatures do not support source.'),
        # The `required_buildflags` field is intended to be used to toggle the
        # availability of certain APIs; if we support this for feature types
        # other than APIFeature, we may emit warnings that are visible to
        # developers which is not desirable.
        (partial(DoesNotHaveProperty, 'required_buildflags'),
         'PermissionFeatures do not support required_buildflags.'),
    ],
})

FINAL_VALIDATION = ({
    'all': [
        # A complex feature requires at least one child entry at all times; with
        # `required_buildflags` it becomes harder to guarantee that this holds
        # for every potential combination of the provided flags.
        (partial(DoesNotHavePropertyInComplexFeature, 'required_buildflags'),
         'required_buildflags cannot be nested in a ComplexFeature'),
    ],
    'APIFeature':
    [(partial(IsFeatureCrossReference, 'alias', 'source'),
      'A feature alias property should reference a feature whose source '
      'property references it back.'),
     (partial(IsFeatureCrossReference, 'source', 'alias'),
      'A feature source property should reference a feature whose alias '
      'property references it back.'),
     (IsEmptyContextsAllowed,
      'An empty contexts list is not allowed for this feature.')],
    'ManifestFeature': [],
    'BehaviorFeature': [],
    'PermissionFeature': []
})

# These keys can not be set on a feature and are hence ignored.
IGNORED_KEYS = ['default_parent', 'required_buildflags']

# By default, if an error is encountered, assert to stop the compilation. This
# can be disabled for testing.
ENABLE_ASSERTIONS = True


def GetCodeForFeatureValues(feature_values):
  """ Gets the Code object for setting feature values for this object. """
  c = Code()
  for key in sorted(feature_values.keys()):
    if key in IGNORED_KEYS:
      continue

    c.Append('feature->set_%s(%s);' % (key, feature_values[key]))
  return c


class Feature(object):
  """A representation of a single simple feature that can handle all parsing,
  validation, and code generation.
  """

  def __init__(self, name):
    self.name = name
    self.has_parent = False
    self.errors = []
    self.feature_values = {}
    self.shared_values = {}

  def AddError(self, error):
    """Adds an error to the feature. If ENABLE_ASSERTIONS is active, this will
    also assert to stop the compilation process (since errors should never be
    found in production).
    """
    self.errors.append(error)
    if ENABLE_ASSERTIONS:
      assert False, error

  def _AddKeyError(self, key, error):
    """Adds an error relating to a particular key in the feature.
    """
    self.AddError('Error parsing feature "%s" at key "%s": %s' %
                  (self.name, key, error))

  def _GetCheckedValue(self, key, expected_type, expected_values, enum_map,
                       value):
    """Returns a string to be used in the generated C++ code for a given key's
    python value, or None if the value is invalid. For example, if the python
    value is True, this returns 'true', for a string foo, this returns "foo",
    and for an enum, this looks up the C++ definition in the enum map.
      key: The key being parsed.
      expected_type: The expected type for this value, or None if any type is
                     allowed.
      expected_values: The list of allowed values for this value, or None if any
                       value is allowed.
      enum_map: The map from python value -> cpp value for all allowed values,
               or None if no special mapping should be made.
      value: The value to check.
    """
    valid = True
    if expected_values and value not in expected_values:
      self._AddKeyError(key, 'Illegal value: "%s"' % value)
      valid = False

    t = type(value)
    if expected_type and t is not expected_type:
      self._AddKeyError(key, 'Illegal value: "%s"' % value)
      valid = False

    if not valid:
      return None

    if enum_map:
      return enum_map[value]

    if t is str:
      if key == 'required_buildflags':
        return value
      return '"%s"' % str(value)
    if t is int:
      return str(value)
    if t is bool:
      return 'true' if value else 'false'
    assert False, 'Unsupported type: %s' % value

  def _ParseKey(self, key, value, shared_values, grammar):
    """Parses the specific key according to the grammar rule for that key if it
    is present in the json value.
      key: The key to parse.
      value: The full value for this feature.
      shared_values: Set of shared vfalues associated with this feature.
      grammar: The rule for the specific key.
    """
    if key not in value:
      return
    v = value[key]

    is_all = False
    if v == 'all' and list in grammar and 'allow_all' in grammar[list]:
      assert grammar[list]['allow_all'], '`allow_all` only supports `True`.'
      v = []
      is_all = True

    if 'shared' in grammar and key in shared_values:
      self._AddKeyError(key, 'Key can be set at most once per feature.')
      return

    value_type = type(v)
    if value_type not in grammar:
      self._AddKeyError(key, 'Illegal value: "%s"' % v)
      return

    if value_type is list and not is_all and len(v) == 0:
      if 'allow_empty' in grammar[list]:
        assert grammar[list]['allow_empty'], \
               '`allow_empty` only supports `True`.'
      else:
        self._AddKeyError(key, 'List must specify at least one element.')
        return

    expected = grammar[value_type]
    expected_values = None
    enum_map = None
    if 'values' in expected:
      expected_values = expected['values']
    elif 'enum_map' in expected:
      enum_map = expected['enum_map']
      expected_values = list(enum_map)

    if is_all:
      v = copy.deepcopy(expected_values)

    expected_type = None
    if value_type is list and 'subtype' in expected:
      expected_type = expected['subtype']

    cpp_value = None
    # If this value is a list, iterate over each entry and validate. Otherwise,
    # validate the single value.
    if value_type is list:
      cpp_value = []
      for sub_value in v:
        cpp_sub_value = self._GetCheckedValue(key, expected_type,
                                              expected_values, enum_map,
                                              sub_value)
        if cpp_sub_value:
          cpp_value.append(cpp_sub_value)
      if key != 'required_buildflags':
        cpp_value = '{' + ','.join(cpp_value) + '}'
    else:
      cpp_value = self._GetCheckedValue(key, expected_type, expected_values,
                                        enum_map, v)

    if 'validators' in expected:
      validators = expected['validators']
      for validator, error in validators:
        if not validator(v):
          self._AddKeyError(key, error)

    if cpp_value:
      if 'shared' in grammar:
        shared_values[key] = cpp_value
      else:
        self.feature_values[key] = cpp_value
    elif key in self.feature_values:
      # If the key is empty and this feature inherited a value from its parent,
      # remove the inherited value.
      del self.feature_values[key]

  def SetParent(self, parent):
    """Sets the parent of this feature, and inherits all properties from that
    parent.
    """
    assert not self.feature_values, 'Parents must be set before parsing'
    self.feature_values = copy.deepcopy(parent.feature_values)
    self.has_parent = True

  def SetSharedValues(self, values):
    self.shared_values = values

  def Parse(self, parsed_json, shared_values):
    """Parses the feature from the given json value."""
    for key in parsed_json.keys():
      if key not in FEATURE_GRAMMAR:
        self._AddKeyError(key, 'Unrecognized key')
    for key, key_grammar in FEATURE_GRAMMAR.items():
      self._ParseKey(key, parsed_json, shared_values, key_grammar)

  def Validate(self, feature_type, shared_values):
    feature_values = self.feature_values.copy()
    feature_values.update(shared_values)
    for validator, error in (VALIDATION[feature_type] + VALIDATION['all']):
      if not validator(feature_values):
        self.AddError(error)

  def GetCode(self, feature_type):
    """Returns the Code object for generating this feature."""
    c = Code()
    cpp_feature_class = SIMPLE_FEATURE_CPP_CLASSES[feature_type]
    c.Append('%s* feature = new %s();' % (cpp_feature_class, cpp_feature_class))
    c.Append('feature->set_name("%s");' % self.name)
    c.Concat(GetCodeForFeatureValues(self.GetAllFeatureValues()))
    return c

  def AsParent(self):
    """ Returns the feature values that should be inherited by children features
    when this feature is set as parent.
    """
    return self

  def GetValue(self, key):
    """ Gets feature value for the specified key """
    value = self.feature_values.get(key)
    return value if value else self.shared_values.get(key)

  def GetAllFeatureValues(self):
    """ Gets all values set for this feature. """
    values = self.feature_values.copy()
    values.update(self.shared_values)
    return values

  def GetErrors(self):
    return self.errors


class ComplexFeature(Feature):
  """ Complex feature - feature that is comprised of list of features.
  Overall complex feature is available if any of contained
  feature is available.
  """

  def __init__(self, name):
    Feature.__init__(self, name)
    self.feature_list = []

  def GetCode(self, feature_type):
    c = Code()
    c.Append('std::vector<Feature*> features;')
    for f in self.feature_list:
      # Sanity check that components of complex features have no shared values
      # set.
      assert not f.shared_values
      c.Sblock('{')
      c.Concat(f.GetCode(feature_type))
      c.Append('features.push_back(feature);')
      c.Eblock('}')
    c.Append('ComplexFeature* feature(new ComplexFeature(&features));')
    c.Append('feature->set_name("%s");' % self.name)
    c.Concat(GetCodeForFeatureValues(self.shared_values))
    return c

  def AsParent(self):
    parent = None
    for p in self.feature_list:
      if 'default_parent' in p.feature_values:
        parent = p
        break
    assert parent, 'No default parent found for %s' % self.name
    return parent

  def GetErrors(self):
    errors = copy.copy(self.errors)
    for feature in self.feature_list:
      errors.extend(feature.GetErrors())
    return errors


class FeatureCompiler(object):
  """A compiler to load, parse, and generate C++ code for a number of
  features.json files."""

  def __init__(self, chrome_root, source_files, feature_type, method_name,
               out_root, gen_dir_relpath, out_base_filename):
    # See __main__'s ArgumentParser for documentation on these properties.
    self._chrome_root = chrome_root
    self._source_files = source_files
    self._feature_type = feature_type
    self._method_name = method_name
    self._out_root = out_root
    self._out_base_filename = out_base_filename
    self._gen_dir_relpath = gen_dir_relpath

    # The json value for the feature files.
    self._json = {}
    # The parsed features.
    self._features = {}

  def Load(self):
    """Loads and parses the source from each input file and puts the result in
    self._json."""
    for f in self._source_files:
      abs_source_file = os.path.join(self._chrome_root, f)
      try:
        with open(abs_source_file, 'r') as f:
          f_json = json_parse.Parse(f.read())
      except:
        print('FAILED: Exception encountered while loading "%s"' %
              abs_source_file)
        raise
      dupes = set(f_json) & set(self._json)
      assert not dupes, 'Duplicate keys found: %s' % list(dupes)
      self._json.update(f_json)

  def _FindParent(self, feature_name, feature_value):
    """Checks to see if a feature has a parent. If it does, returns the
    parent."""
    no_parent = False
    if type(feature_value) is list:
      no_parent_values = ['noparent' in v for v in feature_value]
      no_parent = all(no_parent_values)
      assert no_parent or not any(no_parent_values), (
          '"%s:" All child features must contain the same noparent value' %
          feature_name)
    else:
      no_parent = 'noparent' in feature_value
    sep = feature_name.rfind('.')
    if sep == -1 or no_parent:
      return None

    parent_name = feature_name[:sep]
    while sep != -1 and parent_name not in self._features:
      # This recursion allows for a feature to have a parent that isn't a direct
      # ancestor. For instance, we could have feature 'alpha', and feature
      # 'alpha.child.child', where 'alpha.child.child' inherits from 'alpha'.
      # TODO(devlin): Is this useful? Or logical?
      sep = feature_name.rfind('.', 0, sep)
      parent_name = feature_name[:sep]

    if sep == -1:
      # TODO(devlin): It'd be kind of nice to be able to assert that the
      # deduced parent name is in our features, but some dotted features don't
      # have parents and also don't have noparent, e.g. system.cpu. We should
      # probably just noparent them so that we can assert this.
      #   raise KeyError('Could not find parent "%s" for feature "%s".' %
      #                      (parent_name, feature_name))
      return None
    return self._features[parent_name].AsParent()

  def _CompileFeature(self, feature_name, feature_value):
    """Parses a single feature."""
    if 'nocompile' in feature_value:
      assert feature_value['nocompile'], (
          'nocompile should only be true; otherwise omit this key.')
      return

    def parse_and_validate(name, value, parent, shared_values):
      try:
        feature = Feature(name)
        if parent:
          feature.SetParent(parent)
        feature.Parse(value, shared_values)
        feature.Validate(self._feature_type, shared_values)
        return feature
      except:
        print('Failure to parse feature "%s"' % feature_name)
        raise

    parent = self._FindParent(feature_name, feature_value)
    shared_values = {}

    # Handle complex features, which are lists of simple features.
    if type(feature_value) is list:
      assert len(feature_value) > 1, (
          'Error parsing feature "%s": A complex feature ' % feature_name +
          'definition is only needed when there are multiple objects ' +
          'specifying different groups of properties for feature ' +
          'availability. You can reduce it down to a single object on the ' +
          'feature key instead of a list.')

      feature = ComplexFeature(feature_name)

      # This doesn't handle nested complex features. I think that's probably for
      # the best.
      for v in feature_value:
        feature.feature_list.append(
            parse_and_validate(feature_name, v, parent, shared_values))
      self._features[feature_name] = feature
    else:
      self._features[feature_name] = parse_and_validate(feature_name,
                                                        feature_value, parent,
                                                        shared_values)

    # Apply parent shared values at the end to enable child features to
    # override parent shared value - if parent shared values are added to
    # shared value set before a child feature is parsed, the child feature
    # overriding shared values set by its parent would cause an error due to
    # shared values being set twice.
    final_shared_values = copy.deepcopy(parent.shared_values) if parent else {}
    final_shared_values.update(shared_values)
    self._features[feature_name].SetSharedValues(final_shared_values)

  def _FinalValidation(self):
    validators = FINAL_VALIDATION['all'] + FINAL_VALIDATION[self._feature_type]
    for name, feature in self._features.items():
      for validator, error in validators:
        if not validator(feature, self._features):
          feature.AddError(error)

  def Compile(self):
    """Parses all features after loading the input files."""
    # Iterate over in sorted order so that parents come first.
    for k in sorted(self._json.keys()):
      self._CompileFeature(k, self._json[k])
    self._FinalValidation()

  def Render(self):
    """Returns the Code object for the body of the .cc file, which handles the
    initialization of all features."""
    c = Code()
    c.Sblock()
    for k in sorted(self._features.keys()):
      c.Sblock('{')
      feature = self._features[k]
      required_buildflags = feature.GetValue('required_buildflags')
      if required_buildflags:
        formatted_buildflags = [
            'BUILDFLAG(%s)' % format(flag.upper())
            for flag in required_buildflags
        ]
        c.Append('#if %s' % format(' && '.join(formatted_buildflags)))
      c.Concat(feature.GetCode(self._feature_type))
      c.Append('provider->AddFeature("%s", feature);' % k)
      if required_buildflags:
        c.Append('#endif')
      c.Eblock('}')
    c.Eblock()
    return c

  def Write(self):
    """Writes the output."""
    header_file = self._out_base_filename + '.h'
    cc_file = self._out_base_filename + '.cc'

    include_file_root = self._out_root[len(self._gen_dir_relpath) + 1:]
    header_file_path = '%s/%s' % (include_file_root, header_file)
    cc_file_path = '%s/%s' % (include_file_root, cc_file)
    substitutions = ({
        'header_file_path':
        header_file_path,
        'header_guard':
        (header_file_path.replace('/', '_').replace('.', '_').upper()),
        'method_name':
        self._method_name,
        'source_files':
        str([ToPosixPath(f) for f in self._source_files]),
        'year':
        str(datetime.now().year)
    })
    if not os.path.exists(self._out_root):
      os.makedirs(self._out_root)
    # Write the .h file.
    with open(os.path.join(self._out_root, header_file), 'w') as f:
      header_file = Code()
      header_file.Append(HEADER_FILE_TEMPLATE)
      header_file.Substitute(substitutions)
      f.write(header_file.Render().strip())
    # Write the .cc file.
    with open(os.path.join(self._out_root, cc_file), 'w') as f:
      cc_file = Code()
      cc_file.Append(CC_FILE_BEGIN)
      cc_file.Substitute(substitutions)
      cc_file.Concat(self.Render())
      cc_end = Code()
      cc_end.Append(CC_FILE_END)
      cc_end.Substitute(substitutions)
      cc_file.Concat(cc_end)
      f.write(cc_file.Render().strip())


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Compile json feature files')
  parser.add_argument('chrome_root',
                      type=str,
                      help='The root directory of the chrome checkout')
  parser.add_argument(
      'feature_type',
      type=str,
      help='The name of the class to use in feature generation ' +
      '(e.g. APIFeature, PermissionFeature)')
  parser.add_argument('method_name',
                      type=str,
                      help='The name of the method to populate the provider')
  parser.add_argument('out_root',
                      type=str,
                      help='The root directory to generate the C++ files into')
  parser.add_argument(
      'gen_dir_relpath',
      default='gen',
      help='Path of the '
      'gen directory relative to the out/. If running in the default '
      'toolchain, the path is gen, otherwise $toolchain_name/gen')
  parser.add_argument(
      'out_base_filename',
      type=str,
      help='The base filename for the C++ files (.h and .cc will be appended)')
  parser.add_argument('source_files',
                      type=str,
                      nargs='+',
                      help='The source features.json files')
  args = parser.parse_args()
  if args.feature_type not in FEATURE_TYPES:
    raise NameError('Unknown feature type: %s' % args.feature_type)
  c = FeatureCompiler(args.chrome_root, args.source_files, args.feature_type,
                      args.method_name, args.out_root, args.gen_dir_relpath,
                      args.out_base_filename)
  c.Load()
  c.Compile()
  c.Write()
