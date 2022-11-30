# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import posixpath
import re

from collections import defaultdict


def uniform_path_format(native_path):
  """Alters the path if needed to be separated by forward slashes."""
  return posixpath.normpath(native_path.replace(os.sep, posixpath.sep))

def parse(filename):
  """Searches the file for lines that start with `# TEAM:` or `# COMPONENT:`.

  Args:
    filename (str): path to the file to parse.
  Returns:
    a dict with the following format, with any subset of the listed keys:
    {
        'component': 'component>name',
        'team': 'team@email.here',
        'os': 'Linux|Windows|Mac|Android|Chrome|Fuchsia'
    }
 """
  team_regex = re.compile('\s*#\s*TEAM\s*:\s*(\S+)')
  component_regex = re.compile('\s*#\s*COMPONENT\s*:\s*(\S+)')
  os_regex = re.compile('\s*#\s*OS\s*:\s*(\S+)')
  result = {}
  with open(filename) as f:
    for line in f:
      team_matches = team_regex.match(line)
      if team_matches:
        result['team'] = team_matches.group(1)
      component_matches = component_regex.match(line)
      if component_matches:
        result['component'] = component_matches.group(1)
      os_matches = os_regex.match(line)
      if os_matches:
        result['os'] = os_matches.group(1)
  return result


def aggregate_components_from_owners(all_owners_data, root):
  """Converts the team/component/os tags parsed from OWNERS into mappings.

  Args:
    all_owners_data (dict): A mapping from relative path to a dir to a dict
        mapping the tag names to their values. See docstring for scrape_owners.
    root (str): the path to the src directory.

  Returns:
    A tuple (data, warnings, stats) where data is a dict of the form
      {'component-to-team': {'Component1': 'team1@chr...', ...},
       'teams-per-component': {'Component1': ['team1@chr...', 'team2@chr...]},
       'dir-to-component': {'/path/to/1': 'Component1', ...}}
       'dir-to-team': {'/path/to/1': 'team1@', ...}}
      , warnings is a list of strings, stats is a dict of form
      {'OWNERS-count': total number of OWNERS files,
       'OWNERS-with-component-only-count': number of OWNERS have # COMPONENT,
       'OWNERS-with-team-and-component-count': number of
                          OWNERS have TEAM and COMPONENT,
       'OWNERS-count-by-depth': {directory depth: number of OWNERS},
       'OWNERS-with-component-only-count-by-depth': {directory depth: number
                          of OWNERS have COMPONENT at this depth},
       'OWNERS-with-team-and-component-count-by-depth':{directory depth: ...}}
  """
  stats = {}
  num_total = 0
  num_with_component = 0
  num_with_team_component = 0
  num_total_by_depth = defaultdict(int)
  num_with_component_by_depth = defaultdict(int)
  num_with_team_component_by_depth = defaultdict(int)
  warnings = []
  teams_per_component = defaultdict(set)
  topmost_team = {}
  dir_to_component = {}
  dir_missing_info_by_depth = defaultdict(list)
  dir_to_team = {}
  for rel_dirname, owners_data in all_owners_data.items():
    # Normalize this relative path to posix-style to make counting separators
    # work correctly as a means of obtaining the file_depth.
    rel_path = uniform_path_format(os.path.relpath(rel_dirname, root))
    file_depth = 0 if rel_path == '.' else rel_path.count(posixpath.sep) + 1
    num_total += 1
    num_total_by_depth[file_depth] += 1
    component = owners_data.get('component')
    team = owners_data.get('team')
    os_tag = owners_data.get('os')
    if os_tag and component:
      component = '%s(%s)' % (component, os_tag)
    if team:
      dir_to_team[rel_dirname] = team
    if component:
      num_with_component += 1
      num_with_component_by_depth[file_depth] += 1
      dir_to_component[rel_dirname] = component
      if team:
        num_with_team_component += 1
        num_with_team_component_by_depth[file_depth] += 1
        teams_per_component[component].add(team)
        if component not in topmost_team or file_depth < topmost_team[
            component]['depth']:
          topmost_team[component] = {'depth': file_depth, 'team': team}
    else:
      rel_owners_path = uniform_path_format(os.path.join(rel_dirname, 'OWNERS'))
      warnings.append('%s has no COMPONENT tag' % rel_owners_path)
      if not team and not os_tag:
        dir_missing_info_by_depth[file_depth].append(rel_owners_path)

  mappings = {
      'component-to-team': {k: v['team']
                            for k, v in topmost_team.items()},
      'teams-per-component':
      {k: sorted(list(v))
       for k, v in teams_per_component.items()},
      'dir-to-component': dir_to_component,
      'dir-to-team': dir_to_team,
  }
  warnings += validate_one_team_per_component(mappings)
  stats = {'OWNERS-count': num_total,
           'OWNERS-with-component-only-count': num_with_component,
           'OWNERS-with-team-and-component-count': num_with_team_component,
           'OWNERS-count-by-depth': num_total_by_depth,
           'OWNERS-with-component-only-count-by-depth':
           num_with_component_by_depth,
           'OWNERS-with-team-and-component-count-by-depth':
           num_with_team_component_by_depth,
           'OWNERS-missing-info-by-depth':
           dir_missing_info_by_depth}
  return mappings, warnings, stats


def validate_one_team_per_component(m):
  """Validates that each component is associated with at most 1 team."""
  warnings = []
  # TODO(robertocn): Validate the component names: crbug.com/679540
  teams_per_component = m['teams-per-component']
  for c in teams_per_component:
    if len(teams_per_component[c]) > 1:
      warnings.append('Component %s has the following teams assigned: %s.\n'
                      'Team %s is being used, as it is defined at the OWNERS '
                      'file at the topmost dir'
                      % (
                          c,
                          ', '.join(teams_per_component[c]),
                          m['component-to-team'][c]
                      ))
  return warnings


def scrape_owners(root, include_subdirs):
  """Recursively parse OWNERS files for tags.

  Args:
    root (str): The directory where to start parsing.
    include_subdirs (bool): Whether to generate entries for subdirs with no
        own OWNERS files based on the parent dir's tags.

  Returns a dict in the form below.
  {
      '/path/to/dir': {
          'component': 'component>name',
          'team': 'team@email.here',
          'os': 'Linux|Windows|Mac|Android|Chrome|Fuchsia'
      },
      '/path/to/dir/inside/dir': {
          'component': ...
      }
  }
  """
  data = {}

  def nearest_ancestor_tag(dirname, tag):
    """ Find the value of tag in the nearest ancestor that defines it."""
    ancestor = os.path.dirname(dirname)
    while ancestor:
      rel_ancestor = uniform_path_format(os.path.relpath(ancestor, root))
      if rel_ancestor in data and data[rel_ancestor].get(tag):
        return data[rel_ancestor][tag]
      if rel_ancestor == '.':
        break
      ancestor = os.path.dirname(ancestor)
    return ''

  for dirname, _, files in os.walk(root):
    # Proofing against windows casing oddities.
    owners_file_names = [f for f in files if f.upper() == 'OWNERS']
    rel_dirname = uniform_path_format(os.path.relpath(dirname, root))
    if owners_file_names or include_subdirs:
      if owners_file_names:
        owners_full_path = os.path.join(dirname, owners_file_names[0])
        data[rel_dirname] = parse(owners_full_path)
      else:
        data[rel_dirname] = {}
      for tag in ('component', 'os', 'team'):
        if not tag in data[rel_dirname]:
          ancestor_tag = nearest_ancestor_tag(dirname, tag)
          if ancestor_tag:
            data[rel_dirname][tag] = ancestor_tag
  return data
