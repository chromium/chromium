# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ui/accessibility."""

import json
import os
import re

AX_MOJOM = 'ui/accessibility/ax_enums.mojom'
AUTOMATION_IDL = 'extensions/common/api/automation.idl'

AX_TS_FILE = 'chrome/browser/resources/accessibility/accessibility.ts'
AX_MODE_HEADER = 'ui/accessibility/ax_mode.h'

def InitialLowerCamelCase(unix_name):
  words = unix_name.split('_')
  return words[0] + ''.join(word.capitalize() for word in words[1:])

def CamelToLowerHacker(str):
  out = ''
  for i in range(len(str)):
    if str[i] >= 'A' and str[i] <= 'Z' and out:
      out += '_'
    out += str[i]
  return out.lower()

# Given a full path to an IDL or MOJOM file containing enum definitions,
# parse the file for enums and return a dict mapping the enum name
# to a list of values for that enum.
def GetEnumsFromFile(fullpath, get_raw_enum_value=False):
  enum_name = None
  enums = {}
  for line in open(fullpath, encoding='utf-8').readlines():
    # Strip out comments
    line = re.sub('//.*', '', line)

    # Strip out mojo annotations.
    line = re.sub('\[(.*)\]', '', line)

    # Look for lines of the form "enum ENUM_NAME {" and get the enum_name
    m = re.search('enum ([\w]+) {', line)
    if m:
      enum_name = m.group(1)
      continue

    # Look for a "}" character signifying the end of an enum
    if line.find('}') >= 0:
      enum_name = None
      continue

    if not enum_name:
      continue

    # We're now inside of a enum definition.
    # First, if requested, add the raw line.
    if get_raw_enum_value:
      enums.setdefault(enum_name, [])
      enums[enum_name].append(line)
      continue

    # Add the first string consisting of alphanumerics plus underscore ("\w") to
    # the list of values for that enum.
    m = re.search('([\w]+)', line)
    if m:
      enums.setdefault(enum_name, [])
      enum_value = m.group(1)
      if (enum_value[0] == 'k' and
          enum_value[1] == enum_value[1].upper()):
        enum_value = CamelToLowerHacker(enum_value[1:])
      if enum_value == 'none' or enum_value == 'last':
        continue
      enums[enum_name].append(enum_value)

  return enums

def CheckMatchingEnum(ax_enums,
                      ax_enum_name,
                      automation_enums,
                      automation_enum_name,
                      errs,
                      output_api,
                      strict_ordering=False,
                      allow_extra_destination_enums=False):
  if ax_enum_name not in ax_enums:
    errs.append(output_api.PresubmitError(
        'Expected %s to have an enum named %s' % (AX_MOJOM, ax_enum_name)))
    return
  if automation_enum_name not in automation_enums:
    errs.append(output_api.PresubmitError(
        'Expected %s to have an enum named %s' % (
            AUTOMATION_IDL, automation_enum_name)))
    return
  src = ax_enums[ax_enum_name]
  dst = automation_enums[automation_enum_name]
  if strict_ordering and len(src) != len(dst):
    errs.append(output_api.PresubmitError(
        'Expected %s to have the same number of items as %s' % (
            automation_enum_name, ax_enum_name)))
    return

  if strict_ordering:
    for index, value in enumerate(src):
      lower_value = InitialLowerCamelCase(value)
      if lower_value != dst[index]:
        errs.append(output_api.PresubmitError(
            ('At index %s in enums, unexpected ordering around %s.%s ' +
            'and %s.%s in %s and %s') % (
                index, ax_enum_name, lower_value,
                automation_enum_name, dst[index],
                AX_MOJOM, AUTOMATION_IDL)))
        return
    return

  for value in src:
    lower_value = InitialLowerCamelCase(value)
    if lower_value in dst:
      dst.remove(lower_value)  # Any remaining at end are extra and a mismatch.
    else:
      errs.append(output_api.PresubmitError(
          'Found %s.%s in %s, but did not find %s.%s in %s' % (
              ax_enum_name, value, AX_MOJOM,
              automation_enum_name, InitialLowerCamelCase(value),
              AUTOMATION_IDL)))
  #  Should be no remaining items
  if not allow_extra_destination_enums:
      for value in dst:
          errs.append(output_api.PresubmitError(
              'Found %s.%s in %s, but did not find %s.%s in %s' % (
                  automation_enum_name, value, AUTOMATION_IDL,
                  ax_enum_name, InitialLowerCamelCase(value),
                  AX_MOJOM)))

