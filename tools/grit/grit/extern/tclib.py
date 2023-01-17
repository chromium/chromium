# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The tclib module contains tools for aggregating, verifying, and storing
# messages destined for the Translation Console, as well as for reading
# translations back and outputting them in some desired format.
#
# This has been stripped down to include only the functionality needed by grit
# for creating Windows .rc and .h files.  These are the only parts needed by
# the Chrome build process.


from grit.extern import FP

# This module assumes that within a bundle no two messages can have the
# same id unless they're identical.

# The basic classes defined here for external use are Message and Translation,
# where the former is used for English messages and the latter for
# translations. These classes have a lot of common functionality, as expressed
# by the common parent class BaseMessage. Perhaps the most important
# distinction is that translated text is stored in UTF-8, whereas original text
# is stored in whatever encoding the client uses (presumably Latin-1).

# --------------------
# The public interface
# --------------------

# Generate message id from message text and meaning string (optional),
# both in utf-8 encoding
#
def GenerateMessageId(message, meaning=''):
  fp = FP.FingerPrint(message)
  if meaning:
    # combine the fingerprints of message and meaning
    fp2 = FP.FingerPrint(meaning)
    if fp < 0:
      fp = fp2 + (fp << 1) + 1
    else:
      fp = fp2 + (fp << 1)
  # To avoid negative ids we strip the high-order bit
  return str(fp & 0x7fffffffffffffff)

# -------------------------------------------------------------------------
# The MessageTranslationError class is used to signal tclib-specific errors.


class MessageTranslationError(Exception):

  def __init__(self, args = ''):
    self.args = args


# -----------------------------------------------------------
# The Placeholder class represents a placeholder in a message.

class Placeholder:
  # String representation
  def __str__(self):
    return '%s, "%s", "%s"' % \
           (self.__presentation, self.__original, self.__example)

  # Getters
  def GetOriginal(self):
    return self.__original

  def GetPresentation(self):
    return self.__presentation

  def GetExample(self):
    return self.__example

  def __eq__(self, other):
    return self.EqualTo(other, strict=1, ignore_trailing_spaces=0)

  # Equality test
  #
  # ignore_trailing_spaces: TC is using varchar to store the
  # phrwr fields, as a result of that, the trailing spaces
  # are removed by MySQL when the strings are stored into TC:-(
  # ignore_trailing_spaces parameter is used to ignore
  # trailing spaces during equivalence comparison.
  #
  def EqualTo(self, other, strict = 1, ignore_trailing_spaces = 1):
    if type(other) is not Placeholder:
      return 0
    if StringEquals(self.__presentation, other.__presentation,
                    ignore_trailing_spaces):
      if not strict or (StringEquals(self.__original, other.__original,
                                     ignore_trailing_spaces)  and
                        StringEquals(self.__example, other.__example,
                                     ignore_trailing_spaces)):
        return 1
    return 0


# -----------------------------------------------------------------
# BaseMessage is the common parent class of Message and Translation.
# It is not meant for direct use.

