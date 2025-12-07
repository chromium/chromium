#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Parser for PPAPI IDL """

#
# IDL Parser
#
# The parser is uses the PLY yacc library to build a set of parsing rules based
# on WebIDL.
#
# WebIDL, and WebIDL regular expressions can be found at:
#   http://dev.w3.org/2006/webapi/WebIDL/
# PLY can be found at:
#   http://www.dabeaz.com/ply/
#
# The parser generates a tree by recursively matching sets of items against
# defined patterns.  When a match is made, that set of items is reduced
# to a new item.   The new item can provide a match for parent patterns.
# In this way an AST is built (reduced) depth first.


import getopt
import glob
import os.path
import re
import sys
import time

from idl_ast import IDLAst
from idl_log import ErrOut, InfoOut, WarnOut
from idl_lexer import IDLLexer
from idl_node import IDLAttribute, IDLFile, IDLNode
from idl_option import GetOption, Option, ParseOptions
from idl_lint import Lint

from ply import lex
from ply import yacc

Option('build_debug', 'Debug tree building.')
Option('parse_debug', 'Debug parse reduction steps.')
Option('token_debug', 'Debug token generation.')
Option('dump_tree', 'Dump the tree.')
Option('srcroot', 'Working directory.', default=os.path.join('..', 'api'))
Option('include_private', 'Include private IDL directory in default API paths.')

#
# ERROR_REMAP
#
# Maps the standard error formula into a more friendly error message.
#
ERROR_REMAP = {
  'Unexpected ")" after "(".' : 'Empty argument list.',
  'Unexpected ")" after ",".' : 'Missing argument.',
  'Unexpected "}" after ",".' : 'Trailing comma in block.',
  'Unexpected "}" after "{".' : 'Unexpected empty block.',
  'Unexpected comment after "}".' : 'Unexpected trailing comment.',
  'Unexpected "{" after keyword "enum".' : 'Enum missing name.',
  'Unexpected "{" after keyword "struct".' : 'Struct missing name.',
  'Unexpected "{" after keyword "interface".' : 'Interface missing name.',
}

# DumpReduction
#
# Prints out the set of items which matched a particular pattern and the
# new item or set it was reduced to.
def DumpReduction(cls, p):
  if p[0] is None:
    InfoOut.Log("OBJ: %s(%d) - None\n" % (cls, len(p)))
    InfoOut.Log("  [%s]\n" % [str(x) for x in p[1:]])
  else:
    out = ""
    for index in range(len(p) - 1):
      out += " >%s< " % str(p[index + 1])
    InfoOut.Log("OBJ: %s(%d) - %s : %s\n"  % (cls, len(p), str(p[0]), out))


# CopyToList
#
# Takes an input item, list, or None, and returns a new list of that set.
def CopyToList(item):
  # If the item is 'Empty' make it an empty list
  if not item: item = []

  # If the item is not a list
  if type(item) is not type([]): item = [item]

  # Make a copy we can modify
  return list(item)



# ListFromConcat
#
# Generate a new List by joining of two sets of inputs which can be an
# individual item, a list of items, or None.
def ListFromConcat(*items):
  itemsout = []
  for item in items:
    itemlist = CopyToList(item)
    itemsout.extend(itemlist)

  return itemsout


# TokenTypeName
#
# Generate a string which has the type and value of the token.
def TokenTypeName(t):
  if t.type == 'SYMBOL':  return 'symbol %s' % t.value
  if t.type in ['HEX', 'INT', 'OCT', 'FLOAT']:
    return 'value %s' % t.value
  if t.type == 'STRING' : return 'string "%s"' % t.value
  if t.type == 'COMMENT' : return 'comment'
  if t.type == t.value: return '"%s"' % t.value
  return 'keyword "%s"' % t.value


#
# IDL Parser
#
# The Parser inherits the from the Lexer to provide PLY with the tokenizing
# definitions.  Parsing patterns are encoded as function where p_<name> is
# is called any time a patern matching the function documentation is found.
# Paterns are expressed in the form of:
# """ <new item> : <item> ....
#                | <item> ...."""
#
# Where new item is the result of a match against one or more sets of items
# separated by the "|".
#
# The function is called with an object 'p' where p[0] is the output object
# and p[n] is the set of inputs for positive values of 'n'.  Len(p) can be
# used to distinguish between multiple item sets in the pattern.
#
# For more details on parsing refer to the PLY documentation at
#    http://www.dabeaz.com/ply/
#
#
# The parser uses the following conventions:
#   a <type>_block defines a block of <type> definitions in the form of:
#       [comment] [ext_attr_block] <type> <name> '{' <type>_list '}' ';'
#   A block is reduced by returning an object of <type> with a name of <name>
#   which in turn has <type>_list as children.
#
#   A [comment] is a optional C style comment block enclosed in /* ... */ which
#   is appended to the adjacent node as a child.
#
#   A [ext_attr_block] is an optional list of Extended Attributes which is
#   appended to the adjacent node as a child.
#
#   a <type>_list defines a list of <type> items which will be passed as a
#   list of children to the parent pattern.  A list is in the form of:
#       [comment] [ext_attr_block] <...DEF...> ';' <type>_list | (empty)
# or
#       [comment] [ext_attr_block] <...DEF...> <type>_cont
#
#   In the first form, the list is reduced recursively, where the right side
#   <type>_list is first reduced then joined with pattern currently being
#   matched.  The list is terminated with the (empty) pattern is matched.
#
#   In the second form the list is reduced recursively, where the right side
#   <type>_cont is first reduced then joined with the pattern currently being
#   matched.  The type_<cont> is in the form of:
#       ',' <type>_list | (empty)
#   The <type>_cont form is used to consume the ',' which only occurs when
#   there is more than one object in the list.  The <type>_cont also provides
#   the terminating (empty) definition.
#


