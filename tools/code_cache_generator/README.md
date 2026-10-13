# WebUI code cache generator

[`code_cache_generator`](code_cache_generator.cc) compiles WebUI JavaScript
modules at build time and writes the V8 code cache for each one. Chrome ships
the result as resources, so a WebUI renderer can deserialize compiled code
instead of compiling its scripts on first use. This matters most for top-chrome
WebUIs such as the toolbar, whose renderer otherwise compiles hundreds of
functions lazily while the first window opens.

At runtime the cache is used only when the `kWebUIBundledCodeCache` feature in
[content_features.cc](/content/public/common/content_features.cc) is enabled.
In the renderer,
[`WebUIBundledCodeCacheFetcher`](/third_party/blink/renderer/platform/loader/fetch/url_loader/webui_bundled_code_cache_fetcher.cc)
reads the cache from the resource bundle and
[`WebUIBundledCachedMetadataHandler`](/third_party/blink/renderer/platform/loader/fetch/webui_bundled_cached_metadata_handler.h)
hands it to V8. They record whether a cache was received and consumed in the
`Blink.ResourceRequest.WebUIBundledCodeCacheFetcher.*` and
`Blink.ResourceRequest.WebUIBundledCachedMetadataHandler.*` histograms.

[TOC]

## Build integration

A WebUI target opts in with
`generate_code_cache = enable_webui_generate_code_cache` in `build_webui()`,
defined in [build_webui.gni](/ui/webui/resources/tools/build_webui.gni). That
runs the `generate_code_cache` template from
[generate_code_cache.gni](/ui/webui/resources/tools/generate_code_cache.gni),
which calls [generate_code_cache.py](/ui/webui/resources/tools/generate_code_cache.py)
to run this binary over the minified modules. The resulting
`*_code_cache_resources.pak` files are packed next to the regular WebUI
resources by [chrome_paks.gni](/chrome/chrome_paks.gni).

`enable_webui_generate_code_cache` is computed in
[webui_features.gni](/ui/webui/webui_features.gni). Generation is on only in
`optimize_webui` builds, never in MSan builds, and only on these build
machines:

| Target | Build machine | Cache shipped in |
| --- | --- | --- |
| Linux, Windows | Same OS and CPU as the target | `resources.pak` |
| Mac, either CPU | Any Mac, including an arm64 Mac building x64 | `webui_code_cache.<arch>.pak` |
| Mac | Linux | Not generated |
| Other cross builds | Any | Not generated |

## What makes a cache valid

V8 accepts a code cache only if its header matches the running V8. The header,
`SerializedCodeData` in
[code-serializer.h](/v8/src/snapshot/code-serializer.h), holds a V8 version
hash, a source hash, a flag hash, and the checksum of the read-only heap in the
V8 snapshot that produced it. A cache with a different snapshot checksum is
rejected silently, and the script compiles as if there were no cache. The
generator therefore has to load exactly the snapshot that Chrome will ship:
`snapshot_blob.bin`, or the context snapshot when `use_v8_context_snapshot` is
true (see
[v8_context_snapshot.gni](/tools/v8_context_snapshot/v8_context_snapshot.gni)).

The snapshot checksum is the only header field that differs between an x64 and
an arm64 cache built from the same source. The compiled payload is otherwise
identical. Even so, each CPU needs its own cache file, since each file names
one snapshot.

The generator runs V8 with `--predictable`, so the output is deterministic for
a given V8, snapshot, and input.

## Cross-compilation

The generator compiles modules and serializes the result; it never executes
JavaScript. A generator that contains V8 for the target CPU can therefore run
natively on a build machine with a different CPU, without emulation. This is
the same arrangement V8 uses for `mksnapshot` and Chrome uses for
`v8_context_snapshot_generator` in
[tools/v8_context_snapshot/BUILD.gn](/tools/v8_context_snapshot/BUILD.gn).

To get that arrangement,
[generate_code_cache.gni](/ui/webui/resources/tools/generate_code_cache.gni)
builds the generator in `v8_snapshot_toolchain`, which
[snapshot_toolchain.gni](/v8/gni/snapshot_toolchain.gni) selects. For an x64
Mac target on an arm64 Mac, that toolchain is `clang_arm64_v8_x64` in
[build/toolchain/mac/BUILD.gn](/build/toolchain/mac/BUILD.gn), which produces
an arm64 program with x64 V8 inside. For native builds it is the default
toolchain. [BUILD.gn](BUILD.gn) also keeps a default-toolchain copy for the
Python unit test.

A generator from another toolchain would load the snapshot built next to
itself, which belongs to the build machine. To avoid that, the build passes
the target's snapshot explicitly with `--snapshot_blob` or
`--context_snapshot`, which `main()` in
[code_cache_generator.cc](code_cache_generator.cc) loads before falling back to
the default snapshot.

The two switches are deliberate. The switch tells the generator which kind of
snapshot it is loading, `gin::V8SnapshotFileType::kDefault` or
`kWithAdditionalContext` from
[v8_snapshot_file_type.h](/gin/public/v8_snapshot_file_type.h). Blink reads
that type back through `gin::GetLoadedSnapshotFileType()` and deserializes
contexts only from a context snapshot, see `GetV8ContextSnapshotMode()` in
[v8_initializer.cc](/third_party/blink/renderer/bindings/core/v8/v8_initializer.cc).
The generator's own `USE_V8_CONTEXT_SNAPSHOT` build flag cannot answer that
question in a cross build, because it comes from the generator's toolchain
rather than the target's.

