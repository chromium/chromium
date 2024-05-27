# Trace Annotator Tool

## Introduction

A tool to annotate functions with ```c++ TRACE_EVENT```. This tool serves two
workflows:

*  Debugging: bulk add traces to all functions in a directory and after finding
   the bug remove the traces.
*  Adding traces to code: bulk add traces to all functions in a directory,
   review the changes carefully and create a patch.

The goal of this tool is to transfer a function:
```c++
int foo(int bar, int baz) {
  return 42;
}
```
into:
```c++
int foo(int bar, int baz) {
  TRACE_EVENT0("test", "foo");
  return 42;
}
```

In future also argument tracing is to be supported.

This document is based on //docs/clang_tool_refactoring.md

## Building

The following might take approx. 2 hours depending on your computer.

*  Make a new checkout of chromium (suggested, but optional).
*  From chromium/src:
*  ```shell cr build all``` To make sure all files have been generated.
*  ```shell cp -R third_party/llvm-build ~```
*  ```shell tools/clang/scripts/build.py --bootstrap --without-android
   --without-fuchsia --extra-tools trace_annotator```
   *  TODO how to build with plugin 'find-bad-constructs'?

### Rebuild just the tool:

*  ```shell cd third_party/llvm-build/Release+Asserts```
*  ```shell ninja trace_annotator```

Beware that running ```shell gclient sync``` might overwrite the build and
another full build might be necessary. A backup of the binary from
//third_party/llvm-build/Release+Asserts/bin/trace_annotator might be useful.

## Testing

*  ```shell tools/clang/scripts/test_tool.py --apply-edits trace_annotator```

## Running

*  Chrome plugins are not supported yet, run: ```shell gn args out/Debug/``` and
   add: ```clang_use_chrome_plugins = false``` option.
*  Make sure you have up to date compilation database:
   *  To generate it run: ```shell tools/clang/scripts/generate_compdb.py -p
      out/Debug/ > out/Debug/compile_commands.json```
   *  These are the compiler options for individual files (needed to use the
      right version of C++, right library paths...).

*  ```shell DIR="net"; \
      git checkout $DIR && tools/clang/scripts/run_tool.py --tool trace_annotator -p out/Debug/ $DIR \
      | tools/clang/scripts/extract_edits.py \
      | tools/clang/scripts/apply_edits.py -p out/Debug $DIR \
      && git cl format $DIR```

*  Consult documentation of ```//tools/clang/scripts/run_tool.py``` for more
   options.

### Suggestion:

Do not run the tool on //base or anything that has to do with tracing or
synchronization. Or at least do not submit the resulting patch.

### Debugging workflow suggestion:

*  Do some changes.
*  ```shell git add . ; git commit```
*  Run the tool.
*  ```shell git add . ; git commit```
*  Do some more changes (including fixing a bug).
*  ```shell git add . ; git commit```
*  ```shell git rebase -i``` and follow the help.

### Creating tracing patch suggestion:

*  Run the tool.
*  Double check all generated code.
*  Add method annotations for methods that are hidden by compiler options (e.g.,
   if you are on unix then the code in ```c++ #ifdef OS_WIN``` will not be
   annotated.

## TODO

*  Add options:
   *  Whether to add "do not submit" comment (in upper case).
   *  Function name formatting (without namespace(s) / getQualifiedNameAsString /
      with namespaces but without template tags).
   *  Category name.
   *  Make tracing of function arguments.
*  Standalone build of the tool (outside of //third_party to avoid overwriting
   by ```shell gclient sync```).
