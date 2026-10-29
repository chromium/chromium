#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json
import os
import pathlib
import shlex
import shutil
import subprocess
import sys
import urllib.request
import zipfile

# Outside docker: //path/to/project/3pp/fetch.py
# Inside docker: //path/to/project/install.py
_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parents[2 if _THIS_DIR.name == '3pp' else 1]

sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))
import common
import graalvm

# If false we roll to the latest branch, otherwise we roll to the latest commit.
_ROLL_TO_TIP_OF_TREE = False

# I have arbitrarily chosen 100 as a number much more than the number of commits
# I expect to see in a day in R8.
_COMMITS_URL = 'https://r8.googlesource.com/r8/+log/HEAD~100..HEAD?format=JSON'
_ARCHIVE_URL = 'https://r8.googlesource.com/r8/+archive/{}.tar.gz'
_DEPOT_TOOLS_URL = ('https://chromium.googlesource.com/chromium/tools/'
                    'depot_tools/+archive/main.tar.gz')
_BRANCHES_URL = 'https://r8-review.googlesource.com/projects/r8/branches?r=%5B1-9%5D.*'
_BRANCH_NAME_PREFIX = 'refs/heads/'

_CHROME_D8_SRC = 'third_party/r8/java/src/org/chromium/build/ChromeD8.java'
_CHROME_R8_SRC = 'third_party/r8/java/src/org/chromium/build/ChromeR8.java'
_FLUSHING_RETRACE_SRC = (
    'build/android/stacktrace/java/org/chromium/build/FlushingReTrace.java')
# Path within the checkout. Cannot be under third_party/r8, since the 3pp recipe
# copies the package definition (this directory) to .3pp/chromium/third_party/r8
# after "checkout" runs, and fails if the directory already exists.
_CHROME_D8_CHECKOUT_SUBPATH = 'r8_custom/ChromeD8.java'
_CHROME_R8_CHECKOUT_SUBPATH = 'r8_custom/ChromeR8.java'
_FLUSHING_RETRACE_CHECKOUT_SUBPATH = 'r8_custom/FlushingReTrace.java'
_CHROME_D8_MAIN_CLASS = 'org.chromium.build.ChromeD8'
_CHROME_R8_MAIN_CLASS = 'org.chromium.build.ChromeR8'
# Lowest min_sdk_version any Chromium target builds with; the globals dex must
# cover it. Any gn arg min_sdk_version >= this works with the same dex.
# LINT.IfChange(min_supported_sdk_version)
_MIN_SUPPORTED_SDK_VERSION = '23'
# LINT.ThenChange(//build/config/android/config.gni:min_supported_sdk_version)
# Uses default methods, static interface methods, and lambdas so that tracing
# covers desugaring.
_SAMPLE_IFACE_JAVA = """\
package org.chromium.sample;

public interface Iface {
  default String greet() { return "hi" + helper(); }
  static String helper() { return "!"; }
}
"""
_SAMPLE_JAVA = """\
package org.chromium.sample;

import java.util.function.Supplier;

public class Sample implements Iface {
  public int mField;
  public Supplier<String> supplier() { return () -> greet(); }
  public static void main(String[] args) {
    System.out.println(new Sample().supplier().get() + new Point(1, 2));
  }
}
"""
# Record desugaring emits a global synthetic (RecordTag), which --intermediate
# dexing rejects unless --ignore-globals is passed.
_SAMPLE_RECORD_JAVA = """\
package org.chromium.sample;

public record Point(int x, int y) {}
"""
_SAMPLE_MAPPING = """\
# compiler: R8
# compiler_version: 8.0
# {"id":"com.android.tools.r8.mapping","version":"2.2"}
org.chromium.sample.Sample -> a.a:
# {"id":"sourceFile","fileName":"Sample.java"}
    int[] mField -> a
      # {"id":"com.android.tools.r8.residualsignature","signature":"[Ljava/lang/Object;"}
    1:3:void inlined():20:22 -> a
    1:3:void greet(int):10 -> a
      # {"id":"com.android.tools.r8.synthesized"}
      # {"id":"com.android.tools.r8.rewriteFrame","conditions":["throws(Ljava/lang/NullPointerException;)"],"actions":["removeInnerFrames(1)"]}
    4:6:void outlined():30:32 -> b
      # {"id":"com.android.tools.r8.outline"}
    7:9:void caller():40:42 -> c
      # {"id":"com.android.tools.r8.outlineCallsite","positions":{"4":41},"outline":"La/a;b()V"}
"""
_SAMPLE_STACKTRACE = """\
Caused by: a.a: Message
\tat a.a.a(PG:2)
Caused by: java.lang.NullPointerException: Attempt to read from field 'int[] a.a.a' on a null object reference
"""


