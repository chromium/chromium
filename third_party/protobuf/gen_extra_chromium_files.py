#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates extra Chromium files and cleans up upstream pre-generated files.

This script builds Chromium's patched `protoc` and uses it to:
1. Clean up unwanted upstream directories/files (e.g. `compatibility/` and
   pre-generated Well-Known Type `.pb.{cc,h}` files).
2. Regenerate bootstrapped C++ proto headers/sources (`descriptor.pb.{h,cc}`
   and `compiler/plugin.pb.{h,cc}`) with Chromium's compiler patches applied.
3. Generate Python proto files (`descriptor_pb2.py` and `plugin_pb2.py`).
4. Generate `python/google/protobuf/internal/python_edition_defaults.py`
   directly via `protoc --edition_defaults_out` without requiring Bazel.
"""

import argparse
import os
import re
import shutil
import subprocess
import tempfile

PROTO_DIR = os.path.dirname(os.path.abspath(__file__))

WKT_PROTOS = [
    'any',
    'api',
    'duration',
    'empty',
    'field_mask',
    'source_context',
    'struct',
    'timestamp',
    'type',
    'wrappers',
]


def run_cmd(cmd):
    print('Running:', ' '.join(cmd))
    subprocess.check_call(cmd)


def c_escape(data: bytes) -> str:
    """Escapes bytes using octal/C-string escaping matching absl::CEscape."""
    out = []
    for b in data:
        if b == ord('\n'):
            out.append(r'\n')
        elif b == ord('\r'):
            out.append(r'\r')
        elif b == ord('\t'):
            out.append(r'\t')
        elif b == ord('\\'):
            out.append(r'\\')
        elif b == ord('"'):
            out.append(r'\"')
        elif b == ord('\''):
            out.append(r'\'')
        elif 32 <= b <= 126:
            out.append(chr(b))
        else:
            out.append(f'\\{b:03o}')
    return ''.join(out)


def get_python_edition_range() -> tuple[str, str]:
    """Parses minimum_edition and maximum_edition from python/build_targets.bzl."""
    bzl_path = os.path.join(PROTO_DIR, 'python', 'build_targets.bzl')
    with open(bzl_path, 'r', encoding='utf-8') as f:
        content = f.read()
    block_match = re.search(
        r'compile_edition_defaults\(\s*name\s*=\s*"python_edition_defaults"[^)]*\)',
        content,
        re.DOTALL,
    )
    if not block_match:
        raise RuntimeError(
            f'Could not find python_edition_defaults rule in {bzl_path}')
    block = block_match.group(0)
    min_match = re.search(r'minimum_edition\s*=\s*"([^"]+)"', block)
    max_match = re.search(r'maximum_edition\s*=\s*"([^"]+)"', block)
    if not min_match or not max_match:
        raise RuntimeError(
            f'Could not parse min/max edition from {bzl_path}: {block}')
    return min_match.group(1), max_match.group(1)


def generate_python_edition_defaults(protoc: str):
    min_ed, max_ed = get_python_edition_range()
    template_path = os.path.join(
        PROTO_DIR,
        'python',
        'google',
        'protobuf',
        'internal',
        'python_edition_defaults.py.template',
    )
    out_path = os.path.join(
        PROTO_DIR,
        'python',
        'google',
        'protobuf',
        'internal',
        'python_edition_defaults.py',
    )
    print(f'Creating //third_party/protobuf/python/google/protobuf/internal/'
          f'python_edition_defaults.py (min={min_ed}, max={max_ed})')
    with tempfile.NamedTemporaryFile(suffix='.binpb') as tmp:
        run_cmd([
            protoc,
            f'--edition_defaults_out={tmp.name}',
            f'--edition_defaults_minimum={min_ed}',
            f'--edition_defaults_maximum={max_ed}',
            f'--proto_path={os.path.join(PROTO_DIR, "src")}',
            os.path.join(PROTO_DIR, 'src', 'google', 'protobuf',
                         'descriptor.proto'),
        ])
        defaults_bytes = tmp.read()

    with open(template_path, 'r', encoding='utf-8') as f:
        template_content = f.read()

    rendered = template_content.replace('DEFAULTS_VALUE',
                                        c_escape(defaults_bytes))
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(rendered)


def cleanup_upstream_files():
    compat_dir = os.path.join(PROTO_DIR, 'compatibility')
    if os.path.exists(compat_dir):
        print('Removing //third_party/protobuf/compatibility/')
        shutil.rmtree(compat_dir)

    for wkt in WKT_PROTOS:
        for ext in ('.pb.h', '.pb.cc'):
            path = os.path.join(PROTO_DIR, 'src', 'google', 'protobuf',
                                wkt + ext)
            if os.path.exists(path):
                print(f'Removing pre-generated WKT file: {path}')
                os.remove(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-directory',
                        '-C',
                        help='Path to build directory',
                        required=True)

    args = parser.parse_args()
    out_build_dir = args.output_directory

    cleanup_upstream_files()

    run_cmd(['autoninja', '-C', out_build_dir, 'protoc'])

    protoc = os.path.join(out_build_dir, 'protoc')
    src_dir = os.path.join(PROTO_DIR, 'src')
    py_dir = os.path.join(PROTO_DIR, 'python')

    print('Regenerating //third_party/protobuf/src/google/protobuf/'
          'descriptor.pb.{h,cc}')
    run_cmd([
        protoc,
        f'--proto_path={src_dir}',
        f'--cpp_out=dllexport_decl=PROTOBUF_EXPORT:{src_dir}',
        os.path.join(src_dir, 'google', 'protobuf', 'descriptor.proto'),
    ])

    print('Regenerating //third_party/protobuf/src/google/protobuf/compiler/'
          'plugin.pb.{h,cc}')
    run_cmd([
        protoc,
        f'--proto_path={src_dir}',
        f'--cpp_out=dllexport_decl=PROTOC_EXPORT:{src_dir}',
        os.path.join(src_dir, 'google', 'protobuf', 'compiler', 'plugin.proto'),
    ])

    print('Creating //third_party/protobuf/python/google/protobuf/'
          'descriptor_pb2.py')
    run_cmd([
        protoc,
        f'--proto_path={src_dir}',
        f'--python_out={py_dir}',
        os.path.join(src_dir, 'google', 'protobuf', 'descriptor.proto'),
    ])

    print('Creating //third_party/protobuf/python/google/protobuf/compiler/'
          'plugin_pb2.py')
    run_cmd([
        protoc,
        f'--proto_path={src_dir}',
        f'--python_out={py_dir}',
        os.path.join(src_dir, 'google', 'protobuf', 'compiler', 'plugin.proto'),
    ])

    generate_python_edition_defaults(protoc)


if __name__ == '__main__':
    main()
