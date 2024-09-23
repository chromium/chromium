# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for extracting emails and components from OWNERS files."""

import extract_histograms
import json
import os
import subprocess
import sys
import re
import xml_utils

_EMAIL_PATTERN = r'^[\w\-\+\%\.]+\@[\w\-\+\%\.]+$'
_OWNERS = 'OWNERS'
# Three '..' are used because calling dirname() yields the path to this
# module's directory, histograms, and the directory above tools, which may or
# may not be src depending on the machine running the code, is up three
# directory levels from the histograms directory.
DIR_ABOVE_TOOLS = [os.path.dirname(__file__), '..', '..', '..']
SRC = 'src/'


class Error(Exception):
  pass


def _AddTextNodeWithNewLineAndIndent(histogram, node_to_insert_before):
  """Creates and adds a DOM Text Node before the given node in the histogram.

  Args:
    histogram: The histogram node in which to insert a text node.
    node_to_insert_before: A node before which to add the text node.
  """
  histogram.insertBefore(
      histogram.ownerDocument.createTextNode('\n  '),
      node_to_insert_before)


def _IsValidPrimaryOwnerEmail(owner_tag_text):
  """Returns true if |owner_tag_text| is a valid primary owner.

  A valid primary owner is an individual (not a team) with a Chromium or Google
  email address.

  Args:
    owner_tag_text: The text in an owner tag
  """
  if '-' in owner_tag_text:  # Check whether it's a team email address.
    return False

  return (owner_tag_text.endswith('@chromium.org')
          or owner_tag_text.endswith('@google.com'))


def _IsEmail(is_first_owner, owner_tag_text, histogram_name):
  """Returns true if owner_tag_text is an email.

  Also, for histograms that are not obsolete, verifies that a histogram's first
  owner tag contains a valid primary owner.

  Args:
    is_first_owner: True if a histogram's first owner tag is being checked.
    owner_tag_text: The text of the owner tag being checked, e.g.
      'julie@google.com' or 'src/ios/net/cookies/OWNERS'.
    histogram_name: The string name of the histogram.

  Raises:
    Error: Raised if (A) the text is from the first owner tag, (B) the histogram
    is not obsolete, and (C) the text is not a valid primary owner.
  """
  is_email = re.match(_EMAIL_PATTERN, owner_tag_text)
  should_check_owner_email = is_first_owner

  if should_check_owner_email and not _IsValidPrimaryOwnerEmail(owner_tag_text):
    raise Error(
        'The histogram {} must have a valid primary owner, i.e. a Googler '
        'with an @google.com or @chromium.org email address. Please '
        'manually update the histogram with a valid primary owner.'.format(
            histogram_name))

  return is_email


def _IsWellFormattedFilePath(path):
  """Returns True if the given path begins with 'src/' and ends with 'OWNERS'.

  Args:
    path: The path to an OWNERS file, e.g. 'src/gin/OWNERS'.
  """
  return path.startswith(SRC) and path.endswith(_OWNERS)


def _GetHigherLevelOwnersFilePath(path):
  """Returns a path to an OWNERS file at a higher level than the given path.

  Returns an empty string if an OWNERS file path in a higher level directory
  cannot be found.

  Suppose the given path is //stuff/chromium/src/jam/tea/milk/OWNERS. The
  path //stuff/chromium/src/jam/tea/OWNERS will then be generated, and if it
  exists, it will be returned. If not, the path //stuff/chromium/src/jam/OWNERS
  will be generated, and if it exists, it will be returned.

  Args:
    path: The path to an OWNERS file.
  """
  # The highest directory that is searched for component information is one
  # directory lower than the directory above tools. Depending on the machine
  # running this code, the directory above tools may or may not be src.
  path_to_limiting_dir = os.path.abspath(os.path.join(*DIR_ABOVE_TOOLS))
  limiting_dir = path_to_limiting_dir.split(os.sep)[-1]
  owners_file_limit = (os.sep).join([limiting_dir, _OWNERS])
  if path.endswith(owners_file_limit):
    return ''

  parent_directory = os.path.dirname(os.path.dirname(path))
  parent_owners_file_path = os.path.join(parent_directory, _OWNERS)

  if (os.path.exists(parent_owners_file_path) and
    os.path.isfile(parent_owners_file_path)):
    return parent_owners_file_path
  return _GetHigherLevelOwnersFilePath(parent_owners_file_path)


def _GetOwnersFilePath(path):
  """Returns an absolute path that can be opened.

  Args:
    path: A well-formatted path to an OWNERS file, e.g. 'src/courgette/OWNERS'.

  Raises:
    Error: Raised if the given path is not well-formatted.
  """
  if _IsWellFormattedFilePath(path):
    # _SRC is removed because the file system on the machine running the code
    # may not have a(n) src directory.
    path_without_src = path[len(SRC):]

    return os.path.abspath(
        os.path.join(*(DIR_ABOVE_TOOLS + path_without_src.split(os.sep))))

  raise Error(
      'The given path {} is not well-formatted. Well-formatted paths begin '
      'with "src/" and end with "OWNERS"'.format(path))