# Examples of version strings:
# 1.8, 2.5.10, 3.7-dev-aosp
def version_tuple(version):
    suffix = None
    if '-' in version:
        version, suffix = version.split('-', 1)
    version_segments = []
    for split in version.split('.'):
        try:
            version_segments.append(int(split))
        except:
            version_segments.append(split)
    if len(version_segments) < 3:
        version_segments += [0] * (3 - len(version_segments))
    return (*version_segments, suffix)


def get_latest_dev_branch():
    response_string = urllib.request.urlopen(_BRANCHES_URL).read()
    # JSON response has a XSSI protection prefix )]}'
    parsed_response = json.loads(response_string.lstrip(b")]}'\n"))
    branch_revisions = {}
    for branch_info in parsed_response:
        # ref string looks like refs/heads/1.7
        branch_name = branch_info['ref'][len(_BRANCH_NAME_PREFIX):]
        branch_revisions[version_tuple(branch_name)] = branch_info['revision']
    latest_version = sorted(branch_revisions.keys())[-1]
    return branch_revisions[latest_version]


def get_commit_before_today():
    """Returns hash of last commit that occurred before today (in Hawaii time)."""
    # We are doing UTC-10 (Hawaii time) since midnight there is roughly equal to a
    # time before North America starts working (where much of the chrome build
    # team is located), and is at least partially through the workday for Denmark
    # (where much of the R8 team is located). This ideally allows 3pp to catch
    # R8's work faster, instead of doing UTC where we would typically have to wait
    # ~18 hours to get R8's latest changes.
    desired_timezone = datetime.timezone(-datetime.timedelta(hours=10))
    today = datetime.datetime.now(desired_timezone).date()
    # Response looks like:
    # {
    # "log": [
    #   {
    #     "commit": "90f65f835fa73e2de31eafd6784cc2a0580dbe80",
    #     "committer": {
    #       "time": "Mon May 02 11:18:05 2022 +0000"
    #       ...
    #     },
    #     ...
    #   },
    #   ...
    # ]}
    response_string = urllib.request.urlopen(_COMMITS_URL).read()
    # JSON response has a XSSI protection prefix )]}'
    parsed_response = json.loads(response_string.lstrip(b")]}'\n"))
    log = parsed_response['log']
    for commit in log:
        # These times are formatted as a ctime() plus a time zone at the end.
        # "Mon May 02 13:09:58 2022 +0200"
        ctime_string = commit['committer']['time']
        commit_time = datetime.datetime.strptime(ctime_string,
                                                 "%a %b %d %H:%M:%S %Y %z")
        normalized_commit_time = commit_time.astimezone(desired_timezone)

        # We are assuming that the commits are given in order of committed time.
        # This appears to be true, but I can't find anywhere that this is
        # guaranteed.
        if normalized_commit_time.date() < today:
            # This is the first commit we can find before today.
            return commit['commit']

    # Couldn't find any commits before today - should probably change the
    # _COMMITS_URL to get more than 100 commits.
    return None


