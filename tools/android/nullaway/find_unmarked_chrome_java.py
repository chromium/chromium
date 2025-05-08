#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import csv
import dataclasses
import logging
import os
import pathlib
import re
import sys

_SRC_ROOT = pathlib.Path(__file__).parents[3]
sys.path.insert(1, str(_SRC_ROOT / 'build/android/gyp'))

import check_for_missing_direct_deps

_CHROME_JAVA_SOURCES = 'gen/chrome/android/chrome_java.sources'

_PACKAGE_RE = re.compile(r'^package\s+(.*?)(;|\s*$)', flags=re.MULTILINE)
_STRIP_NESTED_RE = re.compile(r'\$.*')
_PACKAGE_FROM_NAME_RE = re.compile(r'(.*?)\.[A-Z]')


@dataclasses.dataclass(frozen=True)
class _JavaClass:
    path: str
    name: str
    null_marked: bool


def _read_file(path):
    return pathlib.Path(path).read_text()


def _analyze_java_file(path):
    data = _read_file(path)
    m = _PACKAGE_RE.search(data)
    package = m.group(1)
    name = os.path.splitext(os.path.basename(path))[0]
    null_marked = '@NullMarked' in data or '@NullUnmarked' in data
    return _JavaClass(path, f'{package}.{name}', null_marked)


def _package_from_name(clazz):
    return _PACKAGE_FROM_NAME_RE.match(clazz).group(1)


def _create_dep_graph():
    # dict of class -> set(referenced classes)
    class_graph = check_for_missing_direct_deps._ParseDepGraph(
        'obj/chrome/android/chrome_java.javac.jar')

    # Strip nested classes.
    ret = collections.defaultdict(set)
    for clazz, deps in class_graph.items():
        clazz = _STRIP_NESTED_RE.sub('', clazz)
        ret[clazz].update(_STRIP_NESTED_RE.sub('', d) for d in deps)
    return ret


def main():
    logging.basicConfig(format='%(message)s', level=logging.INFO)
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv', action='store_true')
    args = parser.parse_args()

    if not os.path.exists('args.gn'):
        parser.error('Must be run from within output directory.')
        sys.exit(1)

    all_paths = _read_file(_CHROME_JAVA_SOURCES).splitlines()
    all_classes = [_analyze_java_file(p) for p in all_paths]

    already_marked = {c.name for c in all_classes if c.null_marked}
    not_already_marked = [c.name for c in all_classes if not c.null_marked]

    logging.info('Marked: %d', len(already_marked))
    logging.info('Unmarked: %d', len(not_already_marked))

    # Find packages that reference only annotated other packages.
    dep_graph = _create_dep_graph()

    names_to_class = {x.name: x for x in all_classes}

    # class name -> set(class names they depend on that are in chrome_java)
    deps_by_name = collections.defaultdict(set)
    for name in not_already_marked:
        deps_by_name[name].update(c for c in dep_graph.get(name, [])
                                  if c != name and c in names_to_class)

    # Sort tuples of name -> deps by class name to try and keep them clustered.
    unmarked_items = sorted(x for x in deps_by_name.items()
                            if x[0] not in already_marked)
    current_unblocked = [
        x for x in unmarked_items if all(d in already_marked for d in x[1])
    ]
    id_set = {id(x) for x in current_unblocked}
    still_blocked = [x for x in unmarked_items if id(x) not in id_set]
    logging.info('Initially unblocked: %d', len(current_unblocked))

    # Keep appending classes if all deps are going to be annotated before them.
    future_marked = set(already_marked)
    future_marked.update(x[0] for x in current_unblocked)
    future_unblocked = []
    for i in range(20):
        newly_unblocked = [
            x for x in still_blocked if all(d in future_marked for d in x[1])
        ]
        logging.info('Unblocked in round %d: %d', i, len(newly_unblocked))
        if not newly_unblocked:
            # No more classes where all deps are unblocked (circular deps).
            break
        future_unblocked.extend(newly_unblocked)
        future_marked.update(x[0] for x in newly_unblocked)
        still_blocked = [x for x in still_blocked if x[0] not in future_marked]

    logging.info('Future unblocked: %d', len(future_unblocked))

    # Filter to just blocked deps.
    still_blocked = [(c, sorted(d for d in deps if d not in future_marked))
                     for c, deps in still_blocked]
    # Sort by smallest number of blocked deps.
    still_blocked.sort(key=lambda x: len(x[1]))
    logging.info('Classes with circular deps: %d', len(still_blocked))

    # TODO(agrieve): Try and find clusters of circuclar within still_blocked.
    # E.g. Sort by len(unique(deps + deps_of_deps + deps_of_deps_of_deps))
    # Then just try and add the first file and all recursive deps until all
    # files are seen.

    if args.csv:
        writer = csv.writer(sys.stdout)
        writer.writerow(('Path', 'Num Deps', 'Has Ciruclar Dep'))
        for name, _ in current_unblocked:
            clazz = names_to_class[name]
            writer.writerow((clazz.path.lstrip('/.'), 0, 'No'))
        for name, deps in future_unblocked:
            clazz = names_to_class[name]
            deps = [x for x in deps if x not in already_marked]
            writer.writerow((clazz.path.lstrip('/.'), len(deps), 'No'))
        for name, ciruclar_deps in still_blocked:
            clazz = names_to_class[name]
            writer.writerow(
                (clazz.path.lstrip('/.'), len(ciruclar_deps), 'Yes'))
        return

    print('Already Unblocked:')
    for name, deps in current_unblocked:
        print(name, len(deps))

    print('Future Unblocked:')
    for name, deps in future_unblocked:
        deps_not_already_null_marked = [
            x for x in deps if x not in already_marked
        ]
        print(name, len(deps), len(deps_not_already_null_marked))

    print('Circular Deps:')
    for name, deps in still_blocked:
        print(name, len(deps))


if __name__ == '__main__':
    main()
