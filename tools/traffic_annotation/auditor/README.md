# Network Traffic Annotation Auditor
This binary runs the clang tool for extraction of Network Traffic Annotations
from chromium source code, collects and summarizes its outputs, and performs
tests and maintenance.
Please see `docs/network_traffic_annotations.md` for an introduction to network
traffic annotations.

## Usage
`traffic_annotation_auditor [OPTIONS]... [path_filter]...`

Extracts network traffic annotations from source files, tests them, and updates
`tools/traffic_annotation/summary/annotations.xml`. If path filter(s) are
specified, only those directories of the source will be analyzed.
Run `traffic_annotation_auditor --help` for options.

Example:
  `traffic_annotation_auditor --build-path=out/Debug`

The binaries of this file and the clang tool are checked out into
`tools/traffic_annotation/bin/[platform]`. This is only done for Linux and
Windows platforms now and will be extended to other platforms later.

## Running
Before running, you need to build the COMPLETE chromium and pass the build path
to the executable.

## Safe List
If there are files, paths, or specific functions that need to be exempted from
all or some tests, they can be added to the `safe_list.txt`. The file is comma
separated, specifying the safe lists based on
`AuditorException::ExceptionType` in
`tools/traffic_annotation/auditor/traffic_annotation_auditor.h`.
Use * as wildcard for zero or more characters when specifying file paths.

Here are the exception types:
* `all`: Files and paths in this category are exempted from all tests.
* `missing`: Files and paths in this category can use the
  MISSING_TRAFFIC_ANNOTATION tag.
* `mutable_tag`: Files and paths in this category can use the
  CreateMutableNetworkTrafficAnnotationTag() function.
* `direct_assignment`: The functions in this category can assign a value
  directly to a MutableNetworkTrafficAnnotationTag. This is controlled to
  avoid assigning arbitrary values to mutable annotations.
* `test_annotation`: Files and paths in this category can use the
  TRAFFIC_ANNOTATION_FOR_TESTS tag.
