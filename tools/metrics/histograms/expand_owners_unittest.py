# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import expand_owners
import os
import shutil
import tempfile
import xml.dom.minidom

_DEFAULT_COMPONENT = '# COMPONENT: Default>Component'


def _GetFileDirective(path):
  """Returns a file directive line.

  Args:
    path: An absolute path, e.g. '/some/directory/chromium/src/tools/OWNERS'.

  Returns:
    A file directive that can be used in an OWNERS file, e.g.
    file://tools/OWNERS.
  """
  return ''.join(['file://', path.split('src/')[1]])


def _GetSrcRelativePath(path):
  """Returns a(n) src-relative path for the given file path.

  Args:
    path: An absolute path, e.g. '/some/directory/chromium/src/tools/OWNERS'.
  """
  return path.split('chromium/')[1]


def _MakeOwnersFile(filename, directory):
  """Makes a temporary file in this directory and returns its absolute path.

  Args:
    filename: A string filename, e.g. 'OWNERS'.
    directory: A string directory under which to make the new file.

  Returns:
    The temporary file's absolute path.
  """
  if not directory:
    directory = os.path.abspath(os.path.join(os.path.dirname(__file__)))
  owners_file = tempfile.NamedTemporaryFile(suffix=filename, dir=directory)
  return os.path.abspath(owners_file.name)


class ExpandOwnersTest(unittest.TestCase):

  def setUp(self):
    super(ExpandOwnersTest, self).setUp()
    self.temp_dir = tempfile.mkdtemp(
        dir=os.path.abspath(os.path.join(os.path.dirname(__file__))))

  def tearDown(self):
    super(ExpandOwnersTest, self).tearDown()
    shutil.rmtree(self.temp_dir)

  def testExpandOwnersWithSimpleOWNERSFilePath(self):
    """Checks that OWNERS files are expanded."""
    absolute_path = _MakeOwnersFile('simple_OWNERS', self.temp_dir)
    src_relative_path = _GetSrcRelativePath(absolute_path)

    with open(absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join(
          ['amy@chromium.org', _DEFAULT_COMPONENT, 'rae@chromium.org']))

    histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>{path}</owner>
  <summary>I like coffee.</summary>
</histogram>

<histogram name="Maple.Syrup" units="units">
  <owner>joe@chromium.org</owner>
  <owner>{path}</owner>
  <owner>kim@chromium.org</owner>
  <summary>I like maple syrup, too.</summary>
</histogram>

