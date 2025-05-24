#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import pathlib
import re
import subprocess

_SRC_ROOT = pathlib.Path(__file__).parents[3]

_GOOGLE_JAVA_FORMAT = (_SRC_ROOT / 'third_party' / 'google-java-format' /
                       'google-java-format')

_IMPORTS = """import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.RequiresNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.build.NullUtil.assertNonNull;
"""

_MODIFIER_KEYWORDS = (r'(?:(?:' + '|'.join([
    'abstract',
    'default',
    'final',
    'native',
    'private',
    'protected',
    'public',
    'static',
    'synchronized',
    r'/\*\s*package\s*\*/',
]) + r')\s+)*')

_NULLABLE_RE = re.compile(r'(\n *)@Nullable'
                          r'('
                          r'(?:\s*@\w+(?:\(.*?\))?)*'
                          r'\s+(?:' + _MODIFIER_KEYWORDS + r')?' +
                          r'(?:<.*?>)?'
                          r')')
_CLASSES_REGEX = re.compile(
    r'(^(?:public|protected|private|/\*\s*package\s*\*/)?\s*'
    r'(?:(?:static|abstract|final|sealed)\s+)*'
    r'(?:class|@?interface|enum)\s+\w+)',
    flags=re.MULTILINE)


def _mark_file(path):
    data = path.read_text()
    if '@NullMarked' in data:
        logging.warning('Skipping %s. Already has @NullMarked', path)
        return False
    if '@NullUnmarked' in data:
        logging.warning('Skipping %s. Already has @NullUnmarked', path)
        return False

    data = data.replace('import androidx.annotation.Nullable;\n', '')
    # Move @Nullable before methods to right before return type.
    data = _NULLABLE_RE.sub(r'\1\2 @Nullable ', data)
    # Fix up type-use position.
    data = re.sub(r'@Nullable\s+((?:\w+\.)+)(\w+)', r'\1@Nullable \2', data)
    data = re.sub(r'@Nullable\s+([\w<>]+)\[\]', r'\1 @Nullable[]', data)

    # Remove @NonNull
    data = data.replace('@NonNull', '')
    # Add imports
    data = re.sub(r'(^package .*\n)',
                  r'\1' + _IMPORTS,
                  data,
                  flags=re.MULTILINE,
                  count=1)

    # Add @NullMarked
    data = _CLASSES_REGEX.sub(r'@NullMarked\n\1', data, count=1)

    # Make all Void's @Nullable
    if re.search(r'\bVoid\b', data):
        data = re.sub(r'\bVoid\b', '@Nullable Void', data)
        data = data.replace('@Nullable @Nullable Void', '@Nullable Void')

    # Make all Supplier<Tab> -> Supplier<@Nullable Tab>
    if 'Supplier<Tab>' in data:
        data = data.replace('Supplier<Tab>', 'Supplier<@Nullable Tab>')

    logging.info('Processed: %s', path)
    path.write_text(data)
    return True


def main():
    logging.basicConfig(format='%(message)s', level=logging.INFO)

    parser = argparse.ArgumentParser()
    parser.add_argument('files', nargs='+')
    args = parser.parse_args()

    changed_paths = []
    for f in args.files:
        if _mark_file(pathlib.Path(f)):
            changed_paths.append(f)

    if changed_paths:
        cmd = [
            str(_GOOGLE_JAVA_FORMAT), '--aosp', '--skip-javadoc-formatting',
            '--skip-removing-unused-imports', '--replace'
        ] + changed_paths
        logging.info('Running: %s', ' '.join(cmd))
        subprocess.check_call(cmd)
    print(f'Added @NullMarked to {len(changed_paths)} path(s).')


if __name__ == '__main__':
    main()
