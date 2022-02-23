# Test disabling tool

This directory contains a tool for automatically disabling tests. It supports
conditionally disabling them under different conditions, and merging with any
existing conditions already present.

# Conditions

A condition represents a build or runtime configuration under which a test is to
be disabled or enabled. Examples include OS (Linux, Mac, Windows, ...),
architecture (x86, ARM, ...), and dynamic analyses (ASan, MSan, ...).

Conditions are specified on the command line in [disjunctive normal
form](https://en.wikipedia.org/wiki/Disjunctive_normal_form) -- that is, an OR
of ANDs. The tool accepts any number of condition arguments, which are
implicitly ORed together. Each one consists of any number of conditions joined
with `&` (with any whitespace ignored). So for instance, disabling a test under
Mac, or Linux under ASan is expressed as `mac 'linux & asan'`. If no conditions
are passed, this is interpreted as "disable under all configurations".

Any existing conditions under which the test is already disabled will be merged
with the new conditions using the following rules:

* If it's currently unconditionally disabled or enabled, then just use the new
  condition.
* Otherwise, take the union of the two (old OR new).

The full list of conditions can be found in the help text:

`$ disable --help`

# Usage

Running the tool will make the necessary source modifications to disable the
test. It's up to you to upload a CL and get it submitted. In the future we may
add support for automating more of this process, but for now it just produces
the edit for you.

The tool relies on metadata fetched from ResultDB, and generated from recent
test runs, so you should sync your checkout close to HEAD, to reduce the chance
that the test has been moved or renamed.

## Examples

Disable a test on all configurations given its full ID:

`$ disable ninja://chrome/test:browser_tests/BrowserViewTest.GetAccessibleTabModalDialogTree`

Disable a test given only the suite and name:

`$ disable BrowserViewTest.GetAccessibleTabModalDialogTree`

Disable a test under a specific condition:

`$ disable BrowserViewTest.GetAccessibleTabModalDialogTree linux`

Disable a test under any of a list of conditions:

`$ disable BrowserViewTest.GetAccessibleTabModalDialogTree linux mac`

Disable a test under a combination of conditions:

`$ disable BrowserViewTest.GetAccessibleTabModalDialogTree 'linux & asan'`

# Supported test frameworks

This tool currently supports:

* GTest
* Test expectations

Not all features may be supported equally across all frameworks. If you'd like
support for a new framework, or if there's a feature that isn't supported under
a currently supported framework, please file a bug as directed below.

# Bug reports

The tool will print out a bug filing link for most failures. This link will
automatically populate the bug summary with information useful for debugging the
test, so please use it if available. If not present, e.g. if the tool did the
wrong thing but didn't crash, please file any bugs under the [Sheriff-o-Matic
component](https://bugs.chromium.org/p/chromium/issues/entry?components=Infra%3ESheriffing%3ESheriffOMatic).

# Development

## Integration tests

This tool has a suite of integration tests under the `tests` directory, run via
`integration_test.py`. New tests can be recorded using the `record` subcommand,
followed by the name of the test and the command line arguments to run it with.

Recording a testcase captures and stores the following data:
* The arguments used to run the test.
* The ResultDB requests made and their corresponding responses.
* The path and content of any data read from files.
* The path and content of any data written to files.

When testcases are replayed, this information is fed into the tool to make it
completely hermetic and reproducible, and as such these tests should be 100%
consistent and reproducible. The data written is considered the only output of
the tool, and this data is compared against what is stored to determine whether
the test passes or fails.

Existing tests can be printed in a readable format by using the `show`
subcommand with the name of the test you want to examine.
