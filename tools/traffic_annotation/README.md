# Network Traffic Annotations
Network traffic annotations provide transparency and auditability for the data
that Chrome sends to the network. For an introduction, please see
`docs/network_traffic_annotations.md`.
This folder provides tools to ensure that every operation in the code base that
requires annotation, is annotated, and annotations are sound and complete.

# Traffic Annotation Auditor
This is a python script that checks the repository, extracts annotations, and
performs required tests and maintenance. See more details in
`tools/traffic_annotation/scripts/README.md`.

# Traffic Annotation Extractor
Traffic Annotation Auditor uses this python script (located in
`tools/traffic_annotation/scripts/extractor.py`) to parse the code and extract
required data for testing and maintenance.

# Automatic Annotation Tests
Network traffic annotations are tested in commit queue using
`tools/traffic_annotation/scripts/check_annotations.py`. This test is currently
run on Linux and Windows trybots, but may expand in future to other platforms.
To perform this test fast enough for a trybot and to avoid spamming the commit
queue if an unexpected general failure happens (see next item), trybot tests are
run in error resilient mode and only on the changed files. A more complete test
runs on an FYI bot using
`tools/traffic_annotation/scripts/traffic_annotation_auditor_tests.py` and
alerts if tests are not running as expected.

# Emergency Brake
In the event that clang changes something that requires the tool to be rebuilt
(or for some other reason the tests don't work correctly), please disable the
trybot test by setting the `TEST_IS_ENABLED` flag to False in
`tools/traffic_annotation/scripts/check_annotations.py`, and file a bug and cc
the people listed in OWNERS; they'll be on the hook to rebuild and re-enable the
test.

# Annotations Summary
`tools/traffic_annotation/summary/annotations.xml` keeps an up to date summary
of all annotations in the repository. This file is automatically updated by
Traffic Annotation Auditor.