def _install(version, output_prefix):
    temp_file_path = 'archive.tar.gz'
    common.download_file(_ARCHIVE_URL.format(version), temp_file_path)
    common.extract_tar(temp_file_path, '.')
    common.download_file(_DEPOT_TOOLS_URL, temp_file_path)
    common.extract_tar(temp_file_path, 'depot_tools')

    os.environ['PATH'] += os.path.pathsep + os.path.abspath('depot_tools')

    # R8's gradle build (d8_r8/commonBuildSrc/.../R8SettingsPlugin.kt) enables
    # its GCS remote build cache whenever SWARMING_BOT_ID is set, which the 3pp
    # bot's service account cannot read (403 on bucket r8-build-cache).
    # Also isolate GRADLE_USER_HOME and pass --no-daemon because 3pp runs with
    # no_docker_env: true on persistent Swarming hosts; otherwise Gradle can
    # connect to a warm daemon from an earlier build whose checkout directory
    # (and third_party/gradle/lib/*.jar) has already been deleted.
    gradle_env = {
        k: v
        for k, v in os.environ.items() if k != 'SWARMING_BOT_ID'
    }
    gradle_env['GRADLE_USER_HOME'] = os.path.abspath('gradle_user_home')
    common.run_cmd(['tools/gradle.py', '--no-daemon', 'r8'], env=gradle_env)

    # Shrink (improves r8/d8 launch time):
    # Needs the -D flag to avoid compilation error, see http://b/311202383.
    java_home = graalvm.ensure_graalvm()
    common.run_cmd(
        shlex.split(f"""
        {java_home}/bin/java
            -Dcom.android.tools.r8.enableKeepAnnotations=1
            -cp build/libs/r8.jar
            com.android.tools.r8.R8
            --debug
            --classfile
            --no-minification
            --no-desugaring
            --pg-conf src/main/keep.txt
            --pg-conf {_THIS_DIR}/chromium_keeps.txt
            --lib {java_home}
            --output r8.jar
            build/libs/r8.jar
        """))
    out_dir = os.path.join(output_prefix, 'lib')
    os.makedirs(out_dir)
    shutil.move('r8.jar', out_dir)
    r8_jar = os.path.join(out_dir, 'r8.jar')

    _build_global_synthetics_dex(
        r8_jar, java_home, os.path.join(out_dir, 'global_synthetics.dex.jar'))
    bin_dir = os.path.join(output_prefix, 'bin')
    os.makedirs(bin_dir)
    _build_native_binaries(r8_jar, bin_dir)


def _build_global_synthetics_dex(r8_jar, java_home, output_path):
    """Dexes every "global synthetic" class that D8 can emit.

    Desugaring some constructs (e.g. Java records) makes D8 emit classes
    shared by the whole app rather than owned by one library. Chromium's
    per-library dexing discards them (--ignore-globals in dex.py) and the
    final dex step of non-R8 APKs adds this .dex.jar instead. In intermediate
    mode D8 emits only library-independent globals (RecordTag, DesugarVarHandle,
    DesugarMethodHandlesLookup, ...), so no android.jar is needed here.
    """
    java = os.path.join(java_home, 'bin', 'java')
    # D8 keys the globals format on the file extension: a ".globals" file is a
    # single globals archive, while ".zip" is expected to hold one.
    globals_path = os.path.abspath('desugar.globals')
    common.run_cmd([
        java,
        '-cp',
        r8_jar,
        'com.android.tools.r8.GlobalSyntheticsGenerator',
        '--min-api',
        _MIN_SUPPORTED_SDK_VERSION,
        '--output',
        globals_path,
    ])
    dex_dir = os.path.abspath('global_synthetics_dex')
    os.makedirs(dex_dir)
    common.run_cmd([
        java,
        '-cp',
        r8_jar,
        'com.android.tools.r8.D8',
        '--no-desugaring',
        '--min-api',
        _MIN_SUPPORTED_SDK_VERSION,
        '--globals',
        globals_path,
        '--output',
        dex_dir,
    ])
    dex_files = sorted(os.listdir(dex_dir))
    if dex_files != ['classes.dex']:
        raise RuntimeError(f'Expected a single classes.dex, got: {dex_files}')
    with zipfile.ZipFile(output_path, 'w') as z:
        z.write(os.path.join(dex_dir, 'classes.dex'), 'classes.dex')


def _sample_d8_args(out_dir, jdk_home, classpath_jar, program_jar,
                    program_entry, service_file):
    """Returns argv lists covering the ways that ChromeD8 is invoked."""
    os.makedirs(out_dir)
    intermediate_jar = os.path.join(out_dir, 'intermediate.jar')
    shard_jar = os.path.join(out_dir, 'shard.jar')
    merged_jar = os.path.join(out_dir, 'merged.jar')
    service_arg = f'META-INF/services/org.chromium.sample.Iface={service_file}'
    common_args = ['--min-api', '21', '--lib', jdk_home]
    return [
        # Per-library dexing with desugaring from --jar-input.
        common_args + [
            '--intermediate',
            '--ignore-globals',
            '--classpath',
            classpath_jar,
            '--jar-input',
            program_jar,
            '--add-file',
            service_arg,
            '--output',
            intermediate_jar,
        ],
        # Sharded per-library dexing with --jar-class.
        common_args + [
            '--intermediate',
            '--ignore-globals',
            '--classpath',
            classpath_jar,
            '--jar-input',
            program_jar,
            '--jar-class',
            program_entry,
            '--output',
            shard_jar,
        ],
        # Dex merging into a stored .jar.
        common_args + [
            '--release',
            '--add-file',
            service_arg,
            '--output',
            merged_jar,
            intermediate_jar,
        ],
    ]


