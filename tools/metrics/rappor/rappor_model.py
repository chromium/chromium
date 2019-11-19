# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# """Model objects for rappor.xml contents."""

import logging
import operator
import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import models

# Model definitions for rappor.xml content
_OBSOLETE_TYPE = models.TextNodeType('obsolete')
_OWNER_TYPE = models.TextNodeType('owner', single_line=True)
_SUMMARY_TYPE = models.TextNodeType('summary')

_NOISE_VALUES_TYPE = models.ObjectNodeType(
    'noise-values',
    attributes=[
        ('fake-prob', float, None),
        ('fake-one-prob', float, None),
        ('one-coin-prob', float, None),
        ('zero-coin-prob', float, None),
    ])

_NOISE_LEVEL_TYPE = models.ObjectNodeType(
    'noise-level',
    extra_newlines=(1, 1, 1),
    attributes=[('name', unicode, None)],
    children=[
        models.ChildType('summary', _SUMMARY_TYPE, False),
        models.ChildType('values', _NOISE_VALUES_TYPE, False),
    ])

_NOISE_LEVELS_TYPE = models.ObjectNodeType(
    'noise-levels',
    extra_newlines=(1, 1, 1),
    indent=False,
    children=[
        models.ChildType('levels', _NOISE_LEVEL_TYPE, True),
    ])

_PARAMETERS_TYPE = models.ObjectNodeType(
    'parameters',
    attributes=[
        ('num-cohorts', int, None),
        ('bytes', int, None),
        ('hash-functions', int, None),
        ('reporting-level', unicode, None),
        ('noise-level', unicode, None),
    ])

_RAPPOR_PARAMETERS_TYPE = models.ObjectNodeType(
    'rappor-parameters',
    extra_newlines=(1, 1, 1),
    attributes=[('name', unicode, None)],
    children=[
        models.ChildType('summary', _SUMMARY_TYPE, False),
        models.ChildType('parameters', _PARAMETERS_TYPE, False),
    ])

_RAPPOR_PARAMETERS_TYPES_TYPE = models.ObjectNodeType(
    'rappor-parameter-types',
    extra_newlines=(1, 1, 1),
    indent=False,
    children=[
        models.ChildType('types', _RAPPOR_PARAMETERS_TYPE, True),
    ])

_STRING_FIELD_TYPE = models.ObjectNodeType(
    'string-field',
    extra_newlines=(1, 1, 0),
    attributes=[('name', unicode, None)],
    children=[
        models.ChildType('summary', _SUMMARY_TYPE, False),
    ])

_FLAG_TYPE = models.ObjectNodeType(
    'flag',
    attributes=[('bit', int, None), ('label', unicode, None)],
    text_attribute='summary',
    single_line=True)

_FLAGS_FIELD_TYPE = models.ObjectNodeType(
    'flags-field',
    extra_newlines=(1, 1, 0),
    attributes=[('name', unicode, None), ('noise-level', unicode, None)],
    children=[
        models.ChildType('flags', _FLAG_TYPE, True),
        models.ChildType('summary', _SUMMARY_TYPE, False),
    ])

_UINT64_FIELD_TYPE = models.ObjectNodeType(
    'uint64-field',
    extra_newlines=(1, 1, 0),
    attributes=[('name', unicode, None), ('noise-level', unicode, None)],
    children=[
        models.ChildType('summary', _SUMMARY_TYPE, False),
    ])

_RAPPOR_METRIC_TYPE = models.ObjectNodeType(
    'rappor-metric',
    extra_newlines=(1, 1, 1),
    attributes=[('name', unicode, None), ('type', unicode, None)],
    children=[
        models.ChildType('obsolete', _OBSOLETE_TYPE, False),
        models.ChildType('owners', _OWNER_TYPE, True),
        models.ChildType('summary', _SUMMARY_TYPE, False),
        models.ChildType('strings', _STRING_FIELD_TYPE, True),
        models.ChildType('flags', _FLAGS_FIELD_TYPE, True),
        models.ChildType('uint64', _UINT64_FIELD_TYPE, True),
    ])

_RAPPOR_METRICS_TYPE = models.ObjectNodeType(
    'rappor-metrics',
    extra_newlines=(1, 1, 1),
    indent=False,
    children=[
        models.ChildType('metrics', _RAPPOR_METRIC_TYPE, True),
    ])

_RAPPOR_CONFIGURATION_TYPE = models.ObjectNodeType(
    'rappor-configuration',
    extra_newlines=(1, 1, 1),
    indent=False,
    children=[
        models.ChildType('noiseLevels', _NOISE_LEVELS_TYPE, False),
        models.ChildType(
            'parameterTypes', _RAPPOR_PARAMETERS_TYPES_TYPE, False),
        models.ChildType('metrics', _RAPPOR_METRICS_TYPE, False),
    ])

RAPPOR_XML_TYPE = models.DocumentType(_RAPPOR_CONFIGURATION_TYPE)


METRIC_DIMENSION_TYPES = [
    'strings',
    'flags',
    'uint64',
]


def _CheckRequired(obj, label, attributes):
  """Check that an JSON object has all required attributes.

  Args:
    obj: The JSON object.
    label: The name of the object, to use in error messages.
    attributes: The attributes to check for.
  Returns:
    True iff the object contains all of the attributes.
  """
  for attr in attributes:
    if attr not in obj:
      logging.error('Missing %s for %s', attr, label)
      return False
  return True


def _CheckAllAttributes(obj, label, node_type):
  """Check that an JSON object has all attributes of some node_type.

  Args:
    obj: The JSON object.
    label: The name of the object, to use in error messages.
    node_type: The NodeType with the attributes to check for.
  Returns:
    True iff the object contains all of the attributes.
  """
  return _CheckRequired(obj, label,
                        (attr for attr, _, _ in node_type.attributes))


