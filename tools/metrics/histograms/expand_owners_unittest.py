# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import expand_owners
import mock
import os
import shutil
import tempfile
import xml.dom.minidom


def _GetToolsParentDir():
  """Returns an absolute path to the the tools directory's parent directory.

  Example: 'C:\a\n\ff\' or '/opt/n/ff/'.
  """
  return os.path.abspath(os.path.join(*expand_owners.DIR_ABOVE_TOOLS))


def _GetFileDirective(path):
  """Returns a file directive line.

  Args:
    path: An absolute path, e.g. '/some/directory/subdirectory/tools/OWNERS'.

  Returns:
    A file directive that can be used in an OWNERS file, e.g.
    file://tools/OWNERS.
  """
  return ''.join(['file://', path[len(_GetToolsParentDir()) + 1:]])


def _GetSrcRelativePath(path):
  """Returns a(n) src-relative path for the given file path.

  Args:
    path: An absolute path, e.g. '/some/directory/subdirectory/tools/OWNERS'.

  Returns:
    A src-relative path, e.g.'src/tools/OWNERS'.
  """
  assert path.startswith(_GetToolsParentDir())
  return expand_owners.SRC + path[len(_GetToolsParentDir()) + 1:]


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

    # The below construction is used rather than __file__.endswith() because
    # the file extension could be .py or .pyc.
    assert os.sep.join(
        ['tools', 'metrics', 'histograms',
         'expand_owners_unittest.py']) in __file__

  def tearDown(self):
    super(ExpandOwnersTest, self).tearDown()
    shutil.rmtree(self.temp_dir)

  def testExpandOwnersUsesMetadataOverOwners(self):
    """Checks that DIR_METADATA is used if available"""
    with open(os.path.join(self.temp_dir, 'DIR_METADATA'), "w+") as md:
      md.write("\n".join([
          'monorail {', 'component: "Bees"', '}', 'buganizer_public {',
          'component_id:123456', '}'
      ]))
    absolute_path = _MakeOwnersFile('simple_OWNERS', self.temp_dir)
    with open(absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join(['amy@chromium.org', 'rae@chromium.org']))
    self.maxDiff = None
    src_relative_path = _GetSrcRelativePath(absolute_path)
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
  <component>123456</component>
</histogram>

<histogram name="Maple.Syrup" units="units">
  <owner>joe@chromium.org</owner>
  <owner>amy@chromium.org</owner>
  <owner>rae@chromium.org</owner>
  <owner>kim@chromium.org</owner>
  <summary>I like maple syrup, too.</summary>
  <component>123456</component>
</histogram>

