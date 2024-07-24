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


## Using the tool

The `rewrite-multiple-platforms.sh` scripts first builds the tool, runs tests and then
runs the tool over every configuation in the list of platforms.

```bash
  ./tools/clang/spanify/rewrite-multiple-platforms.sh
```
