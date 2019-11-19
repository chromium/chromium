# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''The 'grit transl2tc' tool.
'''

from __future__ import print_function

from grit import grd_reader
from grit import util
from grit.tool import interface
from grit.tool import rc2grd

from grit.extern import tclib


class TranslationToTc(interface.Tool):
  '''A tool for importing existing translations in RC format into the
Translation Console.

Usage:

grit -i GRD transl2tc [-l LIMITS] [RCOPTS] SOURCE_RC TRANSLATED_RC OUT_FILE

The tool needs a "source" RC file, i.e. in English, and an RC file that is a
translation of precisely the source RC file (not of an older or newer version).

The tool also requires you to provide a .grd file (input file) e.g. using the
-i global option or the GRIT_INPUT environment variable.  The tool uses
information from your .grd file to correct placeholder names in the
translations and ensure that only translatable items and translations still
being used are output.

This tool will accept all the same RCOPTS as the 'grit rc2grd' tool.  To get
a list of these options, run 'grit help rc2grd'.

Additionally, you can use the -l option (which must be the first option to the
tool) to specify a file containing a list of message IDs to which output should
be limited.  This is only useful if you are limiting the output to your XMB
files using the 'grit xmb' tool's -l option.  See 'grit help xmb' for how to
generate a file containing a list of the message IDs in an XMB file.

