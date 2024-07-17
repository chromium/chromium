# Handle to DirectHandle migration tool

A clang tool for the migration of `v8::internal::Handle<T>` to
`v8::internal::DirectHandle<T>`. This is only useful for the V8 code base,
as such handles can only be used there. The two types are only different
in builds with `v8_enable_direct_handle=true`, which implies
`v8_enable_conservative_stack_scanning=true`, so the tool should be applied
to such a debug build.

## Building

The first time, build with:
```
tools/clang/scripts/build.py --without-android --without-fuchsia --extra-tools v8_handle_migrate
```
Then, recompile with:
```
ninja -C third_party/llvm-build/Release+Asserts v8_handle_migrate
```

## Usage

Build the release with which to use it (see above for GN args), e.g.:
```
autoninja -C out/css all
```

You may need to generate the database of compile commands:
```
tools/clang/scripts/generate_compdb.py -p out/css -o out/css/compile_commands.json
```

Then, process files with:
```
tools/clang/scripts/run_tool.py --tool v8_handle_migrate -p out/css files.cpp
```

## Testing

Run the tests with:
```
tools/clang/scripts/test_tool.py v8_handle_migrate --apply-edits
```

Notice that the tests use mock versions of the relevant V8 header files.
