# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''The <output> and <file> elements.
'''


import copy
import os
import re

from grit import constants
from grit import xtb_reader
from grit.node import base

DATA_PACKAGE_FILENAME_RE = re.compile(r'((\w|-)+).pak')
ANDROID_FILENAME_RE = re.compile(r'(values(-((\w|-|\+)+))?)/')


class FileNode(base.Node):
  '''A <file> element.'''

  def __init__(self):
    super().__init__()
    self.re = None
    self.should_load_ = True

  def IsTranslation(self):
    return True

  def GetLang(self):
    return self.attrs['lang']

  def DisableLoading(self):
    self.should_load_ = False

  def MandatoryAttributes(self):
    return ['path', 'lang']

  def RunPostSubstitutionGatherer(self, debug=False):
    if not self.should_load_:
      return

    root = self.GetRoot()
    defs = getattr(root, 'defines', {})
    target_platform = getattr(root, 'target_platform', '')

    xtb_file = open(self.ToRealPath(self.GetInputPath()), 'rb')
    try:
      lang = xtb_reader.Parse(xtb_file,
                              self.UberClique().GenerateXtbParserCallback(
                                  self.attrs['lang'], debug=debug),
                              defs=defs,
                              target_platform=target_platform)
    except:
      print("Exception during parsing of %s" % self.GetInputPath())
      raise
    # Translation console uses non-standard language codes 'iw' and 'no' for
    # Hebrew and Norwegian Bokmal instead of 'he' and 'nb' used in Chrome.
    # Note that some Chrome's .grd still use 'no' instead of 'nb', but 'nb' is
    # always used for generated .pak files.
    ALTERNATIVE_LANG_CODE_MAP = { 'he': 'iw', 'nb': 'no' }
    assert (lang == self.attrs['lang'] or
            lang == ALTERNATIVE_LANG_CODE_MAP[self.attrs['lang']]), (
            'The XTB file you reference must contain messages in the language '
            'specified\nby the \'lang\' attribute.')

  def GetInputPath(self):
    return os.path.expandvars(self.attrs['path'])


class OutputNode(base.Node):
  '''An <output> element.'''

  def EndParsing(self):
    super().EndParsing()

    # 'data_package' and 'android' types are expected to always have a gender
    # set, even if self.translate_genders=False.
    if self.GetType() == 'data_package' or self.GetType() == 'android':
      self.gender = constants.DEFAULT_GENDER

    if not self.translate_genders:
      return

    # Create 3 extra copies of each data_package and android output node so that
    # we have one per gender per language. Adjust file names accordingly.
    if self.GetType() == 'data_package' or self.GetType() == 'android':
      for gender in constants.TRANSLATED_GENDERS:
        cloned_node = self._Clone()
        cloned_node.gender = gender
        cloned_node._AddGenderToFilenames()

      self._AddGenderToFilenames()

  def _AddGenderToFilenames(self):
    if hasattr(self, 'output_filename'):
      self.output_filename = self._AddGenderToFilename(self.output_filename)
    self.attrs['filename'] = self._AddGenderToFilename(self.attrs['filename'])

  def _Clone(self):
    # Temporarily remove the parent to avoid deep-copying it.
    parent = self.parent
    self.parent = None
    self_copy = copy.deepcopy(self)

    self.parent = parent
    self_copy.parent = parent
    parent.AddChild(self_copy)

    return self_copy

  def _AddGenderToFilename(self, path):
    assert self.GetType() == 'data_package' or self.GetType() == 'android'

    if self.GetGender() == constants.DEFAULT_GENDER:
      return path

    if self.GetType() == 'data_package':
      match = DATA_PACKAGE_FILENAME_RE.search(path)
      assert match is not None, f'unrecognized data_package path: {path}'
      return path.replace(match.group(),
                          f'{match.group(1)}_{self.GetGender()}.pak')
    else:  # self.GetType() == 'android'
      match = ANDROID_FILENAME_RE.search(path)
      assert match is not None, f'unrecognized android path: {path}'
      return path.replace(match.group(1),
                          f'{match.group(1)}-{self.GetGender()}')

  def MandatoryAttributes(self):
    return ['filename', 'type']

  def DefaultAttributes(self):
    return {
      'lang' : '', # empty lang indicates all languages
      'language_section' : 'neutral', # defines a language neutral section
      'context' : '',
      'fallback_to_default_layout' : 'true',
    }

  def GetType(self):
    return self.attrs['type']

  def GetLanguage(self):
    '''Returns the language ID, default 'en'.'''
    return self.attrs['lang']

  def GetContext(self):
    return self.attrs['context']

  def GetFilename(self):
    return self.attrs['filename']

  def GetOutputFilename(self):
    path = None
    if hasattr(self, 'output_filename'):
      path = self.output_filename
    else:
      path = self.attrs['filename']
    return os.path.expandvars(path)

  def GetFallbackToDefaultLayout(self):
    return self.attrs['fallback_to_default_layout'].lower() == 'true'

  def GetGender(self):
    # Only 'data_package' and 'android' output node types will have gender set.
    if hasattr(self, 'gender'):
      assert self.GetType() == 'data_package' or self.GetType() == 'android'
      return self.gender

    assert self.GetType() != 'data_package' and self.GetType() != 'android'
    return None

  def _IsValidChild(self, child):
    return isinstance(child, EmitNode)

class EmitNode(base.ContentNode):
  ''' An <emit> element.'''

  def DefaultAttributes(self):
    return { 'emit_type' : 'prepend'}

  def GetEmitType(self):
    '''Returns the emit_type for this node. Default is 'append'.'''
    return self.attrs['emit_type']