class BaseMessage:
  # Three types of message construction is supported. If the message text is a
  # simple string with no dynamic content, you can pass it to the constructor
  # as the "text" parameter. Otherwise, you can omit "text" and assemble the
  # message step by step using AppendText() and AppendPlaceholder(). Or, as an
  # alternative, you can give the constructor the "presentable" version of the
  # message and a list of placeholders; it will then parse the presentation and
  # build the message accordingly. For example:
  # Message(text = "There are NUM_BUGS bugs in your code",
  #         placeholders = [Placeholder("NUM_BUGS", "%d", "33")],
  #         description = "Bla bla bla")
  def __eq__(self, other):
    # "source encoding" is nonsense, so ignore it
    return _ObjectEquals(self, other, ['_BaseMessage__source_encoding'])

  def GetName(self):
    return self.__name

  def GetSourceEncoding(self):
    return self.__source_encoding

  # Append a placeholder to the message
  def AppendPlaceholder(self, placeholder):
    if not isinstance(placeholder, Placeholder):
      raise MessageTranslationError("Invalid message placeholder %s in "
                                    "message %s" % (placeholder, self.GetId()))
    # Are there other placeholders with the same presentation?
    # If so, they need to be the same.
    for other in self.GetPlaceholders():
      if placeholder.GetPresentation() == other.GetPresentation():
        if not placeholder.EqualTo(other):
          raise MessageTranslationError(
              "Conflicting declarations of %s within message" %
              placeholder.GetPresentation())
    # update placeholder list
    dup = 0
    for item in self.__content:
      if isinstance(item, Placeholder) and placeholder.EqualTo(item):
        dup = 1
        break
    if not dup:
      self.__placeholders.append(placeholder)

    # update content
    self.__content.append(placeholder)

  # Strips leading and trailing whitespace, and returns a tuple
  # containing the leading and trailing space that was removed.
  def Strip(self):
    leading = trailing = ''
    if len(self.__content) > 0:
      s0 = self.__content[0]
      if not isinstance(s0, Placeholder):
        s = s0.lstrip()
        leading = s0[:-len(s)]
        self.__content[0] = s

      s0 = self.__content[-1]
      if not isinstance(s0, Placeholder):
        s = s0.rstrip()
        trailing = s0[len(s):]
        self.__content[-1] = s
    return leading, trailing

  # Return the id of this message
  def GetId(self):
    if self.__id is None:
      return self.GenerateId()
    return self.__id

  # Set the id of this message
  def SetId(self, id):
    if id is None:
      self.__id = None
    else:
      self.__id = str(id)  # Treat numerical ids as strings

  # Return content of this message as a list (internal use only)
  def GetContent(self):
    return self.__content

  # Return a human-readable version of this message
  def GetPresentableContent(self):
    presentable_content = ""
    for item in self.__content:
      if isinstance(item, Placeholder):
        presentable_content += item.GetPresentation()
      else:
        presentable_content += item

    return presentable_content

  # Return a fragment of a message in escaped format
  def EscapeFragment(self, fragment):
    return fragment.replace('%', '%%')

  # Return the "original" version of this message, doing %-escaping
  # properly.  If source_msg is specified, the placeholder original
  # information inside source_msg will be used instead.
  def GetOriginalContent(self, source_msg = None):
    original_content = ""
    for item in self.__content:
      if isinstance(item, Placeholder):
        if source_msg:
          ph = source_msg.GetPlaceholder(item.GetPresentation())
          if not ph:
            raise MessageTranslationError(
                "Placeholder %s doesn't exist in message: %s" %
                (item.GetPresentation(), source_msg))
          original_content += ph.GetOriginal()
        else:
          original_content += item.GetOriginal()
      else:
        original_content += self.EscapeFragment(item)
    return original_content

  # Return the example of this message
  def GetExampleContent(self):
    example_content = ""
    for item in self.__content:
      if isinstance(item, Placeholder):
        example_content += item.GetExample()
      else:
        example_content += item
    return example_content

  # Return a list of all unique placeholders in this message
  def GetPlaceholders(self):
    return self.__placeholders

  # Return a placeholder in this message
  def GetPlaceholder(self, presentation):
    for item in self.__content:
      if (isinstance(item, Placeholder) and
          item.GetPresentation() == presentation):
        return item
    return None

  # Return this message's description
  def GetDescription(self):
    return self.__description

  # Add a message source
  def AddSource(self, source):
    self.__sources.append(source)

  # Return this message's sources as a list
  def GetSources(self):
    return self.__sources

  # Return this message's sources as a string
  def GetSourcesAsText(self, delimiter = "; "):
    return delimiter.join(self.__sources)

  # Set the obsolete flag for a message (internal use only)
  def SetObsolete(self):
    self.__obsolete = 1

  # Get the obsolete flag for a message (internal use only)
  def IsObsolete(self):
    return self.__obsolete

  # Get the sequence number (0 by default)
  def GetSequenceNumber(self):
    return self.__sequence_number

  # Set the sequence number
  def SetSequenceNumber(self, number):
    self.__sequence_number = number

  # Increment instance counter
  def AddInstance(self):
    self.__num_instances += 1

  # Return instance count
  def GetNumInstances(self):
    return self.__num_instances

  def GetErrors(self, from_tc=0):
    """
    Returns a description of the problem if the message is not
    syntactically valid, or None if everything is fine.

    Args:
      from_tc: indicates whether this message came from the TC. We let
      the TC get away with some things we normally wouldn't allow for
      historical reasons.
    """
    # check that placeholders are unambiguous
    pos = 0
    phs = {}
    for item in self.__content:
      if isinstance(item, Placeholder):
        phs[pos] = item
        pos += len(item.GetPresentation())
      else:
        pos += len(item)
    presentation = self.GetPresentableContent()
    for ph in self.GetPlaceholders():
      for pos in FindOverlapping(presentation, ph.GetPresentation()):
        # message contains the same text as a placeholder presentation
        other_ph = phs.get(pos)
        if ((not other_ph
             and not IsSubstringInPlaceholder(pos, len(ph.GetPresentation()), phs))
            or
            (other_ph and len(other_ph.GetPresentation()) < len(ph.GetPresentation()))):
          return  "message contains placeholder name '%s':\n%s" % (
            ph.GetPresentation(), presentation)
    return None


  def __CopyTo(self, other):
    """
    Returns a copy of this BaseMessage.
    """
    assert isinstance(other,  self.__class__) or isinstance(self, other.__class__)
    other.__source_encoding = self.__source_encoding
    other.__content         = self.__content[:]
    other.__description     = self.__description
    other.__id              = self.__id
    other.__num_instances   = self.__num_instances
    other.__obsolete        = self.__obsolete
    other.__name            = self.__name
    other.__placeholders    = self.__placeholders[:]
    other.__sequence_number = self.__sequence_number
    other.__sources         = self.__sources[:]

    return other

  def HasText(self):
    """Returns true iff this message has anything other than placeholders."""
    for item in self.__content:
      if not isinstance(item, Placeholder):
        return True
    return False

# --------------------------------------------------------
# The Message class represents original (English) messages