class IDLParser(IDLLexer):
# TOP
#
# This pattern defines the top of the parse tree.  The parse tree is in the
# the form of:
#
# top
#   *modifiers
#     *comments
#     *ext_attr_block
#       ext_attr_list
#          attr_arg_list
#   *integer, value
#   *param_list
#   *typeref
#
#   top_list
#     describe_block
#       describe_list
#     enum_block
#       enum_item
#     interface_block
#       member
#     label_block
#       label_item
#     struct_block
#       member
#     typedef_decl
#       typedef_data
#       typedef_func
#
# (* sub matches found at multiple levels and are not truly children of top)
#
# We force all input files to start with two comments.  The first comment is a
# Copyright notice followed by a set of file wide Extended Attributes, followed
# by the file comment and finally by file level patterns.
#
  # Find the Copyright, File comment, and optional file wide attributes.  We
  # use a match with COMMENT instead of comments to force the token to be
  # present.  The extended attributes and the top_list become siblings which
  # in turn are children of the file object created from the results of top.
  def p_top(self, p):
    """top : COMMENT COMMENT ext_attr_block top_list"""

    Copyright = self.BuildComment('Copyright', p, 1)
    Filedoc = self.BuildComment('Comment', p, 2)

    p[0] = ListFromConcat(Copyright, Filedoc, p[3], p[4])
    if self.parse_debug: DumpReduction('top', p)

  def p_top_short(self, p):
    """top : COMMENT ext_attr_block top_list"""
    Copyright = self.BuildComment('Copyright', p, 1)
    Filedoc = IDLNode('Comment', self.lexobj.filename, p.lineno(2)-1,
        p.lexpos(2)-1, [self.BuildAttribute('NAME', ''),
          self.BuildAttribute('FORM', 'cc')])
    p[0] = ListFromConcat(Copyright, Filedoc, p[2], p[3])
    if self.parse_debug: DumpReduction('top', p)

  # Build a list of top level items.
  def p_top_list(self, p):
    """top_list : callback_decl top_list
                | describe_block top_list
                | dictionary_block top_list
                | enum_block top_list
                | inline top_list
                | interface_block top_list
                | label_block top_list
                | namespace top_list
                | struct_block top_list
                | typedef_decl top_list
                | bad_decl top_list
                | """
    if len(p) > 2:
      p[0] = ListFromConcat(p[1], p[2])
    if self.parse_debug: DumpReduction('top_list', p)

  # Recover from error and continue parsing at the next top match.
  def p_top_error(self, p):
    """top_list : error top_list"""
    p[0] = p[2]

  # Recover from error and continue parsing at the next top match.
  def p_bad_decl(self, p):
    """bad_decl : modifiers SYMBOL error '}' ';'"""
    p[0] = []

#
# Modifier List
#
#
  def p_modifiers(self, p):
    """modifiers : comments ext_attr_block"""
    p[0] = ListFromConcat(p[1], p[2])
    if self.parse_debug: DumpReduction('modifiers', p)

#
# Scoped name is a name with an optional scope.
#
# Used for types and namespace names. eg. foo_bar.hello_world, or
# foo_bar.hello_world.SomeType.
#
  def p_scoped_name(self, p):
    """scoped_name : SYMBOL scoped_name_rest"""
    p[0] = ''.join(p[1:])
    if self.parse_debug: DumpReduction('scoped_name', p)

  def p_scoped_name_rest(self, p):
    """scoped_name_rest : '.' scoped_name
                        |"""
    p[0] = ''.join(p[1:])
    if self.parse_debug: DumpReduction('scoped_name_rest', p)

#
# Type reference
#
#
  def p_typeref(self, p):
    """typeref : scoped_name"""
    p[0] = p[1]
    if self.parse_debug: DumpReduction('typeref', p)


#
# Comments
#
# Comments are optional list of C style comment objects.  Comments are returned
# as a list or None.
#
  def p_comments(self, p):
    """comments : COMMENT comments
                | """
    if len(p) > 1:
      child = self.BuildComment('Comment', p, 1)
      p[0] = ListFromConcat(child, p[2])
      if self.parse_debug: DumpReduction('comments', p)
    else:
      if self.parse_debug: DumpReduction('no comments', p)


#
# Namespace
#
# A namespace provides a named scope to an enclosed top_list.
#
  def p_namespace(self, p):
    """namespace : modifiers NAMESPACE namespace_name '{' top_list '}' ';'"""
    children = ListFromConcat(p[1], p[5])
    p[0] = self.BuildNamed('Namespace', p, 3, children)

  # We allow namespace names of the form foo.bar.baz.
  def p_namespace_name(self, p):
    """namespace_name : scoped_name"""
    p[0] = p[1]


