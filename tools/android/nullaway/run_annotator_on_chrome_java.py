#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys

_SRC_ROOT = pathlib.Path(__file__).parents[3]
sys.path.insert(1, str(_SRC_ROOT / 'build/android/gyp'))

from util import build_utils
import action_helpers

_ANNOTATOR_JAR = ('../NullAwayAnnotator/annotator-core/build/libs/'
                  'annotator-core-1.3.16-SNAPSHOT.jar')
_CHROME_JAVA_TURBINE_JAR = 'obj/chrome/android/chrome_java.turbine.jar'
_NULLAWAY_JAR = (
    '../../third_party/android_build_tools/nullaway/cipd/nullaway.jar')


def _read_file(path):
    return pathlib.Path(path).read_text()


def _write_file(path, data):
    return pathlib.Path(path).write_text(data)


def _find_unmarked(java_files):
    ret = set()
    for path in java_files:
        data = _read_file(path)
        if '@NullUnmarked' in data or '@SuppressWarnings("NullAway' in data:
            ret.add(path)
    return ret


def _read_build_config_value(path, key):
    value = build_utils.ExpandFileArgs([f'@FileArg({path}:{key})'])[0]
    return action_helpers.parse_gn_list(value)


def prep_errorprone_run(enable_annotator, parser):
    if enable_annotator:
        if not os.path.exists(_ANNOTATOR_JAR):
            parser.error('Annotator .jar not found. Follow steps to build it.')
    if not os.path.exists('args.gn'):
        parser.error('Must be run from within output directory.')
    if not os.path.exists(_CHROME_JAVA_TURBINE_JAR):
        parser.error('Run "autoninja chrome/android:chrome_java" first.')

    # Compile only files that are @NullMarked (to speed things up).
    java_files = _read_file(
        'gen/chrome/android/chrome_java.sources').splitlines()
    java_files = [p for p in java_files if '@NullMarked' in _read_file(p)]
    sources_path = 'null-away-chrome-java-sources.txt'
    _write_file(sources_path, '\n'.join(java_files))

    classpath = [
        'obj/third_party/android_sdk/android_sdk_java.ijar.jar',
        _CHROME_JAVA_TURBINE_JAR,
    ]
    classpath += _read_build_config_value(
        'gen/chrome/android/chrome_java.javac.build_config.json',
        'javac_full_interface_classpath')

    processor_path = [_NULLAWAY_JAR] + _read_build_config_value(
        'gen/tools/android/errorprone_plugin/errorprone_plugin.build_config.json',
        'processed_classpath')
    if enable_annotator:
        processor_path.append(_ANNOTATOR_JAR)

    contract_annotations = [
        'org.chromium.build.annotations.Contract',
        'org.chromium.support_lib_boundary.util.Contract',
    ]
    init_methods = [
        'android.app.Application.onCreate',
        'android.app.Activity.onCreate',
        'android.app.Service.onCreate',
        'android.app.backup.BackupAgent.onCreate',
        'android.content.ContentProvider.attachInfo',
        'android.content.ContentProvider.onCreate',
        'android.content.ContextWrapper.attachBaseContext',
        'androidx.preference.PreferenceFragmentCompat.onCreatePreferences',
    ]
    errorprone_args = [
        '-Xplugin:ErrorProne',
        '-XepDisableAllChecks',
        '-Xep:NullAway:ERROR',
        '-XepOpt:NullAway:OnlyNullMarked',
        '-XepOpt:NullAway:CustomContractAnnotations=' +
        ','.join(contract_annotations),

        # TODO(agrieve): Re-enable once we sort out nullability of
        #     ObservableSuppliers. https://crbug.com/430320400
        #'-XepOpt:NullAway:CastToNonNullMethod=org.chromium.build.NullUtil.assumeNonNull',
        '-XepOpt:NullAway:AssertsEnabled=true',
        '-XepOpt:NullAway:AcknowledgeRestrictiveAnnotations=true',
        '-XepOpt:Nullaway:AcknowledgeAndroidRecent=true',
        '-XepOpt:NullAway:JSpecifyMode=true',
        '-XepOpt:NullAway:KnownInitializers=' + ','.join(init_methods),
    ]
    if enable_annotator:
        errorprone_args += [
            '-XepOpt:NullAway:SerializeFixMetadata=true',
            '-XepOpt:NullAway:FixSerializationConfigPath=../nullaway_config.xml',
            '-Xep:AnnotatorScanner:ERROR',
            '-XepOpt:AnnotatorScanner:ConfigPath=../nullaway_scanner.xml',
        ]

    javac_cmd = [
        '../../third_party/jdk/current/bin/javac', '-g', '-parameters',
        '--release', '17', '-encoding', 'UTF-8', '-sourcepath', ':',
        '-Xlint:-dep-ann', '-Xlint:-removal', '-J-XX:+PerfDisableSharedMem',
        '-Xmaxerrs', '100000', '-Xmaxwarns', '100000',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.api=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.file=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.main=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.model=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.parser=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.processing=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED',
        '-J--add-exports=jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED',
        '-J--add-opens=jdk.compiler/com.sun.tools.javac.code=ALL-UNNAMED',
        '-J--add-opens=jdk.compiler/com.sun.tools.javac.comp=ALL-UNNAMED',
        ' '.join(errorprone_args), '-XDcompilePolicy=simple',
        '-XDshould-stop.ifError=FLOW', '-XDshould-stop.ifNoError=FLOW',
        '-processorpath', ':'.join(processor_path), '-d',
        'nullaway-annotator-output', '-classpath', ':'.join(classpath),
        f'@{sources_path}'
    ]

    return java_files, javac_cmd


