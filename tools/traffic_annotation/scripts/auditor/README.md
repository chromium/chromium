# Network Traffic Annotation Auditor

This script runs extractor.py to extract Network Traffic Annotations
from chromium source code, collects and summarizes its outputs, and performs
tests and maintenance.

Please see `docs/network_traffic_annotations.md` for an introduction to network
traffic annotations.

## Before Running

Before running, you need to build the `traffic_annotation_proto` target, and
pass the build path to the executable. For instance:

```
$ autoninja -C out/Debug traffic_annotation_proto
$ vpython3 ./tools/traffic_annotation/scripts/auditor/auditor.py --build-path=out/Debug
```

`traffic_annotation_proto` is a dependency of the `chrome` target, as well. So
if you regularly use this build directory for development, you can probably skip
the first step.

## Usage

`vpython3 ./tools/traffic_annotation/scripts/auditor/auditor.py [OPTIONS]... [path_filter]...`

Extracts network traffic annotations from source files, tests them, and updates
`tools/traffic_annotation/summary/annotations.xml`. If path filter(s) are
specified, only those directories of the source will be analyzed.
Run `./tools/traffic_annotation/scripts/auditor/auditor.py --help` for options.

Example:
  `vpython3 ./tools/traffic_annotation/scripts/auditor/auditor.py --build-path=out/Debug`

## Safe List

If there are files, paths, or specific functions that need to be exempted from
all or some tests, they can be added to the `safe_list.txt`. The file is comma
separated, specifying the safe lists based on `ExceptionType` in `auditor.py`.

Use * as wildcard for zero or more characters when specifying file paths.

Here are the exception types:
* `all`: Files and paths in this category are exempted from all tests.
* `missing`: Files and paths in this category can use the
  MISSING_TRAFFIC_ANNOTATION tag.
* `mutable_tag`: Files and paths in this category can use the
  CreateMutableNetworkTrafficAnnotationTag() function.
* `test_annotation`: Files and paths in this category can use the
  TRAFFIC_ANNOTATION_FOR_TESTS tag.
* `missing_new_fields`: If none of internal::contacts::email, user_data::type
  and last_reviewed fields are populated then file and paths in this category
  are exempted getting validated.

## hashes.py

In logs from chrome://net-export, there is a `traffic_annotation` field, which
contains the ***hash code** of the network annotation. Since this hash code is
not recorded in annotations.xml, it is non-trivial to map it to its annotation.

Running this scripts print a list of all annotations from `annotations.xml`,
along with their hash codes. This makes it easier to look up the annotation.

```
vpython3 ./tools/traffic_annotation/scripts/auditor/hashes.py
```