</histograms>
""".format(path=src_relative_path))

    expected_histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>amy@chromium.org</owner>
  <owner>rae@chromium.org</owner>
  <summary>I like coffee.</summary>
  <component>Default&gt;Component</component>
</histogram>

<histogram name="Maple.Syrup" units="units">
  <owner>joe@chromium.org</owner>
  <owner>amy@chromium.org</owner>
  <owner>rae@chromium.org</owner>
  <owner>kim@chromium.org</owner>
  <summary>I like maple syrup, too.</summary>
  <component>Default&gt;Component</component>
</histogram>

</histograms>
""")

    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertMultiLineEqual(histograms.toxml(), expected_histograms.toxml())

  def testExpandOwnersWithDuplicateOwners(self):
    """Checks that owners are unique."""
    absolute_path = _MakeOwnersFile('simple_OWNERS', self.temp_dir)
    src_relative_path = _GetSrcRelativePath(absolute_path)

    with open(absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join(
          ['amy@chromium.org', _DEFAULT_COMPONENT, 'rae@chromium.org']))

    histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>rae@chromium.org</owner>
  <owner>{}</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""".format(src_relative_path))

    expected_histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>rae@chromium.org</owner>
  <owner>amy@chromium.org</owner>
  <summary>I like coffee.</summary>
  <component>Default&gt;Component</component>
</histogram>

</histograms>
""")

    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertMultiLineEqual(histograms.toxml(), expected_histograms.toxml())

  def testExpandOwnersWithFileDirectiveOWNERSFilePath(self):
    """Checks that OWNERS files with file directives are expanded."""
    simple_absolute_path = _MakeOwnersFile('simple_OWNERS', self.temp_dir)

    with open(simple_absolute_path, 'w') as owners_file:
      owners_file.write('naz@chromium.org')

    file_directive_absolute_path = (
        _MakeOwnersFile('file_directive_OWNERS', self.temp_dir))
    file_directive_src_relative_path = (
        _GetSrcRelativePath(file_directive_absolute_path))

    directive = _GetFileDirective(simple_absolute_path)
    with open(file_directive_absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join([
          'amy@chromium.org', directive, 'rae@chromium.org', _DEFAULT_COMPONENT
      ]))

    histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>{}</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""".format(file_directive_src_relative_path))

    expected_histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>amy@chromium.org</owner>
  <owner>naz@chromium.org</owner>
  <owner>rae@chromium.org</owner>
  <summary>I like coffee.</summary>
  <component>Default&gt;Component</component>
</histogram>

</histograms>
""")

    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertEqual(histograms.toxml(), expected_histograms.toxml())

  def testExpandOwnersForOWNERSFileWithDuplicateComponents(self):
    """Checks that only one component tag is added if there are duplicates."""
    absolute_path = _MakeOwnersFile('OWNERS', self.temp_dir)
    src_relative_path = _GetSrcRelativePath(absolute_path)

    with open(absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join(['amy@chromium.org', _DEFAULT_COMPONENT]))

    duplicate_owner_absolute_path = (
        _MakeOwnersFile('duplicate_owner_OWNERS', self.temp_dir))
    duplicate_owner_src_relative_path = (
        _GetSrcRelativePath(duplicate_owner_absolute_path))

    with open(duplicate_owner_absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join(['rae@chromium.org', _DEFAULT_COMPONENT]))

    histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>{}</owner>
  <owner>{}</owner>
  <summary>I like coffee.</summary>
  <component>Default&gt;Component</component>
</histogram>

</histograms>
""".format(src_relative_path, duplicate_owner_src_relative_path))

    expected_histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>amy@chromium.org</owner>
  <owner>rae@chromium.org</owner>
  <summary>I like coffee.</summary>
  <component>Default&gt;Component</component>
</histogram>

</histograms>
""")

    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertEqual(histograms.toxml(), expected_histograms.toxml())

  def testExpandOwnersWithoutOWNERSFilePath(self):
    """Checks that histograms without OWNERS file paths are unchanged."""
    histograms_without_file_paths = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <summary>I like coffee.</summary>

</histogram>

