#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate a compact character composition table.

Normal use:
  ./generate_character_composer_data.py \
      --output character_composer_data.h \
      character_composer_sequences.txt

Run this script with --help for a description of arguments.

Input file format:

  Comments begin with '#' and extend to the end of the line.

  Each non-comment line is a sequence of two or more keys, separated by
  space, the last of which is the result of composing the initial part.
  A sequence must begin with a dead key or Compose, and the result must
  be a character key.

  Each key can either be a character key, a dead key, or the compose key.

  A character key is written as one of the following inside matched delimiters:
  - a single printable ASCII character;
  - a Unicode character name;
  - 'U+' followed by one or more hexadecimal digits.
  Delimiter pairs are any of '' "" () <> [] or {}.

  A dead key is written as the word 'Dead' followed (immediately, without space)
  by the combining character written in the same form as a character key.

  A compose key is written as the word 'Compose'.

Output file format:

  The output file is a C++ header containing a small header structure
  |ui::TreeComposeChecker::CompositionData| and a tree of composition sequences.

  For space efficiency, the tree is stored in a single array of 16-bit values,
  which represent either characters (printable or dead-key) or subtree array
  indices.

  Each tree level consists for four tables: two key kinds (character or dead)
  by two node types (internal or leaf), in the order:
  - character internal
  - character leaf
  - dead-key internal
  - dead-key leaf
  This order is used because character key entries are more common than dead-key
  entries, and internal entries are more common than leaf entries.

  Each table consists of a word containing the number of table entries |n|,
  followed immediately by |n| key-value pairs of words, ordered by key.
  For internal edges, the value is the array index of the resulting subtree.
  For leaf edges, the value is the unicode character code of the composition
  result.
