# UTR - The Universal Test Runner

UTR is a tool that can locally compile and/or run tests the same way it's done
on the bots. At its heart, it wraps the same codebase used by the bots to drive
their builds, while bypassing parts that don't make sense for developer's
machines. This abstracts away pesky details that are hard to remember or
discover on bots like:
- GN args that may or may not prevent local reproduction
- Full command line args for a test invocation
- Precise OS version to target for remote tests

With this tool, all such details will automatically use what the given builder
uses. See the [Google-only design doc](https://goto.google.com/chrome-utr) for
further context.

## Command-line Examples

TODO(crbug.com/41492687): Add use-case examples as support is rolled out.