def CheckEnumsMatch(input_api, output_api):
  repo_root = input_api.change.RepositoryRoot()
  ax_enums = GetEnumsFromFile(os.path.join(repo_root, AX_MOJOM))
  automation_enums = GetEnumsFromFile(os.path.join(repo_root, AUTOMATION_IDL))

  # Focused state only exists in automation.
  automation_enums['StateType'].remove('focused')
  # Offscreen state only exists in automation.
  automation_enums['StateType'].remove('offscreen')

  errs = []
  CheckMatchingEnum(ax_enums, 'Role', automation_enums, 'RoleType', errs,
                    output_api)
  CheckMatchingEnum(ax_enums, 'State', automation_enums, 'StateType', errs,
                    output_api, strict_ordering=True)
  CheckMatchingEnum(ax_enums, 'Action', automation_enums, 'ActionType', errs,
                    output_api, strict_ordering=True)
  CheckMatchingEnum(ax_enums, 'Event', automation_enums, 'EventType', errs,
                    output_api, allow_extra_destination_enums=True)
  CheckMatchingEnum(ax_enums, 'NameFrom', automation_enums, 'NameFromType',
                    errs, output_api)
  CheckMatchingEnum(ax_enums, 'DescriptionFrom', automation_enums,
                    'DescriptionFromType', errs, output_api)
  CheckMatchingEnum(ax_enums, 'Restriction', automation_enums,
                   'Restriction', errs, output_api)
  CheckMatchingEnum(ax_enums, 'DefaultActionVerb', automation_enums,
                   'DefaultActionVerb', errs, output_api)
  CheckMatchingEnum(ax_enums, 'MarkerType', automation_enums,
                   'MarkerType', errs, output_api)
  CheckMatchingEnum(ax_enums, 'Command', automation_enums,
                   'IntentCommandType', errs, output_api)
  CheckMatchingEnum(ax_enums, 'InputEventType', automation_enums,
                   'IntentInputEventType', errs, output_api)
  CheckMatchingEnum(ax_enums, 'TextBoundary', automation_enums,
                   'IntentTextBoundaryType', errs, output_api)
  CheckMatchingEnum(ax_enums, 'MoveDirection', automation_enums,
                   'IntentMoveDirectionType', errs, output_api)
  CheckMatchingEnum(ax_enums, 'SortDirection', automation_enums,
                   'SortDirectionType', errs, output_api)
  CheckMatchingEnum(ax_enums, 'HasPopup', automation_enums,
                   'HasPopup', errs, output_api)
  CheckMatchingEnum(ax_enums, 'AriaCurrentState', automation_enums,
                   'AriaCurrentState', errs, output_api)
  return errs

def CheckAXEnumsOrdinals(input_api, output_api):
  repo_root = input_api.change.RepositoryRoot()
  ax_enums = GetEnumsFromFile(
      os.path.join(repo_root, AX_MOJOM), get_raw_enum_value=True)

  # Find all enums containing enum values with ordinals and save each enum value
  # as a pair e.g. (kEnumValue, 100).
  enums_with_ordinal_values = {}
  for enum_name in ax_enums:
    for enum_value in ax_enums[enum_name]:
      m = re.search("([\w]+) = ([\d]+)", enum_value)
      if not m:
        continue

      enums_with_ordinal_values.setdefault(enum_name, [])
      enums_with_ordinal_values[enum_name].append(m.groups(1))

  # Now, do the validation for each enum.
  errs = []
  for enum_name in enums_with_ordinal_values:
    # This is expected to not be continuous.
    if enum_name == "MarkerType":
      continue

    enum = enums_with_ordinal_values[enum_name]
    enum.sort(key = lambda item: int(item[1]))
    index = 0
    for enum_value in enum:
      if index == int(enum_value[1]):
        index += 1
        continue

      errs.append(output_api.PresubmitError(
          "Unexpected enum %s ordinal: %s = %s. Expected %d." % (
              enum_name, enum_value[0], enum_value[1], index)))

  return errs

