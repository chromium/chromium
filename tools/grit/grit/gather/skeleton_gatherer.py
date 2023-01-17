# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''A baseclass for simple gatherers that store their gathered resource in a
list.
'''

from grit.gather import interface
from grit import clique
from grit import exception
from grit import tclib


class SkeletonGatherer(interface.GathererBase):
  '''Common functionality of gatherers that parse their input as a skeleton of
  translatable and nontranslatable chunks.
  '''

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    # List of parts of the document. Translateable parts are
    # clique.MessageClique objects, nontranslateable parts are plain strings.
    # Translated messages are inserted back into the skeleton using the quoting
    # rules defined by self.Escape()
    self.skeleton_ = []
    # A list of the names of IDs that need to be defined for this resource
    # section to compile correctly.
    self.ids_ = []
    # True if Parse() has already been called.
    self.have_parsed_ = False
    # True if a translatable chunk has been added
    self.translatable_chunk_ = False
    # If not None, all parts of the document will be put into this single
    # message; otherwise the normal skeleton approach is used.
    self.single_message_ = None
    # Number to use for the next placeholder name.  Used only if single_message
    # is not None
    self.ph_counter_ = 1

  def GetText(self):
    '''Returns the original text of the section'''
    return self.text_

  def Escape(self, text):
    '''Subclasses can override.  Base impl is identity.
    '''
    return text

  def UnEscape(self, text):
    '''Subclasses can override. Base impl is identity.
    '''
    return text

  def GetTextualIds(self):
    '''Returns the list of textual IDs that need to be defined for this
    resource section to compile correctly.'''
    return self.ids_

  def _AddTextualId(self, id):
    self.ids_.append(id)

  def GetCliques(self):
    '''Returns the message cliques for each translateable message in the
    resource section.'''
    return [x for x in self.skeleton_ if isinstance(x, clique.MessageClique)]

  def Translate(self, lang, pseudo_if_not_available=True,
                skeleton_gatherer=None, fallback_to_english=False):
    if len(self.skeleton_) == 0:
      raise exception.NotReady()
    if skeleton_gatherer:
      assert len(skeleton_gatherer.skeleton_) == len(self.skeleton_)

    out = []
    for ix in range(len(self.skeleton_)):
      if isinstance(self.skeleton_[ix], str):
        if skeleton_gatherer:
          # Make sure the skeleton is like the original
          assert (isinstance(skeleton_gatherer.skeleton_[ix], str))
          out.append(skeleton_gatherer.skeleton_[ix])
        else:
          out.append(self.skeleton_[ix])
      else:
        if skeleton_gatherer:  # Make sure the skeleton is like the original
          assert (not isinstance(skeleton_gatherer.skeleton_[ix], str))
        msg = self.skeleton_[ix].MessageForLanguage(lang,
                                                    pseudo_if_not_available,
                                                    fallback_to_english)

        def MyEscape(text):
          return self.Escape(text)
        text = msg.GetRealContent(escaping_function=MyEscape)
        out.append(text)
    return ''.join(out)

  def Parse(self):
    '''Parses the section.  Implemented by subclasses.  Idempotent.'''
    raise NotImplementedError()

  def _AddNontranslateableChunk(self, chunk):
    '''Adds a nontranslateable chunk.'''
    if self.single_message_:
      ph = tclib.Placeholder('XX%02dXX' % self.ph_counter_, chunk, chunk)
      self.ph_counter_ += 1
      self.single_message_.AppendPlaceholder(ph)
    else:
      self.skeleton_.append(chunk)

  def _AddTranslateableChunk(self, chunk):
    '''Adds a translateable chunk.  It will be unescaped before being added.'''
    # We don't want empty messages since they are redundant and the TC
    # doesn't allow them.
    if chunk == '':
      return

    unescaped_text = self.UnEscape(chunk)
    if self.single_message_:
      self.single_message_.AppendText(unescaped_text)
    else:
      self.skeleton_.append(self.uberclique.MakeClique(
        tclib.Message(text=unescaped_text)))
      self.translatable_chunk_ = True

  def SubstituteMessages(self, substituter):
    '''Applies substitutions to all messages in the tree.

    Goes through the skeleton and finds all MessageCliques.

    Args:
      substituter: a grit.util.Substituter object.
    '''
    if self.single_message_:
      self.single_message_ = substituter.SubstituteMessage(self.single_message_)
    new_skel = []
    for chunk in self.skeleton_:
      if isinstance(chunk, clique.MessageClique):
        old_message = chunk.GetMessage()
        new_message = substituter.SubstituteMessage(old_message)
        if new_message is not old_message:
          new_skel.append(self.uberclique.MakeClique(new_message))
          continue
      new_skel.append(chunk)
    self.skeleton_ = new_skel
