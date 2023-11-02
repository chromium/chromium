#!/usr/bin/env python3

# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for extract_sqlite_api.py.

These tests should be getting picked up by the PRESUBMIT.py in this directory.
"""

from importlib.machinery import SourceFileLoader
import os
import shutil
import sys
import tempfile
import unittest


class ExtractSqliteApiUnittest(unittest.TestCase):
    def setUp(self):
        self.test_root = tempfile.mkdtemp()
        source_path = os.path.join(
            os.path.dirname(os.path.realpath(__file__)),
            'extract_sqlite_api.py')
        self.extractor = SourceFileLoader('extract_api',
                                          source_path).load_module()

    def tearDown(self):
        if self.test_root:
            shutil.rmtree(self.test_root)

    def testExtractLineTuples(self):
        golden = [(1, 'Line1'), (2, ''), (3, 'Line 2'), (4, 'Line3'), (5, '')]
        text_with_newline = "Line1\n\nLine 2  \nLine3\n"
        self.assertEqual(
            self.extractor.ExtractLineTuples(text_with_newline), golden)

        golden = [(1, 'Line1'), (2, ''), (3, 'Line 2'), (4, 'Line3')]
        text_without_newline = "Line1\n\nLine 2  \nLine3"
        self.assertEqual(
            self.extractor.ExtractLineTuples(text_without_newline), golden)

    def testExtractPreprocessorDirectives(self):
        lines = [
            (1, '// Header comment'),
            (2, '#define DIRECTIVE 1'),
            (3, 'int main() { // \\'),
            (4, '}'),
            (5, ''),
            (6, '#define MULTILINE \\'),
            (7, 'MORE_MULTILINE_DIRECTIVE\\'),
            (8, 'END_MULTILINE_DIRECTIVE'),
            (9, 'void code() { }'),
        ]

        directives, code_lines = self.extractor.ExtractPreprocessorDirectives(
            lines)
        self.assertEqual(directives, [
            '#define DIRECTIVE 1',
            '#define MULTILINE \nMORE_MULTILINE_DIRECTIVE\nEND_MULTILINE_DIRECTIVE',
        ])
        self.assertEqual(code_lines, [
            (1, '// Header comment'),
            (3, 'int main() { // \\'),
            (4, '}'),
            (5, ''),
            (9, 'void code() { }'),
        ])

    def testExtractDefineMacroName(self):
        self.assertEqual(
            'SQLITE_API',
            self.extractor.ExtractDefineMacroName('#define SQLITE_API 1'))
        self.assertEqual(
            'SQLITE_API',
            self.extractor.ExtractDefineMacroName('#define SQLITE_API'))
        self.assertEqual(
            'SQLITE_API',
            self.extractor.ExtractDefineMacroName('#define SQLITE_API\n1'))
        self.assertEqual(
            'SQLITE_API',
            self.extractor.ExtractDefineMacroName(
                '#    define   SQLITE_API   1'))
        self.assertEqual(
            'SQLITE_API',
            self.extractor.ExtractDefineMacroName('#\tdefine\tSQLITE_API\t1'))
        self.assertEqual(
            None,
            self.extractor.ExtractDefineMacroName(' #define SQLITE_API 1'))
        self.assertEqual(
            None,
            self.extractor.ExtractDefineMacroName(' #define SQLITE_API() 1'))
        self.assertEqual(None, self.extractor.ExtractDefineMacroName(''))

    def testRemoveLineComments(self):
        self.assertEqual('word;', self.extractor.RemoveLineComments('word;'))
        self.assertEqual('', self.extractor.RemoveLineComments(''))
        self.assertEqual('', self.extractor.RemoveLineComments('// comment'))
        self.assertEqual('',
                         self.extractor.RemoveLineComments('/* comment */'))
        self.assertEqual('word;',
                         self.extractor.RemoveLineComments('wo/*comment*/rd;'))
        self.assertEqual(
            'word;*/', self.extractor.RemoveLineComments('wo/*comment*/rd;*/'))
        self.assertEqual(
            'word;*/',
            self.extractor.RemoveLineComments('wo/*/*comment*/rd;*/'))
        self.assertEqual(
            'word;', self.extractor.RemoveLineComments('wo/*comm//ent*/rd;'))

    def testRemoveComments(self):
        lines = [
            (1, 'code();'),
            (2, 'more_code(); /* with comment */ more_code();'),
            (3, '/**'),
            (4, 'Spec text'),
            (5, '**/ spec_code();'),
            (6,
             'late_code(); /* with comment */ more_late_code(); /* late comment'
             ),
            (7, 'ends here // C++ trap */ code(); // /* C trap'),
            (8, 'last_code();'),
        ]

        self.assertEqual(
            self.extractor.RemoveComments(lines), [
                (1, 'code();'),
                (2, 'more_code();  more_code();'),
                (3, ''),
                (5, ' spec_code();'),
                (6, 'late_code();  more_late_code(); '),
                (7, ' code(); '),
                (8, 'last_code();'),
            ])

    def testToStatementTuples(self):
        lines = [(1, 'void function();'), (2, 'int main('),
                 (3, '  int argc, char* argv) {'),
                 (4, '  statement1; statement2;'), (5, '}'), (6, 'stat'),
                 (7, 'ement4; statement5; sta'), (8, 'tem'),
                 (9, 'ent6; statement7;')]

        self.assertEqual(
            self.extractor.ToStatementTuples(lines), [
                (1, 1, 'void function()'),
                (2, 3, 'int main(\n  int argc, char* argv)'),
                (4, 4, 'statement1'),
                (4, 4, 'statement2'),
                (5, 5, ''),
                (6, 7, 'stat\nement4'),
                (7, 7, 'statement5'),
                (7, 9, 'sta\ntem\nent6'),
                (9, 9, 'statement7'),
            ])

    def testExtractApiExport(self):
        self.assertEqual(
            'sqlite3_init',
            self.extractor.ExtractApiExport(set(), 'SQLITE_API',
                                            'SQLITE_API void sqlite3_init()'))
        self.assertEqual(
            'sqlite3_sleep',
            self.extractor.ExtractApiExport(
                set(), 'SQLITE_API', 'SQLITE_API int sqlite3_sleep(int ms)'))
        self.assertEqual(
            'sqlite3_sleep',
            self.extractor.ExtractApiExport(
                set(), 'SQLITE_API',
                'SQLITE_API long long sqlite3_sleep(int ms)'))
        self.assertEqual(
            'sqlite3rbu_temp_size',
            self.extractor.ExtractApiExport(
                set(), 'SQLITE_API',
                'SQLITE_API sqlite3_int64 sqlite3rbu_temp_size(sqlite3rbu *pRbu)'
            ))
        self.assertEqual(
            'sqlite3_expired',
            self.extractor.ExtractApiExport(
                set(['SQLITE_DEPRECATED']), 'SQLITE_API',
                'SQLITE_API SQLITE_DEPRECATED int sqlite3_expired(sqlite3_stmt*)'
            ))
        # SQLite's header actually #defines double (in some cases).
        self.assertEqual(
            'sqlite3_column_double',
            self.extractor.ExtractApiExport(
                set(['double']), 'SQLITE_API',
                'SQLITE_API double sqlite3_column_double(sqlite3_stmt*, int iCol)'
            ))
        self.assertEqual(
            'sqlite3_temp_directory',
            self.extractor.ExtractApiExport(
                set(['SQLITE_EXTERN']), 'SQLITE_API',
                'SQLITE_API SQLITE_EXTERN char *sqlite3_temp_directory'))
        self.assertEqual(
            'sqlite3_version',
            self.extractor.ExtractApiExport(
                set(['SQLITE_EXTERN']), 'SQLITE_API',
                'SQLITE_API SQLITE_EXTERN const char sqlite3_version[]'))
        self.assertEqual(
            None,
            self.extractor.ExtractApiExport(
                set(['SQLITE_DEPRECATED']), 'SQLITE_API',
                'NOT_SQLITE_API struct sqlite_type sqlite3_sleep(int ms)'))

        with self.assertRaisesRegex(self.extractor.ExtractError,
                                    'Mixed simple .* and composite'):
            self.extractor.ExtractApiExport(
                set(), 'SQLITE_API',
                'SQLITE_API void int sqlite3_sleep(int ms)')
        with self.assertRaisesRegex(self.extractor.ExtractError,
                                    'Unsupported keyword struct'):
            self.extractor.ExtractApiExport(
                set(), 'SQLITE_API',
                'SQLITE_API struct sqlite_type sqlite3_sleep(int ms)')
        with self.assertRaisesRegex(self.extractor.ExtractError,
                                    'int\+\+ parsed as type name'):
            self.extractor.ExtractApiExport(
                set(), 'SQLITE_API', 'SQLITE_API int++ sqlite3_sleep(int ms)')
        with self.assertRaisesRegex(self.extractor.ExtractError,
                                    'sqlite3\+sleep parsed as symbol'):
            self.extractor.ExtractApiExport(
                set(), 'SQLITE_API', 'SQLITE_API int sqlite3+sleep(int ms)')

    def testExportedSymbolLine(self):
        self.assertEqual(
            '#define sqlite3_sleep chrome_sqlite3_sleep  // Line 42',
            self.extractor.ExportedSymbolLine(
                'chrome_', 'sqlite3_sleep',
                (42, 42, 'SQLITE_API int chrome_sqlite3_sleep(int ms)')))
        self.assertEqual(
            '#define sqlite3_sleep chrome_sqlite3_sleep  // Lines 42-44',
            self.extractor.ExportedSymbolLine(
                'chrome_', 'sqlite3_sleep',
                (42, 44, 'SQLITE_API int chrome_sqlite3_sleep(int ms)')))

    def testExportedExceptionLine(self):
        self.assertEqual(
            '// TODO: Lines 42-44 -- Something went wrong',
            self.extractor.ExportedExceptionLine(
                self.extractor.ExtractError('Something went wrong'),
                (42, 44, 'SQLITE_API int chrome_sqlite3_sleep(int ms)')))

    def testProcessSource(self):
        file_content = '\n'.join([
            '/*',
            'struct sqlite_type sqlite3_sleep;  // Remove comments',
            '*/',
            '#define SQLITE_DEPRECATED',
            'SQLITE_API int sqlite3_sleep(int ms);',
            'SQLITE_API struct sqlite_type sqlite3_sleep(int ms);',
            'SQLITE_API SQLITE_DEPRECATED int sqlite3_expired(sqlite3_stmt*);',
        ])
        golden_output = [
            '// Header',
            '#define sqlite3_expired chrome_sqlite3_expired  // Line 7',
            '#define sqlite3_sleep chrome_sqlite3_sleep  // Line 5',
            '// TODO: Lines 6-6 -- Unsupported keyword struct',
            '// Footer',
        ]
        self.assertEqual(
            golden_output,
            self.extractor.ProcessSource('SQLITE_API', 'chrome_', '// Header',
                                         '// Footer', file_content))

    def testProcessSourceFile(self):
        file_content = '\n'.join([
            '/*',
            'struct sqlite_type sqlite3_sleep;  // Remove comments',
            '*/',
            '#define SQLITE_DEPRECATED',
            'SQLITE_API int sqlite3_sleep(int ms);',
            'SQLITE_API struct sqlite_type sqlite3_sleep(int ms);',
            'SQLITE_API SQLITE_DEPRECATED int sqlite3_expired(sqlite3_stmt*);',
        ])
        golden_output = '\n'.join([
            '// Header',
            '#define sqlite3_expired chrome_sqlite3_expired  // Line 7',
            '#define sqlite3_sleep chrome_sqlite3_sleep  // Line 5',
            '// TODO: Lines 6-6 -- Unsupported keyword struct',
            '// Footer',
            '',
        ])

        input_file = os.path.join(self.test_root, 'input.h')
        output_file = os.path.join(self.test_root, 'macros.h')
        with open(input_file, 'w') as f:
            f.write(file_content)
        self.extractor.ProcessSourceFile('SQLITE_API', 'chrome_', '// Header',
                                         '// Footer', input_file, output_file)
        with open(output_file, 'r') as f:
            self.assertEqual(f.read(), golden_output)


if __name__ == '__main__':
    unittest.main()