def _ExtractEmailAddressesFromOWNERS(path, depth=0):
  """Returns a list of email addresses in the given file.

  Args:
    path: The path to an OWNERS file.
    depth: The depth of the recursion, which is used to fail fast in the rare
      case that the OWNERS file path results in a loop.

  Raises:
    Error: Raised in two situations. First, raised if (A) the OWNERS file with
      the given path has a file directive and (B) the OWNERS file indicated by
      the directive does not exist. Second, raised if the depth reaches a
      certain limit.
  """
  # It is unlikely that any chain of OWNERS files will exceed 10 redirections
  # via file:// directives.
  limit = 10
  if (depth > limit):
    raise Error('_ExtractEmailAddressesFromOWNERS has been called {} times. The'
                ' path {} may be part of an OWNERS loop.'.format(limit, path))

  directive = 'file://'
  email_pattern = re.compile(_EMAIL_PATTERN)
  extracted_emails = []

  with open(path, 'r') as owners_file:
    for line in [line.lstrip()
                 for line in owners_file.read().splitlines() if line]:
      index = line.find(' ')
      first_word = line[:index] if index != -1 else line

      if email_pattern.match(first_word):
        extracted_emails.append(first_word)

      elif first_word.startswith(directive):
        next_path = _GetOwnersFilePath(
            os.path.join(SRC, first_word[len(directive):]))

        if os.path.exists(next_path) and os.path.isfile(next_path):
          extracted_emails.extend(
              _ExtractEmailAddressesFromOWNERS(next_path, depth + 1))
        else:
          raise Error('The path derived from {} does not exist. '
                      'Derived path: {}'.format(first_word, next_path))

  return extracted_emails


def _ComponentFromDirmd(json_data, subpath):
  """Returns the component for a subpath based on dirmd output.

  Returns an empty string if no component can be extracted.

  Args:
    json_data: json object output from dirmd.
    subpath: The subpath for the directory being queried, e.g. src/storage'.
  """
  return json_data.get('dirs', {}).get(subpath,
                                       {}).get('buganizerPublic',
                                               {}).get('componentId', '')


# Memoize decorator from: https://stackoverflow.com/a/1988826
# TODO(asvitkine): Replace with @functools.cache once we're on Python 3.9+.
class Memoize:
  def __init__(self, f):
    self.f = f
    self.memo = {}

  def __call__(self, *args):
    if not args in self.memo:
      self.memo[args] = self.f(*args)
    return self.memo[args]


def _MakeOwners(document, path, emails_with_dom_elements):
  """Makes DOM Elements for owners and returns the elements.

  The owners are extracted from the OWNERS file with the given path and
  deduped using the given set emails_with_dom_elements. This set has email
  addresses that were explicitly listed as histogram owners, e.g.
  <owner>liz@chromium.org</owner>. If a histogram has multiple OWNERS file
  paths, e.g. <owner>src/cc/OWNERS</owner> and <owner>src/ui/OWNERS</owner>,
  then the given set also contains any email addresses that have already been
  extracted from OWNERS files.

  New owners that are extracted from the given file are also added to
  emails_with_dom_elements.

  Args:
    document: The Document to which the new owners elements will belong.
    path: The absolute path to an OWNERS file.
    emails_with_dom_elements: The set of email addresses that already have
      corresponding DOM Elements.

  Returns:
    A collection of DOM Elements made from owners in the given OWNERS file.
  """
  owner_elements = []
  # TODO(crbug.com/41472818): An OWNERS file API would be ideal.
  emails_from_owners_file = _ExtractEmailAddressesFromOWNERS(path)
  if not emails_from_owners_file:
    raise Error('No emails could be derived from {}.'.format(path))

  # A list is used to respect the order of email addresses in the OWNERS file.
  deduped_emails_from_owners_file = []
  for email in emails_from_owners_file:
    if email not in emails_with_dom_elements:
      deduped_emails_from_owners_file.append(email)
      emails_with_dom_elements.add(email)

  for email in deduped_emails_from_owners_file:
    owner_element = document.createElement('owner')
    owner_element.appendChild(document.createTextNode(email))
    owner_elements.append(owner_element)
  return owner_elements


def _UpdateHistogramOwners(histogram, owner_to_replace, owners_to_add):
  """Replaces |owner_to_replace| with |owners_to_add| for the given histogram.

  Args:
    histogram: The DOM Element to update.
    owner: The DOM Element to be replaced. This is a child node of histogram,
      and its text is a file path to an OWNERS file, e.g. 'src/mojo/OWNERS'
    owners_to_add: A collection of DOM Elements with which to replace
      owner_to_replace.
  """
  node_after_owners_file = owner_to_replace.nextSibling
  replacement_done = False

  for owner_to_add in owners_to_add:
    if not replacement_done:
      histogram.replaceChild(owner_to_add, owner_to_replace)
      replacement_done = True
    else:
      _AddTextNodeWithNewLineAndIndent(histogram, node_after_owners_file)
      histogram.insertBefore(owner_to_add, node_after_owners_file)