def _d8_pgo_args(out_dir, jdk_home, r8_jar):
    """Returns argv lists for PGO training of ChromeD8 (dexing + merging r8.jar)."""
    os.makedirs(out_dir)
    common_args = ['--min-api', '24', '--lib', jdk_home]
    intermediate_jar = os.path.join(out_dir, 'intermediate.jar')
    return [
        common_args + [
            '--intermediate',
            '--ignore-globals',
            '--jar-input',
            r8_jar,
            '--output',
            intermediate_jar,
        ],
        common_args + [
            '--release',
            '--output',
            os.path.join(out_dir, 'merged.jar'),
            intermediate_jar,
        ],
    ]


def _sample_r8_args(out_dir, jdk_home, classpath_jar, program_jar,
                    program_entry, service_file, pg_conf, mapping_file,
                    stacktrace_file):
    """Returns argv lists covering the ways that ChromeR8 is invoked."""
    r8_dex_dir = os.path.join(out_dir, 'r8_dex')
    os.makedirs(r8_dex_dir)
    r8_map = os.path.join(out_dir, 'r8.mapping')
    keep_radius_html = os.path.join(out_dir, 'keepradius.html')
    classes_dex = os.path.join(r8_dex_dir, 'classes.dex')
    d8_args_list = [['--d8'] + args for args in _sample_d8_args(
        os.path.join(out_dir, 'd8'), jdk_home, classpath_jar, program_jar,
        program_entry, service_file)]
    return d8_args_list + [
        # Whole-program R8 shrinking, optimization, and dexing (as invoked by
        # proguard.py, including keep-radius protobuf generation).
        [
            f'-Dcom.android.tools.r8.dumpkeepradiushtmltofile={keep_radius_html}',
            '--enable-startup-layout-optimization',
            '--release',
            '--min-api',
            '26',
            '--no-data-resources',
            '--map-id-template',
            'Sample',
            '--source-file-template',
            'PG',
            '--map-diagnostics:CheckDiscardDiagnostic',
            'error',
            'none',
            '--map-diagnostics:CheckEnumUnboxedDiagnostic',
            'error',
            'none',
            '--map-diagnostics:EmptyMemberRulesToDefaultInitRuleConversionDiagnostic',
            'warning',
            'none',
            '--map-diagnostics',
            'info',
            'warning',
            '--map-diagnostics',
            'error',
            'warning',
            '--lib',
            jdk_home,
            '--pg-conf',
            pg_conf,
            '--pg-map-output',
            r8_map,
            '--output',
            r8_dex_dir,
            classpath_jar,
            program_jar,
        ],
        # TraceReferences check on the R8 dex output. Uses program_jar (which
        # omits Iface.class) as a --lib archive so that both
        # ArchiveClassFileProvider and MissingDefinitionsDiagnostic (mapped to a
        # warning) are exercised.
        [
            '--tracereferences',
            '--map-diagnostics:MissingDefinitionsDiagnostic',
            'error',
            'warning',
            '--check',
            '--lib',
            jdk_home,
            '--lib',
            program_jar,
            '--source',
            classes_dex,
        ],
        # Stack trace deobfuscation via FlushingReTrace.
        [
            '--retrace',
            mapping_file,
            stacktrace_file,
        ],
        # Dex disassembly via com.android.tools.r8.Disassemble.
        [
            '--disassemble',
            '--pg-map',
            r8_map,
            classes_dex,
        ],
    ]


def _check_d8_smoke_out(out_dir):
    """Verifies that _sample_d8_args produced expected outputs in out_dir."""
    with zipfile.ZipFile(os.path.join(out_dir, 'merged.jar')) as z:
        if 'classes.dex' not in z.namelist():
            raise RuntimeError(
                f'D8 smoke test in {out_dir} did not produce classes.dex')


