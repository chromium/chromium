#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates GN build files for protobuf. See update_file_lists.sh from
protobuf."""

import os.path

PROTO_DIR = os.path.dirname(__file__)
MAKEFILE = os.path.join(PROTO_DIR, 'Makefile.am')
SRC_MAKEFILE = os.path.join(PROTO_DIR, 'src', 'Makefile.am')
PROTO_SOURCES_GNI = os.path.join(PROTO_DIR, 'proto_sources.gni')


def read_makefile_lines(f):
    # Roughly implement https://www.gnu.org/software/make/manual/html_node/Splitting-Lines.html
    # but skip condensing whitespace. That can be handled by the reader. Also
    # make no distinction between recipe and non-recipe lines.
    cont = []
    while True:
        line = f.readline()
        if line == '':
            if len(cont) != 0:
                raise ValueError('Continuation at end of file')
            break
        line = line.rstrip('\n')
        if line.endswith('\\'):
            cont.append(line[:-1])
        else:
            cont.append(line)
            yield ' '.join(cont)
            cont = []


def read_makefile_variables(f):
    ret = {}
    for line in read_makefile_lines(f):
        if not line or line[0].isspace():
            continue
        # Sometimes there aren't spaces surrounding equals.
        line = line.replace('=', ' = ')
        # Do a very rough parse.
        tokens = line.split()
        if len(tokens) >= 2 and tokens[1] == '=':
            value = []
            for token in tokens[2:]:
                if token.startswith('$(') and token.endswith(')'):
                    value.extend(ret.get(token[2:-1], []))
                else:
                    value.append(token)
            ret[tokens[0]] = value
    return ret


def is_protoc_header(path):
    if '/compiler/' not in path:
        return False
    # compiler/importer.h and compiler/parser.h should be part of libprotobuf
    # itself.
    return not path.endswith("/importer.h") and not path.endswith("/parser.h")


def prefix_paths(paths):
    return [f'src/{p}' for p in paths]


def write_gn_variable(f, name, value):
    f.write(f'\n{name} = [\n')
    # Sort and deduplicate the file lists. Protobuf has some duplicate entries.
    for path in sorted(set(value)):
        f.write(f'  "{path}",\n')
    f.write(']\n')


def main():
    with open(SRC_MAKEFILE) as f:
        vars = read_makefile_variables(f)
    protobuf_headers = [
        p for p in vars['nobase_include_HEADERS'] if not is_protoc_header(p)
    ]
    protobuf_lite_sources = vars['libprotobuf_lite_la_SOURCES']
    protobuf_sources = [
        p for p in vars['libprotobuf_la_SOURCES']
        if p not in protobuf_lite_sources
    ]
    protoc_sources = vars['libprotoc_la_SOURCES']
    protoc_headers = [
        p for p in vars['nobase_include_HEADERS'] if is_protoc_header(p)
    ]

    protoc_java_sources = [p for p in protoc_sources if 'compiler/java' in p]
    protoc_java_headers = [p for p in protoc_headers if 'compiler/java' in p]

    protoc_python_sources = [p for p in protoc_sources if 'compiler/python' in p]
    protoc_python_headers = [p for p in protoc_headers if 'compiler/python' in p]

    protoc_sources =[p for p in protoc_sources if not p in protoc_java_sources]
    protoc_headers =[p for p in protoc_headers if not p in protoc_java_headers]

    protoc_sources =[p for p in protoc_sources if not p in protoc_python_sources]
    protoc_headers =[p for p in protoc_headers if not p in protoc_python_headers]

    protobuf_headers = prefix_paths(protobuf_headers)
    protobuf_lite_sources = prefix_paths(protobuf_lite_sources)
    protobuf_sources = prefix_paths(protobuf_sources)
    protoc_sources = prefix_paths(protoc_sources)
    protoc_headers = prefix_paths(protoc_headers)
    protoc_java_sources = prefix_paths(protoc_java_sources)
    protoc_java_headers = prefix_paths(protoc_java_headers)
    protoc_python_sources = prefix_paths(protoc_python_sources)
    protoc_python_headers = prefix_paths(protoc_python_headers)

    # Not upstream protobuf, added via Chromium patch.
    protobuf_lite_sources.append("src/google/protobuf/arenastring.cc")

    with open(MAKEFILE) as f:
        vars = read_makefile_variables(f)
    all_python_sources = [
        p for p in vars['python_EXTRA_DIST']
        if p.endswith('.py') and 'test' not in p
    ]
    # The copy rules in BUILD.gn can only handle files in the same directory, so
    # the list must be split into per-directory lists.
    pyproto_sources = [
        p for p in all_python_sources
        if os.path.dirname(p) == 'python/google/protobuf'
    ]
    pyproto_internal_sources = [
        p for p in all_python_sources
        if os.path.dirname(p) == 'python/google/protobuf/internal'
    ]

    with open(PROTO_SOURCES_GNI, 'w') as f:
        f.write('''# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generated by gen_chromium_file_lists.py. Do not edit by hand.
''')
        write_gn_variable(f, 'protobuf_headers', protobuf_headers)
        write_gn_variable(f, 'protobuf_lite_sources', protobuf_lite_sources)
        write_gn_variable(f, 'protobuf_sources', protobuf_sources)
        write_gn_variable(f, 'protoc_sources', protoc_sources)
        write_gn_variable(f, 'protoc_headers', protoc_headers)
        write_gn_variable(f, 'protoc_java_sources', protoc_java_sources)
        write_gn_variable(f, 'protoc_java_headers', protoc_java_headers)
        write_gn_variable(f, 'protoc_python_sources', protoc_python_sources)
        write_gn_variable(f, 'protoc_python_headers', protoc_python_headers)
        write_gn_variable(f, 'pyproto_sources', pyproto_sources)
        write_gn_variable(f, 'pyproto_internal_sources',
                          pyproto_internal_sources)


if __name__ == '__main__':
    main()