@Memoize
def ExtractComponentViaDirmd(path):
  """Returns the component for Buganizer issues at the given path.

  Examples are '1287811' and '1456399'.

  Uses dirmd in third_party/depot_tools to parse metadata and walk parent
  directories up to the top level of the repo.

  Returns an empty string if no component can be extracted.

  Args:
    path: The path to a directory to query, e.g. 'src/storage'.
  """
  # Verify that the paths are absolute and the root is a parent of the
  # passed in path.
  root_path = os.path.abspath(os.path.join(*DIR_ABOVE_TOOLS))
  path = os.path.abspath(path)
  if not path.startswith(root_path):
    raise Error('Path {} is not a subpath of the root path {}.'.format(
        path, root_path))
  subpath = path[len(root_path) + 1:] or '.'  # E.g. content/public.
  dirmd_exe = 'dirmd'
  if sys.platform == 'win32':
    dirmd_exe = 'dirmd.bat'
  dirmd_path = os.path.join(*(DIR_ABOVE_TOOLS +
                              ['third_party', 'depot_tools', dirmd_exe]))
  dirmd_command = [dirmd_path, 'read', '-form', 'sparse', root_path, path]
  dirmd = subprocess.Popen(dirmd_command,
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
  if dirmd.wait() != 0:
    raise Error('dirmd failed: "' + ' '.join(dirmd_command) + '": ' +
                dirmd.stderr.read().decode('utf-8'))
  json_out = json.load(dirmd.stdout)
  # On Windows, dirmd output still uses Unix path separators.
  if sys.platform == 'win32':
    subpath = subpath.replace('\\', '/')
  return _ComponentFromDirmd(json_out, subpath)


def AddHistogramComponent(histogram, component):
  """Makes a DOM Element for the component and adds it to the given histogram.

  Args:
    histogram: The DOM Element to update.
    component: A string component to add, e.g. 'Internals>Network' or 'Build'.
  """
  node_to_insert_before = histogram.lastChild
  _AddTextNodeWithNewLineAndIndent(histogram, node_to_insert_before)

  document = histogram.ownerDocument
  component_element = document.createElement('component')
  component_element.appendChild(document.createTextNode(component))
  histogram.insertBefore(component_element, node_to_insert_before)


def ExpandHistogramsOWNERS(histograms):
  """Updates the given DOM Element's descendants, if necessary.

  When a histogram has an owner node whose text is an OWNERS file path rather
  than an email address, e.g. <owner>src/base/android/OWNERS</owner> instead of
  <owner>joy@chromium.org</owner>, then (A) the histogram's owners need to be
  updated and (B) a component may be added.

  If the text of an owner node is an OWNERS file path, then this node is
  replaced by owner nodes for the emails derived from the OWNERS file. If a
  component, e.g. 1287811, can be derived from the OWNERS file or an OWNERS file
  in a higher-level directory, then a component tag will be added to the
  histogram, e.g. <component>1287811</component>.

  Args:
    histograms: The DOM Element whose descendants may be updated.

  Raises:
    Error: Raised if the OWNERS file with the given path does not exist.
  """
  email_pattern = re.compile(_EMAIL_PATTERN)
  iter_matches = xml_utils.IterElementsWithTag

  for histogram in iter_matches(histograms, 'histogram'):
    owners = [owner for owner in iter_matches(histogram, 'owner', 1)]

    # owner is a DOM Element with a single child, which is a DOM Text Node.
    emails_with_dom_elements = set([
        owner.childNodes[0].data
        for owner in owners
        if email_pattern.match(owner.childNodes[0].data)])

    # component is a DOM Element with a single child, which is a DOM Text Node.
    components_with_dom_elements = set(
        xml_utils.NormalizeString(component.childNodes[0].data)
        for component in iter_matches(histogram, 'component', 1))

    for index, owner in enumerate(owners):
      owner_text = owner.childNodes[0].data.strip()
      name = histogram.getAttribute('name')
      if _IsEmail(index == 0, owner_text, name):
        continue

      path = _GetOwnersFilePath(owner_text)
      if not os.path.exists(path) or not os.path.isfile(path):
        raise Error('The file at {} does not exist.'.format(path))

      owners_to_add = _MakeOwners(
        owner.ownerDocument, path, emails_with_dom_elements)
      if not owners_to_add:
        continue

      _UpdateHistogramOwners(histogram, owner, owners_to_add)

      component = ExtractComponentViaDirmd(os.path.dirname(path))
      if component and component not in components_with_dom_elements:
        components_with_dom_elements.add(component)
        AddHistogramComponent(histogram, component)
