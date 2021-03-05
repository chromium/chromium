# style_variable_generator

This is a python tool that generates cross-platform style variables in order to
centralize UI constants.

This script uses third_party/pyjson5 to read input json5 files and then
generates various output formats as needed by clients (e.g CSS Variables,
preview HTML page).

For input format examples, see the \*_test.json5 files which contain up to date
illustrations of each feature, as well as expected outputs in the corresponding
\*_test_expected.\* files.

Run `python style_variable_generator.py -h` for usage details.

## Generator Options

### CSS

**Dark mode selector**

`--generator-option 'dark_mode_selector=html[dark]'`

Replaces the default media query (`@media (prefers-color-scheme: dark)`) which
triggers colors to switch to dark mode with a custom css selector. The example
above would produce

```
html[dark] {
    ...dark mode colors
}
```

instead of the default

```
@media (prefers-color-scheme: dark) {
    html:not(body) {
        ... colors
    }
}
```

This should only be used if you want to generate a stylesheet for testing where
you can control the switch to dark/light mode, in production always prefer to
use the default behavior which will respect operating system level dark mode
switches.
