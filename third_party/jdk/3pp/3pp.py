#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Packages Temurin JDK, plus (on Linux) a native-image of javac/javap/jdeps."""

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
sys.path.insert(1, str(_THIS_DIR / 'launcher'))

import common
import fetch_github_release
import graalvm
import javac_launcher

# Must match GraalVM's major version, since jdk-tools-graalvm mixes GraalVM's
# java.base with this JDK's jdk.compiler and jdk.jdeps.
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

_JDK_TOOLS_MAIN_JAVA = """\
import java.util.Arrays;

public class JdkToolsMain {
  public static void main(String[] args) throws Exception {
    if (args.length == 0) {
      System.err.println("Usage: jdk-tools-graalvm <javac|javap|jdeps> [args...]");
      System.exit(2);
    }
    String[] toolArgs = Arrays.copyOfRange(args, 1, args.length);
    switch (args[0]) {
      case "javac":
        JavacLauncher.main(toolArgs);
        break;
      case "javap":
        com.sun.tools.javap.Main.main(toolArgs);
        break;
      case "jdeps":
        com.sun.tools.jdeps.Main.main(toolArgs);
        break;
      default:
        System.err.println("Unknown tool: " + args[0]);
        System.exit(2);
    }
  }
}
"""

_SAMPLE_JAVA = """\
package foo;

import java.util.ArrayList;
import java.util.List;

/** Exercises javac's warning output, plus javap and jdeps. */
public class Sample {
  public static final int CONST = 42;

  @SuppressWarnings("rawtypes")
  List mRaw = new ArrayList();

  @Deprecated
  public List<String> f() {
    List<String> ret = mRaw;
    return ret;
  }

  private native void nativeMethod(String s);
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


def _write_readme(output_prefix, license_file):
    template = (_THIS_DIR / 'README.chromium.template').read_text(
        encoding='utf-8')
    readme = (template.replace('$_JDK_MAJOR_VERSION', _MAJOR_VERSION).replace(
        '$_LICENSE_FILE', license_file))
    (pathlib.Path(output_prefix) / 'README.chromium').write_text(
        readme, encoding='utf-8')


def _zip_classes(src_dir, dest_jar):
    """Zips .class files under |src_dir| into |dest_jar|, returning entries."""
    src_path = pathlib.Path(src_dir)
    entries = []
    with zipfile.ZipFile(dest_jar, 'w') as z:
        for p in sorted(src_path.rglob('*.class')):
            rel = p.relative_to(src_path).as_posix()
            z.write(str(p), rel)
            if p.name not in ('package-info.class', 'module-info.class'):
                entries.append(rel)
    return entries


def _run_capture(cmd, env=None):
    res = common.run_cmd(
        cmd,
        check=False,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    if res.returncode:
        raise RuntimeError(
            f'Command {cmd} failed (exit {res.returncode}):\n{res.stdout}')
    return res.stdout


def _build_native_jdk_tools(jdk_home):
    """Builds bin/jdk-tools-graalvm (javac, javap, jdeps) from |jdk_home|."""
    work_dir = os.path.abspath('native_jdk_tools')
    os.makedirs(work_dir)

    # Use javac, javap, and jdeps from this JDK (not from GraalVM's JDK).
    modules_dir = os.path.join(work_dir, 'modules')
    common.run_cmd([
        os.path.join(jdk_home, 'bin', 'jimage'),
        'extract',
        '--include',
        r'regex:/(jdk\.compiler|jdk\.jdeps)/.*',
        '--dir',
        modules_dir,
        os.path.join(jdk_home, 'lib', 'modules'),
    ])
    jdk_compiler_dir = os.path.join(modules_dir, 'jdk.compiler')
    jdk_jdeps_dir = os.path.join(modules_dir, 'jdk.jdeps')
    # Not allowed by --patch-module.
    os.unlink(os.path.join(jdk_compiler_dir, 'module-info.class'))
    os.unlink(os.path.join(jdk_jdeps_dir, 'module-info.class'))

    # Compile the thin dispatcher class that routes to javac/javap/jdeps.
    add_exports = [
        'jdk.jdeps/com.sun.tools.javap',
        'jdk.jdeps/com.sun.tools.jdeps',
    ] + javac_launcher.JAVAC_EXPORTS
    main_src = os.path.join(work_dir, 'JdkToolsMain.java')
    main_classes_dir = os.path.join(work_dir, 'main_classes')
    javac_launcher.compile_launcher(
        os.path.join(jdk_home, 'bin', 'javac'), main_classes_dir)
    pathlib.Path(main_src).write_text(_JDK_TOOLS_MAIN_JAVA, encoding='utf-8')
    common.run_cmd(
        [
            os.path.join(jdk_home, 'bin', 'javac'),
            '--add-modules=jdk.jdeps',
            '-cp',
            main_classes_dir,
        ] + [f'--add-exports={exp}=ALL-UNNAMED' for exp in add_exports] +
        ['-d', main_classes_dir, main_src])

    sample_java = os.path.join(work_dir, 'Sample.java')
    pathlib.Path(sample_java).write_text(_SAMPLE_JAVA, encoding='utf-8')
    base_javac_args = ['-g', '-parameters', '-Xlint:all']
    javac_args = base_javac_args + ['--release', _MAJOR_VERSION]

    # Pre-compile Sample.java for javap/jdeps tracing & smoke tests.
    sample_classes_dir = os.path.join(work_dir, 'sample_classes')
    common.run_cmd([os.path.join(jdk_home, 'bin', 'javac')] + javac_args +
                   ['-d', sample_classes_dir, sample_java])
    sample_class = os.path.join(sample_classes_dir, 'foo', 'Sample.class')
    sample_jar = os.path.join(work_dir, 'sample.jar')
    _zip_classes(sample_classes_dir, sample_jar)
    sample_jar_url = f'jar:file://{sample_jar}!/foo/Sample.class'

    # Compiling javac itself and inspecting its classes are representative
    # workloads for PGO.
    src_zip = os.path.join(jdk_home, 'lib', 'src.zip')
    pgo_src_dir = os.path.join(work_dir, 'pgo_src')
    with zipfile.ZipFile(src_zip) as z:
        for name in z.namelist():
            if name.startswith(('jdk.compiler/', 'java.compiler/')):
                z.extract(name, pgo_src_dir)
    pgo_jar = os.path.join(work_dir, 'pgo_jdk_compiler.jar')
    pgo_class_entries = _zip_classes(jdk_compiler_dir, pgo_jar)

    jdeps_cmd = [
        'jdeps',
        '-verbose:class',
        '-filter:none',
        '--multi-release',
        'base',
    ]

    def sample_tool_cmds(class_file):
        return [
            ['javap', '-constants', sample_jar_url],
            ['javap', '-private', '-cp', sample_jar, 'foo.Sample'],
            ['javap', '-private', '-c', class_file],
            ['javap', '-protected', class_file],
            jdeps_cmd + [sample_jar],
        ]

    output_path = os.path.join(jdk_home, 'bin', 'jdk-tools-graalvm')
    graalvm.build_native_image(
        None,
        output_path,
        main_class='JdkToolsMain',
        extra_cp=[main_classes_dir],
        add_exports=add_exports,
        # Traced using GraalVM's own jdk.compiler/jdk.jdeps (rather than this
        # JDK's), which is fine so long as their major versions match.
        tracing_args_list=(
            [[
                'javac',
                '-XDcrFastClassPath',
                '-XDcrClassPathUsageOut=' +
                os.path.join(work_dir, 'trace.usage.txt'),
            ] + javac_args + [
                '-cp',
                sample_jar,
                '-d',
                os.path.join(work_dir, 'trace_out'),
                sample_java,
            ]] + sample_tool_cmds(sample_class)),
        pgo_args_list=[
            ['javac', '-XDcrFastClassPath'] + base_javac_args + [
                '-d',
                os.path.join(work_dir, 'pgo_out'),
                '--module-source-path',
                pgo_src_dir,
                '--module',
                'jdk.compiler',
            ],
            ['javac', '-XDcrFastClassPath'] + javac_args + [
                '-cp',
                os.pathsep.join([pgo_jar, sample_jar]),
                '-d',
                os.path.join(work_dir, 'pgo_cp_out'),
                sample_java,
            ],
            # Sample both jar:file:// URLs (jni_zero) and loose .class files
            # (Cronet) rather than `-cp <jar> <class>` which javap resolves
            # from jrt:/jdk.compiler/ instead of the jar. Striding across the
            # module exercises concrete com.sun.tools.javac.* classes with
            # bytecode and constants rather than just leading AST interfaces.
            ['javap', '-private', '-constants', '-c'] + [
                f'jar:file://{pgo_jar}!/{e}' for e in pgo_class_entries[::32]
            ] + [
                os.path.join(jdk_compiler_dir, e)
                for e in pgo_class_entries[16::32]
            ],
            jdeps_cmd + [pgo_jar],
        ],
        extra_reachability_metadata={
            'resources': [
                {'bundle': 'com.sun.tools.javac.resources.version'},
                {'bundle': 'com.sun.tools.javap.resources.javap'},
                {'bundle': 'com.sun.tools.javap.resources.version'},
                {'bundle': 'com.sun.tools.jdeps.resources.jdeps'},
                {'bundle': 'com.sun.tools.jdeps.resources.jdkinternals'},
                {'bundle': 'com.sun.tools.jdeps.resources.version'},
            ],
        },
        # bin/jdk-tools-graalvm -> java.home is its parent directory.
        java_home_relative_to_executable='..',
        check_workloads=True,
        extra_native_image_args=[
            f'-J--patch-module=jdk.compiler={jdk_compiler_dir}',
            f'-J--patch-module=jdk.jdeps={jdk_jdeps_dir}',
            # Needed for javap jar:file://.../foo.jar!/Bar.class URLs.
            '--enable-url-protocols=jar',
            # JdepsConfiguration$SystemModuleFinder caches java.home in a
            # static final field; defer initialization until GraalVmWrapper
            # sets java.home at run time.
            '--initialize-at-run-time='
            'com.sun.tools.jdeps.JdepsConfiguration$SystemModuleFinder',
            # Required to read lib/modules (for --release and jdeps).
            # graalvm.py makes it read $java.home/lib/modules.
            '-H:+UnlockExperimentalVMOptions',
            '-H:+AllowJRTFileSystem',
        ],
    )

    # Ensure all bundled tools work, and that they use this JDK's lib/modules
    # (even when JAVA_HOME is set and .graalvm is absent, as on CIPD clients).
    graalvm_dir = os.path.abspath('.graalvm')
    hidden_graalvm_dir = graalvm_dir + '.hidden'
    renamed_graalvm = os.path.isdir(graalvm_dir)
    if renamed_graalvm:
        os.rename(graalvm_dir, hidden_graalvm_dir)
    try:
        smoke_out = os.path.join(work_dir, 'smoke_out')
        env = dict(os.environ, JAVA_HOME='/nonexistent')
        common.run_cmd(
            [output_path, 'javac'] + javac_args +
            ['-d', smoke_out, sample_java],
            env=env)
        smoke_class = os.path.join(smoke_out, 'foo', 'Sample.class')
        if not os.path.exists(smoke_class):
            raise RuntimeError('jdk-tools-graalvm javac smoke test failed')

        javac_launcher.smoke_test(
            [output_path, 'javac', '--release', _MAJOR_VERSION],
            work_dir,
            env=env,
        )

        smoke_cmds = (
            [[t, '-version'] for t in ('javac', 'javap', 'jdeps')] +
            sample_tool_cmds(smoke_class))
        for cmd in smoke_cmds:
            expected = _run_capture(
                [os.path.join(jdk_home, 'bin', cmd[0])] + cmd[1:])
            actual = _run_capture([output_path] + cmd, env=env)
            if actual != expected:
                raise RuntimeError(
                    f'jdk-tools-graalvm {cmd} output mismatch:\n'
                    f'--- expected ---\n{expected}\n--- actual ---\n{actual}')
    finally:
        if renamed_graalvm:
            os.rename(hidden_graalvm_dir, graalvm_dir)


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
        _write_readme(args.output_prefix, 'Contents/Home/legal/java.base/LICENSE')
        return

    # Structure is "jdk-25.0.4.1+1/{bin,conf,...}".
    for child in root.iterdir():
        shutil.move(str(child), args.output_prefix)
    _build_native_jdk_tools(args.output_prefix)
    # Done after building jdk-tools-graalvm since the wrapper isn't needed.
    _write_java_wrapper(os.path.join(args.output_prefix, 'bin'))
    _write_readme(args.output_prefix, 'legal/java.base/LICENSE')


def main():
    common.main(
        do_latest=do_latest,
        do_install=do_install,
        version_deps=[
            '//third_party/jdk/3pp/README.chromium.template',
            '//third_party/jdk/3pp/launcher',
        ],
        # Used on all platforms (even though only linux uses GraalVM) since
        # DEPS requires the version to be identical across platforms.
        version_strings_func=graalvm.version_strings,
    )


if __name__ == '__main__':
    main()