#
# Dictionary
#
# A dictionary is a named list of optional and required members.
#
  def p_dictionary_block(self, p):
    """dictionary_block : modifiers DICTIONARY SYMBOL '{' struct_list '}' ';'"""
    p[0] = self.BuildNamed('Dictionary', p, 3, ListFromConcat(p[1], p[5]))

  def p_dictionary_errorA(self, p):
    """dictionary_block : modifiers DICTIONARY error ';'"""
    p[0] = []

  def p_dictionary_errorB(self, p):
    """dictionary_block : modifiers DICTIONARY error '{' struct_list '}' ';'"""
    p[0] = []

#
# Callback
#
# A callback is essentially a single function declaration (outside of an
# Interface).
#
  def p_callback_decl(self, p):
    """callback_decl : modifiers CALLBACK SYMBOL '=' SYMBOL param_list ';'"""
    children = ListFromConcat(p[1], p[6])
    p[0] = self.BuildNamed('Callback', p, 3, children)


#
# Inline
#
# Inline blocks define option code to be emitted based on language tag,
# in the form of:
# #inline <LANGUAGE>
# <CODE>
# #endinl
#
  def p_inline(self, p):
    """inline : modifiers INLINE"""
    words = p[2].split()
    name = self.BuildAttribute('NAME', words[1])
    lines = p[2].split('\n')
    value = self.BuildAttribute('VALUE', '\n'.join(lines[1:-1]) + '\n')
    children = ListFromConcat(name, value, p[1])
    p[0] = self.BuildProduction('Inline', p, 2, children)
    if self.parse_debug: DumpReduction('inline', p)

# Extended Attributes
#
# Extended Attributes denote properties which will be applied to a node in the
# AST.  A list of extended attributes are denoted by a brackets '[' ... ']'
# enclosing a comma separated list of extended attributes in the form of:
#
#  Name
#  Name=HEX | INT | OCT | FLOAT
#  Name="STRING"
#  Name=Function(arg ...)
#  TODO(bradnelson) -Not currently supported:
#  ** Name(arg ...) ...
#  ** Name=Scope::Value
#
# Extended Attributes are returned as a list or None.

  def p_ext_attr_block(self, p):
    """ext_attr_block : '[' ext_attr_list ']'
                  | """
    if len(p) > 1:
      p[0] = p[2]
      if self.parse_debug: DumpReduction('ext_attr_block', p)
    else:
      if self.parse_debug: DumpReduction('no ext_attr_block', p)

  def p_ext_attr_list(self, p):
    """ext_attr_list : SYMBOL '=' SYMBOL ext_attr_cont
                     | SYMBOL '=' value ext_attr_cont
                     | SYMBOL '=' SYMBOL param_list ext_attr_cont
                     | SYMBOL ext_attr_cont"""
    # If there are 4 tokens plus a return slot, this must be in the form
    # SYMBOL = SYMBOL|value ext_attr_cont
    if len(p) == 5:
      p[0] = ListFromConcat(self.BuildAttribute(p[1], p[3]), p[4])
    # If there are 5 tokens plus a return slot, this must be in the form
    # SYMBOL = SYMBOL (param_list) ext_attr_cont
    elif len(p) == 6:
      member = self.BuildNamed('Member', p, 3, [p[4]])
      p[0] = ListFromConcat(self.BuildAttribute(p[1], member), p[5])
    # Otherwise, this must be: SYMBOL ext_attr_cont
    else:
      p[0] = ListFromConcat(self.BuildAttribute(p[1], 'True'), p[2])
    if self.parse_debug: DumpReduction('ext_attribute_list', p)

  def p_ext_attr_list_values(self, p):
    """ext_attr_list : SYMBOL '=' '(' values ')' ext_attr_cont
                     | SYMBOL '=' '(' symbols ')' ext_attr_cont"""
    p[0] = ListFromConcat(self.BuildAttribute(p[1], p[4]), p[6])

  def p_values(self, p):
    """values : value values_cont"""
    p[0] = ListFromConcat(p[1], p[2])

  def p_symbols(self, p):
    """symbols : SYMBOL symbols_cont"""
    p[0] = ListFromConcat(p[1], p[2])

  def p_symbols_cont(self, p):
    """symbols_cont : ',' SYMBOL symbols_cont
                    | """
    if len(p) > 1: p[0] = ListFromConcat(p[2], p[3])

  def p_values_cont(self, p):
    """values_cont : ',' value values_cont
                   | """
    if len(p) > 1: p[0] = ListFromConcat(p[2], p[3])

  def p_ext_attr_cont(self, p):
    """ext_attr_cont : ',' ext_attr_list
                     |"""
    if len(p) > 1: p[0] = p[2]
    if self.parse_debug: DumpReduction('ext_attribute_cont', p)

  def p_ext_attr_func(self, p):
    """ext_attr_list : SYMBOL '(' attr_arg_list ')' ext_attr_cont"""
    p[0] = ListFromConcat(self.BuildAttribute(p[1] + '()', p[3]), p[5])
    if self.parse_debug: DumpReduction('attr_arg_func', p)

  def p_ext_attr_arg_list(self, p):
    """attr_arg_list : SYMBOL attr_arg_cont
                     | value attr_arg_cont"""
    p[0] = ListFromConcat(p[1], p[2])

  def p_attr_arg_cont(self, p):
    """attr_arg_cont : ',' attr_arg_list
                     | """
    if self.parse_debug: DumpReduction('attr_arg_cont', p)
    if len(p) > 1: p[0] = p[2]

  def p_attr_arg_error(self, p):
    """attr_arg_cont : error attr_arg_cont"""
    p[0] = p[2]
    if self.parse_debug: DumpReduction('attr_arg_error', p)


