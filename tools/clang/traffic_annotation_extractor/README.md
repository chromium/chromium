# Traffic Annotation Extrator
This is a clang tool to extract network traffic annotations. The tool is run by
`tools/traffic_annotation/auditor/traffic_annotation_auditor`. Refer to it for
help on how to use.

## Build on Linux
`tools/clang/scripts/build.py --bootstrap
   --without-android --extra-tools traffic_annotation_extractor`

## Build on Window
1. Either open a `VS2015 x64 Native Tools Command Prompt`, or open a normal
   command prompt and run `depot_tools\win_toolchain\vs_files\
   $long_autocompleted_hash\win_sdk\bin\setenv.cmd /x64`
2. Run `python tools/clang/scripts/build.py --bootstrap
   --without-android --extra-tools traffic_annotation_extractor`

## Usage
Run `traffic_annotation_extractor --help` for parameters help.

Example for direct call:
  `third_party/llvm-build/Release+Asserts/bin/traffic_annotation_extractor
     -p=out/Debug components/spellcheck/browser/spelling_service_client.cc`

Example for call using run_tool.py:
  `tools/clang/scripts/run_tool.py --tool=traffic_annotation_extractor
     --generate-compdb -p=out/Debug components/spellcheck/browser`

The executable extracts network traffic annotations and calls to network request
  generation functions from given file paths based on build parameters in build
  path, and writes them to llvm::outs. It also finds all code sites in which a
  network traffic annotation tag or any of its variants are directly assigned
  using a list expression constructor or assignment to its |unique_id_hash_code|
  argument.

Each annotation output will have the following format:
  - Line 1: "==== NEW ANNOTATION ===="
  - Line 2: File path.
  - Line 3: Name of the function in which the annotation is defined.
  - Line 4: Line number of the annotation.
  - Line 5: Function type ("Definition", "Partial", "Completing",
            "BranchedCompleting").
  - Line 6: Unique id of annotation.
  - Line 7: Completing id or group id, when applicable, empty otherwise.
  - Line 8-: Serialized protobuf of the annotation. (Several lines)
  - Last line:  "==== ANNOTATION ENDS ===="

Each function call output will have the following format:
  - Line 1: "==== NEW CALL ===="
  - Line 2: File path.
  - Line 3: Name of the function in which the call is made.
  - Line 4: Name of the called function.
  - Line 5: Does the call have an annotation?
  - Line 6: "==== CALL ENDS ===="

Each direct assignment output will have the following format:
  - Line 1: "==== NEW ASSIGNMENT ===="
  - Line 2: File path.
  - Line 3: Name of the function in which assignment is done.
  - Line 4: Line number of the assignment.
  - Line 5: "==== ASSIGNMENT ENDS ===="
