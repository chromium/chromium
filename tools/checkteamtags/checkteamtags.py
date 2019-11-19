#!/usr/bin/env python
# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Makes sure OWNERS files have consistent TEAM and COMPONENT tags."""

from __future__ import print_function

import json
import logging
import optparse
import os
import posixpath
import re
import sys
import urllib2

from collections import defaultdict

from owners_file_tags import parse, uniform_path_format


DEFAULT_MAPPING_URL = \
    'https://storage.googleapis.com/chromium-owners/component_map.json'

# Represent tags that may be found in OWNERS files.
_COMPONENT = 'COMPONENT'
_TEAM = 'TEAM'

# Represents the pattern of a line that contains a TEAM or COMPONENT tag. Note
# that matching lines may not have valid formatting. For example, if {} is
# replaced with TEAM, the line ' #TEAMS :some-team@chromium.org' matches the
# pattern; however, the correct formatting is '# TEAM: some-team@chromium.org'.
LINE_PATTERN = '.*#\s*{}.*:.*'

def _get_entries(lines, tag):
  """Returns a line for the given tag, e.g. '# TEAM: some-team@google.com'.

  Also, validates the tag's prefix format.

  Args:
    lines: A list of lines from which to extract information, e.g.
      ['liz@google.com', '# COMPONENT: UI', '# TEAM: ui-team@chromium.org'].
    tag: The string tag denoting the entry for which to extract information,
      e.g. 'COMPONENT'.

  Raises:
    ValueError: Raised if (A) a line matches the specified line pattern, but
    does not match the  the prefix pattern. This can happen, e.g., if a line is
    '#TEAM: some-team@google.com', which is missing a space between # and TEAM.
    Also, raised if (B) there are multiple lines matching the line pattern.
    This can happen if there are, e.g. two team lines.
  """
  line_pattern = re.compile(LINE_PATTERN.format(tag))
  entries = [line for line in lines if line_pattern.match(line)]

  if not entries:
    return None

  if len(entries) == 1:
    if re.match('^# {}: .*$'.format(tag), entries[0]):
      return entries[0]
    raise ValueError(
        '{} has an invalid prefix. Use \'# {}: \''.format(entries[0], tag))

  raise ValueError('Contains more than one {} per directory'.format(tag))


def rel_and_full_paths(root, owners_path):
  if root:
    full_path = os.path.join(root, owners_path)
    rel_path = owners_path
  else:
    full_path = os.path.abspath(owners_path)
    rel_path = os.path.relpath(owners_path)
  return rel_path, full_path


def validate_mappings(options, args):
  """Ensure team/component mapping remains consistent after patch.

  The main purpose of this check is to notify the user if any edited (or added)
  team tag makes a component map to multiple teams.

  Args:
    options: Command line options from optparse
    args: List of paths to affected OWNERS files
  Returns:
    A string containing the details of any multi-team per component.
  """
  mappings_file = json.load(urllib2.urlopen(options.current_mapping_url))
  new_dir_to_component = mappings_file.get('dir-to-component', {})
  new_dir_to_team = mappings_file.get('dir-to-team', {})

  affected = {}
  deleted = []
  affected_components = set()

  # Parse affected OWNERS files
  for f in args:
    rel, full = rel_and_full_paths(options.root, f)
    if os.path.exists(full):
      affected[os.path.dirname(rel)] = parse(full)
    else:
      deleted.append(os.path.dirname(rel))

  # Update component mapping with current changes.
  for rel_path_native, tags in affected.iteritems():
    # Make the path use forward slashes always.
    rel_path = uniform_path_format(rel_path_native)
    component = tags.get('component')
    team = tags.get('team')
    os_tag = tags.get('os')
    if component:
      if os_tag:
        component = '%s(%s)' % (component, os_tag)
      new_dir_to_component[rel_path] = component
      affected_components.add(component)
    elif rel_path in new_dir_to_component:
      del new_dir_to_component[rel_path]
    if team:
      new_dir_to_team[rel_path] = team
    elif rel_path in new_dir_to_team:
      del new_dir_to_team[rel_path]
  for deleted_dir in deleted:
    if deleted_dir in new_dir_to_component:
      del new_dir_to_component[deleted_dir]
    if deleted_dir in new_dir_to_team:
      del new_dir_to_team[deleted_dir]

  # For the components affected by this patch, compute the directories that map
  # to it.
  affected_component_to_dirs = {}
  for d, component in new_dir_to_component.iteritems():
    if component in affected_components:
      affected_component_to_dirs.setdefault(component, [])
      affected_component_to_dirs[component].append(d)

  # Convert component->[dirs], dir->team to component->[teams].
  affected_component_to_teams = {
      component: list(set([
          new_dir_to_team[d]
          for d in dirs
          if d in new_dir_to_team
      ])) for component, dirs in affected_component_to_dirs.iteritems()
  }

  # Perform cardinality check.
  warnings = ''
  for component, teams in affected_component_to_teams.iteritems():
    if len(teams) > 1:
      warnings += ('\nThe set of all OWNERS files with COMPONENT: %s list '
                   "multiple TEAM's: %s") % (component, ', '.join(teams))
  if warnings:
    warnings = ('Are you sure these are correct? After landing this patch:%s'
                % warnings)

  return warnings


