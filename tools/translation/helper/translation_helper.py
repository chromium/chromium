# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helpers for dealing with translation files."""

from __future__ import print_function

import ast
import os
import re
import sys
import xml.etree.cElementTree as ElementTree

if sys.version_info.major != 2:
  basestring = str  # pylint: disable=redefined-builtin


class GRDFile:
  """Class representing a grd xml file.

  Attributes:
    path: the path to the grd file.
    dir: the path to the the grd's parent directery.
    name: the base name of the grd file.
    grdp_paths: the list of grdp files included in the grd via <part>.
    structure_paths: the paths of any <structure> elements in the grd file.
    xtb_paths: the xtb paths where the grd's translations live.
    lang_to_xtb_path: maps each language to the xtb path for that language.
    appears_translatable: whether the contents of the grd indicate that it's
        supposed to be translated.
    expected_languages: the languages that this grd is expected to have
        translations for, based on the translation expectations file.
  """

  def __init__(self, path):
    self.path = path
    self.dir, self.name = os.path.split(path)
    dom, self.grdp_paths = _parse_grd_file(path)
    self.structure_paths = [os.path.join(self.dir, s.get('file'))
                            for s in dom.findall('.//structure')]
    self.xtb_paths = [os.path.join(self.dir, f.get('path'))
                      for f in dom.findall('.//file')]
    self.lang_to_xtb_path = {}
    self.appears_translatable = (len(self.xtb_paths) != 0 or
                                 dom.find('.//message') is not None)
    self.expected_languages = None

  def _populate_lang_to_xtb_path(self, errors):
    """Populates the lang_to_xtb_path attribute."""
    grd_root = os.path.splitext(self.name)[0]
    lang_pattern = re.compile(r'%s_([^_]+)\.xtb$' % re.escape(grd_root))
    for xtb_path in self.xtb_paths:
      xtb_basename = os.path.basename(xtb_path)
      xtb_lang_match = re.match(lang_pattern, xtb_basename)
      if not xtb_lang_match:
        errors.append('%s: invalid xtb name: %s. xtb name must be %s_<lang>'
                      '.xtb where <lang> is the language code.' %
                      (self.name, xtb_basename, grd_root))
        continue
      xtb_lang = xtb_lang_match.group(1)
      if xtb_lang in self.lang_to_xtb_path:
        errors.append('%s: %s is listed twice' % (self.name, xtb_basename))
        continue
      self.lang_to_xtb_path[xtb_lang] = xtb_path

    return errors


def get_translatable_grds(repo_root, all_grd_paths,
                          translation_expectations_path):
  """Returns all the grds that should be translated as a list of GRDFiles.

  This verifies that every grd file that appears translatable is listed in
  the translation expectations, and that every grd in the translation
  expectations actually exists.

  Args:
    repo_root: The path to the root of the repository.
    all_grd_paths: All grd paths in the repository relative to repo_root.
    translation_expectations_path: The path to the translation expectations
        file, which specifies which grds to translate and into which languages.
  """
  parsed_expectations = _parse_translation_expectations(
      translation_expectations_path)
  grd_to_langs, untranslated_grds, internal_grds = parsed_expectations

  errors = []
  # Make sure that grds in internal_grds aren't processed, since they might
  # contain pieces not available publicly.
  for internal_grd in internal_grds:
    try:
      all_grd_paths.remove(internal_grd)
    except ValueError:
      errors.append(
          '%s is listed in translation expectations as an internal file to be '
          'ignored, but this grd file does not exist.' % internal_grd)
  # Check that every grd that appears translatable is listed in
  # the translation expectations.
  grds_with_expectations = set(grd_to_langs.keys()).union(untranslated_grds)
  all_grds = {p: GRDFile(os.path.join(repo_root, p)) for p in all_grd_paths}
  for path, grd in all_grds.items():
    if grd.appears_translatable:
      if path not in grds_with_expectations:
        errors.append('%s appears to be translatable (because it contains '
            '<file> or <message> elements), but is not listed in the '
            'translation expectations.' % path)

  # Check that every file in translation_expectations exists.
  for path in grds_with_expectations:
    if path not in all_grd_paths:
      errors.append('%s is listed in the translation expectations, but this '
          'grd file does not exist.' % path)

  if errors:
    raise Exception('%s needs to be updated. Please fix these issues:\n - %s' %
                    (translation_expectations_path, '\n - '.join(errors)))

  translatable_grds = []
  for path, expected_languages_list in grd_to_langs.items():
    grd = all_grds[path]
    grd.expected_languages = expected_languages_list
    grd._populate_lang_to_xtb_path(errors)
    translatable_grds.append(grd)

    # Ensure each grd lists the expected languages.
    expected_languages = set(expected_languages_list)
    actual_languages = set(grd.lang_to_xtb_path.keys())
    if expected_languages.difference(actual_languages):
      errors.append('%s: missing translations for these languages: %s. Add '
                    '<file> and <output> elements to the grd file, or update '
                    'the translation expectations.' % (grd.name,
                    sorted(expected_languages.difference(actual_languages))))
    if actual_languages.difference(expected_languages):
      errors.append('%s: references translations for unexpected languages: %s. '
                    'Remove the offending <file> and <output> elements from the'
                    ' grd file, or update the translation expectations.'
                    % (grd.name,
                    sorted(actual_languages.difference(expected_languages))))

  if errors:
    raise Exception('Please fix these issues:\n - %s' %
                    ('\n - '.join(errors)))

  return translatable_grds


