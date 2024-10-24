# Clang Blink GC plugin

A Clang plugin that checks various invariants of the Blink garbage
collection infrastructure.

Consider this plugin as a best effort. The plugin is not meant to
be exhaustive and/or cover all potential GC issues. The plugin
covers the most common Blink GC pitfalls, but may miss other
potential (sometimes known) issues.

## Build

Clang is built using CMake. To run cmake, this script can be used:
```bash
  ./tools/clang/scripts/build.py     \
    --without-android                \
    --without-fuchsia
```

The build directory is created into: `third_party/llvm-build/Release+Asserts/`
and you can build it again using:
```bash
  ninja -C third_party/llvm-build/Release+Asserts/
```


## Run the tests

```bash
	./tools/clang/blink_gc_plugin/tests/test.py \
    $(pwd)/third_party/llvm-build/Release+Asserts/bin/clang
```

## Using the plugin

To enable the plugin, add the following to your BUILD.gn file:
```bash
cflags += [
    "-Xclang",
    "-add-plugin",
    "-Xclang",
    "blink-gc-plugin",
]
```

To further enable specific plugin options, add the following:
```bash
cflags += [
    "-Xclang",
    "-plugin-arg-blink-gc-plugin",
    "-Xclang",
    "<option>",
]
```

See [blink/renderer/BUILD.gn](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/BUILD.gn;drc=5c316b13946670129cf516b0b6ec854b48d769a3;l=112) for example.
