# Inspect tools

Note: please see convenience scripts section, it makes the tools much easier to use.

## ax_dump_events

This tool helps monitor accessibility events. It currently works on Windows,
and Mac is TBD.

Events are dumped to the console. To use it, run
`ax_dump_events --pid=[processid]`

Press Ctrl+C to quit.

## ax_dump_tree

This tool helps to inspect accessibility trees of applications. Trees are dumped into console.

### Build

`autoninja -C out/Default ax_dump_tree`

### Run

To dump accessibility tree of application, run
`ax_dump_tree --window=id`
where `id` is HWND on Windows, PID on Linux and Mac.

Alternatively, you can indicate an application by its title:
`ax_dump_tree --pattern=title`

Notes:
* To use a hex window handle prefix it with `0x`.
* For json output, use the `--json` option
* To filter certain properties, use `--filters=[path-to-filters.txt]` where the filters text file has a series of `@ALLOW` and/or `@DENY` lines. See example-tree-filters.txt in tools/accessibility/inspect.
* [Mac] You have to turn on Accessibility for Terminal in Security & Privacy System Preferences.

## Convenience PowerShell scripts

Run these scripts to avoid the difficulty of looking up the process id or window handle you want to inspect.
Sometimes there may be several windows open for the given app, and disambuation. In this case, after you run the script, it will list top level windows/processes and ask you to re-run with an argument that includes a substring from the window title you want to inspect the tree/events for. For example, `chrome-tree live` will inspect a tab with the name "Live region tests" (the title matcher is case insensitive).

* chrome-tree and chrome-events for official Google Chrome (has 'Google Chrome' in the window title)
* chromium-tree and chromium-events for Chrome you built yourself (has 'Chromium' in the window title)
* ff-tree and ff-events for Firefox
