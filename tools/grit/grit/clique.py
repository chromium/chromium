# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Collections of messages and their translations, called cliques.  Also
collections of cliques (uber-cliques).
'''

from __future__ import print_function

import re

import six

from grit import constants
from grit import exception
from grit import lazy_re
from grit import pseudo
from grit import pseudo_rtl
from grit import tclib


class UberClique(object):
  '''A factory (NOT a singleton factory) for making cliques.  It has several
  methods for working with the cliques created using the factory.
  '''

  def __init__(self):
    # A map from message ID to list of cliques whose source messages have
    # that ID.  This will contain all cliques created using this factory.
    # Different messages can have the same ID because they have the
    # same translateable portion and placeholder names, but occur in different
    # places in the resource tree.
    #
    # Each list of cliques is kept sorted by description, to achieve
    # stable results from the BestClique method, see below.
    self.cliques_ = {}

    # A map of clique IDs to list of languages to indicate translations where we
    # fell back to English.
    self.fallback_translations_ = {}

    # A map of clique IDs to list of languages to indicate missing translations.
    self.missing_translations_ = {}

  def _AddMissingTranslation(self, lang, clique, is_error):
    tl = self.fallback_translations_
    if is_error:
      tl = self.missing_translations_
    id = clique.GetId()
    if id not in tl:
      tl[id] = {}
    if lang not in tl[id]:
      tl[id][lang] = 1

  def HasMissingTranslations(self):
    return len(self.missing_translations_) > 0

  def MissingTranslationsReport(self):
    '''Returns a string suitable for printing to report missing
    and fallback translations to the user.
    '''
    def ReportTranslation(clique, langs):
      text = clique.GetMessage().GetPresentableContent()
      # The text 'error' (usually 'Error:' but we are conservative)
      # can trigger some build environments (Visual Studio, we're
      # looking at you) to consider invocation of grit to have failed,
      # so we make sure never to output that word.
      extract = re.sub(r'(?i)error', 'REDACTED', text[0:40])[0:40]
      ellipsis = ''
      if len(text) > 40:
        ellipsis = '...'
      langs_extract = langs[0:6]
      describe_langs = ','.join(langs_extract)
      if len(langs) > 6:
        describe_langs += " and %d more" % (len(langs) - 6)
      return "  %s \"%s%s\" %s" % (clique.GetId(), extract, ellipsis,
                                   describe_langs)
    lines = []
    if len(self.fallback_translations_):
      lines.append(
        "WARNING: Fell back to English for the following translations:")
      for (id, langs) in self.fallback_translations_.items():
        lines.append(
            ReportTranslation(self.cliques_[id][0], list(langs.keys())))
    if len(self.missing_translations_):
      lines.append("ERROR: The following translations are MISSING:")
      for (id, langs) in self.missing_translations_.items():
        lines.append(
            ReportTranslation(self.cliques_[id][0], list(langs.keys())))
    return '\n'.join(lines)

  def MakeClique(self, message, translateable=True):
    '''Create a new clique initialized  with a message.

    Args:
      message: tclib.Message()
      translateable: True | False
    '''
    clique = MessageClique(self, message, translateable)

    # Enable others to find this clique by its message ID
    if message.GetId() in self.cliques_:
      presentable_text = clique.GetMessage().GetPresentableContent()
      if not message.HasAssignedId():
        for c in self.cliques_[message.GetId()]:
          assert c.GetMessage().GetPresentableContent() == presentable_text
      self.cliques_[message.GetId()].append(clique)
      # We need to keep each list of cliques sorted by description, to
      # achieve stable results from the BestClique method, see below.
      self.cliques_[message.GetId()].sort(
          key=lambda c:c.GetMessage().GetDescription())
    else:
      self.cliques_[message.GetId()] = [clique]

    return clique

  def FindCliqueAndAddTranslation(self, translation, language):
    '''Adds the specified translation to the clique with the source message
    it is a translation of.

    Args:
      translation: tclib.Translation()
      language: 'en' | 'fr' ...

    Return:
      True if the source message was found, otherwise false.
    '''
    if translation.GetId() in self.cliques_:
      for clique in self.cliques_[translation.GetId()]:
        clique.AddTranslation(translation, language)
      return True
    else:
      return False

  def BestClique(self, id):
    '''Returns the "best" clique from a list of cliques.  All the cliques
    must have the same ID.  The "best" clique is chosen in the following
    order of preference:
    - The first clique that has a non-ID-based description.
    - If no such clique found, the first clique with an ID-based description.
    - Otherwise the first clique.

    This method is stable in terms of always returning a clique with
    an identical description (on different runs of GRIT on the same
    data) because self.cliques_ is sorted by description.
    '''
    clique_list = self.cliques_[id]
    clique_with_id = None
    clique_default = None
    for clique in clique_list:
      if not clique_default:
        clique_default = clique

      description = clique.GetMessage().GetDescription()
      if description and len(description) > 0:
        if not description.startswith('ID:'):
          # this is the preferred case so we exit right away
          return clique
        elif not clique_with_id:
          clique_with_id = clique
    if clique_with_id:
      return clique_with_id
    else:
      return clique_default

  def BestCliquePerId(self):
    '''Iterates over the list of all cliques and returns the best clique for
    each ID.  This will be the first clique with a source message that has a
    non-empty description, or an arbitrary clique if none of them has a
    description.
    '''
    for id in self.cliques_:
      yield self.BestClique(id)

  def BestCliqueByOriginalText(self, text, meaning):
    '''Finds the "best" (as in BestClique()) clique that has original text
    'text' and meaning 'meaning'.  Returns None if there is no such clique.
    '''
    # If needed, this can be optimized by maintaining a map of
    # fingerprints of original text+meaning to cliques.
    for c in self.BestCliquePerId():
      msg = c.GetMessage()
      if msg.GetRealContent() == text and msg.GetMeaning() == meaning:
        return msg
    return None

  def AllMessageIds(self):
    '''Returns a list of all defined message IDs.
    '''
    return list(self.cliques_.keys())

  def AllCliques(self):
    '''Iterates over all cliques.  Note that this can return multiple cliques
    with the same ID.
    '''
    for cliques in self.cliques_.values():
      for c in cliques:
        yield c

  def GenerateXtbParserCallback(self, lang, debug=False):
    '''Creates a callback function as required by grit.xtb_reader.Parse().
    This callback will create Translation objects for each message from
    the XTB that exists in this uberclique, and add them as translations for
    the relevant cliques.  The callback will add translations to the language
    specified by 'lang'

    Args:
      lang: 'fr'
      debug: True | False
    '''
    def Callback(id, structure):
      if id not in self.cliques_:
        if debug:
          print("Ignoring translation #%s" % id)
        return

      if debug:
        print("Adding translation #%s" % id)

      # We fetch placeholder information from the original message (the XTB file
      # only contains placeholder names).
      original_msg = self.BestClique(id).GetMessage()

      translation = tclib.Translation(id=id)
      for is_ph,text in structure:
        if not is_ph:
          translation.AppendText(text)
        else:
          found_placeholder = False
          for ph in original_msg.GetPlaceholders():
            if ph.GetPresentation() == text:
              translation.AppendPlaceholder(tclib.Placeholder(
                ph.GetPresentation(), ph.GetOriginal(), ph.GetExample()))
              found_placeholder = True
              break
          if not found_placeholder:
            raise exception.MismatchingPlaceholders(
              'Translation for message ID %s had <ph name="%s"/>, no match\n'
              'in original message' % (id, text))
      self.FindCliqueAndAddTranslation(translation, lang)
    return Callback


class CustomType(object):
  '''A base class you should implement if you wish to specify a custom type
  for a message clique (i.e. custom validation and optional modification of
  translations).'''

  def Validate(self, message):
    '''Returns true if the message (a tclib.Message object) is valid,
    otherwise false.
    '''
    raise NotImplementedError()

  def ValidateAndModify(self, lang, translation):
    '''Returns true if the translation (a tclib.Translation object) is valid,
    otherwise false.  The language is also passed in.  This method may modify
    the translation that is passed in, if it so wishes.
    '''
    raise NotImplementedError()

  def ModifyTextPart(self, lang, text):
    '''If you call ModifyEachTextPart, it will turn around and call this method
    for each text part of the translation.  You should return the modified
    version of the text, or just the original text to not change anything.
    '''
    raise NotImplementedError()

  def ModifyEachTextPart(self, lang, translation):
    '''Call this to easily modify one or more of the textual parts of a
    translation.  It will call ModifyTextPart for each part of the
    translation.
    '''
    contents = translation.GetContent()
    for ix in range(len(contents)):
      if (isinstance(contents[ix], six.string_types)):
        contents[ix] = self.ModifyTextPart(lang, contents[ix])


class OneOffCustomType(CustomType):
  '''A very simple custom type that performs the validation expressed by
  the input expression on all languages including the source language.
  The expression can access the variables 'lang', 'msg' and 'text()' where
  'lang' is the language of 'msg', 'msg' is the message or translation being
  validated and 'text()' returns the real contents of 'msg' (for shorthand).
  '''
  def __init__(self, expression):
    self.expr = expression
  def Validate(self, message):
    return self.ValidateAndModify(MessageClique.source_language, message)
  def ValidateAndModify(self, lang, msg):
    def text():
      return msg.GetRealContent()
    return eval(self.expr, {},
            {'lang' : lang,
             'text' : text,
             'msg' : msg,
             })


class MessageClique(object):
  '''A message along with all of its translations.  Also code to bring
  translations together with their original message.'''

  # change this to the language code of Messages you add to cliques_.
  # TODO(joi) Actually change this based on the <grit> node's source language
  source_language = 'en'

  # A constant translation we use when asked for a translation into the
  # special language constants.CONSTANT_LANGUAGE.
  CONSTANT_TRANSLATION = tclib.Translation(text='TTTTTT')

  # A pattern to match messages that are empty or whitespace only.
  WHITESPACE_MESSAGE = lazy_re.compile(r'^\s*$')

  def __init__(self, uber_clique, message, translateable=True,
               custom_type=None):
    '''Create a new clique initialized with just a message.

    Note that messages with a body comprised only of whitespace will implicitly
    be marked non-translatable.

    Args:
      uber_clique: Our uber-clique (collection of cliques)
      message: tclib.Message()
      translateable: True | False
      custom_type: instance of clique.CustomType interface
    '''
    # Our parent
    self.uber_clique = uber_clique
    # If not translateable, we only store the original message.
    self.translateable = translateable

    # We implicitly mark messages that have a whitespace-only body as
    # non-translateable.
    if MessageClique.WHITESPACE_MESSAGE.match(message.GetRealContent()):
      self.translateable = False

    # A mapping of language identifiers to tclib.BaseMessage and its
    # subclasses (i.e. tclib.Message and tclib.Translation).
    self.clique = { MessageClique.source_language : message }
    # A list of the "shortcut groups" this clique is
    # part of.  Within any given shortcut group, no shortcut key (e.g. &J)
    # must appear more than once in each language for all cliques that
    # belong to the group.
    self.shortcut_groups = []
    # An instance of the CustomType interface, or None.  If this is set, it will
    # be used to validate the original message and translations thereof, and
    # will also get a chance to modify translations of the message.
    self.SetCustomType(custom_type)

  def GetMessage(self):
    '''Retrieves the tclib.Message that is the source for this clique.'''
    return self.clique[MessageClique.source_language]

  def GetId(self):
    '''Retrieves the message ID of the messages in this clique.'''
    return self.GetMessage().GetId()

  def IsTranslateable(self):
    return self.translateable

  def AddToShortcutGroup(self, group):
    self.shortcut_groups.append(group)

  def SetCustomType(self, custom_type):
    '''Makes this clique use custom_type for validating messages and
    translations, and optionally modifying translations.
    '''
    self.custom_type = custom_type
    if custom_type and not custom_type.Validate(self.GetMessage()):
      raise exception.InvalidMessage(self.GetMessage().GetRealContent())

  def MessageForLanguage(self, lang, pseudo_if_no_match=True,
                         fallback_to_english=False):
    '''Returns the message/translation for the specified language, providing
    a pseudotranslation if there is no available translation and a pseudo-
    translation is requested.

    The translation of any message whatsoever in the special language
    'x_constant' is the message "TTTTTT".

    Args:
      lang: 'en'
      pseudo_if_no_match: True
      fallback_to_english: False

    Return:
      tclib.BaseMessage
    '''
    if not self.translateable:
      return self.GetMessage()

    if lang == constants.CONSTANT_LANGUAGE:
      return self.CONSTANT_TRANSLATION

    for msglang in self.clique:
      if lang == msglang:
        return self.clique[msglang]

    if lang == constants.FAKE_BIDI:
      return pseudo_rtl.PseudoRTLMessage(self.GetMessage())

    if fallback_to_english:
      self.uber_clique._AddMissingTranslation(lang, self, is_error=False)
      return self.GetMessage()

    # If we're not supposed to generate pseudotranslations, we add an error
    # report to a list of errors, then fail at a higher level, so that we
    # get a list of all messages that are missing translations.
    if not pseudo_if_no_match:
      self.uber_clique._AddMissingTranslation(lang, self, is_error=True)

    return pseudo.PseudoMessage(self.GetMessage())

  def AllMessagesThatMatch(self, lang_re, include_pseudo = True):
    '''Returns a map of all messages that match 'lang', including the pseudo
    translation if requested.

    Args:
      lang_re: re.compile(r'fr|en')
      include_pseudo: True

    Return:
      { 'en' : tclib.Message,
        'fr' : tclib.Translation,
        pseudo.PSEUDO_LANG : tclib.Translation }
    '''
    if not self.translateable:
      return [self.GetMessage()]

    matches = {}
    for msglang in self.clique:
      if lang_re.match(msglang):
        matches[msglang] = self.clique[msglang]

    if include_pseudo:
      matches[pseudo.PSEUDO_LANG] = pseudo.PseudoMessage(self.GetMessage())

    return matches

  def AddTranslation(self, translation, language):
    '''Add a translation to this clique.  The translation must have the same
    ID as the message that is the source for this clique.

    If this clique is not translateable, the function just returns.

    Args:
      translation: tclib.Translation()
      language: 'en'

    Throws:
      grit.exception.InvalidTranslation if the translation you're trying to add
      doesn't have the same message ID as the source message of this clique.
    '''
    if not self.translateable:
      return
    if translation.GetId() != self.GetId():
      raise exception.InvalidTranslation(
        'Msg ID %s, transl ID %s' % (self.GetId(), translation.GetId()))

    assert not language in self.clique

    # Because two messages can differ in the original content of their
    # placeholders yet share the same ID (because they are otherwise the
    # same), the translation we are getting may have different original
    # content for placeholders than our message, yet it is still the right
    # translation for our message (because it is for the same ID).  We must
    # therefore fetch the original content of placeholders from our original
    # English message.
    #
    # See grit.clique_unittest.MessageCliqueUnittest.testSemiIdenticalCliques
    # for a concrete explanation of why this is necessary.

    original = self.MessageForLanguage(self.source_language, False)
    if len(original.GetPlaceholders()) != len(translation.GetPlaceholders()):
      print("ERROR: '%s' translation of message id %s does not match" %
            (language, translation.GetId()))
      assert False

    transl_msg = tclib.Translation(id=self.GetId(),
                                   text=translation.GetPresentableContent(),
                                   placeholders=original.GetPlaceholders())

    if (self.custom_type and
        not self.custom_type.ValidateAndModify(language, transl_msg)):
      print("WARNING: %s translation failed validation: %s" %
            (language, transl_msg.GetId()))

    self.clique[language] = transl_msg
