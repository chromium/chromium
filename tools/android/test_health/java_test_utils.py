# Lint as: python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import dataclasses
import pathlib
import re
import sys
from typing import List, Optional

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).resolve(strict=True).parents[1]
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import git_metadata_utils

_CHROMIUM_SRC_PATH = git_metadata_utils.get_chromium_src_path()

_SIX_SRC_PATH = (_CHROMIUM_SRC_PATH / 'third_party' / 'six' /
                 'src').resolve(strict=True)
# six is a dependency of javalang
sys.path.insert(0, str(_SIX_SRC_PATH))

_JAVALANG_SRC_PATH = (_CHROMIUM_SRC_PATH / 'third_party' / 'javalang' /
                      'src').resolve(strict=True)
if str(_JAVALANG_SRC_PATH) not in sys.path:
    sys.path.insert(1, str(_JAVALANG_SRC_PATH))
import javalang
from javalang.tree import (Annotation, ClassDeclaration, CompilationUnit,
                           MethodDeclaration, PackageDeclaration)

_TEST_ANNOTATION = 'Test'
_DISABLED_TEST_ANNOTATION = 'DisabledTest'
_DISABLE_IF_TEST_ANNOTATION = 'DisableIf'

_DISABLE_IF_TEST_PATTERN = re.compile(_DISABLE_IF_TEST_ANNOTATION + r'\.\w+')


@dataclasses.dataclass(frozen=True)
class JavaTestHealth:
    """Holder class for Java test health information."""
    java_package: Optional[str]
    """The Java package containing the test, if specified, else `None`."""
    disabled_tests_count: int
    """The number of test cases annotated with @DisabledTest."""
    disable_if_tests_count: int
    """The number of test cases annotated with @DisableIf."""
    tests_count: int
    """The total number of test cases."""
    disabled_tests: List[str]
    disable_if_tests: List[str]


def get_java_test_health(test_path: pathlib.Path) -> JavaTestHealth:
    """Gets test health information for a Java test.

    The Java test health information includes the Java package, if applicable,
    that the test belongs to and the number of test cases marked as disabled,
    conditionally-disabled and flaky.

    Args:
        test_path:
            The path to a Java test class.

    Returns:
        Test health information obtained from the abstract syntax tree (AST).
    """
    java_file_contents: str = test_path.resolve(strict=True).read_text()

    try:
        java_ast: CompilationUnit = javalang.parse.parse(java_file_contents)
    except javalang.parser.JavaSyntaxError as syntax_error:
        raise JavaSyntaxError(syntax_error.description,
                              line_num=syntax_error.at.position.line,
                              column_num=syntax_error.at.position.column,
                              file_path=test_path,
                              java_src_code=java_file_contents) from None
    except javalang.tokenizer.LexerError as lexer_error:
        raise JavaSyntaxError(str(lexer_error),
                              line_num=0,
                              column_num=0,
                              file_path=test_path,
                              java_src_code=java_file_contents) from None

    return _get_java_test_health(java_ast)


