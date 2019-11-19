# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''The <structure> element.
'''

from __future__ import print_function

import os
import platform
import re

from grit import exception
from grit import util
from grit.node import base
from grit.node import variant

import grit.gather.admin_template
import grit.gather.chrome_html
import grit.gather.chrome_scaled_image
import grit.gather.policy_json
import grit.gather.rc
import grit.gather.tr_html
import grit.gather.txt

import grit.format.rc

# Type of the gatherer to use for each type attribute
_GATHERERS = {
  'accelerators'        : grit.gather.rc.Accelerators,
  'admin_template'      : grit.gather.admin_template.AdmGatherer,
  'chrome_html'         : grit.gather.chrome_html.ChromeHtml,
  'chrome_scaled_image' : grit.gather.chrome_scaled_image.ChromeScaledImage,
  'dialog'              : grit.gather.rc.Dialog,
  'menu'                : grit.gather.rc.Menu,
  'rcdata'              : grit.gather.rc.RCData,
  'tr_html'             : grit.gather.tr_html.TrHtml,
  'txt'                 : grit.gather.txt.TxtFile,
  'version'             : grit.gather.rc.Version,
  'policy_template_metafile' : grit.gather.policy_json.PolicyJson,
}


# TODO(joi) Print a warning if the 'variant_of_revision' attribute indicates
# that a skeleton variant is older than the original file.


class StructureNode(base.Node):
  '''A <structure> element.'''

  # Regular expression for a local variable definition.  Each definition
  # is of the form NAME=VALUE, where NAME cannot contain '=' or ',' and
  # VALUE must escape all commas: ',' -> ',,'.  Each variable definition
  # should be separated by a comma with no extra whitespace.
  # Example: THING1=foo,THING2=bar
  variable_pattern = re.compile(r'([^,=\s]+)=((?:,,|[^,])*)')

  def __init__(self):
    super(StructureNode, self).__init__()

    # Keep track of the last filename we flattened to, so we can
    # avoid doing it more than once.
    self._last_flat_filename = None

    # See _Substitute; this substituter is used for local variables and
    # the root substituter is used for global variables.
    self.substituter = None

  def _IsValidChild(self, child):
    return isinstance(child, variant.SkeletonNode)

  def _ParseVariables(self, variables):
    '''Parse a variable string into a dictionary.'''
    matches = StructureNode.variable_pattern.findall(variables)
    return dict((name, value.replace(',,', ',')) for name, value in matches)

  def EndParsing(self):
    super(StructureNode, self).EndParsing()

    # Now that we have attributes and children, instantiate the gatherers.
    gathertype = _GATHERERS[self.attrs['type']]

    self.gatherer = gathertype(self.attrs['file'],
                               self.attrs['name'],
                               self.attrs['encoding'])
    self.gatherer.SetGrdNode(self)
    self.gatherer.SetUberClique(self.UberClique())
    if hasattr(self.GetRoot(), 'defines'):
      self.gatherer.SetDefines(self.GetRoot().defines)
    self.gatherer.SetAttributes(self.attrs)
    if self.ExpandVariables():
      self.gatherer.SetFilenameExpansionFunction(self._Substitute)

    # Parse local variables and instantiate the substituter.
    if self.attrs['variables']:
      variables = self.attrs['variables']
      self.substituter = util.Substituter()
      self.substituter.AddSubstitutions(self._ParseVariables(variables))

    self.skeletons = {}  # Maps expressions to skeleton gatherers
    for child in self.children:
      assert isinstance(child, variant.SkeletonNode)
      skel = gathertype(child.attrs['file'],
                        self.attrs['name'],
                        child.GetEncodingToUse(),
                        is_skeleton=True)
      skel.SetGrdNode(self)  # TODO(benrg): Or child? Only used for ToRealPath
      skel.SetUberClique(self.UberClique())
      if hasattr(self.GetRoot(), 'defines'):
        skel.SetDefines(self.GetRoot().defines)
      if self.ExpandVariables():
        skel.SetFilenameExpansionFunction(self._Substitute)
      self.skeletons[child.attrs['expr']] = skel

  def MandatoryAttributes(self):
    return ['type', 'name', 'file']

  def DefaultAttributes(self):
    return { 'encoding' : 'cp1252',
             'exclude_from_rc' : 'false',
             'line_end' : 'unix',
             'output_encoding' : 'utf-8',
             'generateid': 'true',
             'expand_variables' : 'false',
             'output_filename' : '',
             'fold_whitespace': 'false',
             # Run an arbitrary command after translation is complete
             # so that it doesn't interfere with what's in translation
             # console.
             'run_command' : '',
             # Leave empty to run on all platforms, comma-separated
             # for one or more specific platforms. Values must match
             # output of platform.system().
             'run_command_on_platforms' : '',
             'allowexternalscript': 'false',
             # preprocess takes the same code path as flattenhtml, but it
             # disables any processing/inlining outside of <if> and <include>.
             'preprocess': 'false',
             'flattenhtml': 'false',
             'fallback_to_low_resolution': 'default',
             'variables': '',
             'compress': 'false',
             'use_base_dir': 'true',
             }

  def IsExcludedFromRc(self):
    return self.attrs['exclude_from_rc'] == 'true'

  def Process(self, output_dir):
    """Writes the processed data to output_dir.  In the case of a chrome_html
    structure this will add references to other scale factors.  If flattening
    this will also write file references to be base64 encoded data URLs.  The
    name of the new file is returned."""
    filename = self.ToRealPath(self.GetInputPath())
    flat_filename = os.path.join(output_dir,
        self.attrs['name'] + '_' + os.path.basename(filename))

    if self._last_flat_filename == flat_filename:
      return

    with open(flat_filename, 'wb') as outfile:
      if self.ExpandVariables():
        text = self.gatherer.GetText()
        file_contents = self._Substitute(text)
      else:
        file_contents = self.gatherer.GetData('', 'utf-8')
      outfile.write(file_contents.encode('utf-8'))

    self._last_flat_filename = flat_filename
    return os.path.basename(flat_filename)

  def GetLineEnd(self):
    '''Returns the end-of-line character or characters for files output because
    of this node ('\r\n', '\n', or '\r' depending on the 'line_end' attribute).
    '''
    if self.attrs['line_end'] == 'unix':
      return '\n'
    elif self.attrs['line_end'] == 'windows':
      return '\r\n'
    elif self.attrs['line_end'] == 'mac':
      return '\r'
    else:
      raise exception.UnexpectedAttribute(
        "Attribute 'line_end' must be one of 'unix' (default), 'windows' or "
        "'mac'")

  def GetCliques(self):
    return self.gatherer.GetCliques()

  def GetDataPackValue(self, lang, encoding):
    """Returns a str represenation for a data_pack entry."""
    if self.ExpandVariables():
      text = self.gatherer.GetText()
      data = util.Encode(self._Substitute(text), encoding)
    else:
      data = self.gatherer.GetData(lang, encoding)
    return self.CompressDataIfNeeded(data)

  def GetHtmlResourceFilenames(self):
    """Returns a set of all filenames inlined by this node."""
    return self.gatherer.GetHtmlResourceFilenames()

  def GetInputPath(self):
    path = self.gatherer.GetInputPath()
    if path is None:
      return path

    # Do not mess with absolute paths, that would make them invalid.
    if os.path.isabs(os.path.expandvars(path)):
      return path

    # We have no control over code that calls ToRealPath later, so convert
    # the path to be relative against our basedir.
    if self.attrs.get('use_base_dir', 'true') != 'true':
      # Normalize the directory path to use the appropriate OS separator.
      # GetBaseDir() may return paths\like\this or paths/like/this, since it is
      # read from the base_dir attribute in the grd file.
      norm_base_dir = util.normpath(self.GetRoot().GetBaseDir())
      return os.path.relpath(path, norm_base_dir)

    return path

  def GetTextualIds(self):
    if not hasattr(self, 'gatherer'):
      # This case is needed because this method is called by
      # GritNode.ValidateUniqueIds before RunGatherers has been called.
      # TODO(benrg): Fix this?
      return [self.attrs['name']]
    return self.gatherer.GetTextualIds()

  def RunPreSubstitutionGatherer(self, debug=False):
    if debug:
      print('Running gatherer %s for file %s' %
            (type(self.gatherer), self.GetInputPath()))

    # Note: Parse() is idempotent, therefore this method is also.
    self.gatherer.Parse()
    for skel in self.skeletons.values():
      skel.Parse()

  def GetSkeletonGatherer(self):
    '''Returns the gatherer for the alternate skeleton that should be used,
    based on the expressions for selecting skeletons, or None if the skeleton
    from the English version of the structure should be used.
    '''
    for expr in self.skeletons:
      if self.EvaluateCondition(expr):
        return self.skeletons[expr]
    return None

  def HasFileForLanguage(self):
    return self.attrs['type'] in ['tr_html', 'admin_template', 'txt',
                                  'chrome_scaled_image',
                                  'chrome_html']

  def ExpandVariables(self):
    '''Variable expansion on structures is controlled by an XML attribute.

    However, old files assume that expansion is always on for Rc files.

    Returns:
      A boolean.
    '''
    attrs = self.GetRoot().attrs
    if 'grit_version' in attrs and attrs['grit_version'] > 1:
      return self.attrs['expand_variables'] == 'true'
    else:
      return (self.attrs['expand_variables'] == 'true' or
              self.attrs['file'].lower().endswith('.rc'))

  def _Substitute(self, text):
    '''Perform local and global variable substitution.'''
    if self.substituter:
      text = self.substituter.Substitute(text)
    return self.GetRoot().GetSubstituter().Substitute(text)

  def RunCommandOnCurrentPlatform(self):
    if self.attrs['run_command_on_platforms'] == '':
      return True
    else:
      target_platforms = self.attrs['run_command_on_platforms'].split(',')
      return platform.system() in target_platforms

  def FileForLanguage(self, lang, output_dir, create_file=True,
                      return_if_not_generated=True):
    '''Returns the filename of the file associated with this structure,
    for the specified language.

    Args:
      lang: 'fr'
      output_dir: 'c:\temp'
      create_file: True
    '''
    assert self.HasFileForLanguage()
    # If the source language is requested, and no extra changes are requested,
    # use the existing file.
    if ((not lang or lang == self.GetRoot().GetSourceLanguage()) and
        self.attrs['expand_variables'] != 'true' and
        (not self.attrs['run_command'] or
         not self.RunCommandOnCurrentPlatform())):
      if return_if_not_generated:
        input_path = self.GetInputPath()
        if input_path is None:
          return None
        return self.ToRealPath(input_path)
      else:
        return None

    if self.attrs['output_filename'] != '':
      filename = self.attrs['output_filename']
    else:
      filename = os.path.basename(self.attrs['file'])
    assert len(filename)
    filename = '%s_%s' % (lang, filename)
    filename = os.path.join(output_dir, filename)

    # Only create the output if it was requested by the call.
    if create_file:
      text = self.gatherer.Translate(
          lang,
          pseudo_if_not_available=self.PseudoIsAllowed(),
          fallback_to_english=self.ShouldFallbackToEnglish(),
          skeleton_gatherer=self.GetSkeletonGatherer())

      file_contents = util.FixLineEnd(text, self.GetLineEnd())
      if self.ExpandVariables():
        # Note that we reapply substitution a second time here.
        # This is because a) we need to look inside placeholders
        # b) the substitution values are language-dependent
        file_contents = self._Substitute(file_contents)

      with open(filename, 'wb') as file_object:
        output_stream = util.WrapOutputStream(file_object,
                                              self.attrs['output_encoding'])
        output_stream.write(file_contents)

      if self.attrs['run_command'] and self.RunCommandOnCurrentPlatform():
        # Run arbitrary commands after translation is complete so that it
        # doesn't interfere with what's in translation console.
        command = self.attrs['run_command'] % {'filename': filename}
        result = os.system(command)
        assert result == 0, '"%s" failed.' % command

    return filename

  def IsResourceMapSource(self):
    return True

  @staticmethod
  def Construct(parent, name, type, file, encoding='cp1252'):
    '''Creates a new node which is a child of 'parent', with attributes set
    by parameters of the same name.
    '''
    node = StructureNode()
    node.StartParsing('structure', parent)
    node.HandleAttribute('name', name)
    node.HandleAttribute('type', type)
    node.HandleAttribute('file', file)
    node.HandleAttribute('encoding', encoding)
    node.EndParsing()
    return node

  def SubstituteMessages(self, substituter):
    '''Propagates substitution to gatherer.

    Args:
      substituter: a grit.util.Substituter object.
    '''
    assert hasattr(self, 'gatherer')
    if self.ExpandVariables():
      self.gatherer.SubstituteMessages(substituter)
