# Clang IteratorChecker

> Note for developers:
> FlowSensitive is a fast evolving framework, and thus some breaking changes
> are regularly introduced. The current state of this plugin works against
> `llvmorg-18.0.0`.

This clang plugin aims at detecting iterator use-after-invalidation bugs using
the clang-tidy dataflow framework FlowSensitive.

For instance:
```cpp
  for (auto* it = container->begin(); it != container->end();) {
    if (it->block_end <= block_offset) {
      // should be it = container->erase(it);
      container->erase(it);
    } else {
      ++it;
    }
  }
}
```
It is not valid using `++it` in the second branch of the loop after if
`container->erase(it)` was called on the first branch. See real code
[example](https://chromium-review.googlesource.com/c/chromium/src/+/4306699).


## Build

Clang is built using CMake. To run cmake, this script can be used:
```bash
  ./tools/clang/scripts/build.py     \
    --without-android                \
    --without-fuchsia                \
    --extra-tools iterator_checker
```

The build directory is created into: `third_party/llvm-build/Release+Asserts/`
and you can build it again using:
```bash
  ninja -C third_party/llvm-build/Release+Asserts/
```


## Run the tests

```bash
	./tools/clang/iterator_checker/tests/test.py \
    $(pwd)/third_party/llvm-build/Release+Asserts/bin/clang
```


## Using the plugin

The procedure is mostly the same as for the other clang plugins in chrome. What
you need to do is to basically add the following [in a GN file](https://source.chromium.org/chromium/chromium/src/+/main:build/config/clang/BUILD.gn)
(depending what you want the plugin to be used for).

```bash
cflags += [
  "-Xclang",
  "-add-plugin",
  "-Xclang",
  "iterator-checker",
]
```
