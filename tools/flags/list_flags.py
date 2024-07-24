#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Emits a formatted, optionally filtered view of the list of flags.
"""

from __future__ import print_function

import argparse
import os
import re
import sys

import flags_utils

DEPOT_TOOLS_PATH = os.path.join(flags_utils.ROOT_PATH, 'third_party',
                                'depot_tools')

sys.path.append(DEPOT_TOOLS_PATH)

import owners_client


def keep_never_expires(flags):
  """Filter flags to contain only flags that never expire.

  >>> keep_never_expires([{'expiry_milestone': -1}, {'expiry_milestone': 2}])
  [{'expiry_milestone': -1}]
  """
  return [f for f in flags if f['expiry_milestone'] == -1]


def resolve_owners(flags):
  """Resolves sets of owners for every flag in the provided list.

  Given a list of flags, for each flag, resolves owners for that flag. Resolving
  owners means, for each entry in a flag's owners list:
  * Turning owners files references into the transitive set of owners listed in
    those files
  * Turning bare usernames into @chromium.org email addresses
  * Passing any other type of entry through unmodified
  """

  owners_db = owners_client.GetCodeOwnersClient(
      host="chromium-review.googlesource.com",
      project="chromium/src",
      branch="main")

  new_flags = []
  for f in flags:
    new_flag = f.copy()
    new_owners = set()
    for o in f['owners']:
      # Assume any filepath is to an OWNERS file.
      if '/' in o:
        new_owners.update(set(owners_db.ListBestOwners(re.sub('//', '', o))))
      elif '@' not in o:
        new_owners.add(o + '@chromium.org')
      else:
        new_owners.add(o)
    new_flag['resolved_owners'] = sorted(new_owners)
    new_flags.append(new_flag)
  return new_flags


def find_unused(flags):
  FLAG_FILES = [
      'chrome/browser/about_flags.cc',
      'ios/chrome/browser/flags/about_flags.mm',
  ]
  flag_files_data = [open(f, 'r', encoding='utf-8').read() for f in FLAG_FILES]
  unused_flags = []
  for flag in flags:
    # Search for the name in quotes.
    needle = '"%s"' % flag['name']
    if not any([needle in data for data in flag_files_data]):
      unused_flags.append(flag)
  return unused_flags


def filter_by_owners(flags, owners):
  """Given a list of owners, returns all flags which have any owner appearing
  in the list. The `owners` arg is a list of owners.

  Need exact match and need to include @google.com or @chromium.org in the
  argument. This is because the owner with ldap only is extended with
  @chromium.org automatically via resolve_owners function.
  TODO(zhangwenyu): Support filter by ldap.

  >>> f1 = {'name': 'f_1', 'owners': ['b@g.com']}
  >>> f1['resolved_owners'] = ['b@g.com']
  >>> f2 = {'name': 'f_2', 'owners': ['z']}
  >>> f2['resolved_owners'] = ['z@c.org']
  >>> f3 = {'name': 'f_3', 'owners': ['c@g.com', 'd@g.com']}
  >>> f3['resolved_owners'] = ['c@g.com', 'd@g.com']

  >>> filter_by_owners([f1, f2, f3], ['b@g.com'])
  [{'name': 'f_1', 'owners': ['b@g.com'], 'resolved_owners': ['b@g.com']}]
  >>> filter_by_owners([f1, f2, f3], ['z@c.org'])
  [{'name': 'f_2', 'owners': ['z'], 'resolved_owners': ['z@c.org']}]
  >>> filter_by_owners([f1, f2, f3], ['z']) # Filter by ldap not supported.
  []
  >>> filter_by_owners([f1, f2, f3], ['b@g.co']) # Need exact match.
  []
  >>> filter_by_owners([f1, f2, f3], ['b@g.com', 'z@c.org'])
  [{'name': 'f_1', 'owners': ['b@g.com'], 'resolved_owners': ['b@g.com']}, {'name': 'f_2', 'owners': ['z'], 'resolved_owners': ['z@c.org']}]
  >>> filter_by_owners([f1, f2, f3], ['c@g.com', 'd@g.com'])
  [{'name': 'f_3', 'owners': ['c@g.com', 'd@g.com'], 'resolved_owners': ['c@g.com', 'd@g.com']}]
  """

  # A helper function to check if there is any intersection between flag's
  # owners and targeted owners.
  def matches_any_owner(flag):
    return set(flag['resolved_owners']) & set(owners)

  return list(filter(matches_any_owner, flags))


def print_flags(flags, verbose):
  """Prints the supplied list of flags.

  In verbose mode, prints name, expiry, and owner list; in non-verbose mode,
  prints just the name. Verbose mode is actually tab-separated values, with
  commas used as separators within individual fields - this is the format the
  rest of the flags automation consumes most readily.

  >>> f1 = {'name': 'foo', 'expiry_milestone': 73, 'owners': ['bar', 'baz']}
  >>> f1['resolved_owners'] = ['bar@c.org', 'baz@c.org']
  >>> f2 = {'name': 'bar', 'expiry_milestone': 74, 'owners': ['//quxx/OWNERS']}
  >>> f2['resolved_owners'] = ['quxx@c.org']
  >>> print_flags([f1], False)
  foo
  >>> print_flags([f1], True) # doctest: +NORMALIZE_WHITESPACE
  foo 73 bar,baz bar@c.org,baz@c.org
  >>> print_flags([f2], False)
  bar
  >>> print_flags([f2], True) # doctest: +NORMALIZE_WHITESPACE
  bar 74 //quxx/OWNERS quxx@c.org
  """
  for f in flags:
    if verbose:
      print('%s\t%d\t%s\t%s' % (f['name'], f['expiry_milestone'], ','.join(
          f['owners']), ','.join(f['resolved_owners'])))
    else:
      print(f['name'])


def main():
  import doctest
  doctest.testmod()

  parser = argparse.ArgumentParser(description=__doc__)
  group = parser.add_mutually_exclusive_group()
  group.add_argument('-n', '--never-expires', action='store_true')
  group.add_argument('-e', '--expired-by', type=int)
  group.add_argument('-u', '--find-unused', action='store_true')
  # The -o argument could be a single owner or multiple owners joined by ','.
  group.add_argument('-o', '--has-owner', type=str)
  parser.add_argument('-v', '--verbose', action='store_true')
  parser.add_argument('--testonly', action='store_true')
  args = parser.parse_args()

  if args.testonly:
    return

  flags = flags_utils.load_metadata()
  if args.expired_by:
    flags = flags_utils.keep_expired_by(flags, args.expired_by)
  if args.never_expires:
    flags = keep_never_expires(flags)
  if args.find_unused:
    flags = find_unused(flags)
  flags = resolve_owners(flags)
  # Filter by owner after resolving owners completed, so it understands
  # owners file.
  if args.has_owner:
    owners = [o.strip() for o in args.has_owner.split(',')]
    flags = filter_by_owners(flags, owners)
  print_flags(flags, args.verbose)


if __name__ == '__main__':
  main()
