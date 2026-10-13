# macOS Sandbox Denial Watcher

This directory contains tools to monitor unexpected macOS Sandbox denials
emitted by Chrome/Chromium and related processes.

## Overview

The macOS sandbox logs denials when a sandboxed process attempts an operation
forbidden by its Seatbelt (`.sb`) profile, such as unauthorized file access,
Mach lookup, or sysctl queries.

Chrome already produces a number of denials today (e.g., from system framework
probing or fallback queries). Not all of them are necessarily desirable, but
the goal of this tool is to detect regressions, so these known denials are
filtered out. The tool uses `/usr/bin/log stream` to watch for unexpected
denials and prints each one to the terminal.

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

### Viewing Denials

When an unexpected denial occurs, the terminal displays the full `[DENIAL]` log
message.

## Adding New Known Denials

If a newly observed denial is determined to be expected:

1. Open `tools/mac/sandbox_denial_watcher/sandbox_denial_watcher.py`.
2. Add the target to the appropriate dictionary:
   - **Exact pattern** (default; matches only the exact action and target,
     not subpaths):
     Add the target string to the corresponding action's frozenset in
     `_KNOWN_EXACT_DENIALS`:
     ```python
     "mach-lookup": frozenset({
         "com.apple.some.service",
     }),
     ```
   - **Directory subpath pattern** (matches the directory and any path beneath
     it):
     Add the directory path string to the corresponding action's tuple in
     `_KNOWN_SUBPATH_DENIALS`:
     ```python
     "file-read-data": (
         "/Library/Managed Preferences/",
     ),
     ```
3. Keep entries sorted alphabetically within their respective category section.
   Entries built from `_HOME_DIR` depend on the local machine, so they are
   grouped separately at the end of the file reads section.