</histograms>
""")

    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertMultiLineEqual(histograms.toxml(), expected_histograms.toxml())

  @mock.patch('expand_owners.ExtractComponentViaDirmd')
  def testExpandOwnersWithSimpleOWNERSFilePath(self, mock_dirmd_extract):
    """Checks that OWNERS files are expanded."""
    mock_dirmd_extract.return_value = None
    absolute_path = _MakeOwnersFile('simple_OWNERS', self.temp_dir)
    src_relative_path = _GetSrcRelativePath(absolute_path)

    with open(absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join(
          ['amy@chromium.org', 'rae@chromium.org']))

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
</histogram>

<histogram name="Maple.Syrup" units="units">
  <owner>joe@chromium.org</owner>
  <owner>amy@chromium.org</owner>
  <owner>rae@chromium.org</owner>
  <owner>kim@chromium.org</owner>
  <summary>I like maple syrup, too.</summary>
</histogram>

</histograms>
""")

    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertMultiLineEqual(histograms.toxml(), expected_histograms.toxml())

  @mock.patch('expand_owners.ExtractComponentViaDirmd')
  def testExpandOwnersWithLongFilePath(self, mock_dirmd_extract):
    """Checks that long OWNERS file paths are supported.

    Most OWNERS file paths appear between owners tags on the same line, e.g.
    <owner>src/chrome/browser</owner>. However, especially long paths may appear
    on their own line between the tags.
    """
    mock_dirmd_extract.return_value = None
    absolute_path = _MakeOwnersFile('simple_OWNERS', self.temp_dir)
    src_relative_path = _GetSrcRelativePath(absolute_path)

    with open(absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join(['amy@chromium.org']))

    histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>
    {path}
  </owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""".format(path=src_relative_path))

    expected_histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>amy@chromium.org</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertMultiLineEqual(histograms.toxml(), expected_histograms.toxml())

  @mock.patch('expand_owners.ExtractComponentViaDirmd')
  def testExpandOwnersWithDuplicateOwners(self, mock_dirmd_extract):
    """Checks that owners are unique."""
    mock_dirmd_extract.return_value = None
    absolute_path = _MakeOwnersFile('simple_OWNERS', self.temp_dir)
    src_relative_path = _GetSrcRelativePath(absolute_path)

    with open(absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join(
          ['amy@chromium.org', 'rae@chromium.org']))

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
</histogram>

</histograms>
""")

    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertMultiLineEqual(histograms.toxml(), expected_histograms.toxml())

  @mock.patch('expand_owners.ExtractComponentViaDirmd')
  def testExpandOwnersWithFileDirectiveOWNERSFilePath(self, mock_dirmd_extract):
    """Checks that OWNERS files with file directives are expanded."""
    mock_dirmd_extract.return_value = None
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
          'amy@chromium.org', directive, 'rae@chromium.org',
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
</histogram>