#
# Describe
#
# A describe block is defined at the top level.  It provides a mechanism for
# attributing a group of ext_attr to a describe_list.  Members of the
# describe list are language specific 'Type' declarations
#
  def p_describe_block(self, p):
    """describe_block : modifiers DESCRIBE '{' describe_list '}' ';'"""
    children = ListFromConcat(p[1], p[4])
    p[0] = self.BuildProduction('Describe', p, 2, children)
    if self.parse_debug: DumpReduction('describe_block', p)

  # Recover from describe error and continue parsing at the next top match.
  def p_describe_error(self, p):
    """describe_list : error describe_list"""
    p[0] = []

  def p_describe_list(self, p):
    """describe_list : modifiers SYMBOL ';' describe_list
                     | modifiers ENUM ';' describe_list
                     | modifiers STRUCT ';' describe_list
                     | modifiers TYPEDEF ';' describe_list
                     | """
    if len(p) > 1:
      Type = self.BuildNamed('Type', p, 2, p[1])
      p[0] = ListFromConcat(Type, p[4])

#
# Constant Values (integer, value)
#
# Constant values can be found at various levels.  A Constant value is returns
# as the string value after validated against a FLOAT, HEX, INT, OCT or
# STRING pattern as appropriate.
#
  def p_value(self, p):
    """value : FLOAT
             | HEX
             | INT
             | OCT
             | STRING"""
    p[0] = p[1]
    if self.parse_debug: DumpReduction('value', p)

  def p_value_lshift(self, p):
    """value : integer LSHIFT INT"""
    p[0] = "%s << %s" % (p[1], p[3])
    if self.parse_debug: DumpReduction('value', p)

# Integers are numbers which may not be floats used in cases like array sizes.
  def p_integer(self, p):
    """integer : HEX
               | INT
               | OCT"""
    p[0] = p[1]
    if self.parse_debug: DumpReduction('integer', p)

#
# Expression
#
# A simple arithmetic expression.
#
  precedence = (
    ('left','|','&','^'),
    ('left','LSHIFT','RSHIFT'),
    ('left','+','-'),
    ('left','*','/'),
    ('right','UMINUS','~'),
    )

  def p_expression_binop(self, p):
    """expression : expression LSHIFT expression
                  | expression RSHIFT expression
                  | expression '|' expression
                  | expression '&' expression
                  | expression '^' expression
                  | expression '+' expression
                  | expression '-' expression
                  | expression '*' expression
                  | expression '/' expression"""
    p[0] = "%s %s %s" % (str(p[1]), str(p[2]), str(p[3]))
    if self.parse_debug: DumpReduction('expression_binop', p)

  def p_expression_unop(self, p):
    """expression : '-' expression %prec UMINUS
                  | '~' expression %prec '~'"""
    p[0] = "%s%s" % (str(p[1]), str(p[2]))
    if self.parse_debug: DumpReduction('expression_unop', p)

  def p_expression_term(self, p):
    """expression : '(' expression ')'"""
    p[0] = "%s%s%s" % (str(p[1]), str(p[2]), str(p[3]))
    if self.parse_debug: DumpReduction('expression_term', p)

  def p_expression_symbol(self, p):
    """expression : SYMBOL"""
    p[0] = p[1]
    if self.parse_debug: DumpReduction('expression_symbol', p)

  def p_expression_integer(self, p):
    """expression : integer"""
    p[0] = p[1]
    if self.parse_debug: DumpReduction('expression_integer', p)

#
# Array List
#
# Defined a list of array sizes (if any).
#
  def p_arrays(self, p):
    """arrays : '[' ']' arrays
              | '[' integer ']' arrays
              | """
    # If there are 3 tokens plus a return slot it is an unsized array
    if len(p) == 4:
      array = self.BuildProduction('Array', p, 1)
      p[0] = ListFromConcat(array, p[3])
    # If there are 4 tokens plus a return slot it is a fixed array
    elif len(p) == 5:
      count = self.BuildAttribute('FIXED', p[2])
      array = self.BuildProduction('Array', p, 2, [count])
      p[0] = ListFromConcat(array, p[4])
    # If there is only a return slot, do not fill it for this terminator.
    elif len(p) == 1: return
    if self.parse_debug: DumpReduction('arrays', p)


# An identifier is a legal value for a parameter or attribute name. Lots of
# existing IDL files use "callback" as a parameter/attribute name, so we allow
# a SYMBOL or the CALLBACK keyword.
  def p_identifier(self, p):
    """identifier : SYMBOL
                  | CALLBACK"""
    p[0] = p[1]
    # Save the line number of the underlying token (otherwise it gets
    # discarded), since we use it in the productions with an identifier in
    # them.
    p.set_lineno(0, p.lineno(1))


#
# Union
#
# A union allows multiple choices of types for a parameter or member.
#

  def p_union_option(self, p):
    """union_option : modifiers SYMBOL arrays"""
    typeref = self.BuildAttribute('TYPEREF', p[2])
    children = ListFromConcat(p[1], typeref, p[3])
    p[0] = self.BuildProduction('Option', p, 2, children)

  def p_union_list(self, p):
    """union_list : union_option OR union_list
                  | union_option"""
    if len(p) > 2:
      p[0] = ListFromConcat(p[1], p[3])
    else:
      p[0] = p[1]

