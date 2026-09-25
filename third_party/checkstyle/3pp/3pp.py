#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import sys
import zipfile

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = next(p for p in _THIS_DIR.parents
                 if (p / 'build' / '3pp_common').is_dir())
sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))

import common
import fetch_github_release
import graalvm

_PROJECT = 'checkstyle/checkstyle'
_ARTIFACT_REGEX = r'-all.*\.jar$'


def _extract_checkstyle_metadata(jar_path):
    """Collects reflection and resource metadata for checkstyle modules."""
    reflection = []
    resources = []
    with zipfile.ZipFile(jar_path) as z:
        for name in z.namelist():
            if not name.startswith('com/puppycrawl/tools/checkstyle/'):
                continue
            if name.endswith('.class'):
                if '/gui/' not in name and not name.endswith('/Main.class'):
                    cls_name = name[:-6].replace('/', '.')
                    reflection.append({
                        'type': cls_name,
                        'allDeclaredConstructors': True,
                        'allPublicMethods': True,
                        'allPublicFields': True,
                    })
            elif not name.endswith('/'):
                resources.append({'glob': name})
                if name.endswith('messages.properties'):
                    bundle = name[:-len('.properties')].replace('/', '.')
                    resources.append({'bundle': bundle})
    return {'reflection': reflection, 'resources': resources}


def do_latest():
    return fetch_github_release.latest_release(_PROJECT,
                                               artifact_regex=_ARTIFACT_REGEX)


def do_install(args):
    jar_path = os.path.join(args.output_prefix, 'checkstyle-all.jar')
    fetch_github_release.download_artifact(
        _PROJECT,
        args.version,
        jar_path,
        artifact_regex=_ARTIFACT_REGEX,
    )

    style_xml = common.path_within_checkout(
        'tools/android/checkstyle/chromium-style-5.0.xml')
    # chromium-style-5.0.xml references tools/android/checkstyle/suppressions.xml
    # relative to CWD. Write a minimal stub so we don't need suppressions.xml in
    # runtime_deps (which would rebuild checkstyle on every suppression change).
    suppressions_xml = pathlib.Path(
        'tools/android/checkstyle/suppressions.xml')
    suppressions_xml.parent.mkdir(parents=True, exist_ok=True)
    suppressions_xml.write_text(
        '<!DOCTYPE suppressions PUBLIC '
        '"-//Puppy Crawl//DTD Suppressions 1.1//EN" '
        '"http://www.puppycrawl.com/dtds/suppressions_1_1.dtd">\n'
        '<suppressions><suppress checks="Foo" files="Bar"/></suppressions>\n',
        encoding='utf-8',
    )
    sample_java = os.path.abspath('Sample.java')
    pathlib.Path(sample_java).write_text(
        'package foo;\nimport java.util.List;\npublic class Sample {}\n',
        encoding='utf-8',
    )

    output_bin = os.path.join(args.output_prefix, 'checkstyle')
    graalvm.build_native_image(
        jar_path,
        output_bin,
        tracing_args_list=[['-c', style_xml, '-f', 'xml', sample_java]],
        extra_reachability_metadata=_extract_checkstyle_metadata(jar_path),
    )

    # Validate the compiled binary against Chromium's checkstyle config.
    valid_java = os.path.abspath('ValidSample.java')
    pathlib.Path(valid_java).write_text(
        'package org.chromium;\n\n'
        '/** Valid sample class. */\n'
        'public class ValidSample {}\n',
        encoding='utf-8',
    )
    common.run_cmd([output_bin, '-c', style_xml, '-f', 'xml', valid_java])


def main():
    common.main(
        do_latest=do_latest,
        do_install=do_install,
        runtime_deps=[
            '//tools/android/checkstyle/chromium-style-5.0.xml',
        ],
    )


if __name__ == '__main__':
    main()
