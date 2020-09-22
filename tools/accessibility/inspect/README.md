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

This will generate `ax_dump_tree` executable in `out/Default` directory.

### Prerequesties

#### Mac

1) Turn on Accessibility for Terminal in Security & Privacy System Preferences.

2) Some applications keep accessibility inactive, which prevents them to generate accessible trees. Thus either:
* start VoiceOver (`CMD+F5`) or
* use application specific runtime flags
** Chromium: `Chromium.app/Contents/MacOS/Chromium --force-renderer-accessibility`

### Run

To dump an accessible tree, run:
`ax_dump_tree <options>`

At your convenience the number of pre-defined application selectors are available:
`--chrome` for Chrome browser
`--chromium` for Chromium browser
`--firefox` for Firefox browser
`--safari` for Safari browser
`--active-tab` to dump a tree of active tab of selected browser.

You can also specify an application by its title:
`ax_dump_tree --pattern=title`

Alternatively you can dump a tree by HWDN on Windows:
`--window=HWDN`
Note, to use a hex window handle prefix it with `0x`.

Or by application PID on Mac and Linux:
`--pid=process_id`

Other options:
`--json` to output a tree in JSON format
`--filters=absolute_path_to_filters.txt` to filter properties, use where the filters text file has a series of `@ALLOW` and/or `@DENY` lines. See example-tree-filters.txt in tools/accessibility/inspect.
`--help` for help


## Convenience PowerShell scripts

Note: Windows only.

Run these scripts to avoid the difficulty of looking up the process id or window handle you want to inspect.
Sometimes there may be several windows open for the given app, and disambuation. In this case, after you run the script, it will list top level windows/processes and ask you to re-run with an argument that includes a substring from the window title you want to inspect the tree/events for. For example, `chrome-tree live` will inspect a tab with the name "Live region tests" (the title matcher is case insensitive).

* chrome-tree and chrome-events for official Google Chrome (has 'Google Chrome' in the window title)
* chromium-tree and chromium-events for Chrome you built yourself (has 'Chromium' in the window title)
* ff-tree and ff-events for Firefox