#
# Parameter List
#
# A parameter list is a collection of arguments which are passed to a
# function.
#
  def p_param_list(self, p):
    """param_list : '(' param_item param_cont ')'
                  | '(' ')' """
    if len(p) > 3:
      args = ListFromConcat(p[2], p[3])
    else:
      args = []
    p[0] = self.BuildProduction('Callspec', p, 1, args)
    if self.parse_debug: DumpReduction('param_list', p)

  def p_param_item(self, p):
    """param_item : modifiers optional typeref arrays identifier"""
    typeref = self.BuildAttribute('TYPEREF', p[3])
    children = ListFromConcat(p[1], p[2], typeref, p[4])
    p[0] = self.BuildNamed('Param', p, 5, children)
    if self.parse_debug: DumpReduction('param_item', p)

  def p_param_item_union(self, p):
    """param_item : modifiers optional '(' union_list ')' identifier"""
    union = self.BuildAttribute('Union', True)
    children = ListFromConcat(p[1], p[2], p[4], union)
    p[0] = self.BuildNamed('Param', p, 6, children)
    if self.parse_debug: DumpReduction('param_item', p)

  def p_optional(self, p):
    """optional : OPTIONAL
                | """
    if len(p) == 2:
      p[0] = self.BuildAttribute('OPTIONAL', True)


  def p_param_cont(self, p):
    """param_cont : ',' param_item param_cont
                  | """
    if len(p) > 1:
      p[0] = ListFromConcat(p[2], p[3])
      if self.parse_debug: DumpReduction('param_cont', p)

  def p_param_error(self, p):
    """param_cont : error param_cont"""
    p[0] = p[2]


#
# Typedef
#
# A typedef creates a new referencable type.  The typedef can specify an array
# definition as well as a function declaration.
#
  def p_typedef_data(self, p):
    """typedef_decl : modifiers TYPEDEF SYMBOL SYMBOL ';' """
    typeref = self.BuildAttribute('TYPEREF', p[3])
    children = ListFromConcat(p[1], typeref)
    p[0] = self.BuildNamed('Typedef', p, 4, children)
    if self.parse_debug: DumpReduction('typedef_data', p)

  def p_typedef_array(self, p):
    """typedef_decl : modifiers TYPEDEF SYMBOL arrays SYMBOL ';' """
    typeref = self.BuildAttribute('TYPEREF', p[3])
    children = ListFromConcat(p[1], typeref, p[4])
    p[0] = self.BuildNamed('Typedef', p, 5, children)
    if self.parse_debug: DumpReduction('typedef_array', p)

  def p_typedef_func(self, p):
    """typedef_decl : modifiers TYPEDEF SYMBOL SYMBOL param_list ';' """
    typeref = self.BuildAttribute('TYPEREF', p[3])
    children = ListFromConcat(p[1], typeref, p[5])
    p[0] = self.BuildNamed('Typedef', p, 4, children)
    if self.parse_debug: DumpReduction('typedef_func', p)

#
# Enumeration
#
# An enumeration is a set of named integer constants.  An enumeration
# is valid type which can be referenced in other definitions.
#
  def p_enum_block(self, p):
    """enum_block : modifiers ENUM SYMBOL '{' enum_list '}' ';'"""
    p[0] = self.BuildNamed('Enum', p, 3, ListFromConcat(p[1], p[5]))
    if self.parse_debug: DumpReduction('enum_block', p)

  # Recover from enum error and continue parsing at the next top match.
  def p_enum_errorA(self, p):
    """enum_block : modifiers ENUM error '{' enum_list '}' ';'"""
    p[0] = []

  def p_enum_errorB(self, p):
    """enum_block : modifiers ENUM error ';'"""
    p[0] = []

  def p_enum_list(self, p):
    """enum_list : modifiers SYMBOL '=' expression enum_cont
                 | modifiers SYMBOL enum_cont"""
    if len(p) > 4:
      val  = self.BuildAttribute('VALUE', p[4])
      enum = self.BuildNamed('EnumItem', p, 2, ListFromConcat(val, p[1]))
      p[0] = ListFromConcat(enum, p[5])
    else:
      enum = self.BuildNamed('EnumItem', p, 2, p[1])
      p[0] = ListFromConcat(enum, p[3])
    if self.parse_debug: DumpReduction('enum_list', p)

  def p_enum_cont(self, p):
    """enum_cont : ',' enum_list
                 |"""
    if len(p) > 1: p[0] = p[2]
    if self.parse_debug: DumpReduction('enum_cont', p)

  def p_enum_cont_error(self, p):
    """enum_cont : error enum_cont"""
    p[0] = p[2]
    if self.parse_debug: DumpReduction('enum_error', p)


#
# Label
#
# A label is a special kind of enumeration which allows us to go from a
# set of labels
#
  def p_label_block(self, p):
    """label_block : modifiers LABEL SYMBOL '{' label_list '}' ';'"""
    p[0] = self.BuildNamed('Label', p, 3, ListFromConcat(p[1], p[5]))
    if self.parse_debug: DumpReduction('label_block', p)

  def p_label_list(self, p):
    """label_list : modifiers SYMBOL '=' FLOAT label_cont"""
    val  = self.BuildAttribute('VALUE', p[4])
    label = self.BuildNamed('LabelItem', p, 2, ListFromConcat(val, p[1]))
    p[0] = ListFromConcat(label, p[5])
    if self.parse_debug: DumpReduction('label_list', p)

  def p_label_cont(self, p):
    """label_cont : ',' label_list
                 |"""
    if len(p) > 1: p[0] = p[2]
    if self.parse_debug: DumpReduction('label_cont', p)

  def p_label_cont_error(self, p):
    """label_cont : error label_cont"""
    p[0] = p[2]
    if self.parse_debug: DumpReduction('label_error', p)


