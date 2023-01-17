# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Structure-preserving parser for resource_ids files.

Naive usage of eval() destroys resource_ids structure. This module provides a
custom parser that annotates source byte ranges of "leaf values" (strings and
integers).
"""


_isWhitespace = lambda ch: ch in ' \t\n'
_isNotNewline = lambda ch: ch != '\n'
_isDigit = lambda ch: ch.isdigit()


def _RenderLineCol(data, pos):
  """Renders |pos| within text |data| in as text showing line and column."""
  # This is used to pinpoint fatal parse errors, so okay to be inefficient.
  new_lines = [i for i in range(pos) if data[i] == '\n']
  row = 1 + len(new_lines)
  col = (pos - new_lines[-1]) if new_lines else 1 + pos
  return 'line %d, column %d' % (row, col)


def Tokenize(data):
  """Generator to split |data| into tokens.

  Each token is specified as |(t, lo, hi)|:
  * |t|: Type, with '#' = space / comments, '0' = int, 'S' = string, 'E' = end,
         and other characters denoting themselves.
  * |lo, hi|: Token's range within |data| (as |data[lo:hi]|).
  """

  class ctx:  # Local context for mutable data shared across inner functions.
    pos = 0

  def _HasData():
    return ctx.pos < len(data)

  # Returns True if ended by |not pred()|, or False if ended by EOF.
  def _EatWhile(pred):
    while _HasData():
      if pred(data[ctx.pos]):
        ctx.pos += 1
      else:
        return True
    return False

  def _NextBlank():
    lo = ctx.pos
    while True:
      if not _EatWhile(_isWhitespace) or data[ctx.pos] != '#':
        break
      ctx.pos += 1
      if not _EatWhile(_isNotNewline):
        break
      ctx.pos += 1
    return None if ctx.pos == lo else (lo, ctx.pos)

  def _EatString():
    lo = ctx.pos
    delim = data[ctx.pos]
    is_escaped = False
    ctx.pos += 1
    while _HasData():
      ch = data[ctx.pos]
      ctx.pos += 1
      if is_escaped:
        is_escaped = False
      elif ch == '\\':
        is_escaped = True
      elif ch == delim:
        return
    raise ValueError('Unterminated string at %s' % _RenderLineCol(data, lo))

  while _HasData():
    blank = _NextBlank()
    if blank is not None:
      yield ('#', blank[0], blank[1])
    if not _HasData():
      break
    lo = ctx.pos
    ch = data[ctx.pos]
    if ch in '{}[],:':
      ctx.pos += 1
      t = ch
    elif ch.isdigit():
      _EatWhile(_isDigit)
      t = '0'
    elif ch in '+-':
      ctx.pos += 1
      if not _HasData() or not data[ctx.pos].isdigit():
        raise ValueError('Invalid int at %s' % _RenderLineCol(data, lo))
      _EatWhile(_isDigit)
      t = '0'
    elif ch in '"\'':
      _EatString()
      t = 'S'
    else:
      raise ValueError('Unknown char %s at %s' %
                       (repr(ch), _RenderLineCol(data, lo)))
    yield (t, lo, ctx.pos)
  yield ('E', ctx.pos, ctx.pos)  # End sentinel.


def _SkipBlanks(toks):
  """Generator to remove whitespace and comments from Tokenize()."""
  for t, lo, hi in toks:
    if t != '#':
      yield t, lo, hi


class AnnotatedValue:
  """Container for leaf values (ints or strings) with an annotated range."""

  def __init__(self, val, lo, hi):
    self.val = val
    self.lo = lo
    self.hi = hi

  def __str__(self):
    return '<%s@%d:%d>' % (str(self.val), self.lo, self.hi)

  def __repr__(self):
    return '<%r@%d:%d>' % (self.val, self.lo, self.hi)

  def __hash__(self):
    return hash(self.val)

  def __eq__(self, other):
    return self.val == other


class ResourceIdParser:
  """resource_ids parser that stores leaf values as AnnotatedValue.

  Algorithm: Use Tokenize() to split |data| into tokens and _SkipBlanks() to
  ignore comments and spacing, then apply a recursive parsing, using a one-token
  look-ahead for decision making.
  """

  def __init__(self, data, tok_gen):
    self.data = data
    self.state = []
    self.toks = _SkipBlanks(tok_gen)
    self.tok_look_ahead = None

  def _MakeErr(self, msg, pos):
    return ValueError(msg + ' at ' + _RenderLineCol(self.data, pos))

  def _PeekTok(self):
    if self.tok_look_ahead is None:
      self.tok_look_ahead = next(self.toks)
    return self.tok_look_ahead

  def _NextTok(self):
    if self.tok_look_ahead is None:
      return next(self.toks)
    ret = self.tok_look_ahead
    self.tok_look_ahead = None
    return ret

  def _EatTok(self, exp_t, tok_name=None):
    t, lo, _ = self._NextTok()
    if t != exp_t:
      raise self._MakeErr('Bad token: Expect \'%s\'' % (tok_name or exp_t), lo)

  def _NextIntOrString(self):
    t, lo, hi = self._NextTok()
    if t != '0' and t != 'S':
      raise self._MakeErr('Expected number or string', lo)
    value = eval(self.data[lo:hi])
    return AnnotatedValue(value, lo, hi)

  # Consumes separator ',' and returns whether |end_ch| is encountered.
  def _EatSep(self, end_ch):
    t, lo, _ = self._PeekTok()
    if t == ',':
      self._EatTok(',')
      # Allow trailing ','.
      t, _, _ = self._PeekTok()
      return t == end_ch
    elif t == end_ch:
      return True
    else:
      raise self._MakeErr('Expect \',\' or \'%s\'' % end_ch, lo)

  def _NextList(self):
    self._EatTok('[')
    ret = []
    t, _, _ = self._PeekTok()
    if t != ']':
      while True:
        ret.append(self._NextObject())
        if self._EatSep(']'):
          break
    self._EatTok(']')
    return ret

  def _NextDict(self):
    self._EatTok('{')
    ret = {}
    t, _, _ = self._PeekTok()
    if t != '}':
      while True:
        k = self._NextIntOrString()
        self._EatTok(':')
        v = self._NextObject()
        ret[k] = v
        if self._EatSep('}'):
          break
    self._EatTok('}')
    return ret

  def _NextObject(self):
    t, lo, _ = self._PeekTok()
    if t == '[':
      return self._NextList()
    elif t == '{':
      return self._NextDict()
    elif t == '0' or t == 'S':
      return self._NextIntOrString()
    else:
      raise self._MakeErr('Bad token: Type = %s' % t, lo)

  def Parse(self):
    root_obj = self._NextObject()
    self._EatTok('E', 'EOF')
    return root_obj