def check_owners(rel_path, full_path):
  """Component and Team check in OWNERS files. crbug.com/667954"""
  def result_dict(error):
    return {
      'error': error,
      'full_path': full_path,
      'rel_path': rel_path,
    }

  if not os.path.exists(full_path):
    return

  with open(full_path) as f:
    owners_file_lines = f.readlines()

  try:
    component_line = _get_entries(owners_file_lines, _COMPONENT)
    team_line = _get_entries(owners_file_lines, _TEAM)
  except ValueError as error:
    return result_dict(str(error))

  if component_line:
    component = component_line.split(':')[1]
    if not component:
      return result_dict('Has COMPONENT line but no component name')
    # Check for either of the following formats:
    #   component1, component2, ...
    #   component1,component2,...
    #   component1 component2 ...
    component_count = max(
        len(component.strip().split()),
        len(component.strip().split(',')))
    if component_count > 1:
      return result_dict('Has more than one component name')
    # TODO(robertocn): Check against a static list of valid components,
    # perhaps obtained from monorail at the beginning of presubmit.

  if team_line:
    team_entry_parts = team_line.split('@')
    if len(team_entry_parts) != 2:
      return result_dict('Has TEAM line, but not exactly 1 team email')
  # TODO(robertocn): Raise a warning if only one of (COMPONENT, TEAM) is
  # present.


def main():
  usage = """Usage: python %prog [--root <dir>] <owners_file1> <owners_file2>...
  owners_fileX  specifies the path to the file to check, these are expected
                to be relative to the root directory if --root is used.

Examples:
  python %prog --root /home/<user>/chromium/src/ tools/OWNERS v8/OWNERS
  python %prog /home/<user>/chromium/src/tools/OWNERS
  python %prog ./OWNERS
  """

  parser = optparse.OptionParser(usage=usage)
  parser.add_option(
      '--root', help='Specifies the repository root.')
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Print debug logging')
  parser.add_option(
      '--bare',
      action='store_true',
      default=False,
      help='Prints the bare filename triggering the checks')
  parser.add_option(
      '--current_mapping_url', default=DEFAULT_MAPPING_URL,
      help='URL for existing dir/component and component/team mapping')
  parser.add_option('--json', help='Path to JSON output file')
  options, args = parser.parse_args()

  levels = [logging.ERROR, logging.INFO, logging.DEBUG]
  logging.basicConfig(level=levels[min(len(levels) - 1, options.verbose)])

  errors = filter(None, [check_owners(*rel_and_full_paths(options.root, f))
                         for f in args])

  warnings = None
  if not errors:
    warnings = validate_mappings(options, args)

  if options.json:
    with open(options.json, 'w') as f:
      json.dump(errors, f)

  if errors:
    if options.bare:
      print('\n'.join(e['full_path'] for e in errors))
    else:
      print('\nFAILED\n')
      print('\n'.join('%s: %s' % (e['full_path'], e['error']) for e in errors))
    return 1
  if not options.bare:
    if warnings:
      print(warnings)
  return 0


if '__main__' == __name__:
  sys.exit(main())
