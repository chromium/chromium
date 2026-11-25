# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Custom javac launcher implementing -XDcrFastClassPath.

Compiles //third_party/jdk/3pp/launcher/*.java for Chromium's
jdk-tools-graalvm and error_prone GraalVM native-images, and provides smoke
tests covering -XDcrFastClassPath (manifest Class-Path disabling,
directory-cached canonicalization, and LazyJavacFileManager edge cases:
multi-release JARs, mixed directory/JAR classpath precedence, symlinks,
non-ASCII packages, stored & deflated entries, and corrupt JAR errors).
"""

import os
import pathlib
import subprocess
import zipfile

import common

_THIS_DIR = pathlib.Path(__file__).resolve().parent

JAVAC_EXPORTS = [
    'jdk.compiler/com.sun.tools.javac.file',
    'jdk.compiler/com.sun.tools.javac.main',
    'jdk.compiler/com.sun.tools.javac.util',
]


def compile_launcher(javac_bin, output_dir):
    """Compiles //third_party/jdk/3pp/launcher/*.java into |output_dir|."""
    java_files = sorted(str(p) for p in _THIS_DIR.glob('*.java'))
    if not java_files:
        raise RuntimeError(f'No .java files found in {_THIS_DIR}')
    os.makedirs(output_dir, exist_ok=True)
    cmd = [javac_bin]
    cmd += ['--add-exports=' + exp + '=ALL-UNNAMED' for exp in JAVAC_EXPORTS]
    cmd += ['-d', output_dir] + java_files
    common.run_cmd(cmd)


