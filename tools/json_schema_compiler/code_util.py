# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class Code(object):
  """A convenience object for constructing code.

  Logically each object should be a block of code. All methods except |Render|
  and |IsEmpty| return self.
  """

  def __init__(self, indent_size=2, comment_length=80):
    self._code = []
    self._indent_size = indent_size
    self._comment_length = comment_length
    self._line_prefixes = []

  def Append(self,
             line='',
             substitute=True,
             indent_level=None,
             new_line=True,
             strip_right=True):
    """Appends a line of code at the current indent level or just a newline if
    line is not specified.

    substitute: indicated whether this line should be affected by
    code.Substitute().
    new_line: whether this should be added as a new line, or should be appended
        to the last line of the code.
    strip_right: whether or not trailing whitespace should be stripped.
    """

    if line:
      prefix = indent_level * ' ' if indent_level else ''.join(
          self._line_prefixes)
    else:
      prefix = ''

    if strip_right:
      line = line.rstrip()

    if not new_line and self._code:
      self._code[-1].value += line
    else:
      self._code.append(Line(prefix + line, substitute=substitute))
    return self

  def IsEmpty(self):
    """Returns True if the Code object is empty.
    """
    return not bool(self._code)

  def Concat(self, obj, new_line=True):
    """Concatenate another Code object onto this one. Trailing whitespace is
    stripped.

    Appends the code at the current indent level. Will fail if there are any
    un-interpolated format specifiers eg %s, %(something)s which helps
    isolate any strings that haven't been substituted.
    """
    if not isinstance(obj, Code):
      raise TypeError(type(obj))
    assert self is not obj
    if not obj._code:
      return self

    for line in obj._code:
      try:
        # line % () will fail if any substitution tokens are left in line
        if line.substitute:
          line.value %= ()
      except TypeError:
        raise TypeError('Unsubstituted value when concatting\n' + line.value)
      except ValueError:
        raise ValueError('Stray % character when concatting\n' + line.value)
    first_line = obj._code.pop(0)
    self.Append(first_line.value, first_line.substitute, new_line=new_line)
    for line in obj._code:
      self.Append(line.value, line.substitute)

    return self

  def Cblock(self, code):
    """Concatenates another Code object |code| onto this one followed by a
    blank line, if |code| is non-empty."""
    if not code.IsEmpty():
      self.Concat(code).Append()
    return self

  def Sblock(self, line=None, line_prefix=None, new_line=True):
    """Starts a code block.

    Appends a line of code and then increases the indent level. If |line_prefix|
    is present, it will be treated as the extra prefix for the code block.
    Otherwise, the prefix will be the default indent level.
    """
    if line is not None:
      self.Append(line, new_line=new_line)
    self._line_prefixes.append(line_prefix or ' ' * self._indent_size)
    return self

  def Eblock(self, line=None):
    """Ends a code block by decreasing and then appending a line (or a blank
    line if not given).
    """
    # TODO(calamity): Decide if type checking is necessary
    #if not isinstance(line, basestring):
    #  raise TypeError
    self._line_prefixes.pop()
    if line is not None:
      self.Append(line)
    return self

  def Comment(self,
              comment,
              comment_prefix='// ',
              wrap_indent=0,
              new_line=True):
    """Adds the given string as a comment.

    Will split the comment if it's too long. Use mainly for variable length
    comments. Otherwise just use code.Append('// ...') for comments.

    Unaffected by code.Substitute().
    """

    # Helper function to trim a comment to the maximum length, and return one
    # line and the remainder of the comment.
    def trim_comment(comment, max_len):
      if len(comment) <= max_len:
        return comment, ''
      # If we ran out of space due to existing content, don't try to wrap.
      if max_len <= 1:
        return '', comment.lstrip()
      last_space = comment.rfind(' ', 0, max_len + 1)
      if last_space != -1:
        line = comment[0:last_space]
        comment = comment[last_space + 1:]
      else:
        # If the line can't be split, then don't try.  The comments might be
        # important (e.g. JSDoc) where splitting it breaks things.
        line = comment
        comment = ''
      return line, comment.lstrip()

    # First line has the full maximum length.
    if not new_line and self._code:
      max_len = self._comment_length - len(self._code[-1].value)
    else:
      max_len = (self._comment_length - len(''.join(self._line_prefixes)) -
                 len(comment_prefix))
    line, comment = trim_comment(comment, max_len)
    self.Append(comment_prefix + line, substitute=False, new_line=new_line)

    # Any subsequent lines be subject to the wrap indent.
    max_len = (self._comment_length - len(''.join(self._line_prefixes)) -
               len(comment_prefix) - wrap_indent)
    assert max_len > 1
    while len(comment):
      line, comment = trim_comment(comment, max_len)
      self.Append(comment_prefix + (' ' * wrap_indent) + line, substitute=False)

    return self

  def Substitute(self, d):
    """Goes through each line and interpolates using the given dict.

    Raises type error if passed something that isn't a dict

    Use for long pieces of code using interpolation with the same variables
    repeatedly. This will reduce code and allow for named placeholders which
    are more clear.
    """
    if not isinstance(d, dict):
      raise TypeError('Passed argument is not a dictionary: ' + d)
    for i, line in enumerate(self._code):
      if self._code[i].substitute:
        # Only need to check %s because arg is a dict and python will allow
        # '%s %(named)s' but just about nothing else
        if '%s' in self._code[i].value or '%r' in self._code[i].value:
          raise TypeError('"%s" or "%r" found in substitution. '
                          'Named arguments only. Use "%" to escape')
        self._code[i].value = line.value % d
        self._code[i].substitute = False
    return self

  def TrimTrailingNewlines(self):
    """Removes any trailing empty Line objects.
    """
    while self._code:
      if self._code[-1].value != '':
        return
      self._code = self._code[:-1]

  def Render(self):
    """Renders Code as a string.
    """
    return '\n'.join([l.value for l in self._code])


class Line(object):
  """A line of code.
  """

  def __init__(self, value, substitute=True):
    self.value = value
    self.substitute = substitute
