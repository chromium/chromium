# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''A baseclass for simple gatherers based on regular expressions.
'''

from __future__ import print_function

from grit.gather import skeleton_gatherer


class RegexpGatherer(skeleton_gatherer.SkeletonGatherer):
  '''Common functionality of gatherers based on parsing using a single
  regular expression.
  '''

  DescriptionMapping_ = {
      'CAPTION' : 'This is a caption for a dialog',
      'CHECKBOX' : 'This is a label for a checkbox',
      'CONTROL': 'This is the text on a control',
      'CTEXT': 'This is a label for a control',
      'DEFPUSHBUTTON': 'This is a button definition',
      'GROUPBOX': 'This is a label for a grouping',
      'ICON': 'This is a label for an icon',
      'LTEXT': 'This is the text for a label',
      'PUSHBUTTON': 'This is the text for a button',
    }

  # Contextualization elements. Used for adding additional information
  # to the message bundle description string from RC files.
  def AddDescriptionElement(self, string):
    if string in self.DescriptionMapping_:
      description = self.DescriptionMapping_[string]
    else:
      description = string
    if self.single_message_:
      self.single_message_.SetDescription(description)
    else:
      if (self.translatable_chunk_):
        message = self.skeleton_[len(self.skeleton_) - 1].GetMessage()
        message.SetDescription(description)

  def _RegExpParse(self, regexp, text_to_parse):
    '''An implementation of Parse() that can be used for resource sections that
    can be parsed using a single multi-line regular expression.

    All translateables must be in named groups that have names starting with
    'text'.  All textual IDs must be in named groups that have names starting
    with 'id'. All type definitions that can be included in the description
    field for contextualization purposes should have a name that starts with
    'type'.

    Args:
      regexp: re.compile('...', re.MULTILINE)
      text_to_parse:
    '''
    chunk_start = 0
    for match in regexp.finditer(text_to_parse):
      groups = match.groupdict()
      keys = sorted(groups.keys())
      self.translatable_chunk_ = False
      for group in keys:
        if group.startswith('id') and groups[group]:
          self._AddTextualId(groups[group])
        elif group.startswith('text') and groups[group]:
          self._AddNontranslateableChunk(
            text_to_parse[chunk_start : match.start(group)])
          chunk_start = match.end(group)  # Next chunk will start after the match
          self._AddTranslateableChunk(groups[group])
        elif group.startswith('type') and groups[group]:
          # Add the description to the skeleton_ list. This works because
          # we are using a sort set of keys, and because we assume that the
          # group name used for descriptions (type) will come after the "text"
          # group in alphabetical order. We also assume that there cannot be
          # more than one description per regular expression match.
          self.AddDescriptionElement(groups[group])

    self._AddNontranslateableChunk(text_to_parse[chunk_start:])

    if self.single_message_:
      self.skeleton_.append(self.uberclique.MakeClique(self.single_message_))
