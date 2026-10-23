#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Packages the Temurin JDK, plus (on Linux) a native-image build of javac."""

import os
import pathlib
import shutil
import subprocess
import sys
import zipfile

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = next(p for p in _THIS_DIR.parents
                 if (p / 'build' / '3pp_common').is_dir())
sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))

import common
import fetch_github_release
import graalvm

# Must match GraalVM's major version, since javac-graalvm mixes GraalVM's
# java.base with this JDK's jdk.compiler.
# LINT.IfChange(jdk_major_version)
_MAJOR_VERSION = '25'
# LINT.ThenChange(//build/3pp_common/graalvm.py:jdk_major_version)
_PROJECT = f'adoptium/temurin{_MAJOR_VERSION}-binaries'

# https://crbug.com/1441023
_JAVA_WRAPPER = """\
#!/bin/sh

# https://crbug.com/1441023
exec "$0.chromium" -XX:+PerfDisableSharedMem "$@"
"""

_SAMPLE_JAVA = """\
package foo;

import java.util.ArrayList;
import java.util.List;

/** Exercises javac's warning output. */
public class Sample {
  @SuppressWarnings("rawtypes")
  List mRaw = new ArrayList();

  @Deprecated
  public List<String> f() {
    List<String> ret = mRaw;
    return ret;
  }
}
"""

def _platform():
    return os.environ['_3PP_PLATFORM']


def _artifact_regex():
    platform = _platform()
    if platform == 'linux-amd64':
        return r'jdk_x64_linux_.*\.tar\.gz$'
    if platform == 'mac-arm64':
        return r'jdk_aarch64_mac_.*\.tar\.gz$'
    raise ValueError(f'Unsupported _3PP_PLATFORM: {platform!r}')


def _write_java_wrapper(bin_dir):
    java = os.path.join(bin_dir, 'java')
    os.rename(java, java + '.chromium')
    pathlib.Path(java).write_text(_JAVA_WRAPPER, encoding='utf-8')
    os.chmod(java, 0o755)


def _build_native_javac(jdk_home):
    """Builds bin/javac-graalvm using javac from |jdk_home|."""
    work_dir = os.path.abspath('native_javac')
    os.makedirs(work_dir)

    # Use javac from this JDK (rather than from the GraalVM JDK).
    jdk_compiler_dir = os.path.join(work_dir, 'modules')
    common.run_cmd([
        os.path.join(jdk_home, 'bin', 'jimage'),
        'extract',
        '--include',
        'regex:/jdk.compiler/.*',
        '--dir',
        jdk_compiler_dir,
        os.path.join(jdk_home, 'lib', 'modules'),
    ])
    jdk_compiler_dir = os.path.join(jdk_compiler_dir, 'jdk.compiler')
    # Not allowed by --patch-module.
    os.unlink(os.path.join(jdk_compiler_dir, 'module-info.class'))

    src_zip = os.path.join(jdk_home, 'lib', 'src.zip')

    sample_java = os.path.join(work_dir, 'Sample.java')
    pathlib.Path(sample_java).write_text(_SAMPLE_JAVA, encoding='utf-8')
    base_javac_args = ['-g', '-parameters', '-Xlint:all']
    javac_args = base_javac_args + ['--release', _MAJOR_VERSION]

    # Compiling javac itself is used as a representative workload for PGO.
    pgo_src_dir = os.path.join(work_dir, 'pgo_src')
    with zipfile.ZipFile(src_zip) as z:
        for name in z.namelist():
            if name.startswith(('jdk.compiler/', 'java.compiler/')):
                z.extract(name, pgo_src_dir)

    output_path = os.path.join(jdk_home, 'bin', 'javac-graalvm')
    graalvm.build_native_image(
        None,
        output_path,
        main_class='com.sun.tools.javac.Main',
        # Traced using GraalVM's own javac (rather than this JDK's), which is
        # fine so long as their major versions match.
        tracing_args_list=[
            javac_args +
            ['-d', os.path.join(work_dir, 'trace_out'), sample_java]
        ],
        pgo_args_list=[
            base_javac_args + [
                '-d',
                os.path.join(work_dir, 'pgo_out'),
                '--module-source-path',
                pgo_src_dir,
                '--module',
                'jdk.compiler',
            ]
        ],
        extra_reachability_metadata={
            'resources': [{
                'bundle': 'com.sun.tools.javac.resources.version'
            }],
        },
        # bin/javac-graalvm -> java.home is its parent directory.
        java_home_relative_to_executable='..',
        extra_native_image_args=[
            f'-J--patch-module=jdk.compiler={jdk_compiler_dir}',
            # Required to read lib/modules (for --release). graalvm.py makes
            # it read $java.home/lib/modules.
            '-H:+UnlockExperimentalVMOptions',
            '-H:+AllowJRTFileSystem',
        ],
    )

    # Ensure the binary works, and that it uses this JDK's lib/modules (even
    # when JAVA_HOME is set).
    smoke_out = os.path.join(work_dir, 'smoke_out')
    env = dict(os.environ, JAVA_HOME='/nonexistent')
    common.run_cmd([output_path] + javac_args + ['-d', smoke_out, sample_java],
                   env=env)
    if not os.path.exists(os.path.join(smoke_out, 'foo', 'Sample.class')):
        raise RuntimeError('javac-graalvm smoke test failed')
    expected_version = common.run_cmd(
        [os.path.join(jdk_home, 'bin', 'javac'), '-version'],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    ).stdout
    actual_version = common.run_cmd(
        [output_path, '-version'],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    ).stdout
    if actual_version != expected_version:
        raise RuntimeError(
            f'javac-graalvm -version: {actual_version!r} != {expected_version!r}'
        )


def do_latest():
    return fetch_github_release.latest_release(
        _PROJECT, artifact_regex=_artifact_regex())


def do_install(args):
    fetch_github_release.download_artifact(_PROJECT,
                                           args.version,
                                           'jdk.tar.gz',
                                           artifact_regex=_artifact_regex())
    common.extract_tar('jdk.tar.gz', 'jdk_extracted')
    # The archive contains a single root directory, e.g. "jdk-25.0.4.1+1".
    (root, ) = pathlib.Path('jdk_extracted').iterdir()

    if _platform() == 'mac-arm64':
        # Structure is "jdk-25.0.4.1+1/Contents/{Home,MacOS,...}".
        _write_java_wrapper(str(root / 'Contents' / 'Home' / 'bin'))
        shutil.move(str(root / 'Contents'), args.output_prefix)
        return

    # Structure is "jdk-25.0.4.1+1/{bin,conf,...}".
    for child in root.iterdir():
        shutil.move(str(child), args.output_prefix)
    _build_native_javac(args.output_prefix)
    # Done after building javac-graalvm since the wrapper is not needed there.
    _write_java_wrapper(os.path.join(args.output_prefix, 'bin'))


def main():
    common.main(
        do_latest=do_latest,
        do_install=do_install,
        # Used on all platforms (even though only linux uses GraalVM) since
        # DEPS requires the version to be identical across platforms.
        version_strings_func=graalvm.version_strings,
    )


if __name__ == '__main__':
    main()