def _get_java_test_health(java_ast: CompilationUnit) -> JavaTestHealth:
    """Gets test health information from the AST of a Java test.

    The Java test health information includes the Java package, if applicable,
    that the test belongs to and the number of test cases marked as disabled,
    conditionally-disabled and flaky.

    Args:
        java_ast:
            The abstract syntax tree (AST) of the Java test.

    Returns:
        Java test health information based on the test's AST.
    """
    annotation_counter = collections.Counter()
    disabled_test_list = []
    disable_if_test_list = []

    java_classes: List[ClassDeclaration] = java_ast.types
    for java_class in java_classes:
        if any(annotation.name == _DISABLED_TEST_ANNOTATION
               for annotation in java_class.annotations):
            all_test_methods = _collect_all_test_methods(java_class)
            annotation_counter[_DISABLED_TEST_ANNOTATION] += len(
                all_test_methods)
            for test_method in all_test_methods:
                test_full_name = (f'{java_ast.package.name}'
                                  f'.{java_class.name}'
                                  f'#{test_method}')
                disabled_test_list.append(test_full_name)
            continue
        elif any(
                re.fullmatch(_DISABLE_IF_TEST_PATTERN, annotation.name)
                for annotation in java_class.annotations):
            all_test_methods = _collect_all_test_methods(java_class)
            annotation_counter[_DISABLE_IF_TEST_ANNOTATION] += len(
                all_test_methods)
            for test_method in all_test_methods:
                test_full_name = (f'{java_ast.package.name}'
                                  f'.{java_class.name}'
                                  f'#{test_method}')
                disable_if_test_list.append(test_full_name)
            continue

        java_methods: List[MethodDeclaration] = java_class.methods
        for java_method in java_methods:
            annotation_counter.update(
                _count_annotations(java_method.annotations))
            if any(annotation.name == _DISABLED_TEST_ANNOTATION
                   for annotation in java_method.annotations):
                test_full_name = (f'{java_ast.package.name}'
                                  f'.{java_class.name}'
                                  f'#{java_method.name}')
                disabled_test_list.append(test_full_name)
            elif any(
                    re.fullmatch(_DISABLE_IF_TEST_PATTERN, annotation.name)
                    for annotation in java_method.annotations):
                test_full_name = (f'{java_ast.package.name}'
                                  f'.{java_class.name}'
                                  f'#{java_method.name}')
                disable_if_test_list.append(test_full_name)

    return JavaTestHealth(
        java_package=_get_java_package_name(java_ast),
        disabled_tests_count=annotation_counter[_DISABLED_TEST_ANNOTATION],
        disabled_tests=disabled_test_list,
        disable_if_tests_count=annotation_counter[_DISABLE_IF_TEST_ANNOTATION],
        tests_count=annotation_counter[_TEST_ANNOTATION],
        disable_if_tests=disable_if_test_list,
    )


def _collect_all_test_methods(java_class: ClassDeclaration) -> List[str]:
    return [
        method.name for method in java_class.methods
        if any(a.name == _TEST_ANNOTATION for a in method.annotations)
    ]


def _collect_all_test_methods(java_class: ClassDeclaration) -> List[str]:
    """Gets the names of all @Test methods in a Java class."""
    return [
        method.name for method in java_class.methods
        if any(a.name == _TEST_ANNOTATION for a in method.annotations)
    ]


def _get_java_package_name(java_ast: CompilationUnit) -> Optional[str]:
    """Gets the Java package name, if specified, from an abstract syntax tree.

    If the abstract syntax tree (AST) does not contain a package declaration in
    the form 'package com.foo.bar.baz;' this returns `None`.

    Args:
        java_ast:
            The abstract syntax tree (AST) of the Java source.

    Returns:
        The Java package name if found, else `None`.
    """
    package: Optional[PackageDeclaration] = java_ast.package

    return package.name if package else None


def _count_annotations(annotations: List[Annotation]) -> collections.Counter:
    counter = collections.Counter()

    for annotation in annotations:
        if annotation.name == _TEST_ANNOTATION:
            counter[_TEST_ANNOTATION] += 1
        elif annotation.name == _DISABLED_TEST_ANNOTATION:
            counter[_DISABLED_TEST_ANNOTATION] += 1
        elif re.fullmatch(_DISABLE_IF_TEST_PATTERN, annotation.name):
            counter[_DISABLE_IF_TEST_ANNOTATION] += 1

    return counter


class JavaSyntaxError(SyntaxError):
    """A syntax error found when parsing Java source code."""

    def __init__(self, error_message, *, line_num: int, column_num: int,
                 file_path: pathlib.Path, java_src_code: str):
        """Instantiates a JavaParseError.

        Args:
            msg:
                A description of the Java syntax error.
            line_num:
                The line containing the start of the error.
            column_num:
                The starting column in the line containing the error.
            filename:
                The filename of the Java file containing the error.
            java_src_code:
                The source code of the Java file containing the error.
        """
        super().__init__(error_message)
        self.lineno = line_num
        self.offset = column_num
        self.filename = str(file_path.relative_to(_CHROMIUM_SRC_PATH))
        if line_num > 0:
            self.text = java_src_code.splitlines()[line_num - 1]
        else:
            self.text = '<unknown line>'
