#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Parses SQLite source code and produces renaming macros for its exported symbols.

Usage:
    extract_sqlite_api.py sqlite.h rename_macros.h

For example, the following renaming macro is produced for sqlite3_initialize().

    #define sqlite3_initialize chrome_sqlite3_initialize
'''

import re
import sys


class ExtractError(ValueError):
    def __init__(self, message):
        self.message = message


def ExtractLineTuples(string):
    '''Returns a list of lines, with start/end whitespace stripped.

    Each line is a tuple of (line number, string).
    '''
    raw_lines = string.split('\n')
    stripped_lines = [line.strip() for line in raw_lines]
    return list(enumerate(stripped_lines, start=1))


def ExtractPreprocessorDirectives(lines):
    '''Extracts preprocessor directives from lines of C code.

    Each input line should be a tuple of (line number, string).

    Returns a list of preprocessor directives, and a list of C code lines with
    the preprocessor directives removed. The returned code lines are a subset
    of the input tuples.
    '''
    code_lines = []
    directives = []
    in_directive = False
    last_directive = []
    for line_tuple in lines:
        line = line_tuple[1]
        # Preprocessor directives start with #.
        if not in_directive:
            if len(line) > 0 and line[0] == '#':
                in_directive = True
                last_directive = []

        # Preprocessor directives use \ as a line continuation character.
        if in_directive:
            if line[-1] == '\\':
                line = line[:-1]
            else:
                in_directive = False
            last_directive.append(line)

            if not in_directive:
                directives.append('\n'.join(last_directive))
        else:
            code_lines.append(line_tuple)

    return directives, code_lines


# Regular expression used to parse a macro definition.
DEFINITION_RE = re.compile(r'^\#\s*define\s+(\w+)(\s|$)')


def ExtractDefineMacroName(line):
    '''Extracts the macro name from a non-function preprocessor definition.

    Returns None if the preprocessor line is not a preprocessor macro
    definition.  Macro functions are not considered preprocessor definitions.
    '''
    match = DEFINITION_RE.match(line)
    if match is None:
        return None
    return match.group(1)


# Matches C++-style // single-line comments.
SINGLE_LINE_COMMENT_RE = re.compile(r'//.*$')
# Matches C-style /* multi-line comments */.
MULTI_LINE_COMMENT_RE = re.compile(
    r'/\*.*?\*/', flags=re.MULTILINE | re.DOTALL)


def RemoveLineComments(line):
    '''Returns the given C code line with comments removed.

    This handles both C-style /* comments */ and C++-style // comments, but
    cannot tackle C-style comments that extend over multiple lines.
    '''
    return SINGLE_LINE_COMMENT_RE.sub('', MULTI_LINE_COMMENT_RE.sub('', line))


def RemoveComments(code_tuples):
    'Returns the given C code tuples with all comments removed.'

    output_tuples = []
    in_comment = False
    for line_number, line in code_tuples:
        if in_comment:
            if '*/' in line:
                _, line = line.split('*/', 1)
                in_comment = False
        if not in_comment:
            line = RemoveLineComments(line)
            if '/*' in line:
                line, _ = line.split('/*', 1)
                in_comment = True
            output_tuples.append((line_number, line))
    return output_tuples


# Splits a line of C code into statement pieces.
STATEMENT_BREAK_RE = re.compile(r'[;{}]')


def ToStatementTuples(code_tuples):
    '''Converts C code lines into statements.

    The input is tuples of (line number, line code string). The output is
    tuples of (min line, max line, statement).

    The function considers ; { and } to be statement separators. This is
    sufficiently correct, given our goal.
    '''
    statements = []
    current_statement = ''
    current_start = 0

    for line_number, line in code_tuples:
        pieces = STATEMENT_BREAK_RE.split(line)
        for piece in pieces[:-1]:  # The last piece is an unfinished statement.
            if current_statement != '':
                current_statement = current_statement + '\n' + piece
                statements.append((current_start, line_number,
                                   current_statement.strip()))
                current_statement = ''
            else:
                statements.append((line_number, line_number, piece.strip()))

        if current_statement == '':
            current_start = line_number
        if pieces[-1] != '':
            current_statement = current_statement + '\n' + pieces[-1]

    return statements


# Used to break down a line into words.
WHITESPACE_RE = re.compile(r'\s+')

# Features unsupported by our extractor.
#
# We do not support parsing struct and enum literals because sqlite typedefs
# them before incorporating them into exported symbols. We can avoid matching
# curly braces because we do not support enum, struct, or union, and we only
# need to consider declarations involving typedef names and primitive types.
UNSUPPORTED_KEYWORDS = set(['enum', 'struct', 'union', 'typedef'])

# Type qualifiers that we can skip over.
#
# We discard storage-class specifiers and type qualifiers. For purposes of
# finding the end of declaration specifiers, they are not needed. This
# additionally discards any pointer type qualifiers.
QUALIFIER_KEYWORDS = set([
    'extern',
    'static',
    'auto',
    'register',
    'const',
    'volatile',
])

# Keywords used in composite primitive types.
#
# Types using these keywords may have more than one keyword, e.g.
# "long long int".
COMPOSITE_TYPE_SPECIFIERS = set([
    'char',
    'short',
    'int',
    'long',
    'float',
    'double',
    'signed',
    'unsigned',
])

# Matches an identifier.
IDENTIFIER_RE = re.compile(r'^[a-zA-Z_0-9]+$')


def ExtractApiExport(macro_names, api_export_macro, statement):
    '''Extracts the symbol name from a statement exporting a function.

    Returns None if the statement does not export a symbol. Throws ExtractError
    if the parser cannot understand the statement.
    '''
    # See http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf, section 6.7
    # for how to parse C declarations. Note that a declaration is a number of
    # declaration-specifiers, followed by a list of declarators with optional
    # initializer. Multiple declarators would be a declaration like:
    #
    # int a, b;
    #
    # While, in principle, one could declare a pair of C functions like this, no
    # one does it. We assume there is only one declarator.
    #
    # int foo(int), bar(int, int);
    #
    # Jumping to section 6.7.5, a declarator includes some optional pointer
    # specifiers (which may have type qualifiers like 'const' embedded, e.g. 'int
    # * const * const foo') and some grouping. Note, however, that in all cases,
    # the declaration name is the first non-type-qualifier identifier.
    #
    # Thus our goal is to skip the declaration specifiers and get to the
    # declarators.

    # Simplification: get rid of pointer characters.
    statement = statement.replace('*', ' ')

    # Simplification: make sure each open parenthesis is each own word.
    statement = statement.replace('(', ' ( ')
    statement = statement.replace('[', ' [ ')

    words = WHITESPACE_RE.split(statement)

    # Ignore statements that don't deal with exporting symbols.
    if api_export_macro not in words:
        return None

    seen_composite_type = False
    seen_simple_type = False
    for word in words:
        if word in UNSUPPORTED_KEYWORDS:
            raise ExtractError("Unsupported keyword %s" % word)

        if word in QUALIFIER_KEYWORDS:
            continue

        # Per section 6.7.2, we must have at least one type specifier (so the first
        # token is one). Moreover, clause 2 implies that if we have a typedef name,
        # enum, struct, or union, it is the only type specifier. If we have a
        # keyword such as 'int', we may have one or more of such keywords.

        if word in COMPOSITE_TYPE_SPECIFIERS:
            if seen_simple_type:
                raise ExtractError(
                    'Mixed simple (struct_name) and composite (int) types')
            seen_composite_type = True
            continue

        # We assume that macros are only used for qualifiers, which can be skipped.
        if word in macro_names or word == api_export_macro:
            continue

        if not seen_composite_type and not seen_simple_type:
            seen_simple_type = True
            if IDENTIFIER_RE.match(word) is None:
                raise ExtractError(
                    "%s parsed as type name, which doesn't make sense" % word)
            continue

        if IDENTIFIER_RE.match(word) is None:
            raise ExtractError(
                "%s parsed as symbol name, which doesn't make sense" % word)
        return word

    raise ExtractError('Failed to find symbol name')


def ExportedSymbolLine(symbol_prefix, symbol, statement_tuple):
    'Returns an output line for an exported symbol.'
    if statement_tuple[0] == statement_tuple[1]:
        lines = 'Line %d' % statement_tuple[0]
    else:
        lines = 'Lines %d-%d' % (statement_tuple[0], statement_tuple[1])
    return '#define %s %s%s  // %s' % (symbol, symbol_prefix, symbol, lines)


def ExportedExceptionLine(exception, statement_tuple):
    'Returns an output line for a parsing failure.'

    # Output a TODO without a name so the broken parsing result doesn't
    # accidentally get checked in.
    return '// TODO: Lines %d-%d -- %s' % (
        statement_tuple[0], statement_tuple[1], exception.message)


def ProcessSource(api_export_macro, symbol_prefix, header_line, footer_line,
                  file_content):
    'Returns a list of lines that rename exported symbols in an C program file.'

    line_tuples = ExtractLineTuples(file_content)
    line_tuples = RemoveComments(line_tuples)
    directives, code_tuples = ExtractPreprocessorDirectives(line_tuples)
    macro_names = set(
        name for name in
        [ExtractDefineMacroName(directive) for directive in directives]
        if name is not None)
    statement_tuples = ToStatementTuples(code_tuples)

    output_lines = []
    for statement_tuple in statement_tuples:
        line = statement_tuple[2]
        try:
            symbol_name = ExtractApiExport(macro_names, api_export_macro, line)
            if symbol_name:
                output_lines.append(
                    ExportedSymbolLine(symbol_prefix, symbol_name,
                                       statement_tuple))
        except ExtractError as exception:
            output_lines.append(
                ExportedExceptionLine(exception, statement_tuple))

    output_lines.sort()
    return [header_line] + output_lines + [footer_line]


def ProcessSourceFile(api_export_macro, symbol_prefix, header_line,
                      footer_line, input_file, output_file):
    'Reads in a C program file and outputs macros renaming exported symbols.'

    with open(input_file, 'r') as f:
        file_content = f.read()
    output_lines = ProcessSource(api_export_macro, symbol_prefix, header_line,
                                 footer_line, file_content)
    output_lines.append('')
    with open(output_file, 'w') as f:
        f.write('\n'.join(output_lines))


header_line = '''// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is generated by extract_sqlite_api.py.

#ifndef THIRD_PARTY_SQLITE_AMALGAMATION_RENAME_EXPORTS_H_
#define THIRD_PARTY_SQLITE_AMALGAMATION_RENAME_EXPORTS_H_
'''

footer_line = '''
#endif  // THIRD_PARTY_SQLITE_AMALGAMATION_RENAME_EXPORTS_H_
'''

if __name__ == '__main__':
    ProcessSourceFile(
        api_export_macro='SQLITE_API',
        symbol_prefix='chrome_',
        header_line=header_line,
        footer_line=footer_line,
        input_file=sys.argv[1],
        output_file=sys.argv[2])
