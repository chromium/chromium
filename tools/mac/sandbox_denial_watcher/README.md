# macOS Sandbox Denial Watcher

This directory contains tools to detect and report unexpected macOS
Sandbox denials emitted by Chrome/Chromium and related processes.

## Overview

The macOS sandbox logs denials when a sandboxed process attempts an operation
forbidden by its Seatbelt (`.sb`) profile, such as unauthorized file access,
Mach lookup, or sysctl queries.

Chrome already produces a number of denials today (e.g., from system framework
probing or fallback queries). Not all of them are necessarily desirable, but
the goal of this tool is to detect regressions, so these known denials are
filtered out. The tool uses `/usr/bin/log stream` to watch for unexpected
denials and displays a dialog when one is detected. From the dialog, you can
file a pre-filled Chromium Issue Tracker bug.

## Requirements

Chrome or Chromium must have sandbox logging enabled, either by enabling the
**Mac Sandbox Logging** flag (`chrome://flags/#mac-sandbox-logging`) or by
launching it with the `--enable-sandbox-logging` command-line flag. Without
sandbox logging, denials will not appear in the unified log stream.

## Usage

### Start the Watcher

Run the script from the Chromium `src` directory:

```bash
tools/mac/sandbox_denial_watcher/sandbox_denial_watcher.py
```

To see diagnostic output, use verbose mode:

```bash
tools/mac/sandbox_denial_watcher/sandbox_denial_watcher.py -v
```

### Launch Chromium or Chrome

Start Chrome or Chromium as you normally would, ensuring that sandbox logging
is enabled (see [Requirements](#requirements)).

### Reporting Bugs

When unexpected denials occur:

- The terminal displays each full `[DENIAL]` log message in real time.
- Related denials occurring within a short time window are batched together by
  process and operation type.
- A single `[REPORT]` URL is emitted for the batch, and a macOS dialog appears
  with **Dismiss** and **File bug** buttons:
  - Clicking **File bug** opens your browser to a pre-filled issue on the
    Chromium Issue Tracker containing all batched denial events and suppresses
    further notifications for that denial for the remainder of the session.
  - Clicking **Dismiss** or letting the dialog close after 15 seconds
    suppresses notifications for that denial for one hour.

The pre-filled bug includes the denial, your macOS version, and your CPU
architecture. Your home directory is replaced with `~`, but please review the
report for any other personal information before filing it. Please also fill
in the "What were you doing when this notification was triggered?" section;
this context helps developers reproduce and resolve the issue more quickly.

## Adding New Known Denials

If a newly observed denial is determined to be expected:

1. Open `tools/mac/sandbox_denial_watcher/sandbox_denial_watcher.py`.
2. Add the target to the appropriate dictionary:
   - **Target pattern** (default; matches only the target itself, not subpaths):
     Add the target string to the corresponding action's frozenset in
     `_KNOWN_TARGET_DENIALS`:
     ```python
     "mach-lookup": frozenset((
         "com.apple.some.service",
     )),
     ```
   - **Directory subpath pattern** (matches the directory and any path beneath
     it):
     Add the directory path string to the corresponding action's frozenset in
     `_KNOWN_SUBPATH_DENIALS`:
     ```python
     "file-read-data": frozenset((
         "/Library/Managed Preferences/",
     )),
     ```
   - Both dictionaries support `*` wildcards to match characters within a single
     path segment (e.g., `tmp.*` or `scoped_dir*`), as well as placeholder
     constants such as `_DARWIN_USER_DIR`, `_CHROME_APP_SUPPORT_DIR`, and
     `_CHROME_BUNDLE_DIR`.
   - Action keys can also use Seatbelt parent operations that end in `*`
     (for example, `file-write*` covers all `file-write-*` operations, including
     `file-write-create`, `file-write-times`, etc.). The watcher evaluates
     denial actions against these Seatbelt parent operation rules.
   - To restrict a rule to a specific Seatbelt profile, use a
     `(pattern, profile)` tuple instead of a plain pattern string:
     ```python
     "file-write*": frozenset((
         (f"{_DARWIN_USER_DIR}/T/scoped_dir*/CRX_INSTALL/", "utility.sb"),
     )),
     ```
3. Keep entries sorted alphabetically within their respective category section.
   Entries built from placeholder constants or `_HOME_DIR` are grouped
   separately at the end of their section.