</histograms>
""")

    expected_histograms = histograms_without_file_paths
    expand_owners.ExpandHistogramsOWNERS(histograms_without_file_paths)
    self.assertEqual(histograms_without_file_paths, expected_histograms)

  def testExpandOwnersWithoutValidFirstOwner(self):
    """Checks that an error is raised when the first owner is not valid.

    A valid first owner is an individual's email address, e.g. rae@gmail.com,
    or the owner placeholder.
    """
    histograms_without_valid_first_owner = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>src/OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    with self.assertRaisesRegexp(
        expand_owners.Error,
        'The histogram Caffeination must have a valid first owner, i.e. an '
        'individual\'s email address.'):
      expand_owners.ExpandHistogramsOWNERS(histograms_without_valid_first_owner)

  def testExpandOwnersWithFakeFilePath(self):
    """Checks that an error is raised with a fake OWNERS file path."""
    histograms_with_fake_file_path = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chormium.org</owner>
  <owner>src/medium/medium/roast/OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    with self.assertRaisesRegexp(
        expand_owners.Error,
        r'The file at .*src/medium/medium/roast/OWNERS does not exist\.'):
      expand_owners.ExpandHistogramsOWNERS(histograms_with_fake_file_path)

  def testExpandOwnersWithoutOwnersFromFile(self):
    """Checks that an error is raised when no owners can be derived."""
    absolute_path = _MakeOwnersFile('empty_OWNERS', self.temp_dir)
    src_relative_path = _GetSrcRelativePath(absolute_path)

    with open(absolute_path, 'w') as owners_file:
      owners_file.write('')  # Write to the file so that it exists.

    histograms_without_owners_from_file = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chormium.org</owner>
  <owner>{}</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""".format(src_relative_path))

    with self.assertRaisesRegexp(
        expand_owners.Error,
        r'No emails could be derived from .*empty_OWNERS\.'):
      expand_owners.ExpandHistogramsOWNERS(histograms_without_owners_from_file)

  def testExpandOwnersWithoutOWNERSPathPrefix(self):
    """Checks that an error is raised when the path is not well-formatted."""
    histograms_without_src_prefix = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chormium.org</owner>
  <owner>latte/OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    with self.assertRaisesRegexp(
        expand_owners.Error,
        r'The given path latte/OWNERS is not well-formatted.*\.'):
      expand_owners.ExpandHistogramsOWNERS(histograms_without_src_prefix)

  def testExpandOwnersWithoutOWNERSPathSuffix(self):
    """Checks that an error is raised when the path is not well-formatted."""
    histograms_without_owners_suffix = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chormium.org</owner>
  <owner>src/latte/file</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    with self.assertRaisesRegexp(
        expand_owners.Error,
        r'The given path src/latte/file is not well-formatted.*\.'):
      expand_owners.ExpandHistogramsOWNERS(histograms_without_owners_suffix)

  def testExtractEmailAddressesUnsupportedSymbolsIgnored(self):
    """Checks that unsupported OWNERS files symbols are ignored.

    The unsupported symbols that may appear at the beginning of a line are as
    follows:
      (i) per-file
      (ii) *
      (iii) #
      (iv) set noparent
      (v) white space, e.g. a space or a blank line
    """
    absolute_path = _MakeOwnersFile('OWNERS', self.temp_dir)

    joe = 'joe@chromium.org'
    unsupported_symbols = [
        '# Words.', ' # Words.', '*', 'per-file *OWNERS=*', 'set noparent'
    ]

    with open(absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join([joe + '  # Words.', _DEFAULT_COMPONENT] +
                                  unsupported_symbols))

    self.assertEqual(
        expand_owners._ExtractEmailAddressesFromOWNERS(absolute_path), [joe])

  def testExtractEmailAddressesLoopRaisesError(self):
    """Checks that an error is raised if OWNERS file path results in a loop."""
    file_directive_absolute_path = _MakeOwnersFile('loop_OWNERS', self.temp_dir)

    directive = _GetFileDirective(file_directive_absolute_path)
    with open(file_directive_absolute_path, 'w') as owners_file:
      owners_file.write(directive)

    with self.assertRaisesRegexp(
        expand_owners.Error,
        r'.*The path.*loop_OWNERS may be part of an OWNERS loop\.'):
      expand_owners._ExtractEmailAddressesFromOWNERS(
          file_directive_absolute_path)


class GetHigherLevelOwnersFilePathTest(unittest.TestCase):

  def testGetHigherLevelPathDerivedPathInSrcDirectory(self):
    """Checks that higher directories are recursively checked for OWNERS."""
    path = expand_owners._GetOwnersFilePath('src/banana/chocolate/OWNERS')
    self.assertRegexpMatches(
        expand_owners._GetHigherLevelOwnersFilePath(path), r'.*src/OWNERS')

  def testGetHigherLevelPathGivenPathInSrcDirectory(self):
    """Checks that '' is returned when the last directory is reached.

    If the directory above the tools directory is src, then receiving
    'src/OWNERS' is the point at which recursion stops. However, this directory
    may not always be src.
    """
    path_to_chromium_directory = [
        os.path.dirname(__file__), '..', '..', '..', '..'
    ]
    path = os.path.abspath(
        os.path.join(*(path_to_chromium_directory + ['src/OWNERS'])))
    self.assertEqual(expand_owners._GetHigherLevelOwnersFilePath(path), '')


if __name__ == '__main__':
  unittest.main()
