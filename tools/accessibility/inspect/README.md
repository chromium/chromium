# Inspect tools

These cli tools are designed to help to debug accessibility problems in
applications, primarily in web browsers.

* `ax_dump_tree` to show the accessibility tree of an application
* `ax_dump_events` to watch accessibility events fired by application

## Build

To build the tools, run:
```
autoninja -C out/Default ax_dump_tree ax_dump_events
```
The command generates `ax_dump_tree`  and `ax_dump_events` executables in
`out/Default` directory.

To use the tools:
1. Enable accessibility on your system and/or target application. Refer to the
"Enabling Accessibility" section below for guidance.
2. Launch the application and go to the specific screen, webpage, or content you
want to inspect.
3. To view the accessibility trees of the application, run `ax_dump_tree
<options>`.
To view accessibility events fired by application, run `ax_dump_events
<options>`. Both tools will output information into the console.

## Options

### Selectors

A selector is needed to specify the application you wish to inspect.

#### Web Browsers:

The following flags can be used to specify a web browser:
* `--chrome` for Chrome browser
* `--chromium` for Chromium browser
* `--firefox` for Firefox browser
* `--edge` for Edge browser (Windows only)
* `--safari` for Safari browser (Mac only)

You may pass in `--active-tab` when using any of the browser selectors above to
dump the tree of active (current) tab. Otherwise, trees from all tabs will be
dumped.

#### Other Applications:

You can specify an application by its title:
`ax_dump_tree --pattern=title`.
The pattern string can contain wildcards like `*` and `?`. Note, you can use the
`--pattern` selector in conjunction with pre-defined browser selectors.

Alternatively, you can dump a tree by HWND on Windows:
`--pid=HWND`
Note, to use a hex window handle prefix it with `0x`.

Or by application PID on Mac and Linux:
`--pid=process_id`

### Filters

By default, the accessibility tree output is filtered to show a specific set of
properties.  To control which properties are included or excluded, use the
`--filters` option.

#### How to Use Filters:

1. **Create a Filter File:** Write a plain text file (e.g., filters.txt) where
each line contains one filtering rule.
2. **Apply Filters:** Run the tool with the `--filters` option, specifying the
full path to your filter file:
```
--filters=absolute_path_to_filters.txt
```

#### Filtering Rules:

- `@ALLOW:property_name`: Include the property only if it has a non-empty value.
- `@ALLOW-EMPTY:property_name`: Include the property even if it has an empty
value.
- `@DENY:property_name`: Exclude the property.

#### Examples:
- `@ALLOW:AXARIALive`: Include the `AXARIALive` attribute in the tree.
- `@ALLOW:*`: Include all properties in the tree.


See [example-tree-filters.txt](https://source.chromium.org/chromium/chromium/src/+/main:tools/accessibility/inspect/example-tree-filters.txt)
in `tools/accessibility/inspect` for more examples.

### API option for Windows

On windows, we support two accessibility APIs, IAccessible2 and UI Automation.
By default, IA2 is selected.

To dump a tree with IAccessible2:

`--api=ia2`

To dump a tree with UI Automation:

`--api=uia`

### Other options

`--help` for help

## Examples

To dump an Edge accessible tree on Windows:
``out\Default\ax_dump_tree --edge``

To dump an accessible tree on Mac for a Firefox window with title containing
``mozilla``:
``out/Default/ax_dump_tree --firefox --pattern=*mozilla*``

To watch Chromium accessibility events on Linux:
``out/Default/ax_dump_events --chromium``

## Enabling Accessibility

You may be required to enable accessibility on your system so that accessibility
tree/events are generated. Depending on the application, you may also be
required to run an assistive technology, like a screen reader, or use an
application-specific runtime flag in order to activate accessibility
application-wide.

### Operating System-Specific Instructions

#### Mac

1) Turn on Accessibility for Terminal in Security & Privacy System Preferences.

2) Start VoiceOver (`CMD+F5`) or if your application has a specific
flag/preference to turn on accessibility, use that instead.

#### Linux

Use Orca or use the application specific flag/preference to turn on
accessibility.

#### Windows

Use Narrator/NVDA/JAWS or use the application specific flag/preference to turn
on accessibility.

### Chrome/Chromium

To enable accessibility for Chrome/Chromium, launch chrome with the run time
flag `--force-renderer-accessibility`. For example, on Mac, the full command
would be ``Chromium.app/Contents/MacOS/Chromium --force-renderer-accessibility``.
