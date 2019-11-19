# Network Traffic Annotations List
This file describes the `tools/traffic_annotation/summary/annotations.xml`.
Please see `docs/network_traffic_annotations.md` for an introduction to network
traffic annotations.

# Content Description
`annotations.xml` includes the summary of all network traffic annotations in
Chromium repository.
The following items are stored for each annotation :
* `id`: Unique ID of the annotation.
* `hash_code`: Hash code of the unique id of the annotation. These values are
     used in the binary as annotation tags.
* `type`: Type of the annotation (complete, partial, ...). Uses enum values
    of `AnnotationInstance` in `tools/traffic_annotation/auditor/instance.h`.
* `content_hash_code`: Hash code of the annotation content. This value is stored
    to check when an annotation is modified.
* `os_list`: List of all platforms on which this annotation exists.
    Currently only including `linux` and `windows`.
* `file_path`: The file path of the annotation.
* `reserved`: Reserved annotations (like annotation for test files) have this
    attribute. If annotation is a reserved one, it does not have
  `content_hash_code` and `file_path` attributes.
* `deprecated`: Once an annotation is removed from the repository, this
    attribute is added to its item with value equal to the deprecation date, and
    `os_list` and `file_path` attributes are removed.
    These items can be manually or automatically pruned after sufficient time.
    Unique id of deprecated annotations cannot be reused.

# How to Generate/Update.
Run `traffic_annotation_auditor` to check for annotations correctness and
automatic update. There are also trybots on Linux and Windows to run the tests
and suggest required updates.
The latest executable of `traffic_annotation_auditor` for supported platforms
can be found in `tools/traffic_annotation/bin/[platform]`.
