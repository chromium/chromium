# Traffic Annotation Scripts
This file describes the scripts in `tools/traffic_annotation/scripts`.

# auditor/auditor.py

This is the python implementation of the Traffic Annotation Auditor, and it
adds new annotations to the grouping xml file.
After the grouping xml file is generated, we'll decide if the new annotation
should be in the report by adding hidden="false".
i.e. you can call it with the same command-line arguments
and it should give similar output.

# check_annotations.py
Runs traffic annotation tests on the changed files or all repository. The tests
are run in error resilient mode. Requires a compiled build directory to run.

# traffic_annotation_auditor_tests.py
Runs tests to ensure auditor/auditor.py is performing as expected. Tests
include:
 - Checking if auditor and underlying extractor run, and an expected minimum
   number of outputs are returned.
 - Checking if enabling or disabling heuristics that make tests faster has any
   effect on the output.
 - Checking if running on the whole repository (instead of only changed files)
   would result in any error.
This test may take a few hours to run and requires a compiled build directory.

# annotation_tools.py
Provides tools for annotation test scripts.

# update_annotations_sheet.py
This script updates the Google sheet that presents all network traffic
annotations.

# extractor.py
Scans through a set of specified C++ files to detect existing traffic
annotations in code. It uses regex expressions on source files.

# extractor_test.py
Unit tests for extractor.py.

# update_annotations_doc.py
Updates the Chrome Browser Network Traffic Annotations document that presents
all network traffic annotations specified within `summary/grouping.xml`.
  - You can use the `hidden="true"` attribute within a group to suppress the
    group and its nested senders and annotations from appearing in the document.
  - You can use the `hidden="true"` attribute within the annotations in
    `grouping.xml` to suppress them from appearing in the document.
  - `grouping.xml` needn't be organized in alphabetical order, the script
    automatically places them in alphabetical order.

# update_annotations_doc_tests.py
Unit tests for update_annotations_doc.py.

# generator_utils.py
Parses the `grouping.xml` and `annotations.tsv` files to provide
`update_annotations_doc.py` with the annotations and their relevant information,
e.g. unique_id, data, trigger, etc. Also includes methods to parse the json
object returned by the Google Docs API `get()` method.

# generator_utils_tests.py
Unit tests for generator_utils.py.
