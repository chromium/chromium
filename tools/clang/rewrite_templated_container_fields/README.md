# Clang Templated Container Rewriter Tool

This tool generates cross-translation unit rewrites converting containers of pointer fields CTN<T*> to CTN<raw_ptr<T>> fields.
Example:
```
std::vector<Pointee*> field_;
```
becomes:
```
std::vector<raw_ptr<Pointee>> field_;
```

More details about the tool design can be found here:
https://docs.google.com/document/d/1P8wLVS3xueI4p3EAPO4JJP6d1_zVp5SapQB0EW9iHQI/

## Build

Clang is built using CMake. To run cmake, this script can be used:
```bash
  ./tools/clang/scripts/build.py     \
    --without-android                \
    --without-fuchsia                \
    --extra-tools rewrite_templated_container_fields
```

The build directory is created into: `third_party/llvm-build/Release+Asserts/`
and you can build it again incrementally using:
```bash
  ninja -C third_party/llvm-build/Release+Asserts/ rewrite_templated_container_fields
```


## Run the tests

```bash
  ./tools/clang/rewrite_templated_container_fields/tests/run_all_tests.py
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
tools/clang/rewrite_templated_container_fields/tests/assignment-tests-original.cc:5:10: fatal error: 'vector' file not found
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
  ./tools/clang/rewrite_templated_container_fields/rewrite-multiple-platforms.sh
```

