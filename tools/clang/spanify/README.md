# Clang Spanification tool

This tool generates cross-translation unit rewrites converting pointer type
expressions (T\*/raw_ptr<T>) to base::span<T>/base::raw_span<T> expressions.

For instance:

```cpp
  std::vector<int> ctn = {1,2,3, 4};
  ...
  int* ptr = ctn.data();
  ...
  ptr[index] = value;
```

ptr here becomes a span, and the code becomes:

```cpp
  std::vector<int> ctn = {1,2,3, 4};
  ...
  base::span<int> ptr = ctn;
  ...
  ptr[index] = value;
```

## Build

Clang is built using CMake. To run cmake, this script can be used:

```bash
  ./tools/clang/scripts/build.py     \
    --without-android                \
    --without-fuchsia                \
    --extra-tools spanify
```

The build directory is created into: `third_party/llvm-build/Release+Asserts/`
and you can build it again incrementally using:

```bash
  ninja -C third_party/llvm-build/Release+Asserts/ spanify
```

## Run the tests

```bash
  ./tools/clang/spanify/run_all_tests.py
```

### Troubleshooting

You will need to have the `python` binary reachable from your path to run the
tests. You can route `python3` to just `python` with the following install.

```bash
sudo apt install python-is-python3
```

Finally you need to have a version of libstdc++ installed. If you see errors
like:

```bash
Failed to process deref-expr-actual.cc
tools/clang/spanify/tests/chrome/deref-expr-actual.cc:4:10: fatal error: 'vector' file not found
    4 | #include <vector>
      |          ^~~~~~~~
1 error generated.
```

You can install libstdc++ as follows:

```bash
sudo apt install libstdc++-14-dev
```

## Using the tool

The `rewrite_multiple_platforms.py` script first builds the tool and then runs
the tool over every configuration in the list of platforms.

```bash
  ./tools/clang/spanify/rewrite_multiple_platforms.py
```

### Hints

#### Flags

1. `--platform`: Platform to rewrite (e.g., linux, win, mac, android).
   Can be specified multiple times to rewrite multiple platforms. Defaults to `linux`.
2. `--project`: The project to spanify. Valid choices are `chrome` (default),
   `partition_alloc`, `dawn`, `skia`, `angle`, and `webrtc`.

The script automatically handles the build step. If it detects a custom spanify
build, it will do an incremental build and keep the build intact. Otherwise, it
will build spanify from scratch and restore the original `llvm-build` directory
when finished.

Example commands:

```bash

  ./tools/clang/spanify/rewrite_multiple_platforms.py
  ./tools/clang/spanify/rewrite_multiple_platforms.py --platform linux
  ./tools/clang/spanify/rewrite_multiple_platforms.py --project partition_alloc
```

### Environment Variables

1. `SPANIFY_SCRATCH_DIR`: By default, the script uses a `spanify_scratch`
   directory in the current working directory to store intermediate results
   (e.g., build artifacts, generated patches). You can override this by setting
   the `SPANIFY_SCRATCH_DIR` environment variable. This is useful if you want to
   avoid cluttering your workdir or if you want to share scratch results across
   different checkouts.

   Example:
   ```bash
   export SPANIFY_SCRATCH_DIR=~/my_spanify_scratch
   ./tools/clang/spanify/rewrite_multiple_platforms.py
   ```

### Troubleshooting

If for some reason your `rewrite_multiple_platforms.py` command fails, it is
important to restore the "normal" state of your clang directory before running
again. As the script starts to run, if you are building clang it does a
`mv third_party/llvm-build third_party/llvm-build-upstream` and will (if it runs
successfully) restore it with the inverse
`mv third_party/llvm-build-upstream third_party/llvm-build` at the end of its
execution. However if you encounter an error or use `Ctrl-C` on the script, this
won't happen and you might see an error saying that
`mv: cannot overwrite 'third_party/llvm-build-upstream/llvm-build': Directory not empty`.
To fix this simply move the saved directory back to its original spot and then
run the script again.

```bash
mv third_party/llvm-build-upstream third_party/llvm-build
```