# Given a full path to c++ header, return an array of the first static
# constexpr defined. (Note there can be more than one defined in a C++
# header)
def GetConstexprFromFile(fullpath):
  values = []
  for line in open(fullpath, encoding='utf-8').readlines():
    # Strip out comments
    line = re.sub('//.*', '', line)

    # Look for lines of the form "static constexpr <type> NAME "
    m = re.search('static constexpr [\w]+ ([\w]+)', line)
    if m:
      value = m.group(1)
      # Skip first/last sentinels
      if (value in ['kNone', 'kFirstModeFlag', 'kLastModeFlag']):
        continue
      values.append(value)

  return values

# Given a full path to js file, return the AXMode consts
# defined
def GetAccessibilityModesFromFile(fullpath):
  values = []
  inside = False
  for line in open(fullpath, encoding='utf-8').readlines():
    if not inside:
      # Look for the block of code that defines the AXMode enum.
      m = re.search('^enum AxMode {$', line)
      if m:
        inside = True
      continue

    # Look for a "}" character signifying the end of the enum.
    m = re.search('^}$', line)
    if m:
      return values

    m = re.search('([\w]+) = ', line)
    if m:
      values.append(m.group(1))
      continue

  return values

# Make sure that the modes defined in the C++ header match those defined in
# the js file. Note that this doesn't guarantee that the values are the same,
# but does make sure if we add or remove we can signal to the developer that
# they should be aware that this dependency exists.
def CheckModesMatch(input_api, output_api):
  errs = []
  repo_root = input_api.change.RepositoryRoot()

  ax_modes_in_header = GetConstexprFromFile(
    os.path.join(repo_root,AX_MODE_HEADER))
  ax_modes_in_js = GetAccessibilityModesFromFile(
    os.path.join(repo_root, AX_TS_FILE))

  # In TypeScript enum values are NAMED_LIKE_THIS. Transform them to make them
  # comparable to the C++ naming scheme.
  ax_modes_in_js = list(
      map(lambda s: ('k' + s.replace('_', '')).lower(), ax_modes_in_js))

  # The following AxMode values are not used in the UI, and are purposefully
  # omitted.
  unused_ax_modes = [
    'kAXModeBasic',
    'kAXModeWebContentsOnly',
    'kAXModeComplete',
    'kAXModeFormControls',
    'kExperimentalFirstFlag',
    'kExperimentalFormControls',
    'kExperimentalLastFlag',
  ]

  for value in ax_modes_in_header:
    if value in unused_ax_modes:
      continue

    equivalent_value = value.lower()
    if equivalent_value not in ax_modes_in_js:
      errs.append(output_api.PresubmitError(
          'Found %s in %s, but did not find an equivalent value in %s' % (
              value, AX_MODE_HEADER, AX_TS_FILE)))
  return errs

def CheckChangeOnUpload(input_api, output_api):
  errs = []
  for path in input_api.LocalPaths():
    path = path.replace('\\', '/')
    if AX_MOJOM == path:
      errs.extend(CheckEnumsMatch(input_api, output_api))
      errs.extend(CheckAXEnumsOrdinals(input_api, output_api))

    if AX_MODE_HEADER == path:
      errs.extend(CheckModesMatch(input_api, output_api))

  return errs

def CheckChangeOnCommit(input_api, output_api):
  errs = []
  for path in input_api.LocalPaths():
    path = path.replace('\\', '/')
    if AX_MOJOM == path:
      errs.extend(CheckEnumsMatch(input_api, output_api))

    if AX_MODE_HEADER == path:
      errs.extend(CheckModesMatch(input_api, output_api))

  return errs
