# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys
from css_generator import CSSStyleGenerator


def BuildGrepQuery(deleted_names):
    # Query is built as \--var-1|--var-2|... The first backslash is necessary to
    # prevent --var-1 from being read as an argument. The pipes make a big OR
    # query.
    return '\\' + '|'.join(deleted_names)


def RunGit(command):
    """Run a git subcommand, returning its output."""
    command = ['git'] + command
    proc = subprocess.Popen(command, stdout=subprocess.PIPE)
    out = proc.communicate()[0].strip()
    return out


def FindDeletedCSSVariables(input_api, output_api, input_file_filter):
    files = input_api.AffectedFiles(
        file_filter=lambda f: input_api.FilterSourceFile(
            f, files_to_check=input_file_filter))

    def get_css_var_names_for_contents(contents_function):
        style_generator = CSSStyleGenerator()
        for f in files:
            file_contents = contents_function(f)
            if len(file_contents) == 0:
                continue
            style_generator.AddJSONToModel('\n'.join(file_contents),
                                           in_file=f.LocalPath())
        return style_generator.GetCSSVarNames()

    old_names = get_css_var_names_for_contents(lambda f: f.OldContents())
    new_names = get_css_var_names_for_contents(lambda f: f.NewContents())

    deleted_names = old_names.difference(new_names)
    if not deleted_names:
        return []

    # Use --full-name and -n for formatting, -E for extended regexp, and :/
    # as pathspec for grepping across entire repository (assumes git > 1.9.1)
    problems = RunGit([
        'grep', '--full-name', '-En',
        BuildGrepQuery(deleted_names), '--', ':/'
    ]).splitlines()

    if not problems:
        return []

    return [
        output_api.PresubmitPromptWarning(
            'style_variable_generator variables were deleted but usages of ' +
            'generated CSS variables were found in the codebase:',
            items=problems)
    ]