#
# Members
#
# A member attribute or function of a struct or interface.
#
  def p_member_attribute(self, p):
    """member_attribute : modifiers typeref arrays questionmark identifier"""
    typeref = self.BuildAttribute('TYPEREF', p[2])
    children = ListFromConcat(p[1], typeref, p[3], p[4])
    p[0] = self.BuildNamed('Member', p, 5, children)
    if self.parse_debug: DumpReduction('attribute', p)

  def p_member_attribute_union(self, p):
    """member_attribute : modifiers '(' union_list ')' questionmark identifier"""
    union = self.BuildAttribute('Union', True)
    children = ListFromConcat(p[1], p[3], p[5], union)
    p[0] = self.BuildNamed('Member', p, 6, children)
    if self.parse_debug: DumpReduction('attribute', p)

  def p_member_function(self, p):
    """member_function : modifiers static typeref arrays SYMBOL param_list"""
    typeref = self.BuildAttribute('TYPEREF', p[3])
    children = ListFromConcat(p[1], p[2], typeref, p[4], p[6])
    p[0] = self.BuildNamed('Member', p, 5, children)
    if self.parse_debug: DumpReduction('function', p)

  def p_static(self, p):
    """static : STATIC
              | """
    if len(p) == 2:
      p[0] = self.BuildAttribute('STATIC', True)

  def p_questionmark(self, p):
    """questionmark : '?'
                    | """
    if len(p) == 2:
      p[0] = self.BuildAttribute('OPTIONAL', True)

#
# Interface
#
# An interface is a named collection of functions.
#
  def p_interface_block(self, p):
    """interface_block : modifiers INTERFACE SYMBOL '{' interface_list '}' ';'"""
    p[0] = self.BuildNamed('Interface', p, 3, ListFromConcat(p[1], p[5]))
    if self.parse_debug: DumpReduction('interface_block', p)

  def p_interface_error(self, p):
    """interface_block : modifiers INTERFACE error '{' interface_list '}' ';'"""
    p[0] = []

  def p_interface_list(self, p):
    """interface_list : member_function ';' interface_list
                      | """
    if len(p) > 1 :
      p[0] = ListFromConcat(p[1], p[3])
      if self.parse_debug: DumpReduction('interface_list', p)


#
# Struct
#
# A struct is a named collection of members which in turn reference other
# types.  The struct is a referencable type.
#
  def p_struct_block(self, p):
    """struct_block : modifiers STRUCT SYMBOL '{' struct_list '}' ';'"""
    children = ListFromConcat(p[1], p[5])
    p[0] = self.BuildNamed('Struct', p, 3, children)
    if self.parse_debug: DumpReduction('struct_block', p)

  # Recover from struct error and continue parsing at the next top match.
  def p_struct_error(self, p):
    """enum_block : modifiers STRUCT error '{' struct_list '}' ';'"""
    p[0] = []

  def p_struct_list(self, p):
    """struct_list : member_attribute ';' struct_list
                   | member_function ';' struct_list
                   |"""
    if len(p) > 1: p[0] = ListFromConcat(p[1], p[3])


#
# Parser Errors
#
# p_error is called whenever the parser can not find a pattern match for
# a set of items from the current state.  The p_error function defined here
# is triggered logging an error, and parsing recover happens as the
# p_<type>_error functions defined above are called.  This allows the parser
# to continue so as to capture more than one error per file.
#
  def p_error(self, t):
    filename = self.lexobj.filename
    self.parse_errors += 1
    if t:
      lineno = t.lineno
      pos = t.lexpos
      prev = self.yaccobj.symstack[-1]
      if type(prev) == lex.LexToken:
        msg = "Unexpected %s after %s." % (
            TokenTypeName(t), TokenTypeName(prev))
      else:
        msg = "Unexpected %s." % (t.value)
    else:
      lineno = self.last.lineno
      pos = self.last.lexpos
      msg = "Unexpected end of file after %s." % TokenTypeName(self.last)
      self.yaccobj.restart()

    # Attempt to remap the error to a friendlier form
    if msg in ERROR_REMAP:
      msg = ERROR_REMAP[msg]

    # Log the error
    ErrOut.LogLine(filename, lineno, pos, msg)

  def Warn(self, node, msg):
    WarnOut.LogLine(node.filename, node.lineno, node.pos, msg)
    self.parse_warnings += 1

  def __init__(self):
    IDLLexer.__init__(self)
    self.yaccobj = yacc.yacc(module=self, tabmodule=None, debug=False,
                             optimize=0, write_tables=0)

    self.build_debug = GetOption('build_debug')
    self.parse_debug = GetOption('parse_debug')
    self.token_debug = GetOption('token_debug')
    self.verbose = GetOption('verbose')
    self.parse_errors = 0