def _IsValidNoiseLevel(noise_level):
  """Check if a noise-level is validly defined.

  Args:
    noise_level: The JSON noise-level to validate.
  Returns:
    True iff noise-level is valid.
  """
  if 'name' not in noise_level:
    logging.error('Missing name for noise-level')
    return False
  label = 'noise-level "%s"' % noise_level['name']
  return (_CheckRequired(noise_level, label, ['summary', 'values']) and
          _CheckAllAttributes(noise_level['values'], label, _NOISE_VALUES_TYPE))


def _GetNoiseLevelNames(config):
  return set(p['name'] for p in config['noiseLevels']['levels'])


def _IsValidRapporType(rappor_type, noise_level_names):
  """Check if a rappor-type is validly defined.

  Args:
    rappor_type: The JSON rappor-type to validate.
    noise_level_names: The set of valid noise_level names.
  Returns:
    True iff rappor-type is valid.
  """
  if 'name' not in rappor_type:
    logging.error('Missing name for rappor-type')
    return False
  label = 'rappor-type "%s"' % rappor_type['name']
  if not _CheckRequired(rappor_type, label, ['summary', 'parameters']):
    return False
  params = rappor_type['parameters']
  if not _CheckAllAttributes(params, label, _PARAMETERS_TYPE):
    return False
  if params['noise-level'] not in noise_level_names:
    logging.error('Invalid noise-level "%s" for %s',
                  params['noise-level'], label)
    return False
  return True


def _GetTypeNames(config):
  return set(p['name'] for p in config['parameterTypes']['types'])


# Old flag definitions look like: 'Bit 0: DID_PROCEED'.  The regex ignores
# whitespace differences.
BIT_DEF_RE = re.compile(r'Bit\s+(\d+)\s*:\s*(\S+)', re.IGNORECASE)


def _FixAndValidateFlagsField(field, parent_label):
  """Update old style flags, and validates them.

  Args:
    field: A flags-field JSON object.
    parent_label: The name of the parent object, to use in error messages.
  Returns:
    True if the field is valid.
  """
  if 'name' not in field:
    logging.error('Missing |name| for field in %s', parent_label)
    return False
  label = 'field "%s" of %s' % (field['name'], parent_label)
  for i, flag in enumerate(field['flags']):
    if 'bit' in flag:
      if 'label' not in flag:
        logging.error('Missing |label| for bit %s of %s', flag['bit'], label)
        return False
      continue
    # Try to upgrade from old format
    if 'summary' not in flag:
      # Missing both bit and summary, but we prefer 'bit' over summary.
      logging.error('Missing bit number in %s', label)
      return False
    match = BIT_DEF_RE.search(flag['summary'])
    if match:
      flag['bit'] = int(match.group(1))
      flag['label'] = match.group(2)
    else:
      # Bit not labeled, infer bit number from position.
      flag['bit'] = i
      flag['label'] = flag['summary']
    del flag['summary']
  field['flags'].sort(key=operator.itemgetter('bit'))
  for i, flag in enumerate(field['flags']):
    if flag['bit'] != i:
      logging.error('Missing bit %s for %s', i, label)
      return False
  return True


def _IsValidMetric(metric, type_names):
  """Check if a rappor-metric is validly defined.

  Args:
    metric: The JSON rappor-metric to validate.
    type_names: The set of valid type names.
  Returns:
    True iff rappor-metric is valid.
  """
  if 'name' not in metric:
    logging.error('Missing name for rappor-metric')
    return False
  label = 'rappor-metric "%s"' % metric['name']
  if not _CheckRequired(metric, label, ['summary', 'type']):
    return False
  if not metric['owners']:
    logging.error('Missing owners for %s', label)
    return False
  if metric['type'] not in type_names:
    logging.error('Invalid type "%s" for %s', metric['type'], label)
    return False
  for field in metric['flags']:
    if not _FixAndValidateFlagsField(field, label):
      return False
  return True


def IsSimpleStringMetric(metric):
  """Checks if the given metric is a simple string metric.

  Args:
    metric: A metric object, as extracted from _RAPPOR_METRIC_TYPE
  Returns:
    True iff the metric is a simple string metric.
  """
  return all(not metric[dim_type] for dim_type in METRIC_DIMENSION_TYPES)


def _HasErrors(config):
  """Checks that rappor.xml passes some basic validation checks.

  Args:
    config: The parsed rappor.xml contents.

  Returns:
    True iff there are validation errors.
  """
  for noise_level in config['noiseLevels']['levels']:
    if not _IsValidNoiseLevel(noise_level):
      return True
  noise_level_names = _GetNoiseLevelNames(config)
  for rappor_type in config['parameterTypes']['types']:
    if not _IsValidRapporType(rappor_type, noise_level_names):
      return True
  rappor_type_names = _GetTypeNames(config)
  for metric in config['metrics']['metrics']:
    if not _IsValidMetric(metric, rappor_type_names):
      return True
  return False


def _Cleanup(config):
  """Performs cleanup on description contents, such as sorting metrics.

  Args:
    config: The parsed rappor.xml contents.
  """
  config['parameterTypes']['types'].sort(key=operator.itemgetter('name'))
  config['metrics']['metrics'].sort(key=operator.itemgetter('name'))


def UpdateXML(original_xml):
  """Parses the original xml and return a pretty printed version.

  Args:
    original_xml: A string containing the original xml file contents.

  Returns:
    A pretty-printed xml string, or None if the config contains errors.
  """
  config = RAPPOR_XML_TYPE.Parse(original_xml)

  if _HasErrors(config):
    return None

  _Cleanup(config)

  return RAPPOR_XML_TYPE.PrettyPrint(config)
