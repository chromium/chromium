# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logic for parsing a function signatures.

Much of this logic is duplicated at
tools/binary_size/libsupersize/caspian/function_signature.cc."""


def _FindParameterListParen(name):
  """Finds index of the "(" that denotes the start of a parameter list."""
  # This loops from left-to-right, but the only reason (I think) that this
  # is necessary (rather than reusing _FindLastCharOutsideOfBrackets), is
  # to capture the outer-most function in the case where classes are nested.
  start_idx = 0
  template_balance_count = 0
  paren_balance_count = 0
  while True:
    idx = name.find('(', start_idx)
    if idx == -1:
      return -1
    template_balance_count += (
        name.count('<', start_idx, idx) - name.count('>', start_idx, idx))
    # Special: operators with angle brackets.
    operator_idx = name.find('operator<', start_idx, idx)
    if operator_idx != -1:
      if name[operator_idx + 9] == '<':
        template_balance_count -= 2
      else:
        template_balance_count -= 1
    else:
      operator_idx = name.find('operator>', start_idx, idx)
      if operator_idx != -1:
        if name[operator_idx + 9] == '>':
          template_balance_count += 2
        else:
          template_balance_count += 1

    paren_balance_count += (
        name.count('(', start_idx, idx) - name.count(')', start_idx, idx))
    if template_balance_count == 0 and paren_balance_count == 0:
      # Special case: skip "(anonymous namespace)".
      if -1 != name.find('(anonymous namespace)', idx, idx + 21):
        start_idx = idx + 21
        continue
      # Special case: skip "decltype (...)"
      # Special case: skip "{lambda(PaintOp*)#63}"
      if name[idx - 1] != ' ' and name[idx - 7:idx] != '{lambda':
        return idx
    start_idx = idx + 1
    paren_balance_count += 1


def _FindLastCharOutsideOfBrackets(name, target_char, prev_idx=None):
  """Returns the last index of |target_char| that is not within ()s nor <>s."""
  paren_balance_count = 0
  template_balance_count = 0
  while True:
    idx = name.rfind(target_char, 0, prev_idx)
    if idx == -1:
      return -1
    # It is much faster to use.find() and.count() than to loop over each
    # character.
    template_balance_count += (
        name.count('<', idx, prev_idx) - name.count('>', idx, prev_idx))
    paren_balance_count += (
        name.count('(', idx, prev_idx) - name.count(')', idx, prev_idx))
    if template_balance_count == 0 and paren_balance_count == 0:
      return idx
    prev_idx = idx


def _FindReturnValueSpace(name, paren_idx):
  """Returns the index of the space that comes after the return type."""
  space_idx = paren_idx
  # Special case: const cast operators (see tests).
  if -1 != name.find(' const', paren_idx - 6, paren_idx):
    space_idx = paren_idx - 6
  while True:
    space_idx = _FindLastCharOutsideOfBrackets(name, ' ', space_idx)
    # Special cases: "operator new", "operator< <templ>", "operator<< <tmpl>".
    # No space is added for operator>><tmpl>.
    if -1 == space_idx:
      break

    if -1 != name.find('operator', space_idx - 8, space_idx):
      space_idx -= 8
    elif -1 != name.find('operator<', space_idx - 9, space_idx):
      space_idx -= 9
    elif -1 != name.find('operator<<', space_idx - 10, space_idx):
      space_idx -= 10
    else:
      break

  return space_idx


def _StripAbiTag(name):
  # Clang attribute. E.g.: std::allocator<Foo[6]>construct[abi:100]<Bar[7]>()
  start_idx = 0
  while True:
    start_idx = name.find('[abi:', start_idx, len(name) - 1)
    if start_idx == -1:
      break
    end_idx = name.find(']', start_idx + 5)
    if end_idx == -1:
      break
    name = name[:start_idx] + name[end_idx + 1:]
  return name


def _StripTemplateArgs(name):
  last_right_idx = None
  while True:
    last_right_idx = name.rfind('>', 0, last_right_idx)
    if last_right_idx == -1:
      return name
    left_idx = _FindLastCharOutsideOfBrackets(name, '<', last_right_idx + 1)
    if left_idx != -1:
      # Leave in empty <>s to denote that it's a template.
      name = name[:left_idx + 1] + name[last_right_idx:]
      last_right_idx = left_idx


def _NormalizeTopLevelGccLambda(name, left_paren_idx):
  # cc::{lambda(PaintOp*)#63}::_FUN() -> cc::$lambda#63()
  left_brace_idx = name.index('{')
  hash_idx = name.index('#', left_brace_idx + 1)
  right_brace_idx = name.index('}', hash_idx + 1)
  number = name[hash_idx + 1:right_brace_idx]
  return '{}$lambda#{}{}'.format(
      name[:left_brace_idx], number, name[left_paren_idx:])


def _NormalizeTopLevelClangLambda(name, left_paren_idx):
  # cc::$_21::__invoke() -> cc::$lambda#21()
  dollar_idx = name.index('$')
  colon_idx = name.index(':', dollar_idx + 1)
  number = name[dollar_idx + 2:colon_idx]
  return '{}$lambda#{}{}'.format(
      name[:dollar_idx], number, name[left_paren_idx:])


def ParseJava(full_name):
  """Breaks java full_name into parts.

  See unit tests for example signatures.

  Returns:
    A tuple of (full_name, template_name, name), where:
      * full_name = "class_with_package#member(args): type"
      * template_name = "class_with_package#member"
      * name = "class_without_package#member

    When a symbols has been merged into a different class:
      * full_name = "new_class#old_class.member(args): type"
      * template_name: Same as above, but uses old_class
      * name: Same as above, but uses old_class
  """
  hash_idx = full_name.find('#')
  if hash_idx != -1:
    # Parse an already parsed full_name.
    # Format: Class#symbol: type
    full_new_class_name = full_name[:hash_idx]
    colon_idx = full_name.find(':')
    if colon_idx == -1:
      member = full_name[hash_idx + 1:]
      member_type = ''
    else:
      member = full_name[hash_idx + 1:colon_idx]
      member_type = full_name[colon_idx:]
  else:
    parts = full_name.split(' ')
    full_new_class_name = parts[0]
    member = parts[-1] if len(parts) > 1 else None
    member_type = '' if len(parts) < 3 else ': ' + parts[1]

  if member is None:
    short_class_name = full_new_class_name.split('.')[-1]
    return full_name, full_name, short_class_name

  full_name = '{}#{}{}'.format(full_new_class_name, member, member_type)
  paren_idx = member.find('(')
  if paren_idx != -1:
    member = member[:paren_idx]

  # Class merging.
  full_old_class_name = full_new_class_name
  dot_idx = member.rfind('.')
  if dot_idx != -1:
    full_old_class_name = member[:dot_idx]
    member = member[dot_idx + 1:]

  short_class_name = full_old_class_name
  dot_idx = full_old_class_name.rfind('.')
  if dot_idx != -1:
    short_class_name = short_class_name[dot_idx + 1:]

  name = '{}#{}'.format(short_class_name, member)
  template_name = '{}#{}'.format(full_old_class_name, member)
  return full_name, template_name, name


def Parse(name):
  """Strips return type and breaks function signature into parts.

  See unit tests for example signatures.

  Returns:
    A tuple of:
    * name without return type (symbol.full_name),
    * full_name without params (symbol.template_name),
    * full_name without params and template args (symbol.name)
  """
  left_paren_idx = _FindParameterListParen(name)

  full_name = name
  if left_paren_idx > 0:
    right_paren_idx = name.rindex(')')
    assert right_paren_idx > left_paren_idx
    space_idx = _FindReturnValueSpace(name, left_paren_idx)
    name_no_params = name[space_idx + 1:left_paren_idx]

    # Special case for top-level lambdas.
    if name_no_params.endswith('}::_FUN'):
      # Don't use |name_no_params| in here since prior _idx will be off if
      # there was a return value.
      name = _NormalizeTopLevelGccLambda(name, left_paren_idx)
      return Parse(name)
    if name_no_params.endswith('::__invoke') and '$' in name_no_params:
      assert '$_' in name_no_params, 'Surprising lambda: ' + name
      name = _NormalizeTopLevelClangLambda(name, left_paren_idx)
      return Parse(name)

    full_name = name[space_idx + 1:]
    name = name_no_params + name[right_paren_idx + 1:]

  name = _StripAbiTag(name)
  template_name = name
  name = _StripTemplateArgs(name)
  return full_name, template_name, name


# An odd place for this, but pylint doesn't want it as a static in models
# (circular dependency), nor as an method on BaseSymbol
# (attribute-defined-outside-init).
def InternSameNames(symbol):
  """Allow using "is" to compare names (and should help with RAM)."""
  if symbol.template_name == symbol.full_name:
    symbol.template_name = symbol.full_name
  if symbol.name == symbol.template_name:
    symbol.name = symbol.template_name
