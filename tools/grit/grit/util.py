# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Utilities used by GRIT.
'''

from __future__ import print_function

import codecs
import io
import os
import re
import shutil
import sys
import tempfile
from xml.sax import saxutils

import six
from six import StringIO
from six.moves import html_entities as entities

from grit import lazy_re

_root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


# Unique constants for use by ReadFile().
BINARY = 0


# Unique constants representing data pack encodings.
_, UTF8, UTF16 = range(3)


def Encode(message, encoding):
  '''Returns a byte stream that represents |message| in the given |encoding|.'''
  # |message| is a python unicode string, so convert to a byte stream that
  # has the correct encoding requested for the datapacks. We skip the first
  # 2 bytes of text resources because it is the BOM.
  if encoding == UTF8:
    return message.encode('utf8')
  if encoding == UTF16:
    return message.encode('utf16')[2:]
  # Default is BINARY
  return message


# Matches all different types of linebreaks.
LINEBREAKS = re.compile('\r\n|\n|\r')

def MakeRelativePath(base_path, path_to_make_relative):
  """Returns a relative path such from the base_path to
  the path_to_make_relative.

  In other words, os.join(base_path,
    MakeRelativePath(base_path, path_to_make_relative))
  is the same location as path_to_make_relative.

  Args:
    base_path: the root path
    path_to_make_relative: an absolute path that is on the same drive
      as base_path
  """

  def _GetPathAfterPrefix(prefix_path, path_with_prefix):
    """Gets the subpath within in prefix_path for the path_with_prefix
    with no beginning or trailing path separators.

    Args:
      prefix_path: the base path
      path_with_prefix: a path that starts with prefix_path
    """
    assert path_with_prefix.startswith(prefix_path)
    path_without_prefix = path_with_prefix[len(prefix_path):]
    normalized_path = os.path.normpath(path_without_prefix.strip(os.path.sep))
    if normalized_path == '.':
      normalized_path = ''
    return normalized_path

  def _GetCommonBaseDirectory(*args):
    """Returns the common prefix directory for the given paths

    Args:
      The list of paths (at least one of which should be a directory)
    """
    prefix = os.path.commonprefix(args)
    # prefix is a character-by-character prefix (i.e. it does not end
    # on a directory bound, so this code fixes that)

    # if the prefix ends with the separator, then it is prefect.
    if len(prefix) > 0 and prefix[-1] == os.path.sep:
      return prefix

    # We need to loop through all paths or else we can get
    # tripped up by "c:\a" and "c:\abc".  The common prefix
    # is "c:\a" which is a directory and looks good with
    # respect to the first directory but it is clear that
    # isn't a common directory when the second path is
    # examined.
    for path in args:
      assert len(path) >= len(prefix)
      # If the prefix the same length as the path,
      # then the prefix must be a directory (since one
      # of the arguements should be a directory).
      if path == prefix:
        continue
      # if the character after the prefix in the path
      # is the separator, then the prefix appears to be a
      # valid a directory as well for the given path
      if path[len(prefix)] == os.path.sep:
        continue
      # Otherwise, the prefix is not a directory, so it needs
      # to be shortened to be one
      index_sep = prefix.rfind(os.path.sep)
      # The use "index_sep + 1" because it includes the final sep
      # and it handles the case when the index_sep is -1 as well
      prefix = prefix[:index_sep + 1]
      # At this point we backed up to a directory bound which is
      # common to all paths, so we can quit going through all of
      # the paths.
      break
    return prefix

  prefix =  _GetCommonBaseDirectory(base_path, path_to_make_relative)
  # If the paths had no commonality at all, then return the absolute path
  # because it is the best that can be done.  If the path had to be relative
  # then eventually this absolute path will be discovered (when a build breaks)
  # and an appropriate fix can be made, but having this allows for the best
  # backward compatibility with the absolute path behavior in the past.
  if len(prefix) <= 0:
    return path_to_make_relative
  # Build a path from the base dir to the common prefix
  remaining_base_path = _GetPathAfterPrefix(prefix, base_path)

  #  The follow handles two case: "" and "foo\\bar"
  path_pieces = remaining_base_path.split(os.path.sep)
  base_depth_from_prefix = len([d for d in path_pieces if len(d)])
  base_to_prefix = (".." + os.path.sep) * base_depth_from_prefix

  # Put add in the path from the prefix to the path_to_make_relative
  remaining_other_path = _GetPathAfterPrefix(prefix, path_to_make_relative)
  return base_to_prefix + remaining_other_path


KNOWN_SYSTEM_IDENTIFIERS = set()

SYSTEM_IDENTIFIERS = None

def SetupSystemIdentifiers(ids):
  '''Adds ids to a regexp of known system identifiers.

  Can be called many times, ids will be accumulated.

  Args:
    ids: an iterable of strings
  '''
  KNOWN_SYSTEM_IDENTIFIERS.update(ids)
  global SYSTEM_IDENTIFIERS
  SYSTEM_IDENTIFIERS = lazy_re.compile(
      ' | '.join([r'\b%s\b' % i for i in KNOWN_SYSTEM_IDENTIFIERS]),
      re.VERBOSE)


# Matches all of the resource IDs predefined by Windows.
SetupSystemIdentifiers((
    'IDOK', 'IDCANCEL', 'IDC_STATIC', 'IDYES', 'IDNO',
    'ID_FILE_NEW', 'ID_FILE_OPEN', 'ID_FILE_CLOSE', 'ID_FILE_SAVE',
    'ID_FILE_SAVE_AS', 'ID_FILE_PAGE_SETUP', 'ID_FILE_PRINT_SETUP',
    'ID_FILE_PRINT', 'ID_FILE_PRINT_DIRECT', 'ID_FILE_PRINT_PREVIEW',
    'ID_FILE_UPDATE', 'ID_FILE_SAVE_COPY_AS', 'ID_FILE_SEND_MAIL',
    'ID_FILE_MRU_FIRST', 'ID_FILE_MRU_LAST',
    'ID_EDIT_CLEAR', 'ID_EDIT_CLEAR_ALL', 'ID_EDIT_COPY',
    'ID_EDIT_CUT', 'ID_EDIT_FIND', 'ID_EDIT_PASTE', 'ID_EDIT_PASTE_LINK',
    'ID_EDIT_PASTE_SPECIAL', 'ID_EDIT_REPEAT', 'ID_EDIT_REPLACE',
    'ID_EDIT_SELECT_ALL', 'ID_EDIT_UNDO', 'ID_EDIT_REDO',
    'VS_VERSION_INFO', 'IDRETRY',
    'ID_APP_ABOUT', 'ID_APP_EXIT',
    'ID_NEXT_PANE', 'ID_PREV_PANE',
    'ID_WINDOW_NEW', 'ID_WINDOW_ARRANGE', 'ID_WINDOW_CASCADE',
    'ID_WINDOW_TILE_HORZ', 'ID_WINDOW_TILE_VERT', 'ID_WINDOW_SPLIT',
    'ATL_IDS_SCSIZE', 'ATL_IDS_SCMOVE', 'ATL_IDS_SCMINIMIZE',
    'ATL_IDS_SCMAXIMIZE', 'ATL_IDS_SCNEXTWINDOW', 'ATL_IDS_SCPREVWINDOW',
    'ATL_IDS_SCCLOSE', 'ATL_IDS_SCRESTORE', 'ATL_IDS_SCTASKLIST',
    'ATL_IDS_MDICHILD', 'ATL_IDS_IDLEMESSAGE', 'ATL_IDS_MRU_FILE' ))


# Matches character entities, whether specified by name, decimal or hex.
_HTML_ENTITY = lazy_re.compile(
  '&(#(?P<decimal>[0-9]+)|#x(?P<hex>[a-fA-F0-9]+)|(?P<named>[a-z0-9]+));',
  re.IGNORECASE)

# Matches characters that should be HTML-escaped.  This is <, > and &, but only
# if the & is not the start of an HTML character entity.
_HTML_CHARS_TO_ESCAPE = lazy_re.compile(
    '"|<|>|&(?!#[0-9]+|#x[0-9a-z]+|[a-z]+;)',
    re.IGNORECASE | re.MULTILINE)


def ReadFile(filename, encoding):
  '''Reads and returns the entire contents of the given file.

  Args:
    filename: The path to the file.
    encoding: A Python codec name or the special value: BINARY to read
              the file in binary mode.
  '''
  if encoding == BINARY:
    mode = 'rb'
    encoding = None
  else:
    mode = 'rU'

  with io.open(filename, mode, encoding=encoding) as f:
    return f.read()


def WrapOutputStream(stream, encoding = 'utf-8'):
  '''Returns a stream that wraps the provided stream, making it write
  characters using the specified encoding.'''
  return codecs.getwriter(encoding)(stream)


def ChangeStdoutEncoding(encoding = 'utf-8'):
  '''Changes STDOUT to print characters using the specified encoding.'''
  # If we're unittesting, don't reconfigure.
  if isinstance(sys.stdout, StringIO):
    return

  if sys.version_info.major < 3:
    # Python 2 has binary streams by default, so reconfigure directly.
    sys.stdout = WrapOutputStream(sys.stdout, encoding)
    sys.stderr = WrapOutputStream(sys.stderr, encoding)
  elif sys.version_info < (3, 7):
    # Python 3 has text streams by default, so we have to detach them first.
    sys.stdout = WrapOutputStream(sys.stdout.detach(), encoding)
    sys.stderr = WrapOutputStream(sys.stderr.detach(), encoding)
  else:
    # Python 3.7+ provides an API for this specifically.
    sys.stdout.reconfigure(encoding=encoding)
    sys.stderr.reconfigure(encoding=encoding)


def EscapeHtml(text, escape_quotes = False):
  '''Returns 'text' with <, > and & (and optionally ") escaped to named HTML
  entities.  Any existing named entity or HTML entity defined by decimal or
  hex code will be left untouched.  This is appropriate for escaping text for
  inclusion in HTML, but not for XML.
  '''
  def Replace(match):
    if match.group() == '&': return '&amp;'
    elif match.group() == '<': return '&lt;'
    elif match.group() == '>': return '&gt;'
    elif match.group() == '"':
      if escape_quotes: return '&quot;'
      else: return match.group()
    else: assert False
  out = _HTML_CHARS_TO_ESCAPE.sub(Replace, text)
  return out


def UnescapeHtml(text, replace_nbsp=True):
  '''Returns 'text' with all HTML character entities (both named character
  entities and those specified by decimal or hexadecimal Unicode ordinal)
  replaced by their Unicode characters (or latin1 characters if possible).

  The only exception is that &nbsp; will not be escaped if 'replace_nbsp' is
  False.
  '''
  def Replace(match):
    groups = match.groupdict()
    if groups['hex']:
      return six.unichr(int(groups['hex'], 16))
    elif groups['decimal']:
      return six.unichr(int(groups['decimal'], 10))
    else:
      name = groups['named']
      if name == 'nbsp' and not replace_nbsp:
        return match.group()  # Don't replace &nbsp;
      assert name != None
      if name in entities.name2codepoint:
        return six.unichr(entities.name2codepoint[name])
      else:
        return match.group()  # Unknown HTML character entity - don't replace

  out = _HTML_ENTITY.sub(Replace, text)
  return out


def EncodeCdata(cdata):
  '''Returns the provided cdata in either escaped format or <![CDATA[xxx]]>
  format, depending on which is more appropriate for easy editing.  The data
  is escaped for inclusion in an XML element's body.

  Args:
    cdata: 'If x < y and y < z then x < z'

  Return:
    '<![CDATA[If x < y and y < z then x < z]]>'
  '''
  if cdata.count('<') > 1 or cdata.count('>') > 1 and cdata.count(']]>') == 0:
    return '<![CDATA[%s]]>' % cdata
  else:
    return saxutils.escape(cdata)


def FixupNamedParam(function, param_name, param_value):
  '''Returns a closure that is identical to 'function' but ensures that the
  named parameter 'param_name' is always set to 'param_value' unless explicitly
  set by the caller.

  Args:
    function: callable
    param_name: 'bingo'
    param_value: 'bongo' (any type)

  Return:
    callable
  '''
  def FixupClosure(*args, **kw):
    if not param_name in kw:
      kw[param_name] = param_value
    return function(*args, **kw)
  return FixupClosure


def PathFromRoot(path):
  r'''Takes a path relative to the root directory for GRIT (the one that grit.py
  resides in) and returns a path that is either absolute or relative to the
  current working directory (i.e .a path you can use to open the file).

  Args:
    path: 'rel_dir\file.ext'

  Return:
    'c:\src\tools\rel_dir\file.ext
  '''
  return os.path.normpath(os.path.join(_root_dir, path))


def ParseGrdForUnittest(body, base_dir=None, predetermined_ids_file=None,
                        run_gatherers=False):
  '''Parse a skeleton .grd file and return it, for use in unit tests.

  Args:
    body: XML that goes inside the <release> element.
    base_dir: The base_dir attribute of the <grit> tag.
  '''
  from grit import grd_reader
  if isinstance(body, six.text_type):
    body = body.encode('utf-8')
  if base_dir is None:
    base_dir = PathFromRoot('.')
  lines = [b'<?xml version="1.0" encoding="UTF-8"?>']
  lines.append(b'<grit latest_public_release="2" current_release="3" '
               b'source_lang_id="en" base_dir="%s">' % base_dir.encode('utf-8'))
  if b'<outputs>' in body:
    lines.append(body)
  else:
    lines.append(b'  <outputs></outputs>')
    lines.append(b'  <release seq="3">')
    lines.append(body)
    lines.append(b'  </release>')
  lines.append(b'</grit>')
  ret = grd_reader.Parse(io.BytesIO(b'\n'.join(lines)), dir='.')
  ret.SetOutputLanguage('en')
  if run_gatherers:
    ret.RunGatherers()
  ret.SetPredeterminedIdsFile(predetermined_ids_file)
  ret.InitializeIds()
  return ret


def StripBlankLinesAndComments(text):
  '''Strips blank lines and comments from C source code, for unit tests.'''
  return '\n'.join(line for line in text.splitlines()
                        if line and not line.startswith('//'))


def dirname(filename):
  '''Version of os.path.dirname() that never returns empty paths (returns
  '.' if the result of os.path.dirname() is empty).
  '''
  ret = os.path.dirname(filename)
  if ret == '':
    ret = '.'
  return ret


def normpath(path):
  '''Version of os.path.normpath that also changes backward slashes to
  forward slashes when not running on Windows.
  '''
  # This is safe to always do because the Windows version of os.path.normpath
  # will replace forward slashes with backward slashes.
  path = path.replace('\\', '/')
  return os.path.normpath(path)


_LANGUAGE_SPLIT_RE = lazy_re.compile('-|_|/')


def CanonicalLanguage(code):
  '''Canonicalizes two-part language codes by using a dash and making the
  second part upper case.  Returns one-part language codes unchanged.

  Args:
    code: 'zh_cn'

  Return:
    code: 'zh-CN'
  '''
  parts = _LANGUAGE_SPLIT_RE.split(code)
  code = [ parts[0] ]
  for part in parts[1:]:
    code.append(part.upper())
  return '-'.join(code)


_LANG_TO_CODEPAGE = {
  'en' : 1252,
  'fr' : 1252,
  'it' : 1252,
  'de' : 1252,
  'es' : 1252,
  'nl' : 1252,
  'sv' : 1252,
  'no' : 1252,
  'da' : 1252,
  'fi' : 1252,
  'pt-BR' : 1252,
  'ru' : 1251,
  'ja' : 932,
  'zh-TW' : 950,
  'zh-CN' : 936,
  'ko' : 949,
}


def LanguageToCodepage(lang):
  '''Returns the codepage _number_ that can be used to represent 'lang', which
  may be either in formats such as 'en', 'pt_br', 'pt-BR', etc.

  The codepage returned will be one of the 'cpXXXX' codepage numbers.

  Args:
    lang: 'de'

  Return:
    1252
  '''
  lang = CanonicalLanguage(lang)
  if lang in _LANG_TO_CODEPAGE:
    return _LANG_TO_CODEPAGE[lang]
  else:
    print("Not sure which codepage to use for %s, assuming cp1252" % lang)
    return 1252

def NewClassInstance(class_name, class_type):
  '''Returns an instance of the class specified in classname

  Args:
    class_name: the fully qualified, dot separated package + classname,
    i.e. "my.package.name.MyClass". Short class names are not supported.
    class_type: the class or superclass this object must implement

  Return:
    An instance of the class, or None if none was found
  '''
  lastdot = class_name.rfind('.')
  module_name = ''
  if lastdot >= 0:
    module_name = class_name[0:lastdot]
    if module_name:
      class_name = class_name[lastdot+1:]
      module = __import__(module_name, globals(), locals(), [''])
      if hasattr(module, class_name):
        class_ = getattr(module, class_name)
        class_instance = class_()
        if isinstance(class_instance, class_type):
          return class_instance
  return None


def FixLineEnd(text, line_end):
  # First normalize
  text = text.replace('\r\n', '\n')
  text = text.replace('\r', '\n')
  # Then fix
  text = text.replace('\n', line_end)
  return text


def BoolToString(bool):
  if bool:
    return 'true'
  else:
    return 'false'


verbose = False
extra_verbose = False

def IsVerbose():
  return verbose

def IsExtraVerbose():
  return extra_verbose

def ParseDefine(define):
  '''Parses a define argument and returns the name and value.

  The format is either "NAME=VAL" or "NAME", using True as the default value.
  Values of "1"/"true" and "0"/"false" are transformed to True and False
  respectively.

  Args:
    define: a string of the form "NAME=VAL" or "NAME".

  Returns:
    A (name, value) pair. name is a string, value a string or boolean.
  '''
  parts = [part.strip() for part in define.split('=', 1)]
  assert len(parts) >= 1
  name = parts[0]
  val = True
  if len(parts) > 1:
    val = parts[1]
  if val == "1" or val == "true": val = True
  elif val == "0" or val == "false": val = False
  return (name, val)


class Substituter(object):
  '''Finds and substitutes variable names in text strings.

  Given a dictionary of variable names and values, prepares to
  search for patterns of the form [VAR_NAME] in a text.
  The value will be substituted back efficiently.
  Also applies to tclib.Message objects.
  '''

  def __init__(self):
    '''Create an empty substituter.'''
    self.substitutions_ = {}
    self.dirty_ = True

  def AddSubstitutions(self, subs):
    '''Add new values to the substitutor.

    Args:
      subs: A dictionary of new substitutions.
    '''
    self.substitutions_.update(subs)
    self.dirty_ = True

  def AddMessages(self, messages, lang):
    '''Adds substitutions extracted from node.Message objects.

    Args:
      messages: a list of node.Message objects.
      lang: The translation language to use in substitutions.
    '''
    subs = [(str(msg.attrs['name']), msg.Translate(lang)) for msg in messages]
    self.AddSubstitutions(dict(subs))
    self.dirty_ = True

  def GetExp(self):
    '''Obtain a regular expression that will find substitution keys in text.

    Create and cache if the substituter has been updated. Use the cached value
    otherwise. Keys will be enclosed in [square brackets] in text.

    Returns:
      A regular expression object.
    '''
    if self.dirty_:
      components = [r'\[%s\]' % (k,) for k in self.substitutions_]
      self.exp = re.compile(r'(%s)' % ('|'.join(components),))
      self.dirty_ = False
    return self.exp

  def Substitute(self, text):
    '''Substitute the variable values in the given text.

    Text of the form [message_name] will be replaced by the message's value.

    Args:
      text: A string of text.

    Returns:
      A string of text with substitutions done.
    '''
    return ''.join([self._SubFragment(f) for f in self.GetExp().split(text)])

  def _SubFragment(self, fragment):
    '''Utility function for Substitute.

    Performs a simple substitution if the fragment is exactly of the form
    [message_name].

    Args:
      fragment: A simple string.

    Returns:
      A string with the substitution done.
    '''
    if len(fragment) > 2 and fragment[0] == '[' and fragment[-1] == ']':
      sub = self.substitutions_.get(fragment[1:-1], None)
      if sub is not None:
        return sub
    return fragment

  def SubstituteMessage(self, msg):
    '''Apply substitutions to a tclib.Message object.

    Text of the form [message_name] will be replaced by a new placeholder,
    whose presentation will take the form the message_name_{UsageCount}, and
    whose example will be the message's value. Existing placeholders are
    not affected.

    Args:
      msg: A tclib.Message object.

    Returns:
      A tclib.Message object, with substitutions done.
    '''
    from grit import tclib  # avoid circular import
    counts = {}
    text = msg.GetPresentableContent()
    placeholders = []
    newtext = ''
    for f in self.GetExp().split(text):
      sub = self._SubFragment(f)
      if f != sub:
        f = str(f)
        count = counts.get(f, 0) + 1
        counts[f] = count
        name = "%s_%d" % (f[1:-1], count)
        placeholders.append(tclib.Placeholder(name, f, sub))
        newtext += name
      else:
        newtext += f
    if placeholders:
      return tclib.Message(newtext, msg.GetPlaceholders() + placeholders,
                           msg.GetDescription(), msg.GetMeaning())
    else:
      return msg


class TempDir(object):
  '''Creates files with the specified contents in a temporary directory,
  for unit testing.
  '''

  def __init__(self, file_data, mode='w'):
    self._tmp_dir_name = tempfile.mkdtemp()
    assert not os.listdir(self.GetPath())
    for name, contents in file_data.items():
      file_path = self.GetPath(name)
      dir_path = os.path.split(file_path)[0]
      if not os.path.exists(dir_path):
        os.makedirs(dir_path)
      with open(file_path, mode) as f:
        f.write(file_data[name])

  def __enter__(self):
    return self

  def __exit__(self, *exc_info):
    self.CleanUp()

  def CleanUp(self):
    shutil.rmtree(self.GetPath())

  def GetPath(self, name=''):
    name = os.path.join(self._tmp_dir_name, name)
    assert name.startswith(self._tmp_dir_name)
    return name

  def AsCurrentDir(self):
    return self._AsCurrentDirClass(self.GetPath())

  class _AsCurrentDirClass(object):
    def __init__(self, path):
      self.path = path
    def __enter__(self):
      self.oldpath = os.getcwd()
      os.chdir(self.path)
    def __exit__(self, *exc_info):
      os.chdir(self.oldpath)