</histograms>
""")

    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertEqual(histograms.toxml(), expected_histograms.toxml())

  @mock.patch('expand_owners.ExtractComponentViaDirmd')
  def testExpandOwnersForOWNERSFileWithDuplicateComponents(
      self, mock_dirmd_extract):
    """Checks that only one component tag is added if there are duplicates."""
    mock_dirmd_extract.return_value = None
    absolute_path = _MakeOwnersFile('OWNERS', self.temp_dir)
    src_relative_path = _GetSrcRelativePath(absolute_path)

    with open(absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join(['amy@chromium.org']))

    duplicate_owner_absolute_path = (
        _MakeOwnersFile('duplicate_owner_OWNERS', self.temp_dir))
    duplicate_owner_src_relative_path = (
        _GetSrcRelativePath(duplicate_owner_absolute_path))

    with open(duplicate_owner_absolute_path, 'w') as owners_file:
      owners_file.write('\n'.join(['rae@chromium.org']))

    histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>{}</owner>
  <owner>{}</owner>
  <summary>I like coffee.</summary>
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

  def testExpandOwnersWithoutValidPrimaryOwner_OwnersPath(self):
    """Checks that an error is raised when the primary owner is a file path.

    A valid primary owner is an individual's email address, e.g. rae@google.com,
    sam@chromium.org, or the owner placeholder.
    """
    histograms_without_valid_first_owner = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>src/OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    with self.assertRaisesRegex(
        expand_owners.Error,
        'The histogram Caffeination must have a valid primary owner, i.e. a '
        'Googler with an @google.com or @chromium.org email address.'):
      expand_owners.ExpandHistogramsOWNERS(histograms_without_valid_first_owner)

  def testExpandOwnersWithoutValidPrimaryOwner_TeamEmail(self):
    """Checks that an error is raised when the primary owner is a team.

    A valid primary owner is an individual's email address, e.g. rae@google.com,
    sam@chromium.org, or the owner placeholder.
    """
    histograms_without_valid_first_owner = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>coffee-team@google.com</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    with self.assertRaisesRegex(
        expand_owners.Error,
        'The histogram Caffeination must have a valid primary owner, i.e. a '
        'Googler with an @google.com or @chromium.org email address.'):
      expand_owners.ExpandHistogramsOWNERS(histograms_without_valid_first_owner)

  def testExpandOwnersWithoutValidPrimaryOwner_InvalidEmail(self):
    """Checks that an error is raised when the primary owner's email is invalid.

    A valid primary owner is an individual's email address, e.g. rae@google.com,
    sam@chromium.org, or the owner placeholder.
    """
    histograms_without_valid_first_owner = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>alex@coffee.com</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    with self.assertRaisesRegex(
        expand_owners.Error,
        'The histogram Caffeination must have a valid primary owner, i.e. a '
        'Googler with an @google.com or @chromium.org email address.'):
      expand_owners.ExpandHistogramsOWNERS(histograms_without_valid_first_owner)

  def testExpandOwnersWithFakeFilePath(self):
    """Checks that an error is raised with a fake OWNERS file path."""
    histograms_with_fake_file_path = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>src/medium/medium/roast/OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    with self.assertRaisesRegex(
        expand_owners.Error, r'The file at .*medium.*OWNERS does not exist\.'):
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
  <owner>joe@chromium.org</owner>
  <owner>{}</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""".format(src_relative_path))

    with self.assertRaisesRegex(
        expand_owners.Error,
        r'No emails could be derived from .*empty_OWNERS\.'):
      expand_owners.ExpandHistogramsOWNERS(histograms_without_owners_from_file)

  def testExpandOwnersWithSameOwners(self):
    """
    Checks that no error is raised when all owners in a file are already in
    <owner> elements.
    """
    absolute_path = _MakeOwnersFile('same_OWNERS', self.temp_dir)
    src_relative_path = _GetSrcRelativePath(absolute_path)

    with open(absolute_path, 'w') as owners_file:
      owners_file.write(
          'joe@chromium.org')  # Write to the file so that it exists.

    histograms_string = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>{}</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""".format(src_relative_path))

    self.assertIsNone(expand_owners.ExpandHistogramsOWNERS(histograms_string))

  def testExpandOwnersWithoutOWNERSPathPrefix(self):
    """Checks that an error is raised when the path is not well-formatted."""
    histograms_without_src_prefix = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>latte/OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    with self.assertRaisesRegex(
        expand_owners.Error,
        r'The given path latte/OWNERS is not well-formatted.*\.'):
      expand_owners.ExpandHistogramsOWNERS(histograms_without_src_prefix)

  def testExpandOwnersWithoutOWNERSPathSuffix(self):
    """Checks that an error is raised when the path is not well-formatted."""
    histograms_without_owners_suffix = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>src/latte/file</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")

    with self.assertRaisesRegex(
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
      owners_file.write('\n'.join([joe + '  # Words.'] + unsupported_symbols))

    self.assertEqual(
        expand_owners._ExtractEmailAddressesFromOWNERS(absolute_path), [joe])

  def testExtractEmailAddressesLoopRaisesError(self):
    """Checks that an error is raised if OWNERS file path results in a loop."""
    file_directive_absolute_path = _MakeOwnersFile('loop_OWNERS', self.temp_dir)

    directive = _GetFileDirective(file_directive_absolute_path)
    with open(file_directive_absolute_path, 'w') as owners_file:
      owners_file.write(directive)

    with self.assertRaisesRegex(
        expand_owners.Error,
        r'.*The path.*loop_OWNERS may be part of an OWNERS loop\.'):
      expand_owners._ExtractEmailAddressesFromOWNERS(
          file_directive_absolute_path)

  def testGetHigherLevelPath(self):
    """Checks that higher directories are recursively checked for OWNERS.

    Also, checks that there isn't a recursive loop.
    """
    path = expand_owners._GetOwnersFilePath('src/banana/chocolate/OWNERS')
    result = expand_owners._GetHigherLevelOwnersFilePath(path)

    # The condition is true when the tools directory's parent directory is src,
    # which is generally the case locally. However, the parent directory is not
    # always src, e.g. on various testing bots.
    if os.path.basename(_GetToolsParentDir()) == 'src':
      self.assertRegex(result, r'.*OWNERS')
    else:
      self.assertEqual(result, '')


if __name__ == '__main__':
  unittest.main()
