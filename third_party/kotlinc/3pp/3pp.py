#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import shutil
import subprocess
import sys

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = next(p for p in _THIS_DIR.parents
                 if (p / 'build' / '3pp_common').is_dir())
sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))

import common
import fetch_github_release
import graalvm

_PROJECT = 'JetBrains/kotlin'
_ARTIFACT_REGEX = r'kotlin-compiler.*\.zip$'

_SAMPLE_KT = """\
package foo
data class Sample(val x: Int) { fun f() = listOf(x).map { it + 1 } }
// Traces the lookup of package-info.class files.
fun g() = java.net.URI("a")
"""


def do_latest():
    return fetch_github_release.latest_release(_PROJECT,
                                               artifact_regex=_ARTIFACT_REGEX)


def do_install(args):
    fetch_github_release.download_artifact(_PROJECT,
                                           args.version,
                                           'kotlinc.zip',
                                           artifact_regex=_ARTIFACT_REGEX)
    common.run_cmd(['unzip', '-q', 'kotlinc.zip'])
    for name in os.listdir('kotlinc'):
        shutil.move(os.path.join('kotlinc', name), args.output_prefix)

    # Build a native kotlinc for use by compile_kt.py.
    graal_home = graalvm.ensure_graalvm()
    pathlib.Path('Sample.kt').write_text(_SAMPLE_KT, encoding='utf-8')
    lib_dir = os.path.join(args.output_prefix, 'lib')
    abi_jar = os.path.join(lib_dir, 'jvm-abi-gen.jar')

    def sample_args(out_dir):
        return [
            '-kotlin-home',
            args.output_prefix,
            '-jdk-home',
            graal_home,
            '-Xjdk-release=25',
            # The scripting plugin is not compiled into the image.
            '-Xdisable-default-scripting-plugin',
            '-d',
            f'{out_dir}/classes',
            # Used by compile_kt.py to generate .interface.jar files.
            f'-Xplugin={abi_jar}',
            '-P',
            f'plugin:org.jetbrains.kotlin.jvm.abi:outputDir={out_dir}/abi',
            'Sample.kt',
        ]

    output_path = os.path.join(args.output_prefix, 'bin', 'kotlinc-graalvm')
    graalvm.build_native_image(
        os.path.join(lib_dir, 'kotlin-compiler.jar'),
        output_path,
        # Plugins cannot be loaded dynamically, so compile them in. kotlinc
        # finds their entry points via META-INF/services, which native-image
        # registers for reflection automatically.
        extra_cp=[
            abi_jar,
            os.path.join(lib_dir, 'compose-compiler-plugin.jar')
        ],
        tracing_args_list=[sample_args('tracing_out')],
        # Register all compiler argument methods for reflection (as upstream
        # JetBrains does) so CLI flags are supported without retracing.
        extra_reachability_metadata={
            'reflection': [
                {
                    'type':
                    'org.jetbrains.kotlin.cli.common.arguments.K2JVMCompilerArguments',
                    'allDeclaredMethods': True,
                },
                {
                    'type':
                    'org.jetbrains.kotlin.cli.common.arguments.CommonCompilerArguments',
                    'allDeclaredMethods': True,
                },
                {
                    'type':
                    'org.jetbrains.kotlin.cli.common.arguments.CommonToolArguments',
                    'allDeclaredMethods': True,
                },
                {
                    'type':
                    'org.jetbrains.kotlin.cli.common.arguments.Freezable',
                    'allDeclaredMethods': True,
                },
            ],
        },
        extra_native_image_args=[
            # kotlin-compiler.jar bundles jline's native-image.properties files,
            # but not the .json configs they point to, which fails the build.
            '--exclude-config',
            r'kotlin-compiler\.jar',
            r'META-INF/native-image/org\.jline/.*',
            # Silence JDK 24+ warnings (as bin/kotlinc does).
            '--enable-native-access=ALL-UNNAMED',
            '-J--sun-misc-unsafe-memory-access=allow',
            # Its <clinit> looks up charsets that are not included by default.
            '--initialize-at-build-time='
            'com.intellij.openapi.vfs.CharsetToolkit',
            # Enable jrt:/ filesystem support required by -jdk-home (as upstream
            # JetBrains does).
            '-H:+UnlockExperimentalVMOptions',
            '-H:+AllowJRTFileSystem',
        ],
    )

    # compile_kt.py fails on any output, so ensure there is none.
    output = common.run_cmd([output_path] + sample_args('smoke_out'),
                            env=dict(os.environ, JAVA_HOME=graal_home),
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT).stdout
    if output or not os.path.exists('smoke_out/abi/foo/Sample.class'):
        raise RuntimeError(f'kotlinc-graalvm smoke test failed:\n{output}')


def main():
    common.main(
        do_latest=do_latest,
        do_install=do_install,
    )


if __name__ == '__main__':
    main()