class Message(BaseMessage):
  # See BaseMessage constructor
  def __init__(self, source_encoding, text=None, id=None,
               description=None, meaning="", placeholders=None,
               source=None, sequence_number=0, clone_from=None,
               time_created=0, name=None, is_hidden = 0):

    if clone_from is not None:
      BaseMessage.__init__(self, None, clone_from=clone_from)
      self.__meaning = clone_from.__meaning
      self.__time_created = clone_from.__time_created
      self.__is_hidden = clone_from.__is_hidden
      return

    BaseMessage.__init__(self, source_encoding, text, id, description,
                         placeholders, source, sequence_number,
                         name=name)
    self.__meaning = meaning
    self.__time_created = time_created
    self.SetIsHidden(is_hidden)

  # String representation
  def __str__(self):
    s = 'source: %s, id: %s, content: "%s", meaning: "%s", ' \
        'description: "%s"' % \
        (self.GetSourcesAsText(), self.GetId(), self.GetPresentableContent(),
         self.__meaning, self.GetDescription())
    if self.GetName() is not None:
      s += ', name: "%s"' % self.GetName()
    placeholders = self.GetPlaceholders()
    for i in range(len(placeholders)):
      s += ", placeholder[%d]: %s" % (i, placeholders[i])
    return s

  # Strips leading and trailing whitespace, and returns a tuple
  # containing the leading and trailing space that was removed.
  def Strip(self):
    leading = trailing = ''
    content = self.GetContent()
    if len(content) > 0:
      s0 = content[0]
      if not isinstance(s0, Placeholder):
        s = s0.lstrip()
        leading = s0[:-len(s)]
        content[0] = s

      s0 = content[-1]
      if not isinstance(s0, Placeholder):
        s = s0.rstrip()
        trailing = s0[len(s):]
        content[-1] = s
    return leading, trailing

  # Generate an id by hashing message content
  def GenerateId(self):
    self.SetId(GenerateMessageId(self.GetPresentableContent(),
                                 self.__meaning))
    return self.GetId()

  def GetMeaning(self):
    return self.__meaning

  def GetTimeCreated(self):
    return self.__time_created

  # Equality operator
  def EqualTo(self, other, strict = 1):
    # Check id, meaning, content
    if self.GetId() != other.GetId():
      return 0
    if self.__meaning != other.__meaning:
      return 0
    if self.GetPresentableContent() != other.GetPresentableContent():
      return 0
    # Check descriptions if comparison is strict
    if (strict and
        self.GetDescription() is not None and
        other.GetDescription() is not None and
        self.GetDescription() != other.GetDescription()):
      return 0
    # Check placeholders
    ph1 = self.GetPlaceholders()
    ph2 = other.GetPlaceholders()
    if len(ph1) != len(ph2):
      return 0
    for i in range(len(ph1)):
      if not ph1[i].EqualTo(ph2[i], strict):
        return 0

    return 1

  def Copy(self):
    """
    Returns a copy of this Message.
    """
    assert isinstance(self, Message)
    return Message(None, clone_from=self)

  def SetIsHidden(self, is_hidden):
    """Sets whether this message should be hidden.

    Args:
      is_hidden : 0 or 1 - if the message should be hidden, 0 otherwise
    """
    if is_hidden not in [0, 1]:
      raise MessageTranslationError("is_hidden must be 0 or 1, got %s")
    self.__is_hidden = is_hidden

  def IsHidden(self):
    """Returns 1 if this message is hidden, and 0 otherwise."""
    return self.__is_hidden

# ----------------------------------------------------
# The Translation class represents translated messages

class Translation(BaseMessage):
  # See BaseMessage constructor
  def __init__(self, source_encoding, text=None, id=None,
               description=None, placeholders=None, source=None,
               sequence_number=0, clone_from=None, ignore_ph_errors=0,
               name=None):
    if clone_from is not None:
      BaseMessage.__init__(self, None, clone_from=clone_from)
      return

    BaseMessage.__init__(self, source_encoding, text, id, description,
                         placeholders, source, sequence_number,
                         ignore_ph_errors=ignore_ph_errors, name=name)

  # String representation
  def __str__(self):
    s = 'source: %s, id: %s, content: "%s", description: "%s"' % \
        (self.GetSourcesAsText(), self.GetId(), self.GetPresentableContent(),
         self.GetDescription());
    placeholders = self.GetPlaceholders()
    for i in range(len(placeholders)):
      s += ", placeholder[%d]: %s" % (i, placeholders[i])
    return s

  # Equality operator
  def EqualTo(self, other, strict=1):
    # Check id and content
    if self.GetId() != other.GetId():
      return 0
    if self.GetPresentableContent() != other.GetPresentableContent():
      return 0
    # Check placeholders
    ph1 = self.GetPlaceholders()
    ph2 = other.GetPlaceholders()
    if len(ph1) != len(ph2):
      return 0
    for i in range(len(ph1)):
      if not ph1[i].EqualTo(ph2[i], strict):
        return 0

    return 1

  def Copy(self):
    """
    Returns a copy of this Translation.
    """
    return Translation(None, clone_from=self)
