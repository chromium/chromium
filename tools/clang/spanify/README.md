# Clang Spanification tool

This tool generates cross-translation unit rewrites converting pointer type expressions (T*/raw_ptr<T>) to base::span<T>/base::raw_span<T> expressions.

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
  ./tools/clang/spanify/tests/run_all_tests.py
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
tools/clang/spanify/tests/deref-expr-actual.cc:4:10: fatal error: 'vector' file not found
    4 | #include <vector>
      |          ^~~~~~~~
1 error generated.
```

You can install libstdc++ as follows:
```bash
sudo apt install libstdc++-14-dev
```

## Using the tool

The `rewrite-multiple-platforms.sh` scripts first builds the tool, runs tests
and then runs the tool over every configuration in the list of platforms.

```bash
  ./tools/clang/spanify/rewrite-multiple-platforms.sh
```

### Hints

#### Flags

1. `--platforms=<comma,separated,list,of,platforms>`
2. `--skip-building-clang` or `-b` for short, only use if you have already built
   clang and know nothing has changed, this implicitly adds `--keep-build` flag,
   see the flag definition below for more details on its behaviour.
3. `--skip-rewrite` or `-r` for short, if you've already ran on the platform and
   have the associated `~/scratch` directory you can just skip generating this.
4. `--skip-extract-edits` or `-e` for short, don't attempt to run
   `extract_edits.py` on the results of the rewrite.
5. `--keep-build` when you plan to run again (and want to use
   `--skip-building-clang`), you first need a run that completed the build part
   with `--keep-build`. IMPORTANT: this will mean `third_party/llvm-build` will
   not be the normal optimized build and will slow down your regular chromium.
   You should restore it by running
   `mv third_party/llvm-build-upstream third_party/llvm-build` or running the
   script without `--keep-build`
6. `--incremental-clang-build` or `-i` for short, will use the already built
   clang and attempt to only incrementally include changes to the clang plugin,
   this implicitly adds `--keep-build` flag, see the flag definition above for
   more details on its behaviour.

The flags are useful to cut down iteration time when working on particular
parts. If you are modifying the plugin you can't skip the build part, but if you
just want to do a new platform this flag is useful. If you've already done the
platform, skipping both the build and the rewrite allows you to work on
`extract_edits.py`'s logic. Skipping edits is only really useful if you are
testing early steps and you aren't interested in the script even attempting the
edits (so it ends earlier).

Example commands:

```bash

  ./tools/clang/spanify/rewrite-multiple-platforms.sh --platforms=linux
  ./tools/clang/spanify/rewrite-multiple-platforms.sh -p=linux --keep-build
  ./tools/clang/spanify/rewrite-multiple-platforms.sh --skip-building-clang
  ./tools/clang/spanify/rewrite-multiple-platforms.sh --b
  ./tools/clang/spanify/rewrite-multiple-platforms.sh \
  --platforms=linux --skip-building-clang --skip-rewrite
```

### Troubleshooting

If for some reason your `rewrite-multiple-platforms.sh` command fails, it is
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

Another possible issue is during the `extract_edits.py` step it might complain
that `~/scratch/rewriter.main.out` doesn't exist. This can happen sometimes
because the script writes out files to `~/scratch/rewriter-$PLATFORM.main.out`
and then later uses `cat platform_file >> ~/scratch/rewriter.main.out` and it
can be unreliable due to the large file size. You can simply create it yourself
either by appending each platform together or symlinking it together.
