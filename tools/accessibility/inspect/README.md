# Inspect tools

These cli tools are designed to help to debug accessibility problems in applications, primarily in web browsers.

`ax_dump_tree` to inspect accessibility tree of application
`ax_dump_events` to watch accessibility events fired by application

Note: if you are on Windows, see convenience scripts section, it makes the tools easier to use on Windows.

## Build

`autoninja -C out/Default ax_dump_tree ax_dump_events`

The command generates `ax_dump_tree`  and `ax_dump_events` executables  in `out/Default` directory.

## Prerequesties

### Mac

1) Turn on Accessibility for Terminal in Security & Privacy System Preferences.

2) Some applications keep accessibility inactive, which prevents them to generate accessible trees or emit events. Thus either:
* start VoiceOver (`CMD+F5`) or
* use application specific runtime flags
** Chromium: `Chromium.app/Contents/MacOS/Chromium --force-renderer-accessibility`

## ax_dump_tree

Helps to inspect accessibility trees of applications. Trees are dumped into console.

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

## ax_dump_events

The tool logs into console accessibility events fired by application.

To watch accessibility events, run:
`ax_dump_events <options>`

Use these pre-defined application selectors to indicate application:
`--chrome` for Chrome browser
`--chromium` for Chromium browser
`--firefox` for Firefox browser
`--safari` for Safari browser

`--active-tab` to scope events to an active tab of selected browser.

You can also specify application by its title:
`--pattern=title`

Or by application PID:
`--pid=process_id`


## Convenience PowerShell scripts

Note: Windows only.

Run these scripts to avoid the difficulty of looking up the process id or window handle you want to inspect.
Sometimes there may be several windows open for the given app, and disambuation. In this case, after you run the script, it will list top level windows/processes and ask you to re-run with an argument that includes a substring from the window title you want to inspect the tree/events for. For example, `chrome-tree live` will inspect a tab with the name "Live region tests" (the title matcher is case insensitive).

* chrome-tree and chrome-events for official Google Chrome (has 'Google Chrome' in the window title)
* chromium-tree and chromium-events for Chrome you built yourself (has 'Chromium' in the window title)
* ff-tree and ff-events for Firefox
