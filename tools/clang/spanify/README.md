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

The `rewrite-multiple-platforms.sh` scripts first builds the tool, runs tests and then
runs the tool over every configuation in the list of platforms.

```bash
  ./tools/clang/spanify/rewrite-multiple-platforms.sh
```