"""

import argparse
import codecs
import collections
import sys
import unicodedata

# Global error counter.
g_fail = 0

unichr_compat = chr if sys.version_info[0] == 3 else unichr

class Key(str):
  """Represents an element of a composition sequence.

  Supports only Compose, dead keys, and BMP unicode characters.
  Based on |str| for easy comparison and sorting.
  The representation is 'C' (for unicode characters) or 'D' (for dead keys)
  followed by 4 hex digits for the character value. The Compose key is
  represented as dead key with combining character 0.
  """
  _kinds = ['character', 'dead_key']

  def __new__(cls, key, character, location=None):
    """Construct a Key.
    Call as:
    - Key(None, character_code)
    - Key('Dead', combining_character_code)
    - Key('Compose', 0)
    """
    global g_fail
    if character is not None and character > 0xFFFF:
      print('{}: unsupported non-BMP character {}'.format(location, character))
      g_fail += 1
      s = 'ERROR'
    elif key is None:
      s = 'C{:04X}'.format(character)
    elif key.lower() == 'dead':
      s = 'D{:04X}'.format(character)
    elif key.lower() == 'compose':
      s = 'D0000'
    else:
      print('{}: unexpected combination {}<{}>'
            .format(location, key, character))
      g_fail += 1
      s = 'ERROR'
    return str.__new__(cls, s)

  def Kind(self):
    return {'C': 'character', 'D': 'dead_key'}[self[0]]

  def CharacterCode(self):
    return int(self[1:], 16)

  def UnicodeName(self):
    v = self.CharacterCode()
    try:
      return unicodedata.name(unichr_compat(v)).lower()
    except ValueError:
      return 'U+{:04X}'.format(v)

  def ShorterUnicodeName(self):
    s = self.UnicodeName()
    if s.startswith('latin ') or s.startswith('greek '):
      s = s[6:]
    if s.startswith('small '):
      s = s[6:]
    return s.replace(' letter ', ' ')

  def Pretty(self):
    if self == 'D0000':
      return 'Compose'
    return ('Dead' if self[0] == 'D' else '') + '<' + self.UnicodeName() + '>'


class Input:
  """
  Takes a sequences of file names and presents them as a single input stream,
  with location reporting for error messages.
  """
  def __init__(self, filenames):
    self._pending = filenames
    self._filename = None
    self._file = None
    self._line = None
    self._lineno = 0
    self._column = 0

  def Where(self):
    """Report the current input location, for error messages."""
    if self._file:
      return '{}:{}:{}'.format(self._filename, self._lineno, self._column)
    if self._pending:
      return '<before>'
    return '<eof>'

  def Get(self):
    """Return the next input character, or None when inputs are exhausted."""
    if self._line is None:
      if self._file is None:
        if not self._pending:
          return None
        self._filename = self._pending[0]
        self._pending = self._pending[1:]
        self._file = codecs.open(self._filename, mode='rb', encoding='utf-8')
        self._lineno = 0
      self._lineno += 1
      self._line = self._file.readline()
      if not self._line:
        self._file = None
        self._filename = None
        return self.Get()
      self._column = 0
    if self._column >= len(self._line):
      self._line = None
      return self.Get()
    c = self._line[self._column]
    self._column += 1
    return c


class Lexer:
  """
  Breaks the input stream into a sequence of tokens, each of which is either
  a Key or the string 'EOL'.
  """
  def __init__(self, compose_input):
    self._input = compose_input

  _delimiters = {
      '"': '"',
      "'": "'",
      '<': '>',
      '(': ')',
      '[': ']',
      '{': '}',
  }

  def GetUntilDelimiter(self, e):
    text = ''
    c = self._input.Get()
    while c and c != e:
      text += c
      c = self._input.Get()
    return text

  def Get(self):
    global g_fail
    c = ' '
    while c and c.isspace() and c != '\n':
      c = self._input.Get()
    if not c:
      return None
    location = self._input.Where()
    if c == '\n':
      return 'EOL'
    if c == '#':
      self.GetUntilDelimiter('\n')
      return 'EOL'
    if c == '\\':
      self.GetUntilDelimiter('\n')
      return self.Get()
    key = None
    character = None
    if c.isalnum():
      key = ''
      while c and c.isalnum():
        key += c
        c = self._input.Get()
    if c in Lexer._delimiters:
      s = self.GetUntilDelimiter(Lexer._delimiters[c])
      if len(s) == 1:
        character = ord(s)
      elif s.startswith('U+'):
        character = int(s[2:], 16)
      else:
        try:
          character = ord(unicodedata.lookup(s.upper()))
        except KeyError:
          g_fail += 1
          character = None
          print('{}: unknown character name "{}"'.format(location,
                                                         s.encode('utf-8')))
    return Key(key, character, location)

  def Where(self):
    return self._input.Where()


class Parser:
  """
  Takes a sequence of tokens from a Lexer and returns a tree of
  composition sequences, represented as nested dictionaries where each
  composition source element key is a dictionary key, and the final
  composition result has a dictionary key of |None|.
  """
  def __init__(self, lexer):
    self._lexer = lexer
    self._trie = {}

  def Parse(self):
    global g_fail
    self._trie = {}
    while True:
      seq = []
      t = self._lexer.Get()
      if not t:
        break
      if t and t != 'EOL' and t.Kind() != 'dead_key':
        g_fail += 1
        print('{}: sequence does not begin with Compose or Dead key'
              .format(self._lexer.Where()))
        break
      while t and t != 'EOL':
        seq.append(t)
        t = self._lexer.Get()
      if not seq:
        continue
      self.AddToSimpleTree(self._trie, seq)
    return self._trie

  def AddToSimpleTree(self, tree, seq):
    first = seq[0]
    rest = seq[1:]
    if first not in tree:
      tree[first] = {}
    if len(rest) == 1:
      # Leaf
      tree[first][None] = rest[0]
    else:
      self.AddToSimpleTree(tree[first], rest)


class GroupedTree:
  """
  Represents composition sequences in a manner close to the output format.

  The core of the representation is the |_tables| dictionary, which has
  an entry for each kind of |Key|, each of which is a dictionary with
  two entries, 'internal' and 'leaf', for the output tables, each being
  a dictionary indexed by a composition sequence |Key|. For 'internal'
  tables the dictionary values are |GroupedTree|s at the next level;
  for 'leaf' tables the dictionary values are |Key| composition results.
  """
  _key_kinds = Key._kinds
  _sub_parts = ['internal', 'leaf']

  def __init__(self, simple_trie, path=None):
    if path is None:
      path = []
    self.path = path
    self.depth = len(path)
    self.height = 0
    self.empty = True
    self.location = -1

    # Initialize |_tables|.
    self._tables = {}
    for k in self._key_kinds:
      self._tables[k] = {}
      for p in self._sub_parts:
        self._tables[k][p] = {}

    # Build the tables.
    for key in simple_trie:
      if key is not None:
        # Leaf table entry.
        if None in simple_trie[key]:
          self.empty = False
          self._tables[key.Kind()]['leaf'][key] = simple_trie[key][None]
        # Internal subtree entries.
        v = GroupedTree(simple_trie[key], path + [key])
        if not v.empty:
          self.empty = False
          self._tables[key.Kind()]['internal'][key] = v
          if self.height < v.height:
            self.height = v.height
    self.height += 1

  def SubTrees(self):
    """Returns a list of all sub-|GroupedTree|s of the current GroupTree."""
    r = []
    for k in self._key_kinds:
      for key in sorted(self._tables[k]['internal']):
        r.append(self._tables[k]['internal'][key])
    return r


class Assembler:
  """Convert a parse tree via a GroupedTree to a C++ header."""

  def __init__(self, args, dtree):
    self._name = args.data_name
    self._type = args.type_name
    self._gtree = GroupedTree(dtree)

  def Write(self, out):
    # First pass: determine table sizes and locations.
    self.Pass(None, self._gtree)

    # Second pass: write the array.
    out.write('\nstatic const uint16_t {}Tree[] = {{\n'.format(self._name))
    end = self.Pass(out, self._gtree)
    out.write('};\n\n')

    # Write the description structure.
    out.write('static const {} {} = {{\n'
              .format(self._type, self._name))
    out.write('  {}, // maximum sequence length\n'.format(self._gtree.height))
    out.write('  {}, // tree array entries\n'.format(end))
    out.write('  {}Tree\n'.format(self._name))
    out.write('};\n\n')

  def Pass(self, out, gtree, location=0):
    gtree.location = location

    # Write tree header.
    if out:
      out.write('\n    // offset 0x{:04X}:\n'.format(location))
      if gtree.path:
        out.write('    //   prefix:\n')
        for key in gtree.path:
          out.write('    //     {}\n'.format(key.Pretty()))

    # Write tables.
    for k in gtree._key_kinds:
      for p in gtree._sub_parts:
        # Write table size.
        location += 1
        if out:
          out.write('    //   {} {} table\n'.format(p, k))
          out.write('    0x{:04X},          // number of entries\n'
                    .format(len(gtree._tables[k][p])))
        # Write table body.
        for key in sorted(gtree._tables[k][p]):
          location += 2
          if out:
            out.write('    0x{:04X},  // {}\n'
                      .format(key.CharacterCode(), key.ShorterUnicodeName()))
            result = gtree._tables[k][p][key]
            if p == 'internal':
              out.write('    0x{:04X},\n'.format(result.location))
            else:
              out.write('    0x{:04X},  // -> {}\n'
                        .format(result.CharacterCode(),
                                result.ShorterUnicodeName()))

    # Assemble subtrees of the current tree.
    for t in gtree.SubTrees():
      location = self.Pass(out, t, location)

    return location


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--type_name',
                      default='ui::TreeComposeChecker::CompositionData')
  parser.add_argument('--data_name', default='kCompositions')
  parser.add_argument('--output', default='character_composer_data.h')
  parser.add_argument('--guard', default=None)
  parser.add_argument('inputs', nargs='*')
  args = parser.parse_args(argv[1:])

  parse_tree = Parser(Lexer(Input(args.inputs))).Parse()

  with (sys.stdout if args.output == '-' else open(args.output, 'w')) as out:
    out.write('// Copyright 2015 The Chromium Authors\n')
    out.write('// Use of this source code is governed by a BSD-style license')
    out.write(' that can be\n// found in the LICENSE file.\n\n')
    out.write('// DO NOT EDIT.\n')
    out.write('//   GENERATED BY {}\n'.format(sys.argv[0]))
    out.write('//   FROM {}\n\n'.format(' '.join(args.inputs)))
    guard = args.guard if args.guard else args.output
    guard = ''.join([c.upper() if c.isalpha() else '_' for c in guard])
    guard = 'UI_BASE_IME_' + guard
    out.write('#ifndef {0}_\n#define {0}_\n'.format(guard))
    Assembler(args, parse_tree).Write(out)
    out.write('#endif  // {}_\n'.format(guard))

  return g_fail

if __name__ == '__main__':
  sys.exit(main(sys.argv))