Without external startup data the snapshot is compiled into the binary, so
[generate_code_cache.gni](/ui/webui/resources/tools/generate_code_cache.gni)
asserts that cross-toolchain generation always has a snapshot file to pass.

## Mac universal builds

A universal Mac app merges a separately built x64 app and arm64 app with
[universalizer.py](/chrome/installer/mac/universalizer.py). A file that differs
between the halves is merged with `lipo`, which only works for Mach-O files, so
every other file must be byte-identical in both halves. A file present in only
one half is copied as is. The x64 half may be cross-compiled on an arm64 Mac.

These rules decide where the Mac cache lives. A cache inside `resources.pak`
makes the two `resources.pak` files differ, because each half's cache carries
its own snapshot checksum, and `lipo` then fails on the non-Mach-O file. This
broke the official universal build in March 2025, which is why Mac generation
stayed off until per-CPU packaging existed (https://crrev.com/c/6323323,
https://crbug.com/399764218).

Mac builds therefore pack the cache into its own per-CPU file,
`webui_code_cache.x86_64.pak` or `webui_code_cache.arm64.pak`, next to
`resources.pak` in the framework's `Resources` directory. The repack target and
the file name come from [chrome_paks.gni](/chrome/chrome_paks.gni) and
[webui_features.gni](/ui/webui/webui_features.gni). The names follow
`v8_context_snapshot.<arch>.bin`, which solves the same problem for the V8
snapshot. `resources.pak` stays identical across CPUs, and the universal app
carries both cache files.

At runtime `chrome::FILE_WEBUI_CODE_CACHE_PACK`, declared in
[chrome_paths.h](/chrome/common/chrome_paths.h) and resolved in
[chrome_paths.cc](/chrome/common/chrome_paths.cc), points at the file for the
running CPU. Only renderer processes load that pak, in
`ChromeMainDelegate::PreSandboxStartup()` in
[chrome_main_delegate.cc](/chrome/app/chrome_main_delegate.cc). The browser
process builds its URL to resource ID map in
`webui::GetWebUIResourceUrlToCodeCacheMap()`, in
[webui_util_desktop.cc](/chrome/browser/ui/webui/util/webui_util_desktop.cc),
from generated resource maps that are compiled into the binary, so it never
reads the cache bytes.

### Rejected: both caches in resources.pak

Shipping both CPUs' caches inside one shared `resources.pak` and choosing one
at runtime would also keep the two halves identical
(https://crrev.com/c/7961379 discusses this). The halves are separate builds,
though, so each half would have to build the other CPU's V8 snapshot and
context snapshot as well, doubling that work in every Mac build. It would also
need per-CPU resource IDs, a CPU check in the cache loader, and would ship an
unused cache in every single-CPU build.

### Why Mac builds on Linux skip generation

Mac builds on Linux machines turn off `use_v8_context_snapshot` so that Blink
is not built a second time in the snapshot toolchain (see
[v8_context_snapshot.gni](/tools/v8_context_snapshot/v8_context_snapshot.gni)).
The generator links Blink, so building it there would bring that cost back. On
Mac machines the snapshot toolchain already builds Blink for
`v8_context_snapshot_generator`, and the generator adds one link plus the
generation actions.

## Cost

*   Size: the cache for the current set of WebUIs is about 3.5 MB per CPU,
    so about 7.1 MB in a universal Mac app. Linux and Windows ship the same
    data inside `resources.pak`.
*   Build time: Mac builds on Mac machines add one link, the generator, plus
    the generation actions. The snapshot toolchain already builds Blink there.
    Mac builds on Linux machines are unchanged, since they skip generation.

## Testing

*   `//ui/webui/resources/tools:webui_resources_tools_python_unittests` runs
    [generate_code_cache_test.py](/ui/webui/resources/tools/generate_code_cache_test.py).
    It covers the script and the generator, including that an explicitly
    passed snapshot yields the same cache as the generator's default snapshot.
*   `TabSearchUIBundledCodeCacheBrowserTest` in
    [tab_search_ui_browsertest.cc](/chrome/browser/ui/webui/tab_search/tab_search_ui_browsertest.cc)
    checks that Blink receives and consumes bundled caches when the feature is
    on, and none when it is off. It is compiled only when
    `enable_webui_generate_code_cache` is true. On Mac, a missing or rejected
    per-CPU pak fails it.

Release Mac builds on Mac machines are the only builds that generate the Mac
cache, so local Linux-hosted Mac builds and debug builds do not exercise it.
On the commit queue, mac-rel builds arm64 on an arm64 Mac, but its
`browser_tests` suite is CI-only. To run the browser test on a CL that touches
this area, add the footers described in [cq.md](/docs/infra/cq.md):

```
Cq-Include-Trybots: luci.chromium.try:mac14-tests
Include-Ci-Only-Tests: chromium.mac:mac26-arm64-rel-tests,chromium.mac:mac14-tests|browser_tests
```

mac26-arm64-rel-tests covers arm64. mac14-tests builds x64 on an arm64 Mac, so
it covers the cross-compiled generator, and runs the tests on Intel Macs.

To check a running build by hand, record a startup trace with the
`disabled-by-default-v8.compile` category. Each `v8.compileModule` event for a
`chrome://` script reports whether a cache was consumed or rejected. A
rejection usually means the cache was built against another snapshot.