The tool will scan through both of the RC files as well as any HTML files they
refer to, and match together the source messages and translated messages.  It
will output a file (OUTPUT_FILE) you can import directly into the TC using the
Bulk Translation Upload tool.
'''

  def ShortDescription(self):
    return 'Import existing translations in RC format into the TC'

  def Setup(self, globopt, args):
    '''Sets the instance up for use.
    '''
    self.SetOptions(globopt)
    self.rc2grd = rc2grd.Rc2Grd()
    self.rc2grd.SetOptions(globopt)
    self.limits = None
    if len(args) and args[0] == '-l':
      self.limits = util.ReadFile(args[1], util.RAW_TEXT).split('\n')
      args = args[2:]
    return self.rc2grd.ParseOptions(args, help_func=self.ShowUsage)

  def Run(self, globopt, args):
    args = self.Setup(globopt, args)

    if len(args) != 3:
      self.Out('This tool takes exactly three arguments:\n'
             '  1. The path to the original RC file\n'
             '  2. The path to the translated RC file\n'
             '  3. The output file path.\n')
      return 2

    grd = grd_reader.Parse(self.o.input, debug=self.o.extra_verbose)
    grd.RunGatherers()

    source_rc = util.ReadFile(args[0], self.rc2grd.input_encoding)
    transl_rc = util.ReadFile(args[1], self.rc2grd.input_encoding)
    translations = self.ExtractTranslations(grd,
                                            source_rc, args[0],
                                            transl_rc, args[1])

    with util.WrapOutputStream(open(args[2], 'w')) as output_file:
      self.WriteTranslations(output_file, translations.items())

    self.Out('Wrote output file %s' % args[2])

  def ExtractTranslations(self, current_grd, source_rc, source_path,
                                             transl_rc, transl_path):
    '''Extracts translations from the translated RC file, matching them with
    translations in the source RC file to calculate their ID, and correcting
    placeholders, limiting output to translateables, etc. using the supplied
    .grd file which is the current .grd file for your project.

    If this object's 'limits' attribute is not None but a list, the output of
    this function will be further limited to include only messages that have
    message IDs in the 'limits' list.

    Args:
      current_grd: grit.node.base.Node child, that has had RunGatherers() run
                   on it
      source_rc: Complete text of source RC file
      source_path: Path to the source RC file
      transl_rc: Complete text of translated RC file
      transl_path: Path to the translated RC file

    Return:
      { id1 : text1, '12345678' : 'Hello USERNAME, howzit?' }
    '''
    source_grd = self.rc2grd.Process(source_rc, source_path)
    self.VerboseOut('Read %s into GRIT format, running gatherers.\n' % source_path)
    source_grd.SetOutputLanguage(current_grd.output_language)
    source_grd.SetDefines(current_grd.defines)
    source_grd.RunGatherers(debug=self.o.extra_verbose)
    transl_grd = self.rc2grd.Process(transl_rc, transl_path)
    transl_grd.SetOutputLanguage(current_grd.output_language)
    transl_grd.SetDefines(current_grd.defines)
    self.VerboseOut('Read %s into GRIT format, running gatherers.\n' % transl_path)
    transl_grd.RunGatherers(debug=self.o.extra_verbose)
    self.VerboseOut('Done running gatherers for %s.\n' % transl_path)

    # Proceed to create a map from ID to translation, getting the ID from the
    # source GRD and the translation from the translated GRD.
    id2transl = {}
    for source_node in source_grd:
      source_cliques = source_node.GetCliques()
      if not len(source_cliques):
        continue

      assert 'name' in source_node.attrs, 'All nodes with cliques should have an ID'
      node_id = source_node.attrs['name']
      self.ExtraVerboseOut('Processing node %s\n' % node_id)
      transl_node = transl_grd.GetNodeById(node_id)

      if transl_node:
        transl_cliques = transl_node.GetCliques()
        if not len(transl_cliques) == len(source_cliques):
          self.Out(
            'Warning: Translation for %s has wrong # of cliques, skipping.\n' %
            node_id)
          continue
      else:
        self.Out('Warning: No translation for %s, skipping.\n' % node_id)
        continue

      if source_node.name == 'message':
        # Fixup placeholders as well as possible based on information from
        # the current .grd file if they are 'TODO_XXXX' placeholders.  We need
        # to fixup placeholders in the translated message so that it looks right
        # and we also need to fixup placeholders in the source message so that
        # its calculated ID will match the current message.
        current_node = current_grd.GetNodeById(node_id)
        if current_node:
          assert len(source_cliques) == len(current_node.GetCliques()) == 1

          source_msg = source_cliques[0].GetMessage()
          current_msg = current_node.GetCliques()[0].GetMessage()

          # Only do this for messages whose source version has not changed.
          if (source_msg.GetRealContent() != current_msg.GetRealContent()):
            self.VerboseOut('Info: Message %s has changed; skipping\n' % node_id)
          else:
            transl_msg = transl_cliques[0].GetMessage()
            transl_content = transl_msg.GetContent()
            current_content = current_msg.GetContent()
            source_content = source_msg.GetContent()

            ok_to_fixup = True
            if (len(transl_content) != len(current_content)):
              # message structure of translation is different, don't try fixup
              ok_to_fixup = False
            if ok_to_fixup:
              for ix in range(len(transl_content)):
                if isinstance(transl_content[ix], tclib.Placeholder):
                  if not isinstance(current_content[ix], tclib.Placeholder):
                    ok_to_fixup = False  # structure has changed
                    break
                  if (transl_content[ix].GetOriginal() !=
                      current_content[ix].GetOriginal()):
                    ok_to_fixup = False  # placeholders have likely been reordered
                    break
                else:  # translated part is not a placeholder but a string
                  if isinstance(current_content[ix], tclib.Placeholder):
                    ok_to_fixup = False  # placeholders have likely been reordered
                    break

            if not ok_to_fixup:
              self.VerboseOut(
                'Info: Structure of message %s has changed; skipping.\n' % node_id)
            else:
              def Fixup(content, ix):
                if (isinstance(content[ix], tclib.Placeholder) and
                    content[ix].GetPresentation().startswith('TODO_')):
                  assert isinstance(current_content[ix], tclib.Placeholder)
                  # Get the placeholder ID and example from the current message
                  content[ix] = current_content[ix]
              for ix in range(len(transl_content)):
                Fixup(transl_content, ix)
                Fixup(source_content, ix)

      # Only put each translation once into the map.  Warn if translations
      # for the same message are different.
      for ix in range(len(transl_cliques)):
        source_msg = source_cliques[ix].GetMessage()
        source_msg.GenerateId()  # needed to refresh ID based on new placeholders
        message_id = source_msg.GetId()
        translated_content = transl_cliques[ix].GetMessage().GetPresentableContent()

        if message_id in id2transl:
          existing_translation = id2transl[message_id]
          if existing_translation != translated_content:
            original_text = source_cliques[ix].GetMessage().GetPresentableContent()
            self.Out('Warning: Two different translations for "%s":\n'
                   '  Translation 1: "%s"\n'
                   '  Translation 2: "%s"\n' %
                   (original_text, existing_translation, translated_content))
        else:
          id2transl[message_id] = translated_content

    # Remove translations for messages that do not occur in the current .grd
    # or have been marked as not translateable, or do not occur in the 'limits'
    # list (if it has been set).
    current_message_ids = current_grd.UberClique().AllMessageIds()
    for message_id in list(id2transl.keys()):
      if (message_id not in current_message_ids or
          not current_grd.UberClique().BestClique(message_id).IsTranslateable() or
          (self.limits and message_id not in self.limits)):
        del id2transl[message_id]

    return id2transl

  @staticmethod
  def WriteTranslations(output_file, translations):
    '''Writes the provided list of translations to the provided output file
    in the format used by the TC's Bulk Translation Upload tool.  The file
    must be UTF-8 encoded.

    Args:
      output_file: util.WrapOutputStream(open('bingo.out', 'w'))
      translations: [ [id1, text1], ['12345678', 'Hello USERNAME, howzit?'] ]

    Return:
      None
    '''
    for id, text in translations:
      text = text.replace('<', '&lt;').replace('>', '&gt;')
      output_file.write(id)
      output_file.write(' ')
      output_file.write(text)
      output_file.write('\n')