def main():
    logging.basicConfig(format='%(message)s', level=logging.INFO)
    parser = argparse.ArgumentParser()
    parser.add_argument('--loud',
                        action='store_true',
                        help='Print compiler while annotating output')
    args = parser.parse_args()

    java_files, javac_cmd = prep_errorprone_run(True, parser)
    logging.info('Running annotator over %d @NullMarked files in chrome_java',
                 len(java_files))
    logging.info('This will probably take 3-5 minutes 🐢🐢🐢')

    unmarked_files_before = _find_unmarked(java_files)

    outdir = os.path.abspath('annotator-out')
    if os.path.exists(outdir):
        shutil.rmtree(outdir)

    compile_script = f'nullaway-annotator-compile.sh'
    compile_logs = f'nullaway-annotator-compile.log'
    if os.path.exists(compile_logs):
        os.unlink(compile_logs)

    _write_file(
        compile_script, f"""\
#!/bin/bash
set -e
echo -e "\n\n============= START OF COMPILE =============" >> {compile_logs}
{shlex.join(javac_cmd)} 2>&1 | tee -a {compile_logs}
""")
    os.chmod(compile_script, 0o744)

    cmd = build_utils.JavaCmd() + [
        '-jar', _ANNOTATOR_JAR, '-bc', f'./{compile_script}', '-n',
        'org.chromium.build.annotations.Nullable', '-d', outdir, '-cp',
        '../nullaway_config.tsv', '-cn', 'NULLAWAY', '-sre',
        'org.chromium.build.annotations.NullUnmarked', '-i',
        'org.chromium.build.annotations.Initializer'
    ]
    if args.loud:
        cmd += ['--redirect-build-output-stderr']

    result = subprocess.run(cmd).returncode
    logging.warning('🪵 Error Prone output (find warnings here):\n%s\n',
                    os.path.abspath(compile_logs))

    if result:
        print('😰 Command failed.')
        sys.exit(result)

    unmarked_files_after = _find_unmarked(java_files)

    unmarked_files = sorted(unmarked_files_after - unmarked_files_before)
    if unmarked_files:
        for path in unmarked_files:
            data = _read_file(path)
            # Leave a whitespace change as a hint it was nullunmarked.
            data = data.replace('@NullUnmarked ', '\t')
            data = data.replace('@SuppressWarnings("NullAway.Init")', '\t')
            _write_file(path, data)

        print("""
The following files have unresolved warnings, which have been marked by \\t \
characters.

Files:""")
        print('\n'.join(unmarked_files))
    else:
        print('No suppressions remained after auto-annotating.')


if __name__ == '__main__':
    main()