#
# Tokenizer
#
# The token function returns the next token provided by IDLLexer for matching
# against the leaf paterns.
#
  def token(self):
    tok = self.lexobj.token()
    if tok:
      self.last = tok
      if self.token_debug:
        InfoOut.Log("TOKEN %s(%s)" % (tok.type, tok.value))
    return tok

#
# BuildProduction
#
# Production is the set of items sent to a grammar rule resulting in a new
# item being returned.
#
# p - Is the Yacc production object containing the stack of items
# index - Index into the production of the name for the item being produced.
# cls - The type of item being producted
# childlist - The children of the new item
  def BuildProduction(self, cls, p, index, childlist=None):
    if not childlist: childlist = []
    filename = self.lexobj.filename
    lineno = p.lineno(index)
    pos = p.lexpos(index)
    out = IDLNode(cls, filename, lineno, pos, childlist)
    if self.build_debug:
      InfoOut.Log("Building %s" % out)
    return out

  def BuildNamed(self, cls, p, index, childlist=None):
    if not childlist: childlist = []
    childlist.append(self.BuildAttribute('NAME', p[index]))
    return self.BuildProduction(cls, p, index, childlist)

  def BuildComment(self, cls, p, index):
    name = p[index]

    # Remove comment markers
    lines = []
    if name[:2] == '//':
      # For C++ style, remove any leading whitespace and the '//' marker from
      # each line.
      form = 'cc'
      for line in name.split('\n'):
        start = line.find('//')
        lines.append(line[start+2:])
    else:
      # For C style, remove ending '*/''
      form = 'c'
      for line in name[:-2].split('\n'):
        # Remove characters until start marker for this line '*' if found
        # otherwise it should be blank.
        offs = line.find('*')
        if offs >= 0:
          line = line[offs + 1:].rstrip()
        else:
          line = ''
        lines.append(line)
    name = '\n'.join(lines)

    childlist = [self.BuildAttribute('NAME', name),
                 self.BuildAttribute('FORM', form)]
    return self.BuildProduction(cls, p, index, childlist)

#
# BuildAttribute
#
# An ExtendedAttribute is a special production that results in a property
# which is applied to the adjacent item.  Attributes have no children and
# instead represent key/value pairs.
#
  def BuildAttribute(self, key, val):
    return IDLAttribute(key, val)


#
# ParseData
#
# Attempts to parse the current data loaded in the lexer.
#
  def ParseData(self, data, filename='<Internal>'):
    self.SetData(filename, data)
    try:
      self.parse_errors = 0
      self.parse_warnings = 0
      return self.yaccobj.parse(lexer=self)

    except lex.LexError as le:
      ErrOut.Log(str(le))
      return []

#
# ParseFile
#
# Loads a new file into the lexer and attemps to parse it.
#
  def ParseFile(self, filename):
    date = time.ctime(os.path.getmtime(filename))
    data = open(filename).read()
    if self.verbose:
      InfoOut.Log("Parsing %s" % filename)
    try:
      out = self.ParseData(data, filename)

      # If we have a src root specified, remove it from the path
      srcroot = GetOption('srcroot')
      if srcroot and filename.find(srcroot) == 0:
        filename = filename[len(srcroot) + 1:]
      filenode = IDLFile(filename, out, self.parse_errors + self.lex_errors)
      filenode.SetProperty('DATETIME', date)
      return filenode

    except Exception as e:
      ErrOut.LogLine(filename, self.last.lineno, self.last.lexpos,
                     'Internal parsing error - %s.' % str(e))
      raise



#
# Flatten Tree
#
# Flattens the tree of IDLNodes for use in testing.
#
def FlattenTree(node):
  add_self = False
  out = []
  for child in node.GetChildren():
    if child.IsA('Comment'):
      add_self = True
    else:
      out.extend(FlattenTree(child))

  if add_self:
    out = [str(node)] + out
  return out


def TestErrors(filename, filenode):
  nodelist = filenode.GetChildren()

  lexer = IDLLexer()
  data = open(filename).read()
  lexer.SetData(filename, data)

  pass_comments = []
  fail_comments = []
  while True:
    tok = lexer.lexobj.token()
    if tok == None: break
    if tok.type == 'COMMENT':
      args = tok.value[3:-3].split()
      if args[0] == 'OK':
        pass_comments.append((tok.lineno, ' '.join(args[1:])))
      else:
        if args[0] == 'FAIL':
          fail_comments.append((tok.lineno, ' '.join(args[1:])))
  obj_list = []
  for node in nodelist:
    obj_list.extend(FlattenTree(node))

  errors = 0

  #
  # Check for expected successes
  #
  obj_cnt = len(obj_list)
  pass_cnt = len(pass_comments)
  if obj_cnt != pass_cnt:
    InfoOut.Log("Mismatched pass (%d) vs. nodes built (%d)."
        % (pass_cnt, obj_cnt))
    InfoOut.Log("PASS: %s" % [x[1] for x in pass_comments])
    InfoOut.Log("OBJS: %s" % obj_list)
    errors += 1
    if pass_cnt > obj_cnt: pass_cnt = obj_cnt

  for i in range(pass_cnt):
    line, comment = pass_comments[i]
    if obj_list[i] != comment:
      ErrOut.LogLine(filename, line, None, "OBJ %s : EXPECTED %s\n" %
                     (obj_list[i], comment))
      errors += 1

  #
  # Check for expected errors
  #
  err_list = ErrOut.DrainLog()
  err_cnt = len(err_list)
  fail_cnt = len(fail_comments)
  if err_cnt != fail_cnt:
    InfoOut.Log("Mismatched fail (%d) vs. errors seen (%d)."
        % (fail_cnt, err_cnt))
    InfoOut.Log("FAIL: %s" % [x[1] for x in fail_comments])
    InfoOut.Log("ERRS: %s" % err_list)
    errors += 1
    if fail_cnt > err_cnt:  fail_cnt = err_cnt

  for i in range(fail_cnt):
    line, comment = fail_comments[i]
    err = err_list[i].strip()

    if err_list[i] != comment:
      ErrOut.Log("%s(%d) Error\n\tERROR : %s\n\tEXPECT: %s" % (
        filename, line, err_list[i], comment))
      errors += 1

  # Clear the error list for the next run
  err_list = []
  return errors