def smoke_test(javac_cmd, work_dir, env=None):
    """Verifies -XDcrFastClassPath and LazyJavacFileManager."""
    test_dir = os.path.join(work_dir, 'javac_launcher_test')
    os.makedirs(test_dir, exist_ok=True)
    out_dir = os.path.join(test_dir, 'out')

    def write_src(rel_path, content):
        path = os.path.join(test_dir, rel_path)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        pathlib.Path(path).write_text(content, encoding='utf-8')
        return path

    def make_jar(rel_path, files=(), texts=(), compression=zipfile.ZIP_STORED):
        jar_path = os.path.join(test_dir, rel_path)
        os.makedirs(os.path.dirname(jar_path), exist_ok=True)
        with zipfile.ZipFile(jar_path, 'w', compression=compression) as z:
            for src_path, arcname in files:
                z.write(src_path, arcname)
            for arcname, text in texts:
                z.writestr(arcname, text)
        return jar_path

    def run(args, expect_fail=None):
        res = common.run_cmd(
            list(javac_cmd) + args,
            check=False,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            errors='replace',
        )
        if expect_fail is not None:
            if res.returncode == 0:
                raise RuntimeError(expect_fail)
        elif res.returncode != 0:
            raise RuntimeError(
                f'Smoke test javac failed (exit {res.returncode}):\n'
                f'{res.stdout}'
            )
        return res

    def run_lazy(cp, src, expect_fail=None, extra_args=()):
        cp_str = os.pathsep.join(cp) if isinstance(cp, (list, tuple)) else cp
        srcs = list(src) if isinstance(src, (list, tuple)) else [src]
        return run(
            ['-XDcrFastClassPath']
            + list(extra_args)
            + ['-cp', cp_str, '-d', out_dir]
            + srcs,
            expect_fail=expect_fail,
        )

    # 1. Manifest Class-Path expansion (default vs -XDcrFastClassPath vs
    #    -XDcrFastClassPath=false override) with a DEFLATED dep.jar and a
    #    STORED wrapper.jar.
    dep_src = write_src('Dep.java', 'public class Dep {}\n')
    dep_classes = os.path.join(test_dir, 'dep_classes')
    run(['-d', dep_classes, dep_src])
    dep_class = os.path.join(dep_classes, 'Dep.class')

    dep_jar = make_jar(
        'dep.jar',
        files=[(dep_class, 'Dep.class')],
        compression=zipfile.ZIP_DEFLATED,
    )
    wrapper_jar = make_jar(
        'wrapper.jar',
        texts=[
            (
                'META-INF/MANIFEST.MF',
                'Manifest-Version: 1.0\nClass-Path: dep.jar\n\n',
            )
        ],
    )
    use_dep = write_src('UseDep.java', 'public class UseDep { Dep d; }\n')

    run(['-cp', wrapper_jar, '-d', out_dir, use_dep])
    run(
        [
            '-XDcrFastClassPath',
            '-XDcrFastClassPath=false',
            '-cp',
            wrapper_jar,
            '-d',
            out_dir,
            use_dep,
        ]
    )
    run_lazy(
        wrapper_jar,
        use_dep,
        expect_fail=('Expected -XDcrFastClassPath to skip manifest Class-Path'),
    )
    run_lazy([wrapper_jar, dep_jar], use_dep)

    # 2. Multi-Release JAR fallback: META-INF/versions/25/mr/Api.class adds
    #    v25(), which must be visible under --release 25.
    mr_base_src = write_src(
        'mr_base/mr/Api.java', 'package mr; public class Api {}\n'
    )
    mr_v25_src = write_src(
        'mr_v25/mr/Api.java',
        'package mr;\n'
        'public class Api { public static int v25() { return 25; } }\n',
    )
    mr_base_out = os.path.join(test_dir, 'mr_base_out')
    mr_v25_out = os.path.join(test_dir, 'mr_v25_out')
    run(['-d', mr_base_out, mr_base_src])
    run(['-d', mr_v25_out, mr_v25_src])
    mr_base_cls = os.path.join(mr_base_out, 'mr', 'Api.class')
    mr_v25_cls = os.path.join(mr_v25_out, 'mr', 'Api.class')

    mr_jar = make_jar(
        'mr.jar',
        files=[
            (mr_base_cls, 'mr/Api.class'),
            (mr_v25_cls, 'META-INF/versions/25/mr/Api.class'),
        ],
        texts=[
            (
                'META-INF/MANIFEST.MF',
                'Manifest-Version: 1.0\nMulti-Release: true\n\n',
            )
        ],
        compression=zipfile.ZIP_DEFLATED,
    )
    use_mr = write_src(
        'UseMr.java', 'public class UseMr { int x = mr.Api.v25(); }\n'
    )
    run_lazy([dep_jar, mr_jar], use_mr)

    # 3. Classpath precedence between a directory and a lazy JAR.
    dir_src = write_src(
        'prec_dir_src/p/Val.java',
        'package p; public class Val { public static int X = 1; }\n',
    )
    jar_src = write_src(
        'prec_jar_src/p/Val.java',
        'package p; public class Val { public static String X = "s"; }\n',
    )
    prec_dir = os.path.join(test_dir, 'prec_dir')
    prec_jar_out = os.path.join(test_dir, 'prec_jar_out')
    run(['-d', prec_dir, dir_src])
    run(['-d', prec_jar_out, jar_src])
    prec_jar = make_jar(
        'prec.jar',
        files=[(os.path.join(prec_jar_out, 'p', 'Val.class'), 'p/Val.class')],
    )

    use_int = write_src(
        'UseInt.java', 'public class UseInt { int v = p.Val.X; }\n'
    )
    use_str = write_src(
        'UseStr.java', 'public class UseStr { String v = p.Val.X; }\n'
    )
    run_lazy([prec_dir, prec_jar], use_int)
    run_lazy([prec_jar, prec_dir], use_str)

    # 4. Symlinked directory, symlinked JAR, '..' traversal through a symlinked
    #    directory, and a symlinked .class inside a classpath directory.
    sym_parent = os.path.join(test_dir, 'sym_real', 'nested')
    os.makedirs(sym_parent, exist_ok=True)
    sym_link_dir = os.path.join(test_dir, 'sym_link_dir')
    os.symlink(sym_parent, sym_link_dir)
    sym_jar_target = make_jar(
        'sym_real/dep_target.jar', files=[(dep_class, 'Dep.class')]
    )
    sym_jar_link = os.path.join(test_dir, 'dep_link.jar')
    os.symlink(sym_jar_target, sym_jar_link)
    sym_dir_link = os.path.join(test_dir, 'prec_dir_link')
    os.symlink(prec_dir, sym_dir_link)
    # Test each symlinked JAR independently so neither can mask the other.
    run_lazy(os.path.join(sym_link_dir, '..', 'dep_target.jar'), use_dep)
    run_lazy(sym_jar_link, use_dep)
    run_lazy([sym_dir_link, prec_jar], use_int)

    sym_inner_dir = os.path.join(test_dir, 'sym_inner_dir')
    os.makedirs(os.path.join(sym_inner_dir, 'p'), exist_ok=True)
    os.symlink(
        os.path.join(prec_dir, 'p', 'Val.class'),
        os.path.join(sym_inner_dir, 'p', 'Val.class'),
    )
    run_lazy([sym_inner_dir, prec_jar], use_int)

    # 5. JAR with META-INF/versions/ entry but no MANIFEST.MF (like ijar jars)
    #    must stay on the fast path and ignore META-INF/versions/.
    no_mf_jar = make_jar(
        'no_mf.jar',
        files=[
            (mr_base_cls, 'mr/Api.class'),
            (mr_v25_cls, 'META-INF/versions/25/mr/Api.class'),
        ],
    )
    run_lazy(
        no_mf_jar,
        use_mr,
        expect_fail=(
            'Expected JAR without Multi-Release manifest to ignore '
            'META-INF/versions/'
        ),
    )

    # 6. Non-ASCII multi-segment package (guards UTF-8 byte vs char length in
    #    inferBinaryName). Compile with an equal-UTF-8-length ASCII package
    #    ('xxxxx' = 5 bytes == 'p\u00e4\u00e4'.encode('utf-8')) and patch the
    #    class bytes in the JAR so no non-ASCII paths touch the filesystem
    #    (safe under Python 3.6 + LC_ALL=C).
    u_src = write_src(
        'u_src/Api.java',
        'package xxxxx.a;\npublic class Api { public static int V = 7; }\n',
    )
    u_out = os.path.join(test_dir, 'u_out')
    run(['-encoding', 'UTF-8', '-d', u_out, u_src])
    u_bytes = pathlib.Path(u_out, 'xxxxx', 'a', 'Api.class').read_bytes()
    u_pkg_utf8 = 'p\u00e4\u00e4/a/Api'.encode('utf-8')
    assert len(u_pkg_utf8) == len(b'xxxxx/a/Api')
    patched_bytes = u_bytes.replace(b'xxxxx/a/Api', u_pkg_utf8)
    assert patched_bytes != u_bytes
    u_jar = make_jar(
        'unicode.jar',
        texts=[('p\u00e4\u00e4/a/Api.class', patched_bytes)],
    )
    use_u = write_src(
        'UseUnicode.java',
        'public class UseUnicode { int v = p\u00e4\u00e4.a.Api.V; }\n',
    )
    run(
        [
            '-encoding',
            'UTF-8',
            '-XDcrFastClassPath',
            '-cp',
            u_jar,
            '-d',
            out_dir,
            use_u,
        ]
    )

    # 7. Corrupt JAR on -classpath must fail compilation rather than be skipped.
    corrupt_jar = os.path.join(test_dir, 'corrupt.jar')
    pathlib.Path(corrupt_jar).write_bytes(b'not a zip file')
    run_lazy(
        [corrupt_jar, dep_jar],
        use_dep,
        expect_fail='Expected corrupt classpath JAR to fail compilation',
    )

    # 8. -XDcrClassPathUsageOut writes one line per used classpath JAR (both
    #    lazy IndexedJars and Multi-Release fallback JARs), omits used
    #    directories and unused/shadowed JARs, writes empty output when 0 JARs
    #    are used, and omits the file on failure or without -XDcrFastClassPath.
    usage_both = os.path.join(test_dir, 'both.usage.txt')
    run_lazy(
        [prec_dir, dep_jar, prec_jar, mr_jar],
        [use_dep, use_mr, use_int],
        extra_args=[f'-XDcrClassPathUsageOut={usage_both}'],
    )
    used_lines = (
        pathlib.Path(usage_both).read_text(encoding='utf-8').splitlines()
    )
    if used_lines != [dep_jar, mr_jar]:
        raise RuntimeError(
            f'Unexpected usage.txt lines: got {used_lines!r}, '
            f'expected {[dep_jar, mr_jar]!r}'
        )

    usage_empty = os.path.join(test_dir, 'empty.usage.txt')
    run_lazy(
        prec_jar,
        dep_src,
        extra_args=[f'-XDcrClassPathUsageOut={usage_empty}'],
    )
    if pathlib.Path(usage_empty).read_text(encoding='utf-8') != '':
        raise RuntimeError(
            'Expected empty usage.txt when no classpath JARs were read'
        )

    usage_disabled = os.path.join(test_dir, 'disabled.usage.txt')
    run(
        [
            f'-XDcrClassPathUsageOut={usage_disabled}',
            '-cp',
            dep_jar,
            '-d',
            out_dir,
            use_dep,
        ]
    )
    if os.path.exists(usage_disabled):
        raise RuntimeError(
            'Expected no usage.txt when -XDcrFastClassPath is not enabled'
        )

    usage_fail = os.path.join(test_dir, 'fail.usage.txt')
    run_lazy(
        prec_jar,
        use_dep,
        extra_args=[f'-XDcrClassPathUsageOut={usage_fail}'],
        expect_fail='Expected compilation with missing Dep to fail',
    )
    if os.path.exists(usage_fail):
        raise RuntimeError('Expected no usage.txt when compilation fails')
