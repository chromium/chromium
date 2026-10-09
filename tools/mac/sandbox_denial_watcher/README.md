# macOS Sandbox Denial Watcher

This directory contains a tool to monitor macOS Sandbox denials emitted by
Chrome and Chromium processes.

## Overview

The macOS sandbox logs denials when a sandboxed process attempts an operation
forbidden by its Seatbelt (`.sb`) profile, such as unauthorized file access,
Mach lookup, or sysctl queries.

The tool uses `/usr/bin/log stream` to watch for denials from Chrome and
Chromium processes and prints each one to the terminal.

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

When a denial occurs, the terminal displays the full `[DENIAL]` log message.