def TestFile(parser, filename):
  # Capture errors instead of reporting them so we can compare them
  # with the expected errors.
  ErrOut.SetConsole(False)
  ErrOut.SetCapture(True)

  filenode = parser.ParseFile(filename)

  # Renable output
  ErrOut.SetConsole(True)
  ErrOut.SetCapture(False)

  # Compare captured errors
  return TestErrors(filename, filenode)


def TestErrorFiles(filter):
  idldir = os.path.split(sys.argv[0])[0]
  idldir = os.path.join(idldir, 'test_parser', '*.idl')
  filenames = glob.glob(idldir)
  parser = IDLParser()
  total_errs = 0
  for filename in filenames:
    if filter and filename not in filter: continue
    errs = TestFile(parser, filename)
    if errs:
      ErrOut.Log("%s test failed with %d error(s)." % (filename, errs))
      total_errs += errs

  if total_errs:
    ErrOut.Log("Failed parsing test.")
  else:
    InfoOut.Log("Passed parsing test.")
  return total_errs


def TestNamespaceFiles(filter):
  idldir = os.path.split(sys.argv[0])[0]
  idldir = os.path.join(idldir, 'test_namespace', '*.idl')
  filenames = glob.glob(idldir)
  testnames = []

  for filename in filenames:
    if filter and filename not in filter: continue
    testnames.append(filename)

  # If we have no files to test, then skip this test
  if not testnames:
    InfoOut.Log('No files to test for namespace.')
    return 0

  InfoOut.SetConsole(False)
  ast = ParseFiles(testnames)
  InfoOut.SetConsole(True)

  errs = ast.GetProperty('ERRORS')
  if errs:
    ErrOut.Log("Failed namespace test.")
  else:
    InfoOut.Log("Passed namespace test.")
  return errs



def FindVersionError(releases, node):
  err_cnt = 0
  if node.IsA('Interface', 'Struct'):
    comment_list = []
    comment = node.GetOneOf('Comment')
    if comment and comment.GetName()[:4] == 'REL:':
      comment_list = comment.GetName()[5:].strip().split(' ')

    first_list = [node.first_release[rel] for rel in releases]
    first_list = sorted(set(first_list))
    if first_list != comment_list:
      node.Error("Mismatch in releases: %s vs %s." % (
          comment_list, first_list))
      err_cnt += 1

  for child in node.GetChildren():
    err_cnt += FindVersionError(releases, child)
  return err_cnt


def TestVersionFiles(filter):
  idldir = os.path.split(sys.argv[0])[0]
  idldir = os.path.join(idldir, 'test_version', '*.idl')
  filenames = glob.glob(idldir)
  testnames = []

  for filename in filenames:
    if filter and filename not in filter: continue
    testnames.append(filename)

  # If we have no files to test, then skip this test
  if not testnames:
    InfoOut.Log('No files to test for version.')
    return 0

  ast = ParseFiles(testnames)
  errs = FindVersionError(ast.releases, ast)
  errs += ast.errors

  if errs:
    ErrOut.Log("Failed version test.")
  else:
    InfoOut.Log("Passed version test.")
  return errs


default_dirs = ['.', 'trusted', 'dev', 'private']
def ParseFiles(filenames):
  parser = IDLParser()
  filenodes = []

  if not filenames:
    filenames = []
    srcroot = GetOption('srcroot')
    dirs = default_dirs
    if GetOption('include_private'):
      dirs += ['private']
    for dirname in dirs:
      srcdir = os.path.join(srcroot, dirname, '*.idl')
      srcdir = os.path.normpath(srcdir)
      filenames += sorted(glob.glob(srcdir))

  if not filenames:
    ErrOut.Log('No sources provided.')

  for filename in filenames:
    filenode = parser.ParseFile(filename)
    filenodes.append(filenode)

  ast = IDLAst(filenodes)
  if GetOption('dump_tree'): ast.Dump(0)

  Lint(ast)
  return ast


def Main(args):
  filenames = ParseOptions(args)

  # If testing...
  if GetOption('test'):
    errs = TestErrorFiles(filenames)
    errs = TestNamespaceFiles(filenames)
    errs = TestVersionFiles(filenames)
    if errs:
      ErrOut.Log("Parser failed with %d errors." % errs)
      return  -1
    return 0

  # Otherwise, build the AST
  ast = ParseFiles(filenames)
  errs = ast.GetProperty('ERRORS')
  if errs:
    ErrOut.Log('Found %d error(s).' % errs);
  InfoOut.Log("%d files processed." % len(filenames))
  return errs


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