def _extract_javac_classes_jar(jdk_home, output_jar):
    """Extracts jdk.compiler and java.compiler classes from lib/modules."""
    mod_dir = os.path.abspath('javac_modules')
    common.run_cmd([
        os.path.join(jdk_home, 'bin', 'jimage'),
        'extract',
        '--include',
        r'regex:/(jdk\.compiler|java\.compiler)/.*',
        '--dir',
        mod_dir,
        os.path.join(jdk_home, 'lib', 'modules'),
    ])
    with zipfile.ZipFile(output_jar, 'w') as z:
        for module in ('jdk.compiler', 'java.compiler'):
            module_root = os.path.join(mod_dir, module)
            for p in sorted(pathlib.Path(module_root).glob('**/*.class')):
                if p.name != 'module-info.class':
                    z.write(str(p), p.relative_to(module_root).as_posix())


def _build_native_binaries(r8_jar, bin_dir):
    """Builds GraalVM native-image binaries for ChromeD8 and ChromeR8."""
    graal_home = graalvm.ensure_graalvm()
    javac_bin = os.path.join(graal_home, 'bin', 'javac')

    custom_classes = os.path.abspath('custom_classes')
    common.run_cmd([
        javac_bin,
        '-cp',
        r8_jar,
        '-d',
        custom_classes,
        common.path_within_checkout(_CHROME_D8_CHECKOUT_SUBPATH),
        common.path_within_checkout(_CHROME_R8_CHECKOUT_SUBPATH),
        common.path_within_checkout(_FLUSHING_RETRACE_CHECKOUT_SUBPATH),
    ])

    # Inputs for the tracing agent, which records the reflection & resource
    # accesses that native-image needs to know about (e.g. R8's
    # reflectively-loaded ThreadingModule provider). Iface is passed via
    # --classpath so that tracing covers interface desugaring.
    sample_src = pathlib.Path('sample_src').resolve()
    sample_src.mkdir()
    (sample_src / 'Iface.java').write_text(_SAMPLE_IFACE_JAVA,
                                           encoding='utf-8')
    (sample_src / 'Sample.java').write_text(_SAMPLE_JAVA, encoding='utf-8')
    (sample_src / 'Point.java').write_text(_SAMPLE_RECORD_JAVA,
                                           encoding='utf-8')
    sample_classes = os.path.abspath('sample_classes')
    common.run_cmd([
        javac_bin, '--release', '17', '-d', sample_classes,
        str(sample_src / 'Iface.java'),
        str(sample_src / 'Sample.java'),
        str(sample_src / 'Point.java')
    ])
    iface_class = 'org/chromium/sample/Iface.class'
    iface_jar = os.path.abspath('sample_classpath.jar')
    with zipfile.ZipFile(iface_jar, 'w') as z:
        z.write(os.path.join(sample_classes, iface_class), iface_class)
    program_jar = os.path.abspath('sample_program.jar')
    program_entries = []
    with zipfile.ZipFile(program_jar, 'w') as z:
        for p in sorted(pathlib.Path(sample_classes).glob('**/*.class')):
            if not p.name.startswith('Iface'):
                rel = p.relative_to(sample_classes).as_posix()
                z.write(str(p), rel)
                program_entries.append(rel)
    service_file = os.path.abspath('sample_service.txt')
    pathlib.Path(service_file).write_text('org.chromium.sample.Sample\n',
                                          encoding='utf-8')
    sample_pgcfg = os.path.abspath('sample.pgcfg')
    pathlib.Path(sample_pgcfg).write_text(
        '-keep public class org.chromium.sample.Sample {\n'
        '  public int mField;\n'
        '  public static void main(java.lang.String[]);\n'
        '}\n',
        encoding='utf-8',
    )
    sample_mapping = os.path.abspath('sample.mapping')
    pathlib.Path(sample_mapping).write_text(_SAMPLE_MAPPING, encoding='utf-8')
    sample_stacktrace = os.path.abspath('sample_stacktrace.txt')
    pathlib.Path(sample_stacktrace).write_text(_SAMPLE_STACKTRACE,
                                               encoding='utf-8')

    jrt_native_image_args = [
        '-H:+UnlockExperimentalVMOptions',
        # R8 reads the JDK passed via --lib using the jrt:/ filesystem.
        '-H:+AllowJRTFileSystem',
    ]

    # 1. Build bin/chrome_d8 (Serial GC, PGO trained by dexing R8 itself).
    d8_output_path = os.path.join(bin_dir, 'chrome_d8')
    graalvm.build_native_image(
        r8_jar,
        d8_output_path,
        main_class=_CHROME_D8_MAIN_CLASS,
        extra_cp=[custom_classes],
        tracing_args_list=_sample_d8_args(os.path.abspath('tracing_d8_out'),
                                          graal_home, iface_jar, program_jar,
                                          program_entries[0], service_file),
        pgo_args_list=_d8_pgo_args(os.path.abspath('pgo_d8_out'), graal_home,
                                   r8_jar),
        extra_native_image_args=jrt_native_image_args,
    )

    smoke_d8_out = os.path.abspath('smoke_test_d8_out')
    for args in _sample_d8_args(smoke_d8_out, graal_home, iface_jar,
                                program_jar, program_entries[0], service_file):
        common.run_cmd([d8_output_path, f'-Djava.home={graal_home}'] + args)
    _check_d8_smoke_out(smoke_d8_out)

    # 2. Build bin/chrome_r8 (G1 GC, PGO trained on D8 dexing/merging of r8.jar
    # plus R8 compilation of jdk.compiler + java.compiler classes and feeding
    # the resulting dex/mapping into --tracereferences, --retrace, and
    # --disassemble).
    r8_output_path = os.path.join(bin_dir, 'chrome_r8')
    r8_pgo_out = os.path.abspath('pgo_r8_out')
    os.makedirs(r8_pgo_out)
    pgo_javac_jar = os.path.join(r8_pgo_out, 'javac.jar')
    _extract_javac_classes_jar(graal_home, pgo_javac_jar)
    pgo_keep_cfg = os.path.join(r8_pgo_out, 'keep.pgcfg')
    pathlib.Path(pgo_keep_cfg).write_text(
        '-keep public class com.sun.tools.javac.Main {\n'
        '  public static void main(java.lang.String[]);\n'
        '}\n'
        '-if class com.sun.tools.javac.main.* {\n'
        '  public *;\n'
        '}\n'
        '-keep,allowobfuscation,allowshrinking class <1> {\n'
        '  <init>(...);\n'
        '}\n'
        '-keepclassmembers,allowobfuscation,allowshrinking'
        ' enum com.sun.tools.javac.** {\n'
        '  public static **[] values();\n'
        '}\n'
        '-identifiernamestring class com.sun.tools.javac.** {\n'
        '  static java.lang.String *;\n'
        '}\n'
        '-assumenosideeffects class com.sun.tools.javac.util.Assert {\n'
        '  public static void check(...);\n'
        '}\n'
        '-dontwarn **\n',
        encoding='utf-8',
    )
    pgo_r8_dex_zip = os.path.join(r8_pgo_out, 'r8_dex.zip')
    pgo_r8_map = os.path.join(r8_pgo_out, 'r8.mapping')
    pgo_stacktrace = os.path.join(r8_pgo_out, 'stacktrace.txt')
    pathlib.Path(pgo_stacktrace).write_text(
        '\tat com.sun.tools.javac.Main.main(PG:1)\n'
        '\tat a.a.a(PG:1)\n' * 100,
        encoding='utf-8',
    )
    # Generated protobuf classes in keepradius.proto use Class.getMethod() via
    # GeneratedMessage$FieldAccessorTable when serializing --keep-radius-output.
    with zipfile.ZipFile(r8_jar) as z:
        proto_reflection = [
            {
                'type': name[:-len('.class')].replace('/', '.'),
                'allDeclaredMethods': True,
                'allPublicMethods': True,
            } for name in sorted(z.namelist())
            if (name.startswith('com/android/tools/r8/keepradius/proto/')
                and name.endswith('.class'))
        ]

    r8_d8_pgo_args_list = [['--d8'] + args for args in _d8_pgo_args(
        os.path.join(r8_pgo_out, 'd8'), graal_home, r8_jar)]
    graalvm.build_native_image(
        r8_jar,
        r8_output_path,
        main_class=_CHROME_R8_MAIN_CLASS,
        extra_cp=[custom_classes],
        tracing_args_list=_sample_r8_args(os.path.abspath('tracing_r8_out'),
                                          graal_home, iface_jar, program_jar,
                                          program_entries[0], service_file,
                                          sample_pgcfg, sample_mapping,
                                          sample_stacktrace),
        extra_reachability_metadata={
            'reflection': [
                {
                    'type': 'com.android.tools.r8.graph.DexEncodedMethod[]',
                },
                {
                    'type':
                    ('com.android.tools.r8.jetbrains.kotlin.metadata.jvm'
                     '.internal.JvmMetadataExtensions'),
                    'methods': [{
                        'name': '<init>',
                        'parameterTypes': []
                    }],
                },
                {
                    'type':
                    ('com.sun.xml.internal.stream.XMLInputFactoryImpl'),
                    'methods': [{
                        'name': '<init>',
                        'parameterTypes': []
                    }],
                },
            ] + proto_reflection,
            'resources': [
                {
                    'glob': ('META-INF/services/com.android.tools.r8.jetbrains'
                             '.kotlin.metadata.internal.extensions'
                             '.MetadataExtensions'),
                },
                {
                    'glob':
                    ('META-INF/services/javax.xml.stream.XMLInputFactory'),
                },
            ],
        },
        pgo_args_list=r8_d8_pgo_args_list + [
            [
                '--release',
                '--min-api',
                '26',
                '--enable-startup-layout-optimization',
                '--no-data-resources',
                '--lib',
                graal_home,
                '--pg-conf',
                pgo_keep_cfg,
                '--pg-map-output',
                pgo_r8_map,
                '--output',
                pgo_r8_dex_zip,
                pgo_javac_jar,
            ],
            [
                '--tracereferences',
                '--map-diagnostics:MissingDefinitionsDiagnostic',
                'error',
                'warning',
                '--check',
                '--lib',
                graal_home,
                '--source',
                pgo_r8_dex_zip,
            ],
            [
                '--retrace',
                pgo_r8_map,
                pgo_stacktrace,
            ],
            [
                '--disassemble',
                '--pg-map',
                pgo_r8_map,
                '--output',
                os.path.join(r8_pgo_out, 'disassembled.txt'),
                pgo_r8_dex_zip,
            ],
        ],
        extra_native_image_args=jrt_native_image_args + ['--gc=G1'],
    )

    smoke_r8_out = os.path.abspath('smoke_test_r8_out')
    for args in _sample_r8_args(smoke_r8_out, graal_home, iface_jar,
                                program_jar, program_entries[0], service_file,
                                sample_pgcfg, sample_mapping,
                                sample_stacktrace):
        proc = common.run_cmd([r8_output_path, f'-Djava.home={graal_home}'] +
                              args,
                              stdout=subprocess.PIPE,
                              universal_newlines=True)
        if args[0] == '--retrace' and (
                'org.chromium.sample.Sample.inlined(Sample.java:21)'
                not in proc.stdout):
            raise RuntimeError(
                f'Unexpected --retrace output from chrome_r8:\n{proc.stdout}')
        if args[0] == '--disassemble' and ('org.chromium.sample.Sample'
                                           not in proc.stdout):
            raise RuntimeError(
                f'Unexpected --disassemble output from chrome_r8:\n'
                f'{proc.stdout}')
    _check_d8_smoke_out(os.path.join(smoke_r8_out, 'd8'))
    if not os.path.exists(os.path.join(smoke_r8_out, 'r8_dex', 'classes.dex')):
        raise RuntimeError('chrome_r8 smoke test did not produce classes.dex')
    if not os.path.exists(os.path.join(smoke_r8_out, 'keepradius.html')):
        raise RuntimeError(
            'chrome_r8 smoke test did not produce keepradius.html')


def main():

    def do_latest():
        if _ROLL_TO_TIP_OF_TREE:
            return get_commit_before_today()
        return get_latest_dev_branch()

    def do_install(args):
        _install(args.version, args.output_prefix)

    common.main(do_latest=do_latest,
                do_install=do_install,
                runtime_deps=[
                    ('//' + _CHROME_D8_SRC, _CHROME_D8_CHECKOUT_SUBPATH),
                    ('//' + _CHROME_R8_SRC, _CHROME_R8_CHECKOUT_SUBPATH),
                    ('//' + _FLUSHING_RETRACE_SRC,
                     _FLUSHING_RETRACE_CHECKOUT_SUBPATH),
                ],
                version_deps=[
                    '//' + _CHROME_D8_SRC,
                    '//' + _CHROME_R8_SRC,
                    '//' + _FLUSHING_RETRACE_SRC,
                ],
                version_strings_func=graalvm.version_strings)


if __name__ == '__main__':
    main()
