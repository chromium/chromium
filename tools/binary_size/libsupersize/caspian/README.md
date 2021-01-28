# Caspian

## What is it?

Caspian is the name for the WebAssembly portion of the SuperSize Tiger Viewer.

## Applying patches

Caspian needs some minor edits that we don't want to commit:

```sh
git apply -3 tools/binary_size/libsupersize/caspian/wasmbuild.patch
```

To re-create .patch file:
```sh
git add ...files to include in patch...
git diff --staged > tools/binary_size/libsupersize/caspian/wasmbuild.patch
# Double check that only expected files are included in the patch:
grep +++ tools/binary_size/libsupersize/caspian/wasmbuild.patch
```

## Building the test app & tests
There is a test and a binary that you can run to help with development (and
allows debugging outside of the browser).

```sh
OUT=$out/linux_debug
autoninja -C $OUT caspian_cli caspian_unittests
$OUT/caspian_cli --help
$OUT/caspian_unittests
# To debug a crash:
lldb $OUT/caspian_cli validatediff supersize_diff.sizediff
```

## Building the wasm module

Install emscripten from:
https://emscripten.org/docs/getting_started/downloads.html

```sh
# Known working version:
./emsdk install 2.0.3 && ./emsdk activate 2.0.3 && source ./emsdk_env.sh
```

Build:
```sh
gn gen out/caspian --args='is_official_build=true treat_warnings_as_errors=false fatal_linker_warnings=false chrome_pgo_phase=0'
( cd out/caspian; autoninja caspian_web && cp wasm/caspian_web.* ../../tools/binary_size/libsupersize/static/ )
```

Run local test server:
```sh
tools/binary_size/libsupersize/upload_html_viewer.py --local
```

Deploy to firebase:
```sh
tools/binary_size/libsupersize/upload_html_viewer.py [--prod | --staging]
```
