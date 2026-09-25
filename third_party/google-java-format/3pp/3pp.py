#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import sys

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = next(p for p in _THIS_DIR.parents
                 if (p / 'build' / '3pp_common').is_dir())
sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))

import common
import fetch_github_release
import graalvm

_PROJECT = 'google/google-java-format'
_ARTIFACT_REGEX = r'-all-deps\.jar$'
_RAW_BASE_URL = (
    'https://raw.githubusercontent.com/google/google-java-format/'
    '{version}/core/src/main/java/com/google/googlejavaformat/java')

_SAMPLE_JAVA = """\
package org.chromium.sample;

import java.util.List;
import org.chromium.build.annotations.Nullable;

/** Sample javadoc comment. */
public class Sample {
  public @Nullable String format(int x, List<String> items) {
    return switch (x) {
      case 1 -> items.isEmpty() ? null : items.get(0);
      default -> "default";
    };
  }
}
"""


def _apply_replace(path, target, replacement):
    text = path.read_text(encoding='utf-8')
    if target not in text:
        raise RuntimeError(f'Failed to find {target!r} in {path}')
    path.write_text(text.replace(target, replacement, 1), encoding='utf-8')


def _build_overrides_jar(version, jar_path):
    """Fetches upstream sources, applies patches, and updates jar_path."""
    graal_home = graalvm.ensure_graalvm()
    javac_bin = os.path.join(graal_home, 'bin', 'javac')
    jar_bin = os.path.join(graal_home, 'bin', 'jar')

    src_dir = pathlib.Path('overrides_src')
    src_dir.mkdir(exist_ok=True)

    import_java = src_dir / 'ImportOrderer.java'
    common.download_file(
        f'{_RAW_BASE_URL.format(version=version)}/ImportOrderer.java',
        str(import_java),
    )
    _apply_replace(
        import_java,
        '.thenComparing(Import::isThirdParty, trueFirst())',
        '.thenComparing(Import::isThirdParty, trueFirst())\n'
        '          .thenComparing(Import::isChromium, trueFirst())',
    )
    _apply_replace(
        import_java,
        'if (prev.isAndroid() && !curr.isAndroid()) {\n'
        '      return true;\n'
        '    }',
        'if (prev.isAndroid() && !curr.isAndroid()) {\n'
        '      return true;\n'
        '    }\n'
        '    if (prev.isChromium() != curr.isChromium()) {\n'
        '      return true;\n'
        '    }',
    )
    _apply_replace(
        import_java,
        'return !(isAndroid() || isJava());',
        'return !(isAndroid() || isJava() || isChromium());\n'
        '    }\n\n'
        '    /** True if this is a Chromium import per AOSP style. */\n'
        '    boolean isChromium() {\n'
        '      return java.util.stream.Stream.of(\n'
        '              "org.chromium.", "com.google.android.apps.chrome")\n'
        '          .anyMatch(imported::startsWith);',
    )

    visitor_java = src_dir / 'JavaInputAstVisitor.java'
    common.download_file(
        f'{_RAW_BASE_URL.format(version=version)}/JavaInputAstVisitor.java',
        str(visitor_java),
    )
    _apply_replace(
        visitor_java,
        '"org.jspecify.annotations.NonNull",',
        '"org.chromium.build.annotations.Nullable",\n'
        '                        "org.jspecify.annotations.NonNull",',
    )

    classes_dir = pathlib.Path('overrides_classes')
    classes_dir.mkdir(exist_ok=True)
    common.run_cmd([
        javac_bin,
        '-d',
        str(classes_dir),
        '--add-exports=jdk.compiler/com.sun.tools.javac.api=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.code=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.file=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.parser=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.main=ALL-UNNAMED',
        '--add-exports=jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED',
        '--add-exports=jdk.internal.opt/jdk.internal.opt=ALL-UNNAMED',
        # Setting JVM version doesn't matter for graalvm binary, but does
        # matter when google-java-format.jar is used (e.g. on non-Linux hosts).
        '-source',
        '21',
        '-target',
        '21',
        '-cp',
        jar_path,
        str(import_java),
        str(visitor_java),
    ])

    common.run_cmd([
        jar_bin,
        '--update',
        '--file',
        jar_path,
        '-C',
        str(classes_dir),
        '.',
    ])


def do_latest():
    return fetch_github_release.latest_release(_PROJECT,
                                               artifact_regex=_ARTIFACT_REGEX)


def do_install(args):
    jar_path = os.path.join(args.output_prefix, 'google-java-format.jar')
    fetch_github_release.download_artifact(
        _PROJECT,
        args.version,
        jar_path,
        artifact_regex=_ARTIFACT_REGEX,
    )

    _build_overrides_jar(args.version, jar_path)
    sample_java = os.path.abspath('Sample.java')
    pathlib.Path(sample_java).write_text(_SAMPLE_JAVA, encoding='utf-8')

    graalvm.build_native_image(
        jar_path,
        os.path.join(args.output_prefix, 'google-java-format'),
        tracing_args_list=[['--aosp', sample_java]],
    )


def main():
    common.main(
        do_latest=do_latest,
        do_install=do_install,
    )


if __name__ == '__main__':
    main()
