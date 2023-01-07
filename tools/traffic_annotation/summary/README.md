# Network Traffic Annotations List
This file describes the `tools/traffic_annotation/summary/annotations.xml`.
Please see `docs/network_traffic_annotations.md` for an introduction to network
traffic annotations.

# Content Description
`annotations.xml` includes the summary of all network traffic annotations in
Chromium repository.
The following items are stored for each annotation :
* `id`: Unique ID of the annotation.
* `added_in_milestone`: Chrome version in which this annotation was added.
* `type`: Type of the annotation (complete, partial, ...). Uses enum values
    of `Annotation.Type` in
    `tools/traffic_annotation/scripts/auditor/auditor.py`. If ommitted, it means
    "definition" (i.e., complete).
* `content_hash_code`: Hash code of the annotation content, as hexadecimal. This
    value is stored to check when an annotation is modified.
* `os_list`: List of all platforms on which this annotation exists.
    Currently only including `linux`, `windows`, `android` and `chromeos`.
* `file_path`: The file path of the annotation.
* `reserved`: Reserved annotations (like annotation for test files) have this
    attribute. If annotation is a reserved one, it does not have
  `content_hash_code` and `file_path` attributes.

# How to Generate/Update.
Run `auditor.py` to check for annotations correctness and
automatic update. There are also trybots on Linux and Windows to run the tests
and suggest required updates.

The script can be found in
`tools/traffic_annotation/scripts/auditor/auditor.py`.
