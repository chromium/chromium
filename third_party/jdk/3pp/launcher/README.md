# Javac Launcher

Custom `javac` entry point compiled into Chromium's GraalVM native-images:

* `//third_party/jdk/3pp/3pp.py` (`jdk-tools-graalvm`)
* `//third_party/android_build_tools/error_prone/3pp/3pp.py` (`error_prone`)

Standard OpenJDK `javac` silently accepts unknown `-XD...` flags, so
`//build/android/gyp/compile_java.py` can pass these flags unconditionally while
falling back safely on stock `javac` and pre-roll CIPD packages.

## Flags

* **`-XDcrFastClassPath`** (disable with `-XDcrFastClassPath=false`):
  * Skips `META-INF/MANIFEST.MF` `Class-Path` expansion.
  * Caches parent-directory canonicalization on Linux (`FastCacheFSInfo`) to
    avoid repeated `realpath` traversals per JAR.
  * Replaces `JavacFileManager`'s upfront `ZipFileSystem` walk with lazy raw
    ZIP central-directory indexing (`LazyJavacFileManager`), delegating
    non-standard classpath entries (directories, Multi-Release JARs, or
    ZIP64/commented/corrupt archives) to `JavacFileManager`.
* **`-XDcrClassPathUsageOut=<path>`** (active when `-XDcrFastClassPath` is
  enabled and compilation succeeds): writes one line per `-classpath` JAR
  actually read during compilation, used by
  `//build/android/gyp/compile_java.py` to prune unused classpath JARs from
  Ninja depfiles.

## Testing Locally

Run the smoke tests against the hermetic Linux JDK (no GraalVM build needed):

```sh
python3 -c "
import sys, tempfile
sys.path[:0] = ['build/3pp_common', 'third_party/jdk/3pp/launcher']
import javac_launcher
with tempfile.TemporaryDirectory() as d:
    jdk = 'third_party/jdk/current/bin'
    javac_launcher.compile_launcher(jdk + '/javac', d + '/cls')
    cmd = [jdk + '/java'] + [
        f'--add-exports={e}=ALL-UNNAMED'
        for e in javac_launcher.JAVAC_EXPORTS
    ] + ['-cp', d + '/cls', 'JavacLauncher', '--release', '25']
    javac_launcher.smoke_test(cmd, d + '/work')
"
```

To test full GraalVM native-image packaging locally:

```sh
_3PP_PLATFORM=linux-amd64 third_party/jdk/3pp/3pp.py local-test \
    --checkout-dir /tmp/jdk_3pp_work --output-prefix /tmp/jdk_3pp_out
third_party/android_build_tools/error_prone/3pp/3pp.py local-test \
    --checkout-dir /tmp/ep_3pp_work --output-prefix /tmp/ep_3pp_out
```

`javac_launcher.py` runs inside the Linux 3pp Docker container and must remain
Python 3.6-compatible. Changes in this directory are tracked in both recipes'
`version_deps` (and `error_prone`'s `runtime_deps`), so CI automatically
rebuilds both packages.