def _parse_grd_file(grd_path):
  """Reads a grd(p) file and any subfiles included via <part file="..." />.

  Args:
    grd_path: The path of the .grd or .grdp file.
  Returns:
    A tuple (grd_dom, grdp_paths). dom is an ElementTree DOM for the grd file,
    with the <part> elements inlined. grdp_paths is the list of grdp files that
    were included via <part> elements.
  """
  grdp_paths = []
  grd_dom = ElementTree.parse(grd_path)
  # We modify grd in the loop, so listify this iterable to be safe.
  part_nodes = list(grd_dom.findall('.//part'))
  for part_node in part_nodes:
    grdp_rel_path = part_node.get('file')
    grdp_path = os.path.join(os.path.dirname(grd_path), grdp_rel_path)
    grdp_paths.append(grdp_path)
    grdp_dom, grdp_grdp_paths = _parse_grd_file(grdp_path)
    grdp_paths.extend(grdp_grdp_paths)
    part_node.append(grdp_dom.getroot())
  return grd_dom, grdp_paths


def _parse_translation_expectations(path):
  """Parses a translations expectations file.

  Example translations expectations file:
  {
    "desktop_grds": {
      "languages": ["es", "fr"],
      "files": [
        "ash/ash_strings.grd",
        "ui/strings/ui_strings.grd",
      ],
    },
    "android_grds": {
      "languages": ["de", "pt-BR"],
      "files": [
        "chrome/android/android_chrome_strings.grd",
      ],
    },
    "untranslated_grds": {
      "chrome/locale_settings.grd": "Not UI strings; localized separately",
      "chrome/locale_settings_mac.grd": "Not UI strings; localized separately",
    },
    "internal_grds": [
      "chrome/internal.grd",
    ],
  }

  Returns:
    A tuple (grd_to_langs, untranslated_grds, internal_grds).
    grd_to_langs maps each grd path to the list of languages into which
    that grd should be translated. untranslated_grds is a list of grds
    that "appear translatable" but should not be translated.
    internal_grds is a list of grds that are internal only and should
    not be read by this helper (since they might contain parts not
    available publicly).
  """
  with open(path, encoding='utf-8') as f:
    file_contents = f.read()

  def assert_list_of_strings(l, name):
    assert isinstance(l, list) and all(isinstance(s, basestring) for s in l), (
        '%s must be a list of strings' % name)

  try:
    translations_expectations = ast.literal_eval(file_contents)
    assert isinstance(translations_expectations, dict), (
        '%s must be a python dict' % path)

    grd_to_langs = {}
    untranslated_grds = []
    internal_grds = []

    for group_name, settings in translations_expectations.items():
      if group_name == 'untranslated_grds':
        untranslated_grds = list(settings.keys())
        assert_list_of_strings(untranslated_grds, 'untranslated_grds')
        continue

      if group_name == 'internal_grds':
        internal_grds = settings
        assert_list_of_strings(internal_grds, 'internal_grds')
        continue

      languages = settings['languages']
      files = settings['files']
      assert_list_of_strings(languages, group_name + '.languages')
      assert_list_of_strings(files, group_name + '.files')
      for grd in files:
        grd_to_langs[grd] = languages

    return grd_to_langs, untranslated_grds, internal_grds

  except Exception:
    print('Error: failed to parse', path)
    raise
